#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"

#include "MH_Z19.h"
#include "CCS811.h"

#define LOG "MAIN"

void get_values()
{
    int co2 = read_co2();
    ESP_LOGI(LOG, "Co2: %i ppm", co2);

    int eco2 = read_ccs811(0);
    ESP_LOGI(LOG, "eCO2: %i ppm", eco2);

    int tvoc = read_ccs811(1);
    ESP_LOGI(LOG, "tvoc: %i ppb", tvoc);
}

void app_main(void)
{
    ESP_LOGI(LOG, "Init Uart");
    init_uart();
    ESP_LOGI(LOG, "Init I2C");
    init_i2c();
    ESP_LOGI(LOG, "Init ccs811");
    init_ccs811();

    const esp_timer_create_args_t sensor_timer_args = {
        .callback = &get_values,
        .name = "Read sensor values"};
    esp_timer_handle_t sensor_timer;
    ESP_ERROR_CHECK(esp_timer_create(&sensor_timer_args, &sensor_timer));

    ESP_ERROR_CHECK(esp_timer_start_periodic(sensor_timer, 5 * 1000000));
}
