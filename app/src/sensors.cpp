#include "thermostat.hpp"
#include <Smoothed.h>
#include <timezonedb_lookup.h>
#include "DFRobot_AHT20.h"

int32_t lastTimeUpdate = 0;

DFRobot_AHT20 aht20;

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



// float degFtoC(float degF)
// {
//   return ((degF - 32.0) / (9.0/5.0));
// }

// // Return the fractional as 0 or 5 based on frac part of num
// // .0 - .4 returns 0
// // .5 to .9 returns 5
// int degCfrac(float tempF)
// {
//   if (OperatingParameters.tempUnits == 'C')
//   {
//     int whole;
//     float roundUp = degFtoC(tempF);
//     whole = (int)(roundUp);
//     float frac = roundUp - (float)(whole);

//     if (frac < 0.5)
//       return 0;
//     else
//       return 5;
//   }
//   return 0;
// }

// int tempOut(float tempF)
// {
//   if (OperatingParameters.tempUnits == 'F')
//     return (int)(tempF + 0.5);
//   else
//     return (int)(degFtoC(tempF));
// }

// float tempIn(float tempC)
// {
//   if (OperatingParameters.tempUnits == 'C')
//     return ((tempC * 9.0/5.0) + 32.0);
//   else
//     return tempC;
// }

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

//const char* ntpServer = "time.google.com";
const char* ntpServer = "pool.ntp.org";
//const char* timezone = "Africa/Luanda";
//const char* timezone = "America/New York";

void updateTimezone()
{
  // To save on program space, let's just 
  // use GMT+/- timezones. Otherwise, there
  // are too many timezones.
  // For ex, Boston would be Etc/GMT+5
  char tz_lookup[16] = "Etc/";
  strcat (tz_lookup, OperatingParameters.timezone);
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

void updateTimeSntp()
{
  struct tm time;
  char buffer[16];
   
  if (!getLocalTime(&time))
  {
    Serial.println("Could not obtain time info");
    return;
  }

  strftime(buffer, sizeof(buffer), "%H:%M:%S", &time);
  Serial.printf("Current time: %s\n", buffer);

}

void initTimeSntp()
{
  Serial.printf ("Time server: %s\n", ntpServer);

  configTime(0, 0, ntpServer);
  updateTimezone();
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

  initTimeSntp();

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
      1,              // Task priority
      NULL            // Task handle
    );

    return true;

  } else {

    return false;

  }
}
