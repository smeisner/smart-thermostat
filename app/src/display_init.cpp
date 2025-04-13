// SPDX-License-Identifier: GPL-3.0-only
/*
 * display.cpp
 *
 * Initialize display hardware and LVGL Graphics environment
 *
 * Copyright (c) 2025 Geoffrey Hausheer (rc2012@pblue.org)
 *
 */

#include "esp_log.h"
#include "driver/ledc.h"

#include "esp_lvgl_port.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_touch_xpt2046.h"

#include "gpio_defs.hpp"
#include "display.hpp"
#include "display_internal.hpp"

#define GPIO_NUM(x) ((gpio_num_t)x)
static const char *TAG = "display";
static lv_display_t *lvgl_disp = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;


void setBrightness(uint32_t level)
{
  if (level >= (1<<13)) {
    level = (1<<13) - 1;
  }
  ESP_LOGI(TAG, "Setting Backlight to %d", level);
  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, (ledc_channel_t)LEDC_BACKLIGHT_CHANNEL, level));
  // Update duty to apply the new value
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, (ledc_channel_t)LEDC_BACKLIGHT_CHANNEL));
}

uint32_t getBrightness() {
  uint32_t backlight = ledc_get_duty(LEDC_MODE, (ledc_channel_t)LEDC_BACKLIGHT_CHANNEL);
  ESP_LOGI(TAG, "Get Backlight: %s", backlight);
  return backlight;
}

static void backlightInit(void)
{
    gpio_config_t bk_gpio_config = {
      .pin_bit_mask = (uint64_t)(1ULL << TFT_LED_PIN),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
    };
    ledc_timer_config_t ledc_timer = {
      .speed_mode       = LEDC_LOW_SPEED_MODE,
      .duty_resolution  = LEDC_TIMER_13_BIT,
      .timer_num        = (ledc_timer_t)LEDC_BACKLIGHT_TIMER,
      .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
      .clk_cfg          = LEDC_AUTO_CLK,
      .deconfigure      = NULL,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  ledc_channel_config_t ledc_channel = {
    .gpio_num       = TFT_LED_PIN,
    .speed_mode     = LEDC_MODE,
    .channel        = (ledc_channel_t)LEDC_BACKLIGHT_CHANNEL,
    .intr_type      = LEDC_INTR_DISABLE,
    .timer_sel      = (ledc_timer_t)LEDC_BACKLIGHT_TIMER,
    .duty           = 0, // Set duty to 0%
    .hpoint         = 0,
    .sleep_mode     = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
    .flags = {
      .output_invert = 0,
    },
  };
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

static void touchInit(void)
{
    // TODO: Handle calibration
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(TOUCH_CS_PIN);
    #pragma GCC diagnostic pop
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM(TOUCH_IRQ_PIN),
        .levels = {
          .reset = 0,
          .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = TOUCH_MIRROR_X,
            .mirror_y = TOUCH_MIRROR_Y,
        },
        .process_coordinates = NULL,
        .interrupt_callback = NULL,
        .user_data = NULL,
        .driver_data = NULL,
    };

    ESP_LOGI(TAG, "Initialize touch controller XPT2046");
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io_handle, &tp_cfg, &touch_handle));
}

static void spiInit()
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
      .mosi_io_num = TFT_MOSI_PIN,
      .miso_io_num = TFT_MISO_PIN,
      .sclk_io_num = TFT_CLK_PIN,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,
      .data_io_default_level = false,
      .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
      .flags = 0,
      .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
      .intr_flags = 0,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

}

static void tftInit()
{
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
      .cs_gpio_num = TFT_CS_PIN,
      .dc_gpio_num = TFT_DC_PIN,
      .spi_mode = 0,
      .pclk_hz = LCD_PIXEL_CLOCK_HZ,
      .trans_queue_depth = 10,
      .on_color_trans_done = NULL,
      .user_ctx = NULL,
      .lcd_cmd_bits = LCD_CMD_BITS,
      .lcd_param_bits = LCD_PARAM_BITS,
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .flags = {
        .dc_high_on_cmd = 0,
        .dc_low_on_data = 0,
        .dc_low_on_param = 0,
        .octal_mode = 0,
        .quad_mode = 0,
        .sio_mode = 0,
        .lsb_first = 0,
        .cs_high_active = 0,
      }
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RESET_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
        .flags = {
          .reset_active_high = 0,
        },
        .vendor_config = NULL,
    };
    ESP_LOGI(TAG, "Install ILI9341 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

static void lvglInit()
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    esp_err_t err = lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .control_handle = NULL,
        .buffer_size = LCD_BUFFER_SIZE,
        .double_buffer = true,
        .trans_size = 0,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = LCD_MIRROR_X,
            .mirror_y = LCD_MIRROR_Y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
            .swap_bytes = LCD_SWAP_BYTES,
            .full_refresh = false,
            .direct_mode = false,
        }
    };

  static lv_disp_t * disp_handle;
  disp_handle = lvgl_port_add_disp(&disp_cfg);

  /* Add touch input (for selected screen) */
  const lvgl_port_touch_cfg_t touch_cfg = {
      .disp = disp_handle,
      .handle = touch_handle,
  };  
  lvgl_port_add_touch(&touch_cfg);
}

void displayInit() {
    backlightInit();
    spiInit();
    touchInit();
    tftInit();
    lvglInit();
    // FIXME
    setBrightness(4096);
}
