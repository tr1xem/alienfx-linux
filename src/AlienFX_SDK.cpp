#include "AlienFX_SDK.h"
#include "alienfx_control.h"
#include "libusb.h"
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <loguru.hpp>

namespace AlienFX_SDK {
vector<Afx_icommand> *Functions::SetMaskAndColor(vector<Afx_icommand> *mods,
                                                 Afx_lightblock *act,
                                                 bool needInverse,
                                                 unsigned long index) {
    Afx_colorcode c;
    c.ci = index ? index : needInverse ? ~((1 << act->index)) : 1 << act->index;
    if (version < API_V4) {
        // index mask generation
        *mods = {{1, {v1OpCodes[act->act.front().type], chain, c.r, c.g, c.b}}};
    }
    Afx_action c1 = act->act.front(), c2 = {0};
    unsigned char tempo = act->act.front().tempo;
    if (act->act.size() >= 2)
        c2 = act->act.back();
    switch (version) {
    case API_V3:
        mods->push_back({6, {c1.r, c1.g, c1.b, c2.r, c2.g, c2.b}});
        break;
    case API_V2:
        mods->push_back(
            {6,
             {(unsigned char)((c1.r & 0xf0) | ((c1.g & 0xf0) >> 4)),
              (unsigned char)((c1.b & 0xf0) | ((c2.r & 0xf0) >> 4)),
              (unsigned char)((c2.g & 0xf0) | ((c2.b & 0xf0) >> 4))}});
        break;
    case API_V6: { // case API_V9: {
        vector<unsigned char> command{0x51,
                                      v6OpCodes[c1.type],
                                      0xd0,
                                      v6TCodes[c1.type],
                                      (unsigned char)index,
                                      c1.r,
                                      c1.g,
                                      c1.b};
        unsigned char mask = (unsigned char)(c1.r ^ c1.g ^ c1.b ^ index);
        switch (c1.type) {
        case AlienFX_A_Color:
            mask ^= 8;
            command.insert(command.end(), {bright, mask});
            break;
        case AlienFX_A_Pulse:
            mask ^= (unsigned char)(tempo ^ 1);
            command.insert(command.end(), {bright, tempo, mask});
            break;
        case AlienFX_A_Breathing:
            c2 = {0};
        case AlienFX_A_Morph:
            mask ^= (unsigned char)(c2.r ^ c2.g ^ c2.b ^ tempo ^ 4);
            command.insert(command.end(),
                           {c2.r, c2.g, c2.b, bright, 2, tempo, mask});
            break;
        }
        unsigned char lpos = 3, cpos = 5;
        // if (version == API_V9) {
        //	lpos = 7; cpos = 0x41;
        // }
        *mods = {{cpos, {command}}, {lpos, {(unsigned char)command.size(), 0}}};
    } break;
    }
    return mods;
}

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

void Functions::AddV8DataBlock(unsigned char bPos, vector<Afx_icommand> *mods,
                               Afx_lightblock *act) {
    mods->push_back(
        {bPos,
         {act->index, v8OpCodes[act->act.front().type], act->act.front().tempo,
          0xa5, act->act.front().time, 0xa, act->act.front().r,
          act->act.front().g, act->act.front().b, act->act.back().r,
          act->act.back().g, act->act.back().b,
          2 /*(byte)(act->act.size() > 1 ? 2 : 1)*/}});
}

void Functions::AddV5DataBlock(unsigned char bPos, vector<Afx_icommand> *mods,
                               unsigned char index, Afx_action *c) {
    mods->push_back({bPos, {(unsigned char)(index + 1), c->r, c->g, c->b}});
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
        AddV8DataBlock(5, &mods, act);
        PrepareAndSend(COMMV8_readyToColor);
        return PrepareAndSend(COMMV8_readyToColor, &mods);
    case API_V7: {
        mods = {{5, {v7OpCodes[act->act.front().type], bright, act->index}}};
        for (int ca = 0; ca < act->act.size(); ca++) {
            if (ca * 3 + 10 < length)
                mods.push_back({ca * 3 + 8,
                                {act->act.at(ca).r, act->act.at(ca).g,
                                 act->act.at(ca).b}});
        }
        return PrepareAndSend(COMMV7_control, &mods);
    }
    case API_V6:
        return PrepareAndSend(COMMV6_colorSet, SetMaskAndColor(&mods, act));
    // case API_V9:
    //	return PrepareAndSend(COMMV9_colorSet, SetMaskAndColor(&mods, act));
    case API_V5:
        AddV5DataBlock(4, &mods, act->index, &act->act.front());
        AddV5DataBlock(4, &mods, act->index, &act->act.front());
        PrepareAndSend(COMMV5_colorSet, &mods);
        return PrepareAndSend(COMMV5_loop);
    case API_V4:
        switch (act->act.front().type) {
        // FIX: Inspect this
        //  case AlienFX_A_Color: // it's a color, so set as color
        //      return PrepareAndSend(
        //          COMMV4_setOneColor,
        //          {{3,
        //            {act->act.front().r, act->act.front().g,
        //            act->act.front().b,
        //             0, 1, (std::uint8_t)act->index}}});
        case AlienFX_A_Power: { // Set power
            std::cout << "A_Power not implemented yet" << std::endl;
            return false;
            // vector<Afx_lightblock> t = {*act};
            // return SetPowerAction(&t);
        } break;
        default: // Set action
            return SetV4Action(act);
        }
    case API_V3:
    case API_V2: {
        std::cout << "V2 not implemented yet" << std::endl;
        return false;
        bool res = false;
        // check types and call
        switch (act->act.front().type) {
        case AlienFX_A_Power: { // SetPowerAction for power!
            if (act->act.size() > 1) {
                vector<Afx_lightblock> t = {{*act}};
                return SetPowerAction(&t);
            }
            break;
        }
        case AlienFX_A_Color:
            break;
        default:
            PrepareAndSend(
                COMMV1_setTempo,
                {{2,
                  {(std::uint8_t)(((unsigned short)act->act.front().tempo << 3 &
                                   0xff00) >>
                                  8),
                   (std::uint8_t)((unsigned short)act->act.front().tempo << 3 &
                                  0xff),
                   (std::uint8_t)(((unsigned short)act->act.front().time << 5 &
                                   0xff00) >>
                                  8),
                   (std::uint8_t)((unsigned short)act->act.front().time << 5 &
                                  0xff)}}});
            PrepareAndSend(COMMV1_loop);
        }
        for (auto ca = act->act.begin(); ca != act->act.end(); ca++) {
            Afx_lightblock t = {act->index, {*ca}};
            if (act->act.size() > 1)
                t.act.push_back(ca + 1 != act->act.end() ? *(ca + 1)
                                                         : act->act.front());
            LOG_S(INFO) << "SDK: Set light " << act->index << "\n";
            PrepareAndSend(COMMV1_color, SetMaskAndColor(&mods, &t));
        }
        // DebugPrint("SDK: Loop\n");
        res = PrepareAndSend(COMMV1_loop);
        chain++;
        return res;
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
                memcpy(buffer + (m.i - 1), m.vval.data(), m.vval.size());
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
        case API_V5:
            return false;
            // res =  DeviceIoControl(devHandle, IOCTL_HID_SET_FEATURE, buffer,
            // length, 0, 0, &written, NULL);
            // return HidD_SetFeature(devHandle, buffer, length);
            break;
        case API_V6:
            return false;
            // /*res =*/return WriteFile(devHandle, buffer, length, &written,
            //                           NULL);
            // if (size == 3)
            //	res &= ReadFile(devHandle, buffer, length, &written, NULL);
            break;
        case API_V7:
            return false;
            // WriteFile(devHandle, buffer, length, &written, NULL);
            // return ReadFile(devHandle, buffer, length, &written, NULL);
            break;
        case API_V8:
            return false;
            // if (needV8Feature) {
            //     Sleep(3);
            //     bool res = HidD_SetFeature(devHandle, buffer, length);
            //     Sleep(6);
            //     return res;
            // } else {
            //     return WriteFile(devHandle, buffer, length, &written, NULL);
            // }
            break;
        }
    }
    return false;
}
bool Functions::SetV4Action(Afx_lightblock *act) {
    bool res = false;
    vector<Afx_icommand> mods;
    PrepareAndSend(COMMV4_colorSel, {{6, {act->index}}});
    for (auto ca = act->act.begin(); ca != act->act.end();) {
        auto &a = *ca;

#ifdef DEBUG
        LOG_S(INFO) << "Action:"
                    << " type=" << static_cast<int>(a.type)
                    << " time=" << static_cast<int>(a.time)
                    << " tempo=" << static_cast<int>(a.tempo)
                    << " r=" << static_cast<int>(a.r)
                    << " g=" << static_cast<int>(a.g)
                    << " b=" << static_cast<int>(a.b) << std::endl;
#endif

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
        PrepareAndSend(COMMV4_control, {{4, {4}} /*, { 5, 0xff }*/});
        inSet = PrepareAndSend(COMMV4_control, {{4, {1}} /*, { 5, 0xff }*/});
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

bool Functions::SetPowerAction(vector<Afx_lightblock> *act, bool save) {
    Afx_lightblock *pwr = NULL;
    switch (version) {
    // ToDo - APIv8 profile save
    case API_V4: {
        UpdateColors();
        if (save) {
            PrepareAndSend(COMMV4_control, {{4, {4, 0, 0x61}}});
            PrepareAndSend(COMMV4_control, {{4, {1, 0, 0x61}}});
            for (auto ca = act->begin(); ca != act->end(); ca++)
                if (ca->act.front().type != AlienFX_A_Power)
                    SetV4Action(&(*ca));
            PrepareAndSend(COMMV4_control, {{4, {2, 0, 0x61}}});
            PrepareAndSend(COMMV4_control, {{4, {6, 0, 0x61}}});
        } else {
            pwr = &act->front();
            // Now set power button....
            for (unsigned char cid = 0x5b; cid < 0x61; cid++) {
                // Init query...
                PrepareAndSend(COMMV4_setPower, {{4, {4, 0, cid}}});
                PrepareAndSend(COMMV4_setPower, {{4, {1, 0, cid}}});
                // Now set color by type...
                Afx_lightblock tact{pwr->index};
                switch (cid) {
                case 0x5b: // AC sleep
                    tact.act.push_back(
                        pwr->act.front()); // afx_act{AlienFX_A_Power, 3, 0x64,
                                           // pwr->act.front().r,
                                           // pwr->act.front().g,
                                           // pwr->act.front().b});
                    tact.act.push_back(
                        Afx_action{AlienFX_A_Power, 3, 0x64, 0, 0, 0});
                    break;
                case 0x5c: // AC power
                    tact.act.push_back(
                        pwr->act.front()); // afx_act{AlienFX_A_Color, 0, 0,
                                           // pwr->act.front().r,
                                           // act[index].act.front().g,
                                           // act[index].act.front().b});
                    tact.act.front().type = AlienFX_A_Color;
                    break;
                case 0x5d: // Charge
                    tact.act.push_back(
                        pwr->act.front()); // afx_act{AlienFX_A_Power, 3, 0x64,
                                           // act[index].act.front().r,
                                           // act[index].act.front().g,
                                           // act[index].act.front().b});
                    tact.act.push_back(
                        pwr->act.back()); // afx_act{AlienFX_A_Power, 3, 0x64,
                                          // act[index].act.back().r,
                                          // act[index].act.back().g,
                                          // act[index].act.back().b});
                    break;
                case 0x5e: // Battery sleep
                    tact.act.push_back(
                        pwr->act.back()); // afx_act{AlienFX_A_Power, 3, 0x64,
                                          // act[index].act.back().r,
                                          // act[index].act.back().g,
                                          // act[index].act.back().b});
                    tact.act.push_back(
                        Afx_action{AlienFX_A_Power, 3, 0x64, 0, 0, 0});
                    break;
                case 0x5f: // Batt power
                    tact.act.push_back(
                        pwr->act.back()); // afx_act{AlienFX_A_Color, 0, 0,
                                          // act[index].act.back().r,
                                          // act[index].act.back().g,
                                          // act[index].act.back().b});
                    tact.act.front().type = AlienFX_A_Color;
                    break;
                case 0x60: // Batt critical
                    tact.act.push_back(
                        pwr->act.back()); // afx_act{AlienFX_A_Color, 0, 0,
                                          // act[index].act.back().r,
                                          // act[index].act.back().g,
                                          // act[index].act.back().b});
                    tact.act.front().type = AlienFX_A_Pulse;
                }
                SetV4Action(&tact);
                // And finish
                PrepareAndSend(COMMV4_setPower, {{4, {2, 0, cid}}});
            }

            PrepareAndSend(COMMV4_control, {{4, {5}} /*, { 6, 0x61 }*/});
#ifdef _DEBUG
            if (!WaitForBusy())
                DebugPrint("Power device busy timeout!\n");
#else
            // WaitForBusy();
#endif
            // WaitForReady();
        }
    } break;
    case API_V3:
    case API_V2: {
        if (!inSet)
            Reset();
        for (auto ca = act->begin(); ca != act->end(); ca++)
            if (ca->act.front().type != AlienFX_A_Power)
                SavePowerBlock(1, &(*ca), false);
            else
                pwr = &(*ca);
        if (pwr) {
            if (act->size() > 1) {
                PrepareAndSend(COMMV1_save);
                Reset();
            }
            chain = 1;
            if (save) {
                // 08 02 - AC standby
                Afx_lightblock tact = *pwr;
                tact.act.front().type = AlienFX_A_Morph;
                tact.act.back() = {AlienFX_A_Morph};
                SavePowerBlock(2, &tact, true, true, true);
                // 08 05 - AC power
                tact.act.front().type = AlienFX_A_Color;
                SavePowerBlock(5, &tact, true);
                // 08 06 - charge
                tact.act.back() = pwr->act.back();
                tact.act.front().type = tact.act.back().type = AlienFX_A_Morph;
                SavePowerBlock(6, &tact, true, true);
                // 08 07 - Battery standby
                tact.act.front() = pwr->act.back();
                tact.act.front().type = AlienFX_A_Morph;
                tact.act.back() = {AlienFX_A_Morph};
                SavePowerBlock(7, &tact, true, true, true);
                // 08 08 - battery
                tact.act.front().type = AlienFX_A_Color;
                SavePowerBlock(8, &tact, true);
                // 08 09 - battery critical
                tact.act.front().type = AlienFX_A_Pulse;
                SavePowerBlock(9, &tact, true);
            }
            SetColor(pwr->index, pwr->act.front());
            UpdateColors();
        } else {
            PrepareAndSend(COMMV1_save);
            Reset();
        }
    } break;
    }
    return true;
}

void Functions::SavePowerBlock(unsigned char blID, Afx_lightblock *act,
                               bool needSave, bool needSecondary,
                               bool needInverse) {
    vector<Afx_icommand> mods, group = {{2, {blID}}};
    PrepareAndSend(COMMV1_saveGroup, &group);
    PrepareAndSend(COMMV1_color, SetMaskAndColor(&mods, act));
    if (needSecondary) {
        Afx_lightblock t = *act;
        swap(t.act.front(), t.act.back());
        PrepareAndSend(COMMV1_saveGroup, &group);
        PrepareAndSend(COMMV1_color, SetMaskAndColor(&mods, &t));
    }
    if (needInverse) {
        Afx_lightblock t = {act->index, {{AlienFX_A_Color}, {AlienFX_A_Color}}};
        PrepareAndSend(COMMV1_saveGroup, &group);
        PrepareAndSend(COMMV1_loop);
        chain++;
        PrepareAndSend(COMMV1_saveGroup, &group);
        PrepareAndSend(COMMV1_color, SetMaskAndColor(&mods, &t, true));
    }
    PrepareAndSend(COMMV1_saveGroup, &group);
    PrepareAndSend(COMMV1_loop);
    chain++;

    if (needSave) {
        PrepareAndSend(COMMV1_save);
        Reset();
    }

    // return true;
}

} // namespace AlienFX_SDK
//
