## CABBiC: Custom Abitrary Bit-Banging in C, using the X-Gecu Pro T48.

The X-Gecu T48 is primarily designed as a Universal Programmer, for performing
tasks like programming EPROMs and Flash devices.  While the list of supported
devices is impressive, the device is capable of much, much more than what is
possible with the supplied software.

It is possible to operate the T48 as a generic "bit banger", which supports
almost arbitrary signalling on the pins of a connected device.  There is huge
flexibility in whether a given pin is used to supply a voltage, is used as a
ground connection, or is used for generic input and/or output signalling.

If you have difficulty finding an off-the-shelf device (plus accompanying
software) to carry out a given task which involves interacting with a chip or
circuit, there's an execellent chance that the T48 is capable of performing
that task, at least from an electical standpoint, as long as the device can
be physically connected to the T48.  Depending on the characteristics of
the target device, the device may need to be connected to the T48 via an
adapter board or "shield".

CABBiC is a simple application framework which communicates with the T48 via
USB, and provides a simple, generic API to allow C programmers to write custom
code to generate and read signals on the pins of a connected device.  It does
not provide any logic to perform any particular task or operate with any
particular connected device (with the exception of some included example code).
You must provide that functionality yourself.  Any C code that you develop
can be built and run locally on the Linux PC to which the T48 is connected.

NOTE: Since each change in the state of the T48 pins involves a USB
transaction, this software is not suitable for applications which require
high-frequency signalling.

**This software is provided as-is: use at your own risk.**

**If you cannot write C code (and are unwilling to try), or if you do not own a
multimeter, then this software is not for you.  Check all voltage levels
carefully before connecting your target device to the T48.**

### Creating a custom app

1. Create a new .c file in the apps folder (you can use one of the
existing apps as a starting point).
2. Implement the application logic you require, making calls to the functions
described in api.h, to interact with the T48.
3. Say you want to name your app 'probeit', in which case the app source must
reside in apps/probeit.c.  Type 'make APP=probeit' to build the the 'probeit'
executable.

The app will communicate with the T48 via libusb.  However, by default,
regular users won't have permission to access the device.  Either run the
app as root (e.g. via sudo), or (much more preferably) run udevperm.bash (
which was tested on Ubuntu 22.04) to grant permission to regular users to
access the device.  You can edit the .rules file in the udev folder beforehand,
for example if you only want to grant access to the device to a specific user.

What kind of applications could potentially be created with CABBiC?

- Readers and programmers for programmable devices such as EPROMs, PALs and
  Flash memories.
- Circuit or IC testing
- Control / automation
- Breadboarding activities
