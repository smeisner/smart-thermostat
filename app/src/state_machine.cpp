// SPDX-License-Identifier: GPL-3.0-only
/*
 * state_machine.cpp
 *
 * Implements the state machine to control the HVAC actions and modes.
 * As part of this responsibility, statuses of items such as wifi connection
 * are also monitored and action taken if necessary.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * Notes:
 *   Need to add support for reversing valve (when aux heat enabled)
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 *  11-Oct-2023: Steve Meisner (steve@meisners.net) - Add support for home automation (MQTT & Matter)
 * 
 */
#include <stdbool.h>
#include "thermostat.hpp"
#include "driver/gpio.h"

OPERATING_PARAMETERS OperatingParameters;
extern int64_t lastTimeUpdate;
int64_t lastWifiReconnect;
static bool MqttConnectCalled = false;
static int64_t lastFanOnTime = 0;

struct gpio_pin_desc {
  short pin;
  short level;
};

#define NR_GPIO_PINS      (8)
#define INVALID_PIN()     {.pin=-1, .level=-1} /* plays as an arbitrary stop guard for the iterator code */
#define PIN(p, l)         {.pin=(p), .level=(l)}
#define DESC(mode, ...)   [(mode)] = {__VA_ARGS__, INVALID_PIN()}

/*
 * NOTE: g++ doesn't support C's non-trivial designated initializers,
 * so the array must be initilazed in the HVAC_MODE enum order
 */
const struct gpio_pin_desc hvac_mode_gpio[NR_HVAC_MODES][NR_GPIO_PINS] = {
  DESC(OFF, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, LOW),
            PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, LOW),
            PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(HEAT, PIN(HVAC_HEAT_PIN, HIGH), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, HIGH),
            PIN(LED_HEAT_PIN, HIGH), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, HIGH),
            PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(COOL, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, HIGH), PIN(HVAC_FAN_PIN, HIGH),
            PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, HIGH), PIN(LED_FAN_PIN, HIGH),
            PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(IDLE, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, LOW),
            PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, LOW),
            PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(FAN_ONLY, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, HIGH),
            PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, HIGH),
            PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(AUTO, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, LOW),
            PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, LOW),
            PIN(HVAC_STAGE2_PIN, LOW)),
  DESC(AUX_HEAT, PIN(HVAC_HEAT_PIN, HIGH), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, HIGH),
            PIN(LED_HEAT_PIN, HIGH), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, HIGH),
            PIN(HVAC_STAGE2_PIN, HIGH)),
  DESC(DRY, PIN(HVAC_HEAT_PIN, LOW), PIN(HVAC_COOL_PIN, LOW), PIN(HVAC_FAN_PIN, LOW),
            PIN(LED_HEAT_PIN, LOW), PIN(LED_COOL_PIN, LOW), PIN(LED_FAN_PIN, LOW),
            PIN(HVAC_STAGE2_PIN, LOW)),
};

static inline bool is_invalid_desc(const struct gpio_pin_desc desc)
{
  struct gpio_pin_desc inv = INVALID_PIN();

  if (desc.pin == inv.pin && desc.level == inv.level)
    return true;

  return false;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

// #include <stdio.h>
// #include <time.h>
// #include <stdbool.h>

#define WINDOW_MINUTES 60
#define BUFFER_SIZE 1024 // Adjust based on expected max frequency of data points

// Structure to hold individual data points
typedef struct
{
  time_t timestamp;
  int64_t value;
} DataPoint;

// Structure for the rolling window tracker
typedef struct
{
  DataPoint buffer[BUFFER_SIZE];
  int head;
  int tail;
  int count;
  int64_t cumulative_sum;
} RollingTracker;

RollingTracker tracker;

// Initialize the tracker
void tracker_init(RollingTracker *tracker)
{
  tracker->head = 0;
  tracker->tail = 0;
  tracker->count = 0;
  tracker->cumulative_sum = 0;
}

// Internal function to remove expired data points
void evict_expired(RollingTracker *tracker, time_t current_time)
{
  time_t cutoff_time = current_time - (WINDOW_MINUTES * 60);

  while (tracker->count > 0 && tracker->buffer[tracker->tail].timestamp < cutoff_time)
  {
    tracker->cumulative_sum -= tracker->buffer[tracker->tail].value;
    tracker->tail = (tracker->tail + 1) % BUFFER_SIZE;
    tracker->count--;
  }
}

// Add a new value to the tracker
bool tracker_add_value(RollingTracker *tracker, int64_t value)
{
  time_t current_time = time(NULL);
  
  // First, clean up any data points older than 60 minutes
  evict_expired(tracker, current_time);

  // Check if buffer is full
  if (tracker->count >= BUFFER_SIZE)
  {
    printf("Error: Buffer full, data point dropped.\n");
    return false;
  }

  // Add new value
  tracker->buffer[tracker->head].timestamp = current_time;
  tracker->buffer[tracker->head].value = value;
  tracker->cumulative_sum += value;

  tracker->head = (tracker->head + 1) % BUFFER_SIZE;
  tracker->count++;

  return true;
}

// Get the current cumulative sum
int64_t tracker_get_sum(RollingTracker *tracker)
{
  evict_expired(tracker, time(NULL));
  return tracker->cumulative_sum;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////



static void set_hvac_mode(HVAC_MODE mode)
{
  const struct gpio_pin_desc *desc = hvac_mode_gpio[mode];

  for (int i = 0; i < NR_GPIO_PINS; i++)
  {
    if (is_invalid_desc(desc[i]))
      break;

    gpio_set_level((gpio_num_t)desc[i].pin, (uint32_t)desc[i].level);
  }

  // Changing modes so the fan will start running. Mark the time so we can track how long the fan is running
  if ((mode == HEAT || mode == COOL || mode == FAN_ONLY) &&
      (OperatingParameters.hvacOpMode == OFF || OperatingParameters.hvacOpMode == IDLE))
    lastFanOnTime = millis();


  // Going to turn off the FAN. Keep track of how long the fan is run
  if ((mode == OFF || mode == IDLE) &&
      (OperatingParameters.hvacOpMode == FAN_ONLY || OperatingParameters.hvacOpMode == HEAT || OperatingParameters.hvacOpMode == COOL))
    {
      tracker_add_value(&tracker, millis() - lastFanOnTime);
      ESP_LOGI(__FUNCTION__, "Fan run time: %lld ms", millis() - lastFanOnTime);
      ESP_LOGI(__FUNCTION__, "Hourly Fan run time: %.2f min", (tracker_get_sum(&tracker) / 1000.0 / 60.0));
    }

  OperatingParameters.hvacOpMode = mode;
}

#define COND_LOG(cond, log_msg, ...) ({           \
  int __ret_on_cond = !!(cond);                   \
  if (__ret_on_cond) {                            \
    ESP_LOGI(__FUNCTION__, log_msg, __VA_ARGS__); \
  }                                               \
  (__ret_on_cond);                                \
})

static inline float get_min_temp(OPERATING_PARAMETERS *params, bool auto_mode)
{
#ifdef AUTOMODE_SUPPORTED
  if (auto_mode)
    return params->tempSetAutoMin - (params->tempSwing / 2.0);
#endif
  return params->tempSet - (params->tempSwing / 2.0);
}

static inline float get_max_temp(OPERATING_PARAMETERS *params, bool auto_mode)
{
#ifdef AUTOMODE_SUPPORTED
  if (auto_mode)
    return params->tempSetAutoMax + (params->tempSwing / 2.0);
#endif
  return params->tempSet + (params->tempSwing / 2.0);
}

void hvacStateUpdate()
{
  float currentTemp;
  float minTemp, maxTemp;
  float autoMinTemp, autoMaxTemp;
  HVAC_MODE prev_mode = OperatingParameters.hvacOpMode;

  if (ModeChangeRequested && (millis() - ModeChangeRequestTime > 2000))
  {
    ESP_LOGI(__FUNCTION__, "Processing requested HVAC mode change to %s", hvacModeToString(requestedHvacMode));
    _updateHvacMode(requestedHvacMode);
    ModeChangeRequested = false;
  }

  if (TempChangeRequested && (millis() - TempChangeRequestTime > 2000))
  {
    ESP_LOGI(__FUNCTION__, "Processing requested HVAC set temperature change to %.2f", requestedHvacSetTemp);
    _updateHvacSetTemp(requestedHvacSetTemp);
    TempChangeRequested = false;
  }

  currentTemp = OperatingParameters.tempCurrent + OperatingParameters.tempCorrection;
  minTemp = get_min_temp(&OperatingParameters, false);
  maxTemp = get_max_temp(&OperatingParameters, false);
  autoMinTemp = get_min_temp(&OperatingParameters, true);
  autoMaxTemp = get_max_temp(&OperatingParameters, true);

  switch (OperatingParameters.hvacSetMode) {
  case OFF:
    set_hvac_mode(OFF);
    COND_LOG(prev_mode != OFF, "Entering off mode: Current: %.2f", currentTemp);
    break;
  case FAN_ONLY:
    set_hvac_mode(FAN_ONLY);
    COND_LOG(prev_mode != FAN_ONLY, "Entering fan only mode: Current: %.2f", currentTemp);
    break;
  case HEAT:
    if (currentTemp < minTemp)
    {
      set_hvac_mode(HEAT);
      COND_LOG(prev_mode != HEAT, "Entering heat mode: Current: %.2f  Lo Limit: %.2f", currentTemp, minTemp);
    } else {
      // Expected overshoot (where furnace continues to run to dissipate residual heat) is ~0.5F
      // Almost the same in Celcius.
      if (currentTemp > maxTemp - 0.5)
      {
        set_hvac_mode(IDLE);
        COND_LOG(prev_mode != IDLE, "Target temp reached: Stopping heat mode: Current: %.2f  Hi Limit: %.2f", currentTemp, maxTemp);
      }
    }
  break;
  case AUX_HEAT:
    //
    // Set up for 2-stage, emergency or aux heat mode
    // Set relays correct
    //
    //@@@ still todo
    //
    if (currentTemp < minTemp) {
      set_hvac_mode(HEAT);
      COND_LOG(prev_mode != HEAT, "Entering aux heat mode: Current: %.2f  Lo Limit: %.2f", currentTemp, minTemp);
    } else {
      set_hvac_mode(IDLE);
      COND_LOG(prev_mode != IDLE, "Target temp reached: Stopping aux heat mode: Current: %.2f  Hi Limit: %.2f", currentTemp, maxTemp);
    }
    break;
  case COOL:
    if (currentTemp > maxTemp) {
      set_hvac_mode(COOL);
      COND_LOG(prev_mode != COOL, "Entering cool mode: Current: %.2f  Hi Limit: %.2f", currentTemp, maxTemp);
    } else {
      set_hvac_mode(IDLE);
      COND_LOG(prev_mode != IDLE, "Target temp reached: Stopping cool mode: Current: %.2f  Lo Limit: %.2f", currentTemp, minTemp);
    }
    break;
  case AUTO:
    if (currentTemp < autoMinTemp) {
      set_hvac_mode(HEAT);
      COND_LOG(prev_mode != HEAT, "Entering auto heat mode: Current: %.2f  auto min Limit: %.2f", currentTemp, autoMinTemp);
    } else if (currentTemp > autoMaxTemp) {
      set_hvac_mode(COOL);
      COND_LOG(prev_mode != COOL, "Entering auto cool mode: Current: %.2f  auto max Limit: %.2f", currentTemp, autoMaxTemp);
    } else {
      set_hvac_mode(IDLE);
      COND_LOG(prev_mode != IDLE, "Target temp reached: Exiting auto heat/cool mode: Current: %.2f  auto min Limit: %.2f"\
          "  auto max Limit: %.2f", currentTemp, autoMinTemp, autoMaxTemp);
    }
    break;
  }
}

static inline bool wifi_reconnect_check(OPERATING_PARAMETERS *params)
{
  bool ret = ( !(params->wifiConnected) && strlen(WifiCreds.ssid) && WifiStarted() ) ||
                WifiRestartPending();
#ifdef MATTER_ENABLED
  ret = ret && !(params->MatterStarted);
#endif
return ret;
}

static inline bool is_mqtt_enabled(OPERATING_PARAMETERS *params)
{
#ifdef MQTT_ENABLED
  return params->MqttEnabled;
#else
  return false;
#endif
}

static inline bool is_mqtt_connected(OPERATING_PARAMETERS *params)
{
#ifdef MQTT_ENABLED
  return params->MqttConnected;
#else
  return false;
#endif
}

void stateMachine(void *parameter)
{
  lastTimeUpdate = millis();
  lastWifiReconnect = millis();
  tracker_init(&tracker);

  for (;;) {
    // Update sensor readings
    OperatingParameters.lightDetected = readLightSensor();

    // Update state of motion sensor
    ld2410_loop();

    // Update HVAC State machine
    hvacStateUpdate();

    // Check and Update wifi connection status
    OperatingParameters.wifiConnected = WifiConnected();
    if ((millis() > lastWifiReconnect + WIFI_CONNECT_INTERVAL) &&
        (wifi_reconnect_check(&OperatingParameters)))
    {
      lastWifiReconnect = millis();
      startReconnectTask();
    }

    if (OperatingParameters.wifiConnected) {
      if (!telnetServiceRunning())
        telnetStart();

      //
      // Call MqttConnect() once to establish the MQTT connection.
      //
      // NB: We never set MqttConnectCalled to false so it is only ever
      // called once at startup. The MQTT subsystem handles reconnects.
      //
      if (is_mqtt_enabled(&OperatingParameters) &&
          !is_mqtt_connected(&OperatingParameters) && !MqttConnectCalled) {
        MqttConnectCalled = true;
        MqttConnect();
      }
    }

    // Determine if it's time to update the SNTP sourced clock and
    // display the amount of available heap space.
    if (COND_LOG((millis() - lastTimeUpdate > UPDATE_TIME_INTERVAL),
                 ">>>> Heap size: %d", esp_get_free_heap_size())) {
      lastTimeUpdate = millis();
      updateTimeSntp();
    }

    // Pause the task again for 40ms
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

void stateCreateTask()
{
  xTaskCreate(
      stateMachine,
      "State Machine",
      8192,
      NULL,
      tskIDLE_PRIORITY, // - 1,
      NULL);
}
