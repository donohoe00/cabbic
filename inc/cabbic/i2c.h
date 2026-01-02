#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void cabbic_i2c_start();
void cabbic_i2c_stop();
uint8_t cabbic_i2c_write_byte(uint8_t data);
uint8_t cabbic_i2c_read_byte(uint8_t ack);
void cabbic_i2c_write_register(uint8_t i2c_addr, uint8_t reg, uint8_t val);
uint8_t cabbic_i2c_read_register(uint8_t i2c_addr, uint8_t reg);

#ifdef __cplusplus
};
#endif
