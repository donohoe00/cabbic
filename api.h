#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CAB_PMODE_0 = 0,    // Use to drive a pin's output low.
    CAB_PMODE_1 = 1,    // Use to drive a pin's output high.
    CAB_PMODE_L = 2,
    CAB_PMODE_H = 3,
    CAB_PMODE_C = 4,
    CAB_PMODE_Z = 5,    // Use to configure a pin as input.
    CAB_PMODE_X = 6,
    CAB_PMODE_G = 7,
    CAB_PMODE_V = 8,
} cab_pin_mode_e;

typedef enum {
    CAB_ERR_NONE            = 0,
    CAB_ERR_STATE           = 1,
    CAB_ERR_OUT_OF_RANGE    = 2,
    CAB_ERR_INVALID_PARAM   = 3,
    CAB_ERR_BAD_POINTER     = 4,
    CAB_ERR_OVERCURRENT     = 5,
    CAB_ERR_BAD_ARGS        = 6,
    CAB_ERR_FILE            = 7,
} cab_err_e;

// Main application entry point
cab_err_e app_run(int argc, char **argv);

// The remaining functions may be called by the application
cab_err_e cab_reset(uint8_t *gnd_pins, int ngnd, uint8_t *vcc_pins, int nvcc,
  uint8_t *vpp_pins, int nvpp, float vcc_voltage, float vpp_voltage);

// cab_set_*_voltage() can be called at any time after cab_reset()
cab_err_e cab_set_vpp_voltage(float voltage);
cab_err_e cab_set_io_voltage(float voltage);

// Causes any mode changes made to pins via cab_io_pin_*() to be deferred until
// cab_io_hold_off() or cab_io_read() is called.  Without turning on hold,
// calls to cab_io_pin_*() take effect immediately.  Holds can be used to
// reduce the number of USB transactions.  It's possible update pins and
// read them in a single transaction, by placing a hold, making one or
// more calls to cab_io_pin_*(), and the calling cab_io_read() (which
// implicitly calls cab_hold_off()).
cab_err_e cab_io_hold_on();

cab_err_e cab_io_pullup(bool enabled);

// Change the modes of the specified pins.
// Pins which were previously configured as VCC, VPP or GND will be unaffected.
//
// pins:     Array of length 'npins', specifying the pins (which are numbered
//           in the range [0, 40]) to be set.
// settings: Array of length 'npins', specifying the setting for each pin
//           to be set.
cab_err_e cab_io_pin_modes(uint8_t *pins, cab_pin_mode_e *mode, int npins);

// Change the mode of the specified pin.
// Pins which were previously configured as VCC, VPP or GND will be unaffected.
cab_err_e cab_io_pin_mode(uint8_t pin, cab_pin_mode_e mode);

// Commit any outstanding pin changes that were applied with cab_io_pin_*(),
// since the last call to cab_io_hold_on().
cab_err_e cab_io_hold_off();

// Commit any outstanding pin mode changes, read all pins, and return a set of
// readout results.
//
// pins:     Array of length 'npins', specifying the pins to be read.
// values:   Array of length 'npins', specifying the values which were read
//           from the corresponding pins (values will be either 0 or 1).
cab_err_e cab_io_read(uint8_t *pins, uint8_t *values, int npins);
