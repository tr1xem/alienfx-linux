#include "AlienFX_SDK.h"
#include "loguru.hpp"
#include <iostream>

int main() {

    // Keyboard zones
    AlienFX_SDK::Mappings afx_map;
    afx_map.LoadMappings();
    afx_map.AlienFXEnumDevices();
    for (auto it = afx_map.fxdevs.begin(); it != afx_map.fxdevs.end(); it++)
        LOG_S(INFO) << "Stored device " << it->name << ", " << it->lights.size()
                    << " lights";
    LOG_S(INFO) << afx_map.fxdevs.size() << " device(s) detected.";

    // And try to set it lights...
    for (int i = 0; i < afx_map.fxdevs.size(); i++) {
        LOG_S(INFO) << hex << "VID: 0x" << afx_map.fxdevs[i].vid << ", PID: 0x"
                    << afx_map.fxdevs[i].pid << ", API v"
                    << afx_map.fxdevs[i].version;
        if (afx_map.fxdevs[i].dev != nullptr) {
            LOG_S(INFO) << "Testing device " << i << "... ";
            //
            // NOTE: SET KEYBOARD ZONE COLOR - Passed

            if (afx_map.fxdevs[i].vid == 0x187c) {
                std::vector<AlienFX_SDK::Afx_action> action_zone{};
                action_zone.push_back(
                    {AlienFX_SDK::AlienFX_A_Color, 2, 64, 0, 255, 255});
                std::vector<AlienFX_SDK::Afx_lightblock> lights;
                lights.push_back({0, action_zone});
                lights.push_back({1, action_zone});
                lights.push_back({2, action_zone});
                lights.push_back({3, action_zone});
                afx_map.fxdevs[i].dev->SetMultiAction(&lights, true);
                afx_map.fxdevs[i].dev->UpdateColors();
                LOG_S(WARNING)
                    << "Alienware device are now Cyan" << i << "... ";
            }
            if (afx_map.fxdevs[i].vid == 0x0d62) {
                std::vector<AlienFX_SDK::Afx_action> action_zone;
                action_zone.push_back(
                    {AlienFX_SDK::AlienFX_A_Morph, 2, 64, 0, 255, 255});

                std::vector<AlienFX_SDK::Afx_lightblock> lights;

                for (uint8_t idx = 0; idx <= 100; ++idx) {
                    lights.push_back({idx, action_zone});
                }

                afx_map.fxdevs[i].dev->SetMultiAction(&lights, true);
                afx_map.fxdevs[i].dev->UpdateColors();
                LOG_S(WARNING) << "Darfon device are now Cyan" << i << "... ";
            }

            // NOTE: Multicolor test - Passed
            //
            // std::vector<AlienFX_SDK::Afx_action> action_zone1{};
            //
            // action_zone1.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 255, 255});
            // action_zone1.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 0, 255, 255});
            // action_zone1.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 0, 0, 255});
            // action_zone1.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 0, 255});
            // action_zone1.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 0, 0});
            // action_zone1.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 255, 0});
            // action_zone1.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 0, 255, 0});
            //
            // std::vector<AlienFX_SDK::Afx_action> action_zone2{};
            // action_zone2.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 100, 255, 100});
            // action_zone2.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 0, 255, 190});
            // action_zone2.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 0, 160, 255});
            // action_zone2.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 120, 0, 255});
            // action_zone2.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 100, 0});
            // action_zone2.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 200, 0});
            // action_zone2.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 0, 255, 100});
            //
            // std::vector<AlienFX_SDK::Afx_action> action_zone3;
            //
            // action_zone3.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 128, 128});
            // action_zone3.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 128, 255, 128});
            // action_zone3.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 128, 128, 255});
            // action_zone3.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 0, 128});
            // action_zone3.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 0, 255, 128});
            // action_zone3.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 128, 255, 0});
            // action_zone3.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 0, 128, 255});
            //
            // std::vector<AlienFX_SDK::Afx_action> action_zone4;
            //
            // action_zone4.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 64, 64, 64});
            // action_zone4.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 128, 128, 128});
            // action_zone4.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 192, 192, 192});
            // action_zone4.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 128, 0});
            // action_zone4.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 128, 0, 255});
            // action_zone4.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 0, 255, 128});
            // action_zone4.push_back(
            //     {AlienFX_SDK::AlienFX_A_Rainbow, 2, 64, 255, 0, 64});
            //
            // std::vector<AlienFX_SDK::Afx_lightblock> lights;
            // lights.push_back({0, action_zone1});
            // lights.push_back({1, action_zone2});
            // lights.push_back({2, action_zone3});
            // lights.push_back({3, action_zone4});
            // afx_map.fxdevs[i].dev->SetMultiAction(&lights, true);
            // afx_map.fxdevs[i].dev->UpdateColors();

            // NOTE: Brightness Test - Passed
            //
            // AlienFX_SDK::Afx_light keyboard_zone1{.lightid = 0,
            //                                       .flags = 0x0000,
            //                                       .scancode = 0x0000,
            //                                       .name = "Keyboard Zone1"};
            //
            // AlienFX_SDK::Afx_light keyboard_zone2{.lightid = 1,
            //                                       .flags = 0x0000,
            //                                       .scancode = 0x0001,
            //                                       .name = "Keyboard Zone2"};
            //
            // AlienFX_SDK::Afx_light keyboard_zone3{.lightid = 2,
            //                                       .flags = 0x0000,
            //                                       .scancode = 0x0002,
            //                                       .name = "Keyboard Zone3"};
            //
            // AlienFX_SDK::Afx_light keyboard_zone4{.lightid = 3,
            //                                       .flags = 0x0000,
            //                                       .scancode = 0x0003,
            //                                       .name = "Keyboard Zone4"};
            //
            // afx_map.fxdevs[i].lights.push_back(keyboard_zone1);
            // afx_map.fxdevs[i].lights.push_back(keyboard_zone2);
            // afx_map.fxdevs[i].lights.push_back(keyboard_zone3);
            // afx_map.fxdevs[i].lights.push_back(keyboard_zone4);
            // afx_map.SetDeviceBrightness(&afx_map.fxdevs[i], 255, false);

            // NOTE: Multicolor test - Passed
            //
            // std::vector<unsigned char> lights{0, 1, 2, 3};
            // afx_map.fxdevs[i].dev->SetMultiColor(
            //     &lights, {AlienFX_SDK::AlienFX_A_Color, 5, 64, 0, 255, 255});

            // NOTE: Single Color Test - Passed
            //
            // afx_map.fxdevs[i].dev->SetColor(
            //     0, {AlienFX_SDK::AlienFX_A_Color, 5, 64, 255, 255, 255});
            // afx_map.fxdevs[i].dev->SetColor(
            //     1, {AlienFX_SDK::AlienFX_A_Color, 5, 64, 255, 255, 255});
            // afx_map.fxdevs[i].dev->SetColor(
            //     2, {AlienFX_SDK::AlienFX_A_Color, 5, 64, 255, 255, 255});
            // afx_map.fxdevs[i].dev->SetColor(
            //     3, {AlienFX_SDK::AlienFX_A_Color, 5, 64, 255, 255, 255});
            // afx_map.fxdevs[i].dev->UpdateColors();
            // cout << "Now trying light 3 to mixed... ";
            // afx_map.fxdevs[i].dev->SetColor(3, {255, 255});
            // afx_map.fxdevs[i].dev->UpdateColors();
            //  	cin.get();
        } else {
            cout << "Device inactive, going next one." << endl;
        }
    }

    return 0;
}
