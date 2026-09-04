#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"

// Define your PCB's permanent traces matching the input-only pins
#define AHT20_SCL               ((gpio_num_t)35)
#define AHT20_SDA               ((gpio_num_t)36)
#define AHT20_SENSOR_ADDR       0x38

class SoftI2C {
private:
    // 100 kHz speed targets ~5 microsecond clock half-periods
    static const uint32_t HALF_CLOCK_US = 5;

    // The open-drain simulation mechanism
    static inline void set_sda_high() {
        // High = Turn into Input, allowing your physical 10k resistor to pull it up
        gpio_set_direction(AHT20_SDA, GPIO_MODE_INPUT);
    }

    static inline void set_sda_low() {
        // Low = Turn into Output, enabling the silicon path to pull down to 0V
        gpio_set_direction(AHT20_SDA, GPIO_MODE_OUTPUT);
        gpio_set_level(AHT20_SDA, 0);
    }

    static inline void set_scl_high() {
        gpio_set_direction(AHT20_SCL, GPIO_MODE_INPUT);
    }

    static inline void set_scl_low() {
        gpio_set_direction(AHT20_SCL, GPIO_MODE_OUTPUT);
        gpio_set_level(AHT20_SCL, 0);
    }

    static inline int read_sda() {
        gpio_set_direction(AHT20_SDA, GPIO_MODE_INPUT);
        return gpio_get_level(AHT20_SDA);
    }

    void start() {
        set_sda_high();
        set_scl_high();
        esp_rom_delay_us(HALF_CLOCK_US);
        set_sda_low();
        esp_rom_delay_us(HALF_CLOCK_US);
        set_scl_low();
        esp_rom_delay_us(HALF_CLOCK_US);
    }

    void stop() {
        set_sda_low();
        esp_rom_delay_us(HALF_CLOCK_US);
        set_scl_high();
        esp_rom_delay_us(HALF_CLOCK_US);
        set_sda_high();
        esp_rom_delay_us(HALF_CLOCK_US);
    }

    bool write_byte(uint8_t byte) {
        for (int i = 0; i < 8; i++) {
            if (byte & 0x80) set_sda_high();
            else set_sda_low();
            byte <<= 1;
            
            esp_rom_delay_us(HALF_CLOCK_US);
            set_scl_high();
            esp_rom_delay_us(HALF_CLOCK_US);
            set_scl_low();
        }
        
        // Read Acknowledgement bit (ACK)
        set_sda_high();
        esp_rom_delay_us(HALF_CLOCK_US);
        set_scl_high();
        esp_rom_delay_us(HALF_CLOCK_US);
        
        bool ack = (read_sda() == 0);
        set_scl_low();
        return ack;
    }

    uint8_t read_byte(bool send_ack) {
        uint8_t byte = 0;
        set_sda_high(); // Ensure pin is in input mode to read
        
        for (int i = 0; i < 8; i++) {
            byte <<= 1;
            esp_rom_delay_us(HALF_CLOCK_US);
            set_scl_high();
            esp_rom_delay_us(HALF_CLOCK_US);
            if (read_sda()) byte |= 1;
            set_scl_low();
        }
        
        // Send ACK/NACK bit back to device
        if (send_ack) set_sda_low();
        else set_sda_high();
        
        esp_rom_delay_us(HALF_CLOCK_US);
        set_scl_high();
        esp_rom_delay_us(HALF_CLOCK_US);
        set_scl_low();
        set_sda_high();
        
        return byte;
    }

public:
    void init() {
        // Initialize lines to high state without binding hardware peripheral engines
        gpio_reset_pin(AHT20_SCL);
        gpio_reset_pin(AHT20_SDA);
        set_sda_high();
        set_scl_high();
    }

    bool transmit(uint8_t address, const uint8_t *data, size_t len) {
        start();
        // Shift address for Write mode (0)
        if (!write_byte((address << 1) | 0)) {
            stop();
            return false; // NACK returned
        }
        for (size_t i = 0; i < len; i++) {
            if (!write_byte(data[i])) {
                stop();
                return false;
            }
        }
        stop();
        return true;
    }

    bool receive(uint8_t address, uint8_t *buffer, size_t len) {
        start();
        // Shift address for Read mode (1)
        if (!write_byte((address << 1) | 1)) {
            stop();
            return false;
        }
        for (size_t i = 0; i < len; i++) {
            // Send ACK for all bytes except the final array element
            buffer[i] = read_byte(i < (len - 1));
        }
        stop();
        return true;
    }
};
