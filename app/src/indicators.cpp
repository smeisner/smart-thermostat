#include "thermostat.hpp"

int freq = 4000;
int channel = 0;
int resolution = 8;

void audioBuzzerInit()
{
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(BUZZER_PIN, channel);
}  

void audioStartupBeep()
{
  ledcWriteTone(channel, 4000);
  ledcWrite(channel, 125);
  delay(125);
  ledcWriteTone(channel, 3000);
  delay(150);
  ledcWriteTone(channel, 0);
}

// Use lastBeep to rate limit the beeping
unsigned long lastBeep = 0;

void audioBeep()
{
  if (OperatingParameters.thermostatBeepEnable)
  {
    if (millis() - lastBeep > 125)  // 1/8 of a sec
    {
      ledcWriteTone(channel, 4000);
      ledcWrite(channel, 125);
      delay(125);
      ledcWriteTone(channel, 0);
      lastBeep = millis();
    }
  }
}

void indicatorsInit()
{
//@@@  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(LED_HEAT_PIN, OUTPUT);
  pinMode(LED_COOL_PIN, OUTPUT);
  pinMode(LED_FAN_PIN, OUTPUT);

  // digitalWrite(LED_HEAT_PIN, LOW);
  // digitalWrite(LED_COOL_PIN, LOW);
  // digitalWrite(LED_FAN_PIN, LOW);
  // delay(1000);

  digitalWrite(LED_HEAT_PIN, HIGH);
  delay(750);
  digitalWrite(LED_HEAT_PIN, LOW);
  digitalWrite(LED_COOL_PIN, HIGH);
  delay(750);
  digitalWrite(LED_COOL_PIN, LOW);
  digitalWrite(LED_FAN_PIN, HIGH);
  delay(750);
  digitalWrite(LED_FAN_PIN, LOW);

  audioBuzzerInit();
}
