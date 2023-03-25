#include "thermostat.hpp"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "version.h"

WebServer server(80);
const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
const char *host = "thermostat";

const char *hvacModeToString(HVAC_MODE mode)
{
    switch (mode)
    {
        case IDLE: return "Idle";
        case HEAT: return "Heat";
        case COOL: return "Cool";
        case FAN:  return "Fan";
        default:   return "Error";
    }
}

void handleRoot()
{
    char html[1024];
    snprintf(html, 1024, 
        "<html>\
        <head>\
            <meta http-equiv='refresh' content='5'/>\
            <title>Truly Smart Thermostat</title>\
            <style>\
                body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
            </style>\
        </head>\
        <body>\
        <h1>Thermostat operating parameters:</h1>\
        <pre><p style=\"font-size:16px; \">\
        <br>HVAC Mode:   %s\
        <br>Temperature: %0.1fF\
        <br>Humidity:    %0.1f%%\
        <br>\
        <br>Wifi:        %d%%\
        <br>Units:       %c\
        <br>Swing:       %d\
        <br>Set Temp:    %0.1fF\
        <br>Correction:  %0.1f\
        <br>Light:       %d\
        <br>Motion:      %s\
        </p></pre>\
        <form action='/upload'><input type='submit' value='FW Update' /></form>\
        <pre><p style=\"font-size:14px; \"><br>Firmware version: %s (%s)<br>%s</p></pre>\
        </body>\
        </html>",
        hvacModeToString(OperatingParameters.hvacMode),
        OperatingParameters.tempCurrent, OperatingParameters.humidCurrent,
        wifiSignal(),
        OperatingParameters.tempUnits, OperatingParameters.tempSwing,
        OperatingParameters.tempSet, OperatingParameters.tempCorrection,
        OperatingParameters.lightDetected, (OperatingParameters.motionDetected == true) ? "True" : "False",
        VERSION_STRING, VERSION_BUILD_DATE_TIME, VERSION_COPYRIGHT
    );

    server.send(200, "text/html", html);
}

void webInit()
{
  if (wifiConnected())
  {
    MDNS.begin(host);

    server.on("/", handleRoot);

    server.on("/upload", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", serverIndex);
    });

    server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
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

    Serial.printf("Ready! Open http://%s.local in your browser\n", host);
  } else {
    Serial.println("Wifi not connected!");
  }
}

void webPump()
{
  server.handleClient();
}