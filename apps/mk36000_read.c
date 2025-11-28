// Read the MK36000 (e.g. the IBM CGA character ROM)

#include <stdio.h>
#include <unistd.h>

#include <api.h>

#define T48_NPINS   40
#define ROM_NPINS   24
#define SKIP        ((T48_NPINS)-(ROM_NPINS))

// Map Pin macro (Device pin to T48 pin)
#define MP(DPIN)    ((DPIN) <= 12 ? (DPIN) : (DPIN) + SKIP)

#define VPP_VOLTAGE     5.0
#define VCC_VOLTAGE     5.0
#define ROM_SIZE        8192

#define PIN_CE  MP(20)

static uint8_t vcc_pins[] = { 
    MP(18), MP(21), MP(24)
};

static uint8_t gnd_pins[] = { 
    MP(12)
};

static uint8_t addr_pins[] = { 
    MP(8), MP(7), MP(6), MP(5), MP(4), MP(3), MP(2), MP(1), MP(23), MP(22), MP(19)
};

static uint8_t data_pins[] = { 
    MP(9), MP(10), MP(11), MP(13), MP(14), MP(15), MP(16), MP(17)
};

static cab_pin_mode_e addr_pin_modes[sizeof addr_pins];
static cab_pin_mode_e data_pin_modes[8];

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

    if ((err = cab_reset(gnd_pins, sizeof gnd_pins,
      vcc_pins, sizeof vcc_pins, NULL, 0, VCC_VOLTAGE, 0)) != CAB_ERR_NONE) {
        return err;
    }

    for (int i = 0; i < 8; i++) {
        data_pin_modes[i] = CAB_PMODE_Z;
    }
    if ((err = cab_io_pin_modes(data_pins,
      data_pin_modes, sizeof data_pins)) != CAB_ERR_NONE) {
        return err;
    }

    cab_io_pin_mode(PIN_CE, CAB_PMODE_1);

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

        usleep(100);

        cab_io_pin_mode(PIN_CE, CAB_PMODE_0);

        uint8_t input[8];
        cab_io_read(data_pins, input, sizeof input);

        cab_io_pin_mode(PIN_CE, CAB_PMODE_1);

        uint8_t byte = 0;
        for (int i = 0; i < 8; i++) {
            byte |= (input[i] ? 1<<i : 0);
        }

        fputc(byte, fout);
    }

    fclose(fout);

    return CAB_ERR_NONE;
}
