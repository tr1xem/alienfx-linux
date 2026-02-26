#pragma once

#include <string_view>

#include "AlienFX_SDK.h"

struct ZONECODE {
    std::string_view name;
    unsigned code;
};

struct ACTIONCODE {
    std::string_view name;
    int afx_code;
};

#define ZONE_LEFT 0x00
#define ZONE_MIDDLE_LEFT 0x01
#define ZONE_MIDDLE_RIGHT 0x02
#define ZONE_RIGHT 0x03

inline constexpr ZONECODE zonecodes[]{
    {"left", ZONE_LEFT},
    {"middle_left", ZONE_MIDDLE_LEFT},
    {"right", ZONE_RIGHT},
    {"middle_right", ZONE_MIDDLE_RIGHT},
};

inline constexpr ACTIONCODE actioncodes[]{
    {"color",
     AlienFX_SDK::Action::AlienFX_A_Color},  // add "color" for disabling action
    {"pulse", AlienFX_SDK::Action::AlienFX_A_Pulse},
    {"morph", AlienFX_SDK::Action::AlienFX_A_Morph},
    {"breath", AlienFX_SDK::Action::AlienFX_A_Breathing},
    {"spectrum", AlienFX_SDK::Action::AlienFX_A_Spectrum},
    {"rainbow", AlienFX_SDK::Action::AlienFX_A_Rainbow},
};
