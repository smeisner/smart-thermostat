#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "esp_log.h"
#include "telnet.h"

bool enableTelnetLogging(bool enabled);
bool enableSdcardLogging(bool enabled);

void setConsoleLogLevel(esp_log_level_t level);
void setSdCardLogLevel(esp_log_level_t level);
void setTelnetLogLevel(esp_log_level_t level);

int localLogger(const char *fmt, va_list list);

