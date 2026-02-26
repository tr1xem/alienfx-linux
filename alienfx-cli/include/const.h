#pragma once

#include <string_view>

#include "AlienFX_SDK.h"

struct ACTIONCODE {
    std::string_view name;
    int afx_code;
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
