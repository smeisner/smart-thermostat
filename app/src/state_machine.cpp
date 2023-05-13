#include "thermostat.hpp"
//#include "tft.hpp"

OPERATING_PARAMETERS OperatingParameters;

int32_t lcdTimestamp = millis() - 18000;
extern int32_t lastMotionDetected;
extern int32_t lastTimeUpdate;
int32_t lastWifiReconnect = 0;

void stateMachine(void * parameter)
{
  float currentTemp;
  float minTemp, maxTemp;
  float autoMinTemp, autoMaxTemp;

  for(;;) // infinite loop
  {
    OperatingParameters.lightDetected = analogRead(LIGHT_SENS_PIN);

    currentTemp = OperatingParameters.tempCurrent + OperatingParameters.tempCorrection;
    minTemp = OperatingParameters.tempSet - (OperatingParameters.tempSwing / 2.0);
    maxTemp = OperatingParameters.tempSet + (OperatingParameters.tempSwing / 2.0);

#if 0
    autoMinTemp = OperatingParameters.tempSetAutoMin - (OperatingParameters.tempSwing / 2.0);
    autoMaxTemp = OperatingParameters.tempSetAutoMax + (OperatingParameters.tempSwing / 2.0);
#else
    autoMinTemp = minTemp;
    autoMaxTemp = maxTemp;
#endif

    if (OperatingParameters.hvacSetMode == OFF)
    {
      OperatingParameters.hvacOpMode = OFF;
      //digitalWrite(HVAC_HEAT_PIN, LOW);
      //digitalWrite(HVAC_COOL_PIN, LOW);
      //digitalWrite(HVAC_FAN_PIN, LOW);
      digitalWrite(LED_COOL_PIN, LOW);
      digitalWrite(LED_HEAT_PIN, LOW);
      digitalWrite(LED_IDLE_PIN, LOW);
    }
    else if (OperatingParameters.hvacSetMode == FAN)
    {
      OperatingParameters.hvacOpMode = FAN;
      //digitalWrite(HVAC_HEAT_PIN, LOW);
      //digitalWrite(HVAC_COOL_PIN, LOW);
      //digitalWrite(HVAC_FAN_PIN, HIGH);
      digitalWrite(LED_COOL_PIN, LOW);
      digitalWrite(LED_HEAT_PIN, LOW);
      digitalWrite(LED_IDLE_PIN, LOW);
    }
    else if (OperatingParameters.hvacSetMode == HEAT)
    {
      if (OperatingParameters.hvacOpMode == COOL)
      {
        //digitalWrite(HVAC_COOL_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_IDLE_PIN, LOW);
      }
      if (currentTemp < minTemp)
      {
        OperatingParameters.hvacOpMode = HEAT;
        //digitalWrite(HVAC_HEAT_PIN, HIGH);
        digitalWrite(LED_HEAT_PIN, HIGH);
        digitalWrite(LED_IDLE_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
      }
    }
    else if (OperatingParameters.hvacSetMode == COOL)
    {
      if (OperatingParameters.hvacOpMode == HEAT)
      {
        //digitalWrite(HVAC_HEAT_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_IDLE_PIN, LOW);
      }
      if (currentTemp > maxTemp)
      {
        OperatingParameters.hvacOpMode = COOL;
        //digitalWrite(HVAC_COOL_PIN, HIGH);
        digitalWrite(LED_COOL_PIN, HIGH);
        digitalWrite(LED_IDLE_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        digitalWrite(LED_IDLE_PIN, HIGH);
      }
    }
    else if (OperatingParameters.hvacSetMode == AUTO)
    {
      if (currentTemp < autoMinTemp)
      {
        OperatingParameters.hvacOpMode = HEAT;
        //digitalWrite(HVAC_HEAT_PIN, HIGH);
        digitalWrite(LED_HEAT_PIN, HIGH);
        //digitalWrite(HVAC_COOL_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_IDLE_PIN, LOW);
      }
      else if (currentTemp > autoMaxTemp)
      {
        OperatingParameters.hvacOpMode = COOL;
        //digitalWrite(HVAC_COOL_PIN, HIGH);
        digitalWrite(LED_COOL_PIN, HIGH);
        //digitalWrite(HVAC_HEAT_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_IDLE_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        //digitalWrite(HVAC_COOL_PIN, LOW);
        //digitalWrite(HVAC_HEAT_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_IDLE_PIN, HIGH);
      }
    }

    if ((currentTemp >= minTemp) && (currentTemp <= maxTemp) && (OperatingParameters.hvacSetMode != FAN) && (OperatingParameters.hvacSetMode != OFF))
    {
        OperatingParameters.hvacOpMode = IDLE;
        //digitalWrite(HVAC_HEAT_PIN, LOW);
        //digitalWrite(HVAC_COOL_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_IDLE_PIN, HIGH);
    }

    if (millis() - lastTimeUpdate > UPDATE_TIME_INTERVAL)
    {
      lastTimeUpdate = millis();
      updateTimeSntp();
    }

    // Provide time for the web server
    webPump();

    // Check wifi
    if (!wifiConnected()) 
    {
      if (millis() > lastWifiReconnect + 15000)
      {
        if (wifiReconnect(WifiCreds.hostname, WifiCreds.ssid, WifiCreds.password))
        {
          lastWifiReconnect = 0;
          OperatingParameters.wifiConnected = true;
        }
        else
        {
          lastWifiReconnect = millis();
          OperatingParameters.wifiConnected = false;
        }
      }
    }

//    if (OperatingParameters.motionDetected)
    if (lastMotionDetected > 0)
    {
      if (millis() > lastMotionDetected + MOTION_TIMEOUT)
      {
        digitalWrite(LED_BUILTIN, LOW);
        lastMotionDetected = 0;
        OperatingParameters.motionDetected = false;
      }
    }

    if (millis() > lcdTimestamp + 20000)
    {
      Serial.println("Restore display");
      //displayStartDemo();
      lcdTimestamp = millis();
    }
    else
    {
      //displayDimDemo(millis() - lcdTimestamp, false);
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