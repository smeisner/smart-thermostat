/*!
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
 * Notes:
 * 
 *  <@michael - include a comment here about where the XML code was fetched from and
 *    how it was adapted for use here. check sensors.cpp and tft.cpp for ideas>
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  18-Aug-2023: Michael Burke (michaelburke2000@gmail.com) - Added updating fields  <-- modify this to be correct! ...also, leave out the email if you don't want to share that
 * 
 */

#include "thermostat.hpp"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "version.h"
#include "web_ui.h"

WebServer server(80);
const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
const char* serverRedirect = "<meta http-equiv=\"refresh\" content=\"0; url='/'\" />";
static char html[2200];

void tempUp()
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
  server.send(200, "text/html", serverRedirect);
}
void tempDown()
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
  server.send(200, "text/html", serverRedirect);
}

void handleRoot()
{
  snprintf(html, sizeof(html), webUI,
    (int)OperatingParameters.tempSet, OperatingParameters.tempUnits,
    hvacModeToString(OperatingParameters.hvacSetMode),
    hvacModeToString(OperatingParameters.hvacOpMode),
    OperatingParameters.tempCurrent + OperatingParameters.tempCorrection,
    OperatingParameters.tempUnits,
    OperatingParameters.humidCurrent, OperatingParameters.lightDetected,
    (OperatingParameters.motionDetected == true) ? "True" : "False",
    OperatingParameters.tempUnits, OperatingParameters.tempSwing,
    OperatingParameters.tempCorrection,
    wifiSignal(), wifiAddress(), VERSION_STRING, VERSION_BUILD_DATE_TIME,
    VERSION_COPYRIGHT
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
