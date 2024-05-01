#ifdef MATTER_ENABLED

#include "thermostat.hpp"

/**
 * CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING
 *
 * A string identifying the software version running on the device.
 */
//#define CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING "V1.0"

/**
 * CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION
 *
 * A monothonic number identifying the software version running on the device.
 */
//#define CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION 1

/**
 * CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION
 *
 * The hardware version number assigned to device or product by the device vendor.  This
 * number is scoped to the device product id, and typically corresponds to a revision of the
 * physical device, a change to its packaging, and/or a change to its marketing presentation.
 * This value is generally *not* incremented for device software versions.
 */
//#define CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION 5

/**
 *  @brief Fallback value for the basic information cluster's Vendor name attribute
 *   if the actual vendor name is not provisioned in the device memory.
 */
#define CHIP_DEVICE_CONFIG_VENDOR_NAME "Tah Der"
#define CONFIG_CHIP_DEVICE_VENDOR_NAME "1Tah Der"

/**
 *  @brief Set the name of the product. This will be written to node0
 * and used as the product name.
 */
#define CHIP_DEVICE_CONFIG_PRODUCT_NAME "RealSmartThermostat"

// Use a default pairing code if one hasn't been provisioned in flash.
// #define CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE 20202021
// #define CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR 0xF00
#define CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME "Tah Der"
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME "Smart prod"
#define CONFIG_CHIP_DEVICE_PRODUCT_NAME "Smart chip prod"
#define CHIP_DEVICE_CONFIG_DEVICE_HARDWARE_VERSION_STRING "v1.0"
#define CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING "v2.0"

#include "Matter.h"
#include <app/server/OnboardingCodesUtil.h>
#include <app/server/Server.h>
#include <app/server/CommissioningWindowManager.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>

using namespace chip;
using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::endpoint;

using chip::DeviceLayer::ConfigurationMgr;

#define TAG "Thermostat"

/**
 * This program presents example Matter light device with OnOff cluster by
 * controlling LED with Matter and toggle button.
 *
 * If your ESP does not have buildin LED, please connect it to LED_PIN
 *
 * You can toggle light by both:
 *  - Matter (via CHIPTool or other Matter controller)
 *  - toggle button (by default attached to GPIO0 - reset button, with debouncing) 
 */

// Please configure your PINs
// const int LED_PIN = 2;
// const int TOGGLE_BUTTON_PIN = 0;

// Debounce for toggle button
// const int DEBOUNCE_DELAY = 500;
// int last_toggle;

constexpr auto k_timeout_seconds = 300;

// Cluster and attribute ID used by Matter light device
const uint32_t CLUSTER_ID = OnOff::Id;
const uint32_t ATTRIBUTE_ID = OnOff::Attributes::OnOff::Id;

// Endpoint and attribute ref that will be assigned to Matter device
uint16_t light_endpoint_id = 0;
attribute_t *attribute_ref;

// There is possibility to listen for various device events, related for example to setup process
// Leaved as empty for simplicity
static void on_device_event(const ChipDeviceEvent *event, intptr_t arg)
{
  Serial.printf ("IN on_device_event() : event->Type = %d\n", event->Type);

  switch (event->Type)
  {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
    {
      ESP_LOGI(TAG, "Fabric removed successfully");
#if 0
      if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0)
      {
        chip::CommissioningWindowManager & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
        constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
        if (!commissionMgr.IsCommissioningWindowOpen())
        {
          /* After removing last fabric, this example does not remove the Wi-Fi credentials
          * and still has IP connectivity so, only advertising on DNS-SD.
          */
          CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds,
                            chip::CommissioningWindowAdvertisement::kDnssdOnly);
          if (err != CHIP_NO_ERROR)
          {
            ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
          }
        }
      }
#endif
      break;
    }

    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved:
        ESP_LOGI(TAG, "Fabric will be removed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricUpdated:
        ESP_LOGI(TAG, "Fabric is updated");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(TAG, "Fabric is committed");
        break;

    default:
        break;
  }
}

static esp_err_t on_identification(identification::callback_type_t type, uint16_t endpoint_id,
                                   uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
  return ESP_OK;
}

// Listener on attribute update requests.
// In this example, when update is requested, path (endpoint, cluster and attribute) is checked
// if it matches light attribute. If yes, LED changes state to new one.
static esp_err_t on_attribute_update(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                     uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data) {
  if (type == attribute::PRE_UPDATE && endpoint_id == light_endpoint_id &&
      cluster_id == CLUSTER_ID && attribute_id == ATTRIBUTE_ID) {
    // We got an light on/off attribute update!
    bool new_state = val->val.b;
//    digitalWrite(LED_PIN, new_state);
  }
  return ESP_OK;
}

bool MatterInit()
{
  //@@@ Need to set for Matter only
  // Enable debug logging
  esp_log_level_set("*", ESP_LOG_DEBUG);
  // esp_log_level_set("*", ESP_LOG_WARN);

  if (!OperatingParameters.MatterEnabled)
  {
    Serial.printf ("MatterInit() called when Matter not enabled\n");
    return false;
  }

  // If no creds in EEPROM, do not start Matter.
  // There may be creds stored in wifi NVS.
  if (strlen(WifiCreds.ssid) == 0)
  {
    Serial.printf ("No wifi credentials set -- not starting matter\n");
    return false;
  }

  // Set wifi credentials
  wifiSetCredentials(WifiCreds.ssid, WifiCreds.password);

  // Setup Matter node
  node::config_t node_config;
  node_t *node = node::create(&node_config, on_attribute_update, on_identification);

  // Set product name (stored in node 0)
  auto endpoint = esp_matter::endpoint::get(node, 0);
  auto cluster = esp_matter::cluster::get(endpoint, chip::app::Clusters::BasicInformation::Id);
  auto attribute = esp_matter::attribute::get(cluster, chip::app::Clusters::BasicInformation::Attributes::NodeLabel::Id);
  std::string data = CHIP_DEVICE_CONFIG_PRODUCT_NAME;
  auto val = esp_matter_char_str(data.data(), data.length());
  esp_matter::attribute::set_val(attribute, &val);



#if 0
{
    esp_err_t error;
    esp_matter_attr_val_t val = esp_matter_char_str((char *)"Lowpan", sizeof("Lowpan"));
    error = attribute::update(
      0,
      chip::app::Clusters::BasicInformation::Id,
      chip::app::Clusters::BasicInformation::Attributes::VendorName::Id,
      &val);
}

  // THE FOLLOWING DO NOT WORK
   attribute = esp_matter::attribute::get(cluster, chip::app::Clusters::BasicInformation::Attributes::ProductName::Id);
   data = CHIP_DEVICE_CONFIG_PRODUCT_NAME;
   val = esp_matter_char_str(data.data(), data.length());
  esp_matter::attribute::set_val(attribute, &val);

   attribute = esp_matter::attribute::get(cluster, chip::app::Clusters::BasicInformation::Attributes::VendorName::Id);
   data = CHIP_DEVICE_CONFIG_VENDOR_NAME;
   val = esp_matter_char_str(data.data(), data.length());
  esp_matter::attribute::set_val(attribute, &val);

   attribute = esp_matter::attribute::get(cluster, chip::app::Clusters::BasicInformation::Attributes::HardwareVersionString::Id);
   data = CHIP_DEVICE_CONFIG_DEVICE_HARDWARE_VERSION_STRING;
   val = esp_matter_char_str(data.data(), data.length());
  esp_matter::attribute::set_val(attribute, &val);
  // THE ABOVE DO NOT WORK
#endif

  // Add enpoint to the node
  thermostat::config_t thermostat_config;
  thermostat_config.thermostat.local_temperature = 19;
  thermostat_config.thermostat.system_mode        = 0;    // OFF state
  endpoint_t *thermostat_endpoint = thermostat::create(node, &thermostat_config, ENDPOINT_FLAG_NONE, NULL);

  uint16_t thermostat_endpoint_id = endpoint::get_id(thermostat_endpoint);
  ESP_LOGI(TAG, "Thermostat endpoint_id = %d", thermostat_endpoint_id);

  // Add additional feature
  cluster_t *thermostat_cluster = cluster::get(thermostat_endpoint, Thermostat::Id);
  cluster::thermostat::feature::heating::config_t heating_config;
  heating_config.abs_max_heat_setpoint_limit = 3000;
  heating_config.abs_min_heat_setpoint_limit = 1500;
  heating_config.max_heat_setpoint_limit = 2800;
  heating_config.min_heat_setpoint_limit = 2000;
  heating_config.occupied_heating_setpoint = 2300;
  heating_config.pi_heating_demand = 0;
  ESP_LOGE(TAG, "Thermostat heating cluster id = %d", cluster::thermostat::feature::heating::get_id());
  cluster::thermostat::feature::heating::add(thermostat_cluster, &heating_config);

  // Add additional cooling cluster
//   cluster_t *thermostat_cluster = cluster::get(thermostat_endpoint, Thermostat::Id);
  esp_matter::cluster::thermostat::feature::cooling::config_t cooling_config;
  //cooling_config.config();MutableCharSpan
  cooling_config.abs_min_cool_setpoint_limit = 1600;
  cooling_config.abs_max_cool_setpoint_limit = 3200;
  cooling_config.pi_cooling_demand           = 50;  // 50%
  cooling_config.occupied_cooling_setpoint   = 2600;
  cooling_config.min_cool_setpoint_limit     = 1600;
  cooling_config.max_cool_setpoint_limit     = 3200;
  ESP_LOGE(TAG, "Thermostat cooling cluster id = %d", esp_matter::cluster::thermostat::feature::cooling::get_id());
  esp_matter::cluster::thermostat::feature::cooling::add(thermostat_cluster, &cooling_config);












//   // Setup Light endpoint / cluster / attributes with default values
//   on_off_light::config_t light_config;
//   light_config.on_off.on_off = false;
//   light_config.on_off.lighting.start_up_on_off = false;
//   endpoint_t *endpoint = on_off_light::create(node, &light_config, ENDPOINT_FLAG_NONE, NULL);

//   // Save on/off attribute reference. It will be used to read attribute value later.
//   attribute_ref = attribute::get(cluster::get(endpoint, CLUSTER_ID), ATTRIBUTE_ID);

//   // Save generated endpoint id
//   light_endpoint_id = endpoint::get_id(endpoint);
  
  // Setup DAC (this is good place to also set custom commission data, passcodes etc.)
  esp_matter::set_custom_dac_provider(Credentials::Examples::GetExampleDACProvider());




  ESP_LOGE(TAG, "Calling esp_matter::start()\n");
  // Start Matter device
  esp_matter::start(on_device_event);

  // Get QR Code 
  std::string QRCode = "                                 ";  // 33 chars
  CHIP_ERROR err;

  ESP_LOGE(TAG, "Calling PrintOnboardingCodes()\n");
  // Print codes needed to setup Matter device
  #if 0
    PrintOnboardingCodes(RendezvousInformationFlags(RendezvousInformationFlag::kOnNetwork));
  #else
    err = GetQRCode((MutableCharSpan &)QRCode, RendezvousInformationFlag::kOnNetwork);
    if (err == CHIP_NO_ERROR)
    {
      // strcpy (OperatingParameters.MatterQR, QRCode.data());
      strcpy (OperatingParameters.MatterQR, QRCode.c_str());
      ESP_LOGE(TAG, "QR Pairing Code: '%s'\n", OperatingParameters.MatterQR);
    } else {
      ESP_LOGE(TAG, "Failed to GetQRCode() -- Error: %d\n", err.AsInteger());
    }
    err = GetManualPairingCode((MutableCharSpan &)QRCode, RendezvousInformationFlag::kOnNetwork);
    if (err == CHIP_NO_ERROR)
    {
      strcpy (OperatingParameters.MatterPairingCode, QRCode.c_str());
      ESP_LOGE(TAG, "Manual Pairing Code: '%s'\n", OperatingParameters.MatterPairingCode);
    } else {
      ESP_LOGE(TAG, "Failed to GetManualPairingCode() -- Error: %d\n", err.AsInteger());
    }
  #endif

  OperatingParameters.MatterStarted = true;  

  // Call into wifi module to set flags appropriately to
  // indicate Matter is running and register wifi callbacks.
  wifiMatterStarted();

  return true;
}


void MatterFactoryReset()
{
  ESP_LOGI(TAG, "Calling ESP factory_reset()\n");
#if 1
  esp_matter::factory_reset();
#else
  chip::DeviceLayer::ConfigurationMgr().InitiateFactoryReset();
#endif
}

// Reads light on/off attribute value
esp_matter_attr_val_t get_onoff_attribute_value() {
  esp_matter_attr_val_t onoff_value = esp_matter_invalid(NULL);
  attribute::get_val(attribute_ref, &onoff_value);
  return onoff_value;
}

// Sets light on/off attribute value
void set_onoff_attribute_value(esp_matter_attr_val_t* onoff_value) {
  attribute::update(light_endpoint_id, CLUSTER_ID, ATTRIBUTE_ID, onoff_value);
}

// When toggle light button is pressed (with debouncing),
// light attribute value is changed
void MatterLoop()
{
//   if ((millis() - last_toggle) > DEBOUNCE_DELAY)
//   {
//     if (!digitalRead(TOGGLE_BUTTON_PIN))
//     {
//       last_toggle = millis();
//       // Read actual on/off value, invert it and set
//       esp_matter_attr_val_t onoff_value = get_onoff_attribute_value();
//       onoff_value.val.b = !onoff_value.val.b;
//       set_onoff_attribute_value(&onoff_value);
//     }
//   }
}

#endif  // #ifdef MATTER_ENABLED
