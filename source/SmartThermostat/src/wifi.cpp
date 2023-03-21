#include "thermostat.hpp"
#include <WiFi.h>

WiFiClient wclient;

bool wifiStart()
{
  int loop = 0;
  boolean result = false;
  const char *ssid = "xxxxxxxxxx";
  const char *pass = "yyyyyyyyyy";
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
