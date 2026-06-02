#include "thermostat.hpp"
#include "logger.h"

// This can be turned off and on from the telnet session with the "Monitor" command
static bool telnet_logging_active = false;
static bool sdcard_logging_active = false;

static esp_log_level_t console_log_level = ESP_LOG_INFO;
static esp_log_level_t sdcard_log_level = ESP_LOG_WARN;
static esp_log_level_t telnet_log_level = ESP_LOG_INFO;

static vprintf_like_t OrigEsplogger = NULL;

bool enableTelnetLogging(bool enabled)
{
  telnet_logging_active = enabled;
  return telnet_logging_active;
}

bool enableSdcardLogging(bool enabled)
{
  sdcard_logging_active = enabled;
  return sdcard_logging_active;
}

void setConsoleLogLevel(esp_log_level_t level)
{
  console_log_level = level;
}

void setSdCardLogLevel(esp_log_level_t level)
{
  sdcard_log_level = level;
}

void setTelnetLogLevel(esp_log_level_t level)
{
  telnet_log_level = level;
}

esp_log_level_t map_char_to_level(char c)
{
  switch (c)
  {
    case 'E':
    case 'e':
      return ESP_LOG_ERROR;
    case 'W':
    case 'w':
      return ESP_LOG_WARN;
    case 'I':
    case 'i':
      return ESP_LOG_INFO;
    case 'D':
    case 'd':
      return ESP_LOG_DEBUG;
    default:
      return ESP_LOG_VERBOSE;
  }
}

int localLogger(const char *fmt, va_list list)
{
  static int ret;
  static int idx;
  static int level;

  idx = 0;
  if (fmt[0] == 0x1b /* ESC */) idx = 7;
  level = map_char_to_level(fmt[idx]);
  ret = -1;

  if (telnet_logging_active && (level <= telnet_log_level))
    ret = telnet_esp32_vprintf(fmt, list);

  if (sdcard_logging_active && (level <= sdcard_log_level))
    ret = sdcard_esp32_vprintf(fmt, list);

  if (level <= console_log_level)
  {
    if (OrigEsplogger == NULL)
      esp_restart();
    ret = OrigEsplogger(fmt, list);
  }

  return ret;
}

void loggerEnableLocalLogging()
{
  if (OrigEsplogger == NULL)
    OrigEsplogger = esp_log_set_vprintf(localLogger);
}
