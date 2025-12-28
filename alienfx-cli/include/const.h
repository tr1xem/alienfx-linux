#pragma once
#include "AlienFX_SDK.h"
#include <cstdint>
#include <string>

struct ARG {
    std::string str;
    int num;
};

struct COMMAND {
    const int id;
    const char *name, *desc;
    uint8_t minArgs;
};

struct ZONECODE {
    const char *name;
    unsigned code;
};

struct ACTIONCODE {
    const char *name;
    int afx_code;
};

#define ZONE_LEFT 0x00
#define ZONE_MIDDLE_LEFT 0x01
#define ZONE_MIDDLE_RIGHT 0x02
#define ZONE_RIGHT 0x03

const ZONECODE zonecodes[]{
    {"left", ZONE_LEFT},
    {"middle_left", ZONE_MIDDLE_LEFT},
    {"right", ZONE_RIGHT},
    {"middle_right", ZONE_MIDDLE_RIGHT},

};

const ACTIONCODE actioncodes[]{
    {"pulse", AlienFX_SDK::Action::AlienFX_A_Pulse},
    {"morph", AlienFX_SDK::Action::AlienFX_A_Morph},
    {"breath", AlienFX_SDK::Action::AlienFX_A_Breathing},
    {"spectrum", AlienFX_SDK::Action::AlienFX_A_Spectrum},
    {"rainbow", AlienFX_SDK::Action::AlienFX_A_Rainbow}};

enum COMMANDS {
    setall = 0,
    setone = 1,
    setzone = 2,
    setaction = 3,
    setzoneact = 4,
    setpower = 5,
    settempo = 6,
    setdim = 8,
    setglobal = 9,
    lowlevel = 10,
    highlevel = 11,
    probe = 12,
    status = 13,
    loop = 14
};

const COMMAND commands[]{
    // {COMMANDS::setall, "setall", "\tr,g,b - set all lights", 3},
    {COMMANDS::setone, "setone", "\tdev,light,r,g,b - set one light", 5},
    {COMMANDS::setzone, "setzone", "\tzone,r,g,b - set zone lights", 4},
    {COMMANDS::setaction, "setaction",
     "dev,light,action,r,g,b[,action,r,g,b] - set light and enable it's action",
     6},
    {COMMANDS::setzoneact, "setzoneaction",
     "zone,action,r,g,b[,action,r,g,b] - set all zone lights and enable it's "
     "action",
     5},
    // {COMMANDS::setpower, "setpower",
    //  "dev,light,r,g,b,r2,g2,b2 - set power button colors (low-level only)",
    //  8},
    {COMMANDS::settempo, "settempo",
     "tempo[,length] - set tempo and effect length for actions", 1},
    {COMMANDS::setdim, "setdim", "\t[dev,]br - set brightness level", 1},
    {COMMANDS::setglobal, "setglobal",
     "dev,type,mode[,r,g,b[,r,g,b]] - set global effect (v5, v8 devices)", 3},
    {COMMANDS::lowlevel, "lowlevel", "switch to low-level SDK (USB)"},
    // {COMMANDS::highlevel, "highlevel", "switch to high-level SDK (LightFX)"},
    {COMMANDS::probe, "probe",
     "\t[l][d][,lights][,devID[,lightID]] - probe lights and set names"},
    {COMMANDS::status, "status",
     "\tshows devices, lights and zones id's and names"},
    {COMMANDS::loop, "loop",
     "\trepeat commands from start, until user press CTRL+c"}};
