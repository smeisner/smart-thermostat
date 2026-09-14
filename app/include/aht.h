#ifdef __cplusplus
extern "C" {
#endif

void aht20_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle);
void aht20_read_temp_hum(i2c_master_dev_handle_t dev_handle, float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif
