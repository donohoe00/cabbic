#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cabbic/api.h>
#include <Wire.h>
#include <si5351.h>

#define VCC_VOLTAGE     5

#define PIN_GND         56
#define PIN_VCC         54

static uint8_t vcc_pins[] = {
    PIN_VCC
};

static uint8_t gnd_pins[] = {
    PIN_GND
};

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
