
#include "thermostat.hpp"

static const char *__hvac_mode_to_str(HVAC_MODE mode, const char *mode_str[])
{
  switch (mode) {
  case OFF:
  case HEAT:
  case COOL:
  case DRY:
  case IDLE:
  case AUTO:
  case FAN_ONLY:
  case AUX_HEAT:
    return mode_str[mode];
  default:
    return mode_str[ERROR];
  }
}

#ifdef MQTT_ENABLED
const char *hvac_op_mode_str_mqtt[NR_HVAC_MODES] = {
  "off", "heat", "cool", "dry", "idle", "fan_only", "auto", "aux heat", "error"
};
const char *hvac_curr_mode_str_mqtt[NR_HVAC_MODES] = {
  "off", "heating", "cooling", "drying", "idle", "fan", "error", "error", "error"
};

const char *hvacModeToMqttCurrMode(HVAC_MODE mode)
{
  return __hvac_mode_to_str(mode, hvac_curr_mode_str_mqtt);
}

const char *hvacModeToMqttOpMode(HVAC_MODE mode)
{
  return __hvac_mode_to_str(mode, hvac_op_mode_str_mqtt);
}
#endif

const char *hvac_mode_str[NR_HVAC_MODES] = {
  "Off", "Heat", "Cool", "Dry", "Idle", "Fan Only", "Auto", "Aux Heat", "Error"
};

const char *hvacModeToString(HVAC_MODE mode)
{
  return __hvac_mode_to_str(mode, hvac_mode_str);
}

HVAC_MODE strToHvacMode(char *mode)
{
  for (int m = 0; m < NR_HVAC_MODES; m++) {
    if (strcmp(mode, hvacModeToString((HVAC_MODE)m)) == 0)
      return (HVAC_MODE)m;
  }

  return ERROR;
}
