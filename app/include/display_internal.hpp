#pragma once

#define LCD_H_RES              240
#define LCD_V_RES              320
#define LCD_HOST               SPI3_HOST
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8
#define LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
#define LCD_MIRROR_X           1
#define LCD_MIRROR_Y           0
#define LCD_BUFFER_SIZE        (LCD_H_RES * LCD_V_RES / 2)
#define LCD_SWAP_BYTES         1

#define TOUCH_MIRROR_X         0
#define TOUCH_MIRROR_Y         1

#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz
