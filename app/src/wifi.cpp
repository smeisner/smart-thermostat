#include "thermostat.hpp"
#include <WiFi.h>
//#include "wifi-credentials.h"

WiFiClient wclient;
WIFI_CREDS WifiCreds;

/////////////////////////////////////////////////////////////////
// Support for TFT UI
//
// From: https://github.com/xpress-embedo/ESP32/blob/master/ConnectToWiFi/src/main.cpp
/////////////////////////////////////////////////////////////////

#define WIFI_MAX_SSID                       (6u)
static char wifi_dd_list[WIFI_MAX_SSID*20] = { 0 };

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
void WiFi_ScanSSID( void )
{
  String ssid_name;
  Serial.println("Start Scanning");
  int n = WiFi.scanNetworks();
  Serial.println("Scanning Done");
  if( n == 0 )
  {
    Serial.println("No Networks Found");
    memset( wifi_dd_list, 0x00, sizeof(wifi_dd_list) );
    strcpy( wifi_dd_list, "-- No SSIDs --" );
  }
  else
  {
    // I am restricting n to max WIFI_MAX_SSID value
    n = n <= WIFI_MAX_SSID ? n : WIFI_MAX_SSID;
    for (int i = 0; i < n; i++) 
    {
      if (ssid_name.indexOf(WiFi.SSID(i)) == -1)
      {
        if( i == 0 )
        {
          ssid_name = WiFi.SSID(i);
        }
        else
        {
          ssid_name = ssid_name + '\n';
          ssid_name = ssid_name + WiFi.SSID(i);
        }
      }
    }
    // clear the array, it might be possible that we coming after rescanning
    memset( wifi_dd_list, 0x00, sizeof(wifi_dd_list) );
    strcpy( wifi_dd_list, ssid_name.c_str() );
    Serial.println(wifi_dd_list);
  }
  Serial.println("Scanning Completed");
}


/////////////////////////////////////////////////////////////////
// End of Support for TFT UI
/////////////////////////////////////////////////////////////////

bool wifiStart(const char *hostname, const char *ssid, const char *pass)
{
  int loop = 0;
  boolean result = false;

  Serial.print("Connecting to ");
  Serial.print(ssid);
  // set up the wifi
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.setHostname(hostname);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  result = (WiFi.status() == WL_CONNECTED);
  while ((result == false) && (loop < 10)) // Wait for connection
  {
    delay (1500);
    Serial.print(".");
    result = (WiFi.status() == WL_CONNECTED);
    loop++;
  }

  if (result == true)
  {
    Serial.println("Ready");
    Serial.print("Host name: ");
    Serial.println (WiFi.getHostname());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println ("WiFi connection Failed!");
  }

  return result;
}

bool wifiConnected() { return (WiFi.status() == WL_CONNECTED); }

void WifiDisconnect() { Serial.printf ("** Disconnecting wifi **\n"); WiFi.disconnect(true, true); delay(100); }

int32_t lastWifiMillis = 0;

bool wifiReconnect(const char *hostname, const char *ssid, const char *pass)
{
  if ((WiFi.status() != WL_CONNECTED) && (millis() - lastWifiMillis >= WIFI_CONNECT_INTERVAL))
  {
    Serial.printf ("Reconnecting wifi - ");
    WiFi.disconnect(true, true);
    delay(500);

#if 1
    return (wifiStart(hostname, ssid, pass));
#else
    WiFi.mode(WIFI_MODE_NULL);
    WiFi.setHostname(hostname);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    lastWifiMillis = millis();

    vTaskDelay(200 / portTICK_PERIOD_MS);

    if (WiFi.status() == WL_CONNECTED)
    {
      lastWifiMillis = 0;
      Serial.printf ("Connected!\n");
      return true;
    } else {
      Serial.printf ("Failed\n");
      return false;
    }
#endif
  }
  return true;
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
  long rssi = WiFi.RSSI();
  // Convert the rssi into a signal quality in percent
  return rssiToPercent(rssi);
}

char *wifiAddress()
{
  static char ipAddress[24];
  snprintf(ipAddress, sizeof(ipAddress), "%s", WiFi.localIP().toString());
  return ipAddress;
}