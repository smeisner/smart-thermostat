#include "wroom32.hpp"
#include <Adafruit_AHTX0.h>
#include <driver/rtc_io.h>

static LGFX lcd;
Adafruit_AHTX0 aht;


RTC_DATA_ATTR int bootCount = 0;  // Retains the number of starts (the value does not disappear even if deepsleep is performed)

int freq = 4000;
int channel = 0;
int resolution = 8;

void displayDimDemo(int32_t timeDelta, bool abort);
int32_t lcdTimestamp = millis() - 18000;

void initBuzzer()
{
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(BUZZER_PIN, channel);
}  

void startupBeep()
{
  ledcWriteTone(channel, 4000);
  ledcWrite(channel, 125);
  delay(125);
  ledcWriteTone(channel, 3000);
  delay(150);
  ledcWriteTone(channel, 0);
}

void beep()
{
  ledcWriteTone(channel, 4000);
  ledcWrite(channel, 125);
  delay(125);
  ledcWriteTone(channel, 0);
}

void initMotion()
{
  pinMode(MOTION_PIN, INPUT);
}

bool initAht()
{
  if (aht.begin())
  {
    Serial.println("Found AHT20");
    return true;
  }
  else
  {
    Serial.println("Didn't find AHT20");
  }
  return false;
}

void readAht()
{
  sensors_event_t humidity, temp;
  float temp_f;
  
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  // display.setCursor(0,20);
  // display.print("AHT20 Demo");
  // display.setCursor(0,40);
  // display.print("Temp: "); display.print(temp.temperature); display.println(" C");
  // display.setCursor(0,60);
  // display.print("Hum: "); display.print(humidity.relative_humidity); display.println(" %");
  temp_f = (temp.temperature * 9.0/5.0) + 32.0;
  Serial.print("Temperature: ");Serial.print(temp.temperature);Serial.print(" degrees C (");Serial.print(temp_f);Serial.println(" F)");
  Serial.print("Humidity: ");Serial.print(humidity.relative_humidity);Serial.println(" RH %");
}



int32_t lastMotionDetected = 0;
bool boolMotionDetected = false;

void IRAM_ATTR MotionDetect_ISR()
{
  digitalWrite(LED_BUILTIN, HIGH);
  lastMotionDetected = millis();
  boolMotionDetected = true;
}






int32_t lastTouchDetected = 0;
TaskHandle_t xTouchHandle;

void touchScreenDriver(void * parameter)
{
  int32_t x, y;
  for(;;) // infinite loop
  {
    vTaskSuspend( NULL );

    if (millis() - lastTouchDetected > 300)  // debounce
    {
      if (lcd.getTouch(&x, &y))
      {
        beep();
        Serial.print(millis() - lastTouchDetected);
        Serial.print(" - Touch - X: "); Serial.print(x); Serial.print(" Y: "); Serial.println(y);
        lastTouchDetected = millis();

        displayDimDemo(millis() - lcdTimestamp, true);

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



#define START_BRIGHTNESS 255
#define END_BRIGHTNESS 0

void initLcd()
{
  lcd.init();            // Calls init at normal startup.
  lcd.clear(TFT_WHITE);
  lcd.clear(TFT_BLACK);
}

void lcdDisplayTest()
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
  lcdDisplayTest();

  lcd.setCursor(bootCount*6, bootCount*8);
  lcd.setBrightness(START_BRIGHTNESS);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);  // Draw the black and white reversed state once
  lcd.print("DeepSleep test : " + String(bootCount));

  ++bootCount;
  if ((bootCount == 0) || (bootCount > 37)) bootCount = 1; // bootCount wrapped?

  lcd.setCursor(bootCount*6, bootCount*8);
  lcd.setTextColor(TFT_BLACK, TFT_WHITE);
  lcd.print("DeepSleep test : " + String(bootCount));
//  lcd.powerSaveOn(); // Power saving specification M5Stack CoreInk prevents colors from fading when power is turned off
  lcd.waitDisplay(); // stand-by
}

void displayDimDemo(int32_t timeDelta, bool abort)
{
  static int brightness;
  static bool dimming = false;
  static bool demo_started = false;

  if ((demo_started == false) && (timeDelta < 2000))
  {
    brightness = START_BRIGHTNESS;
    demo_started = true;
    dimming = false;
  }

  if ((timeDelta == 2000) && (!dimming))
  {
    Serial.println("Dimming started...");
    dimming = true;
  }

  if (abort)
  {
    lcd.setBrightness(START_BRIGHTNESS);
    dimming = false;
    brightness = END_BRIGHTNESS;
  }

  if ((dimming) && (brightness > END_BRIGHTNESS))
  {
    --brightness;
    lcd.setBrightness(brightness);
    delay(40);
  }

  if (brightness == END_BRIGHTNESS)
    demo_started = false;

}


void updateAht(void * parameter)
{
  Serial.println("In updateAht()");
  for(;;) // infinite loop
  {
    Serial.println("Reading aht20");
    // Read latest temp & humidity values
    readAht();

    // Pause the task again for 10000ms
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}



void setup(void)
{
  Serial.begin(115200);
  Serial.println("In setup()");

  pinMode(LED_BUILTIN, OUTPUT);

  initBuzzer();
  initMotion();
  initAht();
  initLcd();

  startupBeep();
  lcdDisplayTest();

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

  delay(1000);

  pinMode(HVAC_HEAT_PIN, OUTPUT);
  digitalWrite(HVAC_HEAT_PIN, HIGH);
  delay(1000);
  digitalWrite(HVAC_HEAT_PIN, LOW);

  attachInterrupt(MOTION_PIN, MotionDetect_ISR, RISING);
  attachInterrupt(TOUCH_IRQ_PIN, TouchDetect_ISR, FALLING);

  xTaskCreate(
    touchScreenDriver,
    "Touch Screen",
    4096,
    NULL,
    tskIDLE_PRIORITY+1,
    &xTouchHandle
  );

  xTaskCreate(
    updateAht,      // Function that should be called
    "Update AHT",   // Name of the task (for debugging)
    4096,           // Stack size (bytes)
    NULL,           // Parameter to pass
    1,              // Task priority
    NULL            // Task handle
  );
  
}



void loop(void)
{
  if (boolMotionDetected)
  {
    Serial.println("Motion detected");
    boolMotionDetected = false;
  }
  if (lastMotionDetected > 0)
  {
    if (millis() - lastMotionDetected > 3000)
    {
      digitalWrite(LED_BUILTIN, LOW);
      lastMotionDetected = 0;
    }
  }


  if (millis() > lcdTimestamp + 20000)
  {
    Serial.println("Restore display");
    displayStartDemo();
    lcdTimestamp = millis();
  }
  else
  {
    displayDimDemo(millis() - lcdTimestamp, false);
  }
}
