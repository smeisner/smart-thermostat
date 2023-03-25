#include "thermostat.hpp"

void eepromReadConfig()
{
    OperatingParameters.hvacMode = IDLE;
    OperatingParameters.tempSet = 68.0;
    OperatingParameters.tempCurrent = 0.0;
    OperatingParameters.humidCurrent = 50.0;
    OperatingParameters.tempUnits = 'F';
    OperatingParameters.tempSwing = 3;
    OperatingParameters.tempCorrection = -5.0;
    OperatingParameters.lightDetected = 1024;
    OperatingParameters.motionDetected = false;
}
