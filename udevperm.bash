#!/bin/bash

# Give regular users permission to access the T48 via libusb
sudo cp udev/60-xgecu_t48-uaccess.rules /etc/udev/rules.d && sudo udevadm trigger
