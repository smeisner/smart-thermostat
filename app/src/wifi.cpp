/*!
// SPDX-License-Identifier: GPL-3.0-only
*/
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
 *  30-Aug-2023: Steve Meisner (steve@meisners.net) - Rewrite to use ESP-IDF
 *   1-Dec-2023: Steve Meisner (steve@meisners.net) - Added support for telnet
 * 
 */

#include <string> // for string class
#include "thermostat.hpp"
#include <esp_mac.h>  // for esp_read_mac()
#if 0

// This isn't working correctly yet. I believe due to relying on the 
// Arduino framework. Once this is abandoned, the following must be 
// revisited to fine tune the log level.
//
// Currently the log level can only be set in platformio.ini (see
// the CORE_DEBUG_LEVEL definition). But changing this causes an entire rebuild.

// Enable Arduino-ESP32 logging in Arduino IDE
#ifdef CORE_DEBUG_LEVEL
  #undef CORE_DEBUG_LEVEL
#endif
#ifdef LOG_LOCAL_LEVEL
  #undef LOG_LOCAL_LEVEL
#endif

#define CORE_DEBUG_LEVEL ESP_LOG_WARN
#define LOG_LOCAL_LEVEL CORE_DEBUG_LEVEL

#endif

static const char *TAG = "WIFI";

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "nvs_flash.h"

//////////////////////////////////////////////////////////////////////
typedef enum {
    TCPIP_ADAPTER_IF_STA = 0,     /**< Wi-Fi STA (station) interface */
    TCPIP_ADAPTER_IF_AP,          /**< Wi-Fi soft-AP interface */
    TCPIP_ADAPTER_IF_ETH,         /**< Ethernet interface */
    TCPIP_ADAPTER_IF_TEST,        /**< tcpip stack test interface */
    TCPIP_ADAPTER_IF_MAX
} tcpip_adapter_if_t;

#define isnan(n) ((n != n) ? 1 : 0)

/////////////////////////////////////////////////////////////////////


#define WIFI_MAX_SSID (6u)
#define WIFI_SSID_LEN (32u)
#define DEFAULT_SCAN_LIST_SIZE WIFI_MAX_SSID

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_FAIL_ABORTED BIT2
#define CONFIG_ESP_MAXIMUM_RETRY 5

static bool wifiScanActive = false;
static bool wifiConnecting = false;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group = NULL;
const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
esp_event_handler_instance_t instance_any_id;
esp_event_handler_instance_t instance_got_ip;

static esp_netif_t *esp_netif_interface_sta;
int s_retry_num = 0;

WIFI_CREDS WifiCreds;
WIFI_STATUS WifiStatus = {};

///////////////////////////////////////////////////////////////////////////////////////
//
// Support for wifi scanning with TFT UI:
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

// using namespace std;

/**
 * @brief Scans for the SSID and store the results in a WiFi drop down list
 * "wifi_dd_list", this list is suitable to be used with the LVGL Drop Down.
 * Initialize Wi-Fi as sta and set scan method.
 * @param  none
 */
void WiFi_ScanSSID(void)
{
  std::string ssid_name;
  uint16_t number = DEFAULT_SCAN_LIST_SIZE;
  wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
  uint16_t ap_count = 0;
  wifi_active_scan_time_t active {
    .min = 0,
    .max = 0
  };
  wifi_scan_time_t scan_time = {
    .active = active,
    .passive = 0
  };
  wifi_scan_config_t scan_config = {
    .ssid = 0,
    .bssid = 0,
    .channel = 0,
    .show_hidden = false,
    .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    .scan_time = scan_time,
    .home_chan_dwell_time = 0
  };

  wifiScanActive = true;
  if (wifiConnecting)
  {
    ESP_LOGW(TAG, "** DELAYING SCAN UNTIL WIFI CONNECT COMPLETE **");
    while (wifiConnecting)
      vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGW(TAG, "** Resuming wifi scan **");
  }

  memset(ap_info, 0, sizeof(ap_info));

  if (!WifiStatus.wifi_started)
  {
    WifiStart("", "", "");
  }

  esp_wifi_scan_start(&scan_config, true);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  wifiScanActive = false;
  ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
  if (ap_count == 0)
  {
    ESP_LOGW(TAG, "- No Networks Found");
    memset (wifi_dd_list, 0x00, sizeof(wifi_dd_list));
    strcpy (wifi_dd_list, "-- No SSIDs --");
  }
  else
  {
    std::string ssid;
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
    {
      ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
      ESP_LOGD(TAG, "RSSI \t\t%d", ap_info[i].rssi);
      if (ap_info[i].authmode != WIFI_AUTH_WEP)
      {
          // print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
      }
      ESP_LOGD(TAG, "Channel \t\t%d", ap_info[i].primary);

      ssid = (const char *)(ap_info[i].ssid);
      if (ssid_name.find(ssid) == -1) //std::string::npos)
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
    }
    // clear the array, it might be possible that we coming after rescanning
    memset (wifi_dd_list, 0x00, sizeof(wifi_dd_list));
    strncpy (wifi_dd_list, ssid_name.c_str(), sizeof(wifi_dd_list));
  }
}

/////////////////////////////////////////////////////////////////
// End of Support for TFT UI
/////////////////////////////////////////////////////////////////

static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
  // Only execute the next call if telnet is not enabled. This is
  // necessary since remote log monitoring could be enabled. If it
  // is, and we are called back here due to wifi disconnect event,
  // the next ESP_LOGx call will cause a panic.
#ifndef TELNET_ENABLED
  ESP_LOGI(TAG, "event_handler()");
#endif

  if (wifiScanActive)
  {
    ESP_LOGE(TAG, "  SSID scan active - aborting wifi connect");
    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_ABORTED);
    // WifiStatus.Connected = false;
    // OperatingParameters.wifiConnected = false;
    wifiConnecting = false;
    return;
  }

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    ESP_LOGD(TAG, "  event = STA_START");
#ifdef MATTER_ENABLED
    if (!OperatingParameters.MatterStarted)
#endif
      esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    // The next call has a side effect of disabling logging via telnet...
#ifdef TELNET_ENABLED
    if (telnetServiceRunning())
      terminateTelnetSession();
#endif
    /// ...making the next line safe.
    ESP_LOGI(TAG, "  event = STA_DISCONECTED - retry # %d (MAX %d)", s_retry_num, CONFIG_ESP_MAXIMUM_RETRY);
    WifiStatus.Connected = false;
    OperatingParameters.wifiConnected = false;
    if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY)
    {
#ifdef MATTER_ENABLED
      if (!OperatingParameters.MatterStarted)
      {
#endif
        s_retry_num++;
        ESP_LOGW(TAG, "  connect to the AP failed - retrying");
        esp_wifi_connect();
#ifdef MATTER_ENABLED
      }
#endif
    }
    else
    {
      ESP_LOGE(TAG, "  retry count exceeded");
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      wifiConnecting = false;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "  event = IP_EVENT_STA_GOT_IP - ip: " IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    WifiStatus.Connected = true;
    OperatingParameters.wifiConnected = true;
    WifiStatus.ip = event->ip_info.ip;
    wifiConnecting = false;
    #ifdef MQTT_ENABLED
    //@@@ Is this necessary when the IP address changes (lease expires)?
    // MqttHomeAssistantDiscovery();
    #endif
  }
}

void WifiSetHostname(const char *hostname)
{
  ESP_LOGI(TAG, "wifiSetHostname(\"%s\")", hostname);
  // Set the hostname for the network interface
  // ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname));
  ESP_ERROR_CHECK(esp_netif_set_hostname(esp_netif_interface_sta, hostname));
}

void WifiSetCredentials(const char *ssid, const char *pass)
{
  bool wasInitialized = false;

  ESP_LOGI(TAG, "wifiSetCredentials(\"%s\", ...)", ssid);

  if (!WifiStatus.if_init)
  {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_LOGD(TAG, "- Calling esp_wifi_init()");
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
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_LOGD(TAG, "- Calling esp_wifi_set_config()");
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  //
  // After creating the driver stack to save the wifi credentials, we
  // now need to tear it all down.
  //
  if (!wasInitialized)
  {
    ESP_LOGD(TAG, "- Calling esp_wifi_deinit()");
    esp_wifi_deinit();
    WifiStatus.driver_started = false;
    WifiStatus.if_init = false;
  }
}

void WifiRegisterEventCallbacks()
{
  ESP_LOGD(TAG, "WifiRegisterEventCallbacks()");

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

void WifiDeregisterEventCallbacks()
{
  ESP_LOGD(TAG, "WifiDeregisterEventCallbacks()");

  if (s_wifi_event_group == NULL)
  {
    ESP_LOGW(TAG, "- Callbacks already deregistered");
    return;
  }

  /* The event will not be processed after unregister */
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));

  vEventGroupDelete(s_wifi_event_group);
  s_wifi_event_group = NULL;
}

#ifdef MATTER_ENABLED
void WifiSwitchMatterMode()
{
  if (OperatingParameters.MatterEnabled)
  {
    // Setting MatterStarted keeps networkReconnect task from running
    OperatingParameters.MatterStarted = true;
    ESP_LOGI(TAG, "Enabling Matter");
    // Since we are shutting down wifi to enable Matter,
    // do not allow callbacks to take any action.
    WifiDeregisterEventCallbacks();
    ESP_LOGI(TAG, "Shutting down wifi connection");
    WifiDisconnect();
    ESP_LOGI(TAG, "Unloading wifi driver");
    WifiDeinit();
    ESP_LOGI(TAG, "Starting Matter API");
    OperatingParameters.MatterStarted = MatterInit();
    // Serial.printf ("Connecting to wifi\n");
    // OperatingParameters.wifiConnected =
    //   WifiStart(WifiCreds.hostname, WifiCreds.ssid, WifiCreds.password);
  }
  else
  {
    ESP_LOGI(TAG, "Disabling Matter");
    updateThermostatParams();
    // ESP restart will happen as part of UI code (see ui_events.cpp)
  }
}

void WifiMatterStarted()
{
  // wifiSetHostname(WifiCreds.hostname);

  // Add additional wifi details...
  WifiStatus.wifi_started = true;
  WifiStatus.if_init = true;
  WifiStatus.driver_started = true;
  WifiStatus.Connected = true;
  // Register callbacks for wifi events
  // WifiRegisterEventCallbacks();
}
#endif

bool WifiStart(const char *hostname, const char *ssid, const char *pass)
{
  // This code needs to be revisited later after Arduino framework removed.
  // See comments at top of module.
  //
  // Enable debug logging
  // esp_log_level_set("*", ESP_LOG_NONE);
  // esp_log_level_set(TAG, ESP_LOG_WARN);
  esp_log_level_set("*", ESP_LOG_INFO);
  //

  ESP_LOGI(TAG, "WifiStart()");

  nvs_flash_init();

  wifiConnecting = true;

#ifdef MATTER_ENABLED
  if (OperatingParameters.MatterStarted)
  {
    ESP_LOGI(TAG, "Matter is already running -- abort WifiStart()");
    return true;
  }
#endif

  if (!WifiStatus.driver_started)
  {
    ESP_LOGW(TAG, "  Wifi driver not started; Calling init and create funcs");
    WifiStatus.driver_started = true;
    //@@@
    // ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_interface_sta = esp_netif_create_default_wifi_sta();
  }

  if (strlen(ssid))
  {
    ESP_LOGD(TAG, "  Calling WifiRegisterEventCallbacks()");
    WifiRegisterEventCallbacks();
  }

  ESP_LOGI(TAG, "  Initializing wifi");

  // Set the hostname for the network interface
  ESP_LOGI(TAG, "  Setting wifi hostname");
  if (strlen(hostname))
    esp_netif_set_hostname(esp_netif_interface_sta, hostname);
  else
    esp_netif_set_hostname(esp_netif_interface_sta, "thermostat");
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  WifiStatus.if_init = true;

  ESP_LOGD(TAG, "  Calling esp_wifi_set_mode()");
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

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
    ESP_LOGD(TAG, "  Calling esp_wifi_set_config()");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  }

  // Retrieve the MAC address of the interface
  esp_read_mac(OperatingParameters.mac, ESP_MAC_WIFI_STA);
  ESP_LOGI (TAG, "MAC Address: %02x:%02x:%02x:%02x:%02x:%02x",
    OperatingParameters.mac[0],
    OperatingParameters.mac[1],
    OperatingParameters.mac[2],
    OperatingParameters.mac[3],
    OperatingParameters.mac[4],
    OperatingParameters.mac[5]);

  ESP_LOGI(TAG, "  Calling esp_wifi_start()");
	ESP_ERROR_CHECK(esp_wifi_start());
  WifiStatus.wifi_started = true;

  if (strlen(ssid) == 0)
  {
    wifiConnecting = false;
    return true;
  }

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  esp_err_t ret_value = ESP_OK;
  ESP_LOGI(TAG, "  Waiting for event callback...");
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_FAIL_ABORTED,
        pdFALSE,
        pdFALSE,
        xTicksToWait);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:****", WifiCreds.ssid);
  }
  else if (bits & WIFI_FAIL_BIT)
  {
    ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:****", WifiCreds.ssid);
    ret_value = ESP_FAIL;
  }
  else if (bits & WIFI_FAIL_ABORTED)
  {
    ESP_LOGE(TAG, "Aborted connect to SSID:%s, password:****", WifiCreds.ssid);
    ret_value = ESP_FAIL;
  }
  else
  {
    ESP_LOGW(TAG, "UNEXPECTED EVENT");
    ret_value = ESP_FAIL;
  }

  //
  // Intentionally leave the callbacks in place as they are used to update
  // statuses such as connection status and ip address.
  //

  ESP_LOGD(TAG, "wifi_connect_sta finished.");
  return (ret_value == ESP_OK);
}

bool WifiConnected()
{
  // Updated in event callbacks
  if (WifiStatus.wifi_started)
    return (WifiStatus.Connected);
  else
    return false;
}

void WifiDeinit()
{
  // Set log level to 'W' (warn) as this function is quite destructive
  ESP_LOGW(TAG, "WifiDeinit()");

  if (WifiStatus.wifi_started)
  {
    ESP_LOGD(TAG, "- Calling esp_wifi_stop()");
    esp_wifi_stop();
    WifiStatus.wifi_started = false;
  }

  if (WifiStatus.driver_started)
  {
    ESP_LOGD(TAG, "- Taking down network stack");
    // WifiDeregisterEventCallbacks();  //@@@ Moved to WifiDisconnect()
    esp_netif_destroy_default_wifi(esp_netif_interface_sta);
    esp_event_loop_delete_default();
    esp_wifi_deinit();
    WifiStatus.driver_started = false;
    WifiStatus.if_init = false;
  }
  else
  {
    ESP_LOGE(TAG, "- WifiDeinit() called, but state wrong:");
    ESP_LOGE(TAG, "  WifiStatus.driver_started = %d (wanted: true)", WifiStatus.driver_started);
  }
}

static int64_t lastWifiMillis = 0;
TaskHandle_t ntReconnectTaskHandler = NULL;

void WifiDisconnect()
{
  ESP_LOGI(TAG, "WifiDisconnect()");

  ESP_LOGD(TAG, "- Calling esp_wifi_disconnect()");
  esp_wifi_disconnect();
  // Reset lastWifiMillis to 0 so a reconnect is enabled right away
  // [see networkReconnectTask()]
  lastWifiMillis = 0;
  OperatingParameters.wifiConnected = false;
}

bool WifiReconnect(const char *hostname, const char *ssid, const char *pass)
{
  ESP_LOGI(TAG, "wifiReconnect()");

  if (!WifiStatus.driver_started)
  {
    ESP_LOGE(TAG, "- wifiReconnect() called, but state(s) wrong:");
    ESP_LOGE(TAG, "  WifiStatus.driver_started = %d (wanted: true)", WifiStatus.driver_started);
    // return false;  //@@@
  }

  //@@@
  WifiDeregisterEventCallbacks();

  if (OperatingParameters.wifiConnected)
  {
    ESP_LOGD(TAG, "- Disconnecting wifi - ");
    WifiDisconnect();
  }

  // if (WifiStatus.wifi_started)
  // {
  //   ESP_LOGD(TAG, "- Calling esp_wifi_stop()");
  //   esp_wifi_stop();
  //   WifiStatus.wifi_started = false;
  // }

  if (WifiStatus.driver_started)
  {
    ESP_LOGD(TAG, "- Calling WifiDeinit()");
    WifiDeinit();
  }

  //@@@
  // return false;

  ESP_LOGI(TAG, "- Restarting wifi");
  return (WifiStart(hostname, ssid, pass));
}

void networkReconnectTask(void *pvParameters)
{
  // Level set to 'W' (warn) since this is kind of significant
  ESP_LOGW(TAG, "Starting network reconnect task");

  if (wifiScanActive)
  {
    ESP_LOGE(TAG, "** ABORTING -- WIFI SSID SCAN ACTIVE **");
    ntReconnectTaskHandler = NULL;
    vTaskDelete(NULL);
    return;
  }

  if (millis() - lastWifiMillis < WIFI_CONNECT_INTERVAL)
  {
    ESP_LOGW(TAG, "- Wifi reconnect retry too soon ... delaying");
    ntReconnectTaskHandler = NULL;
    vTaskDelete(NULL);
    return; // Will never get here, but wanted to give the compiler an exit path
  }

  // Update timestamp for last wifi connect attempt
  lastWifiMillis = millis();

  if (WifiReconnect(OperatingParameters.DeviceName, WifiCreds.ssid, WifiCreds.password) == true)
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
#ifdef MATTER_ENABLED
  if (OperatingParameters.MatterStarted)
  {
    ESP_LOGW(TAG, "Network reconnect task -- Matter running ... Exiting");
    return;
  }
#endif
  if (ntReconnectTaskHandler)
  {
    ESP_LOGE(TAG, "Network reconnect task already running ... Exiting");
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
  rssi = std::min(std::max(2 * (rssi + 100.0), 0.0), 100.0);
  return (uint16_t)rssi;
}

uint16_t WifiSignal()
{
  long rssi;
  wifi_ap_record_t ap;
  if (WifiStatus.wifi_started && WifiStatus.Connected)
  {
    esp_wifi_sta_get_ap_info(&ap);
    rssi = ap.rssi;

    // Convert the rssi into a signal quality in percent
    return rssiToPercent(rssi);
  }
  else
  {
    return 0;
  }
}

char *WifiAddress()
{
  static char ipAddress[24];
  if (WifiStatus.wifi_started)
  {
    // tcpip_adapter_ip_info_t ipInfo; 
    // tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
    esp_netif_ip_info_t ipInfo;
    esp_netif_get_ip_info(esp_netif_interface_sta, &ipInfo);
    snprintf(ipAddress, sizeof(ipAddress), "%d.%d.%d.%d", 
      (int)(ipInfo.ip.addr & 0x000000ff),
      (int)((ipInfo.ip.addr & 0x0000ff00) >> 8),
      (int)((ipInfo.ip.addr & 0x00ff0000) >> 16),
      (int)((ipInfo.ip.addr & 0xff000000) >> 24)
    );
  }
  else
  {
    strcpy (ipAddress, "0.0.0.0");
  }
  return ipAddress;
}
