/*
 * Native Unity test — DHT22 5-byte frame decode + checksum.
 *
 * Characterization mirror of the pure decode logic in:
 *   esp-idf-iot/sensor-node/main/sensors/dht22.c  ->  dht22_read()
 *
 * dht22_read() drives GPIO timing and cannot run on the host. The bit-banging
 * is untestable off-hardware, but the decode of the resulting 5 raw bytes is
 * pure arithmetic and is what we lock here:
 *
 *   checksum   = (data[0]+data[1]+data[2]+data[3]) & 0xFF, compared to data[4]
 *   humidity   = ((data[0]<<8) | data[1]) / 10.0f            (16-bit, high byte first)
 *   temperature= sign in data[2]&0x80; magnitude ((data[2]&0x7F)<<8 | data[3]) / 10.0f
 *
 * KEEP IN SYNC with dht22.c. Runs via: pio test -e native
 */

#include <unity.h>
#include <stdbool.h>
#include <stdint.h>

/* Return value of decode: valid=false means checksum rejected the frame. */
typedef struct
{
    bool valid;
    float humidity;
    float temperature;
} dht_reading_t;

/* --- characterization mirror of dht22_read()'s decode section (keep in sync) --- */
static dht_reading_t dht22_decode(const uint8_t data[5])
{
    dht_reading_t out = {false, 0.0f, 0.0f};

    uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (checksum != data[4])
    {
        return out; /* reject */
    }

    out.humidity = ((data[0] << 8) | data[1]) / 10.0f;

    int16_t temp_raw = ((data[2] & 0x7F) << 8) | data[3];
    if (data[2] & 0x80)
    {
        temp_raw = -temp_raw; /* negative temperature */
    }
    out.temperature = temp_raw / 10.0f;

    out.valid = true;
    return out;
}

void setUp(void) {}
void tearDown(void) {}

/* 25.0C, 61.0% RH: hum=610=0x0262, temp=250=0x00FA. checksum=0x02+0x62+0x00+0xFA=0x15E&0xFF=0x5E */
static void test_valid_frame_positive(void)
{
    uint8_t frame[5] = {0x02, 0x62, 0x00, 0xFA, 0x5E};
    dht_reading_t r = dht22_decode(frame);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 61.0f, r.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, r.temperature);
}

/* -10.5C, 45.2% RH: hum=452=0x01C4, temp mag=105=0x0069 + sign bit -> data[2]=0x80. */
static void test_valid_frame_negative_temp(void)
{
    /* checksum = 0x01+0xC4+0x80+0x69 = 0x1AE & 0xFF = 0xAE */
    uint8_t frame[5] = {0x01, 0xC4, 0x80, 0x69, 0xAE};
    dht_reading_t r = dht22_decode(frame);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 45.2f, r.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.5f, r.temperature);
}

/* Checksum with 8-bit overflow wrap must be masked to a byte. */
static void test_checksum_wraps_to_byte(void)
{
    /* 100.0% (0x03E8) + 80.0C (0x0320): sum=0x03+0xE8+0x03+0x20=0x10E&0xFF=0x0E */
    uint8_t frame[5] = {0x03, 0xE8, 0x03, 0x20, 0x0E};
    dht_reading_t r = dht22_decode(frame);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, r.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 80.0f, r.temperature);
}

/* A single corrupted checksum byte must be rejected. */
static void test_bad_checksum_rejected(void)
{
    uint8_t frame[5] = {0x02, 0x62, 0x00, 0xFA, 0x00}; /* correct is 0x5E */
    dht_reading_t r = dht22_decode(frame);
    TEST_ASSERT_FALSE(r.valid);
}

/* All-zero frame is a valid checksum (0==0): 0% / 0C. */
static void test_zero_frame_is_valid(void)
{
    uint8_t frame[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    dht_reading_t r = dht22_decode(frame);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.temperature);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_frame_positive);
    RUN_TEST(test_valid_frame_negative_temp);
    RUN_TEST(test_checksum_wraps_to_byte);
    RUN_TEST(test_bad_checksum_rejected);
    RUN_TEST(test_zero_frame_is_valid);
    return UNITY_END();
}
