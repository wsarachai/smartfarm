#include <stdbool.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pump_config.h" // RELAY_GPIO, RELAY_ACTIVE_LEVEL (committed hardware config)
#include "relay.h"

// Derived inactive level (opposite of the active level configured in pump_config.h).
#define RELAY_INACTIVE_LEVEL ((RELAY_ACTIVE_LEVEL) ? 0 : 1)

static const char *TAG = "relay";
static bool relay_current_state = false;

esp_err_t relay_set(bool enabled)
{
	int level = enabled ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL;
	esp_err_t err = gpio_set_level(RELAY_GPIO, level);

	if (err == ESP_OK) {
		relay_current_state = enabled;
		ESP_LOGI(TAG, "Relay (pump) %s", enabled ? "ON" : "OFF");
	}

	return err;
}

bool relay_get_state(void)
{
	return relay_current_state;
}

void relay_init(void)
{
	gpio_config_t relay_gpio_config = {
		.pin_bit_mask = 1ULL << RELAY_GPIO,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};

	ESP_ERROR_CHECK(gpio_config(&relay_gpio_config));
	ESP_ERROR_CHECK(relay_set(false));
}
