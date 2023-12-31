#!/usr/bin/env python3

import usb.core
import usb.util

USB_VID = 0x2e8a
dev = usb.core.find(idVendor=0x2e8a, idProduct=0xc011)

print("Openning HID device with VID = 0x%X" % USB_VID)

def hid_set_report(dev, report):
    dev.ctrl_transfer(
        0x21,  # REQUEST_TYPE_CLASS | RECIPIENT_INTERFACE | ENDPOINT_OUT
        9,     # SET_REPORT
        0x200, # "Vendor" Descriptor Type + 0 Descriptor Index
        0,     # USB interface № 0
        report # the HID payload as a byte array -- e.g. from struct.pack()
    )

if dev is None:
    raise ValueError('Device not found')

# dev.set_configuration()

data=[0x00, 0x04, 0x04, 0xFF, 0xFF, 0xFF, 0x00, 0x00]
result=dev.ctrl_transfer(0x21, 0x9, wValue=0x200, wIndex=0x00, data_or_wLength=data)
