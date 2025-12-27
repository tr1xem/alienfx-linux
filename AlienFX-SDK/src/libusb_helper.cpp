#include <cstdint>
#include <iostream>
#include <libusb.h>

#define HID_SET_REPORT 0x09
#define HID_GET_REPORT 0x01
#define HID_REPORT_TYPE_FEATURE 0x03
#define HID_REPORT_TYPE_OUTPUT 0x02

bool HidD_SetFeature(libusb_device_handle *dev, uint8_t *buffer, size_t length,
                     uint8_t iface = 0) {
    if (!dev || !buffer || length == 0)
        return false;
    uint8_t report_id = buffer[0];
    int rc = libusb_control_transfer(
        dev,
        LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
            LIBUSB_ENDPOINT_OUT,
        HID_SET_REPORT, (HID_REPORT_TYPE_FEATURE << 8) | report_id, iface,
        buffer, length, 1000);
    return rc == (int)length;
}

bool HidD_SetOutputReport(libusb_device_handle *dev, uint8_t *buffer,
                          size_t length, uint8_t iface = 0) {
    if (!dev || !buffer || length == 0)
        return false;
    uint8_t report_id = buffer[0];
    int rc = libusb_control_transfer(
        dev,
        LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
            LIBUSB_ENDPOINT_OUT,
        HID_SET_REPORT, (HID_REPORT_TYPE_OUTPUT << 8) | report_id, iface,
        buffer, length, 1000);
    return rc == (int)length;
}

bool WriteFile(libusb_device_handle *dev, uint8_t *buffer, size_t length,
               int &transferred, uint8_t out_ep) {
    if (!dev || !buffer)
        return false;
    int rc = libusb_interrupt_transfer(dev, out_ep, buffer, length,
                                       &transferred, 1000);
    return rc == 0;
}

bool ReadFile(libusb_device_handle *dev, uint8_t *buffer, size_t length,
              int &transferred, uint8_t in_ep) {
    if (!dev || !buffer)
        return false;
    int rc = libusb_interrupt_transfer(dev, in_ep, buffer, length, &transferred,
                                       1000);
    return rc == 0 && transferred > 0;
}

int HidD_GetFeature(libusb_device_handle *dev, uint8_t *buffer, size_t length,
                    uint8_t iface = 0) {
    if (!dev || !buffer || length == 0)
        return -1;
    uint8_t report_id = buffer[0];
    int rc = libusb_control_transfer(
        dev,
        LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
            LIBUSB_ENDPOINT_IN,
        HID_GET_REPORT, (HID_REPORT_TYPE_FEATURE << 8) | report_id, iface,
        buffer, length, 1000);
    if (rc < 0) {
        std::cerr << "GetFeature failed: " << rc << std::endl;
        return rc;
    }
    return rc; // number of bytes read
}

int HidD_GetInputReport(libusb_device_handle *dev, uint8_t *buffer,
                        size_t length, uint8_t in_ep) {
    if (!dev || !buffer || length == 0)
        return -1;
    int transferred = 0;
    int rc = libusb_interrupt_transfer(dev, in_ep, buffer, length, &transferred,
                                       1000);
    if (rc != 0)
        return rc;
    return transferred; // number of bytes read
}
