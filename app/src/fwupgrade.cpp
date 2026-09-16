#include "version.h"
int telnet_esp32_printf(const char *fmt, ...);

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"

static const char *TAG = "GITHUB_OTA";
// CURRENT_FIRMWARE_VERSION defined in version.h
#define GITHUB_USER     "smeisner"
#define GITHUB_REPO     "smart-thermostat"
#define GITHUB_REDIRECT_URL "https://github.com/" GITHUB_USER "/" GITHUB_REPO "/releases/latest"

// Buffer to store the intercepted Location URL string
static char redirected_location[256] = {0};

// Change this macro if you alter your binary compilation output name on GitHub Releases
#define GITHUB_BIN_ASSET_NAME "firmware.bin"

static void download_and_install_ota(const char *tag_name, bool sourceTelnet)
{
  if (sourceTelnet) telnet_esp32_printf("New version validated: %s. Initiating flashing sequence...\n", tag_name);
  ESP_LOGI(TAG, "New version validated: %s. Initiating flashing sequence...", tag_name);

  // Build the dynamic download string safely on the stack
  char ota_url[256] = {0};
  snprintf(ota_url, sizeof(ota_url), 
            "https://github.com/%s/%s/releases/download/%s/%s", 
            GITHUB_USER, GITHUB_REPO, tag_name, GITHUB_BIN_ASSET_NAME);

  if (sourceTelnet) telnet_esp32_printf("Targeting Asset Binary: %s\n", ota_url);
  ESP_LOGI(TAG, "Targeting Asset Binary: %s", ota_url);

  // Initialize the transport structure using the explicit memset pattern
  esp_http_client_config_t http_config;
  memset(&http_config, 0, sizeof(esp_http_client_config_t));

  http_config.url = ota_url;
  http_config.crt_bundle_attach = esp_crt_bundle_attach; // Authenticates Amazon AWS S3 storage chains
  http_config.max_redirection_count = 4;                 // Crucial: Allows leaping from GitHub to AWS S3 storage
  http_config.keep_alive_enable = true;

  // FIX: Allocate larger internal buffers to safely read long AWS S3 Redirect headers without breaking
  http_config.buffer_size = 2048;    // Expands rx buffer space from 512 bytes
  http_config.buffer_size_tx = 2048; // Expands tx buffer space from 512 bytes

  // Bind the transport configuration to Espressif's HTTPS OTA subsystem engine
  esp_https_ota_config_t ota_config;
  memset(&ota_config, 0, sizeof(esp_https_ota_config_t));
  ota_config.http_config = &http_config;

  if (sourceTelnet) telnet_esp32_printf("Streaming update payload over TLS. Please do not power off device...\n");
  ESP_LOGI(TAG, "Streaming update payload over TLS. Please do not power off device...");
  
  esp_https_ota_handle_t ota_handle; // = NULL;
  esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);
  
  if (ret == ESP_OK)
  {
      if (sourceTelnet) telnet_esp32_printf("Connection open. Downloading and flashing new firmware image...\n");
      ESP_LOGI(TAG, "Connection open. Downloading and flashing new firmware image...");

      // Complete download loop via streaming handle wrapper
      while (1)
      {
        esp_err_t ota_status = esp_https_ota_perform(ota_handle);
        if (ota_status != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        {
          ret = ota_status;
          break;
        }
      }
  }

  // Final check and finish operations
  if (ret == ESP_OK && esp_https_ota_finish(ota_handle) == ESP_OK)
  {
    if (sourceTelnet) telnet_esp32_printf("Firmware binary flashed successfully!\nRestarting chip to boot into new slot...\n");
    ESP_LOGI(TAG, "Firmware binary flashed successfully! Restarting chip to boot into new slot...");
    vTaskDelay(pdMS_TO_TICKS(2000)); 
    esp_restart();
  }
  else
  {
    if (ota_handle) esp_https_ota_abort(ota_handle);
    if (sourceTelnet) telnet_esp32_printf("OTA Flashing Failure! Error reason: %s\n", esp_err_to_name(ret));
    ESP_LOGE(TAG, "OTA Flashing Failure! Error reason: %s", esp_err_to_name(ret));
  }
}

static esp_err_t _http_event_handler(esp_http_client_event_handle_t evt)
{
  switch (evt->event_id)
  {
    case HTTP_EVENT_ON_HEADER:
      // Check if the incoming header key matches "Location"
      if (strcasecmp(evt->header_key, "Location") == 0)
      {
        strncpy(redirected_location, evt->header_value, sizeof(redirected_location) - 1);
        ESP_LOGI(TAG, "Intercepted Location Header: %s", redirected_location);
      }
      break;
    default:
      break;
  }
  return ESP_OK;
}

bool check_github_for_updates(bool sourceTelnet)
{
  if (sourceTelnet) telnet_esp32_printf("Checking GitHub for firmware updates...\n");
  ESP_LOGI(TAG, "Querying GitHub redirect header for latest tag...");

  memset(redirected_location, 0, sizeof(redirected_location));

  esp_http_client_config_t config;
  memset(&config, 0, sizeof(esp_http_client_config_t));

  config.url = GITHUB_REDIRECT_URL;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.event_handler = _http_event_handler;
  config.disable_auto_redirect = true; // Stop at 302 hop
  
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL)
  {
    if (sourceTelnet) telnet_esp32_printf("Failed to initialize HTTP client.\n");
    ESP_LOGE(TAG, "Failed to initialize HTTP client.");
    return false;
  }

  esp_http_client_set_header(client, "User-Agent", "ESP32-S3-Thermostat-Client");

  esp_err_t err = esp_http_client_perform(client);
  int status_code = esp_http_client_get_status_code(client);

  if (err == ESP_OK && (status_code == 302 || status_code == 301))
  {
    if (strlen(redirected_location) > 0)
    {
      // Expected URL shape: https://github.com
      const char *tag_ptr = strrchr(redirected_location, '/');
      if (tag_ptr != NULL)
      {
        tag_ptr++; // Move past the final slash

        if (sourceTelnet) telnet_esp32_printf("Extracted Latest Tag: %s | Running Version: %s\n", tag_ptr, CURRENT_FIRMWARE_VERSION);
        ESP_LOGI(TAG, "Extracted Latest Tag: %s | Running Version: %s", tag_ptr, CURRENT_FIRMWARE_VERSION);

        //
        // We just compare the running version to the latest version on Github.
        // If they differ, we have a new version available. No examination of the tag string is done here,
        // since we are not using semantic versioning.
        //
        if (strcasecmp(tag_ptr, CURRENT_FIRMWARE_VERSION) != 0)
        {
          if (sourceTelnet) telnet_esp32_printf("New firmware version detected: %s.\n", tag_ptr);
          ESP_LOGI(TAG, "New version detected! Closing metadata check to boot downloader...");
          esp_http_client_cleanup(client);
          return true; // Indicate that an update is available
        }
        else
        {
          if (sourceTelnet) telnet_esp32_printf("System firmware matches the latest online tag.\n");
          ESP_LOGI(TAG, "System firmware matches the latest online tag.");
        }
      }
      else
      {
        if (sourceTelnet) telnet_esp32_printf("Failed to parse tag token boundaries from Location string.\n");
        ESP_LOGE(TAG, "Failed to parse tag token boundaries from Location string.");
      }
    }
    else
    {
      if (sourceTelnet) telnet_esp32_printf("Location header string was empty during event capture.\n");
      ESP_LOGE(TAG, "Location header string was empty during event capture.");
    }
  }
  else
  {
    if (sourceTelnet) telnet_esp32_printf("Failed to capture redirect. HTTP Status: %d, Error: %s\n", status_code, esp_err_to_name(err));
    ESP_LOGE(TAG, "Failed to capture redirect. HTTP Status: %d, Error: %s", status_code, esp_err_to_name(err));
  }
    
  esp_http_client_cleanup(client);

  return false; // No update available or error occurred
}

bool performUpdate(bool sourceTelnet)
{
  if (sourceTelnet) telnet_esp32_printf("Initiating firmware update check sequence...\n");
  ESP_LOGI(TAG, "Initiating firmware update check sequence...");
  const char *tag_ptr = strrchr(redirected_location, '/');
  if (tag_ptr != NULL)
  {
    tag_ptr++; // Move past the final slash
    download_and_install_ota(tag_ptr, sourceTelnet);
    return true;
  }
  return false;
}