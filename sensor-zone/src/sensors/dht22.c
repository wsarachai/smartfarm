#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

// DHT22 sensor pin (GPIO 32)
#define DHT_PIN GPIO_NUM_32
#define DHT_TIMEOUT_US 100
#define DHT_DATA_BITS 40

/**
 * Read 8 bits from DHT22
 * Returns false if timeout, true if success
 */
static bool dht_read_bits(uint8_t *bits, int num_bits)
{
    for (int i = 0; i < num_bits; i++)
    {
        // Wait for LOW -> HIGH transition (start of bit)
        int timeout = 100;
        while (gpio_get_level(DHT_PIN) == 0 && timeout--)
        {
            esp_rom_delay_us(1);
        }
        if (timeout <= 0)
            return false;

        // Measure HIGH duration to determine bit value (0 or 1)
        esp_rom_delay_us(30); // Wait 30µs
        uint8_t bit = gpio_get_level(DHT_PIN);
        bits[i / 8] <<= 1;
        bits[i / 8] |= bit;

        // Wait for HIGH -> LOW transition (end of bit)
        timeout = 100;
        while (gpio_get_level(DHT_PIN) == 1 && timeout--)
        {
            esp_rom_delay_us(1);
        }
        if (timeout <= 0)
            return false;
    }
    return true;
}

/**
 * Read temperature and humidity from DHT22
 * Returns true if successful
 */
bool dht22_read(float *humidity, float *temperature)
{
    uint8_t data[5] = {0};

    // Prepare GPIO as open-drain output
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT_PIN),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Send start signal: pull LOW for 18ms
    gpio_set_level(DHT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(18));
    gpio_set_level(DHT_PIN, 1);

    // Switch to input mode to read
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);

    // Wait for DHT22 to respond (LOW pulse)
    int timeout = 100;
    while (gpio_get_level(DHT_PIN) == 1 && timeout--)
    {
        esp_rom_delay_us(1);
    }
    if (timeout <= 0)
    {
        printf("DHT22: No response signal detected\n");
        return false;
    }

    // Wait for response HIGH pulse
    timeout = 100;
    while (gpio_get_level(DHT_PIN) == 0 && timeout--)
    {
        esp_rom_delay_us(1);
    }
    if (timeout <= 0)
    {
        printf("DHT22: Response signal too short\n");
        return false;
    }

    // Wait for data transmission to start
    timeout = 100;
    while (gpio_get_level(DHT_PIN) == 1 && timeout--)
    {
        esp_rom_delay_us(1);
    }
    if (timeout <= 0)
    {
        printf("DHT22: No data transmission\n");
        return false;
    }

    // Read 40 bits of data
    if (!dht_read_bits(data, DHT_DATA_BITS))
    {
        printf("DHT22: Failed to read data bits\n");
        return false;
    }

    // Verify checksum
    uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (checksum != data[4])
    {
        printf("DHT22: Checksum mismatch (expected %d, got %d)\n", data[4], checksum);
        return false;
    }

    // Extract humidity (16-bit, high byte first)
    *humidity = ((data[0] << 8) | data[1]) / 10.0f;

    // Extract temperature (16-bit, high byte first, with sign bit)
    int16_t temp_raw = ((data[2] & 0x7F) << 8) | data[3];
    if (data[2] & 0x80)
    {
        temp_raw = -temp_raw; // Negative temperature
    }
    *temperature = temp_raw / 10.0f;

    return true;
}
