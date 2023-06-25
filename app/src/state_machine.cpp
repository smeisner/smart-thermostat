#include "thermostat.hpp"

OPERATING_PARAMETERS OperatingParameters;
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
      digitalWrite(HVAC_HEAT_PIN, LOW);
      digitalWrite(HVAC_COOL_PIN, LOW);
      digitalWrite(HVAC_FAN_PIN, LOW);
      digitalWrite(LED_COOL_PIN, LOW);
      digitalWrite(LED_HEAT_PIN, LOW);
      digitalWrite(LED_FAN_PIN, LOW);
    }
    else if (OperatingParameters.hvacSetMode == FAN)
    {
      OperatingParameters.hvacOpMode = FAN;
      digitalWrite(HVAC_HEAT_PIN, LOW);
      digitalWrite(HVAC_COOL_PIN, LOW);
      digitalWrite(HVAC_FAN_PIN, HIGH);
      digitalWrite(LED_COOL_PIN, LOW);
      digitalWrite(LED_HEAT_PIN, LOW);
      digitalWrite(LED_FAN_PIN, HIGH);
    }
    else if (OperatingParameters.hvacSetMode == HEAT)
    {
      if (OperatingParameters.hvacOpMode == COOL)
      {
        digitalWrite(HVAC_COOL_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_FAN_PIN, LOW);
      }
      if (currentTemp < minTemp)
      {
        OperatingParameters.hvacOpMode = HEAT;
        digitalWrite(HVAC_HEAT_PIN, HIGH);
        digitalWrite(LED_HEAT_PIN, HIGH);
        digitalWrite(LED_FAN_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_FAN_PIN, LOW);
      }
    }
    else if (OperatingParameters.hvacSetMode == AUX_HEAT)
    {
      //
      // Set up for 2-stage, emergency or aux heat mode
      // Set relays correct
      // For now, just emulate HEAT mode
      //
      if (OperatingParameters.hvacOpMode == COOL)
      {
        //digitalWrite(HVAC_COOL_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_FAN_PIN, LOW);
      }
      if (currentTemp < minTemp)
      {
        OperatingParameters.hvacOpMode = HEAT;
        digitalWrite(HVAC_HEAT_PIN, HIGH);
        digitalWrite(LED_HEAT_PIN, HIGH);
        digitalWrite(LED_FAN_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_FAN_PIN, LOW);
      }
    }
    else if (OperatingParameters.hvacSetMode == COOL)
    {
      if (OperatingParameters.hvacOpMode == HEAT)
      {
        digitalWrite(HVAC_HEAT_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_FAN_PIN, LOW);
      }
      if (currentTemp > maxTemp)
      {
        OperatingParameters.hvacOpMode = COOL;
        digitalWrite(HVAC_COOL_PIN, HIGH);
        digitalWrite(LED_COOL_PIN, HIGH);
        digitalWrite(LED_FAN_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        digitalWrite(LED_FAN_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
      }
    }
    else if (OperatingParameters.hvacSetMode == AUTO)
    {
      if (currentTemp < autoMinTemp)
      {
        OperatingParameters.hvacOpMode = HEAT;
        digitalWrite(HVAC_HEAT_PIN, HIGH);
        digitalWrite(LED_HEAT_PIN, HIGH);
        digitalWrite(HVAC_COOL_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_FAN_PIN, LOW);
      }
      else if (currentTemp > autoMaxTemp)
      {
        OperatingParameters.hvacOpMode = COOL;
        digitalWrite(HVAC_COOL_PIN, HIGH);
        digitalWrite(LED_COOL_PIN, HIGH);
        digitalWrite(HVAC_HEAT_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_FAN_PIN, LOW);
      }
      else
      {
        OperatingParameters.hvacOpMode = IDLE;
        digitalWrite(HVAC_COOL_PIN, LOW);
        digitalWrite(HVAC_HEAT_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_FAN_PIN, LOW);
      }
    }

    if ((currentTemp >= minTemp) && (currentTemp <= maxTemp) && (OperatingParameters.hvacSetMode != FAN) && (OperatingParameters.hvacSetMode != OFF))
    {
        OperatingParameters.hvacOpMode = IDLE;
        digitalWrite(HVAC_HEAT_PIN, LOW);
        digitalWrite(HVAC_COOL_PIN, LOW);
        digitalWrite(LED_COOL_PIN, LOW);
        digitalWrite(LED_HEAT_PIN, LOW);
        digitalWrite(LED_FAN_PIN, LOW);
    }

    if (millis() - lastTimeUpdate > UPDATE_TIME_INTERVAL)
    {
      lastTimeUpdate = millis();
      updateTimeSntp();
    }

    // Check wifi
    if (!wifiConnected()) 
    {
      if (millis() > lastWifiReconnect + 15000)
      {
        lastWifiReconnect = millis();
        startReconnectTask();
      }
    }

    if (Serial.available())
    {
      String temp = Serial.readString();
      Serial.print ("[USB] "); Serial.println(temp);
      if (temp.indexOf("reset") > -1)
        ESP.restart();
      // if (temp.indexOf("scan") > -1)
      //   scanI2cBus();
      if (temp.indexOf("temp") > -1)
      {
        Serial.printf ("Current Set Temp: %.1f\n", OperatingParameters.tempSet);
      }
    }

/*
 * Test code
 */
  static int loop = 0;
  if (loop++ >= 40) 
  {
//    Serial.printf ("Value of Motion GPIO %d: %d\n", MOTION_PIN, analogRead(MOTION_PIN));
//    Serial.printf ("Value of Motion GPIO %d: %d\n", MOTION_PIN, digitalRead(MOTION_PIN));
    digitalWrite (LED_FAN_PIN, digitalRead(MOTION_PIN));

    if (digitalRead(MOTION_PIN))
      tftMotionTrigger = true;

    // digitalWrite (GPIO_NUM_35, HIGH);
    // digitalWrite (GPIO_NUM_36, HIGH);
    // delay(50);
    // digitalWrite (GPIO_NUM_35, LOW);
    // digitalWrite (GPIO_NUM_36, LOW);

    loop = 0;
  }
/*
 * Test code
 */

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
  //@@@ digitalWrite(HVAC_HEAT_PIN, HIGH);
  // delay(750);
  // digitalWrite(HVAC_HEAT_PIN, LOW);
}

void serialStart()
{
  int32_t tmo = millis() + 60000;
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.printf("Regular Serial (via USB - usually /dev/ttyUSB0)\n");
}