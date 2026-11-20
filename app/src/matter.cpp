// SPDX-License-Identifier: GPL-3.0-only
/*
 * matter.cpp
 *
 * This module provides ESP-Matter integration for the thermostat. It creates
 * the Matter node, thermostat endpoint with its humidity measurement cluster,
 * and a separate occupancy-sensor endpoint. It handles Matter lifecycle events
 * and attribute writes, and reports thermostat state, humidity, and presence.
 *
 * MatterInit() validates the Matter setting and saved Wi-Fi credentials,
 * starts the endpoint and server, generates onboarding codes, and notifies
 * the Wi-Fi module that Matter networking is active. Its caller stores the
 * result in OperatingParameters.MatterStarted. MatterUpdate() is called by
 * the thermostat state loop and reports changed attributes after the Matter
 * server is ready. MatterFactoryReset() delegates fabric reset to ESP-Matter.
 *
 * LocalTemperature includes the configured temperature correction. The
 * configured set temperature is used for both occupied heating and cooling
 * setpoints. Matter temperatures are in hundredths of a degree Celsius;
 * conversions account for the thermostat's selected display units. Matter
 * writes to setpoints and SystemMode update the thermostat's operating state.
 *
 * Notes:
 *  This module is compiled only when MATTER_ENABLED is defined.
 * MatterUpdate() does nothing until Matter has started and the server is ready.
 */
#ifdef MATTER_ENABLED

#include "thermostat.hpp"

// ESP-Matter's CHIP headers use std::aligned_storage, deprecated in C++23.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <esp_matter.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <app/server/Server.h>
#include <esp_wifi.h>
#include <lib/support/logging/TextOnlyLogging.h>
#pragma GCC diagnostic pop

#include <atomic>
#include <cmath>
#include <inttypes.h>

using namespace chip;
using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::endpoint;

#define TAG "Matter"

namespace {

// Matter Thermostat cluster SystemMode values supported by this endpoint.
constexpr uint8_t kSystemModeOff = 0;
constexpr uint8_t kSystemModeAuto = 1;
constexpr uint8_t kSystemModeCool = 3;
constexpr uint8_t kSystemModeHeat = 4;
constexpr uint8_t kSystemModeEmergencyHeat = 5;
constexpr uint8_t kSystemModeFanOnly = 7;

// HVACSystemTypeConfiguration encodes cooling and heating stage counts in two-bit fields.
constexpr uint8_t kOneCoolingStage = 0x01;
constexpr uint8_t kOneHeatingStage = 0x04;
constexpr uint8_t kTwoHeatingStages = 0x08;

// HVAC capabilities captured when the Matter data model is created.
bool matter_cooling_enabled;
bool matter_fan_enabled;
bool matter_two_stage_heat_enabled;

// Endpoint used by the attribute callback and all subsequent reports.
uint16_t thermostat_endpoint_id;
// Set by the CHIP server-ready event before periodic attribute reports begin.
std::atomic<bool> matter_server_ready{false};

// Tracks whether MatterUpdate() has run its initial report pass.
bool last_reported_state_valid;

// Cached Matter Thermostat LocalTemperature value.
int16_t last_local_temperature;
// Cached Matter Thermostat OccupiedHeatingSetpoint value.
int16_t last_heating_setpoint;
// Cached Matter Thermostat OccupiedCoolingSetpoint value.
int16_t last_cooling_setpoint;
// Cached Matter Thermostat SystemMode value.
uint8_t last_system_mode;
// Cached Matter RelativeHumidityMeasurement value in hundredths of a percent.
nullable<uint16_t> last_humidity;
// Endpoint used to report the LD2410 presence state.
uint16_t presence_endpoint_id;
bool last_reported_presence;
bool last_reported_presence_valid;

// Convert the thermostat's selected temperature unit to Matter hundredths of Celsius.
int16_t to_matter_temperature(float temperature)
{
  const float celsius = OperatingParameters.tempUnits == 'F' ? ftoc(temperature) : temperature;
  return static_cast<int16_t>(std::lround(celsius * 100.0f));
}

// Convert corrected relative humidity to Matter hundredths of a percent.
nullable<uint16_t> to_matter_relative_humidity(float humidity)
{
  if (!std::isfinite(humidity))
  {
    return nullable<uint16_t>();
  }
  if (humidity < 0.0f)
  {
    humidity = 0.0f;
  }
  else if (humidity > 100.0f)
  {
    humidity = 100.0f;
  }
  return nullable<uint16_t>(static_cast<uint16_t>(std::lround(humidity * 100.0f)));
}

// Convert Matter hundredths of Celsius to the thermostat's selected temperature unit.
float from_matter_temperature(int16_t temperature)
{
  const float celsius = static_cast<float>(temperature) / 100.0f;
  return OperatingParameters.tempUnits == 'F' ? ctof(celsius) : celsius;
}

// Return whether a requested Matter mode is enabled by the local HVAC configuration.
bool is_matter_system_mode_enabled(uint8_t mode)
{
  switch (mode)
  {
    case kSystemModeOff:
    case kSystemModeHeat:
      return true;
    case kSystemModeAuto:
    case kSystemModeCool:
      return matter_cooling_enabled;
    case kSystemModeEmergencyHeat:
      return matter_two_stage_heat_enabled;
    case kSystemModeFanOnly:
      return matter_fan_enabled;
    default:
      return false;
  }
}

// Translate the thermostat HVAC state into a supported Matter SystemMode.
uint8_t to_matter_system_mode(HVAC_MODE mode)
{
  switch (mode)
  {
    case AUTO:
      return matter_cooling_enabled ? kSystemModeAuto : kSystemModeHeat;
    case COOL:
      return matter_cooling_enabled ? kSystemModeCool : kSystemModeOff;
    case HEAT:
      return kSystemModeHeat;
    case AUX_HEAT:
      return matter_two_stage_heat_enabled ? kSystemModeEmergencyHeat : kSystemModeHeat;
    case FAN_ONLY:
      return matter_fan_enabled ? kSystemModeFanOnly : kSystemModeOff;
    case OFF:
    case IDLE:
    case ERROR:
    case NR_HVAC_MODES:
      return kSystemModeOff;
  }
  return kSystemModeOff;
}

// Apply Matter writes for this endpoint's Thermostat cluster to the HVAC state.
esp_err_t on_attribute_update(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                              uint32_t attribute_id, esp_matter_attr_val_t *value, void *priv_data)
{
  if (type != attribute::PRE_UPDATE || endpoint_id != thermostat_endpoint_id ||
      cluster_id != Thermostat::Id)
  {
    return ESP_OK;
  }

  if (attribute_id == Thermostat::Attributes::SystemMode::Id)
  {
    if (!is_matter_system_mode_enabled(value->val.u8))
    {
      ESP_LOGW(TAG, "Ignoring Matter thermostat mode %u; disabled in local HVAC settings",
               value->val.u8);
      return ESP_ERR_INVALID_ARG;
    }

    switch (value->val.u8)
    {
      case kSystemModeOff:
        updateHvacMode(OFF);
        return ESP_OK;
      case kSystemModeAuto:
        updateHvacMode(AUTO);
        return ESP_OK;
      case kSystemModeCool:
        updateHvacMode(COOL);
        return ESP_OK;
      case kSystemModeHeat:
        updateHvacMode(HEAT);
        return ESP_OK;
      case kSystemModeEmergencyHeat:
        updateHvacMode(AUX_HEAT);
        return ESP_OK;
      case kSystemModeFanOnly:
        updateHvacMode(FAN_ONLY);
        return ESP_OK;
      default:
        ESP_LOGW(TAG, "Ignoring unsupported Matter thermostat system mode: %u", value->val.u8);
        return ESP_ERR_INVALID_ARG;
    }
  }

  if (attribute_id == Thermostat::Attributes::OccupiedHeatingSetpoint::Id ||
      attribute_id == Thermostat::Attributes::OccupiedCoolingSetpoint::Id)
  {
    updateHvacSetTemp(from_matter_temperature(value->val.i16));
  }

  return ESP_OK;
}

// Handle CHIP network, server, and commissioning events relevant to Matter.
static void on_device_event(const ChipDeviceEvent *event, intptr_t arg)
{
  switch (event->Type)
  {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
      WifiMatterStarted();
      ESP_LOGI(TAG, "Matter network address changed");
      break;
    case chip::DeviceLayer::DeviceEventType::kServerReady:
      chip::Logging::SetLogFilter(chip::Logging::kLogCategory_Error);
      matter_server_ready.store(true);
      ESP_LOGI(TAG, "Matter server ready");
      break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
      ESP_LOGI(TAG, "Matter commissioning complete");
      break;
    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
      ESP_LOGW(TAG, "Matter commissioning failed; fail-safe timer expired");
      break;
    default:
      break;
  }
}

// Report one Thermostat cluster attribute and log any reporting error.
esp_err_t report_attribute(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                           esp_matter_attr_val_t *value)
{
  const esp_err_t err = attribute::report(endpoint_id, cluster_id, attribute_id, value);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to report Matter attribute 0x%08" PRIx32
                  " in cluster 0x%08" PRIx32 " on endpoint %u: %s",
             attribute_id, cluster_id, endpoint_id, esp_err_to_name(err));
  }
  return err;
}

} // namespace

// Create the Matter node and endpoints, start the server, and prepare the QR and
// manual pairing codes. Returns false if Matter is disabled, Wi-Fi credentials
// are missing, or node, endpoint, or server startup fails.
bool MatterInit()
{
  //
  // The ESP-Matter component’s log tag for its attribute data-model code—for example,
  // looking up attribute handles and reporting or updating attribute values. It’s
  // separate from CHIP’s EM and IM log modules.
  //
  esp_log_level_set("esp_matter_attribute", ESP_LOG_WARN);

  //
  // All module initials can be found in the #define CHIP_LOGMODULES_ENUMERATE in
  // the Constants.h file under 'managed_components/espressif__esp_matter/connectedhomeip/connectedhomeip/src/lib/support/logging/Constants.h'.
  //


  if (!OperatingParameters.MatterEnabled)
  {
    ESP_LOGW(TAG, "Matter initialization requested while Matter is disabled");
    return false;
  }
  if (WifiCreds.ssid[0] == '\0')
  {
    ESP_LOGW(TAG, "Matter needs Wi-Fi credentials; configure Wi-Fi before enabling Matter");
    return false;
  }

  chip::Logging::SetLogFilter(chip::Logging::kLogCategory_Max);
  matter_server_ready.store(false);
  last_reported_state_valid = false;
  last_reported_presence_valid = false;
  matter_cooling_enabled = OperatingParameters.hvacCoolEnable;
  matter_fan_enabled = OperatingParameters.hvacFanEnable;
  matter_two_stage_heat_enabled = OperatingParameters.hvac2StageHeatEnable;
  WifiSetCredentials(WifiCreds.ssid, WifiCreds.password);

  node::config_t node_config;
  node_t *node = node::create(&node_config, on_attribute_update, nullptr);
  if (node == nullptr)
  {
    ESP_LOGE(TAG, "Failed to create Matter node");
    return false;
  }

  thermostat::config_t thermostat_config;
  thermostat_config.thermostat.local_temperature =
      to_matter_temperature(OperatingParameters.tempCurrent + OperatingParameters.tempCorrection);
  thermostat_config.thermostat.system_mode = to_matter_system_mode(OperatingParameters.hvacSetMode);
  thermostat_config.thermostat.control_sequence_of_operation =
      static_cast<uint8_t>(matter_cooling_enabled
                               ? Thermostat::ControlSequenceOfOperationEnum::kCoolingAndHeating
                               : Thermostat::ControlSequenceOfOperationEnum::kHeatingOnly);
  thermostat_config.thermostat.feature_flags = cluster::thermostat::feature::heating::get_id();
  if (matter_cooling_enabled)
  {
    thermostat_config.thermostat.feature_flags |=
        cluster::thermostat::feature::cooling::get_id() |
        cluster::thermostat::feature::auto_mode::get_id();
  }
  thermostat_config.thermostat.features.heating.occupied_heating_setpoint =
      to_matter_temperature(OperatingParameters.tempSet);
  if (matter_cooling_enabled)
  {
    thermostat_config.thermostat.features.cooling.occupied_cooling_setpoint =
        to_matter_temperature(OperatingParameters.tempSet);
  }
  endpoint_t *thermostat_endpoint =
      thermostat::create(node, &thermostat_config, ENDPOINT_FLAG_NONE, nullptr);
  if (thermostat_endpoint == nullptr)
  {
    ESP_LOGE(TAG, "Failed to create Matter thermostat endpoint");
    return false;
  }
  thermostat_endpoint_id = endpoint::get_id(thermostat_endpoint);

  cluster_t *thermostat_cluster = cluster::get(thermostat_endpoint, Thermostat::Id);
  const uint8_t hvac_system_type =
      (matter_cooling_enabled ? kOneCoolingStage : 0x00) |
      (matter_two_stage_heat_enabled ? kTwoHeatingStages : kOneHeatingStage);
  attribute_t *hvac_system_type_attribute =
      attribute::create(thermostat_cluster, Thermostat::Attributes::HVACSystemTypeConfiguration::Id,
                        ATTRIBUTE_FLAG_NONE, esp_matter_bitmap8(hvac_system_type));
  if (hvac_system_type_attribute == nullptr)
  {
    ESP_LOGE(TAG, "Failed to create Matter HVAC system type attribute");
    return false;
  }

  cluster::relative_humidity_measurement::config_t humidity_config;
  humidity_config.measured_value =
      to_matter_relative_humidity(OperatingParameters.humidCurrent +
                                  OperatingParameters.humidityCorrection);
  cluster_t *humidity_cluster =
      cluster::relative_humidity_measurement::create(thermostat_endpoint, &humidity_config,
                                                      CLUSTER_FLAG_SERVER);
  if (humidity_cluster == nullptr)
  {
    ESP_LOGE(TAG, "Failed to create Matter relative-humidity cluster");
    return false;
  }

  occupancy_sensor::config_t presence_config;
  presence_config.occupancy_sensing.occupancy =
      OperatingParameters.presenceDetected
          ? static_cast<uint8_t>(OccupancySensing::OccupancyBitmap::kOccupied)
          : 0;
  presence_config.occupancy_sensing.feature_flags =
      cluster::occupancy_sensing::feature::radar::get_id();
  endpoint_t *presence_endpoint =
      occupancy_sensor::create(node, &presence_config, ENDPOINT_FLAG_NONE, nullptr);
  if (presence_endpoint == nullptr)
  {
    ESP_LOGE(TAG, "Failed to create Matter presence-sensor endpoint");
    return false;
  }
  presence_endpoint_id = endpoint::get_id(presence_endpoint);

  const esp_err_t err = esp_matter::start(on_device_event);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to start Matter: %s", esp_err_to_name(err));
    return false;
  }

  char qr_code[33] = {};
  MutableCharSpan qr_span(qr_code, sizeof(qr_code));
  CHIP_ERROR chip_err = GetQRCode(qr_span, RendezvousInformationFlag::kOnNetwork);
  if (chip_err == CHIP_NO_ERROR)
  {
    strlcpy(OperatingParameters.MatterQR, qr_code, sizeof(OperatingParameters.MatterQR));
  }
  else
  {
    ESP_LOGE(TAG, "Failed to generate Matter QR code: %" CHIP_ERROR_FORMAT, chip_err.Format());
  }

  char pairing_code[12] = {};
  MutableCharSpan pairing_span(pairing_code, sizeof(pairing_code));
  chip_err = GetManualPairingCode(pairing_span, RendezvousInformationFlag::kOnNetwork);
  if (chip_err == CHIP_NO_ERROR)
  {
    strlcpy(OperatingParameters.MatterPairingCode, pairing_code,
            sizeof(OperatingParameters.MatterPairingCode));
  }
  else
  {
    ESP_LOGE(TAG, "Failed to generate Matter manual pairing code: %" CHIP_ERROR_FORMAT, chip_err.Format());
  }

  WifiMatterStarted();
  ESP_LOGI(TAG, "Matter started on thermostat endpoint %u and presence endpoint %u",
           thermostat_endpoint_id, presence_endpoint_id);
  return true;
}

// Delegate Matter fabric and commissioning-data reset to ESP-Matter.
void MatterFactoryReset()
{
  matter_server_ready.store(false);
  OperatingParameters.MatterStarted = false;
  const esp_err_t err = esp_matter::factory_reset();
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to reset Matter configuration: %s", esp_err_to_name(err));
  }
}

// Report thermostat attributes, humidity, and presence when Matter is started
// and server-ready. Sends initial values and subsequent changes.
void MatterUpdate()
{
  if (!OperatingParameters.MatterStarted || !matter_server_ready.load())
  {
    return;
  }

  const int16_t local_temperature =
      to_matter_temperature(OperatingParameters.tempCurrent + OperatingParameters.tempCorrection);
  nullable<uint16_t> humidity =
      to_matter_relative_humidity(OperatingParameters.humidCurrent +
                                  OperatingParameters.humidityCorrection);
  const int16_t setpoint = to_matter_temperature(OperatingParameters.tempSet);
  const uint8_t system_mode = to_matter_system_mode(OperatingParameters.hvacSetMode);

  if (!last_reported_state_valid || local_temperature != last_local_temperature)
  {
    esp_matter_attr_val_t value = esp_matter_nullable_int16(nullable<int16_t>(local_temperature));
    if (report_attribute(thermostat_endpoint_id, Thermostat::Id,
                         Thermostat::Attributes::LocalTemperature::Id, &value) == ESP_OK)
    {
      last_local_temperature = local_temperature;
    }
  }
  const bool humidity_changed =
      !last_reported_state_valid || humidity.is_null() != last_humidity.is_null() ||
      (!humidity.is_null() && humidity.value() != last_humidity.value());
  if (humidity_changed)
  {
    esp_matter_attr_val_t value = esp_matter_nullable_uint16(humidity);
    if (report_attribute(thermostat_endpoint_id, RelativeHumidityMeasurement::Id,
                         RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &value) == ESP_OK)
    {
      last_humidity = humidity;
    }
  }
  const bool presence_detected = OperatingParameters.presenceDetected;
  if (!last_reported_presence_valid || presence_detected != last_reported_presence)
  {
    const uint8_t occupancy = presence_detected
                                  ? static_cast<uint8_t>(OccupancySensing::OccupancyBitmap::kOccupied)
                                  : 0;
    esp_matter_attr_val_t value = esp_matter_bitmap8(occupancy);
    if (report_attribute(presence_endpoint_id, OccupancySensing::Id,
                         OccupancySensing::Attributes::Occupancy::Id, &value) == ESP_OK)
    {
      last_reported_presence = presence_detected;
      last_reported_presence_valid = true;
    }
  }
  if (!last_reported_state_valid || setpoint != last_heating_setpoint)
  {
    esp_matter_attr_val_t value = esp_matter_int16(setpoint);
    if (report_attribute(thermostat_endpoint_id, Thermostat::Id,
                         Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &value) == ESP_OK)
    {
      last_heating_setpoint = setpoint;
    }
  }
  if (matter_cooling_enabled &&
      (!last_reported_state_valid || setpoint != last_cooling_setpoint))
  {
    esp_matter_attr_val_t value = esp_matter_int16(setpoint);
    if (report_attribute(thermostat_endpoint_id, Thermostat::Id,
                         Thermostat::Attributes::OccupiedCoolingSetpoint::Id, &value) == ESP_OK)
    {
      last_cooling_setpoint = setpoint;
    }
  }
  if (!last_reported_state_valid || system_mode != last_system_mode)
  {
    esp_matter_attr_val_t value = esp_matter_enum8(system_mode);
    if (report_attribute(thermostat_endpoint_id, Thermostat::Id,
                         Thermostat::Attributes::SystemMode::Id, &value) == ESP_OK)
    {
      last_system_mode = system_mode;
    }
  }

  last_reported_state_valid = true;
}

#endif
