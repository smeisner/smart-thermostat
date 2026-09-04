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
#include "driver/i2c_types.h"
#include "driver/i2c_master.h"
#include <aht.h>
#include <timezonedb_lookup.h>
#include <ld2410.h>
#include <Smoothed.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "SENSORS";

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

HVAC_MODE requestedHvacMode = ERROR;
float requestedHvacSetTemp = 0.0;
bool ModeChangeRequested;
int64_t ModeChangeRequestTime;
bool TempChangeRequested;
int64_t TempChangeRequestTime;

void _updateHvacMode(HVAC_MODE mode)
{
  OperatingParameters.hvacSetMode = mode;
  eepromUpdateHvacSetMode();
#ifdef MQTT_ENABLED
  MqttUpdateStatusTopic();
#endif
}

void _updateHvacSetTemp(float setTemp)
{
  OperatingParameters.tempSet = setTemp;
  eepromUpdateHvacSetTemp();
  ESP_LOGI(TAG, "Set temp: %.1f", setTemp);
#ifdef MQTT_ENABLED
  MqttUpdateStatusTopic();
#endif
}

void updateHvacMode(HVAC_MODE mode)
{
  requestedHvacMode = mode;
  ModeChangeRequested = true;
  ModeChangeRequestTime = millis();
}

void updateHvacSetTemp(float setTemp)
{
  requestedHvacSetTemp = setTemp;
  TempChangeRequested = true;
  TempChangeRequestTime = millis();
}

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
  // Read the physical state. If it's 1, a voltage transition occurred.
  if (gpio_get_level((gpio_num_t)MOTION_PIN) == 1)
  {

    // DE-BOUNCE/STABILITY CHECK:
    // Loop briefly (approx 10-15 microseconds) to verify the line stays high.
    // High-speed cross-talk noise spikes vanish instantly, but a real human 
    // presence signal from the LD2410 remains a rock-solid 3.3V.
    bool true_signal = true;
    for (int i = 0; i < 50; i++)
    {
      if (gpio_get_level((gpio_num_t)MOTION_PIN) == 0)
      {
        true_signal = false;
        break;
      }
    }

    // Only trigger your thermostat's display if the signal is verified as real
    if (true_signal)
    {
      tftMotionTrigger = true;
    }
  }
}

bool ld2410_init()
{
  bool rc = false;

  // Initialize the UART hardware settings safely using stream.cpp
  RadarPort.begin(UART_NUM_2, 256000, LD_RX, LD_TX); 

  int radar_retries = 5;
  ESP_LOGI(TAG, "Attempting connection to LD2410 Radar Module...");

  for (int attempt = 1; attempt <= radar_retries; attempt++)
  {
    vTaskDelay(pdMS_TO_TICKS(150)); 
    uart_flush(UART_NUM_2);
    uart_clear_intr_status(UART_NUM_2, 0xFFFFFFFF);

    if (radar.begin(RadarPort))
    {
      ESP_LOGI(TAG, "LD2410: Sensor started successfully on attempt %d!", attempt);
      rc = true;
      break;
    }
    else
    {
      ESP_LOGW(TAG, "LD2410 handshake attempt %d failed. Retrying...", attempt);
    }
  }

  // Process configurations ONLY if the loop caught a valid hardware signature
  if (rc)
  {
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
    radar.setGateSensitivityThreshold(0, 90, 80); 
    radar.setGateSensitivityThreshold(1, 85, 75);
    //
    // Each gate is ~0.75m, therefore moving gate should be limited to gate 1 (1.5m) and stationary gate should be
    // limited to 0 (0.75m). Use this to also change the inactivity timer.
    //
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
    // ... inside ld2410_init() after setMaxValues ...
    if (radar.requestRestart()) 
    {
      ESP_LOGW(TAG, "LD2410: Restart requested. Waiting for sensor boot...");
      // CRITICAL: Give the physical radar hardware time to reboot 
      // BEFORE allowing the loop to flood the serial port!
      vTaskDelay(pdMS_TO_TICKS(1500)); 
      uart_flush(UART_NUM_2); // Flush any bootup bootloader junk characters
    }
    else
    {
      ESP_LOGE(TAG, "LD2410: FAILED requesting restart");
      OperatingParameters.Errors.hardwareErrors++;
    }
  }
  else
  {
    ESP_LOGE(TAG, "LD2410: Sensor not connected after maximum retries");
    OperatingParameters.Errors.hardwareErrors++;
    rc = false;
  }
  return rc;
}

void ld2410_loop()
{
  // Silently read bytes 1-by-1. ONLY execute logic when a complete frame (1) is ready.
  // Do NOT log warnings if it returns 0; 0 just means "still waiting for full packet".
  int readStatus = radar.read(); 

  if (readStatus == 1)
  {
    // A complete, valid data frame has arrived from the radar!
    if (radar.presenceDetected())
    {
      tftMotionTrigger = true;
    }

    // Report metrics every 1000ms
    if (millis() - last_ld2410_Reading > 1000)  
    {
      last_ld2410_Reading = millis();
      ESP_LOGD(TAG, "LD2410: Radar Active. Target Presence: %s", radar.presenceDetected() ? "YES" : "NO");
      
      if (radar.presenceDetected())
      {
        if (radar.stationaryTargetDetected())
          ESP_LOGD(TAG, "LD2410: Stationary target: %d in", (int)((float)(radar.stationaryTargetDistance()) / 2.54));
        if (radar.movingTargetDetected())
          ESP_LOGD(TAG, "LD2410: Moving target: %d in", (int)((float)(radar.movingTargetDistance()) / 2.54));
      }
    }
  }
  else if (readStatus == -1)
  {
    // The underlying library encountered a completely corrupted packet frame
    ESP_LOGE(TAG, "LD2410: Frame parsing checksum or formatting failure.");
    OperatingParameters.Errors.hardwareErrors++;
  }
}

/*---------------------------------------------------------------
  Function to reset variable controlled by call Smoothed library
---------------------------------------------------------------*/
void resetTempSmooth() { sensorTemp.clear(); }

/*---------------------------------------------------------------
        AHT20 sensor (temp & humidity sensor)
---------------------------------------------------------------*/

#include "soft_i2c.hpp"

void process_aht20_data(const uint8_t *read_buffer, float *temperature, float *humidity)
{
  // 1. Extract the 20-bit raw Humidity integer value
  // Combines Byte 1, Byte 2, and the high nibble (upper 4 bits) of Byte 3
  uint32_t raw_humidity = ((uint32_t)read_buffer[1] << 12) | 
                          ((uint32_t)read_buffer[2] << 4)  | 
                          ((read_buffer[3] & 0xF0) >> 4);

  // 2. Extract the 20-bit raw Temperature integer value
  // Combines the low nibble (lower 4 bits) of Byte 3, Byte 4, and Byte 5
  uint32_t raw_temperature = (((uint32_t)read_buffer[3] & 0x0F) << 16) | 
                              ((uint32_t)read_buffer[4] << 8)   | 
                              ((uint32_t)read_buffer[5]);

  // 3. Apply floating-point math scaling calculations (2^20 = 1048576)
  *humidity = ((float)raw_humidity / 1048576.0f) * 100.0f;
  *temperature = ((float)raw_temperature / 1048576.0f) * 200.0f - 50.0f;



  // // Humidity is 20 bits (data[1], data[2], data[3] upper nibble)
  // uint32_t raw_humidity = ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | (data[3] >> 4);
  // *humidity = (float)(raw_humidity * 100 / 0x100000);

  // // Temperature is 20 bits (data[3] lower nibble, data[4], data[5])
  // uint32_t raw_temperature = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
  // *temperature = ((float)raw_temperature / 1048576) * 200.0 - 50.0;

  ESP_LOGD(TAG, "Temperature: %.1f°C, Humidity: %.2f%%", *temperature, *humidity);

  if (OperatingParameters.tempUnits == 'F')
    *temperature = (*temperature * 9.0 / 5.0) + 32;

  ESP_LOGD(TAG, "Temperature: %.1f°C, Humidity: %.2f%%", *temperature, *humidity);

  sensorTemp.add(*temperature);
  sensorHumidity.add(*humidity);

  OperatingParameters.tempCurrent = sensorTemp.get();
  OperatingParameters.humidCurrent = sensorHumidity.get();

  ESP_LOGI(TAG, "Temp: %0.1f (raw: %0.2f %c)  Humidity: %0.1f (raw: %0.2f)",
          sensorTemp.get() + OperatingParameters.tempCorrection,
          *temperature, OperatingParameters.tempUnits,
          sensorHumidity.get() + OperatingParameters.humidityCorrection,
          *humidity);

#ifdef MQTT_ENABLED
  MqttUpdateStatusTopic();
#endif
}

// static const char *TAG = "AHT20_BITBANG";
static SoftI2C software_i2c;

void aht20_sensor_task(void *pvParameters)
{
  float humidity=0.0, temperature=0.0;

  // Initialize the software layers
  software_i2c.init();
  vTaskDelay(pdMS_TO_TICKS(150)); // Wake up delay window

  // Initialize the AHT20 sensor
  uint8_t init_cmd[] = {0xE1, 0x08, 0x00};
  
  ESP_LOGI(TAG, "Initializing AHT20 via Software Bit-Banging...");
  if (software_i2c.transmit(AHT20_SENSOR_ADDR, init_cmd, sizeof(init_cmd)))
  {
    ESP_LOGI(TAG, "--> SUCCESS! The device acknowledged the software handshake!");
  }
  else
  {
    // This always fails on the first try, but subsequent calls work.
    //@@@ ESP_LOGE(TAG, "--> FAILED! Device NACKed or traces could not be driven.");
  }

  uint8_t read_buffer[6] = {0};
  uint8_t trigger_cmd[] = {0xAC, 0x33, 0x00};

  while (1)
  {
    // Trigger measurement
    software_i2c.transmit(AHT20_SENSOR_ADDR, trigger_cmd, sizeof(trigger_cmd));
    vTaskDelay(pdMS_TO_TICKS(80)); // Wait for hardware data conversion

    // Read data packets back
    if (software_i2c.receive(AHT20_SENSOR_ADDR, read_buffer, sizeof(read_buffer)))
    {
      process_aht20_data(read_buffer, &temperature, &humidity);
    } else {
      ESP_LOGE(TAG, "Packet read transaction failed.");
    }

    vTaskDelay(pdMS_TO_TICKS(10000)); // Sample loop execution window
  }
}

bool startAht()
{

  xTaskCreate(aht20_sensor_task, "aht20_sensor_task", 4096, NULL, 5, NULL);
  return true;
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

// Init last_time to -1 so we can detect the first time update and not report a time change
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

    // If the time changed more than UPDATE_TIME_INTERVAL seconds ... plus a fudge factor (10 seconds)
    if ((last_time != (time_t)-1) && (diff_t > ((double)(UPDATE_TIME_INTERVAL) / 1000.0) + 10.0))
    {
      // To calculate the actual time adjustment, we need to subtract the
      // polling interval as defined by UPDATE_TIME_INTERVAL.
      ESP_LOGW(TAG, ">>  Time changed by %.1f seconds! <<", 
        (diff_t - ((double)(UPDATE_TIME_INTERVAL) / 1000.0)));
      ESP_LOGW(TAG, "Prior current time: %s", asctime(gmtime(&last_time)));
      ESP_LOGW(TAG, "New current time:   %s", asctime(gmtime(&end)));
    }
  }

  if (OperatingParameters.wifiConnected)
  {
    if (getLocalTime(&local_time, 1000))
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
#define MAX_HTTP_RECV_BUFFER 1023

/*

To disable cert/CA auth in an HTTPS request:

Open a terminal in your project's root directory.
Run pio run -t menuconfig.
Navigate through the menu options: Component config -> ESP-TLS.
Enable the option "Allow potentially insecure options".
Enable the option "Skip server certificate verification by default" (accepting the risks).

*/
bool lookupGeoIpTimezone(char *TimeZone, int MaxLen)
{
  char *timeServer = (char *)"https://ipapi.co/json/";
  char *buffer = (char *)malloc(MAX_HTTP_RECV_BUFFER + 1);

  esp_http_client_config_t config = {};
  memset(&config, 0, sizeof(config));
  config.url = timeServer;
  config.path = "/";
  config.transport_type = HTTP_TRANSPORT_OVER_SSL;
  config.buffer_size = MAX_HTTP_RECV_BUFFER;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err;
  if ((err = esp_http_client_open(client, 0)) != ESP_OK)
  {
    ESP_LOGE(__FUNCTION__, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    free(buffer);
    return false;
  }
  int content_length = esp_http_client_fetch_headers(client);
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

  if ((esp_http_client_get_status_code(client) == -1) ||
      (esp_http_client_get_content_length(client) == 0))
  {
    ESP_LOGE(__FUNCTION__, "esp_http_client_fetch_headers failed!\n");
    free(buffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }
  ESP_LOGI(__FUNCTION__, "HTTP Stream Content: \"%s\"", buffer);

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  JsonDocument doc;
  deserializeJson(doc, buffer);
  free(buffer);

  const char* datetime = doc["timezone"];
  // DateTime dt = parseISO8601(String(datetime));
  ESP_LOGI (__FUNCTION__, "Timezone:  %s", datetime);

  // Generate GMT-xx format timzeone string
  {
    // "utc_offset":"-04:00" ===> Etc/GMT-4
    const char *offset = doc["utc_offset"];
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

  return true;
}

void initTimeSntp()
{
  ESP_LOGI(TAG, "Time server: %s", ntpServer);
  char TimeZone[32];

  if (!WifiConnected()) return;

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
