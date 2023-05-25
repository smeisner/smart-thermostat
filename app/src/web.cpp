#include "thermostat.hpp"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "version.h"

WebServer server(80);
const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
const char* serverRedirect = "<meta http-equiv=\"refresh\" content=\"0; url='/'\" />";
static char html[1600];

void tempUp()
{
    OperatingParameters.tempSet += 1.0;
    eepromUpdateHvacSetTemp();
    server.send(200, "text/html", serverRedirect);
}
void tempDown()
{
    OperatingParameters.tempSet -= 1.0;
    eepromUpdateHvacSetTemp();
    server.send(200, "text/html", serverRedirect);
}

void handleRoot()
{
    snprintf(html, sizeof(html), 
"<html><head><meta http-equiv='refresh' content='5'/><title>Truly Smart Thermostat</title><style>\
body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
</style></head><body><h1>Thermostat operating parameters:</h1>\
<pre><p style=\"font-size:16px; \">\
<br>HVAC Mode:   %s (Set: %s)\
<br>Temperature: %0.1f%c\
<br>Humidity:    %0.1f%%\
<br>\
<br>Wifi:        %d%%  [IP: %s]\
<br>Units:       %c\
<br>Swing:       %0.1f\
<br>Set Temp:    %d.%d%c\
<br>Correction:  %0.1f\
<br>Light:       %d\
<br>Motion:      %s</p></pre>\
<form action='/upload'><input type='submit' value='FW Update' /></form>\
<button onclick=\"window.location.href = '/clearFirmware';\">CLEAR Config</button>&nbsp\
<p></p>Set Temp: <button onclick=\"window.location.href = '/tempDown';\">TEMP &laquo;</button>&nbsp\
<button onclick=\"window.location.href = '/tempUp';\">TEMP &raquo;</button>\
<p></p>\
HVAC Mode: <button onclick=\"window.location.href = '/hvacModeOff';\">OFF</button>&nbsp\
<button onclick=\"window.location.href = '/hvacModeAuto';\">AUTO</button>&nbsp\
<button onclick=\"window.location.href = '/hvacModeHeat';\">HEAT</button>&nbsp\
<button onclick=\"window.location.href = '/hvacModeCool';\">COOL</button>&nbsp\
<button onclick=\"window.location.href = '/hvacModeFan';\">FAN</button>\
<pre><p style=\"font-size:14px; \"><br>Firmware version: %s (%s)<br>%s</p></pre>\
<p></p><a href=\"https://github.com/smeisner/smart-thermostat\" target=\"_blank\">Smart Thermostat Project on GitHub</a>\
</body></html>",
        hvacModeToString(OperatingParameters.hvacOpMode), hvacModeToString(OperatingParameters.hvacSetMode),
        (OperatingParameters.tempUnits == 'C') ? degFtoC(OperatingParameters.tempCurrent + OperatingParameters.tempCorrection): (OperatingParameters.tempCurrent + OperatingParameters.tempCorrection), 
        OperatingParameters.tempUnits, 
        OperatingParameters.humidCurrent, wifiSignal(), wifiAddress(),
        OperatingParameters.tempUnits, OperatingParameters.tempSwing,
        tempOut(OperatingParameters.tempSet), degCfrac(OperatingParameters.tempSet),
        OperatingParameters.tempUnits, OperatingParameters.tempCorrection,
        OperatingParameters.lightDetected, (OperatingParameters.motionDetected == true) ? "True" : "False",
        VERSION_STRING, VERSION_BUILD_DATE_TIME, VERSION_COPYRIGHT
    );

    server.send(200, "text/html", html);
}

void webInit(void * parameter)
{
  if (!wifiConnected())
    Serial.printf ("***  WARNING: Starting web server while wifi down!!  *****\n");

  MDNS.begin(WifiCreds.hostname);

  server.on("/", handleRoot);
  server.on("/tempUp", tempUp);
  server.on("/tempDown", tempDown);

  server.on("/clearFirmware", HTTP_GET, []() {
      clearNVS();
      server.send(200, "text/html", serverRedirect);
  });

  server.on("/hvacModeOff", HTTP_GET, []() {
      OperatingParameters.hvacSetMode = OFF;
      eepromUpdateHvacSetMode();
      server.send(200, "text/html", serverRedirect);
  });

  server.on("/hvacModeAuto", HTTP_GET, []() {
      OperatingParameters.hvacSetMode = AUTO;
      eepromUpdateHvacSetMode();
      server.send(200, "text/html", serverRedirect);
  });

  server.on("/hvacModeHeat", HTTP_GET, []() {
      OperatingParameters.hvacSetMode = HEAT;
      eepromUpdateHvacSetMode();
      server.send(200, "text/html", serverRedirect);
  });

  server.on("/hvacModeCool", HTTP_GET, []() {
      OperatingParameters.hvacSetMode = COOL;
      eepromUpdateHvacSetMode();
      server.send(200, "text/html", serverRedirect);
  });

  server.on("/hvacModeFan", HTTP_GET, []() {
      OperatingParameters.hvacSetMode = FAN;
      eepromUpdateHvacSetMode();
      server.send(200, "text/html", serverRedirect);
  });

  server.on("/upload", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin()) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    } else {
      Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
    }
  });
  server.begin();
  MDNS.addService("http", "tcp", 80);

  Serial.printf("Ready! Open http://%s.local in your browser\n", WifiCreds.hostname);

  Serial.printf ("Starting web server task\n");
  for (;;) { server.handleClient(); delay(1); }

}

void webCreateTask()
{
  xTaskCreate (
      webInit,
      "Web Server",
      4096,
      NULL,
      tskIDLE_PRIORITY-1,
      NULL
  );
}
