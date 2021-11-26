#include <stdio.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "MH_Z19.h"

#define LOG "MH_Z19C"

#define RXD2 (GPIO_NUM_16)
#define TXD2 (GPIO_NUM_17)
#define BUF_SIZE (1024)

char checksum(char *packet)
{
    uint8_t i;
    unsigned char checksum = 0;
    for (i = 1; i < 8; i++)
    {
        checksum += packet[i];
    }
    checksum = 0xff - checksum;
    checksum += 1;
    return checksum;
}

void init_uart()
{
    const uart_port_t uart_num = UART_NUM_2;
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    // Set UART pins(TX: IO4, RX: IO5, RTS: IO18, CTS: IO19)
    ESP_ERROR_CHECK(uart_set_pin(uart_num, RXD2, TXD2, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Setup UART buffered IO with event queue
    const int uart_buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;
    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
}

int read_co2()
{
    int result = -1;
    char data_read[9];
    //command to get co2 concentration
    char data_write[9] = {0xff, 0x01, 0x86, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x79};
    uart_flush(UART_NUM_2);
    int res = uart_write_bytes(UART_NUM_2, data_write, 9);
    if (res != 9)
    {
        ESP_LOGW(LOG, "Not all bytes where send: %i", res);
    }

    res = uart_read_bytes(UART_NUM_2, data_read, 9, 100);
    if (res != 9)
    {
        ESP_LOGW(LOG, "Not all bytes where recieved: %i", res);
    }
    if (data_read[1] != 0x86) 
    {
        ESP_LOGW(LOG, "Command dosn't match: %c", data_read[1]);
    }
    if (data_read[8] != checksum(data_read))
    {
        ESP_LOGW(LOG, "Checksums don't match: got 0x%hhX, calculated 0x%hhX", data_read[8], checksum(data_read));
    }

    result = data_read[2] * 256 + data_read[3];

    return result;
}

void set_self_calibration(bool self_calibration)
{
    //command to get co2 concentration
    char data_write[9] = {0xff, 0x01, 0x79, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x79};

    if (self_calibration) { 
        data_write[3] = 0xa0; 
    } else { 
        data_write[3] = 0x00;
    };

    uart_flush(UART_NUM_2);
    int res = uart_write_bytes(UART_NUM_2, data_write, 9);
    if (res != 9)
    {
        ESP_LOGW(LOG, "Not all bytes where send: %i", res);
    }
}