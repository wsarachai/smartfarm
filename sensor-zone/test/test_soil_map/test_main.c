/*
 * Native Unity test — soil moisture voltage->percent mapping.
 *
 * Characterization mirror of:
 *   esp-idf-iot/sensor-node/main/sensors/soil_moisture_adc.c
 *     - static int clamp_int(int, int, int)
 *     - static int map_voltage_to_percent(int millivolts)
 *
 * The source functions are `static` and depend on the ESP-IDF ADC driver, so
 * they cannot be linked on the host. These are byte-for-byte re-implementations
 * of the pure integer logic. KEEP IN SYNC with the source: if the calibration
 * constants or the mapping arithmetic change in soil_moisture_adc.c, update the
 * mirror below AND these assertions.
 *
 * Runs via: pio test -e native
 */

#include <unity.h>

/* --- mirror of src constants (soil_moisture_adc.c) --- */
#define SOIL_DRY_MV 2800 /* probe in dry air  -> 0%   */
#define SOIL_WET_MV 1200 /* probe in water    -> 100% */

/* --- characterization mirror: clamp_int (keep in sync) --- */
static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

/* --- characterization mirror: map_voltage_to_percent (keep in sync) --- */
static int map_voltage_to_percent(int millivolts)
{
    const int input_span = SOIL_WET_MV - SOIL_DRY_MV; /* = -1600 */

    if (input_span == 0)
    {
        return 0;
    }

    /* NOTE: integer division, matches source exactly (truncation toward zero). */
    const int percent = ((millivolts - SOIL_DRY_MV) * 100) / input_span;
    return clamp_int(percent, 0, 100);
}

void setUp(void) {}
void tearDown(void) {}

/* ---------------- clamp_int ---------------- */

static void test_clamp_within_range(void)
{
    TEST_ASSERT_EQUAL_INT(50, clamp_int(50, 0, 100));
    TEST_ASSERT_EQUAL_INT(0, clamp_int(0, 0, 100));
    TEST_ASSERT_EQUAL_INT(100, clamp_int(100, 0, 100));
}

static void test_clamp_below_and_above(void)
{
    TEST_ASSERT_EQUAL_INT(0, clamp_int(-5, 0, 100));
    TEST_ASSERT_EQUAL_INT(0, clamp_int(-1000, 0, 100));
    TEST_ASSERT_EQUAL_INT(100, clamp_int(101, 0, 100));
    TEST_ASSERT_EQUAL_INT(100, clamp_int(999999, 0, 100));
}

/* ---------------- map_voltage_to_percent ---------------- */

static void test_map_endpoints(void)
{
    /* DRY endpoint -> 0%, WET endpoint -> 100% */
    TEST_ASSERT_EQUAL_INT(0, map_voltage_to_percent(SOIL_DRY_MV)); /* 2800mV */
    TEST_ASSERT_EQUAL_INT(100, map_voltage_to_percent(SOIL_WET_MV)); /* 1200mV */
}

static void test_map_midpoint(void)
{
    /* 2000mV is exactly halfway between 2800 (dry) and 1200 (wet) -> 50% */
    TEST_ASSERT_EQUAL_INT(50, map_voltage_to_percent(2000));
}

static void test_map_out_of_range_is_clamped(void)
{
    /* Drier than DRY (higher mV) clamps to 0 */
    TEST_ASSERT_EQUAL_INT(0, map_voltage_to_percent(3300));
    TEST_ASSERT_EQUAL_INT(0, map_voltage_to_percent(2801));
    /* Wetter than WET (lower mV) clamps to 100 */
    TEST_ASSERT_EQUAL_INT(100, map_voltage_to_percent(1199));
    TEST_ASSERT_EQUAL_INT(100, map_voltage_to_percent(0));
}

static void test_map_quarter_points(void)
{
    /* 2400mV -> 25%, 1600mV -> 75% (linear) */
    TEST_ASSERT_EQUAL_INT(25, map_voltage_to_percent(2400));
    TEST_ASSERT_EQUAL_INT(75, map_voltage_to_percent(1600));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_clamp_within_range);
    RUN_TEST(test_clamp_below_and_above);
    RUN_TEST(test_map_endpoints);
    RUN_TEST(test_map_midpoint);
    RUN_TEST(test_map_out_of_range_is_clamped);
    RUN_TEST(test_map_quarter_points);
    return UNITY_END();
}
