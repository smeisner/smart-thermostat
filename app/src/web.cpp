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
 */

#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include "thermostat.hpp"
#include "version.h"
#include "web_ui.h"

static const char *TAG = "WEB";

static char html[2200];
static char xml[600];

void doTempUp(void)
{
  if (OperatingParameters.tempUnits == 'C')
  {
    OperatingParameters.tempSet += 0.5;
    OperatingParameters.tempSet = roundValue(OperatingParameters.tempSet, 1);
  } else {
    OperatingParameters.tempSet += 1.0;
    OperatingParameters.tempSet = roundValue(OperatingParameters.tempSet, 0);
  }
  eepromUpdateHvacSetTemp();
}

void doTempDown(void)
{
  if (OperatingParameters.tempUnits == 'C')
  {
    OperatingParameters.tempSet -= 0.5;
    OperatingParameters.tempSet = roundValue(OperatingParameters.tempSet, 1);
  } else {
    OperatingParameters.tempSet -= 1.0;
    OperatingParameters.tempSet = roundValue(OperatingParameters.tempSet, 0);
  }
  eepromUpdateHvacSetTemp();
}

#define BUTTON_CONTENT_SIZE 30
void buttonDispatch(char content[BUTTON_CONTENT_SIZE])
{
  if (!strncmp(content, "tempUp", BUTTON_CONTENT_SIZE))
    doTempUp();
  else if (!strncmp(content, "tempDown", BUTTON_CONTENT_SIZE))
    doTempDown();
  else if (!strncmp(content, "hvacModeOff", BUTTON_CONTENT_SIZE))
    OperatingParameters.hvacSetMode = OFF;
  else if (!strncmp(content, "hvacModeAuto", BUTTON_CONTENT_SIZE))
    OperatingParameters.hvacSetMode = AUTO;
  else if (!strncmp(content, "hvacModeHeat", BUTTON_CONTENT_SIZE))
    OperatingParameters.hvacSetMode = HEAT;
  else if (!strncmp(content, "hvacModeCool", BUTTON_CONTENT_SIZE))
    OperatingParameters.hvacSetMode = COOL;
  else if (!strncmp(content, "hvacModeFan", BUTTON_CONTENT_SIZE))
    OperatingParameters.hvacSetMode = FAN;
  else if (!strncmp(content, "clear", BUTTON_CONTENT_SIZE))
    clearNVS();
  else if (!strncmp(content, "hvacCoolEnable", BUTTON_CONTENT_SIZE))
    OperatingParameters.hvacCoolEnable = !OperatingParameters.hvacCoolEnable;  
  else if (!strncmp(content, "hvacFanEnable", BUTTON_CONTENT_SIZE)) {
    OperatingParameters.hvacFanEnable = !OperatingParameters.hvacFanEnable;  
  }
  else
    ESP_LOGI(TAG, "Could not dispatch request \"%s\"", content);
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
#undef BUTTON_CONTENT_SIZE

esp_err_t handleXML(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/xml");
  char buf[128];
  strcpy(xml, "<?xml version = '1.0'?><Data>\n");
  snprintf(buf, sizeof(buf), "<curTemp>%.1f</curTemp>\n", OperatingParameters.tempCurrent + OperatingParameters.tempCorrection);
  strcat(xml, buf);
  if (OperatingParameters.tempUnits == 'F')
    snprintf(buf, sizeof(buf), "<setTemp>%.0f</setTemp>\n", OperatingParameters.tempSet);
  else
    snprintf(buf, sizeof(buf), "<setTemp>%.1f</setTemp>\n", OperatingParameters.tempSet);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<curMode>%s</curMode>\n", hvacModeToString(OperatingParameters.hvacOpMode));
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<setMode>%s</setMode>\n", hvacModeToString(OperatingParameters.hvacSetMode));
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<humidity>%.1f</humidity>\n", OperatingParameters.humidCurrent);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<light>%d</light>\n", OperatingParameters.lightDetected);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<motion>%s</motion>\n", OperatingParameters.motionDetected ? "True" : "False");
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<units>%c</units>\n", OperatingParameters.tempUnits);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<swing>%.1f</swing>\n", OperatingParameters.tempSwing);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<correction>%.1f</correction>\n", OperatingParameters.tempCorrection);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<wifiStrength>%d</wifiStrength>\n", wifiSignal());
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<address>%s</address>\n", wifiAddress());
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<firmwareVer>%s</firmwareVer>\n", VERSION_STRING);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<firmwareDt>%s</firmwareDt>\n", VERSION_BUILD_DATE_TIME);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<copyright>%s</copyright>\n", VERSION_COPYRIGHT);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<hvacCoolEnable>%d</hvacCoolEnable>\n", OperatingParameters.hvacCoolEnable);
  strcat(xml, buf);
  snprintf(buf, sizeof(buf), "<hvacFanEnable>%d</hvacFanEnable>\n", OperatingParameters.hvacFanEnable);
  strcat(xml, buf);
  strcat(xml, "</Data>");;
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

  ESP_LOGI (TAG, "Firmware size: %i\n", remaining);

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
  if (!wifiConnected())
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
  }

  if (server == NULL)
  {
    ESP_LOGE (TAG, "** FAILED TO START WEB SERVER **");
  }
}
