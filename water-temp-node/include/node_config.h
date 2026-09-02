/*
 * node_config.h — COMMITTED hardware + behaviour contract for water-temp-node.
 * Nothing secret lives here (raw point-to-point LoRa needs no keys), so unlike
 * the ESP nodes there is no secrets.h. Edit the pins/interval to match wiring.
 *
 * Board: NUCLEO-WL55JC1 (STM32WL55JC). The front-end is a LARGE CUSTOM PCB
 * joined to the Nucleo by a CABLE, so every logic signal below lives on the ST
 * morpho header **CN10** and is reachable in one 2x19 IDC ribbon; battery power
 * arrives separately at CN6. See docs/hardware-interface.md for the connector
 * contract, the CN10 pin layout, and the reasoning.
 *
 * Pins below deliberately avoid:
 *   PC3/PC4/PC5  RF switch control (radio)  -- CN10-38 / CN10-2 / CN10-4
 *   PB0          RF_TCXO_VCC (radio TCXO)   -- CN10-22
 *   PA13/PA14    SWD (ST-LINK debug)        -- CN7-13/15
 *   PA2/PA3      ST-LINK virtual COM -- NOT on the morpho at all, and the
 *                ST-LINK is unpowered anyway. Debug goes out on PB6/PB7.
 *   PA0/PA1/PC6  user buttons B1/B2/B3      -- switch + pull-up already fitted
 *   PB9/PB11/PB15 user LEDs LED2/LED3/LED1  -- an LED across a 1-Wire line ruins it
 * ALL of these are reachable on CN10, so the front-end must avoid them by
 * intent rather than by geometry -- see docs/hardware-interface.md.
 */
#ifndef NODE_CONFIG_H
#define NODE_CONFIG_H

/* This node's id byte on the wire. The gateway maps it to a friendly device_id
 * (see lora-gateway/include/gateway_config.h). 1..255. */
#define NODE_ID                 1

/* Deep-sleep wake interval in seconds (Stop2 + RTC). 900 = 15 min. */
#define WAKE_INTERVAL_S         900

/* ---- DS18B20 1-Wire data pins ------------------------------------------- *
 * ONE PROBE PER PIN, SKIP ROM — no shared bus and no ROM search. That is what
 * makes the driver's SKIP ROM legal, lets all six convert in PARALLEL (one
 * 750 ms wait, not six), and keeps a single shorted probe from taking the other
 * five down with it.
 *
 * Probes are identified by which connector they are plugged into: probe 0 is
 * whatever hangs off DQ_P0. Probes 0 and 1 land in the wire frame's original
 * two temperature slots and are published as `temp_hot` / `temp_cold` so an
 * existing dashboard's history stays continuous (lora_packet.h, v4).
 *
 * Pin order below is ASCENDING CN10 position, so the buzz-out in
 * docs/hardware-interface.md §7 walks the connector in one direction:
 *
 *   DQ_P0  PA5   CN10-11      DQ_P3  PC2   CN10-21
 *   DQ_P1  PA4   CN10-17      DQ_P4  PB8   CN10-27   <-- moved off PC1 (2026-08)
 *   DQ_P2  PA9   CN10-19      DQ_P5  PB10  CN10-25
 *
 * DQ_P4 moved from PC1 to PB8 because PC1 is now LPUART1_TX for the S88 CO2
 * sensor -- see S88_* below. PB8 was spare and sits at CN10-27, flanked by the
 * two LED positions (26/28) which the front-end leaves open, so it keeps the
 * "every DQ has a guard neighbour" rule from docs/hardware-interface.md.
 *
 * Fewer than six probes fitted? Leave the table alone and simply do not plug
 * them in — an unconnected line reads no presence pulse and is transmitted as
 * the invalid sentinel, which the gateway drops. Set DS_PROBE_COUNT lower only
 * if you also want the ~7 ms/probe of bus traffic back.
 */
#define DS_PROBE_COUNT          6

/* Initializer for an array of ds_bus_t. Order defines the probe index. */
#define DS_PROBE_BUSES          { { GPIOA, GPIO_PIN_5  },   /* DQ_P0 */ \
                                  { GPIOA, GPIO_PIN_4  },   /* DQ_P1 */ \
                                  { GPIOA, GPIO_PIN_9  },   /* DQ_P2 */ \
                                  { GPIOC, GPIO_PIN_2  },   /* DQ_P3 */ \
                                  { GPIOB, GPIO_PIN_8  },   /* DQ_P4 */ \
                                  { GPIOB, GPIO_PIN_10 } }  /* DQ_P5 */

/* Every GPIO bank the table above (and the gate below) touches. */
#define DS_PROBE_GPIO_CLK()     do { __HAL_RCC_GPIOA_CLK_ENABLE(); \
                                     __HAL_RCC_GPIOB_CLK_ENABLE(); \
                                     __HAL_RCC_GPIOC_CLK_ENABLE(); } while (0)

/* ---- Sensor rail power gate (AO3401A P-MOSFET high-side, active-LOW) -------- */
/* LOW  = gate pulled low  = P-FET ON  = sensor rail powered.
 * HIGH = gate = source     = P-FET OFF = rail off (also the Hi-Z sleep state,
 *        held OFF by the external 100k gate->3V3 pull-up).
 *
 * PA8 (CN10-16) sits inside the DQ block, so the gate and the lines it powers
 * travel the same ribbon. Its alternate functions are unused here, and the WL
 * has no boot strap on it (BOOT0 is PH3, nBOOT1 is an option byte), so it
 * resets to a floating input -- which is exactly what the external 100k gate
 * pull-up needs in order to hold the rail off before firmware runs.
 *
 * NOTE the name: DS_PWR_* predates the SHT45s and gates the whole 3.3 V sensor
 * rail, not just the DS18B20 probes. The docs call this signal SENS_GATE.
 *
 * 2026-08: its PURPOSE changed. On the old battery node it existed to stop uA of
 * leakage mattering; on 24 V solar that is irrelevant. It is kept because it is
 * the only reliable way to recover a hung I2C bus -- three unshielded 5 m
 * branches in a greenhouse WILL latch SDA low eventually. On a failed
 * transaction: gate off, wait, gate on, retry. The S88 is NOT behind it. */
#define DS_PWR_PORT             GPIOA
#define DS_PWR_PIN              GPIO_PIN_8
#define DS_PWR_GPIO_CLK()       __HAL_RCC_GPIOA_CLK_ENABLE()
#define DS_PWR_ON_LEVEL         GPIO_PIN_RESET   /* active-low */
#define DS_PWR_OFF_LEVEL        GPIO_PIN_SET

/* Settling time after powering the rail before starting a conversion (ms). */
#define DS_POWER_SETTLE_MS      10

/* DS18B20 12-bit conversion time (ms). Drop to ~94 ms if you set 9-bit res.
 * Unchanged by the probe count: the probes convert in parallel. */
#define DS_CONVERT_MS           750

/* ---- I2C sensor bus (the three SHT45s -- nothing else) -------------------- */
/* I2C2 on PA11/PA12 (CN10-5 / CN10-3). Chosen over I2C1 (PA9/PA10 -- PA9 is a
 * probe) and I2C3 (PB10/PB11 -- PB10 is a probe and PB11 is the Nucleo's LED3,
 * which would sit on SDA).
 *
 * 2026-08: the SCD41 LEFT this bus. CO2 is now a Senseair S88 LP on Modbus/RS-485
 * (see S88_* below), so the ONLY things on I2C are the TCA9548A and the three
 * SHT45s behind it. The SHT45s are no longer on the board either -- each sits at
 * the far end of its own <=5 m cable, which is why the downstream pull-ups are
 * 2.2k rather than 4.7k. Still on the gated VSENS rail. */
#define I2C_SDA_PIN             PA11
#define I2C_SCL_PIN             PA12
#define I2C_CLOCK_HZ            100000   /* 100 kHz: kind to three 5 m branches */

/* ---- SHT45 air temp + humidity: THREE of them ---------------------------
 * The SHT4x I2C address is fixed at the factory by the order code
 * (SHT45-AD1B = 0x44). There is no address pin, so three of them CANNOT share
 * a bus, and power-gating them individually does not help either -- an
 * unpowered part still clamps SDA/SCL to its own VDD through its ESD diodes
 * and phantom-powers itself to ~2.7 V, where it happily answers at 0x44.
 *
 * So the three sit behind a TCA9548A bus switch, one channel connected at a
 * time. Nothing else is on this bus any more -- the SCD41 that used to sit
 * upstream at 0x62 has been replaced by the S88 LP on RS-485. With every channel
 * closed, an I2C scan should now find 0x70 and nothing else. See src/tca9548a.h
 * and docs/hardware-interface.md.
 */
#define SHT45_COUNT             3
#define SHT45_ADDR              0x44   /* all three -- factory-fixed */

/* TCA9548A bus switch: address (A2/A1/A0 strapping) and the channel each
 * SHT45 hangs off. Index == sensor index in the LoRa frame and on the
 * dashboard, so sensor 0 is whatever is on I2C_MUX_CHANNELS[0].
 *
 * 2026-08: each channel now drives its OWN <=5 m cable to a sensor somewhere
 * else. The mux is what makes that work -- only the selected branch's ~320 pF
 * loads the bus, so three 5 m branches never add up.
 *
 *   ch0 -> SHT45 #0  head of the greenhouse   (frame slot 0, historic series)
 *   ch1 -> SHT45 #1  tail of the greenhouse   (frame slot 1)
 *   ch2 -> SHT45 #2  OUTSIDE, ambient ref     (frame slot 2, radiation shield)
 *
 * Swapping two branch cables silently relabels the data and it still looks
 * plausible -- label both ends. */
#define I2C_MUX_ADDR            0x70
#define I2C_MUX_CHANNELS        { 0, 1, 2 }

/* ---- CO2: Senseair S88 LP on Modbus RTU / RS-485 -------------------------- *
 * REPLACED THE SCD41 (2026-08). This is not a drop-in:
 *   - UART/Modbus, NOT I2C. It is not on the I2C bus at all.
 *   - 4.5-5.25 V, so it has its own buck rail, not VSENS.
 *   - It runs CONTINUOUSLY and is NOT gated by SENS_GATE. Its ABC (Automatic
 *     Baseline Correction) has an 8-day period and the datasheet specifies
 *     accuracy at continuous operation; at the old 0.28% duty cycle eight days
 *     of powered time would have taken 7.8 years and ABC would never complete.
 *     On 24 V solar its 18 mA average (~2.2 Wh/day) is ~3% of a 20 W panel.
 *
 * So CO2_ENABLED / CO2_EVERY_N_WAKES / CO2_SINGLE_SHOT_WARMUP are GONE: there is
 * nothing to pace and nothing to warm up. Each wake just reads the latest value.
 *
 * LPUART1 on PC1/PC0 (CN10-23 / CN10-14). USART1 is not available (its TX pins
 * are PA9 = DQ_P2 and PB6 = the debug header), and USART2 exists only on PA2/PA3.
 * Freeing PC1 is why DQ_P4 moved to PB8.
 *
 * Register map: TDE14367 "Modbus on Senseair S88" rev 5 (2024-09-04), NOT the
 * product spec. Verified 2026-09-02:
 *   - CO2 is INPUT register IR4, address 0x0003, read with FUNCTION 0x04. It is
 *     not a holding register; HR4 at the same address is the pressure setting.
 *   - Sensor default is 9600 8N1 only; it REPLIES with 2 stop bits (harmless).
 *   - Response time-out is 180 ms max; silent interval 3.5 chars (~4 ms).
 *   - Address 0xFE is "any sensor": every S88 answers it regardless of its own
 *     address. Senseair says production/test only because it violates Modbus
 *     on a multi-drop bus -- this link is point-to-point with ONE slave, so it
 *     is the right choice here and survives a sensor swap. HR20 holds the
 *     individual address (1..254) if a second device ever shares the pair. */
#define S88_UART_TX_PIN         PC1
#define S88_UART_RX_PIN         PC0
#define S88_UART_BAUD           9600     /* 8N1, Modbus RTU framing + CRC16 */
/* RS-485 driver enable. DE and !RE are tied together on the transceiver, so
 * HIGH = transmitting, LOW = listening. s88.cpp raises it, writes, then calls
 * flush() -- which blocks until the last stop bit has actually left the shift
 * register -- before dropping it. Dropping DE on a delay instead of on the
 * transmit-complete flag truncates the frame; that is the classic RS-485 bug.
 * An auto-direction transceiver would remove this pin, but the common ones
 * (MAX13487E) are 5 V parts whose RO would drive 5 V into a non-5V-tolerant
 * STM32WL pin. PA7 was spare; use it. */
#define S88_DE_PIN              PA7
#define S88_MODBUS_ADDR         0xFE     /* "any sensor" -- see above */
#define S88_IR_STATUS           0x0000   /* IR1 MeterStatus; IR4 CO2 = +3, read
                                          * together with FC 0x04, count 4 */
#define S88_HR_PRESSURE         0x0003   /* HR4, live, RAM, LSB 0.1 hPa */
#define S88_HR_DEFAULT_PRESSURE 0x001A   /* HR27, EEPROM, loaded into HR4 at
                                          * power-up. 0 = compensation off */
#define S88_RESPONSE_TIMEOUT_MS 200      /* >= the sensor's 180 ms maximum */
#define S88_RETRIES             2        /* Modbus has CRC16: a bad frame is
                                          * detectable, so retry rather than
                                          * publish a corrupted reading. */
/* Site pressure for the S88's built-in compensation, in 0.1 hPa. The sensor
 * reads 1.6 % low per kPa below 1013.25 hPa -- ~5 % low at 330 m (Maejo) and
 * ~9 % at 500 m -- so this is NOT a cosmetic setting. Standard atmosphere:
 * 0 m 10132, 100 m 10012, 200 m 9894, 300 m 9777, 400 m 9661, 500 m 9546.
 * 0 leaves the sensor untouched (factory default: compensation disabled).
 * Written to HR27 (EEPROM) only when it differs -- see s88_apply_site_pressure. */
#define S88_SITE_PRESSURE_DHPA  0

/* ---- Sensor rail settle time for the I2C parts --------------------------- */
/* Was 1000 ms for the SCD41's power-up, which dominated every wake and had to be
 * overlapped with the 750 ms DS18B20 conversion to avoid paying it twice. With
 * the SCD41 gone the only I2C parts are the SHT45s (~1 ms) and the mux, so this
 * collapses to the same 10 ms as the probes. Rail-on time is now set purely by
 * DS_CONVERT_MS. The S88 needs no settle window: it is never powered down. */
#define I2C_POWER_SETTLE_MS     10

/* ---- Supply telemetry: 24 V bank via a divider ---------------------------- *
 * battery_read_mv()'s VREFINT trick is DEAD. It measured VDDA, which is now the
 * 3.3 V buck output -- the same number whether the bank is full or flat.
 *
 * Replacement: a 300k/30k divider (11:1) off the 24 V input to PB3 = ADC1_IN2,
 * the ONLY ADC-capable pin left on CN10. 32 V in -> 2.9 V at the pin.
 *
 * VREFINT is still read, for a different job: recover the ACTUAL VDDA so the
 * divider reading can be scaled against it. Otherwise the buck's tolerance shows
 * up as error on every reported bank voltage. */
#define VBAT_SENSE_PIN          PB3
#define VBAT_SENSE_NUM          300000UL  /* R24, ohms */
#define VBAT_SENSE_DEN          30000UL   /* R25, ohms */
#define VBAT_SENSE_SAMPLE_US    100       /* 300k||30k = 27k source impedance */

/* ---- Debug UART (USART1 -> 3-pin header J13, NOT the ST-LINK VCP) --------- *
 * The VCP is gone: feeding 3V3 directly at CN6-4 leaves the ST-LINK unpowered,
 * so PA2/PA3 reach nothing. USART1 on PB6/PB7 (CN10-35/37) goes out to a 3-pin
 * header instead and you attach a USB-serial adapter. Those two positions were
 * marked forbidden while they were assumed to be the VCP; they are not -- PA2/PA3
 * are not on the morpho at all. Comment out to drop all serial logging. */
#define DEBUG_UART_ENABLED      1
#define DEBUG_UART_TX_PIN       PB6
#define DEBUG_UART_RX_PIN       PB7
#define DEBUG_UART_BAUD         115200

/* Software bound on a single TX attempt (ms) before we give up and sleep. */
#define LORA_TX_TIMEOUT_MS      4000

#endif /* NODE_CONFIG_H */
