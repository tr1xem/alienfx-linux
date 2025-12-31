#include <cstdint>
#include <hidapi.h>
#include <libusb.h>
#include <loguru.hpp>

int GetMaxPacketSize(libusb_context *ctx, unsigned short vid,
                     unsigned short pid) {
    int maxPacketSize = -1;
    libusb_device **devs = nullptr;

    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        LOG_S(ERROR) << "Failed to get device list";
        return -1;
    }

    libusb_device *device = nullptr;
    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0 &&
            desc.idVendor == vid && desc.idProduct == pid) {
            device = devs[i];
            break;
        }
    }

    if (!device) {
        libusb_free_device_list(devs, 1);
        return -1;
    }

    libusb_config_descriptor *config = nullptr;
    int result = libusb_get_config_descriptor(device, 0, &config);
    if (result != 0) {
        libusb_free_device_list(devs, 1);
        return -1;
    }

    for (int ifc = 0; ifc < config->bNumInterfaces; ifc++) {
        const libusb_interface &iface = config->interface[ifc];

        for (int alt = 0; alt < iface.num_altsetting; alt++) {
            const libusb_interface_descriptor &altset = iface.altsetting[alt];

            if (altset.bInterfaceClass != LIBUSB_CLASS_HID)
                continue;

            for (int ep = 0; ep < altset.bNumEndpoints; ep++) {
                const libusb_endpoint_descriptor &e = altset.endpoint[ep];

                if ((e.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                        LIBUSB_ENDPOINT_IN &&
                    (e.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) ==
                        LIBUSB_TRANSFER_TYPE_INTERRUPT) {

                    maxPacketSize = e.wMaxPacketSize;
                }
            }
        }
    }

    libusb_free_config_descriptor(config);
    libusb_free_device_list(devs, 1);

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
