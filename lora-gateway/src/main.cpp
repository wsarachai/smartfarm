/*
 * lora-gateway — main.cpp  (STM32duino / Arduino framework)
 *
 * NUCLEO-WL55JC1 receive-only LoRa gateway. Listens continuously on the shared
 * AS923 channel, validates each frame's CRC, maps node_id -> device_id, and
 * prints ONE JSON line per frame out Serial (the ST-LINK virtual COM port). A
 * host-side bridge (lora-gateway/bridge/) reads those lines and POSTs them to
 * the web-server's /api/v1/telemetry.
 *
 * Line format (rssi/snr added here from the LoRa RX):
 *   {"device_id":"water-temp-01","metrics":{"temp_hot":41.30,"temp_cold":22.60,
 *    "temp_p2":23.10,"temp_p3":23.44,"battery_v":3.140,"air_temp":24.13,
 *    "humidity":58.20,"co2":812,"rssi":-92,"snr":8.5,"seq":42}}
 * Non-JSON lines (starting with '#') are diagnostics the bridge ignores.
 *
 * Which metrics appear depends on the frame version and its valid-flags — the
 * node decides, this just forwards. v1 frames carry 2 water temps + battery; v2
 * adds air temp/humidity/pressure; v3 adds CO2; v4 widens the temperatures from
 * 2 probes to 6; v5 widens the air sensors from 1 SHT45 to 3.
 * lora_packet_unpack() accepts all five, so an older node in the field keeps
 * working unchanged and keeps its metric names.
 *
 * Probe and air-sensor metric names come from gw_probe_metric() /
 * gw_air_metric() / gw_hum_metric() in gateway_config.h; rename them there.
 *
 * The LoRa driver (src/lora/subghz_lora.c) is our own HAL_SUBGHZ command driver;
 * STM32duino IS the STM32Cube HAL underneath, so it compiles unchanged.
 */
#include <Arduino.h>
#include <stdio.h>

extern "C" {
#include "lora/subghz_lora.h"
}
#include "lora/lora_packet.h"
#include "gateway_config.h"

/* -------------------------------------------------------------------------- */
/* Append "<int>.<NN>" for a centi value (e.g. -412 -> "-4.12"), no float printf. */
static int append_centi(char *dst, int cap, int centi)
{
    int neg = centi < 0;
    int a = neg ? -centi : centi;
    return snprintf(dst, cap, "%s%d.%02d", neg ? "-" : "", a / 100, a % 100);
}

/*
 * Append helpers that cannot run past `cap`.
 *
 * snprintf returns the length it WANTED to write, not what it wrote, so a bare
 * `n += snprintf(...)` lets n sail past cap and the next call then computes
 * `out + n` beyond the end of the buffer. With two probes the line could not get
 * close to the cap; with six it can, so clamp instead of relying on that.
 * Both macros read `out`, `cap` and `n` from the enclosing scope.
 */
#define JADD(...)  do { \
        if (n < cap) n += snprintf(out + n, (size_t)(cap - n), __VA_ARGS__); \
        if (n > cap) n = cap; \
    } while (0)
#define JCENTI(v)  do { \
        if (n < cap) n += append_centi(out + n, cap - n, (v)); \
        if (n > cap) n = cap; \
    } while (0)

/* Build the telemetry JSON line into out. Returns the length written (<= cap). */
static int build_json(char *out, int cap, const lora_payload_t *p,
                      int rssi, int snr_tenths)
{
    char idbuf[24];
    const char *id = gw_device_id(p->node_id);
    if (id == 0) {
        snprintf(idbuf, sizeof(idbuf), "water-node-%u", (unsigned)p->node_id);
        id = idbuf;
    }

    int n = 0;
    JADD("{\"device_id\":\"%s\",\"metrics\":{", id);

    int first = 1;
    /* Temperature probes. A v1/v2/v3 frame carries 2, a v4 frame 6; probes that
     * are absent, unfitted or failed are excluded by lora_probe_valid() and
     * simply do not appear in the JSON — the dashboard's schema is a sparse
     * metrics dict, so a missing key is the right way to say "no reading". */
    for (int i = 0; i < LORA_PROBE_MAX; i++) {
        if (!lora_probe_valid(p, i)) continue;
        JADD("%s\"%s\":", first ? "" : ",", gw_probe_metric(i));
        JCENTI(p->probe_c100[i]);
        first = 0;
    }
    if (p->flags & LORA_FLAG_BATT) {
        JADD("%s\"battery_v\":%u.%03u", first ? "" : ",",
             (unsigned)(p->battery_mv / 1000),
             (unsigned)(p->battery_mv % 1000));
        first = 0;
    }
    /* Air sensors. v2/v3/v4 carry 1, a v5 frame 3. Sensor 0 gates on the AIR/
     * HUM flags, sensors 1/2 on their sentinels — lora_air_valid() knows. */
    for (int i = 0; i < LORA_AIR_MAX; i++) {
        if (lora_air_valid(p, i)) {
            JADD("%s\"%s\":", first ? "" : ",", gw_air_metric(i));
            JCENTI(p->air_temp_c100[i]);
            first = 0;
        }
        if (lora_hum_valid(p, i)) {
            JADD("%s\"%s\":%u.%02u", first ? "" : ",", gw_hum_metric(i),
                 (unsigned)(p->humidity_x100[i] / 100),
                 (unsigned)(p->humidity_x100[i] % 100));
            first = 0;
        }
    }
    if (p->flags & LORA_FLAG_PRESS) {
        JADD("%s\"pressure\":%u.%u", first ? "" : ",",
             (unsigned)(p->pressure_dhpa / 10),
             (unsigned)(p->pressure_dhpa % 10));
        first = 0;
    }
    if (p->flags & LORA_FLAG_CO2) {
        JADD("%s\"co2\":%u", first ? "" : ",", (unsigned)p->co2_ppm);
        first = 0;
    }
    int sneg = snr_tenths < 0;
    int sa = sneg ? -snr_tenths : snr_tenths;
    JADD("%s\"rssi\":%d,\"snr\":%s%d.%d,\"seq\":%u}}",
         first ? "" : ",", rssi,
         sneg ? "-" : "", sa / 10, sa % 10, (unsigned)p->seq);
    return n;
}

#undef JADD
#undef JCENTI

/* -------------------------------------------------------------------------- */
void setup(void)
{
    Serial.begin(GW_UART_BAUD);
#if defined(LED_BUILTIN)
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
#endif

    Serial.println("# lora-gateway up: AS923 923.2MHz SF9BW125, RX-only");

    if (subghz_lora_init() != 0) {
        Serial.println("# radio init FAILED");
        while (1) { delay(1000); }
    }
    subghz_lora_recv_start();
    Serial.println("# listening");
}

void loop(void)
{
    uint8_t buf[LORA_PKT_LEN_MAX + 4];   /* longest frame any version sends */
    uint8_t len = 0;
    int     rssi = 0;
    float   snr = 0.0f;

    int r = subghz_lora_recv_poll(buf, sizeof(buf), &len, &rssi, &snr);
    if (r == 1) {
        lora_payload_t p;
        if (lora_packet_unpack(buf, len, &p)) {
            int snr_tenths = (int)(snr * 10.0f + (snr >= 0 ? 0.5f : -0.5f));
            /* Worst case is a v5 frame: 6 probes, 3 air temps, 3 humidities
             * plus every other metric, ~350 chars. build_json truncates
             * rather than overruns. */
            char line[512];
            build_json(line, sizeof(line), &p, rssi, snr_tenths);
            Serial.println(line);
#if defined(LED_BUILTIN)
            digitalWrite(LED_BUILTIN, HIGH);
            delay(15);
            digitalWrite(LED_BUILTIN, LOW);
#endif
        }
        /* CRC/magic already validated in unpack; bad frames silently dropped. */
    } else if (r < 0) {
        subghz_lora_recv_start();   /* transient radio error -> re-arm RX */
    }
    delay(2);
}
