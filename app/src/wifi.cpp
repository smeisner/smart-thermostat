#include "thermostat.hpp"
#include <WiFi.h>
//#include "wifi-credentials.h"

WiFiClient wclient;
WIFI_CREDS WifiCreds;

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
