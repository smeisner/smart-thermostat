/*!
 * @file DFRobot_AHT20.cpp
 * @brief This AHT20 temperature & humidity sensor employs digital output and I2C interface, through which users can read the measured temperature and humidity. The AHT20 chip-based sensor offers the following features:
 * @n 1. Collect ambient temperature, unit Celsius (℃), range -40-85℃, resolution: 0.01, error: ±0.3-±1.6℃
 * @n 2. Collect ambient relative humidity, unit: %RH, range 0-100%RH, resolution 0.024%RH, error: when the temprature is 25℃, error range is ±2-±5%RH
 * @n 3. Use I2C interface, I2C address is default to be 0x38
 * @n 4. uA level sensor, the measured value is up to 200uA.
 * @n 5. Power supply range 3.3-5V
 *
 * @copyright   Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author [Arya](xue.peng@dfrobot.com)
 * @version  V1.0
 * @date  2022-02-09
 * @url https://github.com/DFRobot/DFRobot_AHT20
 */

#include "DFRobot_AHT20.h"

#ifndef DBG

#if 0  //< Change 0 to 1 to open debug macro and check program debug information
#define DBG(...) {Serial.print("["); Serial.print(__FUNCTION__); Serial.print("(): "); Serial.print(__LINE__); Serial.print(" ] "); Serial.println(__VA_ARGS__);}
#else
#define DBG(...)
#endif

#endif 

#define AHT20_DEF_I2C_ADDR           0x38  ///< Default I2C address of AHT20 sensor 
#define CMD_INIT                     0xBE  ///< Init command
#define CMD_INIT_PARAMS_1ST          0x08  ///< The first parameter of init command: 0x08
#define CMD_INIT_PARAMS_2ND          0x00  ///< The second parameter of init command: 0x00
#define CMD_INIT_TIME                10    ///< Waiting time for init completion: 10ms
#define CMD_MEASUREMENT              0xAC  ///< Trigger measurement command
#define CMD_MEASUREMENT_PARAMS_1ST   0x33  ///< The first parameter of trigger measurement command: 0x33
#define CMD_MEASUREMENT_PARAMS_2ND   0x00  ///< The second parameter of trigger measurement command: 0x00
#define CMD_MEASUREMENT_TIME         80    ///< Measurement command completion time: 80ms
#define CMD_MEASUREMENT_DATA_LEN     6     ///< Return length when the measurement command is without CRC check.
#define CMD_MEASUREMENT_DATA_CRC_LEN 7     ///< Return data length when the measurement command is with CRC check.
#define CMD_SOFT_RESET               0xBA  ///< Soft reset command
#define CMD_SOFT_RESET_TIME          20    ///< Soft reset time: 20ms
#define CMD_STATUS                   0x71  ///< Get status word command

union uStatusBit_t{
  struct{
	uint8_t rsv0        : 3; /**< reserve bits */
    uint8_t calEnable : 1; /**< calibrate enble bit: 1 - calibrated, 0 - not calibrated */
    uint8_t rsv1      : 3; /**< reserve bits */
    uint8_t busy      : 1; /**< Busy indication 1 - the device is busy, in measurement status, 0 - the device is idle, in hibernation status */
  };
  uint8_t status;          /**< status bit */
};

DFRobot_AHT20::DFRobot_AHT20(TwoWire &wire)
  :_pWire(&wire), _addr(AHT20_DEF_I2C_ADDR), _temperature(0.0), _humidity(0.0), _init(false){}

uint8_t DFRobot_AHT20::begin(){
  //step1: Determine whether _pWire is valid, if not, return 1, if valid, initialize the bus
  if(!_pWire) {
	  DBG("_pWire is NULL!");
	  return 1;
  }
  if(!_init)  _pWire->begin();
  //step2: Determine whether the device is connected, return 2 if not connected
  _pWire->beginTransmission(_addr);
  uint8_t ret = _pWire->endTransmission();
  if(ret != 0){
	  DBG("Device not found, please check if the device is connected.");
	  return 2;
  }
  //step3: Init, if it failed, return 3
  if(!init()){
	  DBG("SHT20 initialization fail.");
	  return 3;
  }
  //If AHT20 init succeeded, return 0
  _init = true;
  return 0;
	
}

void DFRobot_AHT20::reset(){
  if(!_init)  _pWire->begin();
  writeCommand(CMD_SOFT_RESET);
  delay(CMD_SOFT_RESET_TIME);
  //getStatusData();
  _init = true;
}

bool DFRobot_AHT20::startMeasurementReady(bool crcEn){
  uint8_t *pData;
  uint8_t recvLen = CMD_MEASUREMENT_DATA_LEN;
  uint32_t temp = 0;
  uStatusBit_t status ;
  if(!ready()){
	  DBG("Not cailibration.");
	  return false;
  }
  if(crcEn) recvLen = CMD_MEASUREMENT_DATA_CRC_LEN;
  if((pData = (uint8_t *)malloc(recvLen)) == NULL){
	  DBG("pdata malloc failed.");
	  return false;
  }
  memset(pData, 0, recvLen);
  writeCommand(CMD_MEASUREMENT, CMD_MEASUREMENT_PARAMS_1ST, CMD_MEASUREMENT_PARAMS_2ND);
  //getStatusData();
  delay(CMD_MEASUREMENT_TIME);
  _pWire->requestFrom(_addr, recvLen);
  for(int i = 0; i < recvLen; i++){
    pData[i] = (uint8_t)_pWire->read();
	  DBG(pData[i], HEX);
  }

  status.status = pData[0];
  if(status.busy){
	  DBG("AHT20 is busy!");
	  free(pData);
	  return false;
  }
  if(crcEn && (!checkCRC8(pData[6], &pData[0], CMD_MEASUREMENT_DATA_LEN))) {
  	DBG("CRC check failed.");
	  free(pData);
  	return false;
  }
  temp = pData[1];
  temp <<= 8;
  temp |= pData[2];
  temp <<= 4;
  temp = temp | (pData[3] >> 4);
  _humidity = (float)(temp * 100) / (float)0x100000;  

  temp = pData[3] & 0x0F;
  temp <<= 8;
  temp |= pData[4];
  temp <<= 8;
  temp |= pData[5];
  _temperature = (float)(temp * 200) / (float)0x100000 - 50;
  free(pData);
  return true;
}

float DFRobot_AHT20::getTemperature_F(){
  return _temperature * 1.8 + 32;
}
float DFRobot_AHT20::getTemperature_C(){
  return _temperature;
}
float DFRobot_AHT20::getHumidity_RH(){
  return _humidity;
}

bool DFRobot_AHT20::checkCRC8(uint8_t crc8, uint8_t *pData, uint8_t len){
  //CRC initial value: 0xFF
  //CRC8 check polynomial: CRC[7: 0] = X8 + X5 + X4 + 1  -  0x1 0011 0001 - 0x131
  uint8_t crc = 0xFF;
  for(int pos = 0; pos < len; pos++){
	  crc ^= pData[pos];
	  for(int i = 8; i > 0; i--){
		  if(crc & 0x80){
			  crc <<= 1;
			  crc ^= 0x31;
		  }else{
			  crc <<= 1;
		  }
	  }

  }
  DBG(crc,HEX);
  if(crc8 == crc) return true;
  return false;
}

bool DFRobot_AHT20::ready(){
  union uStatusBit_t status;
  status.status = getStatusData();
  if(status.busy) return false;
  return true;
}

bool DFRobot_AHT20::init(){
  union uStatusBit_t status;
  status.status = getStatusData();
  if(status.calEnable) return true;

  writeCommand(CMD_INIT, CMD_INIT_PARAMS_1ST, CMD_INIT_PARAMS_2ND);
  delay(CMD_INIT_TIME);
  status.status = getStatusData();
  if(status.calEnable) return true;
 
  return false;
}

uint8_t DFRobot_AHT20::getStatusData(){
  uint8_t status = 0;
  readData(CMD_STATUS, &status, 1);
  DBG(status, HEX);
  return status;
}

void DFRobot_AHT20::writeCommand(uint8_t cmd){
  _pWire->beginTransmission(_addr);
  _pWire->write(cmd);
  _pWire->endTransmission();
}

void DFRobot_AHT20::writeCommand(uint8_t cmd, uint8_t args1, uint8_t args2){
   _pWire->beginTransmission(_addr);
   _pWire->write(cmd);
   _pWire->write(args1);
   _pWire->write(args2);
   _pWire->endTransmission();
}

uint8_t DFRobot_AHT20::readData(uint8_t cmd, void* pBuf, size_t size){
  if(pBuf == NULL){
    DBG("pBuf ERROR!! : null pointer");
    return 0;
  }
  uint8_t *_pBuf = (uint8_t *)pBuf;
  _pWire->beginTransmission(_addr);
  _pWire->write(cmd);
  if(_pWire->endTransmission() != 0){
      return 0;
  }
  _pWire->requestFrom(_addr, (uint8_t) size);
  for(uint16_t i = 0; i < size; i++){
    _pBuf[i] = (char)_pWire->read();
  }
  return size;
}