#include "AlienFX_SDK.h"
#include "alienfx_control.h"
#include "libusb.h"
#include "libusb_helper.h"
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <loguru.hpp>
#include <unistd.h>
#define LOWORD(l) ((uint16_t)((l) & 0xFFFF))
#define HIWORD(l) ((uint16_t)(((l) >> 16) & 0xFFFF))

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
    uint8_t tempo = act->act.front().tempo;
    if (act->act.size() >= 2)
        c2 = act->act.back();
    switch (version) {
    case API_V3:
        mods->push_back({6, {c1.r, c1.g, c1.b, c2.r, c2.g, c2.b}});
        break;
    case API_V2:
        mods->push_back({6,
                         {(uint8_t)((c1.r & 0xf0) | ((c1.g & 0xf0) >> 4)),
                          (uint8_t)((c1.b & 0xf0) | ((c2.r & 0xf0) >> 4)),
                          (uint8_t)((c2.g & 0xf0) | ((c2.b & 0xf0) >> 4))}});
        break;
    case API_V6: { // case API_V9: {
        vector<uint8_t> command{0x51,           v6OpCodes[c1.type],
                                0xd0,           v6TCodes[c1.type],
                                (uint8_t)index, c1.r,
                                c1.g,           c1.b};
        uint8_t mask = (uint8_t)(c1.r ^ c1.g ^ c1.b ^ index);
        switch (c1.type) {
        case AlienFX_A_Color:
            mask ^= 8;
            command.insert(command.end(), {bright, mask});
            break;
        case AlienFX_A_Pulse:
            mask ^= (uint8_t)(tempo ^ 1);
            command.insert(command.end(), {bright, tempo, mask});
            break;
        case AlienFX_A_Breathing:
            c2 = {0};
        case AlienFX_A_Morph:
            mask ^= (uint8_t)(c2.r ^ c2.g ^ c2.b ^ tempo ^ 4);
            command.insert(command.end(),
                           {c2.r, c2.g, c2.b, bright, 2, tempo, mask});
            break;
        }
        uint8_t lpos = 3, cpos = 5;
        // if (version == API_V9) {
        //	lpos = 7; cpos = 0x41;
        // }
        *mods = {{cpos, {command}}, {lpos, {(uint8_t)command.size(), 0}}};
    } break;
    }
    return mods;
}

inline bool Functions::PrepareAndSend(const uint8_t *command,
                                      vector<Afx_icommand> mods) {
    return PrepareAndSend(command, &mods);
}

bool Functions::PrepareAndSend(const uint8_t *command,
                               vector<Afx_icommand> *mods) {

    std::uint8_t buffer[MAX_BUFFERSIZE];
    int written{0};
    bool needV8Feature = true;
    if (reportIDList[version] == 0) {
        memset(buffer, version == API_V6 ? 0xff : 0x00, length);
        memcpy(buffer, command + 1, command[0]);
    } else {
        memset(buffer, version == API_V6 ? 0xff : 0x00, length);
        memcpy(buffer, command, command[0] + 1);
        buffer[0] = reportIDList[version];
    }
    // if (version == API_V8 || version == API_V5)
    //     buffer[0] = reportIDList[version];

    if (mods) {
        for (auto &m : *mods) {
            // NOTE: As in linux size of buffer is 33 so we need to substract 1
            // from it as in windows buffer size is 34
            memcpy(buffer + (m.i - 1), m.vval.data(), m.vval.size());
        }
        needV8Feature = mods->front().vval.size() == 1;
        mods->clear();
    }
#ifdef DEBUG
    LOG_S(INFO) << "Sending USB packet (" << length << " std::uint8_ts): ";
    for (int i = 0; i < length; i++) {
        std::cout << std::hex << setw(2) << std::setfill('0')
                  << static_cast<int>(buffer[i]) << " ";
    }
    std::cout << std::dec << std::endl;
#endif
    bool result = false;

    // NOTE: Attach device and claim interface
    if (libusb_kernel_driver_active(devHandle, 0) == 1) {
        int rc = libusb_detach_kernel_driver(devHandle, 0);
        if (rc != 0) {
            LOG_S(ERROR) << "Failed to detach kernel driver: "
                         << libusb_error_name(rc);
            return false;
        }
    }
    if (!devHandle) {
        LOG_S(ERROR) << "Failed to open device";
        return false;
    }

    int rc = libusb_claim_interface(devHandle, 0);
    if (rc != 0) {
        LOG_S(ERROR) << "Failed to claim interface: " << libusb_error_name(rc);
        return false;
    }

    switch (version) {
    case API_V2:
    case API_V3:
    case API_V4:
        result = HidD_SetOutputReport(devHandle, buffer, length);
        break;
    case API_V5:
        result = HidD_SetFeature(devHandle, buffer, length);
        break;
    case API_V6:
        result = WriteFile(devHandle, buffer, length, written, out_ep);
        break;
    case API_V7:
        WriteFile(devHandle, buffer, length, written, out_ep);
        result = ReadFile(devHandle, buffer, length, written, in_ep);
        break;
    case API_V8:
        if (needV8Feature) {
            usleep(3000);
            result = HidD_SetFeature(devHandle, buffer, length);
            usleep(6000);
            break;
        } else {
            result = WriteFile(devHandle, buffer, length, written, out_ep);
        }

        break;
    }
    libusb_release_interface(devHandle, 0);
    return result;
}

void Functions::SavePowerBlock(uint8_t blID, Afx_lightblock *act, bool needSave,
                               bool needSecondary, bool needInverse) {
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

bool Functions::AlienFXProbeDevice(libusb_device *device, libusb_context *ctxx,
                                   unsigned short vidd, unsigned short pidd) {
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

            // printf("\nInterface %d, Alt %d:\n", ifc, alt);
            // printf("  bInterfaceNumber    %u\n", altset.bInterfaceNumber);
            // printf("  baltseternateSetting   %u\n",
            // altset.bAlternateSetting); printf("  bNumEndpoints       %u\n",
            // altset.bNumEndpoints); printf("  bInterfaceClass     0x%02x\n",
            // altset.bInterfaceClass); printf("  bInterfaceSubClass  0x%02x\n",
            // altset.bInterfaceSubClass); printf("  bInterfaceProtocol
            // 0x%02x\n",
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
                // printf("      bmAttributes     0x%02x\n", e.bmAttributes);
                // printf("      wMaxPacketSize   %u\n",
                //        e.wMaxPacketSize & 0x07FF);
                // printf("      bInterval        %u\n", e.bInterval);
                // if ((e.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
                //     LIBUSB_TRANSFER_TYPE_INTERRUPT)
                //     continue;

                // Store IN/OUT endpoint
                if ((e.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                    LIBUSB_ENDPOINT_OUT) {
                    out_ep = e.bEndpointAddress;
                } else {
                    in_ep = e.bEndpointAddress;
                }

                // NOTE: Check the max packet size for IN endpoint
                if ((e.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ==
                    LIBUSB_ENDPOINT_IN) {
                    length = e.wMaxPacketSize & 0x07FF;
                    /*
                    LOG_S(INFO) << "VID: 0x" << std::hex << vidd << " PID: 0x"
                                << std::hex << (int)pidd
                                << " IN EP: " << std::hex << (int)in_ep
                                << ", Length: " << std::dec << (int)length;
                    */
                }
            }
        }
    }

    libusb_free_config_descriptor(config);

    switch (vidd) {
    case 0x0d62: // Darfon
        switch (length + 1) {
        case 65:
            version = API_V5;
            break;
        }
        break;
    case 0x187c: // Alienware
        switch (length + 1) {
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
        if (length + 1 == 65)
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

    pid = pidd;
    vid = vidd;
    ctx = ctxx;
    libusb_device_descriptor desc{};
    libusb_get_device_descriptor(device, &desc);
    uint8_t buf[256];
    if (libusb_open(device, &devHandle) != 0) {
        return false;
    }

    if (desc.iManufacturer &&
        libusb_get_string_descriptor_ascii(devHandle, desc.iManufacturer, buf,
                                           sizeof(buf)) > 0) {
        description.append(reinterpret_cast<char *>(buf));
    }
    description.append(" ");
    if (desc.iProduct && libusb_get_string_descriptor_ascii(
                             devHandle, desc.iProduct, buf, sizeof(buf)) > 0) {
        description.append(reinterpret_cast<char *>(buf));
    }
#ifdef DEBUG
    LOG_S(INFO) << "Probing device VID: 0x" << std::hex << std::setw(4)
                << std::setfill('0') << static_cast<int>(vidd) << ", PID: 0x"
                << std::setw(4) << std::setfill('0') << static_cast<int>(pidd)
                << ", Version: " << std::dec
                << version // switch back to decimal
                << ", Length: " << length << ", Description: " << description
                << ", OUT EP: 0x" << std::hex << static_cast<int>(out_ep)
                << ", IN EP: 0x" << static_cast<int>(in_ep);

#endif
    return version != API_UNKNOWN;
}

// NOTE: Not needed? as we already initalize in probe
//
//   bool Functions::AlienFXInitialize(unsigned short vidd, unsigned short
//   pidd)
//   {
//       unsigned long dwRequiredSize = 0;
//       SP_DEVICE_INTERFACE_DATA deviceInterfaceData{
//           sizeof(SP_DEVICE_INTERFACE_DATA)};
//       devHandle = NULL;
//
//       HDEVINFO hDevInfo =
//           SetupDiGetClassDevs(&GUID_DEVINTERFACE_HID, NULL, NULL,
//                               DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
//       if (hDevInfo != INVALID_HANDLE_VALUE) {
//           for (unsigned long dw = 0;
//                SetupDiEnumDeviceInterfaces(hDevInfo, NULL,
//                &GUID_DEVINTERFACE_HID,
//                                            dw, &deviceInterfaceData) &&
//                (deviceInterfaceData.Flags & SPINT_ACTIVE) &&
//                !AlienFXProbeDevice(hDevInfo, &deviceInterfaceData, vidd,
//                pidd); dw++)
//               ;
//           SetupDiDestroyDeviceInfoList(hDevInfo);
//       }
//       return version != API_UNKNOWN;
//   }

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
        GetDeviceStatus();
    } break;
    case API_V4: {
        WaitForReady();
        PrepareAndSend(COMMV4_control, {{4, {4}} /*, { 5, 0xff }*/});
        inSet = PrepareAndSend(COMMV4_control, {{4, {1}} /*, { 5, 0xff }*/});
    } break;
    case API_V3:
    case API_V2: {
        chain = 1;
        inSet = PrepareAndSend(COMMV1_reset);
        WaitForReady();
        LOG_S(INFO) << "Post-Reset status: " << to_string(GetDeviceStatus());
    } break;
    default:
        inSet = true;
    }
    return inSet;
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
            LOG_S(INFO) << "Post-update status: " +
                               to_string(GetDeviceStatus());
        } break;
        default:
            inSet = false;
        }
    }
    return !inSet;
}
bool Functions::SetColor(uint8_t index, Afx_action c) {
    Afx_lightblock act{index, {c}};
    return SetAction(&act);
}

void Functions::AddV8DataBlock(uint8_t bPos, vector<Afx_icommand> *mods,
                               Afx_lightblock *act) {
    mods->push_back(
        {bPos,
         {act->index, v8OpCodes[act->act.front().type], act->act.front().tempo,
          0xa5, act->act.front().time, 0xa, act->act.front().r,
          act->act.front().g, act->act.front().b, act->act.back().r,
          act->act.back().g, act->act.back().b,
          2 /*(std::uint8_t)(act->act.size() > 1 ? 2 : 1)*/}});
}

void Functions::AddV5DataBlock(uint8_t bPos, vector<Afx_icommand> *mods,
                               uint8_t index, Afx_action *c) {
    mods->push_back({bPos, {(uint8_t)(index + 1), c->r, c->g, c->b}});
}

bool Functions::SetMultiColor(vector<uint8_t> *lights, Afx_action c) {
    bool val = false;
    Afx_lightblock act{0, {c}};
    vector<Afx_icommand> mods;
    if (!inSet)
        Reset();
    switch (version) {
    case API_V8: {
        PrepareAndSend(COMMV8_readyToColor,
                       {{2, {(std::uint8_t)lights->size()}}});
        auto nc = lights->begin();
        for (std::uint8_t cnt = 1; nc != lights->end(); cnt++) {
            for (std::uint8_t bPos = 5; bPos < length && nc != lights->end();
                 bPos += 15) {
                act.index = *nc;
                AddV8DataBlock(bPos, &mods, &act);
                nc++;
            }
            if (mods.size()) {
                mods.push_back({4, {cnt}});
                val = PrepareAndSend(COMMV8_readyToColor, &mods);
            }
        }
    } break;
    case API_V5:
        for (auto nc = lights->begin(); nc != lights->end();) {
            for (std::uint8_t bPos = 4; bPos < length && nc != lights->end();
                 bPos += 4) {
                AddV5DataBlock(bPos, &mods, *nc, &c);
                nc++;
            }
            if (mods.size())
                PrepareAndSend(COMMV5_colorSet, &mods);
        }
        val = PrepareAndSend(COMMV5_loop);
        break;
    case API_V4: {
        mods = {{3, {c.r, c.g, c.b, 0, (std::uint8_t)lights->size()}},
                {8, *lights}};
        val = PrepareAndSend(COMMV4_setOneColor, &mods);
    } break;
    case API_V3:
    case API_V2:
    case API_V6: // case API_V9:
    case API_ACPI: {
        unsigned long fmask = 0;
        for (auto nc = lights->begin(); nc < lights->end(); nc++)
            fmask |= 1 << (*nc);
        SetMaskAndColor(&mods, &act, false, fmask);
        switch (version) {
        case API_V6:
            val = PrepareAndSend(COMMV6_colorSet, &mods);
            break;
            // case API_V9:
            //	val = PrepareAndSend(COMMV9_colorSet, &mods);
            //	break;
        default:
            PrepareAndSend(COMMV1_color, &mods);
            val = PrepareAndSend(COMMV1_loop);
            chain++;
        }
    } break;
    default:
        for (auto nc = lights->begin(); nc < lights->end(); nc++) {
            act.index = *nc;
            val = SetAction(&act);
        }
    }
    return val;
}

bool Functions::SetMultiAction(vector<Afx_lightblock> *act, bool save) {
    bool val = true;
    vector<Afx_icommand> mods;

    if (!inSet)
        Reset();
    switch (version) {
    case API_V8: {
        // std::uint8_t bPos = 5, cnt = 0;
        PrepareAndSend(COMMV8_readyToColor, {{2, {(std::uint8_t)act->size()}}});
        auto nc = act->begin();
        for (std::uint8_t cnt = 1; nc != act->end(); cnt++) {
            for (std::uint8_t bPos = 5; bPos < length && nc != act->end();
                 bPos += 15) {
                AddV8DataBlock(bPos, &mods, &(*nc));
                nc++;
            }
            mods.push_back({4, {cnt}});
            val = PrepareAndSend(COMMV8_readyToColor, &mods);
        }
    } break;
    case API_V5: {
        for (auto nc = act->begin(); nc != act->end();) {
            for (std::uint8_t bPos = 4; bPos < length && nc != act->end();
                 bPos += 4) {
                AddV5DataBlock(bPos, &mods, nc->index, &nc->act.front());
                nc++;
            }
            PrepareAndSend(COMMV5_colorSet, &mods);
        }
        val = PrepareAndSend(COMMV5_loop);
    } break;
    default: {
        for (auto nc = act->begin(); nc != act->end(); nc++)
            val = SetAction(&(*nc));
    } break;
    }

    return save ? SetPowerAction(act, save) : val;
}

bool Functions::SetV4Action(Afx_lightblock *act) {
    bool res = false;
    vector<Afx_icommand> mods;
    PrepareAndSend(COMMV4_colorSel, {{6, {act->index}}});

#ifdef DEBUG
    // for (auto ca = act->act.begin(); ca != act->act.end();) {
    //     auto &a = *ca;
    //     LOG_S(INFO) << "Action:"
    //                 << " type=" << static_cast<int>(a.type)
    //                 << " time=" << static_cast<int>(a.time)
    //                 << " tempo=" << static_cast<int>(a.tempo)
    //                 << " r=" << static_cast<int>(a.r)
    //                 << " g=" << static_cast<int>(a.g)
    //                 << " b=" << static_cast<int>(a.b);
    // }
#endif

    for (auto ca = act->act.begin(); ca != act->act.end();) {
        // 3 actions per record..
        for (uint8_t bPos = 3; bPos < length && ca != act->act.end();
             bPos += 8) {
            mods.push_back(
                {bPos,
                 {(uint8_t)(ca->type < AlienFX_A_Breathing ? ca->type
                                                           : AlienFX_A_Morph),
                  ca->time, v4OpCodes[ca->type], 0,
                  (uint8_t)(ca->type == AlienFX_A_Color ? 0xfa : ca->tempo),
                  ca->r, ca->g, ca->b}});
            ca++;
        }
        res = PrepareAndSend(COMMV4_colorSet, &mods);
    }
    return res;
}

bool Functions::SetAction(Afx_lightblock *act) {
    if (act->act.empty())
        return false;
    if (!inSet)
        Reset();

    vector<Afx_icommand> mods;
    switch (version) {
    case API_V8:
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
    //	return PrepareAndSend(COMMV9_colorSet, SetMaskAndColor(&mods,
    // act));
    case API_V5:
        AddV5DataBlock(4, &mods, act->index, &act->act.front());
        AddV5DataBlock(4, &mods, act->index, &act->act.front());
        PrepareAndSend(COMMV5_colorSet, &mods);
        return PrepareAndSend(COMMV5_loop);
    case API_V4:
        switch (act->act.front().type) {
            // NOTE: This is for fast pace color change
        case AlienFX_A_Color: // it's a color, so set as color
            return PrepareAndSend(
                COMMV4_setOneColor,
                {{3,
                  {act->act.front().r, act->act.front().g, act->act.front().b,
                   0, 1, (std::uint8_t)act->index}}});
        case AlienFX_A_Power: { // Set power
            vector<Afx_lightblock> t = {*act};
            return SetPowerAction(&t);
        } break;
        default: // Set action
            return SetV4Action(act);
        }
    case API_V3:
    case API_V2: {
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
            LOG_S(INFO) << "SDK: Set light " << act->index;
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
            for (uint8_t cid = 0x5b; cid < 0x61; cid++) {
                // Init query...
                PrepareAndSend(COMMV4_setPower, {{4, {4, 0, cid}}});
                PrepareAndSend(COMMV4_setPower, {{4, {1, 0, cid}}});
                // Now set color by type...
                Afx_lightblock tact{pwr->index};
                switch (cid) {
                case 0x5b: // AC sleep
                    tact.act.push_back(
                        pwr->act.front()); // afx_act{AlienFX_A_Power, 3,
                                           // 0x64, pwr->act.front().r,
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
                        pwr->act.back()); // afx_act{AlienFX_A_Power, 3,
                                          // 0x64, act[index].act.back().r,
                                          // act[index].act.back().g,
                                          // act[index].act.back().b});
                    break;
                case 0x5e: // Battery sleep
                    tact.act.push_back(
                        pwr->act.back()); // afx_act{AlienFX_A_Power, 3,
                                          // 0x64, act[index].act.back().r,
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
#ifdef DEBUG
            if (!WaitForBusy())
                LOG_S(WARNING) << "Power device busy timeout!";
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

bool Functions::SetBrightness(std::uint8_t brightness, std::uint8_t gbr,
                              vector<Afx_light> *mappings, bool power) {
    // return true if update needed
    LOG_S(INFO) << "State update: PID: 0x" << std::hex << std::setw(4)
                << std::setfill('0') << pid << ", VID: 0x" << std::hex
                << std::setw(4) << std::setfill('0') << vid
                << ", brightness: " << to_string(brightness)
                << ", power: " << to_string(power);

    if (inSet)
        UpdateColors();
    int oldBr = bright;
    bright = (((brightness * gbr) / 255) * brightnessScale[version]) / 0xff;
    switch (version) {
    case API_V8:
        PrepareAndSend(COMMV8_setBrightness, {{2, {bright}}});
        break;
    case API_V5:
        Reset();
        PrepareAndSend(COMMV5_turnOnSet, {{4, {bright}}});
        break;
    case API_V4: {
        int pos = 6;
        vector<Afx_icommand> mods{
            {3,
             {(std::uint8_t)(0x64 - bright), 0,
              (std::uint8_t)mappings->size()}} /*, { 6, idlist}*/};
        for (auto i = mappings->begin(); i < mappings->end(); i++)
            if (!i->flags || power) {
                mods.push_back({pos++, {(std::uint8_t)i->lightid}});
            }
        PrepareAndSend(COMMV4_turnOn, &mods);
        break;
    }
    case API_V3:
    case API_V2:
        if (!bright || !oldBr) {
            PrepareAndSend(COMMV1_reset, {{2,
                                           {(std::uint8_t)(brightness ? 4
                                                           : power    ? 3
                                                                      : 1)}}});
            WaitForReady();
        }
        PrepareAndSend(COMMV1_dim, {{2, {bright}}});
        return bright && !oldBr;
    case API_V6:
    case API_V7: // case API_V9:
        return true;
    }
    return false;
}
bool Functions::SetGlobalEffects(std::uint8_t effType, std::uint8_t mode,
                                 std::uint8_t nc, std::uint8_t tempo,
                                 Afx_colorcode act1, Afx_colorcode act2) {
    vector<Afx_icommand> mods;
    switch (version) {
    case API_V8:
        PrepareAndSend(COMMV8_effectReady);
        PrepareAndSend(COMMV8_effectReady,
                       {{3,
                         {effType, act1.r, act1.g, act1.b, act2.r, act2.g,
                          act2.b, tempo, bright, 1, mode, nc}}});
        usleep(20);
        return true;
    case API_V5:
        if (inSet)
            UpdateColors();
        Reset();
        if (!effType)
            PrepareAndSend(COMMV5_setEffect, {{2, {1, 0xfe}}});
        else
            PrepareAndSend(COMMV5_setEffect,
                           {{2, {effType, tempo}},
                            {9,
                             {(std::uint8_t)(nc - 1), act1.r, act1.g, act1.b,
                              act2.r, act2.g, act2.b /*, mode*/}}});
        return UpdateColors();
    }
    return false;
}

std::uint8_t Functions::GetDeviceStatus() {
    std::uint8_t buffer[MAX_BUFFERSIZE];
    // unsigned long written;
    if (devHandle)
        switch (version) {
        // case API_V9:
        //	HidD_GetInputReport(devHandle, buffer, length);
        //	return 1;
        case API_V5: {
            PrepareAndSend(COMMV5_status);
            if (HidD_GetFeature(devHandle, buffer, length))
                // if (DeviceIoControl(devHandle, IOCTL_HID_GET_FEATURE, 0,
                // 0, buffer, length, &written, NULL))
                return buffer[2];
        } break;
        case API_V4: {
            if (HidD_GetInputReport(devHandle, buffer, length, in_ep)) {
                // if (DeviceIoControl(devHandle,
                // IOCTL_HID_GET_INPUT_REPORT, 0, 0, buffer, length,
                // &written, NULL)) DebugPrint("Status: " +
                // to_string(buffer[2]) + "\n");
                return buffer[2];
            }
        } break;
        case API_V3:
        case API_V2: {
            PrepareAndSend(COMMV1_status);
            if (HidD_GetInputReport(devHandle, buffer, length, in_ep))
                // if (DeviceIoControl(devHandle,
                // IOCTL_HID_GET_INPUT_REPORT, 0, 0, buffer, length,
                // &written, NULL))
                return buffer[0];
        } break;
        }
    return 0;
}

std::uint8_t Functions::WaitForReady() {
    int i = 0;
    switch (version) {
    case API_V3:
    case API_V2:
        // if (!GetDeviceStatus())
        //	Reset();
        for (; i < 100 && GetDeviceStatus() != ALIENFX_V2_READY; i++)
            usleep(10);
        return i < 100;
    case API_V4:
        while (!IsDeviceReady())
            usleep(20);
        return 1;
    default:
        return GetDeviceStatus();
    }
}

std::uint8_t Functions::WaitForBusy() {
    int i = 0;
    switch (version) {
    case API_V3:
    case API_V2:
        if (GetDeviceStatus())
            for (; i < 100 && GetDeviceStatus() != ALIENFX_V2_BUSY; i++)
                usleep(10);
        return i < 100;
        break;
    case API_V4: {
        if (pid == 0x551) // patch for newer v4
            return true;
        else {
            for (; i < 500 && GetDeviceStatus() != ALIENFX_V4_BUSY; i++)
                usleep(20);
            return i < 500;
        }
    } break;
    default:
        return GetDeviceStatus();
    }
}

std::uint8_t Functions::IsDeviceReady() {
    int status = GetDeviceStatus();
    switch (version) {
    case API_V5:
        return status != ALIENFX_V5_WAITUPDATE;
    case API_V4:
        // #ifdef DEBUG
        //         LOG_S(INFO) << "Status: " << std::hex << status;
        //         status = status ? status != ALIENFX_V4_BUSY : 0xff;
        //
        //         if (status == 0xff)
        //             LOG_S(ERROR) << "Device hang!";
        //         return status;
        // #else
        return status ? status != ALIENFX_V4_BUSY : 0xff;
        // #endif
    case API_V3:
    case API_V2:
        return status == ALIENFX_V2_READY ? 1 : Reset();
    default:
        return !inSet;
    }
}

Functions::~Functions() {
    if (devHandle) {
        libusb_close(devHandle);
        LOG_S(INFO) << "Functions destructor: Close device handle for VID 0x"
                    << std::hex << vid << " PID: 0x" << pid;
    }
}

Mappings::Mappings() {
    int result = libusb_init(&ctx);
    if (result < 0) {
        LOG_S(ERROR) << "Failed to initialize libusb:  "
                     << libusb_error_name(result);
    } else {
        LOG_S(INFO) << "Mappings constructor: Initialized libusb";
    }
}

Mappings::~Mappings() {
    for (auto &d : fxdevs) {
        delete d.dev;
    }
    if (ctx) {
        libusb_exit(ctx);
        LOG_S(INFO) << "Mappings destructor: Free libusb";
    }
}

void Mappings::AlienFxUpdateDevice(Functions *dev) {
// TODO: Devinfo persistant vector
#ifdef DEBUG
    LOG_S(INFO) << "Update device " << std::hex << dev->vid << ":" << std::hex
                << dev->pid;
#endif
    // auto devInfo = GetDeviceById(dev->pid, dev->vid);
    // if (devInfo) {
    //     devInfo->version = dev->version;
    //     devInfo->present = true;
    //     activeLights += (unsigned)devInfo->lights.size();
    //     if (devInfo->dev) {
    //         delete dev;
    //         DebugPrint("Scan: VID: " + to_string(devInfo->vid) +
    //                    ", PID: " + to_string(devInfo->pid) + ", Version:
    //                    " + to_string(devInfo->version) + " - present
    //                    already\n");
    //         devInfo->arrived = false;
    //     } else {
    //         devInfo->dev = dev;
    //         deviceListChanged = devInfo->arrived = true;
    //         DebugPrint("Scan: VID: " + to_string(devInfo->vid) +
    //                    ", PID: " + to_string(devInfo->pid) + ", Version:
    //                    " + to_string(devInfo->version) + " - return
    //                    back\n");
    //     }
    //     activeDevices++;
    // } else {
    fxdevs.push_back({dev->pid, dev->vid, dev, dev->description, dev->version});
    deviceListChanged = fxdevs.back().arrived = fxdevs.back().present = true;
    activeDevices++;
#ifdef DEBUG
    LOG_S(INFO) << "Scan: VID: 0x" << std::hex << std::setw(4)
                << std::setfill('0') << dev->vid << ", PID: 0x" << std::hex
                << std::setw(4) << std::setfill('0') << dev->pid
                << ", Version: " << to_string(dev->version)
                << " - new device added";
#endif
    //}
}

bool Mappings::AlienFXEnumDevices(void *acc) {
    LOG_S(INFO) << "Enumerating devices";
    Functions *dev = nullptr;
    deviceListChanged = false;

    // Reset active status
    for (auto i = fxdevs.begin(); i != fxdevs.end(); i++)
        i->present = false;
    activeDevices = activeLights = 0;

    libusb_device **device_list;
    ssize_t device_count = libusb_get_device_list(ctx, &device_list);

    if (device_count < 0) {
        LOG_S(ERROR) << "Failed to get device list: "
                     << libusb_error_name(device_count);
        return deviceListChanged;
    }

    // Iterate through all USB devices
    for (ssize_t i = 0; i < device_count; i++) {
        libusb_device *device = device_list[i];
        struct libusb_device_descriptor desc;

        // Get device descriptor
        int result = libusb_get_device_descriptor(device, &desc);
        if (result < 0) {
            continue; // Skip this device if we can't get descriptor
        }

        // Create new Functions object for each potential device
        dev = new Functions(ctx);

        // Try to probe/initialize this device
        if (dev->AlienFXProbeDevice(device, ctx, desc.idVendor,
                                    desc.idProduct)) {
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
    return deviceListChanged;
}
Afx_device *Mappings::GetDeviceById(unsigned short pid, unsigned short vid) {
    for (auto pos = fxdevs.begin(); pos < fxdevs.end(); pos++)
        if (pos->pid == pid && (!vid || pos->vid == vid)) {
            return &(*pos);
        }
    return nullptr;
}

Afx_device *Mappings::GetDeviceById(unsigned long devID) {
    for (auto pos = fxdevs.begin(); pos < fxdevs.end(); pos++)
        if (pos->devID == devID) {
            return &(*pos);
        }
    return nullptr;
}

Afx_grid *Mappings::GetGridByID(uint8_t id) {
    for (auto pos = grids.begin(); pos != grids.end(); pos++)
        if (pos->id == id)
            return &(*pos);
    return nullptr;
}

Afx_device *Mappings::AddDeviceById(unsigned long devID) {
    Afx_device *dev = GetDeviceById(devID);
    if (!dev) {
        fxdevs.push_back({LOWORD(devID), HIWORD(devID), NULL});
        dev = &fxdevs.back();
    }
    return dev;
}

Afx_light *Mappings::GetMappingByDev(Afx_device *dev, unsigned short LightID) {
    if (dev) {
        for (auto pos = dev->lights.begin(); pos != dev->lights.end(); pos++)
            if (pos->lightid == LightID)
                return &(*pos);
    }
    return nullptr;
}

vector<Afx_group> *Mappings::GetGroups() { return &groups; }

bool Mappings::SetDeviceBrightness(Afx_device *dev, std::uint8_t br,
                                   bool power) {
    if (dev->dev) {
        return dev->dev->SetBrightness(br, dev->brightness, &dev->lights,
                                       power);
    }
    return false;
}

void Mappings::RemoveMapping(Afx_device *dev, unsigned short lightID) {
    if (dev) {
        for (auto del_map = dev->lights.begin(); del_map != dev->lights.end();
             del_map++)
            if (del_map->lightid == lightID) {
                dev->lights.erase(del_map);
                return;
            }
    }
}

Afx_group *Mappings::GetGroupById(unsigned long gID) {
    for (auto pos = groups.begin(); pos != groups.end(); pos++)
        if (pos->gid == gID)
            return &(*pos);
    return nullptr;
}

void Mappings::LoadMappings() {
    // TODO: implement
    LOG_S(INFO) << "Load mappings";
    //   HKEY   mainKey;
    //
    // fxdevs.clear();
    // groups.clear();
    // grids.clear();
    //
    // RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Alienfx_SDK"), 0,
    // NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &mainKey, NULL);
    // unsigned vindex; char kName[255], name[255]; DWORD len, lend; int
    // dID; WORD vid, pid; byte lID; for (vindex = 0; RegEnumValue(mainKey,
    // vindex, kName,
    // &(len = 255), NULL, NULL, (LPBYTE)name, &(lend = 255)) ==
    // ERROR_SUCCESS; vindex++) { 	if (sscanf_s(kName, "Dev#%hd_%hd", &vid,
    // &pid) == 2) { 		AddDeviceById(MAKELPARAM(pid,
    // vid))->name = string(name); 		continue;
    // 	}
    // 	if (sscanf_s(kName, "DevWhite#%hd_%hd", &vid, &pid) == 2) {
    // 		AddDeviceById(MAKELPARAM(pid, vid))->white.ci =
    // ((DWORD*)name)[0]; 		continue;
    // 	}
    // 	if (sscanf_s(kName, "DevBright#%hd_%hd", &vid, &pid) == 2) {
    // 		AddDeviceById(MAKELPARAM(pid, vid))->brightness =
    // ((DWORD*)name)[0]; 		continue;
    // 	}
    // }
    // for (vindex = 0; RegEnumKey(mainKey, vindex, kName, 255) ==
    // ERROR_SUCCESS; vindex++) { 	if (sscanf_s(kName, "Light%d-%hhd",
    // &dID, &lID) == 2) { 		RegGetValue(mainKey, kName, "Name",
    // RRF_RT_REG_SZ, 0, name,
    // &(lend = 255)); 		RegGetValue(mainKey, kName, "Flags",
    // RRF_RT_REG_DWORD, 0, &len, &(lend = sizeof(DWORD)));
    // AddDeviceById(dID)->lights.push_back({ lID, { LOWORD(len),
    // HIWORD(len) }, name });
    // 	}
    // 	if (sscanf_s(kName, "Grid%d", &dID) == 1) {
    // 		RegGetValue(mainKey, kName, "Name", RRF_RT_REG_SZ, 0,
    // name,
    // &(lend = 255)); 		RegGetValue(mainKey, kName, "Size",
    // RRF_RT_REG_DWORD, 0, &len, &(lend = sizeof(DWORD)));
    // byte x = HIBYTE(len), y = LOBYTE(len); 		Afx_groupLight*
    // grid = new Afx_groupLight[x
    // * y]; 		RegGetValue(mainKey, kName, "Grid",
    // RRF_RT_REG_BINARY, 0, grid,
    // &(lend = x * y * sizeof(DWORD))); 		grids.push_back({
    // (byte)dID, x, y, name, grid });
    // 	}
    // }
    // for (vindex = 0; RegEnumKey(mainKey, vindex, kName, 255) ==
    // ERROR_SUCCESS; vindex++) { 	if (sscanf_s(kName, "Group%d", &dID) ==
    // 1) { 		RegGetValue(mainKey, kName, "Name",
    // RRF_RT_REG_SZ, 0, name,
    // &(lend = 255));
    // 		// -1 patch
    // 		if (dID < 0) {
    // 			dID = 0x1ffff;
    // 		}
    // 		groups.push_back({ (DWORD)dID, name });
    // 		vector<Afx_groupLight>* gl = &groups.back().lights;
    // 		if (RegGetValue(mainKey, kName, "LightList",
    // RRF_RT_REG_BINARY, 0, NULL, &lend) != ERROR_FILE_NOT_FOUND) {
    // gl->resize(lend / sizeof(DWORD));
    // RegGetValue(mainKey, kName, "LightList", RRF_RT_REG_BINARY, 0,
    // gl->data(), &lend);
    // 		}
    // 	}
    // }
    // RegCloseKey(mainKey);
}

void Mappings::SaveMappings() {
    // TODO: implement
    LOG_S(INFO) << "Save mappings";
    //
    // HKEY   hKeybase, hKeyStore;
    // size_t numGroups = groups.size();
    // size_t numGrids = grids.size();
    //
    // // Remove all maps!
    // RegDeleteTree(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Alienfx_SDK"));
    // RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Alienfx_SDK"), 0,
    // NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKeybase,
    // NULL);
    //
    // for (auto i = fxdevs.begin(); i != fxdevs.end(); i++) {
    // 	// Saving device data..
    // 	string devID = to_string(i->vid) + "_" + to_string(i->pid);
    // 	string name = "Dev#" + devID;
    // 	if (i->name.length())
    // 		RegSetValueEx(hKeybase, name.c_str(), 0, REG_SZ, (BYTE
    // *) i->name.c_str(), (DWORD) i->name.length() ); 	name =
    // "DevWhite#" + devID; 	RegSetValueEx(hKeybase, name.c_str(), 0,
    // REG_DWORD, (BYTE *) &i->white.ci, sizeof(DWORD)); 	name =
    // "DevBright#" + devID; DWORD br = i->brightness;
    // RegSetValueEx(hKeybase, name.c_str(), 0, REG_DWORD, (BYTE*)&br,
    // sizeof(DWORD)); 	for (auto cl = i->lights.begin(); cl <
    // i->lights.end(); cl++) {
    // 		// Saving all lights from current device
    // 		string name = "Light" + to_string(i->devID) + "-" +
    // to_string(cl->lightid); 		RegCreateKey(hKeybase,
    // name.c_str(), &hKeyStore); 		RegSetValueEx(hKeyStore, "Name",
    // 0, REG_SZ, (BYTE*)cl->name.c_str(), (DWORD)cl->name.length());
    // 		RegSetValueEx(hKeyStore, "Flags", 0, REG_DWORD,
    // (BYTE*)&cl->data, sizeof(DWORD)); 		RegCloseKey(hKeyStore);
    // 	}
    // }
    //
    // for (auto i = groups.begin(); i != groups.end(); i++) {
    // 	string name = "Group" + to_string(i->gid);
    //
    // 	RegCreateKey(hKeybase, name.c_str(), &hKeyStore);
    // 	RegSetValueEx(hKeyStore, "Name", 0, REG_SZ, (BYTE *)
    // i->name.c_str(), (DWORD) i->name.length()); 	RegSetValueEx(hKeyStore,
    // "LightList", 0, REG_BINARY, (BYTE*)i->lights.data(),
    // (DWORD)i->lights.size() * sizeof(DWORD)); 	RegCloseKey(hKeyStore);
    // }
    //
    // for (auto i = grids.begin(); i != grids.end(); i++) {
    // 	string name = "Grid" + to_string(i->id);
    // 	RegCreateKey(hKeybase, name.c_str(), &hKeyStore);
    // 	RegSetValueEx(hKeyStore, "Name", 0, REG_SZ,
    // (BYTE*)i->name.c_str(), (DWORD)i->name.length()); 	DWORD sizes =
    // ((DWORD)i->x << 8) | i->y; 	RegSetValueEx(hKeyStore, "Size", 0,
    // REG_DWORD, (BYTE*)&sizes, sizeof(DWORD)); 	RegSetValueEx(hKeyStore,
    // "Grid", 0, REG_BINARY, (BYTE*)i->grid, i->x * i->y * sizeof(DWORD));
    // RegCloseKey(hKeyStore);
    // }
    //
    // RegCloseKey(hKeybase);
}

Afx_light *Mappings::GetMappingByID(unsigned short pid, unsigned short lid) {
    Afx_device *dev = GetDeviceById(pid);
    return dev ? GetMappingByDev(dev, lid) : nullptr;
}

int Mappings::GetFlags(Afx_device *dev, unsigned short lightid) {
    Afx_light *lgh = GetMappingByDev(dev, lightid);
    return lgh ? lgh->flags : 0;
}

int Mappings::GetFlags(unsigned short pid, unsigned short lightid) {
    Afx_device *dev = GetDeviceById(pid);
    return dev ? GetFlags(dev, lightid) : 0;
}

bool Functions::IsHaveGlobal() {
    return version == API_V5 || version == API_V8;
}

} // namespace AlienFX_SDK
//
