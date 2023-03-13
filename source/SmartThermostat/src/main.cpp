#include "wroom32.hpp"
#include <Adafruit_AHTX0.h>
#include <driver/rtc_io.h>

static LGFX lcd;
Adafruit_AHTX0 aht;


RTC_DATA_ATTR int bootCount = 0;  // Retains the number of starts (the value does not disappear even if deepsleep is performed)

int freq = 4000;
int channel = 0;
int resolution = 8;


void initBuzzer()
{
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(BUZZER_PIN, channel);

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




bool checkForMotion(void)
{
  static int32_t lastMotion;

  if ((millis() - lastMotion) > 2000)
  {
    digitalWrite(LED_BUILTIN, LOW);
    if (digitalRead(MOTION_PIN) == HIGH)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      lastMotion = millis();
      return true;
    }
  }
  return false;
}

bool checkForTouch()
{
  int32_t x, y;
  if (lcd.getTouch(&x, &y))
  {
    beep();
    Serial.print("Touch - X: "); Serial.print(x); Serial.print(" Y: "); Serial.println(y);
    return true;
  }
  return false;
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
//  initLcd();
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





void setup(void)
{
  Serial.begin(115200);
  Serial.println("In setup()");

  pinMode(LED_BUILTIN, OUTPUT);
//  pinMode(TOUCH_IRQ_PIN, INPUT);

  initBuzzer();
  initMotion();
  initAht();
  initLcd();

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
}



void loop(void)
{
  static int32_t ahtTimestamp = millis() - 9000;
  static int32_t lcdTimestamp = millis() - 18000;
  bool abortDisplayDemo = false;

  if (checkForMotion())
    Serial.println("Motion Detected!!");

  if (checkForTouch())
    abortDisplayDemo = true;

  if (millis() > ahtTimestamp + 10000)
  {
    readAht();
    ahtTimestamp = millis();
  }

  if (millis() > lcdTimestamp + 20000)
  {
    Serial.println("Restore display");
    displayStartDemo();
    lcdTimestamp = millis();
  }
  else
  {
    displayDimDemo(millis() - lcdTimestamp, abortDisplayDemo);
  }
}
