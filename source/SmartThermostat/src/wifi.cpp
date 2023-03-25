#include "thermostat.hpp"
#include <WiFi.h>
#include "wifi-credentials.h"

WiFiClient wclient;

bool wifiStart()
{
  int loop = 0;
  boolean result = false;
  const char *hostname = "thermostat";

  Serial.print("Connecting to ");
  Serial.print(ssid);
  // set up the wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  WiFi.setHostname(hostname);
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
