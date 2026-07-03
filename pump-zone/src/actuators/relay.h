#ifndef RELAY_H
#define RELAY_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize the relay GPIO and set it to the inactive (pump OFF) state.
 */
void relay_init(void);

/**
 * @brief Control the relay (pump) state.
 *
 * @param enabled true to activate the relay (pump ON), false to deactivate.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t relay_set(bool enabled);

/**
 * @brief Get the current relay state.
 *
 * @return true if the relay is ON, false if OFF.
 */
bool relay_get_state(void);

#endif // RELAY_H
