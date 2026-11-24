// SPDX-License-Identifier: GPL-3.0-only
/*
 * web.cpp
 *
 * This module implements the web UI page. It also provide the
 * initialization of mDNS so the web page can be connected to without
 * knowing the IP address.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  18-Aug-2023: Michael Burke (michaelburke2000@gmail.com) - WebUI refresh + reorganization
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrote to support ESP-IDF framework instead of Arduino
 *  06-Oct-2023: Michael Burke (michaelburke2000@gmail.com) - Reimplement XML live updating, add more settings to webui
 *
 *
 *
 * Portions of this module and ../include/web_ui.h, and ONLY these two modules, are inspired by an example project by Kris Kasprzak.
 * https://www.youtube.com/watch?v=pL3dhGtmcMY
 * https://github.com/KrisKasprzak/ESP32_WebPage/tree/main
 * Kris's project is distributed under an MIT License.
 *  The MIT License (MIT)
  Permission is hereby granted, free of charge, to any person obtaining a copy of
  this software and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the rights to
  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
  the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <ArduinoJson.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include "thermostat.hpp"
#include "ui/ui.h"
#include "version.h"
#include "web_ui.h"
#include "logger.h"

static const char *TAG = "WEB";

// Firmware upgrade functions
bool check_github_for_updates(bool);
bool performUpdate(bool);

static char html[2200];
static char xml[800];

float doTempUp(void)
{
  if (OperatingParameters.tempUnits == 'C')
  {
    OperatingParameters.tempSet += 0.5;
    updateHvacSetTemp(roundValue(OperatingParameters.tempSet, 1));
  } else {
    OperatingParameters.tempSet += 1.0;
    updateHvacSetTemp(roundValue(OperatingParameters.tempSet, 0));
  }
  return OperatingParameters.tempSet;
}

float doTempDown(void)
{
  if (OperatingParameters.tempUnits == 'C')
  {
    OperatingParameters.tempSet -= 0.5;
    updateHvacSetTemp(roundValue(OperatingParameters.tempSet, 1));
  } else {
    OperatingParameters.tempSet -= 1.0;
    updateHvacSetTemp(roundValue(OperatingParameters.tempSet, 0));
  }
  return OperatingParameters.tempSet;
}

float enforceRange(float value, float min, float max) {
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

#define BUTTON_CONTENT_SIZE 30
bool buttonDispatch(const char *content, bool &matter_capabilities_changed)
{
  //@@@ Make this independent of Matter. Maybe change name to hvac_capabilities_changed() or something like that.
  matter_capabilities_changed = false;

  if (!strncmp(content, "tempUp", BUTTON_CONTENT_SIZE))
    doTempUp();
  else if (!strncmp(content, "tempDown", BUTTON_CONTENT_SIZE))
    doTempDown();
  else if (!strncmp(content, "hvacModeOff", BUTTON_CONTENT_SIZE))
    updateHvacMode(OFF);
  else if (!strncmp(content, "hvacModeAuto", BUTTON_CONTENT_SIZE))
    updateHvacMode(AUTO);
  else if (!strncmp(content, "hvacModeHeat", BUTTON_CONTENT_SIZE))
    updateHvacMode(HEAT);
  else if (!strncmp(content, "hvacModeCool", BUTTON_CONTENT_SIZE))
    updateHvacMode(COOL);
  else if (!strncmp(content, "hvacModeFan", BUTTON_CONTENT_SIZE))
    updateHvacMode(FAN_ONLY);
  else if (!strncmp(content, "clearNVS", BUTTON_CONTENT_SIZE))
    clearNVS();
#ifdef TELNET_ENABLED
  else if (!strncmp(content, "terminateTelnet", BUTTON_CONTENT_SIZE))
//    if (telnetServiceRunning()) //@@@
    terminateTelnetSession();
#endif
  else if (!strncmp(content, "hvacCoolEnable", BUTTON_CONTENT_SIZE))
  {
    OperatingParameters.hvacCoolEnable = !OperatingParameters.hvacCoolEnable; 
#ifdef MATTER_ENABLED
    matter_capabilities_changed = true;
#endif
    #ifdef MQTT_ENABLED
    updateEnabledHvacModes();
    #endif
  }
  else if (!strncmp(content, "hvacFanEnable", BUTTON_CONTENT_SIZE))
  {
    OperatingParameters.hvacFanEnable = !OperatingParameters.hvacFanEnable;
#ifdef MATTER_ENABLED
    matter_capabilities_changed = true;
#endif
    #ifdef MQTT_ENABLED
    updateEnabledHvacModes();
    #endif
  }
  else if (!strncmp(content, "swingUp", BUTTON_CONTENT_SIZE))
  {
    OperatingParameters.tempSwing = enforceRange(OperatingParameters.tempSwing + 0.1, 0.0, 6.0);
    eepromUpdateArbFloat("setSwing", OperatingParameters.tempSwing);
  }
  else if (!strncmp(content, "swingDown", BUTTON_CONTENT_SIZE))
  {
    OperatingParameters.tempSwing = enforceRange(OperatingParameters.tempSwing - 0.1, 0.0, 6.0);
    eepromUpdateArbFloat("setSwing", OperatingParameters.tempSwing);
  }
  else if (!strncmp(content, "correctionUp", BUTTON_CONTENT_SIZE))
  {
    OperatingParameters.tempCorrection = enforceRange(OperatingParameters.tempCorrection + 0.1, -10.0, 10.0);
    eepromUpdateArbFloat("setTempCorr", OperatingParameters.tempCorrection);
  }
  else if (!strncmp(content, "correctionDown", BUTTON_CONTENT_SIZE))
  {
    OperatingParameters.tempCorrection = enforceRange(OperatingParameters.tempCorrection - 0.1, -10.0, 10.0);
    eepromUpdateArbFloat("setTempCorr", OperatingParameters.tempCorrection);
  }
  else if (!strncmp(content, "twoStageEnable", BUTTON_CONTENT_SIZE))
  {
    OperatingParameters.hvac2StageHeatEnable = !OperatingParameters.hvac2StageHeatEnable;
#ifdef MATTER_ENABLED
    matter_capabilities_changed = true;
#endif
    #ifdef MQTT_ENABLED
    updateEnabledHvacModes();
    #endif
  }
  else if (!strncmp(content, "reverseEnable", BUTTON_CONTENT_SIZE))
  {
    OperatingParameters.hvacReverseValveEnable = !OperatingParameters.hvacReverseValveEnable;
    #ifdef MQTT_ENABLED
    updateEnabledHvacModes();
    #endif
  }
  else if (!strncmp(content, "unitToggle", BUTTON_CONTENT_SIZE))
  {
    if (OperatingParameters.tempUnits == 'F')
    {
      // OperatingParameters.tempSet = (OperatingParameters.tempSet - 32.0) / (9.0/5.0);
      // OperatingParameters.tempCurrent = (OperatingParameters.tempCurrent - 32.0) / (9.0/5.0);
      // OperatingParameters.tempCorrection = OperatingParameters.tempCorrection * 5.0 / 9.0;
      // OperatingParameters.tempSwing = OperatingParameters.tempSwing * 5.0 / 9.0;
      // resetTempSmooth();
      lv_arc_set_range(ui_TempArc, 7*10, 33*10);
      lv_obj_clear_flag(ui_SetTempFrac, LV_OBJ_FLAG_HIDDEN);
      OperatingParameters.tempUnits = 'C';
    }
    else
    {
      // OperatingParameters.tempSet = (OperatingParameters.tempSet * 9.0/5.0) + 32.0;
      // OperatingParameters.tempCurrent = (OperatingParameters.tempCurrent * 9.0/5.0) + 32.0;
      // OperatingParameters.tempCorrection = OperatingParameters.tempCorrection * 1.8;
      // OperatingParameters.tempSwing = OperatingParameters.tempSwing * 1.8;
      // resetTempSmooth();
      lv_arc_set_range(ui_TempArc, 45*10, 92*10);
      lv_obj_add_flag(ui_SetTempFrac, LV_OBJ_FLAG_HIDDEN);
      OperatingParameters.tempUnits = 'F';
    }
  }
  else
  {
    ESP_LOGE(TAG, "Could not dispatch request \"%s\"", content);
    OperatingParameters.Errors.systemErrors++;
    return false;
  }

  return true;
}

#define min(x, y) ((x < y) ? x : y)

esp_err_t handleButton(httpd_req_t *req)
{
  char content[BUTTON_CONTENT_SIZE + 1] = {};
  const size_t content_len = req->content_len;
  if (content_len == 0 || content_len > BUTTON_CONTENT_SIZE)
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid button command size");
  }

  size_t received = 0;
  while (received < content_len)
  {
    const int recv_len = httpd_req_recv(req, content + received, content_len - received);
    if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
    {
      continue;
    }
    if (recv_len <= 0)
    {
      ESP_LOGW(TAG, "Failed to receive complete web button command");
      return ESP_FAIL;
    }
    received += static_cast<size_t>(recv_len);
  }

  bool matter_capabilities_changed = false;
  if (!buttonDispatch(content, matter_capabilities_changed))
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown button command");
  }

  httpd_resp_set_type(req, "text/plain");
  //@@@ Make this independent of Matter. Maybe change name to hvac_capabilities_changed() or something like that.
#ifdef MATTER_ENABLED
  if (matter_capabilities_changed)
  {
    updateThermostatParams();
  }
#endif
  const esp_err_t response_err = httpd_resp_send(req, 0, 0);
#ifdef MATTER_ENABLED
  if (matter_capabilities_changed && OperatingParameters.MatterEnabled &&
      OperatingParameters.MatterStarted)
  {
    ESP_LOGW(TAG, "HVAC capabilities changed through web UI; resetting Matter so Home Assistant can re-pair");
    MatterFactoryReset();
  }
#endif
  return response_err;
}

#define CAT_IF_SPACE(xmlBuffer, other, space, request)          \
  if (space < 0) {                                              \
    ESP_LOGE(TAG, "XML buffer ran out of space. Aborting...");  \
    OperatingParameters.Errors.systemErrors++;                  \
    return httpd_resp_send_500(request);                        \
  }                                                             \
  strcat(xmlBuffer, other);

esp_err_t handleXML(httpd_req_t *req)
{
  char buf[128];
  ssize_t xmlSpace = sizeof(xml) - 1;
  httpd_resp_set_type(req, "text/xml");
  xmlSpace -= snprintf(xml, xmlSpace, "<?xml version = '1.0'?><Data>\n");
  if (xmlSpace < 0)
    return httpd_resp_send_500(req);
  if (OperatingParameters.tempUnits == 'F')
    xmlSpace -= snprintf(buf, sizeof(buf), "<curTemp>%.1f</curTemp>\n", OperatingParameters.tempCurrent + OperatingParameters.tempCorrection);
  else
    xmlSpace -= snprintf(buf, sizeof(buf), "<curTemp>%.1f</curTemp>\n", ftoc(OperatingParameters.tempCurrent + OperatingParameters.tempCorrection));
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  if (OperatingParameters.tempUnits == 'F')
    xmlSpace -= snprintf(buf, sizeof(buf), "<setTemp>%.0f</setTemp>\n", OperatingParameters.tempSet);
  else
    xmlSpace -= snprintf(buf, sizeof(buf), "<setTemp>%.1f</setTemp>\n", ftoc(OperatingParameters.tempSet));
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<curMode>%s</curMode>\n", hvacModeToString(OperatingParameters.hvacOpMode));
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<setMode>%s</setMode>\n", hvacModeToString(OperatingParameters.hvacSetMode));
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<humidity>%.1f</humidity>\n", OperatingParameters.humidCurrent + OperatingParameters.humidityCorrection);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<light>%d</light>\n", OperatingParameters.lightDetected);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<motion>%s</motion>\n", OperatingParameters.motionDetected ? "True" : "False");
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<units>%c</units>\n", OperatingParameters.tempUnits);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<swing>%.1f</swing>\n", OperatingParameters.tempSwing);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<correction>%.1f</correction>\n", OperatingParameters.tempCorrection);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<wifiStrength>%d</wifiStrength>\n", WifiSignal());
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<address>%s</address>\n", WifiAddress());
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<firmwareVer>%s</firmwareVer>\n", VersionString);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<firmwareDt>%s</firmwareDt>\n", VersionBuildDateTime);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<copyright>%s</copyright>\n", VersionCopyright);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<hvacCoolEnable>%d</hvacCoolEnable>\n", OperatingParameters.hvacCoolEnable);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<hvacFanEnable>%d</hvacFanEnable>\n", OperatingParameters.hvacFanEnable);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<twoStageEnable>%d</twoStageEnable>\n", OperatingParameters.hvac2StageHeatEnable);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  xmlSpace -= snprintf(buf, sizeof(buf), "<reverseEnable>%d</reverseEnable>\n", OperatingParameters.hvacReverseValveEnable);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  CAT_IF_SPACE(xml, "</Data>", xmlSpace, req);
  // ESP_LOGD(TAG, "Remaining XML space: %ld", xmlSpace);
  return httpd_resp_send(req, xml, strlen(xml));
}

esp_err_t handleRoot(httpd_req_t *req)
{
  return httpd_resp_send(req, webUI, HTTPD_RESP_USE_STRLEN);
}

esp_err_t fwUpload(httpd_req_t *req)
{
  return httpd_resp_send(req, web_fw_upload, strlen(web_fw_upload));
}

esp_err_t webLogPage(httpd_req_t *req)
{
  return httpd_resp_send(req, webLogUI, HTTPD_RESP_USE_STRLEN);
}

esp_err_t webStatus(httpd_req_t *req)
{
  const size_t query_len = httpd_req_get_url_query_len(req);
  if (query_len == 0)
  {
    return httpd_resp_send(req, webStatusUI, HTTPD_RESP_USE_STRLEN);
  }

  char query[32];
  char state[8];
  if (query_len >= sizeof(query) ||
      httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "state", state, sizeof(state)) != ESP_OK ||
      (strcmp(state, "static") != 0 && strcmp(state, "live") != 0))
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid status request");
  }

  const bool static_request = strcmp(state, "static") == 0;
  JsonDocument status;
  if (static_request)
  {
    status["firmwareVersion"] = VersionString;
    status["firmwareBuildDate"] = VersionBuildDateTime;
    status["deviceName"] = OperatingParameters.DeviceName;
    status["friendlyName"] = OperatingParameters.FriendlyName;

    char mac_address[18];
    snprintf(mac_address, sizeof(mac_address), "%02x:%02x:%02x:%02x:%02x:%02x",
             OperatingParameters.mac[0], OperatingParameters.mac[1], OperatingParameters.mac[2],
             OperatingParameters.mac[3], OperatingParameters.mac[4], OperatingParameters.mac[5]);
    status["macAddress"] = mac_address;
    status["wifiSsid"] = WifiCreds.ssid;
    status["timezone"] = OperatingParameters.timezone != nullptr ? OperatingParameters.timezone : "Unknown";
    status["ld2410Firmware"] = OperatingParameters.ld2410FirmWare;
    status["sdCardPresent"] = is_sd_card_present();
    status["displaySleepTime"] = OperatingParameters.thermostatSleepTime;
    status["touchscreenBeep"] = OperatingParameters.thermostatBeepEnable;

    const bool celsius = OperatingParameters.tempUnits == 'C';
    status["tempUnits"] = celsius ? "C" : "F";
    status["tempCorrection"] = OperatingParameters.tempCorrection;
    status["humidityCorrection"] = OperatingParameters.humidityCorrection;
    status["tempSwing"] = OperatingParameters.tempSwing;
    status["hvacCoolEnabled"] = OperatingParameters.hvacCoolEnable;
    status["hvacFanEnabled"] = OperatingParameters.hvacFanEnable;
    status["twoStageHeatEnabled"] = OperatingParameters.hvac2StageHeatEnable;
    status["twoStageCoolEnabled"] = OperatingParameters.hvac2StageCoolEnable;
    status["reverseValveEnabled"] = OperatingParameters.hvacReverseValveEnable;

#ifdef MQTT_ENABLED
    status["hasMqtt"] = true;
    status["mqttEnabled"] = OperatingParameters.MqttEnabled;
    status["mqttBroker"] = OperatingParameters.MqttBrokerHost;
    status["mqttPort"] = OperatingParameters.MqttBrokerPort;
    status["mqttUsername"] = OperatingParameters.MqttBrokerUsername;
    status["mqttPasswordConfigured"] = OperatingParameters.MqttBrokerPassword[0] != '\0';
#else
    status["hasMqtt"] = false;
#endif

#ifdef MATTER_ENABLED
    status["hasMatter"] = true;
    status["matterEnabled"] = OperatingParameters.MatterEnabled;
#else
    status["hasMatter"] = false;
#endif

#ifdef TELNET_ENABLED
    status["hasTelnet"] = true;
#else
    status["hasTelnet"] = false;
#endif
  }
  else
  {
    const uint64_t uptime = static_cast<uint64_t>(millis());
    char uptime_text[48];
    snprintf(uptime_text, sizeof(uptime_text), "%llu days, %02u:%02u:%02u",
             static_cast<unsigned long long>(uptime / (1000ULL * 60ULL * 60ULL * 24ULL)),
             static_cast<unsigned>((uptime / (1000ULL * 60ULL * 60ULL)) % 24ULL),
             static_cast<unsigned>((uptime / (1000ULL * 60ULL)) % 60ULL),
             static_cast<unsigned>((uptime / 1000ULL) % 60ULL));
    status["uptime"] = uptime_text;

    const uint32_t free_heap_bytes = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    const uint32_t total_heap_bytes = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    status["freeHeapBytes"] = esp_get_free_heap_size();
    status["heapFreeBytes"] = free_heap_bytes;
    status["heapTotalBytes"] = total_heap_bytes;
    status["heapFreePercent"] =
        total_heap_bytes == 0 ? 0.0f : free_heap_bytes * 100.0f / static_cast<float>(total_heap_bytes);

    status["wifiConnected"] = OperatingParameters.wifiConnected;
    status["wifiSignal"] = WifiSignal();
    status["wifiIpAddress"] = WifiAddress();

    char current_time[16] = "Unavailable";
    struct tm local_time;
    if (getLocalTime(&local_time, 1))
    {
      strftime(current_time, sizeof(current_time), "%H:%M:%S", &local_time);
    }
    status["currentTime"] = current_time;
    status["lightDetected"] = OperatingParameters.lightDetected;
    status["motionDetected"] = OperatingParameters.motionDetected;

    const bool celsius = OperatingParameters.tempUnits == 'C';
    status["currentTemp"] = celsius
                                ? ftoc(OperatingParameters.tempCurrent + OperatingParameters.tempCorrection)
                                : OperatingParameters.tempCurrent + OperatingParameters.tempCorrection;
    status["currentHumidity"] = OperatingParameters.humidCurrent + OperatingParameters.humidityCorrection;
    status["targetTemp"] = celsius ? ftoc(OperatingParameters.tempSet) : OperatingParameters.tempSet;
    status["hvacSetMode"] = hvacModeToString(OperatingParameters.hvacSetMode);
    status["hvacCurrentMode"] = hvacModeToString(OperatingParameters.hvacOpMode);

#ifdef MQTT_ENABLED
    status["mqttConnected"] = OperatingParameters.MqttConnected;
#endif
#ifdef MATTER_ENABLED
    status["matterStarted"] = OperatingParameters.MatterStarted;
#endif

    status["systemErrors"] = OperatingParameters.Errors.systemErrors;
    status["hardwareErrors"] = OperatingParameters.Errors.hardwareErrors;
    status["wifiErrors"] = OperatingParameters.Errors.wifiErrors;
#ifdef MQTT_ENABLED
    status["mqttConnectErrors"] = OperatingParameters.Errors.mqttConnectErrors;
    status["mqttProtocolErrors"] = OperatingParameters.Errors.mqttProtocolErrors;
#endif
#ifdef MATTER_ENABLED
    status["matterConnectErrors"] = OperatingParameters.Errors.matterConnectErrors;
#endif
#ifdef TELNET_ENABLED
    status["telnetNetworkErrors"] = OperatingParameters.Errors.telnetNetworkErrors;
#endif
  }

  char response[2048];
  const size_t response_size = measureJson(status);
  if (response_size >= sizeof(response))
  {
    ESP_LOGE(TAG, "Web status response exceeds buffer capacity");
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Status response is too large");
  }

  serializeJson(status, response, sizeof(response));
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, response, response_size);
}

esp_err_t webLogData(httpd_req_t *req)
{
  uint64_t cursor = 0;
  const size_t query_len = httpd_req_get_url_query_len(req);
  if (query_len > 0)
  {
    char query[64];
    if (query_len >= sizeof(query) ||
        httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
    {
      return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid log cursor");
    }

    char cursor_text[24];
    if (httpd_query_key_value(query, "since", cursor_text, sizeof(cursor_text)) != ESP_OK ||
        cursor_text[0] == '-')
    {
      return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid log cursor");
    }

    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed_cursor = strtoull(cursor_text, &end, 10);
    if (errno == ERANGE || end == cursor_text || *end != '\0')
    {
      return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid log cursor");
    }
    cursor = static_cast<uint64_t>(parsed_cursor);
  }

  char log_data[1024];
  constexpr char kTruncatedMessage[] = "[Older log output was discarded]\n";
  constexpr size_t kTruncatedMessageSize = sizeof(kTruncatedMessage) - 1;
  size_t bytes_read = 0;
  bool truncated = false;
  if (!readWebLog(&cursor, log_data + kTruncatedMessageSize,
                  sizeof(log_data) - kTruncatedMessageSize, &bytes_read, &truncated))
  {
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to read log buffer");
  }

  if (truncated)
  {
    memcpy(log_data, kTruncatedMessage, kTruncatedMessageSize);
    bytes_read += kTruncatedMessageSize;
  }
  else
  {
    memmove(log_data, log_data + kTruncatedMessageSize, bytes_read);
  }

  char cursor_header[24];
  snprintf(cursor_header, sizeof(cursor_header), "%llu", static_cast<unsigned long long>(cursor));
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "X-Log-Next", cursor_header);
  return httpd_resp_send(req, log_data, bytes_read);
}

static bool copyConfigString(JsonVariantConst value, char *destination, size_t destination_size)
{
  if (!value.is<const char *>() || destination_size == 0)
  {
    return false;
  }

  const char *source = value.as<const char *>();
  const size_t length = strnlen(source, destination_size);
  if (length >= destination_size)
  {
    return false;
  }

  memcpy(destination, source, length + 1);
  return true;
}

esp_err_t webConfiguration(httpd_req_t *req)
{
  const size_t query_len = httpd_req_get_url_query_len(req);
  if (query_len == 0)
  {
    return httpd_resp_send(req, webConfigUI, HTTPD_RESP_USE_STRLEN);
  }

  char query[32];
  char state[8];
  if (query_len >= sizeof(query) ||
      httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "state", state, sizeof(state)) != ESP_OK ||
      strcmp(state, "1") != 0)
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid configuration request");
  }

  JsonDocument config;
  config["deviceName"] = OperatingParameters.DeviceName;
  config["friendlyName"] = OperatingParameters.FriendlyName;
  config["ssid"] = WifiCreds.ssid;
  config["tempUnits"] = OperatingParameters.tempUnits == 'C' ? "C" : "F";
  config["tempSwing"] = OperatingParameters.tempSwing;
  config["tempCorrection"] = OperatingParameters.tempCorrection;
  config["humidityCorrection"] = OperatingParameters.humidityCorrection;
  config["sleepTime"] = OperatingParameters.thermostatSleepTime;
  config["timezone"] = OperatingParameters.timezone_sel;
  config["beep"] = OperatingParameters.thermostatBeepEnable;
  config["hvacCoolEnable"] = OperatingParameters.hvacCoolEnable;
  config["hvacFanEnable"] = OperatingParameters.hvacFanEnable;
  config["twoStageEnable"] = OperatingParameters.hvac2StageHeatEnable;
  config["twoStageCoolEnable"] = OperatingParameters.hvac2StageCoolEnable;
  config["reverseEnable"] = OperatingParameters.hvacReverseValveEnable;
#ifdef MATTER_ENABLED
  config["hasMatter"] = true;
  config["matterEnabled"] = OperatingParameters.MatterEnabled;
#else
  config["hasMatter"] = false;
#endif
#ifdef MQTT_ENABLED
  config["hasMqtt"] = true;
  config["mqttEnabled"] = OperatingParameters.MqttEnabled;
  config["mqttHost"] = OperatingParameters.MqttBrokerHost;
  config["mqttPort"] = OperatingParameters.MqttBrokerPort;
  config["mqttUsername"] = OperatingParameters.MqttBrokerUsername;
#else
  config["hasMqtt"] = false;
#endif

  char response[1024];
  const size_t response_size = measureJson(config);
  if (response_size >= sizeof(response))
  {
    ESP_LOGE(TAG, "Web configuration response exceeds buffer capacity");
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Configuration response is too large");
  }

  serializeJson(config, response, sizeof(response));
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, response, response_size);
}

esp_err_t saveWebConfiguration(httpd_req_t *req)
{
  constexpr size_t kMaxRequestSize = 1024;
  char request_body[kMaxRequestSize + 1] = {};
  const size_t request_size = req->content_len;
  if (request_size == 0 || request_size > kMaxRequestSize)
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid configuration size");
  }

  size_t received = 0;
  while (received < request_size)
  {
    const int recv_len = httpd_req_recv(req, request_body + received, request_size - received);
    if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
    {
      continue;
    }
    if (recv_len <= 0)
    {
      ESP_LOGW(TAG, "Failed to receive complete web configuration");
      return ESP_FAIL;
    }
    received += static_cast<size_t>(recv_len);
  }

  JsonDocument config;
  const DeserializationError parse_error = deserializeJson(config, request_body, received);
  if (parse_error)
  {
    ESP_LOGW(TAG, "Invalid web configuration JSON: %s", parse_error.c_str());
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid configuration data");
  }

  OPERATING_PARAMETERS updated_parameters = OperatingParameters;
  WIFI_CREDS updated_wifi_creds = WifiCreds;
  char submitted_wifi_password[sizeof(updated_wifi_creds.password)] = {};
  if (!copyConfigString(config["deviceName"], updated_parameters.DeviceName, sizeof(updated_parameters.DeviceName)) ||
      !copyConfigString(config["friendlyName"], updated_parameters.FriendlyName, sizeof(updated_parameters.FriendlyName)) ||
      !copyConfigString(config["ssid"], updated_wifi_creds.ssid, sizeof(updated_wifi_creds.ssid)) ||
      !copyConfigString(config["wifiPassword"], submitted_wifi_password, sizeof(submitted_wifi_password)) ||
      !config["clearWifiPassword"].is<bool>() ||
      !config["tempUnits"].is<const char *>() ||
      updated_parameters.DeviceName[0] == '\0' || updated_parameters.FriendlyName[0] == '\0' ||
      !config["tempSwing"].is<float>() || !config["tempCorrection"].is<float>() ||
      !config["humidityCorrection"].is<float>() || !config["sleepTime"].is<int>() ||
      !config["timezone"].is<int>() || !config["beep"].is<bool>() ||
      !config["hvacCoolEnable"].is<bool>() || !config["hvacFanEnable"].is<bool>() ||
      !config["twoStageEnable"].is<bool>() || !config["twoStageCoolEnable"].is<bool>() ||
      !config["reverseEnable"].is<bool>())
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid configuration field");
  }

  const char *temp_units = config["tempUnits"].as<const char *>();
  if (strcmp(temp_units, "F") != 0 && strcmp(temp_units, "C") != 0)
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid temperature units");
  }

  const float temp_swing = config["tempSwing"].as<float>();
  const float temp_correction = config["tempCorrection"].as<float>();
  const float humidity_correction = config["humidityCorrection"].as<float>();
  const int sleep_time = config["sleepTime"].as<int>();
  const int timezone = config["timezone"].as<int>();
  if (!isfinite(temp_swing) || temp_swing < 0.0f || temp_swing > 6.0f ||
      !isfinite(temp_correction) || temp_correction < -10.0f || temp_correction > 10.0f ||
      !isfinite(humidity_correction) || humidity_correction < -100.0f || humidity_correction > 100.0f ||
      sleep_time < 0 || sleep_time > UINT16_MAX || timezone < 0 || timezone >= 24)
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Configuration value out of range");
  }

  updated_parameters.tempSwing = temp_swing;
  updated_parameters.tempCorrection = temp_correction;
  updated_parameters.humidityCorrection = humidity_correction;
  updated_parameters.thermostatSleepTime = static_cast<uint16_t>(sleep_time);
  updated_parameters.timezone_sel = static_cast<uint16_t>(timezone);
  updated_parameters.timezone = (char *)gmt_timezones[timezone];
  updated_parameters.tempUnits = temp_units[0];
  updated_parameters.thermostatBeepEnable = config["beep"].as<bool>();
  updated_parameters.hvacCoolEnable = config["hvacCoolEnable"].as<bool>();
  updated_parameters.hvacFanEnable = config["hvacFanEnable"].as<bool>();
  updated_parameters.hvac2StageHeatEnable = config["twoStageEnable"].as<bool>();
  updated_parameters.hvac2StageCoolEnable = config["twoStageCoolEnable"].as<bool>();
  updated_parameters.hvacReverseValveEnable = config["reverseEnable"].as<bool>();
  if (config["clearWifiPassword"].as<bool>())
  {
    updated_wifi_creds.password[0] = '\0';
  }
  else if (submitted_wifi_password[0] != '\0')
  {
    memcpy(updated_wifi_creds.password, submitted_wifi_password, sizeof(submitted_wifi_password));
  }

#ifdef MATTER_ENABLED
  if (!config["matterEnabled"].is<bool>())
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing Matter setting");
  }
  updated_parameters.MatterEnabled = config["matterEnabled"].as<bool>();
#endif

#ifdef MQTT_ENABLED
  if (!config["mqttEnabled"].is<bool>() || !config["mqttPort"].is<int>() ||
      !copyConfigString(config["mqttHost"], updated_parameters.MqttBrokerHost,
                        sizeof(updated_parameters.MqttBrokerHost)) ||
      !copyConfigString(config["mqttUsername"], updated_parameters.MqttBrokerUsername,
                        sizeof(updated_parameters.MqttBrokerUsername)) ||
      !config["mqttPassword"].is<const char *>() ||
      !config["clearMqttPassword"].is<bool>())
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid MQTT setting");
  }
  char submitted_mqtt_password[sizeof(updated_parameters.MqttBrokerPassword)] = {};
  if (!copyConfigString(config["mqttPassword"], submitted_mqtt_password, sizeof(submitted_mqtt_password)))
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid MQTT password");
  }
  const int mqtt_port = config["mqttPort"].as<int>();
  if (mqtt_port < 0 || mqtt_port > UINT16_MAX ||
      (config["mqttEnabled"].as<bool>() && mqtt_port == 0))
  {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "MQTT port out of range");
  }
  updated_parameters.MqttEnabled = config["mqttEnabled"].as<bool>();
  updated_parameters.MqttBrokerPort = static_cast<uint16_t>(mqtt_port);
  if (config["clearMqttPassword"].as<bool>())
  {
    updated_parameters.MqttBrokerPassword[0] = '\0';
  }
  else if (submitted_mqtt_password[0] != '\0')
  {
    memcpy(updated_parameters.MqttBrokerPassword, submitted_mqtt_password, sizeof(submitted_mqtt_password));
  }
#endif

#ifdef MATTER_ENABLED
  const bool matter_was_started = OperatingParameters.MatterStarted;
  const bool matter_capabilities_changed =
      OperatingParameters.hvacCoolEnable != updated_parameters.hvacCoolEnable ||
      OperatingParameters.hvacFanEnable != updated_parameters.hvacFanEnable ||
      OperatingParameters.hvac2StageHeatEnable != updated_parameters.hvac2StageHeatEnable ||
      OperatingParameters.hvac2StageCoolEnable != updated_parameters.hvac2StageCoolEnable;
#endif
  OperatingParameters = updated_parameters;
  WifiCreds = updated_wifi_creds;
  const bool wifi_saved = setWifiCreds();
  const bool parameters_saved = updateThermostatParams();
  if (!wifi_saved || !parameters_saved)
  {
    ESP_LOGE(TAG, "Failed to save web configuration to NVRAM");
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "Failed to save configuration to NVRAM; some settings may have been written");
  }

  ESP_LOGI(TAG, "Web configuration saved to NVRAM");
  httpd_resp_set_type(req, "text/plain");
  const esp_err_t response_err =
      httpd_resp_sendstr(req, "Settings saved to NVRAM. Restart the thermostat to apply the changes.");
#ifdef MATTER_ENABLED
  if (matter_capabilities_changed && matter_was_started)
  {
    ESP_LOGW(TAG, "HVAC capabilities changed through web configuration; resetting Matter so Home Assistant can re-pair");
    MatterFactoryReset();
  }
#endif
  return response_err;
}

esp_err_t GithubFirmwareUpgrade(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/plain");

  ESP_LOGI(TAG, "Checking Github for firmware upgrade...");
  if (check_github_for_updates(false))
  {
    ESP_LOGI(TAG, "Firmware upgrade available. Starting upgrade process...");
    const esp_err_t response_err =
        httpd_resp_sendstr(req, "Firmware upgrade available. Starting update; the thermostat will restart.");
    if (response_err != ESP_OK)
    {
      ESP_LOGW(TAG, "Could not send firmware upgrade status to the web client: %s",
               esp_err_to_name(response_err));
    }

    performUpdate(false);
    return response_err;
  }

  ESP_LOGI(TAG, "No firmware upgrade available.");
  return httpd_resp_sendstr(req, "No update found, or the update check failed. See the device log for details.");
}

esp_err_t rebootThermostat(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/plain");
  const esp_err_t response_err = httpd_resp_sendstr(req, "Thermostat is restarting.");
  if (response_err != ESP_OK)
  {
    ESP_LOGW(TAG, "Could not send reboot status to the web client: %s", esp_err_to_name(response_err));
  }

  ESP_LOGI(TAG, "Reboot requested through web UI");
  vTaskDelay(1500 / portTICK_PERIOD_MS);
  esp_restart();
  return response_err;
}


/*
 * Handle OTA file upload
 *
 * From: https://github.com/Jeija/esp32-softap-ota/blob/master/main/main.c
 */
esp_err_t fwUpdate(httpd_req_t *req)
{
	char ota_data[1000];
	esp_ota_handle_t ota_handle;
	int remaining = req->content_len;

	const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
  ESP_LOGI (TAG, "Writing to partition subtype %d at offset 0x%lx",
            ota_partition->subtype, ota_partition->address);
	ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

  ESP_LOGI (TAG, "Firmware size: %i", remaining);

	while (remaining > 0)
  {
		int recv_len = httpd_req_recv(req, ota_data, min(remaining, sizeof(ota_data)));

		// Timeout Error: Just retry
		if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
			continue;

		// Serious Error: Abort OTA
		} else if (recv_len <= 0) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
			return ESP_FAIL;
		}

		// Successful Upload: Flash firmware chunk
		if (esp_ota_write(ota_handle, (const void *)ota_data, recv_len) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
			return ESP_FAIL;
		}

		remaining -= recv_len;
	}

	// Validate and switch to new OTA image and reboot
	if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
			return ESP_FAIL;
	}

	httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");

	vTaskDelay(1500 / portTICK_PERIOD_MS);
	esp_restart();

	return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handleRoot,
    .user_ctx = NULL};
httpd_uri_t uri_xml = {
    .uri = "/xml",
    .method = HTTP_PUT,
    .handler = handleXML,
    .user_ctx = NULL};
httpd_uri_t uri_button = {
    .uri = "/button",
    .method = HTTP_PUT,
    .handler = handleButton,
    .user_ctx = NULL};
httpd_uri_t uri_upload = {
    .uri = "/upload",
    .method = HTTP_GET,
    .handler = fwUpload,
    .user_ctx = NULL};
httpd_uri_t uri_logs = {
    .uri = "/logs",
    .method = HTTP_GET,
    .handler = webLogPage,
    .user_ctx = NULL};
httpd_uri_t uri_status = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = webStatus,
    .user_ctx = NULL};
httpd_uri_t uri_config = {
    .uri = "/config",
    .method = HTTP_GET,
    .handler = webConfiguration,
    .user_ctx = NULL};
httpd_uri_t uri_config_save = {
    .uri = "/config",
    .method = HTTP_POST,
    .handler = saveWebConfiguration,
    .user_ctx = NULL};
httpd_uri_t uri_log_data = {
    .uri = "/logdata",
    .method = HTTP_GET,
    .handler = webLogData,
    .user_ctx = NULL};
httpd_uri_t uri_update = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = fwUpdate,
    .user_ctx = NULL};
httpd_uri_t uri_gh_upgrade = {
    .uri = "/gh_upgrade",
    .method = HTTP_POST,
    .handler = GithubFirmwareUpgrade,
    .user_ctx = NULL};
httpd_uri_t uri_reboot = {
    .uri = "/reboot",
    .method = HTTP_POST,
    .handler = rebootThermostat,
    .user_ctx = NULL};

void webStart()
{
  if (!WifiConnected())
  {
    ESP_LOGW (TAG, "***  WARNING: Starting web server while wifi down!!  *****");
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 12;
  config.stack_size = 8192;
  httpd_handle_t server = NULL;

  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_xml);
    httpd_register_uri_handler(server, &uri_button);
    httpd_register_uri_handler(server, &uri_upload);
    httpd_register_uri_handler(server, &uri_status);
    httpd_register_uri_handler(server, &uri_config);
    httpd_register_uri_handler(server, &uri_config_save);
    httpd_register_uri_handler(server, &uri_logs);
    httpd_register_uri_handler(server, &uri_log_data);
    httpd_register_uri_handler(server, &uri_update);
    httpd_register_uri_handler(server, &uri_gh_upgrade);
    httpd_register_uri_handler(server, &uri_reboot);
 }

  if (server == NULL)
  {
    ESP_LOGE (TAG, "** FAILED TO START WEB SERVER **");
    OperatingParameters.Errors.systemErrors++;
  }
}
