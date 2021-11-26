#include <stdio.h>
#include "CCS811.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LOG "CCS811"

static gpio_num_t i2c_gpio_sda = 21;
static gpio_num_t i2c_gpio_scl = 22;
static uint32_t i2c_frequency = 100000;
static i2c_port_t i2c_port = I2C_NUM_0;

short value;
i2c_cmd_handle_t cmd;

void init_i2c()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = i2c_gpio_sda,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = i2c_gpio_scl,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = i2c_frequency,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    // Install drivers and set i2c params
    // longer clock stretching is required for CCS811
    // i2c_set_clock_stretch(i2c_port, CCS811_I2C_CLOCK_STRETCH);
    ESP_ERROR_CHECK(i2c_driver_install(i2c_port, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0));
    ESP_ERROR_CHECK(i2c_param_config(i2c_port, &conf));
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
}

bool init_ccs811()
{
    const uint8_t app_start = 0;

    // Hardware id number (should be 0x81)
    if (ccs811_read_byte(CCS811_REG_HW_ID) != 0x81)
    {
        ESP_LOGE(LOG, "Hardware id dosn't match: should be 0x81 but got 0x%hhX", ccs811_read_byte(CCS811_REG_HW_ID));
        // return false;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Put 'No data' (0) in APP_START register to init sensor
    ESP_ERROR_CHECK(ccs811_write_byte(CCS811_REG_APP_START, app_start, 0));
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Read Status
    ESP_LOGI(LOG, "Status: 0x%hhX", ccs811_read_byte(CCS811_REG_STATUS) >> 7);

    // check if error in status bit
    if (ccs811_read_byte(CCS811_REG_STATUS) == CCS811_STATUS_ERROR)
    {
        // which error is in the error address
        ESP_LOGE(LOG, "Error in Status register: %d", ccs811_read_byte(CCS811_REG_ERROR_ID) >> 7);
        switch (ccs811_read_byte(CCS811_REG_ERROR_ID) >> 7)
        {
        case (CCS811_ERR_WRITE_REG_INV):
            ESP_LOGE(LOG, "Write register invalid");
            break;
        case (CCS811_ERR_READ_REG_INV):
            ESP_LOGE(LOG, "Read register invalid");
            break;
        case (CCS811_ERR_MEASMODE_INV):
            ESP_LOGE(LOG, "Meassurement Mode invalid");
            break;
        case (CCS811_ERR_MAX_RESISTANCE):
            ESP_LOGE(LOG, "Maximum sensor resistance exeeded");
            break;
        case (CCS811_ERR_HEATER_FAULT):
            ESP_LOGE(LOG, "heater current not in range");
            break;
        case (CCS811_ERR_HEATER_SUPPLY):
            ESP_LOGE(LOG, "heater voltage not applied correctly");
            break;
        
        default:
            break;
        }
        return false;
    }

    // Define Mode
    const uint8_t mode = CCS811_MODE << 4;
    ESP_ERROR_CHECK(ccs811_write_byte(CCS811_REG_MEAS_MODE, mode, 8));
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Read Mode
    ESP_LOGI(LOG, "Mode: %d", ccs811_read_byte(CCS811_REG_MEAS_MODE) >> 4);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    return true;
}

float read_ccs811(int a)
{
    // ESP_LOGI(LOG, "Status: 0x%hhX", ccs811_read_byte(CCS811_REG_STATUS) >> 6);

    // if (ccs811_read_byte(CCS811_REG_STATUS) == CCS811_STATUS_DATA_RDY)
    // {
        // ESP_LOGI(LOG, "New data ready. Reading now");
        ccs811_data d = ccs811_sensor_data(CCS811_REG_ALG_RESULT_DATA);

        switch (a)
        {
        case 0: /* CO2 */
            return d.dataCO2;
        case 1: /* VOC */
            return d.dataVOC;
        }
    // }

    return 0.0;
}

ccs811_data ccs811_sensor_data(uint8_t addr)
{
    ccs811_data data;
    uint8_t dataCO2_HB, dataCO2_LB, dataVOC_HB, dataVOC_LB;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();                                    // Command link Create
    ESP_ERROR_CHECK(i2c_master_start(cmd));                                                           // Start bit
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, CCS811_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN)); // Write an single byte address
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, addr, ACK_CHECK_EN));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));                                                            // Stop bit
    int ret = i2c_master_cmd_begin(I2C_PORT_NUMBER, cmd, TICK_DELAY);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_FAIL)
    {
        ESP_LOGE(LOG, "Sensor data cmd (1) fail");
    }

    vTaskDelay(30 / portTICK_RATE_MS);
    cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, CCS811_I2C_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN));
    ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &dataCO2_HB, ACK_VAL));
    ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &dataCO2_LB, ACK_VAL));
    ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &dataVOC_HB, ACK_VAL));
    ESP_ERROR_CHECK(i2c_master_read_byte(cmd, &dataVOC_LB, ACK_VAL));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    ret = i2c_master_cmd_begin(I2C_PORT_NUMBER, cmd, TICK_DELAY);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_FAIL)
    {
        ESP_LOGE(LOG, "Sensor data cmd (2) fail");
    }

    data.dataCO2 = (dataCO2_HB << 8) | dataCO2_LB;
    data.dataVOC = (dataVOC_HB << 8) | dataVOC_LB;

    return data;
}

esp_err_t ccs811_write_byte(uint8_t reg_addr, const uint8_t data, const uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, CCS811_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN));
    ESP_ERROR_CHECK(i2c_master_write(cmd, &data, len, ACK_CHECK_EN));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    int ret = i2c_master_cmd_begin(I2C_PORT_NUMBER, cmd, TICK_DELAY);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_FAIL)
    {
        ESP_LOGE(LOG, "Write byte error");
        return ESP_OK;
    }
    return ESP_OK;
}

uint8_t ccs811_read_byte(uint8_t addr)
{
    uint8_t data;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();                                    // Command link Create
    i2c_master_start(cmd);                                                           // Start bit
    i2c_master_write_byte(cmd, CCS811_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN); // Write an single byte address
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_stop(cmd);                                                            // Stop bit
    int write = i2c_master_cmd_begin(I2C_PORT_NUMBER, cmd, TICK_DELAY);
    i2c_cmd_link_delete(cmd);

    if (write == ESP_FAIL)
    {
        ESP_LOGE(LOG, "read byte: write command failed");
    }

    vTaskDelay(30 / portTICK_RATE_MS);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, CCS811_I2C_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &data, ACK_VAL);
    i2c_master_stop(cmd);
    int read = i2c_master_cmd_begin(I2C_PORT_NUMBER, cmd, TICK_DELAY);
    i2c_cmd_link_delete(cmd);

    if (read == ESP_FAIL)
    {
        ESP_LOGE(LOG, "read byte: read command failed");
    }

    return data;
}