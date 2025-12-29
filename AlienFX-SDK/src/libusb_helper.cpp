#include <cstdint>
#include <hidapi.h>
#include <libusb.h>
#include <loguru.hpp>

int GetMaxPacketSize(libusb_context *ctxx, unsigned short vidd,
                     unsigned short pidd) {

    int maxPacketSize = -1;
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(ctxx, &devs);
    if (cnt < 0) {
        LOG_S(ERROR) << "Failed to get device list";
        libusb_exit(ctxx);
        return 1;
    }
    libusb_device *libusbdevice = nullptr;
    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == vidd && desc.idProduct == pidd) {
                libusbdevice = devs[i];
                break;
            }
        }
    }
    int result{};
    libusb_config_descriptor *config = nullptr;
    libusb_device_handle *handle = nullptr;
    if (!libusbdevice) {
        libusb_free_device_list(devs, 1);
        return false;
    }
    if (libusbdevice) {
        result = libusb_get_config_descriptor(libusbdevice, 0, &config);
        if (result != 0) {
            return false;
        }
        for (int ifc = 0; ifc < config->bNumInterfaces; ifc++) {
            const libusb_interface &iface = config->interface[ifc];
            for (int alt = 0; alt < iface.num_altsetting; alt++) {
                const libusb_interface_descriptor &altset =
                    iface.altsetting[alt];

                // printf("\nInterface %d, Alt %d:\n", ifc, alt);
                // printf("  bInterfaceNumber    %u\n",
                // altset.bInterfaceNumber); printf("  baltseternateSetting
                // %u\n", altset.bAlternateSetting); printf("  bNumEndpoints
                // %u\n", altset.bNumEndpoints); printf("  bInterfaceClass
                // 0x%02x\n", altset.bInterfaceClass); printf("
                // bInterfaceSubClass  0x%02x\n", altset.bInterfaceSubClass);
                // printf("  bInterfaceProtocol 0x%02x\n",
                //        altset.bInterfaceProtocol);
                // printf("  iInterface  %u\n", altset.iInterface);

                if (altset.bInterfaceClass != LIBUSB_CLASS_HID)
                    return false;

                for (int ep = 0; ep < altset.bNumEndpoints; ep++) {
                    const libusb_endpoint_descriptor &e = altset.endpoint[ep];

                    // printf("    Endpoint %d:\n", ep);
                    // printf(
                    //     "      bEndpointAddress 0x%02x (%s)\n",
                    //     e.bEndpointAddress, (e.bEndpointAddress &
                    //     LIBUSB_ENDPOINT_IN) ? "IN" : "OUT");
                    // printf("      bmAttributes     0x%02x\n",
                    // e.bmAttributes); printf("      wMaxPacketSize   %u\n",
                    //        e.wMaxPacketSize & 0x07FF);
                    // printf("      bInterval        %u\n", e.bInterval);
                    // if ((e.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
                    //     LIBUSB_TRANSFER_TYPE_INTERRUPT)
                    //     continue;

                    // Store IN/OUT endpoint
                    // if ((e.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                    //     LIBUSB_ENDPOINT_OUT) {
                    //     // out_ep = e.bEndpointAddress;
                    // } else {
                    //     // in_ep = e.bEndpointAddress;
                    // }

                    // NOTE: Check the max packet size for IN endpoint
                    if ((e.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                        LIBUSB_ENDPOINT_IN) {
                        // FIXME: size+1 for report ID not needed in linux?
                        maxPacketSize = (e.wMaxPacketSize & 0x07FF);
                        /*
                        LOG_S(INFO) << "VID: 0x" << std::hex << vidd << " PID:
                        0x"
                                    << std::hex << (int)pidd
                                    << " IN EP: " << std::hex << (int)in_ep
                                    << ", Length: " << std::dec << (int)length;
                        */
                    }
                }
            }
        }
        libusb_free_config_descriptor(config);
        libusb_free_device_list(devs, 1);
    } else {
        LOG_S(ERROR) << "Device not found";
        return -1;
    }

    return maxPacketSize;
}

bool HidD_SetOutputReport(hid_device *dev, uint8_t *buffer, size_t length) {
    return hid_send_output_report(dev, buffer, length);
}

bool HidD_SetFeature(hid_device *dev, uint8_t *buffer, size_t length) {
    return hid_send_feature_report(dev, buffer, length);
}

bool WriteFile(hid_device *dev, uint8_t *buffer, size_t length) {
    return hid_write(dev, buffer, length);
}

bool ReadFile(hid_device *dev, uint8_t *buffer, size_t length) {
    return hid_read(dev, buffer, length);
}
int HidD_GetFeature(hid_device *dev, uint8_t *buffer, size_t length) {
    return hid_get_feature_report(dev, buffer, length);
}

int HidD_GetInputReport(hid_device *dev, uint8_t *buffer, size_t length) {
    return hid_get_input_report(dev, buffer, length);
}
