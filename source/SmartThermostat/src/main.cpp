#include "wroom32.hpp"

#if 0

// Create an instance of the prepared class
LGFX display;

void setup(void)
{
  // It will be available by executing the initialization of the SPI bus and panel
  display.init();

  display.setTextSize((std::max(display.width(), display.height()) + 255) >> 8);

  // Calibrate when the touch is available. (Optional)
   if (display.touch())
   {
     if (display.width() < display.height()) display.setRotation(display.getRotation() ^ 1);

     // Draw a guide text on the screen.
     display.setTextDatum(textdatum_t::middle_center);
     display.drawString("touch the arrow marker.", display.width()>>1, display.height() >> 1);
     display.setTextDatum(textdatum_t::top_left);

     // If you use a touch, you can calibrate. Touch the tip of the arrow displayed in the four corners of the screen in order.
     std::uint16_t fg = TFT_WHITE;
     std::uint16_t bg = TFT_BLACK;
     if (display.isEPD()) std::swap(fg, bg);
     display.calibrateTouch(nullptr, fg, bg, std::max(display.width(), display.height()) >> 3);
   }

  display.fillScreen(TFT_BLACK);
}

uint32_t count = ~0;
void loop(void)
{
  display.startWrite();
  display.setRotation(++count & 7);
  display.setColorDepth((count & 8) ? 16 : 24);

  display.setTextColor(TFT_WHITE);
  display.drawNumber(display.getRotation(), 16, 0);

  display.setTextColor(0xFF0000U);
  display.drawString("R", 30, 16);
  display.setTextColor(0x00FF00U);
  display.drawString("G", 40, 16);
  display.setTextColor(0x0000FFU);
  display.drawString("B", 50, 16);

  display.drawRect(30,30,display.width()-60,display.height()-60,count*7);
  display.drawFastHLine(0, 0, 10);

  display.endWrite();

  int32_t x, y;
  if (display.getTouch(&x, &y)) {
    display.fillRect(x-2, y-2, 5, 5, count*7);
  }
}


#else

#include <driver/rtc_io.h>

static LGFX lcd;

RTC_DATA_ATTR int bootCount = 0;  // Retains the number of starts (the value does not disappear even if deepsleep is performed)

int freq = 4000;
int channel = 0;
int resolution = 8;

void init_buzzer()
{
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(BUZZER_PIN, channel);

  ledcWriteTone(channel, 4000);
  
  for (int dutyCycle = 0; dutyCycle <= 255; dutyCycle=dutyCycle+64){
  
    Serial.println(dutyCycle);
  
    ledcWrite(channel, dutyCycle);
    delay(1000);
  }
  
  ledcWrite(channel, 125);
  
  for (int freq = 255; freq < 5000; freq = freq + 250){
  
     Serial.println(freq);
  
     ledcWriteTone(channel, freq);
     delay(1000);
  }
}

void setup(void)
{
  Serial.begin(115200);
  Serial.println("In setup()");

  pinMode(MOTION_PIN, INPUT);

//  init_buzzer();

  // pinMode(LED_HEAT_PIN, OUTPUT);
  // pinMode(LED_COOL_PIN, OUTPUT);
  // pinMode(LED_IDLE_PIN, OUTPUT);

  // digitalWrite(LED_HEAT_PIN, HIGH);
  // delay(1000);
  // digitalWrite(LED_HEAT_PIN, LOW);
  // digitalWrite(LED_COOL_PIN, HIGH);
  // delay(1000);
  // digitalWrite(LED_COOL_PIN, LOW);
  // digitalWrite(LED_IDLE_PIN, HIGH);
  // delay(1000);
  // digitalWrite(LED_IDLE_PIN, LOW);

  switch(esp_sleep_get_wakeup_cause())
  {
    case ESP_SLEEP_WAKEUP_EXT0 :
    case ESP_SLEEP_WAKEUP_EXT1 :
    case ESP_SLEEP_WAKEUP_TIMER :
    case ESP_SLEEP_WAKEUP_TOUCHPAD :
    case ESP_SLEEP_WAKEUP_ULP :
      lcd.init_without_reset(); // Call init_without_reset when returning from deep sleep.
      break;

    default :
      lcd.init();            // Calls init at normal startup.
      lcd.clear(TFT_WHITE);
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
      break;
  }

  lcd.setCursor(bootCount*6, bootCount*8);
  lcd.setBrightness(128);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);  // Draw the black and white reversed state once
  lcd.print("DeepSleep test : " + String(bootCount));

  ++bootCount;
  if ((bootCount == 0) || (bootCount > 40)) bootCount = 1; // bootCount wrapped?

  lcd.setCursor(bootCount*6, bootCount*8);
  lcd.setTextColor(TFT_BLACK, TFT_WHITE);
  lcd.print("DeepSleep test : " + String(bootCount));
  lcd.powerSaveOn(); // Power saving specification M5Stack CoreInk prevents colors from fading when power is turned off
  lcd.waitDisplay(); // stand-by

  auto pin_rst = (gpio_num_t)lcd.getPanel()->config().pin_rst;
  if ((uint32_t)pin_rst < GPIO_NUM_MAX)
  {
    // Manage the RST pin with RTC_GPIO and keep it high
    rtc_gpio_set_level(pin_rst, 1);
    rtc_gpio_set_direction(pin_rst, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_init(pin_rst);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  }

//   auto light = lcd.getPanel()->getLight();
//   if (light)
//   {
//     auto pin_bl = (gpio_num_t)((lgfx::Light_PWM*)light)->config().pin_bl;
//     if ((uint32_t)pin_bl < GPIO_NUM_MAX)
//     {
//       // Manage the BackLight pin with RTC_GPIO and keep it high
//       rtc_gpio_set_level(pin_bl, 1);
//       rtc_gpio_set_direction(pin_bl, RTC_GPIO_MODE_OUTPUT_ONLY);
//       rtc_gpio_init(pin_bl);
//       esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
//     }
//   }

  delay(1000);

  Serial.println("Dimming started...");
  int start_brightness=255;
  int end_brightness=0;
  int32_t x, y;
  for (int i=start_brightness; i >= end_brightness; i--)
  {
    if (lcd.getTouch(&x, &y)) {
      Serial.println("TouchScreen touched!!!");
      lcd.setBrightness(start_brightness);
      delay(3000);
      break;
    }

    bool sensorValue = digitalRead(MOTION_PIN);
    if (sensorValue == HIGH)
    {
      Serial.println("Motion Detected!!");
    }
    digitalWrite(LED_BUILTIN, sensorValue);
    delay(60);
    lcd.setBrightness(i);
  }

//  ESP_LOGW("STEVE", "sleep");
//  esp_sleep_enable_timer_wakeup(1 * 1000 * 1000); // micro sec
  esp_sleep_enable_timer_wakeup(7 * 1000 * 1000); // micro sec

  esp_deep_sleep_start();
}

void loop(void)
{
  delay(10000);
}

#endif
