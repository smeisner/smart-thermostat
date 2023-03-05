//
// https://github.com/Gozem/am2320/blob/master/am2321.c
//

#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h> 
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <stdint.h>


#define I2C_DEVICE "/dev/i2c-1"
#define AM2321_ADDR 0x5C


static uint16_t 
_calc_crc16(const uint8_t *buf, size_t len) {
  uint16_t crc = 0xFFFF;
  
  while(len--) {
    crc ^= (uint16_t) *buf++;
    for (unsigned i = 0; i < 8; i++) {
      if (crc & 0x0001) {
	crc >>= 1;
	crc ^= 0xA001;
      } else {
	crc >>= 1;      
      }
    }
  }
  
  return crc;
}

static uint16_t
_combine_bytes(uint8_t msb, uint8_t lsb)
{
  return ((uint16_t)msb << 8) | (uint16_t)lsb;
}

int 
am2321(float *out_temperature, float *out_humidity) 
{
  int fd;
  uint8_t data[8];

  fd = open(I2C_DEVICE, O_RDWR);
  if (fd < 0)
    return 1;

  if (ioctl(fd, I2C_SLAVE, AM2321_ADDR) < 0)
    return 2;
   
  /* wake AM2320 up, goes to sleep to not warm up and
   * affect the humidity sensor 
   */
  write(fd, NULL, 0);
  usleep(1000); /* at least 0.8ms, at most 3ms */
  
  /* write at addr 0x03, start reg = 0x00, num regs = 0x04 */
  data[0] = 0x03; 
  data[1] = 0x00; 
  data[2] = 0x04;
  if (write(fd, data, 3) < 0)
    return 3;
  
  /* wait for AM2320 */
  usleep(1600); /* Wait atleast 1.5ms */
  
  /*
   * Read out 8 bytes of data
   * Byte 0: Should be Modbus function code 0x03
   * Byte 1: Should be number of registers to read (0x04)
   * Byte 2: Humidity msb
   * Byte 3: Humidity lsb
   * Byte 4: Temperature msb
   * Byte 5: Temperature lsb
   * Byte 6: CRC lsb byte
   * Byte 7: CRC msb byte
   */
  if (read(fd, data, 8) < 0)
    return 4;
  
  close(fd);

  //printf("[0x%02x 0x%02x  0x%02x 0x%02x  0x%02x 0x%02x  0x%02x 0x%02x]\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7] );

  /* Check data[0] and data[1] */
  if (data[0] != 0x03 || data[1] != 0x04)
    return 9;

  /* Check CRC */
  uint16_t crcdata = _calc_crc16(data, 6);
  uint16_t crcread = _combine_bytes(data[7], data[6]);
  if (crcdata != crcread) 
    return 10;

  uint16_t temp16 = _combine_bytes(data[4], data[5]); 
  uint16_t humi16 = _combine_bytes(data[2], data[3]);   
  //printf("temp=%u 0x%04x  hum=%u 0x%04x\n", temp16, temp16, humi16, humi16);
  
  /* Temperature resolution is 16Bit, 
   * temperature highest bit (Bit15) is equal to 1 indicates a
   * negative temperature, the temperature highest bit (Bit15)
   * is equal to 0 indicates a positive temperature; 
   * temperature in addition to the most significant bit (Bit14 ~ Bit0)
   *  indicates the temperature sensor string value.
   * Temperature sensor value is a string of 10 times the
   * actual temperature value.
   */
  if (temp16 & 0x8000)
    temp16 = -(temp16 & 0x7FFF);

  *out_temperature = (float)temp16 / 10.0;
  *out_humidity = (float)humi16 / 10.0;

  return 0;
}

int get_sensor_readings(void) {
  float temp, humi;

  int ret = am2321(&temp, &humi);
  if (ret) {
    printf("Err=%d\n", ret);
    return ret;
  }

  printf( "Temperature %.1f [C]\n", temp);
  printf( "Humidity    %.1f [%%]\n", humi);
  
  return 0;
}
