#include "thermostat.hpp"
#include "logger.h"
#include <string.h>

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
  int ret;
  esp_log_level_t level;
  va_list list_copy;

  const size_t fmt_length = strlen(fmt);
  const size_t idx = fmt[0] == 0x1b /* ESC */ && fmt_length > 7 ? 7 : 0;
  const bool has_esp_log_prefix =
      fmt_length >= idx + 3 &&
      (fmt[idx] == 'E' || fmt[idx] == 'W' || fmt[idx] == 'I' || fmt[idx] == 'D' || fmt[idx] == 'V') &&
      fmt[idx + 1] == ' ' && fmt[idx + 2] == '(';

  // CHIP prints its prefix separately and passes only the bare message here.
  level = has_esp_log_prefix ? map_char_to_level(fmt[idx]) : ESP_LOG_INFO;
  ret = -1;

  if (telnet_logging_active && (level <= telnet_log_level))
  {
    va_copy(list_copy, list);
    ret = telnet_esp32_vprintf(fmt, list_copy);
    va_end(list_copy);
  }

  if (sdcard_logging_active && (level <= sdcard_log_level))
  {
    va_copy(list_copy, list);
    ret = sdcard_esp32_vprintf(fmt, list_copy);
    va_end(list_copy);
  }

  if (level <= console_log_level)
  {
    if (OrigEsplogger == NULL)
      esp_restart();
    va_copy(list_copy, list);
    ret = OrigEsplogger(fmt, list_copy);
    va_end(list_copy);
  }

  return ret;
}

void loggerEnableLocalLogging()
{
  if (OrigEsplogger == NULL)
    OrigEsplogger = esp_log_set_vprintf(localLogger);
}
