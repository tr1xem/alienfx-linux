#include "AlienFX_SDK.h"
#include "libusb.h"
#include <iostream>
#include <loguru.hpp>

namespace AlienFX_SDK {
void Mappings::LoadMappings() {
    // TODO: implement
    LOG_S(INFO) << "Load mappings";
}
bool Mappings::AlienFXEnumDevices(void *acc) {
    LOG_S(INFO) << "Enumerating devices";
    Functions *dev = nullptr;
    deviceListChanged = false;

    // Reset active status
    for (auto i = fxdevs.begin(); i != fxdevs.end(); i++)
        i->present = false;
    activeDevices = activeLights = 0;

    libusb_context *ctx = nullptr;
    int result = libusb_init(&ctx);
    if (result < 0) {
        LOG_S(ERROR) << "Failed to initialize libusb:  "
                     << libusb_error_name(result);
        return deviceListChanged;
    }

    libusb_device **device_list;
    ssize_t device_count = libusb_get_device_list(ctx, &device_list);

    if (device_count < 0) {
        LOG_S(ERROR) << "Failed to get device list: "
                     << libusb_error_name(device_count);
        libusb_exit(ctx);
        return deviceListChanged;
    }

    // Iterate through all USB devices
    for (ssize_t i = 0; i < device_count; i++) {
        libusb_device *device = device_list[i];
        struct libusb_device_descriptor desc;

        // Get device descriptor
        result = libusb_get_device_descriptor(device, &desc);
        if (result < 0) {
            continue; // Skip this device if we can't get descriptor
        }

        // Create new Functions object for each potential device
        dev = new Functions();

        // Try to probe/initialize this device
        if (dev->AlienFXProbeDevice(device, desc.idVendor, desc.idProduct)) {
            LOG_S(INFO) << "Found AlienFX device - VID: 0x" << std::hex
                        << desc.idVendor << ", PID: 0x" << desc.idProduct;
            AlienFxUpdateDevice(dev);
        } else {
            // Device probe failed, clean up
            delete dev;
            dev = nullptr;
        }
    }

    // Free the device list
    libusb_free_device_list(device_list, 1);

    // Check removed devices
    for (auto i = fxdevs.begin(); i != fxdevs.end(); i++) {
        if (!i->present && i->dev) {
            deviceListChanged = true;
            LOG_S(INFO) << "Device removed - VID: 0x" << std::hex << i->vid
                        << ", PID:  0x" << i->pid;
            i->arrived = false;
            break;
        }
    }

    libusb_exit(ctx);
    return deviceListChanged;
}

void Mappings::AlienFxUpdateDevice(Functions *dev) {
    // TODO: implement
    LOG_S(INFO) << "Update device";
}

bool Functions::AlienFXProbeDevice(libusb_device *device, unsigned short vid,
                                   unsigned short pid) {
    int length{};
    version = API_UNKNOWN;
    libusb_config_descriptor *config = nullptr;
    int result = libusb_get_config_descriptor(device, 0, &config);
    if (result != 0) {
        return false;
    }
    for (int ifc = 0; ifc < config->bNumInterfaces; ifc++) {
        const libusb_interface &iface = config->interface[ifc];

        for (int alt = 0; alt < iface.num_altsetting; alt++) {
            const libusb_interface_descriptor &altset = iface.altsetting[alt];

            // HID interface
            if (altset.bInterfaceClass != LIBUSB_CLASS_HID)
                continue;

            for (int ep = 0; ep < altset.bNumEndpoints; ep++) {
                const libusb_endpoint_descriptor &e = altset.endpoint[ep];

                // Interrupt endpoint
                if ((e.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) ==
                    LIBUSB_TRANSFER_TYPE_INTERRUPT) {

                    // NOTE: Convert it to decimal
                    length = (e.wMaxPacketSize & 0x07FF) + 1;

                    if ((e.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                        LIBUSB_ENDPOINT_OUT) {
                        break;
                    }
                }
            }
        }
    }
    switch (vid) {
    case 0x0d62: // Darfon
        // if (caps.Usage == 0xcc && !length) {
        //     length = caps.FeatureReportByteLength;
        //     version = API_V5;
        // }
        break;
    case 0x187c: // Alienware
        switch (length) {
        case 9:
            version = API_V2;
            break;
        case 12:
            version = API_V3;
            break;
        case 34:
            version = API_V4;
            break;
        case 65:
            version = API_V6;
            break;
            // case 193:
            //	version = API_V9;
        }
        break;
    default:
        if (length == 65)
            switch (vid) {
            case 0x0424: // Microchip
                if (pid != 0x274c)
                    version = API_V6;
                break;
            case 0x0461: // Primax
                version = API_V7;
                break;
            case 0x04f2: // Chicony
                version = API_V8;
                break;
            }
    }

#ifdef DEBUG
    LOG_S(INFO) << "Probing device VID: 0x" << std::hex << vid << ", PID: 0x"
                << pid << ", Version: " << version << ", Length: " << length;
#endif
    return version != API_UNKNOWN;
}

} // namespace AlienFX_SDK
