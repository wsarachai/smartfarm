#ifndef SOIL_MOISTURE_ADC_H_
#define SOIL_MOISTURE_ADC_H_

#include "esp_err.h"

esp_err_t soil_moisture_adc_init(void);
esp_err_t soil_moisture_adc_read_percent(float *soil_percent);

#endif // SOIL_MOISTURE_ADC_H_
