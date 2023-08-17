/*!
// SPDX-License-Identifier: GPL-3.0-only
/*
 * wifi.cpp
 *
 * All functionality related to wireless interface are included here.
 * An API is provided for the thermostat to connect and control the
 * wifi interface using the ESP-IDF framework. Details about the
 * connection (such as signal strength, connection status, IP address)
 * are supplied here.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * Notes:
 * 
 * Some of the inspiration for code was gotten from:
 * https://github.com/xpress-embedo/ESP32/blob/master/ConnectToWiFi/src/main.cpp
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrite to use ESP-IDF
 * 
 */

#include "thermostat.hpp"

#define WIFI_MAX_SSID (6u)
#define WIFI_SSID_LEN (32u)

#define DEFAULT_SCAN_LIST_SIZE WIFI_MAX_SSID

#define CONFIG_LOG_DEFAULT_LEVEL_VERBOSE 1
#define CONFIG_LOG_DEFAULT_LEVEL 5
#define CONFIG_LOG_MAXIMUM_EQUALS_DEFAULT 1
#define CONFIG_LOG_MAXIMUM_LEVEL 5
#define CONFIG_LOG_TIMESTAMP_SOURCE_RTOS 1
// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
static const char *TAG = "WIFI";

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define CONFIG_ESP_MAXIMUM_RETRY 3

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group = NULL;
const TickType_t xTicksToWait = 20000 / portTICK_PERIOD_MS;
esp_event_handler_instance_t instance_any_id;
esp_event_handler_instance_t instance_got_ip;

// void wifi_disconnect_sta(void);
static esp_netif_t *esp_netif_interface_sta;
int s_retry_num = 0;

WIFI_CREDS WifiCreds;
WIFI_STATUS WifiStatus = {0};

void WifiDeinit();

///////////////////////////////////////////////////////////////////////////////////////
//
// Support for TFT UI
//
// From: https://github.com/xpress-embedo/ESP32/blob/master/ConnectToWiFi/src/main.cpp
//
///////////////////////////////////////////////////////////////////////////////////////

static char wifi_dd_list[WIFI_MAX_SSID*WIFI_SSID_LEN] = { 0 };

/**
 * @brief This function returns the WiFi SSID Names appended together which is 
 *        suitable for LVGL drop down widget
 * @param  none
 * @return pointer to wifi ssid drop down list
 */
char *Get_WiFiSSID_DD_List( void )
{
  return wifi_dd_list;
}

/**
 * @brief Scans for the SSID and store the results in a WiFi drop down list
 * "wifi_dd_list", this list is suitable to be used with the LVGL Drop Down
 * @param  none
 */
/* Initialize Wi-Fi as sta and set scan method */

void WiFi_ScanSSID(void)
{
  String ssid_name;
  uint16_t number = DEFAULT_SCAN_LIST_SIZE;
  wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
  uint16_t ap_count = 0;

  memset(ap_info, 0, sizeof(ap_info));

  if (!WifiStatus.wifi_started)
  {
    wifiStart("", "", "");
  }

  esp_wifi_scan_start(NULL, true);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
  Serial.printf("Total APs scanned = %u\n", ap_count);
  if (ap_count == 0)
  {
    Serial.println("No Networks Found");
    memset (wifi_dd_list, 0x00, sizeof(wifi_dd_list));
    strcpy (wifi_dd_list, "-- No SSIDs --");
  }
  else
  {
    String ssid;
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
    {
      ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
      Serial.printf("SSID \t\t%s\n", ap_info[i].ssid);
      ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
      Serial.printf("RSSI \t\t%d\n", ap_info[i].rssi);
      // print_auth_mode(ap_info[i].authmode);
      if (ap_info[i].authmode != WIFI_AUTH_WEP)
      {
          // print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
      }
      ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
      Serial.printf("Channel \t\t%d\n", ap_info[i].primary);

      String ssid((const char *)(ap_info[i].ssid));

      if (ssid_name.indexOf(ssid) == -1)
      {
        if( i == 0 )
        {
          ssid_name = ssid;
        }
        else
        {
          ssid_name = ssid_name + '\n';
          ssid_name = ssid_name + ssid;
        }
      }
      // clear the array, it might be possible that we coming after rescanning
      memset (wifi_dd_list, 0x00, sizeof(wifi_dd_list));
      strncpy (wifi_dd_list, ssid_name.c_str(), WIFI_SSID_LEN);
      // Serial.println(wifi_dd_list);
    }
  }
}

/////////////////////////////////////////////////////////////////
// End of Support for TFT UI
/////////////////////////////////////////////////////////////////

static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
  Serial.printf ("event_handler()\n");
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    Serial.printf ("  event = STA_START\n");
    esp_wifi_connect();
	}
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    Serial.printf ("  event = STA_DISCONECTED (retry = %d (MAX %d)\n", s_retry_num, CONFIG_ESP_MAXIMUM_RETRY);
    WifiStatus.Connected = false;
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY)
    {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
      Serial.printf ("  retry to connect to AP\n");
			//wifi_disconnect_sta();
		}
    else
    {
      Serial.printf ("  retry count exceeded\n");
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
    Serial.printf ("  connect to the AP fail\n");
		ESP_LOGI(TAG,"connect to the AP fail");
	}
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    Serial.printf ("  event = STA_GOT_IP\n");
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		Serial.printf ("  got ip: " IPSTR "\n", IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    WifiStatus.Connected = true;
    WifiStatus.ip = event->ip_info.ip;
	}
  // return (wifiConnected() == ESP_OK);
}

void wifiSetHostname(const char *hostname)
{
  Serial.printf ("wifiSetHostname(\"%s\")\n", hostname);

  // Set the hostname for the network interface
  ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname));
}

void wifiSetCredentials(const char *ssid, const char *pass)
{
  bool wasInitialized = false;

  Serial.printf ("wifiSetCredentials(\"%s\", ...)\n", ssid);

  ////////////////////////////////////////////////////////////////////////////////////WifiStatus.driver_started = true;
  // ESP_ERROR_CHECK(esp_netif_init()); // ADDED
  // ESP_ERROR_CHECK(esp_event_loop_create_default()); // ADDED
  // esp_netif_interface_sta = esp_netif_create_default_wifi_sta(); // ADDED

  if (!WifiStatus.if_init)
  {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    Serial.printf ("- Calling esp_wifi_init()\n");
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    WifiStatus.if_init = true;
  }
  else
  {
    wasInitialized = true;
  }

  // Set the wifi parameters
	wifi_config_t wifi_config;
	bzero(&wifi_config, sizeof(wifi_config_t));
	strcpy((char *)wifi_config.sta.ssid, ssid);
	strcpy((char *)wifi_config.sta.password, pass);
  if (!wasInitialized)
  {
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
  }
  /* Initialize STA */
  // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  Serial.printf ("- Calling esp_wifi_set_config()\n");
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  //
  // After creating the driver stack to save the wifi credentials, we
  // now need to tear it all down.
  //
  // WifiDeinit();
  if (!wasInitialized)
  {
    Serial.printf ("- Calling esp_wifi_deinit()\n");
    esp_wifi_deinit();
    WifiStatus.driver_started = false;
    WifiStatus.if_init = false;
  }
}

void wifiRegisterEventCallbacks()
{
  Serial.printf ("wifiRegisterEventCallbacks()\n");

  /* Initialize event group */
  s_wifi_event_group = xEventGroupCreate();
  s_retry_num = 0;

  /* Register Event handler */
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                            ESP_EVENT_ANY_ID,
                            &event_handler,
                            NULL,
                            &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                            IP_EVENT_STA_GOT_IP,
                            &event_handler,
                            NULL,
                            &instance_got_ip));

}

void wifiDeregisterEventCallbacks()
{
  Serial.printf ("wifiDeregisterEventCallbacks()\n");

  if (s_wifi_event_group == NULL)
  {
    Serial.printf ("- Callbacks already deregistered\n");
    return;
  }

  /* The event will not be processed after unregister */
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));

  vEventGroupDelete(s_wifi_event_group);
  s_wifi_event_group = NULL;
}

bool wifiStart(const char *hostname, const char *ssid, const char *pass)
{
  // Enable debug logging
  esp_log_level_set(TAG, ESP_LOG_DEBUG);
  // esp_log_level_set("*", ESP_LOG_INFO); 

	ESP_LOGI(TAG, "wifiStart()\n");
  Serial.printf("wifiStart()\n");

  if (!WifiStatus.driver_started)
  {
    Serial.printf("  Wifi driver not started; Calling init and create funcs\n");
    WifiStatus.driver_started = true;
    ESP_ERROR_CHECK(esp_netif_init()); // ADDED
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // ADDED
    esp_netif_interface_sta = esp_netif_create_default_wifi_sta(); // ADDED
  }

  if (strlen(ssid))
  {
    ESP_LOGI(TAG, "wifi_connect_sta");
    Serial.printf("  Calling wifiRegisterEventCallbacks()\n");
    wifiRegisterEventCallbacks();
  }

  ESP_LOGI(TAG, "Initializing wifi\n");
  Serial.printf("  Initializing wifi\n");

  // Set the hostname for the network interface
  Serial.printf("  Setting wifi hostname\n");
  if (strlen(hostname))
    esp_netif_set_hostname(esp_netif_interface_sta, hostname);
  else
    esp_netif_set_hostname(esp_netif_interface_sta, "thermostat");
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // ADDDED
  ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // ADDED
  WifiStatus.if_init = true;

  if (strlen(ssid))
  {
    // Set the wifi parameters
    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config_t));
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, pass);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    /* Initialize STA */
    Serial.printf("  Calling esp_wifi_set_mode()\n");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    Serial.printf("  Calling esp_wifi_set_config()\n");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  }

  Serial.printf("  Calling esp_wifi_start()\n");
	ESP_LOGI(TAG, "wifi_start");
	ESP_ERROR_CHECK(esp_wifi_start());
  WifiStatus.wifi_started = true;

  if (strlen(ssid) == 0)
    return true;

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	esp_err_t ret_value = ESP_OK;
  Serial.printf("  Waiting for event callback...\n");
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
			WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
			pdFALSE,
			pdFALSE,
			xTicksToWait);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT)
  {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", WifiCreds.ssid, WifiCreds.password);
		Serial.printf("connected to ap SSID:%s password:%s\n", WifiCreds.ssid, WifiCreds.password);
	}
  else if (bits & WIFI_FAIL_BIT)
  {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", WifiCreds.ssid, WifiCreds.password);
    Serial.printf("Failed to connect to SSID:%s, password:%s\n", WifiCreds.ssid, WifiCreds.password);
		ret_value = ESP_FAIL;
		//wifi_disconnect_sta();
	}
  else
  {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
		Serial.printf ("UNEXPECTED EVENT\n");
		ret_value = ESP_FAIL;
	}

  //
  // Intentionally leave the callbacks in place as they are used to update
  // statuses such as connection status and ip address.
  //

	ESP_LOGI(TAG, "wifi_connect_sta finished.");
	return (ret_value == ESP_OK);
}

bool wifiConnected()
{
  // Updated in event callbacks
  if (WifiStatus.wifi_started)
    return (WifiStatus.Connected);
  else
    return false;
}

void WifiDeinit()
{
  Serial.printf ("WifiDeinit()\n");

  if (WifiStatus.wifi_started)
  {
    Serial.printf ("- Calling esp_wifi_stop()\n");
    esp_wifi_stop();
    WifiStatus.wifi_started = false;
  }

  if (WifiStatus.driver_started)
  {
    Serial.printf ("- Taking down network stack\n");
    wifiDeregisterEventCallbacks();
    esp_netif_destroy_default_wifi(esp_netif_interface_sta);
    esp_event_loop_delete_default();
    esp_wifi_deinit();
    WifiStatus.driver_started = false;
    WifiStatus.if_init = false;
  }
  else
  {
    Serial.printf ("- WifiDeinit() called, but state wrong:\n");
    Serial.printf ("  WifiStatus.driver_started = %d (wanted: true)\n", WifiStatus.driver_started);
  }
}

void WifiDisconnect()
{
  Serial.printf ("WifiDisconnect()\n");

  Serial.printf ("- Calling esp_wifi_disconnect()\n");
  esp_wifi_disconnect();
  OperatingParameters.wifiConnected = false;
}

int32_t lastWifiMillis = 0;
TaskHandle_t ntReconnectTaskHandler = NULL;

bool wifiReconnect(const char *hostname, const char *ssid, const char *pass)
{
  Serial.printf ("wifiReconnect()\n");
  if (!WifiStatus.driver_started)
  {
    Serial.printf ("- wifiReconnect() called, but state(s) wrong:\n");
    Serial.printf ("  WifiStatus.driver_started = %d (wanted: true)\n", WifiStatus.driver_started);
    return false;
  }

  if (OperatingParameters.wifiConnected)
  {
    Serial.printf ("- Disconnecting wifi - \n");
    WifiDisconnect();
    // Serial.printf ("- Wifi stopped\n");
    // vTaskDelay(500 / portTICK_PERIOD_MS);
  }

  if (WifiStatus.wifi_started)
  {
    Serial.printf ("- Calling esp_wifi_stop()\n");
    esp_wifi_stop();
    WifiStatus.wifi_started = false;
  }

  if (WifiStatus.driver_started)
  {
    Serial.printf ("- Calling WifiDeinit()\n");
    WifiDeinit();
  }

  Serial.printf ("- Restarting wifi\n");
  return (wifiStart(hostname, ssid, pass));
}

void networkReconnectTask(void *pvParameters)
{
  Serial.printf("Starting network reconnect task\n");

  if (millis() - lastWifiMillis < WIFI_CONNECT_INTERVAL)
  {
    Serial.printf ("- Wifi reconnect retry too soon ... delaying\n");
    ntReconnectTaskHandler = NULL;
    vTaskDelete(NULL);
    return; // Will never get here, but wanted to give the compiler an exit path
  }

  lastWifiMillis = millis();

  if (wifiReconnect(WifiCreds.hostname, WifiCreds.ssid, WifiCreds.password) == true)
  {
    OperatingParameters.wifiConnected = true;
  }
  else
  {
    OperatingParameters.wifiConnected = false;
  }

  ntReconnectTaskHandler = NULL;
  vTaskDelete(NULL);
}

void startReconnectTask()
{
  if (ntReconnectTaskHandler)
  {
    Serial.printf ("Network reconnect task already running ... Exiting\n");
    return;
  }

  xTaskCreate(
    networkReconnectTask,
    "NetworkReconnectTask",
    4096,
    NULL,
    1,
    &ntReconnectTaskHandler);
}


uint16_t rssiToPercent(int rssi_i)
{
  float rssi = (float)rssi_i;
  rssi = isnan(rssi) ? -100.0 : rssi;
  rssi = min(max(2 * (rssi + 100.0), 0.0), 100.0);
  return (uint16_t)rssi;
}

uint16_t wifiSignal()
{
  long rssi;
  wifi_ap_record_t ap;
  if (WifiStatus.wifi_started && WifiStatus.Connected)
  {
    esp_wifi_sta_get_ap_info(&ap);
    rssi = ap.rssi;
    // Serial.printf("rssi: %d\n", rssi);

    // Convert the rssi into a signal quality in percent
    return rssiToPercent(rssi);
  }
  else
  {
    return 0;
  }
}

char *wifiAddress()
{
  static char ipAddress[24];
  if (WifiStatus.wifi_started)
  {
#if 1
    tcpip_adapter_ip_info_t ipInfo; 
    // IP address.
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
    snprintf(ipAddress, sizeof(ipAddress), "%d.%d.%d.%d", 
      (ipInfo.ip.addr & 0x000000ff),
      ((ipInfo.ip.addr & 0x0000ff00) >> 8),
      ((ipInfo.ip.addr & 0x00ff0000) >> 16),
      ((ipInfo.ip.addr & 0xff000000) >> 24)
    );
#else
    snprintf(ipAddress, sizeof(ipAddress), "%d.%d.%d.%d", IP2STR(&WifiStatus.ip));
#endif
  }
  else
  {
    strcpy (ipAddress, "0.0.0.0");
  }
  // Serial.printf ("Current IP: %s\n", ipAddress);
  return ipAddress;
}
