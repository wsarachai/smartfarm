/*
 * Native Unity test — telemetry JSON body shape (server contract lock).
 *
 * Characterization mirror of the reshaped body format in:
 *   sensor-zone/src/.../http_client.c  (Firmware teammate owns src/)
 *
 * The new build POSTs a metrics-NESTED body to /api/v1/telemetry:
 *   {"device_id":"...","metrics":{"temperature":T,"humidity":H,"soil_moisture":S}}
 * (timestamp intentionally omitted; the server stamps it.)
 *
 * This differs deliberately from the LEGACY flat body
 *   {"device_id":"...","temperature":T,"humidity":H,"soil_moisture":S}
 * so we lock the EXACT string the web-server dashboard's metricMeta expects.
 * Format uses %.2f (two decimals) for each metric.
 *
 * KEEP IN SYNC with http_client.c's snprintf format. Runs via: pio test -e native
 */

#include <unity.h>
#include <stdio.h>
#include <string.h>

/*
 * Mirror of the new http_client body format string. If src/ changes the format,
 * change it here too and update test_exact_contract_shape's expected literal.
 */
#define TELEMETRY_BODY_FMT \
    "{\"device_id\":\"%s\",\"metrics\":{\"temperature\":%.2f,\"humidity\":%.2f,\"soil_moisture\":%.2f}}"

static int build_body(char *buf, size_t buf_len, const char *device_id,
                      float temperature, float humidity, float soil_moisture)
{
    return snprintf(buf, buf_len, TELEMETRY_BODY_FMT,
                    device_id, temperature, humidity, soil_moisture);
}

void setUp(void) {}
void tearDown(void) {}

/* The load-bearing assertion: byte-for-byte contract shape. */
static void test_exact_contract_shape(void)
{
    char body[192];
    int n = build_body(body, sizeof(body), "esp32-TEST", 27.3f, 61.0f, 44.2f);

    const char *expected =
        "{\"device_id\":\"esp32-TEST\",\"metrics\":"
        "{\"temperature\":27.30,\"humidity\":61.00,\"soil_moisture\":44.20}}";

    TEST_ASSERT_TRUE(n > 0 && n < (int)sizeof(body));
    TEST_ASSERT_EQUAL_STRING(expected, body);
}

/* Two-decimal formatting must hold for whole and fractional values. */
static void test_two_decimal_formatting(void)
{
    char body[192];
    build_body(body, sizeof(body), "esp32-ABCDEF", 0.0f, 100.0f, 5.5f);

    const char *expected =
        "{\"device_id\":\"esp32-ABCDEF\",\"metrics\":"
        "{\"temperature\":0.00,\"humidity\":100.00,\"soil_moisture\":5.50}}";

    TEST_ASSERT_EQUAL_STRING(expected, body);
}

/* Negative temperature must render with a leading minus, still 2 decimals. */
static void test_negative_temperature(void)
{
    char body[192];
    build_body(body, sizeof(body), "esp32-TEST", -3.5f, 40.0f, 12.0f);

    const char *expected =
        "{\"device_id\":\"esp32-TEST\",\"metrics\":"
        "{\"temperature\":-3.50,\"humidity\":40.00,\"soil_moisture\":12.00}}";

    TEST_ASSERT_EQUAL_STRING(expected, body);
}

/* The body must be metrics-nested, NOT the legacy flat shape. */
static void test_is_not_legacy_flat_shape(void)
{
    char body[192];
    build_body(body, sizeof(body), "esp32-TEST", 27.3f, 61.0f, 44.2f);

    TEST_ASSERT_NOT_NULL(strstr(body, "\"metrics\":{"));
    /* Legacy had temperature as a top-level key: "...\",\"temperature\":" */
    TEST_ASSERT_NULL(strstr(body, "\",\"temperature\":"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exact_contract_shape);
    RUN_TEST(test_two_decimal_formatting);
    RUN_TEST(test_negative_temperature);
    RUN_TEST(test_is_not_legacy_flat_shape);
    return UNITY_END();
}
