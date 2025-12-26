#include "AlienFX_SDK.h"
#include <iostream>

int main() {

    AlienFX_SDK::Mappings mappings;
    mappings.LoadMappings();
    mappings.AlienFXEnumDevices();
    // vector<AlienFX_SDK::Functions*> devs = mappings.AlienFXEnumDevices();

    // std::cout << "Hello World!" << std::endl;
    return 0;
}
