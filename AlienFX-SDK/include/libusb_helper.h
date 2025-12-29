#pragma once
#include <cstddef>
#include <cstdint>
#include <hidapi.h>
#include <hidapi_libusb.h>
#include <libusb.h>

//[Linux Compatibility] Gets the maximum packet size for IN endpoint for the
// device
int GetMaxPacketSize(libusb_context *ctxx, unsigned short vidd,
                     unsigned short pidd);

// Override for hid_send_output_report
bool HidD_SetFeature(hid_device *dev, uint8_t *buffer, size_t length);

// Override for hid_send_output_report
int HidD_SetOutputReport(hid_device *dev, uint8_t *buffer, size_t length);

// Override for hid_write
bool WriteFile(hid_device *dev, uint8_t *buffer, size_t length);

// Override for hid_read
bool ReadFile(hid_device *dev, uint8_t *buffer, size_t length);

// Override for hid_get_feature_report
int HidD_GetFeature(hid_device *dev, uint8_t *buffer, size_t length);

// Override for hid_get_input_report
int HidD_GetInputReport(hid_device *dev, uint8_t *buffer, size_t length);
