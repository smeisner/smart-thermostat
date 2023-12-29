#ifdef MQTT_ENABLED

// SPDX-License-Identifier: GPL-3.0-only
/*
 * mqtt.cpp
 *
 * Implement support for MQTT and Home Assistant. This module connects to the MQTT broker
 * (usually running as part of Home Assistant) and provides a discovery MQTT publish. Then
 * periodically, provide status updates via MQTT publishes.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * Notes:
 * 
 * - This module still requires a good deal of code review & cleanup
 *
 * History
 *  11-Oct-2023: Steve Meisner (steve@meisners.net) - Initial version
 *  16-Oct-2023: Steve Meisner (steve@meisners.net) - Add support for friendly name & many operational improvements
 *  15-Dec-2023: Steve Meisner (steve@meisners.net) - Enable auto reconnect
 *  29-Dec-2023: Steve Meisner (steve@meisners.net) - Add motion sensor as MQTT sensor device
 *
 */

#include <string> // for string class
#include "thermostat.hpp"
#include "version.h"
#include <stdint.h>
#include <stddef.h>
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include <ArduinoJson.h>

static const char *TAG = "MQTT";

// Variable used for MQTT Discovery
const char*         g_deviceModel = "Truly Smart Thermostat";                 // Hardware Model
const char*         g_swVersion = VersionString;                             // Firmware Version
const char*         g_manufacturer = "Steve Meisner";                         // Manufacturer Name
std::string         g_deviceName; // = OperatingParameters.DeviceName;            // Device Name
std::string         g_friendlyName;
std::string         g_mqttStatusTopic; // = "SmartThermostat/" + g_deviceName;    // MQTT Topic
std::string         g_mqttSensorStatusTopic;

void MqttSubscribeTopic(esp_mqtt_client_handle_t client, std::string topic);

struct CustomWriter {
  std::string str;

  size_t write(uint8_t c)
  {
    str.append(1, static_cast<char>(c));
    return 1;
  }

  size_t write(const uint8_t *s, size_t n)
  {
    str.append(reinterpret_cast<const char *>(s), n);
    return n;
  }
};

#define MQTT_EVENT_CONNECTED_BIT BIT0
#define MQTT_EVENT_DISCONNECTED_BIT BIT1
#define MQTT_EVENT_PUB_BIT BIT2
#define MQTT_ERROR_BIT BIT3
static EventGroupHandle_t s_mqtt_event_group = NULL;
const TickType_t xTicksToWait = 11000 / portTICK_PERIOD_MS;

static void MqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            OperatingParameters.MqttConnected = true;
            xEventGroupSetBits(s_mqtt_event_group, MQTT_EVENT_CONNECTED_BIT);
            // Resubscribe to topics when connection (re) established
            ESP_LOGI(TAG, "Subscribing to topic %s/set/#", OperatingParameters.DeviceName);
            MqttSubscribeTopic(client, g_deviceName + "/set/#");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            OperatingParameters.MqttConnected = false;
            xEventGroupSetBits(s_mqtt_event_group, MQTT_EVENT_DISCONNECTED_BIT);
#ifdef TELNET_ENABLED
            terminateTelnetSession();
#endif
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            // xEventGroupSetBits(s_mqtt_event_group, MQTT_EVENT_PUB_BIT);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            OperatingParameters.MqttConnected = false;
            xEventGroupSetBits(s_mqtt_event_group, MQTT_ERROR_BIT);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            if (strnstr(event->topic, "/set/mode", event->topic_len) != NULL)
            {
                char m[event->data_len + 1];
                strncpy (m, event->data, event->data_len);
                m[event->data_len] = '\0';
                // Make first letter uppercase to match our string representation
                m[0] = toupper(m[0]);
                ESP_LOGI(TAG, "Looking up %s", m);
    			HVAC_MODE mode = strToHvacMode(m);
			    if (mode == ERROR)
                    ESP_LOGW(TAG, "Mode %s invalid", m);
                else
                    updateHvacMode (mode);
            }
            else if (strnstr(event->topic, "/set/temp", event->topic_len) != NULL)
            {
                if (strlen(event->data) > 0 && atof(event->data) != 0)
                {
                    updateHvacSetTemp (atof(event->data));
                }
                else
                {
                    ESP_LOGE(TAG, "Received invalid string for set temp: \"%.*s\"", event->data_len, event->data);
                    OperatingParameters.Errors.mqttProtocolErrors++;
                }
            }
            else if (strnstr(event->topic, "fan", event->topic_len) != NULL)
            {
                // if (strlen(event->data) > 0)
                if (event->data_len > 0)
                {
                    if (strncmp("on", event->data, event->data_len) == 0)
                    {
                        updateHvacMode (FAN_ONLY);
                    }
                    else
                    {
                        if (OperatingParameters.hvacOpMode == FAN_ONLY)
                        {
                            updateHvacMode (OFF);
                        }
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Received invalid string for set fan mode: \"%.*s\"", event->data_len, event->data);
                    OperatingParameters.Errors.mqttProtocolErrors++;
                }
            }
            break;
    }
}

void MqttPublish(const char *topic, const char *payload, bool retain)
{
    int msg_id;
    if (!OperatingParameters.MqttConnected)
    {
        ESP_LOGW(TAG, "Trying to MQTT publish while not connected");
        OperatingParameters.Errors.mqttProtocolErrors++;
        return;
    }
    msg_id = esp_mqtt_client_publish(
        (esp_mqtt_client_handle_t)(OperatingParameters.MqttClient), topic, payload, 0, 0, (retain ? 1 : 0));
    ESP_LOGV(TAG, "Sent MQTT publish -- msg_id=%d", msg_id);
    xEventGroupSetBits(s_mqtt_event_group, MQTT_EVENT_PUB_BIT);
}

void makeLower(const char *string)
{
    char *p = (char *)string;
    while (*p)
    {
        *p = tolower(*p);
        p++;
    }
}

void MqttUpdateStatusTopic()
{
  if (OperatingParameters.MqttEnabled && OperatingParameters.MqttConnected)
  {
    std::string mode = hvacModeToString(OperatingParameters.hvacSetMode);
    std::string curr_mode = hvacModeToMqttCurrMode(OperatingParameters.hvacOpMode);
    StaticJsonDocument<160> payload;

    std::string temp(16, '\0');
    auto written = std::snprintf(&temp[0], temp.size(), "%.2f", (OperatingParameters.tempCurrent + OperatingParameters.tempCorrection));
    temp.resize(written);
    payload["Temperature"] = temp;

    std::string hum(16, '\0');
    written = std::snprintf(&hum[0], hum.size(), "%.2f", (OperatingParameters.humidCurrent + OperatingParameters.humidityCorrection));
    hum.resize(written);
    payload["Humidity"] = hum;

    payload["Setpoint"] = OperatingParameters.tempSet;

    makeLower(mode.c_str());
    payload["Mode"] = mode;

    makeLower(curr_mode.c_str());
    payload["CurrMode"] = curr_mode;

    if (OperatingParameters.hvacSetMode == AUTO)
    {
        payload["LowSetpoint"] = OperatingParameters.tempSetAutoMin;
        payload["HighSetpoint"] = OperatingParameters.tempSetAutoMax;
    }

    std::string strPayload;
    serializeJson(payload, strPayload);
    ESP_LOGI (TAG, "%s", strPayload.c_str());

    std::string topic;
    topic = g_mqttStatusTopic;
    MqttPublish(topic.c_str(), strPayload.c_str(), false);
  }
}

void MqttMotionUpdate(bool state)
{
    ESP_LOGI (TAG, "Topic: %s Payload: %s", g_mqttSensorStatusTopic.c_str(), (state == true) ? "ON" : "OFF");
    MqttPublish(g_mqttSensorStatusTopic.c_str(), (state == true) ? "ON" : "OFF", false);
}

void MqttHomeAssistantStatDiscovery()
{
    CustomWriter writer;
    static char mac[24];
    std::string discoveryTopic;
    std::string strPayload;

    if (!OperatingParameters.MqttEnabled || !OperatingParameters.MqttConnected)
    {
        ESP_LOGE(TAG, "MQTT not enabled or not connected!");
        OperatingParameters.Errors.mqttConnectErrors++;
        return;
    }

    {
        ESP_LOGI(TAG, "Send Home Assistant Discovery...");
        StaticJsonDocument<740> payload;
        JsonObject device;
        JsonArray modes;
        JsonArray identifiers;

        discoveryTopic = "homeassistant/climate/" + g_deviceName + "/thermostat/config";

        snprintf (mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x", 
            OperatingParameters.mac[0],
            OperatingParameters.mac[1],
            OperatingParameters.mac[2],
            OperatingParameters.mac[3],
            OperatingParameters.mac[4],
            OperatingParameters.mac[5]);

        payload["name"] = ""; //@@@g_friendlyName;
        payload["uniq_id"] = mac;

        modes = payload.createNestedArray("modes");
        modes.add("off");
        modes.add("heat");
        if (OperatingParameters.hvacCoolEnable)
        {
            modes.add("cool");
            modes.add("auto");
        }
        if (OperatingParameters.hvacFanEnable)
            modes.add("fan_only");

        payload["~"] = g_mqttStatusTopic;
        payload["act_t"] = "~";
        payload["act_tpl"] = "{{ value_json.CurrMode }}";
        payload["curr_temp_t"] = "~";
        payload["curr_temp_tpl"] = "{{ value_json.Temperature|float|round(1) }}";
        payload["temp_stat_t"] = "~";
        payload["temp_stat_tpl"] = "{{ value_json.Setpoint }}";
        payload["mode_stat_t"] = "~";
        payload["mode_stat_tpl"] = "{{ value_json.Mode }}";
        payload["current_humidity_topic"] = "~";
        payload["current_humidity_template"] = "{{ value_json.Humidity|float|round(1) }}";
        if (OperatingParameters.hvacFanEnable)
        {
            payload["fan_mode_stat_t"] = "~";
            payload["fan_mode_stat_tpl"] = "{{ value_json.Mode }}";
            payload["fan_mode_cmd_t"] = g_deviceName + "/set/fan";
            modes = payload.createNestedArray("fan_modes");
            modes.add("off");
            modes.add("on");
        }
        if (OperatingParameters.hvacCoolEnable)
        {
            // Auto is enabled, so we need to specify hi & lo temps
            payload["temp_lo_stat_t"] = "~";
            payload["temp_lo_stat_tpl"] = "{{ value_json.LowSetpoint }}";
            payload["temp_hi_stat_t"] = "~";
            payload["temp_hi_stat_tpl"] = "{{ value_json.HighSetpoint }}";
        }

        payload["mode_cmd_t"] = g_deviceName + "/set/mode";
        payload["temp_cmd_t"] = g_deviceName + "/set/temp";

        payload["temp_unit"] = &OperatingParameters.tempUnits;

        device = payload.createNestedObject("device");
        device["name"] = g_friendlyName;
        device["mdl"] = g_deviceModel;
        device["sw"] = g_swVersion;
        device["mf"] = g_manufacturer;
        std::string ip = wifiAddress();
        device["cu"] = "http://" + ip;     //@@@ Must be updated if dhcp address changes

        identifiers = device.createNestedArray("identifiers");
        identifiers.add(mac);

        serializeJsonPretty(payload, writer);
        serializeJson(payload, strPayload);
        ESP_LOGI (TAG, "Payload: %s", strPayload.c_str());

        xEventGroupClearBits (s_mqtt_event_group, MQTT_EVENT_PUB_BIT | MQTT_ERROR_BIT);

        MqttPublish(discoveryTopic.c_str(), strPayload.c_str(), true);

        EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
            MQTT_EVENT_PUB_BIT | MQTT_ERROR_BIT,
            pdFALSE,
            pdFALSE,
            xTicksToWait);

        if (bits & MQTT_ERROR_BIT)
        {
            ESP_LOGE(TAG, "Publish failed");
            OperatingParameters.Errors.mqttProtocolErrors++;
        }
    }
}

void MqttSensorDiscovery()
{
    CustomWriter writer;
    static char mac[24];
    std::string discoveryTopic;
    std::string strPayload;

    if (!OperatingParameters.MqttEnabled || !OperatingParameters.MqttConnected)
    {
        ESP_LOGE(TAG, "Sensor: MQTT not enabled or not connected!");
        OperatingParameters.Errors.mqttConnectErrors++;
        return;
    }

    ESP_LOGI(TAG, "Send Home Assistant Sensor Discovery...");

    discoveryTopic = "homeassistant/binary_sensor/" + g_deviceName + "/config";

    StaticJsonDocument<320> sensorPayload;
    JsonObject device;
    JsonArray identifiers;

    snprintf (mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x", 
        OperatingParameters.mac[0],
        OperatingParameters.mac[1],
        OperatingParameters.mac[2],
        OperatingParameters.mac[3],
        OperatingParameters.mac[4],
        OperatingParameters.mac[5] + 1);

    sensorPayload["name"] = g_friendlyName + " Motion";
    sensorPayload["unique_id"] = mac;
    sensorPayload["device_class"] = "motion";
    sensorPayload["stat_t"] = g_mqttSensorStatusTopic;

    device = sensorPayload.createNestedObject("device");
    device["name"] = g_friendlyName + " Motion";
    device["mdl"] = "Thermostat Motion Sensor";
    device["sw"] = g_swVersion;
    device["mf"] = g_manufacturer;
    std::string ip = wifiAddress();
    device["cu"] = "http://" + ip;     //@@@ Must be updated if dhcp address changes
    identifiers = device.createNestedArray("identifiers");
    identifiers.add(mac);

    serializeJsonPretty(sensorPayload, writer);
    serializeJson(sensorPayload, strPayload);
    ESP_LOGI (TAG, "Sensor Payload: %s", strPayload.c_str());

    xEventGroupClearBits (s_mqtt_event_group, MQTT_EVENT_PUB_BIT | MQTT_ERROR_BIT);

    MqttPublish(discoveryTopic.c_str(), strPayload.c_str(), true);

    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
        MQTT_EVENT_PUB_BIT | MQTT_ERROR_BIT,
        pdFALSE,
        pdFALSE,
        xTicksToWait);

    if (bits & MQTT_ERROR_BIT)
    {
        ESP_LOGE(TAG, "Sensor Publish failed");
        OperatingParameters.Errors.mqttProtocolErrors++;
    }
}

void MqttHomeAssistantDiscovery()
{
    MqttHomeAssistantStatDiscovery();

    ESP_LOGI(TAG, "Sending sensor discovery packet");
    MqttSensorDiscovery();
}


void MqttSubscribeTopic(esp_mqtt_client_handle_t client, std::string topic)
{
    int msg_id;
    msg_id = esp_mqtt_client_subscribe(client, topic.c_str(), 1);
    if (msg_id < 0)
    {
        ESP_LOGE (TAG, "Subscribe topic FAILED, Topic=%s,  msg_id=%d", topic.c_str(), msg_id);
        OperatingParameters.Errors.mqttProtocolErrors++;
    }
    else
    {
        ESP_LOGI (TAG, "Subscribe topic successful, Topic=%s,  msg_id=%d", topic.c_str(), msg_id);
    }
}

bool MqttConnect(void)
{
	ESP_LOGI(__FUNCTION__, "Connecting to MQTT broker");
	esp_mqtt_client_config_t mqtt_cfg;
	memset(&mqtt_cfg, 0, sizeof(esp_mqtt_client_config_t));

    // Build up the fully qualified MQTT URI host string
    char port[16];
    snprintf (port, 15, "%d", OperatingParameters.MqttBrokerPort);
    std::string _host = OperatingParameters.MqttBrokerHost;
    std::string host = "mqtt://" + _host + ":" + port;

    ESP_LOGI(TAG, "Broker FQDN: %s", host.c_str());
    ESP_LOGI(TAG, "Broker:    %s", OperatingParameters.MqttBrokerHost);
    ESP_LOGI(TAG, "Username:  %s", OperatingParameters.MqttBrokerUsername);
    // ESP_LOGI(TAG, "Password:  %s", OperatingParameters.MqttBrokerPassword);
    mqtt_cfg.broker.address.uri = host.c_str();
    mqtt_cfg.credentials.username = OperatingParameters.MqttBrokerUsername;
    mqtt_cfg.credentials.set_null_client_id = true;
    mqtt_cfg.credentials.authentication.password = OperatingParameters.MqttBrokerPassword;

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to init client!");
        OperatingParameters.Errors.mqttConnectErrors++;
        return false;
    }
    OperatingParameters.MqttClient = client;

    xEventGroupClearBits (s_mqtt_event_group, MQTT_EVENT_CONNECTED_BIT | MQTT_ERROR_BIT | MQTT_EVENT_DISCONNECTED_BIT);

    esp_err_t err;
    err = esp_mqtt_client_register_event (client, MQTT_EVENT_ANY, MqttEventHandler, client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register client: 0x%x", err);
        if (err == ESP_ERR_NO_MEM) ESP_LOGE(TAG, "ESP_ERR_NO_MEM");
        if (err == ESP_ERR_INVALID_ARG) ESP_LOGE(TAG, "ESP_ERR_INVALID_ARG");
        OperatingParameters.Errors.mqttConnectErrors++;
        esp_mqtt_client_destroy(client);
        return false;
    }

    err = esp_mqtt_client_start(client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start client: 0x%x", err);
        if (err == ESP_FAIL) ESP_LOGE(TAG, "ESP_FAIL");
        if (err == ESP_ERR_INVALID_ARG) ESP_LOGE(TAG, "ESP_ERR_INVALID_ARG");
        OperatingParameters.Errors.mqttConnectErrors++;
        esp_mqtt_client_unregister_event(client, MQTT_EVENT_ANY, MqttEventHandler);
        esp_mqtt_client_destroy(client);
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
        MQTT_EVENT_CONNECTED_BIT | MQTT_ERROR_BIT | MQTT_EVENT_DISCONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        xTicksToWait);

    // bits == 0 means timeout
    if ((bits == 0) || (bits & MQTT_ERROR_BIT) || (bits & MQTT_EVENT_DISCONNECTED_BIT))
    {
        ESP_LOGE (TAG, "MQTT Connect failed");
        // esp_mqtt_client_stop(client); // Commented out since connect never worked
        ESP_LOGW(TAG, "Removing MQTT data structures");
        OperatingParameters.Errors.mqttConnectErrors++;
        esp_mqtt_client_unregister_event(client, MQTT_EVENT_ANY, MqttEventHandler);
        esp_mqtt_client_destroy(client);
        return false;
    }

    if (bits & MQTT_EVENT_CONNECTED_BIT)
    {
        // Send discovery packet to let all listeners know we have arrived
        MqttHomeAssistantDiscovery();
    }

    return true;
}

void MqttInit()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    /* Initialize event group */
    s_mqtt_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "MQTT Startup...");

    g_deviceName = OperatingParameters.DeviceName;
    g_friendlyName = OperatingParameters.FriendlyName;
    g_mqttStatusTopic = g_deviceName + "/status";
    g_mqttSensorStatusTopic = g_deviceName + "/motion";
    OperatingParameters.MqttConnected = false;

    if (!OperatingParameters.MqttEnabled)
    {
        ESP_LOGW(TAG, "MQTT is not enabled!");
        // No need to count this as an error
    }

    if (!wifiConnected())
    {
        ESP_LOGE(TAG, "Attempting to start MQTT when wifi connection down");
        OperatingParameters.Errors.mqttConnectErrors++;
    }
}

#endif  // #ifdef MQTT_ENABLED
