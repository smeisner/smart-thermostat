#include "thermostat.hpp"
#include "tft.hpp"

OPERATING_PARAMETERS OperatingParameters;

int32_t lcdTimestamp = millis() - 18000;
extern int32_t lastMotionDetected;

void stateMachine(void * parameter)
{
  float currentTemp;
  float minTemp, maxTemp;

  for(;;) // infinite loop
  {
    OperatingParameters.lightDetected = analogRead(LIGHT_SENS_PIN);

    currentTemp = OperatingParameters.tempCurrent + OperatingParameters.tempCorrection;
    minTemp = OperatingParameters.tempSet - (OperatingParameters.tempSwing / 2.0);
    maxTemp = OperatingParameters.tempSet + (OperatingParameters.tempSwing / 2.0);

    if (OperatingParameters.hvacSetMode == OFF)
    {
        OperatingParameters.hvacOpMode = OFF;
        //digitalWrite(HVAC_HEAT_PIN, LOW);
        //digitalWrite(HVAC_COOL_PIN, LOW);
        //digitalWrite(HVAC_FAN_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
    }
    else if (OperatingParameters.hvacSetMode == FAN)
    {
        OperatingParameters.hvacOpMode = FAN;
        //digitalWrite(HVAC_HEAT_PIN, LOW);
        //digitalWrite(HVAC_COOL_PIN, LOW);
        //digitalWrite(HVAC_FAN_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
    }
    else if (((OperatingParameters.hvacSetMode == AUTO) || (OperatingParameters.hvacSetMode == HEAT)) && (currentTemp < minTemp))
    {
        OperatingParameters.hvacOpMode = HEAT;
        //digitalWrite(HVAC_HEAT_PIN, HIGH);
        digitalWrite(LED_HEAT_PIN, HIGH);
    }
    else if (((OperatingParameters.hvacSetMode == AUTO) || (OperatingParameters.hvacSetMode == COOL)) && (currentTemp > maxTemp))
    {
        OperatingParameters.hvacOpMode = COOL;
        //digitalWrite(HVAC_COOL_PIN, HIGH);
        digitalWrite(LED_COOL_PIN, HIGH);
    }

    if ((currentTemp >= minTemp) && (currentTemp <= maxTemp) && (OperatingParameters.hvacSetMode != FAN) && (OperatingParameters.hvacSetMode != OFF))
    {
        OperatingParameters.hvacOpMode = IDLE;
        //digitalWrite(HVAC_HEAT_PIN, LOW);
        //digitalWrite(HVAC_COOL_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
    }

    // Provide time for the web server
    webPump();

    // Check wifi
    if (!wifiConnected()) wifiReconnect();

//    if (OperatingParameters.motionDetected)
    if (lastMotionDetected > 0)
    {
      if (millis() - lastMotionDetected > MOTION_TIMEOUT)
      {
        digitalWrite(LED_BUILTIN, LOW);
        lastMotionDetected = 0;
        OperatingParameters.motionDetected = false;
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

    // Pause the task again for 40ms
    vTaskDelay(40 / portTICK_PERIOD_MS);
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
  delay(750);
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