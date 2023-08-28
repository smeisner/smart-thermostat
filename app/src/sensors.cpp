/*!
// SPDX-License-Identifier: GPL-3.0-only
/*
 * sensors.cpp
 *
 * This module supports all the sensor devices included with the thermostat. This includes
 * includes the AHT20, the LDR light detector, LD2410 uWave human presence detector and
 * the SNTP provided time.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * Notes:
 *  The DFRobot_AHT20 module was provided on Github and used here to
 *  drive the AHT20 temp/humidity sensor. This module was supplied by DFRobot.
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 * 
 */

#include "thermostat.hpp"
#include <Smoothed.h>
#include <timezonedb_lookup.h>
#include "DFRobot_AHT20.h"
#include <ld2410.h>

int32_t lastTimeUpdate = 0;

DFRobot_AHT20 aht20;

ld2410 radar;
uint32_t last_ld2410_Reading = 0;

Smoothed <float> sensorTemp;
Smoothed <float> sensorHumidity;




float getRoundedFrac(float value)
{
  int whole;

  whole = (int)(value);
  float frac = value - (float)(whole);

  if (frac < 0.5)
    return 0;
  else
    return 5;
}

float roundValue(float value, int places)
{
  Serial.printf ("Convert: %.1f ---> ", value);
  float r = 0.0;

  if (places == 0)
    r = (float)((int)(value + 0.5));
  if (places == 1)
    r = (float)((int)(value + 0.25) + (getRoundedFrac(value + 0.25) / 10.0));
  Serial.printf ("%.1f\n", r);
  return r;

  // 12.2 -> 12.0
  // 12.3 -> 12.5
  // 12.7 -> 12.5
  // 12.8 -> 13.0
}


//////////////////////////////////////////////////////////////////////////////////////
//
//    LD2410 support code
//
//////////////////////////////////////////////////////////////////////////////////////

bool ld2410_init()
{
  bool rc;

  //Uncomment to show debug information from the library on the Serial Monitor. By default this does not show sensor reads as they are very frequent.
  //radar.debug(Serial);

  Serial2.begin (256000, SERIAL_8N1, LD_TX, LD_RX); //UART for monitoring the radar
  delay(500);

  if (radar.begin(Serial2))
  {
    Serial.printf("LD2410: Sensor started\n");
    rc = true;

    if (radar.requestFirmwareVersion())
    {
      Serial.printf ("LD2410: Firmware: v%u.%02u.%u%u%u%u\n",
        radar.firmware_major_version,
        radar.firmware_minor_version,
        (radar.firmware_bugfix_version & 0xff000000) >> 24,
        (radar.firmware_bugfix_version & 0x00ff0000) >> 16,
        (radar.firmware_bugfix_version & 0x0000ff00) >> 8,
        radar.firmware_bugfix_version & 0x000000ff
        );
    } else {
      Serial.printf ("LD2410: Failed to read firmware version\n");
    }

    if (radar.requestCurrentConfiguration())
    {
      Serial.printf("LD2410: Maximum gate ID: %d\n", radar.max_gate);
      Serial.printf("LD2410: Maximum gate for moving targets: %d\n", radar.max_moving_gate);
      Serial.printf("LD2410: Maximum gate for stationary targets: %d\n", radar.max_stationary_gate);
      Serial.printf("LD2410: Idle time for targets: %d\n", radar.sensor_idle_time);
      Serial.printf("LD2410: Gate sensitivity\n");
      for (uint8_t gate = 0; gate <= radar.max_gate; gate++)
      {
        Serial.printf("  Gate %d moving targets: %d stationary targets: %d\n",
          gate, radar.motion_sensitivity[gate], radar.stationary_sensitivity[gate]);
      }
    }

    //
    // The LD2410 module has multiple gates, one per each 0.75m of distance. So gate 0 will specify the sensitivity
    // for 0 - 0.75m, gate 1 will specify sensitivity for 0.75 - 1.5m, etc. Setting MaxValues (below) specifies
    // max distance based on the number of gates enabled. For example, specifying 1 for max gates will allow 1.5m (0 & 1).
    //
    //bool setGateSensitivityThreshold(uint8_t gate, uint8_t moving, uint8_t stationary);
    radar.setGateSensitivityThreshold(0, 50, 30); // Default values (50 & 30)
    radar.setGateSensitivityThreshold(1, 50, 30);
    //
    // Each gate is ~0.75m, therefore moving gate should be limited to gate 1 (1.5m) and stationary gate should be
    // limited to 0 (0.75m). Use this to also change the inactivity timer.
    //
    //bool setMaxValues(uint16_t moving, uint16_t stationary, uint16_t inactivityTimer);
    if (radar.setMaxValues(1, 0, (MOTION_TIMEOUT / 2000))) Serial.printf ("LD2410: Max gate values set\n"); else Serial.printf ("LD2410: FAILED to set max gate values\n");
    //
    // Now request a restart to enable all the setting specified above
    //
    if (radar.requestRestart()) Serial.printf ("LD2410: Restart requested\n"); else Serial.printf ("LD2410: FAILED requesting restart\n");
  }
  else
  {
    Serial.printf ("LD2410: Sensor not connected\n");
    rc = false;
  }
  return rc;
}

void ld2410_loop()
{
  if (!radar.isConnected())
    return;

  radar.read();
  if (radar.isConnected() && millis() - last_ld2410_Reading > 1000)  //Report every 1000ms
  {
    last_ld2410_Reading = millis();
    if (radar.presenceDetected())
    {
      if (radar.stationaryTargetDetected())
        Serial.printf ("LD2410: Stationary target: %d in\n", (int)((float)(radar.stationaryTargetDistance()) / 2.54));
      // {
      //   Serial.print(F("Stationary target: "));
      //   Serial.print(radar.stationaryTargetDistance());
      //   Serial.print(F("cm energy:"));
      //   Serial.println(radar.stationaryTargetEnergy());
      // }
      if (radar.movingTargetDetected())
        Serial.printf ("LD2410: Moving target: %d in\n", (int)((float)(radar.movingTargetDistance()) / 2.54));
      // {
      //   Serial.print(F("Moving target: "));
      //   Serial.print(radar.movingTargetDistance());
      //   Serial.print(F("cm energy:"));
      //   Serial.println(radar.movingTargetEnergy());
      // }
    }
    // else
    // {
    //   Serial.println(F("No target"));
    // }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////


bool initAht()
{
  Serial.println("Calling aht20.begin()");

  uint8_t status;
  status = aht20.begin();
  while (status != 0)
  {
    Serial.printf("AHT20 sensor initialization failed. error status : %d\n", status);
    delay(1000);
    status = aht20.begin();
  }
  return true;
}

void resetTempSmooth() { sensorTemp.clear(); }

void readAht()
{
  float humidity, temperature;
  float temp_f;
  
  if (aht20.startMeasurementReady(/* crcEn = */true))
  {
    if (OperatingParameters.tempUnits == 'C')
      temperature = aht20.getTemperature_C();
    else
      temperature = aht20.getTemperature_F();
    humidity = aht20.getHumidity_RH();

    sensorTemp.add(temperature);
    sensorHumidity.add(humidity);

    OperatingParameters.tempCurrent = sensorTemp.get();
    OperatingParameters.humidCurrent = sensorHumidity.get();

    Serial.printf ("Temp: %0.1f (raw: %0.2f %c)  Humidity: %0.1f (raw: %0.2f)\n", 
      sensorTemp.get(), temperature, OperatingParameters.tempUnits, sensorHumidity.get(), humidity);
  }
}

// Read sensor temp and return rounded up and correction applied
int getTemp()
{
  return (int)((sensorTemp.get() + 0.5) + OperatingParameters.tempCorrection);
}

int getHumidity()
{
  return (int)((sensorHumidity.get() + 0.5) + OperatingParameters.humidityCorrection);
}

void updateAht(void * parameter)
{
  for(;;) // infinite loop
  {
    // Read latest temp & humidity values
    readAht();

    // Pause the task again for 10000ms
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

const char* ntpServer = "pool.ntp.org";
//const char* timezone = "Africa/Luanda";
//const char* timezone = "America/New York";

void updateTimezone(bool logInfo)
{
  // To save on program space, let's just 
  // use GMT+/- timezones. Otherwise, there
  // are too many timezones.
  // For ex, Boston would be Etc/GMT+4
  char tz_lookup[16] = "Etc/";
  strcat (tz_lookup, OperatingParameters.timezone);
  if (logInfo)
    Serial.printf ("Timezone: %s\n", tz_lookup);
  auto tz = lookup_posix_timezone_tz(tz_lookup);
  if (!tz)
  {
    Serial.printf ("Invalid Timezone: %s\n", OperatingParameters.timezone);
    return;
  }

  setenv("TZ", tz, 1);
  tzset();
}

bool updateTime(struct tm * info)
{
  if (OperatingParameters.wifiConnected)
  {
    if (!getLocalTime(info, 2000))
    {
      Serial.println("Could not obtain time info");
      return false;
    }
    else
      return true;
  }

  return false;
}

void updateTimeSntp()
{
  struct tm local_time;
  char buffer[16];

  if (updateTime(&local_time))
  {
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &local_time);
    Serial.printf("Current time: %s\n", buffer);
  }
}

void initTimeSntp(bool logInfo)
{
  if (logInfo)
    Serial.printf ("Time server: %s\n", ntpServer);

  configTime(0, 0, ntpServer);
  updateTimezone(logInfo);
  updateTimeSntp();
}

void IRAM_ATTR MotionDetect_ISR()
{
  tftMotionTrigger = true;
}

bool sensorsInit()
{
  sensorTemp.begin(SMOOTHED_EXPONENTIAL, 10);
  sensorHumidity.begin(SMOOTHED_EXPONENTIAL, 10);
  sensorTemp.clear();
  sensorHumidity.clear();

  initTimeSntp(true);
  
  ld2410_init();

  pinMode(LIGHT_SENS_PIN, INPUT);
  pinMode(MOTION_PIN, INPUT);
  attachInterrupt(MOTION_PIN, MotionDetect_ISR, RISING);

  if (initAht())
  {
    xTaskCreate(
      updateAht,      // Function that should be called
      "Update AHT",   // Name of the task (for debugging)
      4096,           // Stack size (bytes)
      NULL,           // Parameter to pass
      tskIDLE_PRIORITY+1, // Task priority
      NULL            // Task handle
    );

    return true;

  } else {

    return false;

  }
}
