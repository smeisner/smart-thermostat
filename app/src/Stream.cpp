#include "Stream.h"

Stream::Stream()
{
}

Stream::~Stream() 
{
}

/// @brief Open UART port and set operating parameters
/// @param _uart_port 
/// @param baud_rate 
/// @param gpio_tx 
/// @param gpio_rx 
/// @return 
bool Stream::begin(uart_port_t _uart_port, int baud_rate, int gpio_tx, int gpio_rx)
{
    esp_err_t ret;

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0, //122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

    uart_port = _uart_port;
    timeout = 5000;

    ESP_ERROR_CHECK(uart_driver_install(uart_port, BUF_SIZE, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(uart_port, &uart_config));
    ESP_ERROR_CHECK(ret = uart_set_pin(uart_port, gpio_tx, gpio_rx, GPIO_RTS, GPIO_CTS));
    return ret == ESP_OK;
}

bool Stream::available()
{
    size_t bytes_available = 0;
    uart_get_buffered_data_len(uart_port, &bytes_available);
    return bytes_available > 0;
}

uint8_t Stream::read()
{
    char ch;
    uart_read_bytes(uart_port, &ch, 1, pdMS_TO_TICKS(timeout));
    return ch;
}

bool Stream::write(uint8_t _ch)
{
    uint8_t ch = _ch;
    esp_err_t ret;

    uart_write_bytes(uart_port, (const char *)&ch, 1);
    ESP_ERROR_CHECK(ret = uart_wait_tx_done(uart_port, timeout));
    return ret == ESP_OK;
}
