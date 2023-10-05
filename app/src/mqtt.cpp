#ifdef MQTT_ENABLED

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
const char*         g_swVersion = VERSION_STRING;                             // Firmware Version
const char*         g_manufacturer = "Steve Meisner";                         // Manufacturer Name
std::string         g_deviceName; // = OperatingParameters.DeviceName;            // Device Name
std::string         g_mqttStatusTopic; // = "SmartThermostat/" + g_deviceName;    // MQTT Topic

struct CustomWriter {
  std::string str;

  size_t write(uint8_t c) {
    str.append(1, static_cast<char>(c));
    // ESP_LOGI(TAG, "(char) -> %c", c);
    return 1;
  }

  size_t write(const uint8_t *s, size_t n) {
    str.append(reinterpret_cast<const char *>(s), n);
    // ESP_LOGI(TAG, "(str) -> %s", str.c_str());
    return n;
  }
};

#define MQTT_EVENT_CONNECTED_BIT BIT0
#define MQTT_EVENT_DISCONNECTED_BIT BIT1
#define MQTT_EVENT_PUB_BIT BIT2
#define MQTT_ERROR_BIT BIT3
static EventGroupHandle_t s_mqtt_event_group = NULL;
const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;

static void MqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    // int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            OperatingParameters.MqttConnected = true;
            xEventGroupSetBits(s_mqtt_event_group, MQTT_EVENT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            OperatingParameters.MqttConnected = false;
            xEventGroupSetBits(s_mqtt_event_group, MQTT_EVENT_DISCONNECTED_BIT);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            // xEventGroupSetBits(s_mqtt_event_group, MQTT_EVENT_PUB_BIT);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            if (strstr(event->topic, "mode"))
            {
                char m[event->data_len + 1];
                strncpy (m, event->data, event->data_len);
                m[event->data_len] = '\0';
                // Make first letter uppercase to match our string representation
                m[0] = toupper(m[0]);
                printf ("Looking up %s\n", m);
                updateHvacMode (strToHvacMode(m));
            }
            if (strstr(event->topic, "temp"))
            {
                updateHvacSetTemp (atof(event->data));
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            xEventGroupSetBits(s_mqtt_event_group, MQTT_ERROR_BIT);
            break;
    }
}

void MqttPublish(const char *topic, const char *payload)
{
    int msg_id;
    if (!OperatingParameters.MqttConnected)
    {
        ESP_LOGW(TAG, "Trying to MQTT publish while not connected");
        return;
    }
    msg_id = esp_mqtt_client_publish(
        (esp_mqtt_client_handle_t)(OperatingParameters.MqttClient), topic, payload, 0, 0, 0);
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
    StaticJsonDocument<200> payload;
    // payload["inputstatus"] = "ON";
    payload["Humidity"] = OperatingParameters.humidCurrent + OperatingParameters.humidityCorrection;
    payload["Temperature"] = OperatingParameters.tempCurrent + OperatingParameters.tempCorrection;
    payload["Setpoint"] = OperatingParameters.tempSet;
    makeLower(mode.c_str());
    payload["Mode"] = mode;

    std::string strPayload;
    serializeJson(payload, strPayload);
    ESP_LOGI (TAG, "%s", strPayload.c_str());

    std::string topic;
    topic = g_mqttStatusTopic + "/status";
    // if(g_mqttPubSub.connected())
    // {
        MqttPublish(topic.c_str(), strPayload.c_str());
    // }
  }
}

void MqttHomeAssistantDiscovery()
{
    CustomWriter writer;
    static char mac[24];
    // std::string UniqueId;
    std::string discoveryTopic;
    std::string payload;
    std::string strPayload;

    // if(g_mqttPubSub.connected())
    {
        ESP_LOGI(TAG, "Send Home Assistant Discovery...");
        StaticJsonDocument<700> payload;
        JsonObject device;
        JsonArray modes;
        JsonArray identifiers;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Temperature
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // discoveryTopic = "homeassistant/sensor/esp32iotsensor/" + g_deviceName + "_temp" + "/config";
        // discoveryTopic = "homeassistant/climate/thermostat/" + g_deviceName + "_temp" + "/config";
        discoveryTopic = "homeassistant/climate/" + g_deviceName + "/thermostat/config";

        snprintf (mac, sizeof(mac), "%02x%02x%02x%02x%02x%02x", 
            OperatingParameters.mac[0],
            OperatingParameters.mac[1],
            OperatingParameters.mac[2],
            OperatingParameters.mac[3],
            OperatingParameters.mac[4],
            OperatingParameters.mac[5]);
        // UniqueId = mac;
        // printf ("Mac: %s\n", mac);

        // payload["name"] = g_deviceName + ".temp";
        payload["name"] = g_deviceName;
        // payload["uniq_id"] = UniqueId + "_temp";
        payload["uniq_id"] = mac;
        // payload["power_command_topic"] = g_deviceName + "/power/set";

        modes = payload.createNestedArray("modes");
        modes.add("off");
        modes.add("heat");
        if (OperatingParameters.hvacCoolEnable)
            modes.add("cool");
        if (OperatingParameters.hvacFanEnable)
            modes.add("fan");
        if ((OperatingParameters.hvacCoolEnable) || (OperatingParameters.hvacFanEnable))
            modes.add("auto");

        // payload["stat_t"] = g_mqttStatusTopic + "/temperature";
        payload["curr_temp_t"] = g_mqttStatusTopic + "/status";
        payload["curr_temp_tpl"] = "{{ value_json.Temperature|float }}";
        payload["temp_stat_t"] = g_mqttStatusTopic + "/status";
        payload["temp_stat_tpl"] = "{{ value_json.Setpoint }}";
        payload["mode_stat_t"] = g_mqttStatusTopic + "/status";
        payload["mode_stat_tpl"] = "{{ value_json.Mode }}";
        payload["current_humidity_topic"] = g_mqttStatusTopic + "/status";
        payload["current_humidity_template"] = "{{ value_json.Humidity|float }}";

        payload["mode_cmd_t"] = g_deviceName + "/set/mode";
        payload["temp_cmd_t"] = g_deviceName + "/set/temp";

        // payload["dev_cla"] = "temperature";
        // payload["val_tpl"] = "{{ value_json.temp | is_defined }}";
        // payload["unit_of_meas"] = "°C";
        payload["temp_unit"] = &OperatingParameters.tempUnits;

        device = payload.createNestedObject("device");
        device["name"] = g_deviceName;
        device["model"] = g_deviceModel;
        device["sw_version"] = g_swVersion;
        device["manufacturer"] = g_manufacturer;

        identifiers = device.createNestedArray("identifiers");
        identifiers.add(mac);

        serializeJsonPretty(payload, writer);
        serializeJson(payload, strPayload);
        ESP_LOGI (TAG, "Payload: %s", strPayload.c_str());

        // g_mqttPubSub.publish(discoveryTopic.c_str(), strPayload.c_str());
        MqttPublish(discoveryTopic.c_str(), strPayload.c_str());

        EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
            MQTT_EVENT_PUB_BIT | MQTT_ERROR_BIT,
            pdFALSE,
            pdFALSE,
            xTicksToWait);

        if (bits & MQTT_ERROR_BIT)
            ESP_LOGE(TAG, "Publish failed");


#if 0
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Humidity
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        payload.clear();
        device.clear();
        identifiers.clear();
        strPayload.clear();

        discoveryTopic = "homeassistant/climate/thermostat/" + g_deviceName + "_hum" + "/config";
        
        payload["name"] = g_deviceName + ".hum";
        // payload["uniq_id"] = UniqueId + "_hum";
        payload["stat_t"] = g_mqttStatusTopic;
        payload["dev_cla"] = "humidity";
        payload["val_tpl"] = "{{ value_json.hum | is_defined }}";
        payload["unit_of_meas"] = "%";
        device = payload.createNestedObject("device");
        device["name"] = g_deviceName;
        device["model"] = g_deviceModel;
        device["sw_version"] = g_swVersion;
        device["manufacturer"] = g_manufacturer;
        identifiers = device.createNestedArray("identifiers");
        identifiers.add("UniqueId2");

        serializeJsonPretty(payload, writer);
        ESP_LOGI (TAG, "test2");
        serializeJson(payload, strPayload);
        printf ("Payload: %s\n", strPayload.c_str());

        // g_mqttPubSub.publish(discoveryTopic.c_str(), strPayload.c_str());
        MqttPublish(discoveryTopic.c_str(), strPayload.c_str());

        bits = xEventGroupWaitBits(s_mqtt_event_group,
            MQTT_EVENT_PUB_BIT | MQTT_ERROR_BIT,
            pdFALSE,
            pdFALSE,
            xTicksToWait);

        if (bits & MQTT_ERROR_BIT)
            ESP_LOGE(TAG, "Publish failed");

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Binary Door
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        payload.clear();
        device.clear();
        identifiers.clear();
        strPayload.clear();

        discoveryTopic = "homeassistant/climate/thermostat/" + g_deviceName + "_door" + "/config";
        
        payload["name"] = g_deviceName + ".door";
        payload["uniq_id"] = UniqueId + "_door";
        payload["stat_t"] = g_mqttStatusTopic;
        payload["dev_cla"] = "door";
        payload["val_tpl"] = "{{ value_json.inputstatus | is_defined }}";
        device = payload.createNestedObject("device");
        device["name"] = g_deviceName;
        device["model"] = g_deviceModel;
        device["sw_version"] = g_swVersion;
        device["manufacturer"] = g_manufacturer;
        identifiers = device.createNestedArray("identifiers");
        identifiers.add(UniqueId);

        // serializeJsonPretty(payload, writer);
        ESP_LOGI (TAG, "test3");
        serializeJson(payload, strPayload);

        // g_mqttPubSub.publish(discoveryTopic.c_str(), strPayload.c_str());
#endif

    }
}

void MqttSubscribeTopic(esp_mqtt_client_handle_t client, std::string topic)
{
    int msg_id;
    msg_id = esp_mqtt_client_subscribe(client, topic.c_str(), 1);
    if (msg_id < 0)
        ESP_LOGE (TAG, "Send subscribe FAILED, Topic=%s,  msg_id=%d", topic.c_str(), msg_id);
    else
        ESP_LOGI (TAG, "Send subscribe successful, Topic=%s,  msg_id=%d", topic.c_str(), msg_id);
}


static void MqttConnect(void)
{
	ESP_LOGI(__FUNCTION__, "Waiting for MQTT...");
	esp_mqtt_client_config_t mqtt_cfg;
	memset(&mqtt_cfg, 0, sizeof(esp_mqtt_client_config_t));

    OperatingParameters.MqttBrokerHost = (char *)malloc(64);
    strcpy (OperatingParameters.MqttBrokerHost, "mqtt://hass.meisners.net:1883");
    OperatingParameters.MqttBrokerUsername = (char *)malloc(16);
    strcpy (OperatingParameters.MqttBrokerUsername, "homeassistant");
    OperatingParameters.MqttBrokerPassword = (char *)malloc(72);
    strcpy (OperatingParameters.MqttBrokerPassword, "einiiZeiphah4ish0Rowae7doh0OhNg6wahngohmie4iNae1meipainaeyijai5i");

    printf ("Broker:   %s\n", OperatingParameters.MqttBrokerHost);
    printf ("Username: %s\n", OperatingParameters.MqttBrokerUsername);
    printf ("Password: %s\n", OperatingParameters.MqttBrokerPassword);
    // mqtt_cfg.broker.address.uri = "mqtt://iot.eclipse.org";
    // mqtt_cfg.broker.address.uri = "mqtt://test.mosquitto.org:1883";
    // mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    mqtt_cfg.broker.address.uri = OperatingParameters.MqttBrokerHost;
    // mqtt_cfg.broker.address.hostname = "mqtt://homeassistant.meisners.net:1883";
    // mqtt_cfg.broker.address.port = 1883;
    // mqtt_cfg.credentials.username = "mqtt";
    mqtt_cfg.credentials.username = OperatingParameters.MqttBrokerUsername;
    mqtt_cfg.credentials.set_null_client_id = true;
    // mqtt_cfg.credentials.authentication.password = "mqtt";
    mqtt_cfg.credentials.authentication.password = OperatingParameters.MqttBrokerPassword;
    

    //  = {
    //     .uri = "mqtt://iot.eclipse.org",
    //     .event_handle = mqtt_event_handler,
    //     // .user_context = (void *)your_context
    // };

    /* Initialize event group */
    s_mqtt_event_group = xEventGroupCreate();

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    OperatingParameters.MqttClient = client;

    esp_mqtt_client_register_event (client, MQTT_EVENT_ANY, MqttEventHandler, client);
    esp_mqtt_client_start(client);

    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
        MQTT_EVENT_CONNECTED_BIT | MQTT_ERROR_BIT,
        pdFALSE,
        pdFALSE,
        xTicksToWait);

    if (bits & MQTT_ERROR_BIT)
        ESP_LOGE (TAG, "MQTT Connect failed");

    if (bits & MQTT_EVENT_CONNECTED_BIT)
    {
        // MqttSubscribeTopic(client, g_mqttStatusTopic);
        // MqttSubscribeTopic(client, g_deviceName + "/set/mode");
        // MqttSubscribeTopic(client, g_deviceName + "/set/temp");
        MqttSubscribeTopic(client, g_deviceName + "/set/#");
    }
}

//@@@ Need a reconnect function

//@@@ Need a disconnect function


void MqttInit()
{
    if (!OperatingParameters.MqttEnabled)
    {
        ESP_LOGW(TAG, "MQTT is not enabled!");
        return;
    }
    ESP_LOGI(TAG, "MQTT Startup...");
    if (!wifiConnected())
    {
        ESP_LOGE(TAG, "Attempting to start MQTT when wifi connection down");
        return;
    }

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    g_deviceName = OperatingParameters.DeviceName;
    g_mqttStatusTopic = g_deviceName + "/status";

    MqttConnect();

    MqttHomeAssistantDiscovery();

    // MqttUpdateStatusTopic();
}

#endif  // #ifdef MQTT_ENABLED
