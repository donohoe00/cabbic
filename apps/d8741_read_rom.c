#include <stdio.h>
#include <unistd.h>

#include <api.h>

// Use the max. T48 VPP voltage of 25V.  This results in 22V supplied to the
// EA pin.  The D8741 datasheet calls for 23V, but it seems to work fine, at
// least for reading.
#define VPP_VOLTAGE     25.0
#define VCC_VOLTAGE     5.6
#define ROM_SIZE        1024

// D8741 must be connected to the T48 via an adapter which was originally
// designed to work with a Willems programmer:
//
// https://minuszerodegrees.net/willem/Willem%20MCS-48%20adapter%20-%20Circuit%20diagram.png
//
// This software was tested with a unit named "MCS-48 Adapter for LPT Willem
// EPROM programmer", purchased from a user called "mcutools" on a website
// called "EBay".
//
// The adapter supplies an appropriate clock to the XTAL inputs, and does
// some other voltage conversions, resulting in the following mapping:
#define PIN_EA  1
#define PIN_T0  8
#define PIN_A3  9
#define PIN_A2  10
#define PIN_A1  11
#define PIN_A0  12
#define PIN_D0  13
#define PIN_D1  14
#define PIN_D2  15
#define PIN_GND 16
#define PIN_D3  25
#define PIN_D4  26
#define PIN_D5  27
#define PIN_D6  28
#define PIN_D7  29
#define PIN_CE  30  // For applying programming voltages to PROG and Vdd
#define PIN_RST 32
#define PIN_VCC 40

static uint8_t vcc_pins[] = { 
    PIN_VCC
};
static uint8_t vpp_pins[] = { 
    PIN_EA
};
static uint8_t gnd_pins[] = { 
    PIN_GND, PIN_A2, PIN_A3, PIN_CE
};

static uint8_t addr_pins[] = { 
    PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7,
    PIN_A0, PIN_A1
};

static uint8_t data_pins[] = { 
    PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7
};

static cab_pin_mode_e addr_pin_modes[sizeof addr_pins];
static cab_pin_mode_e data_pin_modes[8] = {
    CAB_PMODE_Z, CAB_PMODE_Z, CAB_PMODE_Z, CAB_PMODE_Z,
    CAB_PMODE_Z, CAB_PMODE_Z, CAB_PMODE_Z, CAB_PMODE_Z,
};

cab_err_e
app_run(int argc, char **argv)
{
    cab_err_e err;
    FILE *fout;

    if (argc != 2) {
        return CAB_ERR_BAD_ARGS;
    }

    if ((fout = fopen(argv[1], "w")) == NULL) {
        perror(argv[1]);
        return CAB_ERR_FILE;
    }

    if ((err = cab_reset(gnd_pins, sizeof gnd_pins, vcc_pins, sizeof vcc_pins,
      vpp_pins, sizeof vpp_pins, VCC_VOLTAGE, VPP_VOLTAGE)) != CAB_ERR_NONE) {
        return err;
    }

    usleep(20000);

    cab_io_hold_on();
    cab_io_pin_mode(PIN_T0, CAB_PMODE_0);
    cab_io_pin_mode(PIN_RST, CAB_PMODE_0);
    cab_io_hold_off();

    usleep(5000);

    for (int addr = 0; addr < ROM_SIZE; addr++) {
        // Assert address
        for (int bit = 0; bit < sizeof addr_pins; bit++) {
            addr_pin_modes[bit] =
              (addr & (1<<bit)) ? CAB_PMODE_1 : CAB_PMODE_0;
        }
        if ((err = cab_io_pin_modes(addr_pins,
          addr_pin_modes, sizeof addr_pins)) != CAB_ERR_NONE) {
            return err;
        }

        cab_io_hold_on();
        cab_io_pin_mode(PIN_RST, CAB_PMODE_1);  // Latch address
        cab_io_pin_mode(PIN_T0, CAB_PMODE_1);   // Verify (read) mode
        cab_io_hold_off();

        usleep(1000);

        // Set data pins to input and read them
        cab_io_hold_on();
        if ((err = cab_io_pin_modes(data_pins,
          data_pin_modes, sizeof data_pins)) != CAB_ERR_NONE) {
            return err;
        }

        uint8_t input[8];
        cab_io_read(data_pins, input, sizeof input);

        cab_io_pin_mode(PIN_T0, CAB_PMODE_0);
        usleep(1000);
        cab_io_pin_mode(PIN_RST, CAB_PMODE_0);
        usleep(1000);

        uint8_t byte = 0;
        for (int i = 0; i < 8; i++) {
            byte |= (input[i] ? 1<<i : 0);
        }

        fputc(byte, fout);
    }

    fclose(fout);

    return CAB_ERR_NONE;
}
