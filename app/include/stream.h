/*
  This file is used to support Arduino libraries (like LD2410) by recreating
  some simple functions.
*/

#pragma once

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h" // ADDED: Pulls in FreeRTOS scheduling handles
#include "freertos/task.h"     // ADDED: Pulls in vTaskDelay for yielding

// FIX FOR ARDUINO LIBRARY COMPILATION UNDER PURE ESP-IDF:
// This defines a global inline yield() function that the ld2410 library can call.
// It instructs the ESP32-S3 scheduler to cleanly yield CPU ticks exactly like Arduino.
inline void yield() {
    vTaskDelay(1);
}

#define GPIO_RTS (UART_PIN_NO_CHANGE)
#define GPIO_CTS (UART_PIN_NO_CHANGE)
#define BUF_SIZE (1024)

class Stream {
  public:
    Stream();      //Constructor function
    ~Stream();     //Destructor function
    bool begin(uart_port_t _uart_port, int baud_rate, int gpio_tx, int gpio_rx);
    int available();
    // FIX: Change return type from uint8_t to int to support the -1 fallback spec
    int read();
    bool write(uint8_t _ch);

    void print(uint8_t) {};
    void print(const char *) {};
    void print(const char *, const char *) {};
    void print(uint8_t, const char *) {};
    void println(uint8_t) {};
    void println(const char *) {};

  protected:
  private:
    uart_port_t uart_port;
    TickType_t timeout;
};
