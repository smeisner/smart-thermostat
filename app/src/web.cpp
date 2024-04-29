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
#include "thermostat.hpp"
#include "ui/ui.h"
#include "version.h"
#include "web_ui.h"

static const char *TAG = "WEB";

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
void buttonDispatch(char content[BUTTON_CONTENT_SIZE])
{
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
  else if (!strncmp(content, "clear", BUTTON_CONTENT_SIZE))
    clearNVS();
#ifdef TELNET_ENABLED
  else if (!strncmp(content, "terminateTelnet", BUTTON_CONTENT_SIZE))
//    if (telnetServiceRunning()) //@@@
    terminateTelnetSession();
#endif
  else if (!strncmp(content, "hvacCoolEnable", BUTTON_CONTENT_SIZE))
    OperatingParameters.hvacCoolEnable = !OperatingParameters.hvacCoolEnable;
  else if (!strncmp(content, "hvacFanEnable", BUTTON_CONTENT_SIZE))
    OperatingParameters.hvacFanEnable = !OperatingParameters.hvacFanEnable;
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
    OperatingParameters.hvac2StageHeatEnable = !OperatingParameters.hvac2StageHeatEnable;
  else if (!strncmp(content, "reverseEnable", BUTTON_CONTENT_SIZE))
    OperatingParameters.hvacReverseValveEnable = !OperatingParameters.hvacReverseValveEnable;
  else if (!strncmp(content, "unitToggle", BUTTON_CONTENT_SIZE)) {
    if (OperatingParameters.tempUnits == 'F') {
      OperatingParameters.tempSet = (OperatingParameters.tempSet - 32.0) / (9.0/5.0);
      OperatingParameters.tempCurrent = (OperatingParameters.tempCurrent - 32.0) / (9.0/5.0);
      OperatingParameters.tempCorrection = OperatingParameters.tempCorrection * 5.0 / 9.0;
      OperatingParameters.tempSwing = OperatingParameters.tempSwing * 5.0 / 9.0;
      resetTempSmooth();
      lv_arc_set_range(ui_TempArc, 7*10, 33*10);
      lv_obj_clear_flag(ui_SetTempFrac, LV_OBJ_FLAG_HIDDEN);
      OperatingParameters.tempUnits = 'C';
    }
    else {
      OperatingParameters.tempSet = (OperatingParameters.tempSet * 9.0/5.0) + 32.0;
      OperatingParameters.tempCurrent = (OperatingParameters.tempCurrent * 9.0/5.0) + 32.0;
      OperatingParameters.tempCorrection = OperatingParameters.tempCorrection * 1.8;
      OperatingParameters.tempSwing = OperatingParameters.tempSwing * 1.8;
      resetTempSmooth();
      lv_arc_set_range(ui_TempArc, 45*10, 92*10);
      lv_obj_add_flag(ui_SetTempFrac, LV_OBJ_FLAG_HIDDEN);
      OperatingParameters.tempUnits = 'F';
    }
  }
  else
  {
    ESP_LOGE(TAG, "Could not dispatch request \"%s\"", content);
    OperatingParameters.Errors.systemErrors++;
  }
}

#define min(x, y) ((x < y) ? x : y)

esp_err_t handleButton(httpd_req_t *req)
{
  char content[BUTTON_CONTENT_SIZE];
  size_t recv_size = min(req->content_len, sizeof(content));
  httpd_resp_set_type(req, "text/xml");
  int ret = httpd_req_recv(req, content, recv_size);
  content[recv_size] = '\0';
  buttonDispatch(content);
  return httpd_resp_send(req, 0, 0);
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
  xmlSpace -= snprintf(buf, sizeof(buf), "<curTemp>%.1f</curTemp>\n", OperatingParameters.tempCurrent + OperatingParameters.tempCorrection);
  CAT_IF_SPACE(xml, buf, xmlSpace, req);
  if (OperatingParameters.tempUnits == 'F')
    xmlSpace -= snprintf(buf, sizeof(buf), "<setTemp>%.0f</setTemp>\n", OperatingParameters.tempSet);
  else
    xmlSpace -= snprintf(buf, sizeof(buf), "<setTemp>%.1f</setTemp>\n", OperatingParameters.tempSet);
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
  ESP_LOGD(TAG, "Remaining XML space: %ld", xmlSpace);
  return httpd_resp_send(req, xml, strlen(xml));
}

esp_err_t handleRoot(httpd_req_t *req)
{
  return httpd_resp_send(req, webUI, sizeof(webUI));
}

esp_err_t fwUpload(httpd_req_t *req)
{
//@@@  clearNVS();
  return httpd_resp_send(req, web_fw_upload, strlen(web_fw_upload));
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
httpd_uri_t uri_update = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = fwUpdate,
    .user_ctx = NULL};

void webStart()
{
  if (!WifiConnected())
  {
    ESP_LOGW (TAG, "***  WARNING: Starting web server while wifi down!!  *****");
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;

  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_xml);
    httpd_register_uri_handler(server, &uri_button);
    httpd_register_uri_handler(server, &uri_upload);
    httpd_register_uri_handler(server, &uri_update);
  }

  if (server == NULL)
  {
    ESP_LOGE (TAG, "** FAILED TO START WEB SERVER **");
    OperatingParameters.Errors.systemErrors++;
  }
}
