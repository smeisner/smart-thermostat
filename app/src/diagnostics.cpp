/*!
// SPDX-License-Identifier: GPL-3.0-only
/*
 * diagnostics.c
 *
 * Provide diagnostic information about devices found on the I2C bus and limited info about memory.
 *
 * Copyright (c) 2023 Steve Meisner (steve@meisners.net)
 * 
 * Notes:
 * I2C slave Address Scanner
 * for 5V bus:
 *  Connect a 4.7k resistor between SDA and Vcc
 *  Connect a 4.7k resistor between SCL and Vcc
 * for 3.3V bus
 *  Connect a 2.4k resistor between SDA and Vcc
 *  Connect a 2.4k resistor between SCL and Vcc
 *
 * History
 *  17-Aug-2023: Steve Meisner (steve@meisners.net) - Initial version
 * 
 */

#include "thermostat.hpp"
#include <Wire.h>

TwoWire I2Cbus = TwoWire(0);

void scan_i2c()
{
    Serial.println("Scanning I2C Addresses Channel 1");
    uint8_t cnt=0;
    for (uint8_t i=0;i<128;i++)
    {
        I2Cbus.beginTransmission(i);
        uint8_t ec = I2Cbus.endTransmission(true);
        if (ec == 0)
        {
            if (i < 16)
                Serial.print('0');
            Serial.print (i,HEX);
            cnt++;
        }
        else
        {
            Serial.print("..");
        }

        Serial.print(' ');

        if ((i & 0x0f) == 0x0f)
            Serial.println();
    }
    Serial.print("Scan Completed, ");
    Serial.print(cnt);
    Serial.println(" I2C Devices found.");
}

void scanI2cBus()
{
  I2Cbus.end();
  I2Cbus.begin(SDA, SCL, 400000UL);
  scan_i2c();
  Serial.println();
  I2Cbus.end();
}

void showConfigurationData()
{
  log_d("Total heap:  %d", ESP.getHeapSize());
  log_d("Free heap:   %d", ESP.getFreeHeap());
  log_d("Total PSRAM: %d", ESP.getPsramSize());
  log_d("Free PSRAM:  %d", ESP.getFreePsram());

  scanI2cBus();
}
