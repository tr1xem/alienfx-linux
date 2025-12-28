#include <cstdint>
#include <iomanip>
#include <iostream>
#include <libusb.h>
#include <loguru.hpp>

#define HID_SET_REPORT 0x09
#define HID_GET_REPORT 0x01
#define HID_REPORT_TYPE_FEATURE 0x03
#define HID_REPORT_TYPE_OUTPUT 0x02

using std::setfill;
using std::setw;

void LOG_BUFFER(uint8_t *buffer, size_t length) {
    LOG_S(INFO) << "Sending USB packet (" << length << " std::uint8_ts): ";
    for (int i = 0; i < length; i++) {
        std::cout << std::hex << setw(2) << setfill('0')
                  << static_cast<unsigned>(buffer[i]) << ' ';
    }
    std::cout << std::dec << std::endl;
}

bool HidD_SetFeature(libusb_device_handle *dev, uint8_t *buffer, size_t length,
                     uint8_t iface = 0) {
    if (!dev || !buffer || length == 0)
        return false;
    uint8_t report_id = buffer[0];
    int skipped_report_id = 0;
    if (report_id == 0x0) {
        buffer++;
        length--;
        skipped_report_id = 1;
    }
#ifdef DEBUG
    LOG_BUFFER(buffer, length);
#endif
    int rc = libusb_control_transfer(
        dev,
        LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
            LIBUSB_ENDPOINT_OUT,
        HID_SET_REPORT, (HID_REPORT_TYPE_FEATURE << 8) | report_id, iface,
        buffer, length, 1000);
    if (rc < 0)
        return false;

    if (skipped_report_id)
        length++;

    return length;
    // return rc == (int)length;
}

int HidD_SetOutputReport(libusb_device_handle *dev, uint8_t *buffer,
                         size_t length, uint8_t iface = 0) {
    if (!dev || !buffer || length == 0)
        return false;
    uint8_t report_id = buffer[0];
    int skipped_report_id = 0;
    if (report_id == 0x0) {
        buffer++;
        length--;
        skipped_report_id = 1;
    }

#ifdef DEBUG
    LOG_BUFFER(buffer, length);
#endif

    int rc = libusb_control_transfer(
        dev,
        LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
            LIBUSB_ENDPOINT_OUT,
        HID_SET_REPORT, (HID_REPORT_TYPE_OUTPUT << 8) | report_id, iface,
        buffer, length, 1000);

    if (rc < 0)
        return false;

    if (skipped_report_id)
        length++;

    return length;
    // return rc == (int)length;
}

bool WriteFile(libusb_device_handle *dev, uint8_t *buffer, size_t length,
               int &transferred, uint8_t out_ep) {
    if (!dev || !buffer)
        return false;
    int report_number;
    int skipped_report_id = 0;

    if (out_ep <= 0) {
        /* No interrupt out endpoint. Use the Control Endpoint */
        return HidD_SetOutputReport(dev, buffer, length);
    }
    if (!buffer || (length == 0)) {
        return -1;
    }
    report_number = buffer[0];

    if (report_number == 0x0) {
        buffer++;
        length--;
        skipped_report_id = 1;
    }
#ifdef DEBUG
    LOG_BUFFER(buffer, length);
#endif

    int rc = libusb_interrupt_transfer(dev, out_ep, buffer, length,
                                       &transferred, 1000);
    if (rc < 0)
        return -1;
    if (skipped_report_id)
        transferred++;

    return transferred;
    // return rc == 0;
}

bool ReadFile(libusb_device_handle *dev, uint8_t *buffer, size_t length,
              int &transferred, uint8_t in_ep) {
    if (!dev || !buffer)
        return false;
#ifdef DEBUG
    LOG_BUFFER(buffer, length);
#endif
    int rc = libusb_interrupt_transfer(dev, in_ep, buffer, length, &transferred,
                                       1000);
    return rc == 0 && transferred > 0;
}

int HidD_GetFeature(libusb_device_handle *dev, uint8_t *buffer, size_t length,
                    uint8_t iface = 0) {
    if (!dev || !buffer || length == 0)
        return -1;
    uint8_t report_id = buffer[0];
    int skipped_report_id = 0;

    if (report_id == 0x0) {
        /* Offset the return buffer by 1, so that the report ID
           will remain in byte 0. */
        buffer++;
        length--;
        skipped_report_id = 1;
    }

#ifdef DEBUG
    LOG_BUFFER(buffer, length);
#endif

    int rc = libusb_control_transfer(
        dev,
        LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
            LIBUSB_ENDPOINT_IN,
        HID_GET_REPORT, (HID_REPORT_TYPE_FEATURE << 8) | report_id, iface,
        buffer, length, 1000);
    if (rc < 0) {
        LOG_S(ERROR) << "Error in getting feature report: " << rc;
        return rc;
    }
    if (rc < 0)
        return -1;
    if (skipped_report_id)
        rc++;
    return rc; // number of bytes read
}

int HidD_GetInputReport(libusb_device_handle *dev, uint8_t *buffer,
                        size_t length, uint8_t in_ep, uint8_t iface = 0) {
    if (!dev || !buffer || length == 0)
        return -1;
    int transferred = 0;
    int skipped_report_id = 0;
    int report_number = buffer[0];
    if (report_number == 0x0) {
        /* Offset the return buffer by 1, so that the report ID
           will remain in byte 0. */
        buffer++;
        length--;
        skipped_report_id = 1;
    }
#ifdef DEBUG
    LOG_BUFFER(buffer, length);
#endif
    int rc = libusb_control_transfer(
        dev,
        LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE |
            LIBUSB_ENDPOINT_IN,
        HID_GET_REPORT, (1 /*HID Input*/ << 8) | report_number, iface,
        (unsigned char *)buffer, length, 1000 /*timeout millis*/);
    if (rc < 0)
        return -1;
    if (skipped_report_id)
        rc++;
    return rc;
    // return transferred; // number of bytes read
}
