#include "stream.h" // Pulls in your exact class definitions and macros

Stream::Stream()
{
  uart_port = UART_NUM_MAX;
  timeout = pdMS_TO_TICKS(5000);
}

Stream::~Stream() 
{
}

bool Stream::begin(uart_port_t _uart_port, int baud_rate, int gpio_tx, int gpio_rx)
{
  esp_err_t ret;

  // Fully zero-initialize the structure first to prevent garbage stack values
  uart_config_t uart_config = {};
  uart_config.baud_rate = baud_rate;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity    = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  
  // FIX FOR POWER MANAGEMENT / LIGHT SLEEP: Force the clock source to use XTAL
  uart_config.source_clk = UART_SCLK_XTAL; 

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  uart_config.flags.allow_pd = 0; // Turn off power down sleep state registers
#endif

  uart_port = _uart_port;
  timeout = pdMS_TO_TICKS(5000);

  // Configure the core parameters while the hardware block is safely idle
  ESP_ERROR_CHECK(uart_param_config(uart_port, &uart_config));
  
  // Map physical bidirectional I/O pins using your header's native macros
  ESP_ERROR_CHECK(ret = uart_set_pin(uart_port, gpio_tx, gpio_rx, GPIO_RTS, GPIO_CTS));
  
  // Install the background driver with a solid 1024-byte RX ring buffer.
  ESP_ERROR_CHECK(uart_driver_install(uart_port, BUF_SIZE, 0, 0, NULL, 0));

  // Clear any startup transmission noise or initial byte bursts
  uart_flush(uart_port);

  return ret == ESP_OK;
}

// Change return type to int and return the actual data length
int Stream::available()
{
  size_t available_bytes = 0;
  // Modern ESP-IDF check to securely see exactly how many bytes are waiting
  uart_get_buffered_data_len(uart_port, &available_bytes);
  return (int)available_bytes; // Returns 0 if empty, or the literal count (e.g., 23)
}


// Match the int return type signature
int Stream::read()
{
  uint8_t byte = 0;
  // Read exactly 1 byte from the ring buffer.
  int length = uart_read_bytes(uart_port, &byte, 1, 0);
  if (length > 0)
  {
    return (int)byte; // Return the valid byte value (0 to 255)
  }
  return -1; // CRITICAL FIX: Return -1 if the buffer is empty!
}

// Matches: bool write(uint8_t _ch);
bool Stream::write(uint8_t _ch)
{
  int bytes_written = uart_write_bytes(uart_port, (const char*)&_ch, 1);
  return (bytes_written > 0);
}
