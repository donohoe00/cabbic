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
