#include <stdint.h>

#include <cabbic/i2c.h>
#include <Wire.h>

static int i2c_error = 0;
static int bytes_available = 0, read_ptr = 0;
static uint8_t read_buffer[256];

// "Wire" interface callbacks
static void
i2c_begin()
{
}

void
i2c_begin_transmission(unsigned i2c_addr)
{
    i2c_error = I2C_ERROR_NONE;

    cabbic_i2c_start();

    uint8_t ack = cabbic_i2c_write_byte(i2c_addr << 1);

    if (ack) {
        i2c_error = I2C_ERROR_ADDR_NACK;
    }
}

unsigned
i2c_end_transmission()
{
    cabbic_i2c_stop();

    return i2c_error;
}

void
i2c_write(unsigned value)
{
    uint8_t ack = cabbic_i2c_write_byte(value);

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

    cabbic_i2c_start();

    cabbic_i2c_write_byte(i2c_addr<<1 | 1);

    for (unsigned i = 0; i < quantity; i++) {
        read_buffer[i] = cabbic_i2c_read_byte(i == quantity-1 ? 1 : 0);
    }

    cabbic_i2c_stop();

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
