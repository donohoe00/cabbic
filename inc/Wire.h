// Return codes for i2c_begin_transmission()
#define I2C_ERROR_NONE      0
#define I2C_ERROR_TOO_LONG  1
#define I2C_ERROR_ADDR_NACK 2
#define I2C_ERROR_DATA_NACK 3
#define I2C_ERROR_OTHER     4
#define I2C_ERROR_TIMEOUT   5

typedef struct {
    void (*begin)();
    void (*beginTransmission)(unsigned i2c_addr);
    unsigned (*endTransmission)();
    void (*write)(unsigned i2c_addr);
    void (*requestFrom)(unsigned i2c_addr, unsigned quantity);
    unsigned (*available)();
    unsigned (*read)();
} i2c_wire;

extern i2c_wire Wire;
