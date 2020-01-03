#!/bin/bash

# Create gadget
mkdir /sys/kernel/config/usb_gadget/mykeyboard
cd /sys/kernel/config/usb_gadget/mykeyboard

# Add basic information
echo 0x0100 > bcdDevice # Version 1.0.0
echo 0x0200 > bcdUSB # USB 2.0
echo 0x00 > bDeviceClass
echo 0x00 > bDeviceProtocol
echo 0x00 > bDeviceSubClass
echo 0x08 > bMaxPacketSize0
echo 0x0104 > idProduct # Multifunction Composite Gadget
echo 0x1d6b > idVendor # Linux Foundation

# Create English locale
mkdir strings/0x409

echo "My manufacturer" > strings/0x409/manufacturer
echo "My virtual keyboard" > strings/0x409/product
echo "0123456789" > strings/0x409/serialnumber

# Create HID function
mkdir functions/hid.usb0

echo 1 > functions/hid.usb0/protocol
echo 8 > functions/hid.usb0/report_length # 8-byte reports
echo 1 > functions/hid.usb0/subclass
echo "05010906a101050719e029e71500250175019508810275089501810175019503050819012903910275019505910175089506150026ff00050719002aff008100c0" | xxd -r -ps > functions/hid.usb0/report_desc

# Create configuration
mkdir configs/c.1
mkdir configs/c.1/strings/0x409

echo 0x80 > configs/c.1/bmAttributes
echo 200 > configs/c.1/MaxPower # 200 mA
echo "Example configuration" > configs/c.1/strings/0x409/configuration

# Link HID function to configuration
ln -s functions/hid.usb0 configs/c.1

# Enable gadget
ls /sys/class/udc > UDC

