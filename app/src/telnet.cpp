#ifdef TELNET_ENABLED

// SPDX-License-Identifier: GPL-3.0-only
/*
 * telnet.cpp
 *
 * Creates a telnet server with a minimal CLI to handle basic commands. This
 * implementation is completely based on llibtelnet done by Sean Middleditch.
 * The Github repo for his code is at:
 *
 * https://github.com/seanmiddleditch/libtelnet
 *
 * The corresponding files (libtelnet.c and libtelnet.h) are directly copied,
 * and unmodified, from his repository.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 *
 * Notes:
 * - Currently, this implementation handles only 1 connected user. This
 *   means once someone is connected via telnet, they cannot be interrupted.
 *   If this feels like a potential security concern, that's because it is (DoS)!
 *
 * History
 *   4-Nov-2023: Steve Meisner (steve@meisners.net) - Initial version
 *   1-Dec-2023: Steve Meisner (steve@meisners.net) - Add remote telnet logging
 *  21-Jan-2024: Steve Meisner (steve@meisners.net) - Added telnet server reset
 *
 */

/**
 * Test the telnet functions.
 *
 * Perform a test using the telnet functions.
 * This code exports two new global functions:
 *
 * void telnet_listenForClients(void (*callback)(uint8_t *buffer, size_t size))
 * void telnet_sendData(uint8_t *buffer, size_t size)
 *
 * For additional details and documentation see:
 * * Free book on ESP32 - https://leanpub.com/kolban-ESP32
 *
 *
 * Neil Kolban <kolban1@kolban.com>
 *
 */
#include <stdlib.h> // Required for libtelnet.h
#include "libtelnet.h"
#include <lwip/sockets.h>
#include <errno.h>

#include "thermostat.hpp"
#include "esp_event.h"
#include "string.h"
#include "telnet.h"
#include "version.h"

// Implemented in web.cpp
float doTempUp(void);
float doTempDown(void);

static char tag[] = "telnet";

static vprintf_like_t OrigEsplogger = NULL;
static bool telnetLoggerActive = false;
static bool disconnectPending = false;

// The global tnHandle ... since we are only processing ONE telnet
// client at a time, this can be a global static.
static telnet_t *tnHandle;

// Task handle for the telnet service task
static TaskHandle_t telnetTaskHandle = NULL;

static void (*receivedDataCallback)(int sock, uint8_t *buffer, size_t size);

struct telnetUserData
{
  int sockfd;
};

void revertTelnetLogger()
{
  // Shutdown the telnet logger, if active;
  if (telnetLoggerActive == true)
  {
    telnetLoggerActive = false;
    esp_log_set_vprintf(OrigEsplogger);
  }
}

/**
 * Convert a telnet event type to its string representation.
 */
static char *eventToString(telnet_event_type_t type)
{
  switch (type)
  {
  case TELNET_EV_COMPRESS:
    return (char *)"TELNET_EV_COMPRESS";
  case TELNET_EV_DATA:
    return (char *)"TELNET_EV_DATA";
  case TELNET_EV_DO:
    return (char *)"TELNET_EV_DO";
  case TELNET_EV_DONT:
    return (char *)"TELNET_EV_DONT";
  case TELNET_EV_ENVIRON:
    return (char *)"TELNET_EV_ENVIRON";
  case TELNET_EV_ERROR:
    return (char *)"TELNET_EV_ERROR";
  case TELNET_EV_IAC:
    return (char *)"TELNET_EV_IAC";
  case TELNET_EV_MSSP:
    return (char *)"TELNET_EV_MSSP";
  case TELNET_EV_SEND:
    return (char *)"TELNET_EV_SEND";
  case TELNET_EV_SUBNEGOTIATION:
    return (char *)"TELNET_EV_SUBNEGOTIATION";
  case TELNET_EV_TTYPE:
    return (char *)"TELNET_EV_TTYPE";
  case TELNET_EV_WARNING:
    return (char *)"TELNET_EV_WARNING";
  case TELNET_EV_WILL:
    return (char *)"TELNET_EV_WILL";
  case TELNET_EV_WONT:
    return (char *)"TELNET_EV_WONT";
  case TELNET_EV_ZMP:
    return (char *)"TELNET_EV_ZMP";
  }
  return (char *)"Unknown type";
} // eventToString

/**
 * Send data to the telnet partner.
 */
void telnet_esp32_sendData(uint8_t *buffer, size_t size)
{
  if (tnHandle != NULL)
  {
    telnet_send(tnHandle, (char *)buffer, size);
  }
} // telnet_esp32_sendData

/**
 * Send a vprintf formatted output to the telnet partner.
 */
int telnet_esp32_vprintf(const char *fmt, va_list va)
{
  if (tnHandle == NULL)
  {
    return 0;
  }
  return telnet_vprintf(tnHandle, fmt, va);
} // telnet_esp32_vprintf

/**
 * Send a vprintf formatted output to the telnet partner.
 */
int telnet_esp32_printf(const char *fmt, ...)
{
  va_list va;
  int rs;

  va_start(va, fmt);
  rs = telnet_vprintf(tnHandle, fmt, va);
  va_end(va);

  return rs;
} // telnet_esp32_vprintf

/**
 * Telnet handler.
 */
static void telnetHandler(
    telnet_t *thisTelnet,
    telnet_event_t *event,
    void *userData)
{
  int rc;
  struct telnetUserData *telnetUserData = (struct telnetUserData *)userData;

  ESP_LOGD(tag, "telnet event: %s", eventToString(event->type));

  switch (event->type)
  {
  case TELNET_EV_SEND:
    if (!wifiConnected())
    {
      if (telnetUserData->sockfd != -1)
      {
        revertTelnetLogger();
        ESP_LOGW(tag, "wifi detected down! Dropping telnet connection");
        closesocket(telnetUserData->sockfd);
        telnetUserData->sockfd = -1;
        OperatingParameters.Errors.wifiErrors++;
      }
    }
    else
    {
      if (telnetUserData->sockfd != -1)
      {
        rc = send(telnetUserData->sockfd, event->data.buffer, event->data.size, 0);
        if (rc < 0)
        {
          revertTelnetLogger();
          ESP_LOGE(tag, "send: %d (%s)", errno, strerror(errno));
          closesocket(telnetUserData->sockfd);
          telnetUserData->sockfd = -1;
          OperatingParameters.Errors.telnetNetworkErrors++;
        }
      }
    }
    break;

  case TELNET_EV_DATA:
    ESP_LOGD(tag, "received data, len=%d", event->data.size);
    if (!wifiConnected())
    {
      if (telnetUserData->sockfd != -1)
      {
        ESP_LOGW(tag, "wifi detected down! Ignoring telnet data");
        closesocket(telnetUserData->sockfd);
        telnetUserData->sockfd = -1;
        OperatingParameters.Errors.wifiErrors++;
      }
    }
    else
    {
      /**
       * Here is where we would want to handle newly received data.
       * The data receive is in event->data.buffer of size
       * event->data.size.
       */
      if (telnetUserData->sockfd != -1)
        if (receivedDataCallback != NULL)
        {
          receivedDataCallback(telnetUserData->sockfd, (uint8_t *)event->data.buffer, (size_t)event->data.size);
          // Clear the prior contents
          memset((void *)(event->data.buffer), 0, sizeof((void *)(event->data.buffer)));
          event->data.size = 0;
        }
    }
    break;

  default:
    break;

  } // End of switch event type
} // myTelnetHandler

static void doTelnet(int partnerSocket)
{
  // ESP_LOGD(tag, ">> doTelnet");
  static const telnet_telopt_t my_telopts[] = {
      {TELNET_TELOPT_ECHO, TELNET_WILL, TELNET_DONT},
      {TELNET_TELOPT_TTYPE, TELNET_WILL, TELNET_DONT},
      {TELNET_TELOPT_COMPRESS2, TELNET_WONT, TELNET_DO},
      {TELNET_TELOPT_ZMP, TELNET_WONT, TELNET_DO},
      {TELNET_TELOPT_MSSP, TELNET_WONT, TELNET_DO},
      {TELNET_TELOPT_BINARY, TELNET_WILL, TELNET_DO},
      {TELNET_TELOPT_NAWS, TELNET_WILL, TELNET_DONT},
      {-1, 0, 0}};

  struct telnetUserData *pTelnetUserData = (struct telnetUserData *)malloc(sizeof(struct telnetUserData));
  pTelnetUserData->sockfd = partnerSocket;
  disconnectPending = false;

  tnHandle = telnet_init(my_telopts, telnetHandler, 0, pTelnetUserData);

  uint8_t buffer[1024];

  struct timeval to;
  to.tv_sec = 1;
  to.tv_usec = 0;
  if (setsockopt(pTelnetUserData->sockfd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)) < 0)
  {
    ESP_LOGE(tag, "Unable to set read timeout on socket!");
    // return false;
  }

  while (/*wifiConnected() && */ (pTelnetUserData->sockfd != -1))
  {
    if ((disconnectPending) /*|| (!wifiConnected()) */)
    {
      ESP_LOGI(tag, "telnet disconect requested");
      disconnectPending = false;
      revertTelnetLogger();
      closesocket(pTelnetUserData->sockfd);
      pTelnetUserData->sockfd = -1;
      break;
    }
    // ESP_LOGD(tag, "waiting for data");
    ssize_t len = recv(partnerSocket, (char *)buffer, sizeof(buffer), 0);

    if ((len == 0) && (telnetLoggerActive == true))
    {
      ESP_LOGI(tag, "Terminating logging");
      break;
    }

    if ((len > sizeof(buffer)) || (len < 0)) //(len != EWOULDBLOCK))
    {
      // Got an error receiving data; just delay and then wait for more
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    if (/*wifiConnected() &&*/ (pTelnetUserData->sockfd != -1))
    {
      ESP_LOGD(tag, "received %d bytes", len);

      // Disable timeout on socket to allow for interaction (like doing a config)
      to.tv_sec = 0;
      setsockopt(pTelnetUserData->sockfd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));

      // Process the received data
      telnet_recv(tnHandle, (char *)buffer, len);

      // Reenable the socket timeout option
      to.tv_sec = 1;
      setsockopt(pTelnetUserData->sockfd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
    }
    telnet_esp32_printf("> ");
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  revertTelnetLogger();
  ESP_LOGI(tag, "Telnet partner finished");
  telnet_free(tnHandle);
  tnHandle = NULL;
} // doTelnet

int serverSocket=0;
/**
 * Listen for telnet clients and handle them.
 */
void telnet_esp32_listenForClients(void (*callbackParam)(int sock, uint8_t *buffer, size_t size))
{
  // ESP_LOGD(tag, ">> telnet_listenForClients");
  receivedDataCallback = callbackParam;
  serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  BaseType_t xTrueValue = pdTRUE;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (void *)&xTrueValue, sizeof(xTrueValue));

  struct sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddr.sin_port = htons(23);

  int rc = bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
  if (rc < 0)
  {
    ESP_LOGE(tag, "bind: %d (%s)", errno, strerror(errno));
    OperatingParameters.Errors.telnetNetworkErrors++;
    return;
  }

  rc = listen(serverSocket, 5);
  if (rc < 0)
  {
    ESP_LOGE(tag, "listen: %d (%s)", errno, strerror(errno));
    OperatingParameters.Errors.telnetNetworkErrors++;
    return;
  }

  while (1)
  {
    socklen_t len = sizeof(serverAddr);
    rc = accept(serverSocket, (struct sockaddr *)&serverAddr, &len);
    if (rc < 0)
    {
      ESP_LOGE(tag, "accept: %d (%s)", errno, strerror(errno));
      OperatingParameters.Errors.telnetNetworkErrors++;
      return;
    }
    int partnerSocket = rc;

    ESP_LOGI(tag, "We have a new client connection!");
    // This call is blocking, so only 1 telnet session at a time!
    doTelnet(partnerSocket);
  }
} // listenForNewClient

void telnet_esp32_CloseSocket()
{
  if (serverSocket == 0)
  {
    return;
  }

  int rc = closesocket(serverSocket);
  if (rc < 0)
  {
    ESP_LOGE(tag, "closesocket: %d (%s)", errno, strerror(errno));
    OperatingParameters.Errors.telnetNetworkErrors++;
    return;
  }
}

void terminateTelnetSession()
{
  // On first invocation, this routine will only close the
  // active telnet session, if there is one...then return. On
  // the next invocation, since the first closed the active
  // session, the task that handles incoming connect requests
  // will be terminated and restarted.

  ESP_LOGI(tag, "Telnet session termination requested");

  if ((tnHandle == NULL) && (telnetTaskHandle == NULL))
  {
    ESP_LOGW(tag, "No action taken - no telnet connection and telnet task not started");
    return;
  }

  if (tnHandle != NULL)
  {
    ESP_LOGW(tag, "Active telnet session -- waiting for it to terminate");

    revertTelnetLogger();

    // Set the flag to start exiting telnet task
    disconnectPending = true;
    while (tnHandle != NULL)
      vTaskDelay(pdMS_TO_TICKS(50));

    // First step completed; Stop active telnet session.
    // On a subsequent call, the socket will be closed and task restarted
    return;
  }

  // Close the listener socket
  telnet_esp32_CloseSocket();

  // telnet session is now aborted and listening socket is closed. Now restart the task.
  if (telnetTaskHandle != NULL)
  {
    ESP_LOGD(tag, "Active telnet sessions stopped and socket closed. Now stopping telnet RTOS task.");
    vTaskDelete(telnetTaskHandle);
    telnetTaskHandle = NULL;
  }

  ESP_LOGI(tag, "Starting new telnet service instance");
  vTaskDelay(pdMS_TO_TICKS(125));
  telnetStart();
}

void DisplayErrors()
{
  telnet_esp32_printf("Current Error Counts:\n");
  telnet_esp32_printf("--------------------------------------------------------\n");

  telnet_esp32_printf("System errors: %d\n", OperatingParameters.Errors.systemErrors);
  telnet_esp32_printf("Hardware errors: %d\n", OperatingParameters.Errors.hardwareErrors);
  telnet_esp32_printf("Wifi errors: %d\n", OperatingParameters.Errors.wifiErrors);

#ifdef MQTT_ENABLED
  telnet_esp32_printf("MQTT Connect errors: %d\n", OperatingParameters.Errors.mqttConnectErrors);
  telnet_esp32_printf("MQTT Protocol errors: %d\n", OperatingParameters.Errors.mqttProtocolErrors);
#endif
#ifdef MATTER_ENABLED
  telnet_esp32_printf("Matter Connect errors: %d\n", OperatingParameters.Errors.matterConnectErrors);
#endif
#ifdef TELNET_ENABLED
  telnet_esp32_printf("Telnet network errors: %d\n", OperatingParameters.Errors.telnetNetworkErrors);
#endif
}

void DisplayStatus()
{
  telnet_esp32_printf("Current Status:\n");
  telnet_esp32_printf("--------------------------------------------------------\n");

  telnet_esp32_printf("Firmware version: %s\n", VersionString);
  telnet_esp32_printf("Firmware build date: %s\n", VersionBuildDateTime);

  telnet_esp32_printf("Device name: %s\n", OperatingParameters.DeviceName);
  telnet_esp32_printf("Friendly name: %s\n", OperatingParameters.FriendlyName);

  {
    int64_t uptime = millis();
    telnet_esp32_printf("Uptime: %d days, %02d:%02d:%02d\n",
                        (int)(uptime / (1000L * 60L * 60L * 24L)), // days
                        (int)(uptime / (1000L * 60L * 60L)) % 24,  // hours
                        (int)(uptime / (1000L * 60L)) % 60,        // mins
                        (int)(uptime / 1000L) % 60);               // seconds
  }

  telnet_esp32_printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                      OperatingParameters.mac[0],
                      OperatingParameters.mac[1],
                      OperatingParameters.mac[2],
                      OperatingParameters.mac[3],
                      OperatingParameters.mac[4],
                      OperatingParameters.mac[5]);
  telnet_esp32_printf("Wifi SSID: %s\n", WifiCreds.ssid);
  telnet_esp32_printf("Wifi connected: %s\n", OperatingParameters.wifiConnected ? "Yes" : "No");
  telnet_esp32_printf("Wifi signal: %d%%\n", wifiSignal());
  telnet_esp32_printf("Wifi IP address: %s\n", wifiAddress());

  {
    struct tm local_time;
    char buffer[16];

    getLocalTime(&local_time, 1);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &local_time);
    telnet_esp32_printf("Current time: %s", buffer);
  }
  telnet_esp32_printf("    Timezone: %s\n", OperatingParameters.timezone);

#ifdef MQTT_ENABLED
  telnet_esp32_printf("MQTT Enabled: %s\n", (OperatingParameters.MqttEnabled) ? "Yes" : "No");
  telnet_esp32_printf("MQTT Connected: %s\n", (OperatingParameters.MqttConnected) ? "Yes" : "No");
  telnet_esp32_printf("MQTT Broker: %s  Port: %d\n", OperatingParameters.MqttBrokerHost, OperatingParameters.MqttBrokerPort);
  telnet_esp32_printf("MQTT Username:  %s\n", OperatingParameters.MqttBrokerUsername);
  telnet_esp32_printf("MQTT Password:  %s\n", OperatingParameters.MqttBrokerPassword);
#endif

#ifdef MATTER_ENABLED
  telnet_esp32_printf("Matter Enabled: %s\n", (OperatingParameters.MatterEnabled) ? "Yes" : "No");
  telnet_esp32_printf("Matter Started: %s\n", (OperatingParameters.MatterStarted) ? "Yes" : "No");
#endif

  telnet_esp32_printf("Current temp: %.1f %c (Correction: %.1f)\n",
                      OperatingParameters.tempCurrent + OperatingParameters.tempCorrection,
                      OperatingParameters.tempUnits,
                      OperatingParameters.tempCorrection);
  telnet_esp32_printf("Current humidity: %.1f%% (Correction: %.1f)\n",
                      OperatingParameters.humidCurrent + OperatingParameters.humidityCorrection,
                      OperatingParameters.humidityCorrection);
  telnet_esp32_printf("Target temp: %.1f %c\n", OperatingParameters.tempSet);
  telnet_esp32_printf("Swing temp: %.1f %c\n", OperatingParameters.tempSwing);

  telnet_esp32_printf("Light detected: %d\n", OperatingParameters.lightDetected);
  telnet_esp32_printf("Motion detected: %s\n", OperatingParameters.motionDetected ? "Yes" : "No");
  telnet_esp32_printf("Display sleep time: %d\n", OperatingParameters.thermostatSleepTime);
  telnet_esp32_printf("Touch screen beep: %s\n", OperatingParameters.thermostatBeepEnable ? "Enabled" : "Disabled");

  telnet_esp32_printf("Current HVAC mode set: %s (Currently: %s)\n",
                      hvacModeToString(OperatingParameters.hvacSetMode),
                      hvacModeToString(OperatingParameters.hvacOpMode));

  telnet_esp32_printf("HVAC modes enabled:\n");
  telnet_esp32_printf("  Heat: %s  Cool: %s  Fan: %s\n",
                      "True", OperatingParameters.hvacCoolEnable ? "True" : "False",
                      OperatingParameters.hvacFanEnable ? "True" : "False");
  telnet_esp32_printf("  2-stage heating enabled: %s\n",
                      OperatingParameters.hvac2StageHeatEnable ? "True" : "False");
  telnet_esp32_printf("  Reversing valve (heat pumps): %s\n",
                      OperatingParameters.hvacReverseValveEnable ? "True" : "False");
}

void doConfiguration(int sock)
{
  char buffer[128];
  ssize_t len;

  telnet_esp32_printf("Configuration\n");

  telnet_esp32_printf("Device name [%s]: ", OperatingParameters.DeviceName);
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    strcpy(OperatingParameters.DeviceName, buffer);

  telnet_esp32_printf("Friendly name [%s]: ", OperatingParameters.FriendlyName);
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    strcpy(OperatingParameters.FriendlyName, buffer);

  telnet_esp32_printf("WIFI Network name [%s]: ", WifiCreds.ssid);
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    strcpy(WifiCreds.ssid, buffer);

  telnet_esp32_printf("WIFI Password or PSK [%s]: ", WifiCreds.password);
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    strcpy(WifiCreds.password, buffer);

  telnet_esp32_printf("Swing temperature [%.1f]: ", OperatingParameters.tempSwing);
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    OperatingParameters.tempSwing = atof(buffer);

  telnet_esp32_printf("Temperature correction [%.1f]: ", OperatingParameters.tempCorrection);
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    OperatingParameters.tempCorrection = atof(buffer);

  telnet_esp32_printf("Humidity correction [%.1f]: ", OperatingParameters.humidityCorrection);
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    OperatingParameters.humidityCorrection = atof(buffer);

  telnet_esp32_printf("Display Sleep time [%d]: ", OperatingParameters.thermostatSleepTime);
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    OperatingParameters.thermostatSleepTime = atoi(buffer);

  telnet_esp32_printf("Timezone [%s]: ", OperatingParameters.timezone);
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
  {
    int i = 0;
    while ((strcmp(gmt_timezones[i], (const char *)buffer) != 0) && (i < 24))
      i++;
    if (i < 24)
    {
      OperatingParameters.timezone_sel = i;
      OperatingParameters.timezone = (char *)(gmt_timezones[OperatingParameters.timezone_sel]);
    }
    else
    {
      telnet_esp32_printf("Invalid timezone (example GMT+10)\n");
    }
  }

  telnet_esp32_printf("Touchscreen Beep [%s]: ", (OperatingParameters.thermostatBeepEnable) ? "Yes" : "No");
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    if (lwip_stricmp("yes", buffer) == 0)
      OperatingParameters.thermostatBeepEnable = true;
    else
      OperatingParameters.thermostatBeepEnable = false;

      //@@@	HVAC modes
      // 2-stage heat
      // Reverse valve
      // timezone

#ifdef MATTER
  telnet_esp32_printf("Enable Matter [%s]: ", (OperatingParameters.Matter) ? "Yes" : "No");
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    if (lwip_stricmp("yes", buffer) == 0)
      OperatingParameters.Matter = true;
    else
      OperatingParameters.Matter = false;
#endif

#ifdef MQTT_ENABLED
  telnet_esp32_printf("Enable MQTT [%s]: ", (OperatingParameters.MqttEnabled) ? "Yes" : "No");
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    if (lwip_stricmp("yes", buffer) == 0)
      OperatingParameters.MqttEnabled = true;
    else
      OperatingParameters.MqttEnabled = false;
  if (OperatingParameters.MqttEnabled)
  {
    telnet_esp32_printf("MQTT Broker Hostname [%s]: ", OperatingParameters.MqttBrokerHost);
    len = recv(sock, buffer, sizeof(buffer), 0);
    len -= 2;
    buffer[len] = '\0';
    if ((len) && (len < sizeof(buffer)))
      strcpy(OperatingParameters.MqttBrokerHost, buffer);

    telnet_esp32_printf("MQTT Broker Port [%d]: ", OperatingParameters.MqttBrokerPort);
    len = recv(sock, buffer, sizeof(buffer), 0);
    len -= 2;
    buffer[len] = '\0';
    if ((len) && (len < sizeof(buffer)))
      OperatingParameters.MqttBrokerPort = atoi(buffer);

    telnet_esp32_printf("MQTT Broker Username [%s]: ", OperatingParameters.MqttBrokerUsername);
    len = recv(sock, buffer, sizeof(buffer), 0);
    len -= 2;
    buffer[len] = '\0';
    if ((len) && (len < sizeof(buffer)))
      strcpy(OperatingParameters.MqttBrokerUsername, buffer);

    telnet_esp32_printf("MQTT Broker Password [%s]: ", OperatingParameters.MqttBrokerPassword);
    len = recv(sock, buffer, sizeof(buffer), 0);
    len -= 2;
    buffer[len] = '\0';
    if ((len) && (len < sizeof(buffer)))
      strcpy(OperatingParameters.MqttBrokerPassword, buffer);
  }
#endif

  telnet_esp32_printf("Save changes? [no]: ");
  len = recv(sock, buffer, sizeof(buffer), 0);
  len -= 2;
  buffer[len] = '\0';
  if ((len) && (len < sizeof(buffer)))
    if (lwip_strnicmp("yes", buffer, len) == 0)
    {
      telnet_esp32_printf("Resetting NVS\n");
      // nvs_flash_init();
      clearNVS();
      /* Rewrite the wifi credentials after resetting NVS storage */
      telnet_esp32_printf("Rewriting wifi credentials\n");
      setWifiCreds();
      telnet_esp32_printf("Saving thermostat config\n");
      updateThermostatParams();
    }

  telnet_esp32_printf("Config complete. If changes made, consider restarting (with 'Reboot')\n");
}

// All valid commands
typedef enum
{
  HELP = 0,
  CONFIG,
  TEMP_UP,
  TEMP_DOWN,
  TEMP_SET,
  MODE_SET,
  MONITOR_LOG,
  STATUS,
  ERROR_COUNTS,
  REBOOT,
  QUIT,
  UNKNOWN
} CMD;

const char *cmdStrings[] = {"HELP", "CONFIG", "UP", "DOWN", "TEMP", "MODE", "MONITOR", "STATUS", "ERROR", "REBOOT", "QUIT", NULL};

#define min(x, y) ((x > y) ? y : x)

static CMD lookupCommnd(char *buffer, size_t len)
{
  int n = 0;
  while (cmdStrings[n] != NULL)
  {
    if (lwip_strnicmp(cmdStrings[n], buffer, min(strlen(cmdStrings[n]), len)) == 0)
    {
      ESP_LOGI(tag, "Received telnet command: %s", cmdStrings[n]);
      return (CMD)n;
    }
    n++;
  }
  return UNKNOWN;
}

static int telnetLogger(const char *fmt, va_list list)
{
  static char telnetBuffer[256];

  if (telnetLoggerActive == false)
  {
    printf("telnetLogger: Telnet logger is not enabled!! Restoring original\n");
    if (OrigEsplogger == NULL)
    {
      printf("telnetLogger: No original logger defined - Restarting...\n");
      esp_restart();
    }
    esp_log_set_vprintf(OrigEsplogger);
    return ESP_OK;
  }

  OrigEsplogger(fmt, list);

  int res = vsprintf(telnetBuffer, fmt, list);
  telnet_esp32_printf(telnetBuffer);

  return res;
}

static void recvData(int sock, uint8_t *buffer, size_t _size)
{
  size_t size = _size - 2; // Every line will end with \r\n
  char *ptr;
  int i;

  if (size == 0)
  {
    if (telnetLoggerActive)
    {
      telnet_esp32_printf("Log monitoring disabled\n");
      revertTelnetLogger();
      ESP_LOGI(tag, "Log monitoring via telnet disabled");
    }
    return;
  }

  if (!wifiConnected())
  {
    ESP_LOGW(tag, "Wifi down! Aborting receive request!");
    OperatingParameters.Errors.wifiErrors++;
    return;
  }

  buffer[size] = '\0';

  ESP_LOGD(tag, "We received: %.*s", size, buffer);
  switch (lookupCommnd((char *)buffer, size))
  {
  case HELP:
    telnet_esp32_printf("Valid commands:\n");
    telnet_esp32_printf("  Config:      Change configuration\n");
    telnet_esp32_printf("  Help:        This!\n");
    telnet_esp32_printf("  Up:          Increase set temp\n");
    telnet_esp32_printf("  Down:        Decrease set temp\n");
    telnet_esp32_printf("  Temp <temp>: Set arbitrary temp\n");
    telnet_esp32_printf("  Mode <mode>: Set operating mode\n");
    telnet_esp32_printf("  Monitor:     Monitor log output\n");
    telnet_esp32_printf("  Status:      Dump status counters\n");
    telnet_esp32_printf("  Error:       Dump error counters\n");
    telnet_esp32_printf("  Quit:        Close telnet session\n");
    telnet_esp32_printf("  Reboot:      Reboot the ESP32\n");
    break;
  case CONFIG:
    doConfiguration(sock);
    break;
  case TEMP_UP:
    telnet_esp32_printf("Temperature up\n");
    telnet_esp32_printf("New set temp: %.1f\n", doTempUp());
    break;
  case TEMP_DOWN:
    telnet_esp32_printf("Temperature down\n");
    telnet_esp32_printf("New set temp: %.1f\n", doTempDown());
    break;
  case TEMP_SET:
    ptr = strchr((char *)buffer, (int)' ');
    ptr++; // Move past ' '
    i = atoi(ptr);
    telnet_esp32_printf("Set temperature to %d\n", i);
    updateHvacSetTemp((float)i);
    break;
  case MODE_SET:
  {
    HVAC_MODE mode = ERROR;
    telnet_esp32_printf("Set mode\n");
    ptr = strchr((char *)buffer, (int)' ');
    ptr++; // Move past ' '
    ptr[0] = toupper(ptr[0]);
    ESP_LOGI(tag, "Looking up %s", ptr);
    mode = strToHvacMode(ptr);
    if (mode == ERROR)
    {
      telnet_esp32_printf("Invalid mode: %s\n", ptr);
    }
    else
    {
      updateHvacMode(mode);
      telnet_esp32_printf("Mode now set to: %s\n", ptr);
    }
  }
  break;
  case MONITOR_LOG:
    if (telnetLoggerActive == false)
    {
      telnet_esp32_printf("Enable log monitoring (CR to exit)\n");
      ESP_LOGI(tag, "Log monitoring via telnet enabled");
      telnetLoggerActive = true;
      OrigEsplogger = esp_log_set_vprintf(telnetLogger);
    }
    else
    {
      telnet_esp32_printf("Error: log monitoring already active\n");
    }
    break;
  case STATUS:
    DisplayStatus();
    break;
  case ERROR_COUNTS:
    telnet_esp32_printf("Dump error stats\n");
    DisplayErrors();
    break;
  case REBOOT:
    telnet_esp32_printf("Restarting the ESP32...\n");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    break;
  case QUIT:
    telnet_esp32_printf("Quiting telnet session\n");
    ESP_LOGI(tag, "Telnet session terminated");
    disconnectPending = true;
    break;
  case UNKNOWN:
  default:
    ESP_LOGW(tag, "Unknown command: %.*s", size, buffer);
    telnet_esp32_printf("Unknown command: %.*s\n", size, buffer);
    break;
  }
}

static void telnetTask(void *data)
{
  ESP_LOGI(tag, "Start telnetTask()");
  telnet_esp32_listenForClients(recvData);

  ESP_LOGI(tag, "Completing telnetTask()");
  vTaskDelete(NULL);
}

esp_err_t telnetStart()
{
  // xTaskCreatePinnedToCore (&telnetTask, "telnetTask", 8048, NULL, 5, NULL, 0);
  xTaskCreate(
      &telnetTask,
      "TelnetTask",
      8048,
      NULL,
      tskIDLE_PRIORITY,
      &telnetTaskHandle);
  return ESP_OK;
}

#endif // #ifdef TELNET_ENABLED
