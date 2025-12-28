#pragma once
#include <cstddef>
#include <cstdint>
#include <libusb.h>

bool HidD_SetFeature(libusb_device_handle *dev, uint8_t *buffer, size_t length,
                     uint8_t iface = 0);
int HidD_SetOutputReport(libusb_device_handle *dev, uint8_t *buffer,
                         size_t length, uint8_t iface = 0);

bool WriteFile(libusb_device_handle *dev, uint8_t *buffer, size_t length,
               int &transferred, uint8_t out_ep);
bool ReadFile(libusb_device_handle *dev, uint8_t *buffer, size_t length,
              int &transferred, uint8_t in_ep);

int HidD_GetFeature(libusb_device_handle *dev, uint8_t *buffer, size_t length,
                    uint8_t iface = 0);
int HidD_GetInputReport(libusb_device_handle *dev, uint8_t *buffer,
                        size_t length, uint8_t in_ep, uint8_t iface = 0);
