#include "hardware_adc.h"
#include "common.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static adc_oneshot_unit_handle_t adc1_handle;

static const adc_oneshot_chan_cfg_t adc_cfg = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
};

int adc_init()
{
    adc_oneshot_unit_init_cfg_t init_cfg{};
    init_cfg.unit_id = ADC_UNIT_1;
#if defined(ADC_CLK_SRC_DEFAULT)
    init_cfg.clk_src = ADC_CLK_SRC_DEFAULT;
#endif
#if defined(ADC_ULP_MODE_DISABLE)
    init_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;
#endif

    esp_err_t ret = adc_oneshot_new_unit(&init_cfg, &adc1_handle);
    printf("[ADC] new_unit -> %d\n", ret);

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, (adc_channel_t)ADC1_CHANNEL_BATTERY, &adc_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, (adc_channel_t)ADC1_CHANNEL_JOYX, &adc_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, (adc_channel_t)ADC1_CHANNEL_JOYY, &adc_cfg));

    return 0;
}

int adc_read_vbatt()
{
    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, (adc_channel_t)ADC1_CHANNEL_BATTERY, &raw));
    int mv = (raw * 3300) / 4095;
    return mv * 2; // diviseur de tension
}

int adc_read_vbatt_percent()
{
    int mv = adc_read_vbatt();
    if (mv < 3300) mv = 3300;
    if (mv > 4200) mv = 4200;
    return (mv - 3300) * 100 / (4200 - 3300);
}

int adc_read_joyx()
{
    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, (adc_channel_t)ADC1_CHANNEL_JOYX, &raw));
    return (raw * JOYX_MAX) / 4095;
}

int adc_read_joyy()
{
    int raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, (adc_channel_t)ADC1_CHANNEL_JOYY, &raw));
    return (raw * JOYX_MAX) / 4095;
}
