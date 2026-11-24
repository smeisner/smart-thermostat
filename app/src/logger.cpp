#include "thermostat.hpp"
#include "logger.h"
#include <string.h>

// This can be turned off and on from the telnet session with the "Monitor" command
static bool telnet_logging_active = false;
static bool sdcard_logging_active = false;

static esp_log_level_t console_log_level = ESP_LOG_INFO;
static esp_log_level_t sdcard_log_level = ESP_LOG_WARN;
static esp_log_level_t telnet_log_level = ESP_LOG_INFO;
static esp_log_level_t web_log_level = ESP_LOG_INFO;

static vprintf_like_t OrigEsplogger = NULL;
static constexpr size_t WEB_LOG_BUFFER_SIZE = 8192;
static char web_log_buffer[WEB_LOG_BUFFER_SIZE];
static uint64_t web_log_bytes_written = 0;
static portMUX_TYPE web_log_mux = portMUX_INITIALIZER_UNLOCKED;

static void appendWebLog(const char *fmt, va_list list)
{
  char line[512];
  va_list list_copy;
  va_copy(list_copy, list);
  const int formatted_size = vsnprintf(line, sizeof(line), fmt, list_copy);
  va_end(list_copy);
  if (formatted_size <= 0)
  {
    return;
  }

  const size_t size = formatted_size < sizeof(line) ? static_cast<size_t>(formatted_size) : sizeof(line) - 1;
  portENTER_CRITICAL(&web_log_mux);
  for (size_t i = 0; i < size; ++i)
  {
    web_log_buffer[web_log_bytes_written % WEB_LOG_BUFFER_SIZE] = line[i];
    ++web_log_bytes_written;
  }
  portEXIT_CRITICAL(&web_log_mux);
}

bool readWebLog(uint64_t *cursor, char *buffer, size_t buffer_size, size_t *bytes_read, bool *truncated)
{
  if (cursor == nullptr || buffer == nullptr || buffer_size == 0 || bytes_read == nullptr || truncated == nullptr)
  {
    return false;
  }

  portENTER_CRITICAL(&web_log_mux);
  const uint64_t oldest = web_log_bytes_written > WEB_LOG_BUFFER_SIZE
                              ? web_log_bytes_written - WEB_LOG_BUFFER_SIZE
                              : 0;
  uint64_t start = *cursor;
  *truncated = start < oldest;
  if (start < oldest)
  {
    start = oldest;
  }
  if (start > web_log_bytes_written)
  {
    start = web_log_bytes_written;
  }

  const uint64_t available = web_log_bytes_written - start;
  *bytes_read = available < buffer_size ? static_cast<size_t>(available) : buffer_size;
  for (size_t i = 0; i < *bytes_read; ++i)
  {
    buffer[i] = web_log_buffer[(start + i) % WEB_LOG_BUFFER_SIZE];
  }
  *cursor = start + *bytes_read;
  portEXIT_CRITICAL(&web_log_mux);
  return true;
}

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

void setWebLogLevel(esp_log_level_t level)
{
  web_log_level = level;
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

  if (level <= web_log_level)
  {
    appendWebLog(fmt, list);
  }

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
