#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"
#include <nvs_flash.h>

#include "app_wifi.h"
#include "homekit.h"
#include "MH_Z19.h"
#include "CCS811.h"

#define LOG "MAIN"

// void get_values()
// {
//     int co2 = read_co2();
//     ESP_LOGI(LOG, "Co2: %i ppm", co2);

//     int eco2 = read_ccs811(0);
//     ESP_LOGI(LOG, "eCO2: %i ppm", eco2);

//     int tvoc = read_ccs811(1);
//     ESP_LOGI(LOG, "tvoc: %i ppb", tvoc);
// }

void app_main(void)
{
    // Initialize NVS partition
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        ret = nvs_flash_erase();
        // Retry nvs_flash_init
        ret |= nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        printf("Failed to init NVS");
        return;
    }

    ESP_LOGI(LOG, "Init Uart");
    init_uart();
    ESP_LOGI(LOG, "Init I2C");
    init_i2c();
    ESP_LOGI(LOG, "Init ccs811");
    init_ccs811();

    /* Initialize Wi-Fi */
    app_wifi_init();

    ESP_LOGI(LOG, "Wifi initialized");

    /* Start Wi-Fi */
    app_wifi_start(portMAX_DELAY);

    ESP_LOGI(LOG, "Init Homekit");
    homekit_server_start();
    ESP_LOGI(LOG, "stack remaining free in main %d", uxTaskGetStackHighWaterMark(NULL));

    // const esp_timer_create_args_t sensor_timer_args = {
    //     .callback = &get_values,
    //     .name = "Read sensor values"};
    // esp_timer_handle_t sensor_timer;
    // ESP_ERROR_CHECK(esp_timer_create(&sensor_timer_args, &sensor_timer));

    // ESP_ERROR_CHECK(esp_timer_start_periodic(sensor_timer, 5 * 1000000));
}
