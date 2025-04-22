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
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 * 
 */

#include "thermostat.hpp"
#include "esp_intr_alloc.h"
#include "esp_adc/adc_continuous.h"
#include "soc/adc_channel.h"
#include "esp_sntp.h"
#include <aht.h>
#include <timezonedb_lookup.h>
#include <ld2410.h>
#include <Smoothed.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "SENSORS";

// const char *gmt_timezones[] = 
//   {"GMT-12", "GMT-11", "GMT-10", "GMT-9", "GMT-8", "GMT-7", "GMT-6", "GMT-5", "GMT-4", "GMT-3", "GMT-2",  "GMT-1"
//    "GMT",    "GMT+1",  "GMT+2",  "GMT+3", "GMT+4", "GMT+5", "GMT+6", "GMT+7", "GMT+8", "GMT+9", "GMT+10", "GMT+11"};

const char *gmt_timezones[] = 
  {"GMT+11", "GMT+10", "GMT+9", "GMT+8", "GMT+7", "GMT+6", "GMT+5", "GMT+4", "GMT+3", "GMT+2", "GMT+1",
   "GMT", "GMT-1", "GMT-2", "GMT-3", "GMT-4", "GMT-5", "GMT-6", "GMT-7", "GMT-8", "GMT-9", "GMT-10", "GMT-11", "GMT-12"};

int64_t lastTimeUpdate = 0;

Stream RadarPort;
ld2410 radar;
uint64_t last_ld2410_Reading = 0;

Smoothed<float> sensorTemp;
Smoothed<float> sensorHumidity;

adc_unit_t adcUnit;
adc_channel_t adcChannel;
adc_oneshot_unit_handle_t adcHandle;


/*---------------------------------------------------------------
        ADC Code (for light sensor)
---------------------------------------------------------------*/
void initLightSensor()
{
  adc_oneshot_io_to_channel(LIGHT_SENS_PIN, &adcUnit, &adcChannel);

  adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = adcUnit,
    .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    .ulp_mode = ADC_ULP_MODE_DISABLE
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adcHandle));

  adc_oneshot_chan_cfg_t config = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adcHandle, adcChannel, &config));
}

int readLightSensor(void)
{
  static int voltage;

  ESP_ERROR_CHECK(adc_oneshot_read(adcHandle, adcChannel, &voltage));
  ESP_LOGV(TAG, "ADC%d Channel[%d] Raw Data: %d", adcUnit + 1, adcChannel, voltage);
  ESP_LOGV(TAG, "Light Sensor: %d mV", (int)voltage);

  return (int)voltage;
}

/*---------------------------------------------------------------
        Functions to convert temp values
---------------------------------------------------------------*/
float getRoundedFrac(float value)
{
  int whole;

  whole = (int)(value);
  float frac = value - (float)(whole);

  if (frac < 0.5)
    return 0;
  // else
  return 5;
}

float roundValue(float value, int places)
{
  float r = 0.0;

  if (places == 0)
    r = (float)((int)(value + 0.5));
  if (places == 1)
    r = (float)((int)(value + 0.25) + (getRoundedFrac(value + 0.25) / 10.0));
  return r;
}

//////////////////////////////////////////////////////////////////////////////////////
//
//    LD2410 support code
//
//////////////////////////////////////////////////////////////////////////////////////

static void IRAM_ATTR MotionDetect_ISR(void *arg)
{
  tftMotionTrigger = true;
}


bool ld2410_init()
{
  bool rc;

  gpio_install_isr_service(0);

  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_POSEDGE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pin_bit_mask = (1ULL<<MOTION_PIN);
  gpio_config(&io_conf);

  //attach isr handler
  gpio_isr_handler_add((gpio_num_t)MOTION_PIN, MotionDetect_ISR, nullptr);

  RadarPort.begin(UART_NUM_2, 256000, LD_RX, LD_TX); //UART for monitoring the radar
  vTaskDelay(pdMS_TO_TICKS(500));

  if (radar.begin(RadarPort))
  {
    ESP_LOGI(TAG, "LD2410: Sensor started");
    rc = true;

    if (radar.requestFirmwareVersion())
    {
      ESP_LOGI(TAG, "LD2410: Firmware: v%u.%02u.%08x",
        radar.firmware_major_version,
        radar.firmware_minor_version,
        radar.firmware_bugfix_version
        );
      snprintf (OperatingParameters.ld2410FirmWare, sizeof(OperatingParameters.ld2410FirmWare),
        "%u.%02u.%08x",
        radar.firmware_major_version,
        radar.firmware_minor_version,
        radar.firmware_bugfix_version
        );
    }
    else
    {
      ESP_LOGE(TAG, "LD2410: Failed to read firmware version\n");
      OperatingParameters.Errors.hardwareErrors++;
      snprintf (OperatingParameters.ld2410FirmWare,
        sizeof(OperatingParameters.ld2410FirmWare),
        "-.--.--------");
    }

    if (radar.requestCurrentConfiguration())
    {
      ESP_LOGI(TAG, "LD2410: Maximum gate ID: %d", radar.max_gate);
      ESP_LOGI(TAG, "LD2410: Maximum gate for moving targets: %d", radar.max_moving_gate);
      ESP_LOGI(TAG, "LD2410: Maximum gate for stationary targets: %d", radar.max_stationary_gate);
      ESP_LOGI(TAG, "LD2410: Idle time for targets: %d", radar.sensor_idle_time);
      ESP_LOGI(TAG, "LD2410: Gate sensitivity");
      for (uint8_t gate = 0; gate <= radar.max_gate; gate++)
      {
        ESP_LOGI(TAG, "  Gate %d moving targets: %d stationary targets: %d",
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
    if (radar.setMaxValues(1, 0, (MOTION_TIMEOUT / 2000)))
    {
      ESP_LOGI(TAG, "LD2410: Max gate values set");
    }
    else
    {
      ESP_LOGE(TAG, "LD2410: FAILED to set max gate values");
      OperatingParameters.Errors.hardwareErrors++;
    }
    //
    // Now request a restart to enable all the setting specified above
    //
    if (radar.requestRestart()) 
    {
      ESP_LOGW(TAG, "LD2410: Restart requested");
    }
    else
    {
      ESP_LOGE(TAG, "LD2410: FAILED requesting restart");
      OperatingParameters.Errors.hardwareErrors++;
    }
  }
  else
  {
    ESP_LOGE(TAG, "LD2410: Sensor not connected");
    OperatingParameters.Errors.hardwareErrors++;
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
      ESP_LOGD (TAG, "LD2410: Presence detected");
      if (radar.stationaryTargetDetected())
        ESP_LOGD (TAG, "LD2410: Stationary target: %d in", (int)((float)(radar.stationaryTargetDistance()) / 2.54));
      if (radar.movingTargetDetected())
        ESP_LOGD (TAG, "LD2410: Moving target: %d in", (int)((float)(radar.movingTargetDistance()) / 2.54));
    }
  }
}

/*---------------------------------------------------------------
  Function to reset variable controlled by call Smoothed library
---------------------------------------------------------------*/
void resetTempSmooth() { sensorTemp.clear(); }

/*---------------------------------------------------------------
        AHT20 sensor (temp & humidity sensor)
---------------------------------------------------------------*/
bool initAht()
{
  return i2cdev_init() == ESP_OK;
}

void updateAht(void *parameter)
{
  float humidity, temperature;

  aht_t dev = {};
  dev.mode = AHT_MODE_NORMAL;
  dev.type = AHT_TYPE_AHT20;

  ESP_ERROR_CHECK(aht_init_desc(&dev, AHT_I2C_ADDRESS_GND, (i2c_port_t)0, (gpio_num_t)SDA_PIN, (gpio_num_t)SCL_PIN));
  esp_err_t res = aht_init(&dev);
  if (res != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to initialize AHT device");
    OperatingParameters.Errors.hardwareErrors++;
    vTaskDelete(NULL);
  }

  bool calibrated;
  ESP_ERROR_CHECK(aht_get_status(&dev, NULL, &calibrated));
  if (calibrated)
  {
    ESP_LOGI(TAG, "AHT Sensor calibrated");
  }
  else
  {
    ESP_LOGW(TAG, "AHT Sensor not calibrated!");
    OperatingParameters.Errors.hardwareErrors++;
  }

  while (1)
  {
    esp_err_t res = aht_get_data(&dev, &temperature, &humidity);
    if (res == ESP_OK)
    {
      if (OperatingParameters.tempUnits == 'F')
        temperature = (temperature * 9.0 / 5.0) + 32;

      ESP_LOGD(TAG, "Temperature: %.1f°C, Humidity: %.2f%%", temperature, humidity);

      sensorTemp.add(temperature);
      sensorHumidity.add(humidity);

      OperatingParameters.tempCurrent = sensorTemp.get();
      OperatingParameters.humidCurrent = sensorHumidity.get();

      ESP_LOGI(TAG, "Temp: %0.1f (raw: %0.2f %c)  Humidity: %0.1f (raw: %0.2f)",
             sensorTemp.get() + OperatingParameters.tempCorrection,
             temperature, OperatingParameters.tempUnits,
             sensorHumidity.get() + OperatingParameters.humidityCorrection,
             humidity);
#ifdef MQTT_ENABLED
      MqttUpdateStatusTopic();
#endif
    }
    else
    {
      ESP_LOGE(TAG, "Error reading data: %d (%s)", res, esp_err_to_name(res));
      OperatingParameters.Errors.hardwareErrors++;
    }

    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

bool startAht()
{
  if (initAht())
  {
    xTaskCreate(
        updateAht,            // Function that should be called
        "Update AHT",         // Name of the task (for debugging)
        4096,                 // Stack size (bytes)
        NULL,                 // Parameter to pass
        tskIDLE_PRIORITY + 1, // Task priority
        NULL                  // Task handle
    );

    return true;
  }
  else
  {
    return false;
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

/*---------------------------------------------------------------
        Time & NTP client code
---------------------------------------------------------------*/
const char *ntpServer = "pool.ntp.org";
// const char* timezone = "Africa/Luanda";
// const char* timezone = "America/New York";

void updateTimezoneFromConfig()
{
  // To save on program space, let's just
  // use GMT+/- timezones. Otherwise, there
  // are too many timezones.
  // For ex, Boston would be Etc/GMT+5
  char tz_lookup[16] = "Etc/";
  strcat(tz_lookup, OperatingParameters.timezone);
  ESP_LOGI(TAG, "Timezone: %s", tz_lookup);
  auto tz = lookup_posix_timezone_tz(tz_lookup);
  if (!tz)
  {
    ESP_LOGE(TAG, "Invalid Timezone: %s", OperatingParameters.timezone);
    OperatingParameters.Errors.systemErrors++;
    return;
  }
  setenv("TZ", tz, 1);
  tzset();
}

void updateTimezone(char *zone)
{
  char tz_lookup[32] = "";

  // Replace all '_' chars with " "
  for (int i=0; i <= strlen(zone); i++)
  {
    if (zone[i] == '_')
      tz_lookup[i] = ' ';
    else
      tz_lookup[i] = zone[i];
  }

  ESP_LOGI(TAG, "** Timezone: %s", tz_lookup);
  auto tz = lookup_posix_timezone_tz(tz_lookup);
  if (!tz)
  {
    ESP_LOGE(TAG, "** Invalid Timezone: %s", tz_lookup);
    OperatingParameters.Errors.systemErrors++;
    return;
  }
  setenv("TZ", tz, 1);
  tzset();
}


bool getLocalTime(struct tm * info, uint64_t ms)
{
  uint64_t start = millis();
  uint64_t tmo = ms;
  time_t now;

  if (!WifiConnected())
    tmo = 20;

  while ((millis() - start) <= tmo)
  {
    time(&now);
    localtime_r(&now, info);
    if (info->tm_year > (2016 - 1900))
    {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return false;
}

static time_t last_time = (time_t)-1;

void updateTimeSntp()
{
  struct tm local_time;
  char buffer[16];
  double diff_t;

  time_t end = time(NULL);
  if ((last_time != (time_t)-1) && (end != (time_t)-1))
  {
    diff_t = difftime(end, last_time);
    // if (diff_t > 0.0)
    // {
    //   ESP_LOGW(TAG, "  Time changed by %f seconds", diff_t);
    // }

    // If the time changed more than UPDATE_TIME_INTERVAL seconds ... plus a fudge factor (10 seconds)
    if (diff_t > ((double)(UPDATE_TIME_INTERVAL) / 1000.0) + 10.0)
    {
      // To calculate the actual time adjustment, we need to subtract the
      // polling interval as defined by UPDATE_TIME_INTERVAL.
      ESP_LOGE(TAG, ">>  Time changed by %.1f seconds! <<", 
        (diff_t - ((double)(UPDATE_TIME_INTERVAL) / 1000.0)));
      ESP_LOGE(TAG, "Prior current time: %s", asctime(gmtime(&last_time)));
      ESP_LOGE(TAG, "New current time:   %s", asctime(gmtime(&end)));
    }
  }

  if (OperatingParameters.wifiConnected)
  {
    if (getLocalTime(&local_time, 1000))
    // if (updateTime(&local_time))
    {
      strftime(buffer, sizeof(buffer), "%H:%M:%S", &local_time);
      ESP_LOGI(TAG, "Current time: %s", buffer);
    }
  }

  last_time = time(NULL);
}

void configTime(const char* server)
{
  ESP_LOGI(TAG, "Initializing SNTP");
  esp_sntp_stop();
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, server);
  esp_sntp_init();
}

#include <ArduinoJson.h>
#include "esp_http_client.h"
#define MAX_HTTP_RECV_BUFFER 512

bool lookupGeoIpTimezone(char *TimeZone, int MaxLen)
{
  char *timeServer = (char *)"http://worldtimeapi.org/api/ip";
  char *buffer = (char *)malloc(MAX_HTTP_RECV_BUFFER + 1);
  
  esp_http_client_config_t config = {
      .url = timeServer,
      .path = "/"
      // .transport_type = HTTP_TRANSPORT_OVER_SSL,
      // .event_handler = _http_event_handler,
      // .cert_pem = howsmyssl_com_root_cert_pem_start,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err;
  if ((err = esp_http_client_open(client, 0)) != ESP_OK)
  {
    ESP_LOGE(__FUNCTION__, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    free(buffer);
    return false;
  }
  int content_length =  esp_http_client_fetch_headers(client);
  int total_read_len = 0, read_len;
  if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER)
  {
      read_len = esp_http_client_read(client, buffer, content_length);
      if (read_len <= 0)
      {
          ESP_LOGE(__FUNCTION__, "Failure during HTTP data read");
      }
      buffer[read_len] = 0;
      ESP_LOGD(__FUNCTION__, ">> read_len = %d", read_len);
  }
  ESP_LOGI(__FUNCTION__, "HTTP Stream reader Status = %d, content_length = %ld",
                  esp_http_client_get_status_code(client),
                  esp_http_client_get_content_length(client));
  ESP_LOGI(__FUNCTION__, "HTTP Stream Content: \"%s\"", buffer);

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  JsonDocument doc;
  deserializeJson(doc, buffer);

  const char* datetime = doc["timezone"];
  // DateTime dt = parseISO8601(String(datetime));
  ESP_LOGI (__FUNCTION__, "Timezone:  %s", datetime);

  // Generate GMT-xx format timzeone string
  {
    // "utc_offset":"-04:00" ===> Etc/GMT-4
    const char *offset=doc["utc_offset"];
    char gmt_tz[16], tmp[8] = "";
    strncpy (tmp, offset, 3);
    // Quick trick to remove leading zeros
    int b = atoi(tmp);
    snprintf (tmp, sizeof(tmp), "%+d", b);
    snprintf (gmt_tz, sizeof(gmt_tz), "GMT%s", tmp);
    ESP_LOGI (__FUNCTION__, "GMT timezone: %s", gmt_tz);

  // Save timezone in "GMTxxxx" format
    int i = 0;
    while ((strcmp(gmt_timezones[i], (const char *)gmt_tz) != 0) && (i < 24))
      i++;
    if (i < 24)
    {
      OperatingParameters.timezone_sel = i;
      ESP_LOGI (__FUNCTION__, "OperatingParameters.timezone_sel: %d", OperatingParameters.timezone_sel);
      OperatingParameters.timezone = (char *)(gmt_timezones[OperatingParameters.timezone_sel]);
      ESP_LOGI (__FUNCTION__, "OperatingParameters.timezone:     %s", OperatingParameters.timezone);
    }
  }

  // Send data back to user
  strncpy (TimeZone, datetime, MaxLen);

  free(buffer);

  return true;
}

void initTimeSntp()
{
  ESP_LOGI(TAG, "Time server: %s", ntpServer);
  char TimeZone[32];

  if (lookupGeoIpTimezone(TimeZone, sizeof(TimeZone)))
    updateTimezone(TimeZone);
  else
    updateTimezoneFromConfig();
  configTime(ntpServer);
  updateTimeSntp();
}

/*---------------------------------------------------------------
        Init code for sensors entry point
---------------------------------------------------------------*/
void sensorsInit()
{
  sensorTemp.begin(SMOOTHED_EXPONENTIAL, 10);
  sensorHumidity.begin(SMOOTHED_EXPONENTIAL, 10);
  sensorTemp.clear();
  sensorHumidity.clear();

  startAht();
  ld2410_init();
  initLightSensor();
}
