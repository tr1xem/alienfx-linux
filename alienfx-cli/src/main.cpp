#include <stdio.h>

#include <cstring>
#include <iostream>
#include <map>

#include "AlienFan-SDK.h"
#include "const.h"
#include "loguru.hpp"

#define ARRAYSIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

using namespace std;
static AlienFX_SDK::Mappings afx_map;
static AlienFan_SDK::Control fan;
uint8_t globalBright = 255;
uint8_t sleepy = 5, longer = 5;
int devType = -1;

void printUsage() {
    printf(
        "Usage: alienfx-cli [command=option,option,option] ... "
        "[loop]\nCommands:\t\tOptions:\n");
    for (int i = 0; i < ARRAYSIZE(commands); i++)
        printf("%s\t\t%s.\n", commands[i].name, commands[i].desc);
    printf(
        "\nProbe argument (can be combined):\n\
\tl - Define number of lights\n\
\td - DeviceID and optionally LightID.\n\
Zones:\tleft, right, middle_right,middle_left or ID of the Group (low-level).\n\
Actions:color (disable action), pulse, morph,\n\
\tbreath, spectrum, rainbow (low-level only).\n\
\tUp to 9 colors can be entered.\n\
Profiles: custom, balanced, quiet, balanced-performance, performance, cool, low-power.\n\
Modes: 1 - global, 2 - keypress.\n");
}

unsigned GetZoneCode(ARG name) {
    if (devType)
        return name.num | 0x10000;
    else {
        for (int i = 0; i < ARRAYSIZE(zonecodes); i++)
            if (name.str == zonecodes[i].name) return zonecodes[i].code;
        return 0xff;
    }
}

int CheckCommand(string name, int args) {
    // find command id, return -1 if wrong.
    for (int i = 0; i < ARRAYSIZE(commands); i++) {
        if (name == commands[i].name) {
            if (commands[i].minArgs > args) {
                LOG_F(ERROR,
                      "%s: Incorrect arguments count (at least %d needed)",
                      commands[i].name, commands[i].minArgs);
                return -1;
            } else
                return commands[i].id;
        }
    }
    return -2;
}

void Update() {
    for (int i = 0; i < afx_map.fxdevs.size(); i++)
        afx_map.fxdevs[i].dev->UpdateColors();
}
unsigned int Act2UINT(AlienFX_SDK::Afx_action* act) {
    return AlienFX_SDK::Afx_colorcode({act->b, act->g, act->r, 255}).ci;
}

vector<AlienFX_SDK::Afx_action> ParseActions(vector<ARG>* args, int startPos) {
    vector<AlienFX_SDK::Afx_action> actions;
    for (int argPos = startPos; argPos + 2 < args->size(); argPos += 3) {
        uint8_t acttype = AlienFX_SDK::Action::AlienFX_A_Color;
        if (argPos + 3 < args->size()) {  // action
            for (int i = 0; i < ARRAYSIZE(actioncodes); i++)
                if (args->at(argPos).str == actioncodes[i].name) {
                    acttype = actioncodes[i].afx_code;
                    break;
                }
            argPos++;
        }
        actions.push_back({
            acttype,
            sleepy,
            longer,
            (uint8_t)(args->at(argPos).num),      // * globalBright) / 255), //r
            (uint8_t)(args->at(argPos + 1).num),  // * globalBright) / 255), //g
            (uint8_t)(args->at(argPos + 2).num),  // * globalBright) / 255) //b
        });
        // AlienFX_SDK::Afx_action* color = &actions.back();
        // color->r = ((unsigned)color->r * globalBright) / 255;// >> 8;
        // color->g = ((unsigned)color->g * globalBright) / 255;// >> 8;
        // color->b = ((unsigned)color->b * globalBright) / 255;// >> 8;
    }
    if (actions.size() < 2 &&
        actions.front().type != (AlienFX_SDK::Action::AlienFX_A_Color))
        actions.push_back(
            {actions.front().type, (uint8_t)sleepy, longer, 0, 0, 0});
    return actions;
}

int main(int argc, char* argv[]) {
    printf("alienfx-cli v%s\n", VERSION);
    if (argc < 2) {
        printUsage();
        return 0;
    }
    fan.Probe();

    afx_map.LoadMappings();
    afx_map.AlienFXEnumDevices();

    // LOG_F(INFO,"Dell API ");
    // if (have_high = (lfxUtil.InitLFX() == -1)) {
    //     LOG_F(INFO,"ready");
    //     devType = 0;
    // }
    // else
    // LOG_F(INFO,"not found");

    if (afx_map.activeDevices) {
        devType = 1;
        // NOTE: We doing this when we are on the light itself
        //  set brightness
        //  for (auto &dev : afx_map.fxdevs) {
        //      if (dev.present) {
        //          afx_map.SetDeviceBrightness(&dev, 255, true);
        //      }
        //  }
    }
    LOG_F(INFO, "%d low-level devices found.", afx_map.activeDevices);

    // if (devType >= 0) {
    for (int cc = 1; cc < argc; cc++) {
        string arg = string(argv[cc]);
        size_t vid = arg.find_first_of('=');
        string command = arg.substr(0, vid);
        vector<ARG> args;

        while (vid != string::npos) {
            size_t argPos = arg.find(',', vid + 1);
            args.push_back({arg.substr(
                vid + 1,
                (argPos == string::npos ? arg.length() : argPos) - vid - 1)});
            args.back().num = atoi(args.back().str.c_str());
            vid = argPos;
        }

        switch (CheckCommand(command, (int)args.size())) {
            // case COMMANDS::setall:  {
            case COMMANDS::setall: {
                auto acts = ParseActions(&args, 0);
                if (devType) {
                    for (auto cd = afx_map.fxdevs.begin();
                         cd != afx_map.fxdevs.end(); cd++) {
                        vector<uint8_t> lights;
                        for (auto i = cd->lights.begin(); i != cd->lights.end();
                             i++) {
                            if (!(i->flags & ALIENFX_FLAG_POWER)) {
                                lights.push_back((uint8_t)i->lightid);
                            }
                        }
                        cd->dev->SetMultiColor(&lights, acts.front());
                    }
                    // } else {
                    //     lfxUtil.SetLFXColor(LFX_ALL, Act2UINT(&acts.
                    //     front()));
                    // }
                    Update();
                }
                break;
            }

            case COMMANDS::setone: {
                auto acts = ParseActions(&args, 2);
                // if (devType) {
                if (args[0].num < afx_map.fxdevs.size()) {
                    AlienFX_SDK::Afx_lightblock act{(uint8_t)args[1].num, acts};
                    afx_map.fxdevs[args[0].num].dev->SetAction(&act);
                }
                // } else {
                //     lfxUtil.SetOneLFXColor(args[0].num, args[1].num,
                //                            &Act2Lfx(&acts.front()));
                // }
                Update();
                break;
            }

            case COMMANDS::setzone: {
                unsigned zoneCode = GetZoneCode(args[0]);
                // if (devType) {
                AlienFX_SDK::Afx_group* grp = afx_map.GetGroupById(zoneCode);
                if (grp) {
                    for (auto j = afx_map.fxdevs.begin();
                         j != afx_map.fxdevs.end(); j++) {
                        vector<unsigned char> lights;
                        for (auto i = grp->lights.begin();
                             i != grp->lights.end(); i++) {
                            if (i->did == j->pid) {
                                lights.push_back((uint8_t)i->lid);
                            }
                        }
                        j->dev->SetMultiColor(&lights,
                                              ParseActions(&args, 1).front());
                    }
                }
                // } else {
                //     lfxUtil.SetLFXColor(zoneCode,
                //     Act2UINT(&ParseActions(&args, 1).front()));
                // }
                Update();
                break;
            }

            case COMMANDS::setaction: {
                AlienFX_SDK::Afx_lightblock act{(uint8_t)args[1].num,
                                                ParseActions(&args, 2)};
                // if (devType) {
                if (args[0].num < afx_map.fxdevs.size()) {
                    afx_map.fxdevs[args[0].num].dev->SetAction(&act);
                }
                // } else {
                //     lfxUtil.SetLFXAction(act. act. front().type, args[0].num,
                //                          act.index,
                //                          &Act2Lfx(&act.act.front()),
                //                          &Act2Lfx(&act. act.back()));
                // }
                // delete act;
                Update();
                break;
            }

            case COMMANDS::setzoneact: {
                unsigned zoneCode = GetZoneCode(args[0]);
                AlienFX_SDK::Afx_lightblock act = {0, ParseActions(&args, 1)};
                // if (devType) {
                AlienFX_SDK::Afx_group* grp = afx_map.GetGroupById(zoneCode);
                if (grp) {
                    AlienFX_SDK::Afx_device* dev;
                    for (auto i = grp->lights.begin(); i != grp->lights.end();
                         i++) {
                        if ((dev = afx_map.GetDeviceById(i->did))) {
                            act.index = (uint8_t)i->lid;
                            dev->dev->SetAction(&act);
                        }
                    }
                }
                // } else {
                //     lfxUtil.SetLFXZoneAction(act.act.front().type, zoneCode,
                //                              Act2UINT(&act.act. front()),
                //                              Act2UINT(&act.act. back()));
                // }
                // delete act;
                Update();
                break;
            }

                // case COMMANDS::settempo:
                //     sleepy = args[0].num;
                //     if (args. size() > 1)
                //         longer = args[1].num;
                //     if (! devType) {
                //         lfxUtil.SetTempo(sleepy);
                //         lfxUtil.Update();
                //     }
                //     sleepy /= 50;
                //     break;

            case COMMANDS::setdim: {
                // set-dim
                if (args.size() > 1 && args[0].num < afx_map.fxdevs.size()) {
                    afx_map.fxdevs[args.front().num].brightness =
                        args.back().num;
                    afx_map.SetDeviceBrightness(
                        &afx_map.fxdevs[args.front().num], globalBright, false);
                }
                // } else {
                //     globalBright = args. front().num;
                // }
                break;
            }

            case COMMANDS::setglobal: {
                // set-global
                if (args[0].num < afx_map.fxdevs.size()) {
                    uint8_t cmode = args.size() < 6   ? 3
                                    : args.size() < 9 ? 1
                                                      : 2;
                    args.resize(9);
                    afx_map.fxdevs[args[0].num].dev->SetGlobalEffects(
                        args[1].num, args[2].num, cmode, sleepy,
                        {(uint8_t)args[3].num, (uint8_t)args[4].num,
                         (uint8_t)args[5].num},
                        {(uint8_t)args[6].num, (uint8_t)args[7].num,
                         (uint8_t)args[8].num});
                }
                break;
            }

            case COMMANDS::lowlevel: {
                // low-level
                if (afx_map.fxdevs.size()) {
                    devType = 1;
                    LOG_F(INFO, "USB Device selected");
                }
                break;
            }

                // case COMMANDS::highlevel:
                //     // high-level
                //     if (have_high) {
                //         devType = 0;
                //         LOG_F(INFO,"Dell API selected\n");
                //     }
                //     break;

            case COMMANDS::status: {
                // status
                // if (devType) {
                for (auto i = afx_map.fxdevs.begin(); i < afx_map.fxdevs.end();
                     i++) {
                    LOG_F(INFO,
                          "Device #%d - %s, VID#%d, PID#%d, APIv%d, %d "
                          "lights%s",
                          (int)(i - afx_map.fxdevs.begin()), i->name.c_str(),
                          i->vid, i->pid, i->version, (int)i->lights.size(),
                          i->present ? "" : " (inactive)");

                    for (int k = 0; k < i->lights.size(); k++) {
                        LOG_F(INFO, "  Light ID#%d - %s%s%s\n",
                              i->lights[k].lightid, i->lights[k].name.c_str(),
                              (i->lights[k].flags & ALIENFX_FLAG_POWER)
                                  ? " (Power button)"
                                  : "",
                              (i->lights[k].flags & ALIENFX_FLAG_INDICATOR)
                                  ? " (Indicator)"
                                  : "");
                    }
                }
                // }
                // now groups...
                if (afx_map.GetGroups()->size()) {
                    LOG_F(INFO, "%d zones:\n",
                          (int)afx_map.GetGroups()->size());
                    for (int i = 0; i < afx_map.GetGroups()->size(); i++) {
                        LOG_F(INFO, "  Zone #%d (%d lights) - %s",
                              (afx_map.GetGroups()->at(i).gid & 0xffff),
                              (int)afx_map.GetGroups()->at(i).lights.size(),
                              afx_map.GetGroups()->at(i).name.c_str());
                    }
                }
                // } else {
                //     lfxUtil.GetStatus();
                // }
                break;
            }

            case COMMANDS::probe: {
                // probe
                int numlights = -1, devID = -1, lightID = -1;
                if (args.size()) {
                    int pos = 1;
                    if (args[0].str.find('l') != string::npos &&
                        args.size() > 1) {
                        numlights = args[1].num;
                        pos++;
                    }
                    if (args[0].str.find('d') != string::npos &&
                        args.size() > pos) {
                        devID = args[pos].num;
                        if (args.size() > pos + 1) {
                            lightID = args[pos + 1].num;
                        }
                    }
                }

                for (auto i = afx_map.fxdevs.begin(); i != afx_map.fxdevs.end();
                     i++) {
                    printf(
                        "===== Device VID_%04x, PID_%04x =====\n"
                        "+++++ Detected as:  %s, APIv%d%s +++++\n",
                        i->vid, i->pid, i->dev->description.c_str(), i->version,
                        i->present ? "" : ", Inactive");
                }

                printf("Do you want to set devices and lights names? ");
                char name[256];
                fgets(name, sizeof(name), stdin);

                if (name[0] == 'y' || name[0] == 'Y') {
                    // printf("\nFor each light please enter LightFX SDK light
                    // ID or light name if ID is not available\n"
                    //        "Tested light become green, and turned off after
                    //        testing.\n" "Just press Enter if no visible light
                    //        at this ID to skip it.\n");

                    if (afx_map.fxdevs.size() > 0) {
                        printf("Found %d device(s)",
                               (int)afx_map.fxdevs.size());
                        // if (have_high) {
                        //     lfxUtil.FillInfo();
                        //     //lfxUtil.GetStatus();
                        // }

                        for (auto cDev = afx_map.fxdevs.begin();
                             cDev != afx_map.fxdevs.end(); cDev++) {
                            bool isLastDevice =
                                (std::next(cDev) == afx_map.fxdevs.end());
                            printf("Probing device VID_%04x, PID_%04x",
                                   cDev->vid, cDev->pid);
                            printf(", current name %s", cDev->name.c_str());
                            printf(", New name (ENTER to skip): ");
                            fgets(name, sizeof(name), stdin);
                            name[strcspn(name, "\r\n")] = 0;

                            if (name[0]) {
                                // cDev->name = isdigit(name[0]) && have_high ?
                                //              lfxUtil.GetDevInfo(atoi(name))->desc
                                //              :  name;
                                cDev->name = name;
                            }
                            printf("New name %s\n", cDev->name.c_str());

                            // How many lights to check?
                            int fnumlights =
                                numlights == -1
                                    ? (cDev->vid == 0x0d62 ? 0x88 : 23)
                                    : numlights;

                            AlienFX_SDK::Afx_lightblock lon{0, {{0}}};
                            for (uint8_t i = 0; i < fnumlights; i++) {
                                if (lightID == -1 || lightID == i) {
                                    printf("Testing light #%d", i);
                                    lon.index = i;
                                    lon.act.front().g = 255;

                                    AlienFX_SDK::Afx_light* lmap =
                                        afx_map.GetMappingByDev(&(*cDev), i);
                                    if (lmap) {
                                        printf(", current name %s",
                                               lmap->name.c_str());
                                    }

                                    std::vector<AlienFX_SDK::Afx_light> light;
                                    light.push_back(
                                        {i, 0x0000, 0x0000, "test"});

                                    printf(", New name (ENTER to skip): ");
                                    cDev->dev->SetBrightness(255, globalBright,
                                                             &light, false);
                                    cDev->dev->SetAction(&lon);
                                    cDev->dev->UpdateColors();
                                    fgets(name, sizeof(name), stdin);
                                    name[strcspn(name, "\r\n")] = 0;

                                    if (name[0]) {
                                        if (lmap) {
                                            lmap->name = name;
                                        } else {
                                            cDev->lights.push_back(
                                                {i, {0, 0}, name});
                                        }
                                        afx_map.SaveMappings();
                                        printf("New name is %s, ", name);
                                    } else {
                                        printf("Skipped, ");
                                    }

                                    lon.act.front().g = 0;
                                    cDev->dev->SetAction(&lon);
                                    cDev->dev->UpdateColors();
                                    afx_map.SaveMappings();
                                }
                            }
                        }
                    }
                }
                break;
            }
            case COMMANDS::getpowerprofile: {
                // getpowerprofile
                string current = fan.GetPowerProfile().name;
                cout << "Current power profile: " << current << std::endl;
                break;
            }

            case COMMANDS::supportedpowerprofiles: {
                // supportedpowerprofiles
                cout << "Supported power profiles: " << std::endl;
                for (auto i = fan.profiles.begin(); i != fan.profiles.end();
                     i++) {
                    cout << i->name << std::endl;
                }
                break;
            }

            case COMMANDS::setpowerprofile: {
                if (!args.empty()) {  // instead of args.size() > 1
                    std::map<std::string, AlienFan_SDK::ALIENFAN_PROFILE>
                        strToProfile = {
                            {"custom", AlienFan_SDK::CUSTOM},
                            {"balanced", AlienFan_SDK::BALANCED},
                            {"quiet", AlienFan_SDK::QUIET},
                            {"balanced-performance",
                             AlienFan_SDK::BALANCED_PERFORMANCE},
                            {"performance", AlienFan_SDK::PERFORMANCE},
                            {"cool", AlienFan_SDK::COOL},
                            {"low-power", AlienFan_SDK::LOW_POWER}};

                    std::string argProfile = args[0].str;
                    auto it = strToProfile.find(argProfile);
                    if (it != strToProfile.end()) {
                        if (fan.SetPowerProfile(it->second)) {
                            std::cout << "Power profile set to " << argProfile
                                      << std::endl;
                        } else {
                            std::cout << "Failed to set power profile to "
                                      << argProfile << std::endl;
                        }
                    } else {
                        std::cout << "Unknown profile: " << argProfile
                                  << std::endl;
                    }
                }
                break;
            }

            case COMMANDS::loop: {
                cc = 1;
                break;
            }

            case -2: {
                LOG_F(ERROR, "Unknown command %s", command.c_str());
                break;
            }
        }
    }
#ifdef DEBUG
    LOG_F(INFO, "Done.");
#endif
    // } else {
    //     LOG_F(ERROR, "Light devices not found, exiting!");
    // }

    // if (have_high)
    //     lfxUtil.Release();

    return 0;
}
