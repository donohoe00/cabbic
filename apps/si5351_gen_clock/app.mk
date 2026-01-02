OBJS += lib/si5351.o lib/Wire.o lib/i2c.o
LD=c++
CFLAGS += -DI2C_PIN_SDA=3 -DI2C_PIN_SCL=4
