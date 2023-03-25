#include "thermostat.hpp"
#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht;
int32_t lastMotionDetected = 0;

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
  OperatingParameters.tempCurrent = temp_f;
  OperatingParameters.humidCurrent = humidity.relative_humidity;
  Serial.print("Temperature: ");Serial.print(temp.temperature);Serial.print(" degrees C (");Serial.print(temp_f);Serial.println(" F)");
  Serial.print("Humidity: ");Serial.print(humidity.relative_humidity);Serial.println(" RH %");
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

void initMotion()
{
  pinMode(MOTION_PIN, INPUT);
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


bool sensorsInit()
{
  bool r;

  initMotion();
  r = initAht();

  pinMode (LIGHT_SENS_PIN, INPUT);

  attachInterrupt(MOTION_PIN, MotionDetect_ISR, RISING);

  xTaskCreate(
    updateAht,      // Function that should be called
    "Update AHT",   // Name of the task (for debugging)
    4096,           // Stack size (bytes)
    NULL,           // Parameter to pass
    1,              // Task priority
    NULL            // Task handle
  );

  return r;
}
