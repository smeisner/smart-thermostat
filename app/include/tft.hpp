#pragma once

#include "ui/ui.h"

#undef LGFX_USE_V0
#define LGFX_USE_V1

#include <LovyanGFX.hpp>

// Setting example when using LovyanGFX with original settings on ESP32

/// Derive from LGFX_Device to create a class with your own settings.
class LGFX : public lgfx::LGFX_Device
{
/*
  You can change the class name from "LGFX" to something else.
  When using with AUTODETECT, "LGFX" is used, so change it to something other than LGFX.
  Also, when using multiple panels at the same time, give each a different name.
  * When changing the class name, it is necessary to change the name of the
    constructor to the same name as well.

  You can decide how to name it freely, but assuming that the number of settings increases,
  For example, when setting ILI9341 for SPI connection with ESP32 DevKit-C,
   LGFX_DevKitC_SPI_ILI9341
  By using a name such as , and matching the file name and class name, it will be difficult to get lost when using it.
//*/


// Prepare an instance that matches the type of panel to be connected.
  lgfx::Panel_ILI9341     _panel_instance;

// Prepare an instance that matches the type of bus that connects the panels.
  lgfx::Bus_SPI        _bus_instance;   // SPI bus instance

// Prepare an instance if backlight control is possible. (remove if not needed)
  lgfx::Light_PWM     _light_instance;

// Prepare an instance that matches the type of touch screen. (remove if not needed)
  lgfx::Touch_XPT2046          _touch_instance;

public:

  // Create a constructor and set various settings here.
  // If you change the class name, specify the same name for the constructor.
  LGFX(void)
  {
    { // Configure bus control settings.
      auto cfg = _bus_instance.config();    // Get a structure for bus settings.

      // SPI bus settings
      cfg.spi_host = SPI3_HOST;     // Select SPI to use ESP32-S2,C3 : SPI2_HOST or SPI3_HOST / ESP32 : VSPI_HOST or HSPI_HOST
      // * With the ESP-IDF version upgrade, the description of VSPI_HOST and HSPI_HOST will be deprecated, so if an error occurs, use SPI2_HOST and SPI3_HOST instead.
      cfg.spi_mode = 0;             // Set SPI communication mode (0 ~ 3)
      cfg.freq_write = 40000000;    // SPI clock for transmission (up to 80MHz, rounded to the value obtained by dividing 80MHz by an integer)
      cfg.freq_read  = 16000000;    // SPI clock when receiving
      cfg.spi_3wire  = true;        // Set to true if reception is done on the MOSI pin
      cfg.use_lock   = true;        // Set true to use transaction lock
      cfg.dma_channel = SPI_DMA_CH_AUTO; // Set DMA channel to use (0=not use DMA / 1=1ch / 2=ch / SPI_DMA_CH_AUTO=auto setting)
      // * With the ESP-IDF version upgrade, SPI_DMA_CH_AUTO (automatic setting) is recommended for the DMA channel. Specifying 1ch and 2ch is deprecated.
      // cfg.pin_sclk = 18;            // Set SPI SCLK pin number
      cfg.pin_sclk = TFT_CLK_PIN;            // Set SPI SCLK pin number
      // cfg.pin_mosi = 23;            // Set SPI MOSI pin number
      cfg.pin_mosi = TFT_MOSI_PIN;            // Set SPI MOSI pin number
      // cfg.pin_miso = 19;            // Set SPI MISO pin number (-1 = disable)
      cfg.pin_miso = TFT_MISO_PIN;            // Set SPI MISO pin number (-1 = disable)
      // cfg.pin_dc   = 27;            // Set SPI D/C pin number (-1 = disable)
      cfg.pin_dc   = TFT_DC_PIN;            // Set SPI D/C pin number (-1 = disable)
     // When using the same SPI bus as the SD card, be sure to set MISO without omitting it.

      _bus_instance.config(cfg);    // Reflects the set value on the bus
      _panel_instance.setBus(&_bus_instance);      // set the bus on the panel
    }

    { // Set the display panel control
      auto cfg = _panel_instance.config();    // Gets a structure for display panel settings

      cfg.pin_cs           =    TFT_CS_PIN;  // Pin number to which CS is connected (-1 = disable)
      cfg.pin_rst          =    TFT_RESET_PIN;  // Pin number to which RST is connected (-1 = disable)
      cfg.pin_busy         =    -1;  // Pin number to which BUSY is connected (-1 = disable)

      // * The following setting values are general initial values for each panel, so please comment out any unknown items and try them.

      cfg.panel_width      =   240;  // actual displayable width
      cfg.panel_height     =   320;  // actual visible height
      cfg.offset_x         =     0;  // Panel offset amount in X direction
      cfg.offset_y         =     0;  // Panel offset amount in Y direction
      cfg.offset_rotation  =     3;  // Rotation direction value offset 0~7 (4~7 is upside down)
      cfg.dummy_read_pixel =     8;  // Number of bits for dummy read before pixel readout
      cfg.dummy_read_bits  =     1;  // Number of bits for dummy read before non-pixel data read
      cfg.readable         =  true;  // Set to true if data can be read
      cfg.invert           = false;  // Set to true if the light and dark of the panel is inverted
      cfg.rgb_order        = false;  // Set to true if the panel's red and blue are swapped
      cfg.dlen_16bit       = false;  // Set to true for panels that transmit data length in 16-bit units with 16-bit parallel or SPI
      cfg.bus_shared       =  true;  // If the bus is shared with the SD card, set to true (bus control with drawJpgFile etc.)

      _panel_instance.config(cfg);
    }

    { // Set backlight control. (Delete if not necessary)
      auto cfg = _light_instance.config();    // Get a structure for backlight settings

      cfg.pin_bl = TFT_LED_PIN;              // Pin number with backlight connected
      cfg.invert = false;           // To reverses the brightness of the backlight True
      cfg.freq   = 44100;           // Backlight PWM frequency
      cfg.pwm_channel = 7;          // PWM channel number to use

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);  // Set the backlight on the panel
    }

    { // Set the touch screen control. (Delete if not necessary)
      auto cfg = _touch_instance.config();

      cfg.x_min      = 0;    // Minimum X value (raw value) obtained from the touchscreen
      cfg.x_max      = 239;  // Maximum X value (raw value) obtained from the touchscreen
      cfg.y_min      = 0;    // Minimum Y value (raw value) obtained from the touchscreen
      cfg.y_max      = 319;  // Maximum Y value (raw value) obtained from the touchscreen
      cfg.pin_int    = TOUCH_IRQ_PIN;   // Pin number connected to INT
      cfg.bus_shared = true; // Set to true when using a common bus with the screen
      cfg.offset_rotation = 0;// Adjustment when the display and touch direction do not match Set with a value of 0 to 7

      // In the case of SPI connection
      cfg.spi_host = SPI3_HOST;// Select the SPI to use (HSPI_HOST or VSPI_HOST)
      cfg.freq = 1000000;      // Set the SPI clock
      cfg.pin_sclk = TFT_CLK_PIN;     // Pin number connected to SCLK
      cfg.pin_mosi = TFT_MOSI_PIN;    // Pin number connected to MOSI
      cfg.pin_miso = TFT_MISO_PIN;    // Pin number connected to MISO
      cfg.pin_cs   = TOUCH_CS_PIN;    // Pin number connected to CS

      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);  // Set the touch screen on the panel
    }

    setPanel(&_panel_instance); // Set the panel to be used
  }
};


#include <lvgl.h>

LGFX tft;

#define screenWidth 320
#define screenHeight 240

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

void my_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  uint16_t touchX, touchY;
  bool touched = tft.getTouch(&touchX, &touchY);
  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touchX;
    data->point.y = touchY;

//    Serial.printf ("Touch X: %d  Y: %d\n", touchX, touchY);
  }
}