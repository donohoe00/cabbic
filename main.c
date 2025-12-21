#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libusb.h>

#include "api.h"

#define T48_USB_VID 0xA466
#define T48_USB_PID 0x0A53
#define T48_DEVTYPE 7

// 40 ZIF pins plus 16 on front-facing jumper connector
#define T48_MAX_PINS        40

#define T48_CMD_QUERY       0x00
#define T48_CONFIG_AND_READ 0x28
#define T48_RESET_PINS      0x2D
#define T48_SET_VCC_PINS    0x2E
#define T48_SET_VPP_PINS    0x2F
#define T48_SET_GND_PINS    0x30

#define USB_TIMEOUT 5000

typedef struct {
    bool supported;
    uint8_t msg_offset;
    uint8_t bit;
} pin_msg_info_t;

static cab_pin_mode_e io_pin_modes[T48_MAX_PINS];

static libusb_device_handle *usb_handle;
static bool device_never_reset = true;
static bool hold = false;
static bool pullup = false;

static void
usb_errchk(const char *what, int err)
{
    if (err < 0) {
        fprintf(stderr, "%s: %s\n", what, libusb_error_name(err));
        exit(EXIT_FAILURE);
    }
}

const char *
cab_sterror(cab_err_e err)
{
    switch (err) {
        case CAB_ERR_NONE:
            return "No error";
        case CAB_ERR_STATE:
            return "Bad State";
            break;
        case CAB_ERR_OUT_OF_RANGE:
            return "Value Out Of Range";
            break;
        case CAB_ERR_BAD_POINTER:
            return "Bad Pointer";
            break;
        case CAB_ERR_OVERCURRENT:
            return "Overcurrent Detector Triggered";
            break;
        case CAB_ERR_BAD_ARGS:
            return "Bad Command Line Arguments";
            break;
        case CAB_ERR_FILE:
            return "FILE Access Error";
            break;
        case CAB_ERR_IO:
            return "Input/Output Error";
            break;
        default:
            return "Unknown Error";
            break;
    }
}

static void
usb_open()
{
    libusb_device **list, *dev, *found = NULL;
    ssize_t ndevices = libusb_get_device_list(NULL, &list);
    int rc;

    usb_errchk("libusb_get_device_list()", ndevices);

    for (int i = 0; i < ndevices; i++) {
        dev = list[i];
        if (dev == NULL) {
            break;
        }
        
		struct libusb_device_descriptor desc;
		rc = libusb_get_device_descriptor(dev, &desc);
        usb_errchk("libusb_get_device_descriptor()", rc);

        if (desc.idVendor == T48_USB_VID && desc.idProduct == T48_USB_PID) {
            found = dev;
            break;
        }
    }

    if (found == NULL) {
        fprintf(stderr, "No T48 detected\n");
        exit(EXIT_FAILURE);
    } else {
        rc = libusb_open(dev, &usb_handle);
        usb_errchk("libusb_open()", rc);
    }

    libusb_free_device_list(list, 1);
}

static int
transact(uint8_t *out, int outl, uint8_t *in, int inl)
{
    int rc, nbytes;

    if (out && outl > 0) {
        rc = libusb_bulk_transfer(usb_handle, LIBUSB_ENDPOINT_OUT|1, out,
           outl, &nbytes, USB_TIMEOUT);
        usb_errchk("libusb_bulk_transfer(out)", rc);
        if (nbytes != outl) {
            fprintf(stderr, "libusb_bulk_transfer(out): "
              "requested %d bytes but %d transferred\n",
              outl, nbytes);
            exit(EXIT_FAILURE);
        }
    }

    if (in && inl > 0) {
        rc = libusb_bulk_transfer(usb_handle, LIBUSB_ENDPOINT_IN|1, in,
           inl, &nbytes, USB_TIMEOUT);
        usb_errchk("libusb_bulk_transfer(in)", rc);
    } else {
        nbytes = 0;
    }

    return nbytes;
}

static void
init_t48(int verbose)
{
    char buf[64];
    uint8_t msg[80];
    memset(msg, 0, sizeof msg);

    msg[0] = T48_CMD_QUERY;
    transact(msg, 5, msg, sizeof msg);

    if (msg[6] != T48_DEVTYPE) {
        fprintf(stderr,
          "This software was written for device type 7 (T48)... exiting\n");
        exit(EXIT_FAILURE);
    }

    if (!verbose) {
        return;
    }

    printf("Device Type: %d (T48)\n", T48_DEVTYPE);
    printf("Firmware Version: %d.%02d\n", msg[5], msg[4]);

    strcpy(buf, "Device Code: ");
    strncat(buf, (char *)&msg[24], 8);
    printf("%s\n", buf);

    strcpy(buf, "Device Serial: ");
    strncat(buf, (char *)&msg[32], 24);
    printf("%s\n", buf);

    strcpy(buf, "Date of Manufacture: ");
    strncat(buf, (char *)&msg[8], 16);
    printf("%s\n", buf);

    char *speed;
    switch (msg[60]) {
        case 0:
            speed = "12Mbps";
            break;
        case 1:
            speed = "480Mbps";
            break;
        case 3:
            speed = "5Gbps";
            break;
        default:
            speed = "Unknown";
            break;
    }
    printf("USB Speed: %s\n", speed);

    double voltage = *(uint32_t *)&msg[56];
    printf("USB Supply Voltage: %f\n", voltage * 0xccf7 / 0x27000 / 100);
}

static cab_err_e
set_pins(uint8_t *pins, int npins,
  int voltage, pin_msg_info_t *pin_info, uint8_t msgtype, const char *pintype)
{
    uint8_t msg[48];
    memset(msg, 0, sizeof msg);
    msg[0] = msgtype;
    for (int i = 0; i < npins; i++) {
        if (pins[i] < 1 || pins[i] > T48_MAX_PINS) {
            fprintf(stderr, "set_pins(): Pin out of range (%d)\n", pins[i]);
            return CAB_ERR_OUT_OF_RANGE;
        }
        pin_msg_info_t *p_pin = &pin_info[pins[i]-1];
        if (!p_pin->supported) {
            fprintf(stderr, "set_pins(): Pin %d can't be used for %s\n", pins[i], pintype);
            return CAB_ERR_INVALID_PARAM;
        }

        msg[8+p_pin->msg_offset] |= 1<<p_pin->bit;
    }

    msg[22] = voltage;

    transact(msg, sizeof msg, NULL, 0);

    return CAB_ERR_NONE;
}

static int
set_vpp_voltage(float voltage)
{
    const float vpp_min = 9.0, vpp_max = 25.0;

    // Map voltage to the range [0, 63]
    int v = (voltage - vpp_min) / (vpp_max - vpp_min) * 63 + 0.5;

    if (v < 0 || v > 63) {
        fprintf(stderr, "VPP is out of range (%f)\n", voltage);
        return CAB_ERR_OUT_OF_RANGE;
    }

    uint8_t msg[48];
    memset(msg, 0, sizeof msg);
    msg[0] = T48_SET_VPP_PINS;
    msg[1] = 1;         // Set VPP voltage
    msg[8] = v;
    transact(msg, sizeof msg, NULL, 0);

    return CAB_ERR_NONE;
}

cab_err_e
cab_set_io_voltage(float voltage)
{
    const float vpp_min = 2.35, vpp_max = 3.45;

    // Map voltage to the range [0, 63]
    int v = (voltage - vpp_min) / (vpp_max - vpp_min) * 4 + 0.5;

    if (v < 0 || v > 4) {
        fprintf(stderr, "IO voltage is out of range (%f)\n", voltage);
        return CAB_ERR_OUT_OF_RANGE;
    }

    uint8_t msg[48];
    memset(msg, 0, sizeof msg);
    msg[0] = T48_SET_VPP_PINS;
    msg[1] = 2;         // Set IO voltage
    msg[8] = v;
    transact(msg, sizeof msg, NULL, 0);

    return CAB_ERR_NONE;
}

static int
set_vpp_pins(uint8_t *pins, int npins, float voltage)
{
    static pin_msg_info_t pin_info[] = {
        { 1, 0, 7 },    // 1
        { 1, 0, 6 },    // 2
        { 1, 0, 5 },    // 3
        { 1, 0, 4 },    // 4
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 1, 0, 3 },    // 9
        { 1, 0, 2 },    // 10
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 1, 0, 1 },    // 30
        { 1, 0, 0 },    // 31
        { 1, 1, 0 },    // 32
        { 1, 1, 1 },    // 33
        { 1, 1, 2 },    // 34
        { 0, 0, 0 },
        { 1, 1, 3 },    // 36
        { 1, 1, 4 },    // 37
        { 1, 1, 5 },    // 38
        { 1, 1, 6 },    // 39
        { 1, 1, 7 },    // 40
    };

    cab_err_e err;
    if ((err = set_pins(pins, npins, 0,
      pin_info, T48_SET_VPP_PINS, "VPP")) != CAB_ERR_NONE) {
        return err;
    }

    return set_vpp_voltage(voltage);
}

static int
set_vcc_pins(uint8_t *pins, int npins, float voltage)
{
    static pin_msg_info_t pin_info[] = {
        { 1, 0, 0 },    // 1
        { 1, 0, 1 },    // 2
        { 1, 0, 2 },    // 3
        { 1, 0, 3 },    // 4
        { 1, 0, 4 },    // 5
        { 1, 0, 5 },    // 6
        { 1, 0, 6 },    // 7
        { 1, 0, 7 },    // 8
        { 1, 1, 7 },    // 9
        { 1, 1, 6 },    // 10
        { 1, 1, 5 },    // 11
        { 1, 1, 4 },    // 12
        { 1, 1, 3 },    // 13
        { 1, 1, 2 },    // 14
        { 1, 1, 1 },    // 15
        { 1, 1, 0 },    // 16
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 1, 2, 0 },    // 25
        { 1, 2, 1 },    // 26
        { 1, 2, 2 },    // 27
        { 1, 2, 3 },    // 28
        { 1, 2, 4 },    // 29
        { 1, 2, 5 },    // 30
        { 1, 2, 6 },    // 31
        { 1, 2, 7 },    // 32
        { 1, 3, 0 },    // 33
        { 1, 3, 1 },    // 34
        { 1, 3, 2 },    // 25
        { 1, 3, 3 },    // 36
        { 1, 3, 4 },    // 37
        { 1, 3, 5 },    // 38
        { 1, 3, 6 },    // 39
        { 1, 3, 7 },    // 40
    };

    const float vcc_min = 1.8, vcc_max = 6.5;

    // Map voltage to the range [1, 63] (0 means leave voltage unchanged).
    int v = (voltage - vcc_min) / (vcc_max - vcc_min) * 62 + 0.5;

    if (v < 1 || v > 63) {
        fprintf(stderr, "VCC is out of range (%f)\n", voltage);
        return CAB_ERR_OUT_OF_RANGE;
    }

    return set_pins(pins, npins, v, pin_info, T48_SET_VCC_PINS, "VCC");
}

static int
set_gnd_pins(uint8_t *pins, int npins)
{
    static pin_msg_info_t pin_info[] = {
        { 1, 0, 7 },    // 1
        { 1, 0, 6 },    // 2
        { 1, 0, 5 },    // 3
        { 1, 0, 4 },    // 4
        { 1, 0, 3 },    // 5
        { 1, 0, 2 },    // 6
        { 1, 0, 1 },    // 7
        { 1, 0, 0 },    // 8
        { 1, 1, 7 },    // 9
        { 1, 1, 6 },    // 10
        { 1, 1, 5 },    // 11
        { 1, 1, 4 },    // 12
        { 1, 1, 3 },    // 13
        { 1, 1, 2 },    // 14
        { 1, 1, 1 },    // 15
        { 1, 1, 0 },    // 16
        { 0, 0, 0 },
        { 0, 2, 7 },    // 18
        { 0, 0, 0 },
        { 0, 2, 6 },    // 20
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 0, 0, 0 },
        { 1, 2, 5 },    // 25
        { 0, 0, 0 },
        { 1, 2, 4 },    // 27
        { 0, 0, 0 },
        { 1, 2, 3 },    // 29
        { 1, 2, 2 },    // 30
        { 1, 2, 1 },    // 31
        { 1, 2, 0 },    // 32
        { 1, 3, 7 },    // 33
        { 1, 3, 6 },    // 34
        { 1, 3, 5 },    // 25
        { 1, 3, 4 },    // 36
        { 1, 3, 3 },    // 37
        { 1, 3, 2 },    // 38
        { 1, 3, 1 },    // 39
        { 1, 3, 0 },    // 40
    };

    return set_pins(pins, npins, 0, pin_info, T48_SET_GND_PINS, "GND");
}

cab_err_e
cab_reset(uint8_t *gnd_pins, int ngnd, uint8_t *vcc_pins, int nvcc,
  uint8_t *vpp_pins, int nvpp, float vcc_voltage, float vpp_voltage)
{
    uint8_t msg[10];
    memset(msg, 0, sizeof msg);

    msg[0] = T48_RESET_PINS;
    transact(msg, sizeof msg, NULL, 0);

    cab_err_e err;
    if ((err = set_gnd_pins(gnd_pins, ngnd)) != CAB_ERR_NONE) {
        return err;
    }

    if ((err = set_vcc_pins(vcc_pins, nvcc, vcc_voltage)) != CAB_ERR_NONE) {
        return err;
    }

    if (vpp_pins && nvpp > 0) {
        if ((err = set_vpp_pins(vpp_pins, nvpp, vpp_voltage)) != CAB_ERR_NONE) {
            return err;
        }
    }

    for (int i = 0; i < sizeof io_pin_modes / sizeof io_pin_modes[0]; i++) {
        io_pin_modes[i] = CAB_PMODE_Z;
    }

    device_never_reset = false;

    return CAB_ERR_NONE;
}

cab_err_e
cab_set_vpp_voltage(float voltage)
{
    if (device_never_reset) {
        return CAB_ERR_STATE;
    }

    return set_vpp_voltage(voltage);
}

cab_err_e
cab_io_pullup(bool enabled)
{
    pullup = enabled;

    return CAB_ERR_NONE;
}

static int
config_and_read(bool pullup, uint8_t *pins, uint8_t *values, int npins)
{
    uint8_t msg[32];
    memset(msg, 0, sizeof msg);

    // Message used by official app for test vectors - lets you configure the
    // IO pins and read them back.  Power and Ground pins are untouched by
    // this call.
    msg[0] = T48_CONFIG_AND_READ;
    msg[1] = pullup ? 0x80 : 0;
    msg[2] = T48_MAX_PINS;
    msg[4] = 1;

    for (int i = 0; i < T48_MAX_PINS; i++) {
        msg[8 + (i>>1)] |= (io_pin_modes[i] & 0xf) << ((i&1) ? 4 : 0);
    }

    transact(msg, sizeof msg, msg, sizeof msg);

    if (msg[1]) {
        fprintf(stderr, "Overcurrent protection triggered!\n");
        return CAB_ERR_OVERCURRENT;
    }

    if (npins > 0 && (pins == NULL || values == NULL)) {
        fprintf(stderr, "Bad pointer(s) passed to config_and_read()\n");
        return CAB_ERR_BAD_POINTER;
    }

    for (int i = 0; i < npins; i++) {
        uint8_t pin = pins[i] - 1;
        values[i] = (msg[8+(pin>>1)] >> ((pin&1) ? 4 : 0)) & 0xf;
    }

    hold = false;

    return CAB_ERR_NONE;
}

cab_err_e
cab_io_hold_on()
{
    if (device_never_reset) {
        return CAB_ERR_STATE;
    }

    hold = true;

    return CAB_ERR_NONE;
}

cab_err_e
cab_io_hold_off()
{
    if (device_never_reset) {
        return CAB_ERR_STATE;
    }

    return config_and_read(pullup, NULL, NULL, 0);
}

cab_err_e
cab_io_pin_modes(uint8_t *pins, cab_pin_mode_e *modes, int npins)
{
    if (device_never_reset) {
        return CAB_ERR_STATE;
    }

    for (int i = 0; i < npins; i++) {
        if (pins[i] > T48_MAX_PINS) {
            return CAB_ERR_OUT_OF_RANGE;
        }
    }

    for (int i = 0; i < npins; i++) {
        io_pin_modes[pins[i]-1] = modes[i];
    }

    if (!hold) {
        return config_and_read(pullup, NULL, NULL, 0);
    }

    return CAB_ERR_NONE;
}

cab_err_e
cab_io_pin_mode(uint8_t pin, cab_pin_mode_e mode)
{
    if (device_never_reset) {
        return CAB_ERR_STATE;
    }

    if (pin > T48_MAX_PINS) {
        return CAB_ERR_OUT_OF_RANGE;
    }

    io_pin_modes[pin-1] = mode;

    if (!hold) {
        return config_and_read(pullup, NULL, NULL, 0);
    }

    return CAB_ERR_NONE;
}

cab_err_e
cab_io_read(uint8_t *pins, uint8_t *values, int npins)
{
    if (device_never_reset) {
        return CAB_ERR_STATE;
    }

    return config_and_read(pullup, pins, values, npins);
}

int
main(int argc, char **argv)
{
    int rc;

	rc = libusb_init(NULL);
    usb_errchk("libusb_init()", rc);

    usb_open();

    init_t48(true);

    rc = app_run(argc, argv);

    printf("Application ");
    if (rc == CAB_ERR_NONE) {
        printf("completed successfully\n");
    } else {
        printf("failed: %s\n", cab_sterror(rc));
    }

    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
