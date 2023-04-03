#include "thermostat.hpp"
#include "tft.hpp"

#define START_BRIGHTNESS 255
#define END_BRIGHTNESS 0

static LGFX lcd;
int32_t lastTouchDetected = 0;
TaskHandle_t xTouchHandle;

// RTC_DATA_ATTR int bootCount = 0;  // Retains the number of starts (the value does not disappear even if deepsleep is performed)
int bootCount = 0;  // Retains the number of starts (the value does not disappear even if deepsleep is performed)

void tftTouchScreenDriver(void * parameter)
{
  int32_t x, y;
  for(;;) // infinite loop
  {
    vTaskSuspend( NULL );

    if (millis() - lastTouchDetected > 300)  // debounce
    {
      if (lcd.getTouchRaw(&x, &y))
      {
        audioBeep();
        Serial.print(millis() - lastTouchDetected);
        Serial.print(" - Touch - X: "); Serial.print(x); Serial.print(" Y: "); Serial.println(y);
        lastTouchDetected = millis();

        displayDimDemo(0, true);

      }
    }
  }
}

void IRAM_ATTR TouchDetect_ISR()
{
  BaseType_t xYieldRequired;
  xYieldRequired = xTaskResumeFromISR( xTouchHandle );
  portYIELD_FROM_ISR( xYieldRequired );
}

void tftCreateTask()
{
    attachInterrupt(TOUCH_IRQ_PIN, TouchDetect_ISR, FALLING);

    xTaskCreate (
        tftTouchScreenDriver,
        "Touch Screen",
        4096,
        NULL,
        tskIDLE_PRIORITY+1,
        &xTouchHandle
    );
}

void tftInit()
{
  lcd.init();            // Calls init at normal startup.
  lcd.clear(TFT_WHITE);
  lcd.clear(TFT_BLACK);
}

void displaySplash()
{
  lcd.clear(TFT_BLACK);

  lcd.startWrite();      // draw the background
  lcd.setColorDepth(24);
  {
    LGFX_Sprite sp(&lcd);
    sp.createSprite(128, 128);
    sp.createPalette();
    for (int y = 0; y < 128; y++)
      for (int x = 0; x < 128; x++)
        sp.writePixel(x, y, sp.color888(x << 1, x + y, y << 1));
    for (int y = 0; y < lcd.height(); y += 128)
      for (int x = 0; x < lcd.width(); x += 128)
        sp.pushSprite(x, y);
  }
  lcd.endWrite();
}

void displayStartDemo()
{
  displaySplash();

  lcd.setCursor(bootCount*6, bootCount*8);
  lcd.setBrightness(START_BRIGHTNESS);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);  // Draw the black and white reversed state once
  lcd.print("Display test : " + String(bootCount));

  ++bootCount;
  if ((bootCount == 0) || (bootCount > 37)) bootCount = 1; // bootCount wrapped?

  lcd.setCursor(bootCount*6, bootCount*8);
  lcd.setTextColor(TFT_BLACK, TFT_WHITE);
  lcd.print("Display test : " + String(bootCount));
//  lcd.powerSaveOn(); // Power saving specification M5Stack CoreInk prevents colors from fading when power is turned off
  lcd.waitDisplay(); // stand-by
}

void displayDimDemo(int32_t timeDelta, bool abort)
{
  static int brightness;
  static bool dimming = false;
  static bool demo_started = false;

  if (abort)
  {
    lcd.setBrightness(START_BRIGHTNESS);
    dimming = false;
    brightness = END_BRIGHTNESS;
    demo_started = false;
    return;
  }

  if ((demo_started == false) && (timeDelta < 2000))
  {
    brightness = START_BRIGHTNESS;
    demo_started = true;
    dimming = false;
  }

  if ((timeDelta > 2000) && (!dimming))
  {
    Serial.println("Dimming started...");
    dimming = true;
  }

  if ((dimming) && (brightness > END_BRIGHTNESS))
  {
    --brightness;
    lcd.setBrightness(brightness);
    //delay(40);
  }

  if (brightness == END_BRIGHTNESS)
    demo_started = false;

}
