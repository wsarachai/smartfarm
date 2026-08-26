"""
sx127x.py — minimal SX127x (SX1276/77/78/79) LoRa driver for a Raspberry Pi.

Drives an external SX1278 module over hardware SPI (spidev) + two GPIOs (RESET,
DIO0) via gpiozero. This is a Python port of the firmware driver used on the
node side (water-temp-node/src/sx1278.cpp) — same register sequence, so both
ends behave identically. Receive-only is all the Pi gateway needs, but a tx()
is included for symmetry / bench testing.

SX1278 is a 433 MHz (low-band) part; freq_hz also lets you use a high-band
SX1276 (868/915/923) if you have one. Both ends MUST share freq/SF/BW/CR/
syncword/preamble or they won't hear each other.
"""
import time

import spidev
from gpiozero import OutputDevice, DigitalInputDevice

# ---- SX127x registers -------------------------------------------------------
REG_FIFO = 0x00
REG_OP_MODE = 0x01
REG_FRF_MSB = 0x06
REG_FRF_MID = 0x07
REG_FRF_LSB = 0x08
REG_PA_CONFIG = 0x09
REG_LNA = 0x0C
REG_FIFO_ADDR_PTR = 0x0D
REG_FIFO_TX_BASE = 0x0E
REG_FIFO_RX_BASE = 0x0F
REG_FIFO_RX_CURRENT = 0x10
REG_IRQ_FLAGS = 0x12
REG_RX_NB_BYTES = 0x13
REG_PKT_SNR_VALUE = 0x19
REG_PKT_RSSI_VALUE = 0x1A
REG_MODEM_CONFIG1 = 0x1D
REG_MODEM_CONFIG2 = 0x1E
REG_PREAMBLE_MSB = 0x20
REG_PREAMBLE_LSB = 0x21
REG_PAYLOAD_LENGTH = 0x22
REG_MODEM_CONFIG3 = 0x26
REG_SYNC_WORD = 0x39
REG_DIO_MAPPING1 = 0x40
REG_VERSION = 0x42
REG_PA_DAC = 0x4D

# OpMode: bit7 = LoRa (LongRangeMode). Low 3 bits = mode.
MODE_LORA = 0x80
MODE_SLEEP = 0x00
MODE_STDBY = 0x01
MODE_TX = 0x03
MODE_RX_CONTINUOUS = 0x05

# IRQ flags
IRQ_RX_DONE = 0x40
IRQ_PAYLOAD_CRC_ERR = 0x20
IRQ_TX_DONE = 0x08

FXOSC = 32_000_000


class SX127x:
    def __init__(self, spi_bus=0, spi_dev=0, reset_pin=22, dio0_pin=25,
                 freq_hz=433_000_000, spi_speed_hz=2_000_000):
        self.spi = spidev.SpiDev()
        self.spi.open(spi_bus, spi_dev)
        self.spi.max_speed_hz = spi_speed_hz
        self.spi.mode = 0
        # SX127x RESET is active-low; OutputDevice.on() -> high (released).
        self._reset = OutputDevice(reset_pin, active_high=True, initial_value=True)
        self._dio0 = DigitalInputDevice(dio0_pin)
        self.freq_hz = freq_hz
        self.low_band = freq_hz < 600_000_000

    # ---- low-level SPI ------------------------------------------------------
    def _read(self, addr):
        return self.spi.xfer2([addr & 0x7F, 0x00])[1]

    def _write(self, addr, val):
        self.spi.xfer2([addr | 0x80, val & 0xFF])

    def _read_fifo(self, n):
        return self.spi.xfer2([REG_FIFO & 0x7F] + [0x00] * n)[1:]

    def _write_fifo(self, data):
        self.spi.xfer2([REG_FIFO | 0x80] + list(data))

    # ---- lifecycle ----------------------------------------------------------
    def _hw_reset(self):
        self._reset.off()   # drive low
        time.sleep(0.002)
        self._reset.on()    # release high
        time.sleep(0.006)

    def begin(self):
        """Reset + put the modem in LoRa standby at freq_hz. True if RegVersion==0x12."""
        self._hw_reset()
        self._write(REG_OP_MODE, MODE_SLEEP)               # FSK sleep
        time.sleep(0.002)
        self._write(REG_OP_MODE, MODE_LORA | MODE_SLEEP)   # LoRa sleep
        time.sleep(0.002)

        frf = int((self.freq_hz << 19) // FXOSC)
        self._write(REG_FRF_MSB, (frf >> 16) & 0xFF)
        self._write(REG_FRF_MID, (frf >> 8) & 0xFF)
        self._write(REG_FRF_LSB, frf & 0xFF)

        self._write(REG_FIFO_TX_BASE, 0x00)
        self._write(REG_FIFO_RX_BASE, 0x00)
        self._write(REG_LNA, 0x23)                         # max gain + boost
        self._write(REG_OP_MODE, MODE_LORA | MODE_STDBY)
        return self.version() == 0x12

    def version(self):
        return self._read(REG_VERSION)

    def config_lora(self, sf=9, bw_code=7, cr_code=1, preamble=8,
                    syncword=0x12, crc_on=True, power_dbm=17):
        self._write(REG_OP_MODE, MODE_LORA | MODE_STDBY)
        # ModemConfig1: BW[7:4] | CR[3:1] | ImplicitHeader(0)
        self._write(REG_MODEM_CONFIG1, ((bw_code & 0x0F) << 4) | ((cr_code & 0x07) << 1))
        # ModemConfig2: SF[7:4] | RxCrcOn[2]
        self._write(REG_MODEM_CONFIG2, ((sf & 0x0F) << 4) | (0x04 if crc_on else 0x00))
        # ModemConfig3: LowDataRateOptimize[3] (SF>=11 @125k) | AgcAutoOn[2]
        self._write(REG_MODEM_CONFIG3, (0x08 if sf >= 11 else 0x00) | 0x04)
        self._write(REG_PREAMBLE_MSB, (preamble >> 8) & 0xFF)
        self._write(REG_PREAMBLE_LSB, preamble & 0xFF)
        self._write(REG_SYNC_WORD, syncword & 0xFF)
        p = max(2, min(17, power_dbm))
        self._write(REG_PA_CONFIG, 0x80 | (p - 2))         # PA_BOOST
        self._write(REG_PA_DAC, 0x84)

    # ---- receive ------------------------------------------------------------
    def rx_start(self):
        self._write(REG_DIO_MAPPING1, 0x00)                # DIO0 = RxDone
        self._write(REG_FIFO_ADDR_PTR, 0x00)
        self._write(REG_IRQ_FLAGS, 0xFF)
        self._write(REG_OP_MODE, MODE_LORA | MODE_RX_CONTINUOUS)

    def rx_poll(self):
        """Return (payload_bytes, rssi_dbm, snr_db) on a valid frame, else None.
        Returns 'crc' on a CRC error. Non-blocking; call in a loop."""
        irq = self._read(REG_IRQ_FLAGS)
        if not (irq & IRQ_RX_DONE):
            return None
        self._write(REG_IRQ_FLAGS, 0xFF)
        if irq & IRQ_PAYLOAD_CRC_ERR:
            return "crc"
        n = self._read(REG_RX_NB_BYTES)
        self._write(REG_FIFO_ADDR_PTR, self._read(REG_FIFO_RX_CURRENT))
        payload = bytes(self._read_fifo(n))
        # RSSI offset: -164 low band (433), -157 high band (868/915).
        rssi = (-164 if self.low_band else -157) + self._read(REG_PKT_RSSI_VALUE)
        snr_raw = self._read(REG_PKT_SNR_VALUE)
        if snr_raw > 127:
            snr_raw -= 256
        snr = snr_raw / 4.0
        return (payload, rssi, snr)

    # ---- transmit (optional, for bench testing) -----------------------------
    def tx(self, data, timeout_s=2.0):
        self._write(REG_OP_MODE, MODE_LORA | MODE_STDBY)
        self._write(REG_FIFO_ADDR_PTR, 0x00)
        self._write_fifo(data)
        self._write(REG_PAYLOAD_LENGTH, len(data))
        self._write(REG_DIO_MAPPING1, 0x40)                # DIO0 = TxDone
        self._write(REG_IRQ_FLAGS, 0xFF)
        self._write(REG_OP_MODE, MODE_LORA | MODE_TX)
        t0 = time.monotonic()
        while not (self._read(REG_IRQ_FLAGS) & IRQ_TX_DONE):
            if time.monotonic() - t0 > timeout_s:
                return False
            time.sleep(0.005)
        self._write(REG_IRQ_FLAGS, IRQ_TX_DONE)
        return True

    def close(self):
        try:
            self.spi.close()
        except Exception:
            pass
