#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <api.h>
#include <Wire.h>
#include <si5351.h>

#define VCC_VOLTAGE     3.3

#define PIN_SDA         3
#define PIN_SCL         4

// Return codes for i2c_begin_transmission()
#define I2C_ERROR_NONE      0
#define I2C_ERROR_TOO_LONG  1
#define I2C_ERROR_ADDR_NACK 2
#define I2C_ERROR_DATA_NACK 3
#define I2C_ERROR_OTHER     4
#define I2C_ERROR_TIMEOUT   5

static uint8_t vcc_pins[] = {
    1
};

static uint8_t gnd_pins[] = {
    2
};

static uint8_t i2c_pins[2] = { PIN_SCL, PIN_SDA };

static int i2c_error = 0;
static int bytes_available = 0, read_ptr = 0;
static uint8_t read_buffer[256];

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
    cab_io_pin_mode(PIN_SCL, val ? CAB_PMODE_1 : CAB_PMODE_0);
}

static void
set_sda(uint8_t val)
{
    cab_io_pin_mode(PIN_SDA, val ? CAB_PMODE_1 : CAB_PMODE_0);
}

static void
set_sda_input()
{
    cab_io_pin_mode(PIN_SDA, CAB_PMODE_Z);
}

static void
i2c_start()
{
    set_i2c_pins(1, 1);
    set_sda(0);
}

static void
i2c_stop()
{
    set_i2c_pins(1, 0);
    set_sda(1);
}

static void
i2c_begin()
{
}

static uint8_t
i2c_write_byte(uint8_t data)
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
    uint8_t data_pin = PIN_SDA;
    cab_io_read(&data_pin, &input, 1);

    return input & 1;
}

static uint8_t
i2c_read_byte(uint8_t ack)
{
    set_sda_input();

    uint8_t data_pin = PIN_SDA;
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
i2c_begin_transmission(unsigned i2c_addr)
{
    i2c_error = I2C_ERROR_NONE;

    i2c_start();

    uint8_t ack = i2c_write_byte(i2c_addr << 1);

    if (ack) {
        i2c_error = I2C_ERROR_ADDR_NACK;
    }
}

unsigned
i2c_end_transmission()
{
    i2c_stop();

    return i2c_error;
}

void
i2c_write(unsigned value)
{
    uint8_t ack = i2c_write_byte(value);

    if (ack) {
        i2c_error = I2C_ERROR_DATA_NACK;
    }
}

void
i2c_request_from(unsigned i2c_addr, unsigned quantity)
{
    if (quantity > sizeof read_buffer) {
        i2c_error = I2C_ERROR_TOO_LONG;
        return;
    }

    i2c_error = I2C_ERROR_NONE;

    i2c_start();

    i2c_write_byte(i2c_addr<<1 | 1);

    for (unsigned i = 0; i < quantity; i++) {
        read_buffer[i] = i2c_read_byte(i == quantity-1 ? 1 : 0);
    }

    i2c_stop();

    bytes_available = quantity;
    read_ptr = 0;
}

unsigned
i2c_available()
{
    return bytes_available;
}

unsigned
i2c_read()
{
    if (bytes_available > 0) {
        bytes_available--;
        return read_buffer[read_ptr++];
    } else {
        return 0xff;
    }
}

i2c_wire Wire = {
    i2c_begin,
    i2c_begin_transmission,
    i2c_end_transmission,
    i2c_write,
    i2c_request_from,
    i2c_available,
    i2c_read
};

#ifdef DEBUG
static void
write_register(uint8_t i2c_addr, uint8_t reg, uint8_t val)
{
    i2c_start();
    i2c_write_byte(i2c_addr<<1);
    i2c_write_byte(reg);
    i2c_write_byte(val);
    i2c_stop();
}

static uint8_t
read_register(uint8_t i2c_addr, uint8_t reg)
{
    uint8_t val;

    i2c_start();
    i2c_write_byte(i2c_addr<<1);
    i2c_write_byte(reg);
    i2c_stop();

    i2c_start();
    i2c_write_byte(i2c_addr<<1 | 1);
    val = i2c_read_byte(1);
    i2c_stop();

    return val;
}
#endif

extern "C" cab_err_e
app_run(int argc, char **argv)
{
    cab_err_e err;
    Si5351 si;
    unsigned clock_freq_hz;

    if (argc != 2) {
        printf("Usage: %s <clock0_freq_in_hz>\n", argv[0]);
        return CAB_ERR_BAD_ARGS;
    }

    clock_freq_hz = strtol(argv[1], NULL, 0);

    if ((err = cab_reset(gnd_pins, sizeof gnd_pins,
      vcc_pins, sizeof vcc_pins, NULL, 0, VCC_VOLTAGE, 0)) != CAB_ERR_NONE) {
        return err;
    }

    if (!si.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0)) {
        fprintf(stderr, "si5351 device not found on the I2C bus\n");
        return CAB_ERR_IO;
    }

    si.set_freq(clock_freq_hz, SI5351_CLK0);

    return CAB_ERR_NONE;
}
