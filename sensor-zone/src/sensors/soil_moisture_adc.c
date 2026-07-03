#include "soil_moisture_adc.h"

#include <stdbool.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"

#define SOIL_ADC_UNIT ADC_UNIT_1
#define SOIL_ADC_CHANNEL ADC_CHANNEL_6
#define SOIL_ADC_ATTEN ADC_ATTEN_DB_12
#define SOIL_ADC_SAMPLES_PER_READING 16

// Calibrate these for your sensor/probe.
#define SOIL_DRY_MV 2800
#define SOIL_WET_MV 1200

static const char *TAG = "soil_adc";

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_adc_cali_handle = NULL;
static bool s_adc_calibration_enabled = false;
static bool s_initialized = false;

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

static int map_voltage_to_percent(int millivolts)
{
    const int input_span = SOIL_WET_MV - SOIL_DRY_MV;

    if (input_span == 0)
    {
        return 0;
    }

    const int percent = ((millivolts - SOIL_DRY_MV) * 100) / input_span;
    return clamp_int(percent, 0, 100);
}

static esp_err_t init_adc_calibration(void)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = SOIL_ADC_UNIT,
        .chan = SOIL_ADC_CHANNEL,
        .atten = SOIL_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(adc_cali_create_scheme_curve_fitting(&cali_config, &s_adc_cali_handle), TAG,
                        "curve fitting calibration init failed");
    s_adc_calibration_enabled = true;
    return ESP_OK;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = SOIL_ADC_UNIT,
        .atten = SOIL_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(adc_cali_create_scheme_line_fitting(&cali_config, &s_adc_cali_handle), TAG,
                        "line fitting calibration init failed");
    s_adc_calibration_enabled = true;
    return ESP_OK;
#else
    ESP_LOGW(TAG, "ADC calibration scheme not supported on this target");
    s_adc_calibration_enabled = false;
    return ESP_OK;
#endif
}

esp_err_t soil_moisture_adc_init(void)
{
    if (s_initialized)
    {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = SOIL_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = SOIL_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &s_adc_handle), TAG,
                        "adc oneshot unit init failed");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc_handle, SOIL_ADC_CHANNEL, &channel_config), TAG,
                        "adc oneshot channel config failed");

    esp_err_t cal_err = init_adc_calibration();
    if (cal_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Continuing without calibration");
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Soil moisture ADC initialized");
    return ESP_OK;
}

esp_err_t soil_moisture_adc_read_percent(float *soil_percent)
{
    if (soil_percent == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || s_adc_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    int raw_sum = 0;
    for (int sample = 0; sample < SOIL_ADC_SAMPLES_PER_READING; ++sample)
    {
        int raw_value = 0;
        esp_err_t err = adc_oneshot_read(s_adc_handle, SOIL_ADC_CHANNEL, &raw_value);
        if (err != ESP_OK)
        {
            return err;
        }
        raw_sum += raw_value;
    }

    int raw_average = raw_sum / SOIL_ADC_SAMPLES_PER_READING;

    if (s_adc_calibration_enabled)
    {
        int millivolts = 0;
        ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(s_adc_cali_handle, raw_average, &millivolts), TAG,
                            "adc raw to voltage failed");
        *soil_percent = (float)map_voltage_to_percent(millivolts);
        return ESP_OK;
    }

    // Fallback mapping on raw ADC when calibration is unavailable.
    int fallback_percent = ((4095 - raw_average) * 100) / 4095;
    fallback_percent = clamp_int(fallback_percent, 0, 100);
    *soil_percent = (float)fallback_percent;
    return ESP_OK;
}
