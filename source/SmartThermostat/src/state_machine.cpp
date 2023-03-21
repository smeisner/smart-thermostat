#include "thermostat.hpp"
#include "tft.hpp"

int32_t lcdTimestamp = millis() - 18000;
extern bool boolMotionDetected;
extern int32_t lastMotionDetected;

void stateMachine(void * parameter)
{
  for(;;) // infinite loop
  {
    // Provide time for the web server
    webPump();

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

    // Pause the task again for 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void stateCreateTask()
{
  xTaskCreate (
    stateMachine,
    "State Machine",
    4096,
    NULL,
    tskIDLE_PRIORITY - 1,
    NULL
  );
}

void testToggleRelays()
{
  delay(500);
  pinMode(HVAC_HEAT_PIN, OUTPUT);
  digitalWrite(HVAC_HEAT_PIN, HIGH);
  delay(1000);
  digitalWrite(HVAC_HEAT_PIN, LOW);
}

void serialStart()
{
  int32_t tmo = millis() + 5000;
  Serial.begin(115200);
  while (!Serial && millis() < tmo) delay(10);
  if (Serial)
    Serial.println("In serialStart()");
}