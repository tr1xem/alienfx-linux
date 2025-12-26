#include "AlienFX_SDK.h"
#include "alienfx_control.h"
#include "libusb.h"
#include <cstdint>
#include <cstring>
#include <iomanip>
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
#ifdef DEBUG
            LOG_S(INFO) << "Found AlienFX device - VID: 0x" << std::hex
                        << desc.idVendor << ", PID: 0x" << desc.idProduct;
#endif
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
    // TODO: Devinfo persistant vector
    LOG_S(INFO) << "Update device " << dev->pid << ":" << dev->vid;
    // auto devInfo = GetDeviceById(dev->pid, dev->vid);
    // if (devInfo) {
    //     devInfo->version = dev->version;
    //     devInfo->present = true;
    //     activeLights += (unsigned)devInfo->lights.size();
    //     if (devInfo->dev) {
    //         delete dev;
    //         DebugPrint("Scan: VID: " + to_string(devInfo->vid) +
    //                    ", PID: " + to_string(devInfo->pid) + ", Version: " +
    //                    to_string(devInfo->version) + " - present already\n");
    //         devInfo->arrived = false;
    //     } else {
    //         devInfo->dev = dev;
    //         deviceListChanged = devInfo->arrived = true;
    //         DebugPrint("Scan: VID: " + to_string(devInfo->vid) +
    //                    ", PID: " + to_string(devInfo->pid) + ", Version: " +
    //                    to_string(devInfo->version) + " - return back\n");
    //     }
    //     activeDevices++;
    // } else {
    fxdevs.push_back({dev->pid, dev->vid, dev, dev->description, dev->version});
    deviceListChanged = fxdevs.back().arrived = fxdevs.back().present = true;
    activeDevices++;
#ifdef DEBUG
    LOG_S(INFO) << "Scan: VID: " << to_string(dev->vid)
                << ", PID: " << to_string(dev->pid)
                << ", Version: " << to_string(dev->version)
                << " - new device added\n";
#endif
    //}
}

bool Functions::AlienFXProbeDevice(libusb_device *device, unsigned short vidd,
                                   unsigned short pidd) {
    version = API_UNKNOWN;
    int to_check{};
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
                    length = (e.wMaxPacketSize & 0x07FF);
                    to_check = (e.wMaxPacketSize & 0x07FF) + 1;

                    if ((e.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                        LIBUSB_ENDPOINT_OUT) {
                        break;
                    }
                }
            }
        }
    }
    switch (vidd) {
    case 0x0d62: // Darfon
        // if (caps.Usage == 0xcc && !length) {
        //     length = caps.FeatureReportstd::uint8_tLength;
        //     version = API_V5;
        // }
        break;
    case 0x187c: // Alienware
        switch (to_check) {
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
        if (to_check == 65)
            switch (vidd) {
            case 0x0424: // Microchip
                if (pidd != 0x274c)
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
    if (version == API_UNKNOWN) {
        return false;
    } else {
        pid = pidd;
        vid = vidd;
        devHandle = device;
        libusb_device_descriptor desc{};
        libusb_get_device_descriptor(device, &desc);
        libusb_device_handle *handle = nullptr;
        unsigned char buf[256];
        if (libusb_open(device, &handle) != 0) {
            // libusb_free_config_descriptor(config);
            return false;
        }
        if (desc.iManufacturer &&
            libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, buf,
                                               sizeof(buf)) > 0) {
            description.append(reinterpret_cast<char *>(buf));
        }
        description.append(" ");
        if (desc.iProduct && libusb_get_string_descriptor_ascii(
                                 handle, desc.iProduct, buf, sizeof(buf)) > 0) {
            description.append(reinterpret_cast<char *>(buf));
        }
    }
    std::cout << description << std::endl;

    return version != API_UNKNOWN;
}

bool Functions::SetColor(unsigned char index, Afx_action c) {
    Afx_lightblock act{index, {c}};
    return SetAction(&act);
}

bool Functions::SetAction(Afx_lightblock *act) {
    if (act->act.empty())
        return false;
    if (!inSet)
        Reset();

    vector<Afx_icommand> mods;
    switch (version) {
    case API_V8:
        std::cout << "V8 not implemented yet" << std::endl;
        return false;
        //
        // AddV8DataBlock(5, &mods, act);
        // PrepareAndSend(COMMV8_readyToColor);
        // return PrepareAndSend(COMMV8_readyToColor, &mods);
    case API_V7: {
        std::cout << "V7 not implemented yet" << std::endl;
        return false;
        // mods = {{5, {v7OpCodes[act->act.front().type], bright, act->index}}};
        // for (int ca = 0; ca < act->act.size(); ca++) {
        //     if (ca * 3 + 10 < length)
        //         mods.push_back({ca * 3 + 8,
        //                         {act->act.at(ca).r, act->act.at(ca).g,
        //                          act->act.at(ca).b}});
        // }
        // return PrepareAndSend(COMMV7_control, &mods);
    }
    case API_V6:
        std::cout << "V6 not implemented yet" << std::endl;
        return false;
        // return PrepareAndSend(COMMV6_colorSet, SetMaskAndColor(&mods, act));
    // case API_V9:
    //	return PrepareAndSend(COMMV9_colorSet, SetMaskAndColor(&mods, act));
    case API_V5:
        std::cout << "V5 not implemented yet" << std::endl;
        return false;
        // AddV5DataBlock(4, &mods, act->index, &act->act.front());
        // AddV5DataBlock(4, &mods, act->index, &act->act.front());
        // PrepareAndSend(COMMV5_colorSet, &mods);
        // return PrepareAndSend(COMMV5_loop);
    case API_V4:
        // check types and call
        switch (act->act.front().type) {
        // case AlienFX_A_Color: // it's a color, so set as color
        //     return PrepareAndSend(
        //         COMMV4_setOneColor,
        //         {{3,
        //           {act->act.front().r, act->act.front().g,
        //           act->act.front().b,
        //            0, 1, (std::uint8_t)act->index}}});
        case AlienFX_A_Power: { // Set power
            std::cout << "A_Power not implemented yet" << std::endl;
            return false;
            // vector<Afx_lightblock> t = {*act};
            // return SetPowerAction(&t);
        } break;
        default: // Set action
            return SetV4Action(act);
            // std::cout << "V4_Action not implemented yet" << std::endl;
            // return false;
        }
    case API_V3:
    case API_V2: {
        std::cout << "V2 not implemented yet" << std::endl;
        return false;
        // bool res = false;
        // // check types and call
        // switch (act->act.front().type) {
        // case AlienFX_A_Power: { // SetPowerAction for power!
        //     if (act->act.size() > 1) {
        //         vector<Afx_lightblock> t = {{*act}};
        //         return SetPowerAction(&t);
        //     }
        //     break;
        // }
        // case AlienFX_A_Color:
        //     break;
        // default:
        //     PrepareAndSend(
        //         COMMV1_setTempo,
        //         {{2,
        //           {(std::uint8_t)(((WORD)act->act.front().tempo << 3 &
        //           0xff00) >> 8),
        //            (std::uint8_t)((WORD)act->act.front().tempo << 3 & 0xff),
        //            (std::uint8_t)(((WORD)act->act.front().time << 5 & 0xff00)
        //            >> 8), (std::uint8_t)((WORD)act->act.front().time << 5 &
        //            0xff)}}});
        //     PrepareAndSend(COMMV1_loop);
        // }
        // for (auto ca = act->act.begin(); ca != act->act.end(); ca++) {
        //     Afx_lightblock t = {act->index, {*ca}};
        //     if (act->act.size() > 1)
        //         t.act.push_back(ca + 1 != act->act.end() ? *(ca + 1)
        //                                                  : act->act.front());
        //     DebugPrint("SDK: Set light " + to_string(act->index) + "\n");
        //     PrepareAndSend(COMMV1_color, SetMaskAndColor(&mods, &t));
        // }
        // // DebugPrint("SDK: Loop\n");
        // res = PrepareAndSend(COMMV1_loop);
        // chain++;
        // return res;
    }
    }
    return false;
}
inline bool Functions::PrepareAndSend(const unsigned char *command,
                                      vector<Afx_icommand> mods) {
    return PrepareAndSend(command, &mods);
}
bool Functions::PrepareAndSend(const unsigned char *command,
                               vector<Afx_icommand> *mods) {

    if (devHandle) { // Is device initialized?
        std::uint8_t buffer[MAX_BUFFERSIZE];
        int transferred = 0;
        bool needV8Feature = true;
        memset(buffer, version == API_V6 ? 0xff : 0x00, length);
        memcpy(buffer, command + 1, command[0]);

        if (mods) {
            for (auto &m : *mods) {
                // NOTE: As in linux size is 33 so we need to substract 1 from
                // vval in windows its 34
                memcpy(buffer + m.i, m.vval.data(), m.vval.size());
            }
            needV8Feature = mods->front().vval.size() == 1;
            mods->clear();
        }
        LOG_S(INFO) << "Sending USB packet (" << length << " std::uint8_ts): ";
        for (int i = 0; i < length; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(buffer[i]) << " ";
        }
        std::cout << std::dec << std::endl; // reset back to decimal formatting
        libusb_context *ctx = nullptr;
        libusb_init(&ctx);
        libusb_device_handle *handle =
            libusb_open_device_with_vid_pid(ctx, vid, pid);
        if (libusb_kernel_driver_active(handle, 0) == 1) {
            int rc = libusb_detach_kernel_driver(handle, 0);
            if (rc != 0) {
                LOG_S(ERROR) << "Failed to detach kernel driver: "
                             << libusb_error_name(rc);
                return false;
            }
        }
        if (!handle) {
            std::cerr << "Failed to open device\n";
            return false;
        }

        int rc = libusb_claim_interface(handle, 0);
        if (rc != 0) {
            std::cerr << "Failed to claim interface: " << libusb_error_name(rc)
                      << "\n";
            libusb_close(handle);
            return false;
        }

        switch (version) {
        case API_V2:
        case API_V3:
        case API_V4:

            // return 1;
            rc = libusb_control_transfer(
                handle,
                0x21, // VENDOR
                0x09, // SET_REPORT
                0x202, 0, reinterpret_cast<unsigned char *>(buffer), length,
                1000);
            libusb_release_interface(handle, 0);
            libusb_close(handle);
            libusb_exit(ctx);
            return rc >= 0;
        }
    }
}
bool Functions::SetV4Action(Afx_lightblock *act) {
    bool res = false;
    vector<Afx_icommand> mods;
    PrepareAndSend(COMMV4_colorSel, {{6, {act->index}}});
    for (auto ca = act->act.begin(); ca != act->act.end();) {
        auto &a = *ca;

        std::cout << "Action:"
                  << " type=" << static_cast<int>(a.type)
                  << " time=" << static_cast<int>(a.time)
                  << " tempo=" << static_cast<int>(a.tempo)
                  << " r=" << static_cast<int>(a.r)
                  << " g=" << static_cast<int>(a.g)
                  << " b=" << static_cast<int>(a.b) << std::endl;

        // 3 actions per record..
        for (std::uint8_t bPos = 3; bPos < length && ca != act->act.end();
             bPos += 8) {
            mods.push_back(
                {bPos,
                 {(std::uint8_t)(ca->type < AlienFX_A_Breathing
                                     ? ca->type
                                     : AlienFX_A_Morph),
                  ca->time, v4OpCodes[ca->type], 0,
                  (std::uint8_t)(ca->type == AlienFX_A_Color ? 0xfa
                                                             : ca->tempo),
                  ca->r, ca->g, ca->b}});
            ca++;
        }
        res = PrepareAndSend(COMMV4_colorSet, &mods);
    }
    return res;
}

bool Functions::UpdateColors() {

    if (inSet) {
        switch (version) {
        // case API_V9:
        //	inSet = !PrepareAndSend(COMMV9_update);
        //	GetDeviceStatus();
        //	break;
        case API_V5: {
            inSet = !PrepareAndSend(COMMV5_update);
        } break;
        case API_V4: {
            inSet = !PrepareAndSend(COMMV4_control);
        } break;
        case API_V3:
        case API_V2: {
            inSet = !PrepareAndSend(COMMV1_update);
            // WaitForBusy();
            // DebugPrint("Post-update status: " + to_string(GetDeviceStatus())
            // +
            //            "\n");
        } break;
        default:
            inSet = false;
        }
    }
    return !inSet;
}

bool Functions::Reset() {
    switch (version) {
    // case API_V9:
    //	if (chain) {
    //		PrepareAndSend(COMMV9_update);
    //		GetDeviceStatus();
    //		chain = 0;
    //	}
    //	break;
    case API_V6:
        // need initial reset if not done
        if (chain) {
            inSet = PrepareAndSend(COMMV6_systemReset);
            chain = 0;
        } else
            inSet = true;
        break;
    case API_V5: {
        inSet = PrepareAndSend(COMMV5_reset);
        // GetDeviceStatus();
    } break;
    case API_V4: {
        // WaitForReady();
        PrepareAndSend(COMMV4_control, {{3, {4}} /*, { 5, 0xff }*/});
        inSet = PrepareAndSend(COMMV4_control, {{3, {1}} /*, { 5, 0xff }*/});
    } break;
    case API_V3:
    case API_V2: {
        chain = 1;
        inSet = PrepareAndSend(COMMV1_reset);
        // WaitForReady();
        // DebugPrint("Post-Reset status: " + to_string(GetDeviceStatus()) +
        // "\n");
    } break;
    default:
        inSet = true;
    }
    return inSet;
}

} // namespace AlienFX_SDK
