// SPDX-License-Identifier: GPL-3.0-only
/*
 * indicators.cpp
 *
 * Code to support all thermostat indicators, including: piezo buzzer,
 * RGB LED. The GPIO pins that drive the HVAC relays are also initiazlied here.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 *
 * Notes:
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 *  16-Oct-2023: Steve Meisner (steve@meisners.net) - Init relay pins for output
 *
 */

#include "thermostat.hpp"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO (BUZZER_PIN) // Define the output GPIO
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 8 bits
#define LEDC_DUTY (4095)                // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY (5000)           // Frequency in Hertz. Set frequency at 5 kHz

static bool audioChanInitialized = false;

void audioBuzzerInit()
{
  if (audioChanInitialized)
    return;

  // Prepare and then apply the LEDC PWM timer configuration
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_MODE,
      .duty_resolution = LEDC_DUTY_RES,
      .timer_num = LEDC_TIMER,
      .freq_hz = LEDC_FREQUENCY, // Set output frequency at 5 kHz
      .clk_cfg = LEDC_AUTO_CLK,
      .deconfigure = false};
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  // Prepare and then apply the LEDC PWM channel configuration
  ledc_channel_config_t ledc_channel = {
      .gpio_num = LEDC_OUTPUT_IO,
      .speed_mode = LEDC_MODE,
      .channel = LEDC_CHANNEL,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER,
      .duty = 0, // Set duty to 0%
      .hpoint = 0,
      .flags = 0};
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  audioChanInitialized = true;
}

void audioStartupBeep()
{
  audioBuzzerInit();

  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 5000));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

  ESP_ERROR_CHECK(ledc_set_freq(LEDC_MODE, LEDC_TIMER, 4000));
  vTaskDelay(pdMS_TO_TICKS(125));
  ESP_ERROR_CHECK(ledc_set_freq(LEDC_MODE, LEDC_TIMER, 2500));
  vTaskDelay(pdMS_TO_TICKS(150));

  ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

// Use lastBeep to rate limit the beeping
uint64_t lastBeep = 0;

void audioBeep()
{
  if (OperatingParameters.thermostatBeepEnable)
  {
    if (millis() - lastBeep > 125) // 1/8 of a sec
    {
      audioBuzzerInit();

      ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 5000));
      ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

      ESP_ERROR_CHECK(ledc_set_freq(LEDC_MODE, LEDC_TIMER, 4000));
      vTaskDelay(pdMS_TO_TICKS(125));

      ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
      ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

      lastBeep = millis();
    }
  }
}

// void pinMode(int _pin, gpio_mode_t mode)
// {
//   // gpio_config_t pin = (gpio_config_t)_pin;

//   //zero-initialize the config structure.
//   gpio_config_t io_conf = {};
//   //disable interrupt
//   io_conf.intr_type = GPIO_INTR_DISABLE;
//   //set as output mode
//   io_conf.mode = mode;
//   //bit mask of the pins that you want to set,e.g.GPIO18/19
//   io_conf.pin_bit_mask = (1ULL<<_pin);
//   //configure GPIO with the given settings
//   gpio_config(&io_conf);
// }

void initRelays()
{
  vTaskDelay(pdMS_TO_TICKS(250));

  gpio_set_direction((gpio_num_t)HVAC_HEAT_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)HVAC_HEAT_PIN, LOW);
  // gpio_reset_pin((gpio_num_t)HVAC_HEAT_PIN);

  gpio_set_direction((gpio_num_t)HVAC_COOL_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)HVAC_COOL_PIN, LOW);
  // gpio_reset_pin((gpio_num_t)HVAC_COOL_PIN);

  gpio_set_direction((gpio_num_t)HVAC_FAN_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)HVAC_FAN_PIN, LOW);
  // gpio_reset_pin((gpio_num_t)HVAC_FAN_PIN);

  gpio_set_direction((gpio_num_t)HVAC_RVALV_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)HVAC_RVALV_PIN, LOW);
  // gpio_reset_pin((gpio_num_t)HVAC_RVALV_PIN);

  gpio_set_direction((gpio_num_t)HVAC_STAGE2_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)HVAC_STAGE2_PIN, LOW);
  // gpio_reset_pin((gpio_num_t)HVAC_STAGE2_PIN);

}


void indicatorsInit()
{
  gpio_reset_pin((gpio_num_t)LED_HEAT_PIN);
  gpio_reset_pin((gpio_num_t)LED_COOL_PIN);
  gpio_reset_pin((gpio_num_t)LED_FAN_PIN);

  gpio_set_direction((gpio_num_t)LED_HEAT_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)LED_COOL_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)LED_FAN_PIN, GPIO_MODE_OUTPUT);

  // Change LED startup dance to be Blue, Green, Red
  // since that's their physical order on the PCB.
  gpio_set_level((gpio_num_t)LED_COOL_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(750));
  gpio_set_level((gpio_num_t)LED_COOL_PIN, LOW);
  gpio_set_level((gpio_num_t)LED_FAN_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(750));
  gpio_set_level((gpio_num_t)LED_FAN_PIN, LOW);
  gpio_set_level((gpio_num_t)LED_HEAT_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(750));
  gpio_set_level((gpio_num_t)LED_HEAT_PIN, LOW);
}
