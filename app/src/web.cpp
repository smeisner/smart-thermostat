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

#include "thermostat.hpp"
#include <esp_http_server.h>
#include "version.h"
#include "web_ui.h"
#include <esp_ota_ops.h>

static const char *TAG = "WEB";

const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
const char* serverRedirect = "<meta http-equiv=\"refresh\" content=\"0; url='/'\" />";
static char html[2200];

esp_err_t tempUp(httpd_req_t *req)
{
  if (OperatingParameters.tempUnits == 'C')
  {
    OperatingParameters.tempSet += 0.5;
    updateHvacSetTemp(roundValue(OperatingParameters.tempSet, 1));
  } else {
    OperatingParameters.tempSet += 1.0;
    updateHvacSetTemp(roundValue(OperatingParameters.tempSet, 0));
  }
  return httpd_resp_send(req, serverRedirect, strlen(serverRedirect));
}

esp_err_t tempDown(httpd_req_t *req)
{
  if (OperatingParameters.tempUnits == 'C')
  {
    OperatingParameters.tempSet -= 0.5;
    updateHvacSetTemp(roundValue(OperatingParameters.tempSet, 1));
  } else {
    OperatingParameters.tempSet -= 1.0;
    updateHvacSetTemp(roundValue(OperatingParameters.tempSet, 0));
  }
  return httpd_resp_send(req, serverRedirect, strlen(serverRedirect));
}

esp_err_t handleRoot(httpd_req_t *req)
{
  snprintf(html, sizeof(html), webUI,
    (int)OperatingParameters.tempSet, OperatingParameters.tempUnits,
    hvacModeToString(OperatingParameters.hvacSetMode),
    hvacModeToString(OperatingParameters.hvacOpMode),
    OperatingParameters.tempCurrent + OperatingParameters.tempCorrection,
    OperatingParameters.tempUnits,
    OperatingParameters.humidCurrent + OperatingParameters.humidityCorrection,
    OperatingParameters.lightDetected,
    (OperatingParameters.motionDetected == true) ? "True" : "False",
    OperatingParameters.tempUnits, OperatingParameters.tempSwing,
    OperatingParameters.tempCorrection,
    wifiSignal(), wifiAddress(), VERSION_STRING, VERSION_BUILD_DATE_TIME,
    VERSION_COPYRIGHT
  );

  return httpd_resp_send(req, html, strlen(html));
}

esp_err_t clearFirmware(httpd_req_t *req)
{
  clearNVS();
  return httpd_resp_send(req, serverRedirect, strlen(serverRedirect));
}

esp_err_t hvacModeOff(httpd_req_t *req)
{
  updateHvacMode(OFF);
  return httpd_resp_send(req, serverRedirect, strlen(serverRedirect));
}

esp_err_t hvacModeAuto(httpd_req_t *req)
{
  updateHvacMode(AUTO);
  return httpd_resp_send(req, serverRedirect, strlen(serverRedirect));
}

esp_err_t hvacModeHeat(httpd_req_t *req)
{
  updateHvacMode(HEAT);
  return httpd_resp_send(req, serverRedirect, strlen(serverRedirect));
}

esp_err_t hvacModeCool(httpd_req_t *req)
{
  updateHvacMode(COOL);
  return httpd_resp_send(req, serverRedirect, strlen(serverRedirect));
}

esp_err_t hvacModeFan(httpd_req_t *req)
{
  updateHvacMode(FAN_ONLY);
  return httpd_resp_send(req, serverRedirect, strlen(serverRedirect));
}

//   server.on("/upload", HTTP_GET, []() {
//     server.sendHeader("Connection", "close");
//     server.send(200, "text/html", serverIndex);
//   });

// void webInit(void * parameter)
// {
//   if (!wifiConnected())
//     Serial.printf ("***  WARNING: Starting web server while wifi down!!  *****\n");

//   MDNS.begin(WifiCreds.hostname);

//   server.on("/", handleRoot);
//   server.on("/tempUp", tempUp);
//   server.on("/tempDown", tempDown);

//   server.on("/clearFirmware", HTTP_GET, []() {
//       clearNVS();
//       server.send(200, "text/html", serverRedirect);
//   });

//   server.on("/hvacModeOff", HTTP_GET, []() {
//       OperatingParameters.hvacSetMode = OFF;
//       eepromUpdateHvacSetMode();
//       server.send(200, "text/html", serverRedirect);
//   });

//   server.on("/hvacModeAuto", HTTP_GET, []() {
//       OperatingParameters.hvacSetMode = AUTO;
//       eepromUpdateHvacSetMode();
//       server.send(200, "text/html", serverRedirect);
//   });

//   server.on("/hvacModeHeat", HTTP_GET, []() {
//       OperatingParameters.hvacSetMode = HEAT;
//       eepromUpdateHvacSetMode();
//       server.send(200, "text/html", serverRedirect);
//   });

//   server.on("/hvacModeCool", HTTP_GET, []() {
//       OperatingParameters.hvacSetMode = COOL;
//       eepromUpdateHvacSetMode();
//       server.send(200, "text/html", serverRedirect);
//   });

//   server.on("/hvacModeFan", HTTP_GET, []() {
//       OperatingParameters.hvacSetMode = FAN;
//       eepromUpdateHvacSetMode();
//       server.send(200, "text/html", serverRedirect);
//   });

//   server.on("/upload", HTTP_GET, []() {
//     server.sendHeader("Connection", "close");
//     server.send(200, "text/html", serverIndex);
//   });

//   server.on("/update", HTTP_POST, []() {
//     server.sendHeader("Connection", "close");
//     server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
//     vTaskDelay(500 / portTICK_PERIOD_MS);
//     ESP.restart();
//   }, []() {
//     HTTPUpload& upload = server.upload();
//     if (upload.status == UPLOAD_FILE_START) {
//       Serial.setDebugOutput(true);
//       Serial.printf("Update: %s\n", upload.filename.c_str());
//       if (!Update.begin()) { //start with max available size
//         Update.printError(Serial);
//       }
//     } else if (upload.status == UPLOAD_FILE_WRITE) {
//       if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
//         Update.printError(Serial);
//       }
//     } else if (upload.status == UPLOAD_FILE_END) {
//       if (Update.end(true)) { //true to set the size to the current progress
//         Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
//       } else {
//         Update.printError(Serial);
//       }
//       Serial.setDebugOutput(false);
//     } else {
//       Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
//     }
//   });
//   server.begin();
//   MDNS.addService("http", "tcp", 80);

//   Serial.printf("Ready! Open http://%s.local in your browser\n", WifiCreds.hostname);

//   Serial.printf ("Starting web server task\n");
//   for (;;) { server.handleClient(); delay(10); }


// }

esp_err_t fwUpload(httpd_req_t *req)
{
//@@@  clearNVS();
  return httpd_resp_send(req, web_fw_upload, strlen(web_fw_upload));
}

#define min(x, y) ((x < y) ? x : y)

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

httpd_uri_t uri_tempUp = {
    .uri = "/tempUp",
    .method = HTTP_GET,
    .handler = tempUp,
    .user_ctx = NULL};

httpd_uri_t uri_tempDown = {
    .uri = "/tempDown",
    .method = HTTP_GET,
    .handler = tempDown,
    .user_ctx = NULL};

httpd_uri_t uri_clearFirmware = {
    .uri = "/clearFirmware",
    .method = HTTP_GET,
    .handler = clearFirmware,
    .user_ctx = NULL};

httpd_uri_t uri_hvacModeOff = {
    .uri = "/hvacModeOff",
    .method = HTTP_GET,
    .handler = hvacModeOff,
    .user_ctx = NULL};
httpd_uri_t uri_hvacModeAuto = {
    .uri = "/hvacModeAuto",
    .method = HTTP_GET,
    .handler = hvacModeAuto,
    .user_ctx = NULL};
httpd_uri_t uri_hvacModeHeat = {
    .uri = "/hvacModeHeat",
    .method = HTTP_GET,
    .handler = hvacModeHeat,
    .user_ctx = NULL};
httpd_uri_t uri_hvacModeCool = {
    .uri = "/hvacModeCool",
    .method = HTTP_GET,
    .handler = hvacModeCool,
    .user_ctx = NULL};
httpd_uri_t uri_hvacModeFan = {
    .uri = "/hvacModeFan",
    .method = HTTP_GET,
    .handler = hvacModeFan,
    .user_ctx = NULL};

httpd_uri_t uri_firmwareLoad = {
	.uri	  = "/upload",
	.method   = HTTP_GET,
	.handler  = fwUpload,
	.user_ctx = NULL
};
httpd_uri_t uri_firmwareUpdate = {
	.uri	  = "/update",
	.method   = HTTP_POST,
	.handler  = fwUpdate,
	.user_ctx = NULL
};

void webStart()
{
  if (!wifiConnected())
  {
    ESP_LOGW (TAG, "***  WARNING: Starting web server while wifi down!!  *****");
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 11;
  httpd_handle_t server = NULL;

  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_firmwareLoad);
    httpd_register_uri_handler(server, &uri_firmwareUpdate);
    httpd_register_uri_handler(server, &uri_tempUp);
    httpd_register_uri_handler(server, &uri_tempDown);
    httpd_register_uri_handler(server, &uri_clearFirmware);
    httpd_register_uri_handler(server, &uri_hvacModeOff);
    httpd_register_uri_handler(server, &uri_hvacModeAuto);
    httpd_register_uri_handler(server, &uri_hvacModeHeat);
    httpd_register_uri_handler(server, &uri_hvacModeCool);
    httpd_register_uri_handler(server, &uri_hvacModeFan);
  }

  if (server == NULL)
  {
    ESP_LOGE (TAG, "** FAILED TO START WEB SERVER **");
  }
}
