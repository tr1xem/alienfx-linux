#include "AlienFX_SDK.h"
#include "loguru.hpp"
#include <iostream>

int main() {

    AlienFX_SDK::Mappings afx_map;
    afx_map.LoadMappings();
    afx_map.AlienFXEnumDevices();
    for (auto it = afx_map.fxdevs.begin(); it != afx_map.fxdevs.end(); it++)
        cout << "Stored device " << it->name << ", " << it->lights.size()
             << " lights\n";
    cout << afx_map.fxdevs.size() << " device(s) detected." << endl;
    // And try to set it lights...
    for (int i = 0; i < afx_map.fxdevs.size(); i++) {
        cout << hex << "VID: 0x" << afx_map.fxdevs[i].vid << ", PID: 0x"
             << afx_map.fxdevs[i].pid << ", API v" << afx_map.fxdevs[i].version
             << endl;
        if (afx_map.fxdevs[i].dev) {
            LOG_S(INFO) << "Now trying light 2 to blue... ";
            afx_map.fxdevs[i].dev->SetColor(2, {255, 255});
            afx_map.fxdevs[i].dev->UpdateColors();
            // 	cout << "Now trying light 3 to mixed... ";
            // 	afx_map.fxdevs[i].dev->SetColor(3, { 255, 255 });
            // 	afx_map.fxdevs[i].dev->UpdateColors();
            // 	cin.get();
        } else {
            cout << "Device inactive, going next one." << endl;
        }
    }

    return 0;
}
