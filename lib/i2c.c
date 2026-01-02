// Generic I2C bitbanging routines.
//
// To use this module, you must define I2C_PIN_SDA and I2C_PIN_SCL, e.g. in
// the Makefile via the -D compiler directive.  These are the pins of the T48
// which are connected to the Data and Clock lines, respectively, of the I2C
// bus.

#include <stdio.h>
#include <cabbic/api.h>
#include <cabbic/i2c.h>

static uint8_t i2c_pins[2] = { I2C_PIN_SCL, I2C_PIN_SDA };

// A general assumption is that the message round trip time is long enough
// that we don't need to add delays between updating pins.

static void
set_i2c_pins(uint8_t scl, uint8_t sda)
{
    cab_err_e err;
    cab_pin_mode_e pin_modes[2];

    pin_modes[0] = (sda ? CAB_PMODE_1 : CAB_PMODE_0);
    pin_modes[1] = (scl ? CAB_PMODE_1 : CAB_PMODE_0);

    if ((err = cab_io_pin_modes(i2c_pins,
      pin_modes, sizeof i2c_pins)) != CAB_ERR_NONE) {
        fprintf(stderr, "set_i2c_pins(): %s\n", cab_sterror(err));
    }
}

static void
set_scl(uint8_t val)
{
    cab_io_pin_mode(I2C_PIN_SCL, val ? CAB_PMODE_1 : CAB_PMODE_0);
}

static void
set_sda(uint8_t val)
{
    cab_io_pin_mode(I2C_PIN_SDA, val ? CAB_PMODE_1 : CAB_PMODE_0);
}

static void
set_sda_input()
{
    cab_io_pin_mode(I2C_PIN_SDA, CAB_PMODE_Z);
}

void
cabbic_i2c_start()
{
    set_i2c_pins(1, 1);
    set_sda(0);
}

void
cabbic_i2c_stop()
{
    set_i2c_pins(1, 0);
    set_sda(1);
}

uint8_t
cabbic_i2c_write_byte(uint8_t data)
{
    for (int i = 7; i >= 0; i--) {
        set_scl(0);
        set_sda((data >> i) & 1);
        set_scl(1);
    }

    set_scl(0);
    set_sda_input();
    set_scl(1);

    // Read ACK
    uint8_t input;
    uint8_t data_pin = I2C_PIN_SDA;
    cab_io_read(&data_pin, &input, 1);

    return input & 1;
}

uint8_t
cabbic_i2c_read_byte(uint8_t ack)
{
    set_sda_input();

    uint8_t data_pin = I2C_PIN_SDA;
    uint8_t data, input;
    for (int i = 0; i < 8; i++) {
        set_scl(0);
        cab_io_read(&data_pin, &input, 1);
        data = data << 1 | (input&1);
        set_scl(1);
    }

    // Send ACK if not last byte, or NACK to complete the read sequence
    set_sda(1);
    set_scl(ack);

    return data;
}

void
cabbic_i2c_write_register(uint8_t i2c_addr, uint8_t reg, uint8_t val)
{
    cabbic_i2c_start();
    cabbic_i2c_write_byte(i2c_addr<<1);
    cabbic_i2c_write_byte(reg);
    cabbic_i2c_write_byte(val);
    cabbic_i2c_stop();
}

uint8_t
cabbic_i2c_read_register(uint8_t i2c_addr, uint8_t reg)
{
    uint8_t val;

    cabbic_i2c_start();
    cabbic_i2c_write_byte(i2c_addr<<1);
    cabbic_i2c_write_byte(reg);
    cabbic_i2c_stop();

    cabbic_i2c_start();
    cabbic_i2c_write_byte(i2c_addr<<1 | 1);
    val = cabbic_i2c_read_byte(1);
    cabbic_i2c_stop();

    return val;
}
