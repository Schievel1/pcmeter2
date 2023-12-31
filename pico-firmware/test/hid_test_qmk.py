#!/usr/bin/env python3

import sys
import hid

vendor_id     = 0x2e8a
product_id    = 0xc011

usage_page    = 0xFF60
usage         = 0x61
report_length = 64

def get_raw_hid_interface():
    device_interfaces = hid.enumerate(vendor_id, product_id)
    raw_hid_interfaces = [i for i in device_interfaces]

    if len(raw_hid_interfaces) == 0:
        return None

    interface = hid.Device(path=raw_hid_interfaces[0]['path'])

    print(f"Manufacturer: {interface.manufacturer}")
    print(f"Product: {interface.product}")

    return interface

def send_raw_report(data):
    interface = get_raw_hid_interface()

    if interface is None:
        print("No device found")
        sys.exit(1)

    request_data = [0x01] * (report_length + 1) # First byte is Report ID
    request_data[1:len(data) + 1] = data
    request_report = bytes(request_data)

    print("Request:")
    print(request_report)

    try:
        interface.write(request_report)

        response_report = interface.read(report_length, timeout=1000)

        print("Response:")
        print(response_report)
    finally:
        interface.close()

if __name__ == '__main__':
    send_raw_report([
        0x01, 0x41, 0x4C, 0x4C, 0x4f, 0x20, 0x4a, 0x4f, 0x48, 0x41, 0x4e, 0x4e, 0x45, 0x53
    ])
