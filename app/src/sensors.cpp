#include "thermostat.hpp"
#include <Adafruit_AHTX0.h>
#include <Smoothed.h>
#include <timezonedb_lookup.h>

Smoothed <float> sensorTemp;
Smoothed <float> sensorHumidity;


float degFtoC(float degF)
{
  return ((degF - 32.0) / (9.0/5.0));
}

// Return the fractional as 0 or 5 based on frac part of num
// .0 - .4 returns 0
// .5 to .9 returns 5
int degCfrac(float tempF)
{
  if (OperatingParameters.tempUnits == 'C')
  {
    int whole;
    float roundUp = degFtoC(tempF);
    whole = (int)(roundUp);
    float frac = roundUp - (float)(whole);

    if (frac < 0.5)
      return 0;
    else
      return 5;
  }
  return 0;
}

int tempOut(float tempF)
{
  if (OperatingParameters.tempUnits == 'F')
    return (int)(tempF + 0.5);
  else
    return (int)(degFtoC(tempF));
}

float tempIn(float tempC)
{
  if (OperatingParameters.tempUnits == 'C')
    return ((tempC * 9.0/5.0) + 32.0);
  else
    return tempC;
}

Adafruit_AHTX0 aht;
int32_t lastMotionDetected = 0;
int32_t lastTimeUpdate = 0;

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
  sensorTemp.add(temp.temperature);
  sensorHumidity.add(humidity.relative_humidity);
  temp_f = (sensorTemp.get() * 9.0/5.0) + 32.0;
  OperatingParameters.tempCurrent = temp_f;
  OperatingParameters.humidCurrent = sensorHumidity.get();

  Serial.printf ("Temp: %0.1f C / %0.1f F (raw: %0.2f C)  Humidity: %0.1f (raw: %0.2f)\n", 
    sensorTemp.get(), temp_f, temp.temperature, sensorHumidity.get(), humidity.relative_humidity);
}

// Read sensor temp and return in deg F, rounded up and correction applied
int getTemp()
{
  return (((sensorTemp.get() * 9.0/5.0) + 32.0 + 0.5) + OperatingParameters.tempCorrection);
}

int getHumidity()
{
  return (int)(sensorHumidity.get() + 0.5);
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

void IRAM_ATTR MotionDetect_ISR()
{
  if (millis() - lastMotionDetected > MOTION_TIMEOUT)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    lastMotionDetected = millis();
    OperatingParameters.motionDetected = true;
  }
}

//const char* ntpServer = "time.google.com";
const char* ntpServer = "pool.ntp.org";
//const char* timezone = "Africa/Luanda";
//const char* timezone = "America/New York";

void updateTimezone()
{
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

bool sensorsInit()
{
  sensorTemp.begin(SMOOTHED_EXPONENTIAL, 10);
  sensorHumidity.begin(SMOOTHED_EXPONENTIAL, 10);
  sensorTemp.clear();
  sensorHumidity.clear();

  initTimeSntp();

  pinMode (LIGHT_SENS_PIN, INPUT);
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
