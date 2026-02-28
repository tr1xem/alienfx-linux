#include <CLI/CLI.hpp>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <loguru.hpp>
#include <map>
#include <string>
#include <vector>

#include "AlienFan-SDK.h"
#include "const.h"

using namespace std;

static AlienFX_SDK::Mappings afx_map;
static AlienFan_SDK::Control fan;

static uint8_t globalBright = 255;
static uint8_t sleepy = 5, longer = 5;
static int devType = -1;

static void Update() {
    for (auto& d : afx_map.fxdevs) {
        if (d.dev) d.dev->UpdateColors();
    }
}

static void initCli() {
    fan.Probe();
    afx_map.LoadMappings();
    afx_map.AlienFXEnumDevices();

    if (afx_map.activeDevices) {
        devType = 1;
        for (auto& dev : afx_map.fxdevs) {
            if (dev.present) {
                afx_map.SetDeviceBrightness(&dev, 255, true);
            }
        }
    }
    LOG_F(INFO, "%d low-level devices found.", afx_map.activeDevices);
}

static unsigned GetZoneCodeFromString(const std::string& zone) {
    if (auto* groups = afx_map.GetGroups()) {
        for (const auto& g : *groups) {
            if (g.name == zone) {
                return static_cast<unsigned>(g.gid);
            }
        }
    }
    try {
        unsigned v = static_cast<unsigned>(std::stoul(zone, nullptr, 0));
        // if (v <= 0xFFFF) v |= 0x10000;  // keep old behavior for short ids
        return v;
    } catch (...) {
        return 0xff;
    }
}
static uint8_t ActionFromString(const std::string& name) {
    for (const auto& a : actioncodes) {
        if (name == a.name) return static_cast<uint8_t>(a.afx_code);
    }
    LOG_F(ERROR, "Unknown action: %s", name.c_str());
    std::exit(1);
}

static AlienFX_SDK::Afx_action MakeColorAction(int r, int g, int b,
                                               uint8_t type) {
    return AlienFX_SDK::Afx_action{
        type,
        sleepy,
        longer,
        static_cast<uint8_t>(r),
        static_cast<uint8_t>(g),
        static_cast<uint8_t>(b),
    };
}

// Parse action list like: action r g b [action r g b] ...
static vector<AlienFX_SDK::Afx_action> ParseActionList(
    const vector<string>& tokens) {
    vector<AlienFX_SDK::Afx_action> actions;

    size_t i = 0;
    while (i < tokens.size()) {
        // expect action
        uint8_t acttype = AlienFX_SDK::Action::AlienFX_A_Color;
        string actName = tokens[i];

        acttype = ActionFromString(actName);
        i++;

        if (i + 2 >= tokens.size()) {
            LOG_F(ERROR, "Action requires r g b");
            std::exit(1);
        }
        int r = stoi(tokens[i++]);
        int g = stoi(tokens[i++]);
        int b = stoi(tokens[i++]);

        actions.push_back(MakeColorAction(r, g, b, acttype));
    }

    // old behavior: if action isn't "color" and only 1 action, add secondary
    // black action
    if (actions.size() < 2 &&
        actions.front().type != (AlienFX_SDK::Action::AlienFX_A_Color)) {
        actions.push_back({actions.front().type, sleepy, longer, 0, 0, 0});
    }

    return actions;
}

static string ReadLineTrimmed() {
    string s;
    getline(cin, s);
    // trim whitespace
    auto is_ws = [](unsigned char c) { return std::isspace(c); };
    while (!s.empty() && is_ws((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && is_ws((unsigned char)s.back())) s.pop_back();
    return s;
}

int main(int argc, char** argv) {
    // loguru::init(argc, argv);

    CLI::App app{"alienfx_cli"};
    app.set_version_flag("--version", string("alienfx-cli v") + VERSION);

    bool needInit = true;
    auto ensureInit = [&]() {
        if (needInit) {
            initCli();
            needInit = false;
        }
    };
    app.add_option("--brightness", globalBright, "Global brightness 0-255")
        ->default_val(255);
    app.add_option("--tempo", sleepy, "Tempo for actions")->default_val(5);
    app.add_option("--length", longer, "Length for actions")->default_val(5);

    // setall r g b
    auto* cmd_setall = app.add_subcommand("setall", "r g b - set all lights");
    int r = 0, g = 0, b = 0;
    cmd_setall->add_option("r", r)->required()->check(CLI::Range(0, 255));
    cmd_setall->add_option("g", g)->required()->check(CLI::Range(0, 255));
    cmd_setall->add_option("b", b)->required()->check(CLI::Range(0, 255));
    cmd_setall->callback([&]() {
        ensureInit();
        auto act =
            MakeColorAction(r, g, b, AlienFX_SDK::Action::AlienFX_A_Color);

        for (auto& dev : afx_map.fxdevs) {
            if (!dev.dev || !dev.present) continue;
            vector<uint8_t> lights;
            for (auto& l : dev.lights) {
                if (!(l.flags & ALIENFX_FLAG_POWER)) {
                    lights.push_back((uint8_t)l.lightid);
                }
            }
            dev.dev->SetMultiColor(&lights, act);
        }
        Update();
    });

    // setone dev light r g b
    auto* cmd_setone =
        app.add_subcommand("setone", "dev light r g b - set one light");
    int devIndex = 0, lightId = 0;
    int r2 = 0, g2 = 0, b2 = 0;
    cmd_setone->add_option("dev", devIndex)
        ->required()
        ->check(CLI::NonNegativeNumber);
    cmd_setone->add_option("light", lightId)
        ->required()
        ->check(CLI::Range(0, 255));
    cmd_setone->add_option("r", r2)->required()->check(CLI::Range(0, 255));
    cmd_setone->add_option("g", g2)->required()->check(CLI::Range(0, 255));
    cmd_setone->add_option("b", b2)->required()->check(CLI::Range(0, 255));
    cmd_setone->callback([&]() {
        ensureInit();
        if ((size_t)devIndex >= afx_map.fxdevs.size())
            throw CLI::ValidationError("dev", "Device index out of range");

        auto& dev = afx_map.fxdevs[(size_t)devIndex];
        if (!dev.dev) throw std::runtime_error("Device not initialized");

        AlienFX_SDK::Afx_lightblock block{
            (uint8_t)lightId,
            {MakeColorAction(r2, g2, b2,
                             AlienFX_SDK::Action::AlienFX_A_Color)}};
        dev.dev->SetAction(&block);
        dev.dev->UpdateColors();
    });

    // setzone zone r g b
    auto* cmd_setzone =
        app.add_subcommand("setzone", "zone r g b - set zone lights");
    string zone;
    int zr = 0, zg = 0, zb = 0;
    cmd_setzone->add_option("zone", zone)->required();
    cmd_setzone->add_option("r", zr)->required()->check(CLI::Range(0, 255));
    cmd_setzone->add_option("g", zg)->required()->check(CLI::Range(0, 255));
    cmd_setzone->add_option("b", zb)->required()->check(CLI::Range(0, 255));
    cmd_setzone->callback([&]() {
        ensureInit();
        unsigned zoneCode = GetZoneCodeFromString(zone);
        auto* grp = afx_map.GetGroupById(zoneCode);
        if (!grp) {
            LOG_F(ERROR, "Zone/group not found: %s", zone.c_str());
            return;
        }

        auto act =
            MakeColorAction(zr, zg, zb, AlienFX_SDK::Action::AlienFX_A_Color);
        for (auto& dev : afx_map.fxdevs) {
            if (!dev.dev || !dev.present) continue;
            vector<unsigned char> lights;
            for (auto& gl : grp->lights) {
                if (gl.did == dev.pid) lights.push_back((uint8_t)gl.lid);
            }
            dev.dev->SetMultiColor(&lights, act);
        }
        Update();
    });

    // setaction dev light (action r g b)+
    auto* cmd_setaction = app.add_subcommand(
        "setaction",
        "dev light action r g b [action r g b] - set light and enable action");
    int sa_dev = 0, sa_light = 0;
    vector<string> sa_tokens;
    cmd_setaction->add_option("dev", sa_dev)
        ->required()
        ->check(CLI::NonNegativeNumber);
    cmd_setaction->add_option("light", sa_light)
        ->required()
        ->check(CLI::Range(0, 255));
    cmd_setaction->add_option("actions", sa_tokens)->required()->expected(-1);
    cmd_setaction->callback([&]() {
        ensureInit();
        if ((size_t)sa_dev >= afx_map.fxdevs.size())
            throw CLI::ValidationError("dev", "Device index out of range");

        auto actions = ParseActionList(sa_tokens);
        AlienFX_SDK::Afx_lightblock block{(uint8_t)sa_light, actions};
        afx_map.fxdevs[(size_t)sa_dev].dev->SetAction(&block);
        Update();
    });

    // setzoneaction zone (action r g b)+
    auto* cmd_setzoneact =
        app.add_subcommand("setzoneaction",
                           "zone action r g b [action r g b] - set all zone "
                           "lights and enable action");
    string sza_zone;
    vector<string> sza_tokens;
    cmd_setzoneact->add_option("zone", sza_zone)->required();
    cmd_setzoneact->add_option("actions", sza_tokens)->required()->expected(-1);
    cmd_setzoneact->callback([&]() {
        ensureInit();
        unsigned zoneCode = GetZoneCodeFromString(sza_zone);
        auto* grp = afx_map.GetGroupById(zoneCode);
        // if (!grp) throw std::runtime_error("Zone/group not found");
        if (!grp) {
            LOG_F(ERROR, "Zone/group not found: %s", sza_zone.c_str());
            std::exit(1);
        }

        auto actions = ParseActionList(sza_tokens);
        AlienFX_SDK::Afx_lightblock block{0, actions};

        for (auto& gl : grp->lights) {
            auto* dev = afx_map.GetDeviceById(gl.did);
            if (!dev || !dev->dev) continue;
            block.index = (uint8_t)gl.lid;
            dev->dev->SetAction(&block);
        }
        Update();
    });

    // setdim [dev] br  (support both: "setdim br" and "setdim dev br")
    auto* cmd_setdim =
        app.add_subcommand("setdim", "[dev] br - set brightness");
    vector<int> dim_args;
    cmd_setdim->add_option("args", dim_args)->required()->expected(1, 2);
    cmd_setdim->callback([&]() {
        ensureInit();
        int br = 0;
        if (dim_args.size() == 1) {
            br = dim_args[0];
            // all devices
            for (auto& dev : afx_map.fxdevs) {
                dev.brightness = (uint8_t)br;
                afx_map.SetDeviceBrightness(&dev, globalBright, false);
            }
        } else {
            int d = dim_args[0];
            br = dim_args[1];
            if (d < 0 || (size_t)d >= afx_map.fxdevs.size())
                throw CLI::ValidationError("dev", "Device index out of range");
            auto& dev = afx_map.fxdevs[(size_t)d];
            dev.brightness = (uint8_t)br;
            afx_map.SetDeviceBrightness(&dev, globalBright, false);
        }
    });

    // setglobal dev type mode [r g b [r g b]]
    auto* cmd_setglobal =
        app.add_subcommand("setglobal", "dev type mode [r g b [r g b]]");
    vector<int> gargs;
    cmd_setglobal->add_option("args", gargs)->required()->expected(3, 9);
    cmd_setglobal->callback([&]() {
        ensureInit();
        int dev = gargs[0];
        if (dev < 0 || (size_t)dev >= afx_map.fxdevs.size())
            throw CLI::ValidationError("dev", "Device index out of range");

        uint8_t cmode = gargs.size() < 6 ? 3 : (gargs.size() < 9 ? 1 : 2);
        while (gargs.size() < 9) gargs.push_back(0);

        afx_map.fxdevs[(size_t)dev].dev->SetGlobalEffects(
            (uint8_t)gargs[1], (uint8_t)gargs[2], cmode, sleepy,
            {(uint8_t)gargs[3], (uint8_t)gargs[4], (uint8_t)gargs[5]},
            {(uint8_t)gargs[6], (uint8_t)gargs[7], (uint8_t)gargs[8]});
    });

    // status
    auto* cmd_status =
        app.add_subcommand("status", "Show devices, lights and zones");
    cmd_status->callback([&]() {
        ensureInit();
        for (auto it = afx_map.fxdevs.begin(); it < afx_map.fxdevs.end();
             it++) {
            cout << "Device #" << (it - afx_map.fxdevs.begin()) << " - "
                 << it->name << ", VID#0x" << std::hex << it->vid << std::dec
                 << ", PID#0x" << std::hex << it->pid << std::dec << ", APIv"
                 << (int)it->version << ", " << it->lights.size() << " lights"
                 << (it->present ? "" : " (inactive)") << "\n";
            for (const auto& l : it->lights) {
                cout << "  Light ID#" << (int)l.lightid << " - " << l.name
                     << ((l.flags & ALIENFX_FLAG_POWER) ? " (Power button)"
                                                        : "")
                     << ((l.flags & ALIENFX_FLAG_INDICATOR) ? " (Indicator)"
                                                            : "")
                     << "\n";
            }
        }

        if (!afx_map.GetGroups()->empty()) {
            cout << afx_map.GetGroups()->size() << " zones:\n";
            for (size_t i = 0; i < afx_map.GetGroups()->size(); i++) {
                const auto& g = afx_map.GetGroups()->at(i);
                cout << "  Zone #" << (g.gid) << " (" << g.lights.size()
                     << " lights) - " << g.name << "\n";
            }
        }
    });

    // probe
    auto* cmd_probe =
        app.add_subcommand("probe", "Probe lights and set names (interactive)");
    int probe_lights = -1;
    int probe_dev = -1;
    int probe_light = -1;
    cmd_probe->add_option("--lights", probe_lights, "How many lights to test");
    cmd_probe->add_option("--dev", probe_dev,
                          "Only probe a specific device index");
    cmd_probe->add_option("--light", probe_light,
                          "Only probe a specific light id");
    cmd_probe->callback([&]() {
        ensureInit();

        for (auto& d : afx_map.fxdevs) {
            cout << "===== Device VID 0x" << std::hex << d.vid << ", PID 0x"
                 << d.pid << std::dec << " =====\n"
                 << "+++++ Detected as: " << d.dev->description << ", APIv"
                 << (int)d.version << (d.present ? "" : ", Inactive")
                 << " +++++\n";
        }

        cout << "Do you want to set devices and lights names? (y/N) ";
        auto ans = ReadLineTrimmed();
        if (ans.empty() || (ans[0] != 'y' && ans[0] != 'Y')) return;

        for (size_t di = 0; di < afx_map.fxdevs.size(); di++) {
            if (probe_dev != -1 && (int)di != probe_dev) continue;
            auto& cDev = afx_map.fxdevs[di];

            cout << "Probing device VID 0x" << std::hex << cDev.vid
                 << ", PID 0x" << cDev.pid << std::dec << ", current name "
                 << cDev.name << ", New name (ENTER to skip): ";
            auto newDevName = ReadLineTrimmed();
            if (!newDevName.empty()) cDev.name = newDevName;

            int fnumlights = (probe_lights != -1)
                                 ? probe_lights
                                 : (cDev.vid == 0x0d62 ? 0x88 : 23);

            AlienFX_SDK::Afx_lightblock lon{0, {{0}}};

            for (int li = 0; li < fnumlights; li++) {
                if (probe_light != -1 && li != probe_light) continue;

                cout << "Testing light #" << li;
                lon.index = (uint8_t)li;
                lon.act.front().g = 255;

                auto* lmap = afx_map.GetMappingByDev(&cDev, (unsigned short)li);
                if (lmap) cout << ", current name " << lmap->name;

                cout << ", New name (ENTER to skip): ";

                std::vector<AlienFX_SDK::Afx_light> light;
                light.push_back({(uint8_t)li, 0x0000, 0x0000, "test"});
                cDev.dev->SetBrightness(255, globalBright, &light, false);
                cDev.dev->SetAction(&lon);
                cDev.dev->UpdateColors();

                auto newLightName = ReadLineTrimmed();
                if (!newLightName.empty()) {
                    if (lmap)
                        lmap->name = newLightName;
                    else
                        cDev.lights.push_back(
                            {(uint8_t)li, {0, 0}, newLightName});
                    afx_map.SaveMappings();
                    cout << "Saved.\n";
                } else {
                    cout << "Skipped.\n";
                }

                lon.act.front().g = 0;
                cDev.dev->SetAction(&lon);
                cDev.dev->UpdateColors();
            }
            afx_map.SaveMappings();
        }
    });

    // createlightzone
    auto* cmd_createlightzone = app.add_subcommand(
        "createlightzone", "Interactively create a light zone/group");
    cmd_createlightzone->callback([&]() {
        ensureInit();

        cout << "Zone name: ";
        string zoneName = ReadLineTrimmed();
        if (zoneName.empty()) {
            LOG_F(ERROR, "Zone name cannot be empty");
            std::exit(1);
        }

        // Check if zone name already exists
        auto* groups = afx_map.GetGroups();
        for (const auto& g : *groups) {
            if (g.name == zoneName) {
                cout << "Zone '" << zoneName
                     << "' already exists. Overwrite? (y/N) ";
                auto ans = ReadLineTrimmed();
                if (ans.empty() || (ans[0] != 'y' && ans[0] != 'Y')) {
                    cout << "Aborted.\n";
                    return;
                }
                // Remove existing zone
                for (auto it = groups->begin(); it != groups->end(); ++it) {
                    if (it->name == zoneName) {
                        groups->erase(it);
                        break;
                    }
                }
                break;
            }
        }

        // Ask for device index once
        cout << "Device index (from status): ";
        string devIdxStr = ReadLineTrimmed();
        if (devIdxStr.empty()) {
            LOG_F(ERROR, "Device index required");
            std::exit(1);
        }
        int devIndex = 0;
        try {
            devIndex = std::stoi(devIdxStr, nullptr, 0);
        } catch (...) {
            LOG_F(ERROR, "Invalid device index");
            std::exit(1);
        }

        if (devIndex < 0 || (size_t)devIndex >= afx_map.fxdevs.size()) {
            LOG_F(ERROR, "Device index out of range");
            std::exit(1);
        }

        auto& dev = afx_map.fxdevs[(size_t)devIndex];
        if (!dev.dev) {
            LOG_F(ERROR, "Device not initialized");
            std::exit(1);
        }

        // Collect light IDs in a loop
        vector<int> lightIds;
        while (true) {
            cout << "Enter light id (ENTER to finish): ";
            string s = ReadLineTrimmed();
            if (s.empty()) break;

            int lid = 0;
            try {
                lid = std::stoi(s, nullptr, 0);
            } catch (...) {
                cout << "Invalid light id, try again.\n";
                continue;
            }
            if (lid < 0 || lid > 255) {
                cout << "Light id must be 0-255\n";
                continue;
            }
            lightIds.push_back(lid);
        }

        if (lightIds.empty()) {
            LOG_F(ERROR, "No lights entered; nothing to save");
            std::exit(1);
        }

        // Generate a new group ID (use max gid + 1, or 1 if no groups exist)
        unsigned long newGid = 1;
        if (!groups->empty()) {
            for (const auto& g : *groups) {
                if (g.gid >= newGid) newGid = g.gid + 1;
            }
        }

        // Build the new group
        AlienFX_SDK::Afx_group newGroup;
        newGroup.gid = newGid;
        newGroup.name = zoneName;
        newGroup.lights.clear();

        for (int lid : lightIds) {
            AlienFX_SDK::Afx_groupLight gl;
            gl.did = dev.pid;  // device PID
            gl.lid = (unsigned short)lid;
            newGroup.lights.push_back(gl);
        }

        // Add to groups and save
        groups->push_back(newGroup);
        afx_map.SaveMappings();

        cout << "Created zone '" << zoneName << "' (gid=" << newGid << ") with "
             << lightIds.size() << " light(s) from device PID 0x" << std::hex
             << dev.pid << std::dec << ".\n";
    });
    // fan commands
    auto* cmd_getpp =
        app.add_subcommand("getpowerprofile", "Get current power profile");
    cmd_getpp->callback([&]() {
        fan.Probe();
        cout << "Current power profile: " << fan.GetPowerProfile().name << "\n";
    });

    auto* cmd_supported =
        app.add_subcommand("supportedprofiles", "Get supported power profiles");
    cmd_supported->callback([&]() {
        fan.Probe();
        cout << "Supported power profiles:\n";
        for (const auto& p : fan.profiles) cout << "  " << p.name << "\n";
    });

    auto* cmd_setpp = app.add_subcommand("setpowerprofile",
                                         "Set power profile (requires root)");
    string profile;
    cmd_setpp->add_option("profile", profile)->required();
    cmd_setpp->callback([&]() {
        fan.Probe();
        std::map<std::string, AlienFan_SDK::ALIENFAN_PROFILE> strToProfile = {
            {"custom", AlienFan_SDK::CUSTOM},
            {"balanced", AlienFan_SDK::BALANCED},
            {"quiet", AlienFan_SDK::QUIET},
            {"balanced-performance", AlienFan_SDK::BALANCED_PERFORMANCE},
            {"performance", AlienFan_SDK::PERFORMANCE},
            {"cool", AlienFan_SDK::COOL},
            {"low-power", AlienFan_SDK::LOW_POWER},
        };

        auto it = strToProfile.find(profile);
        if (it == strToProfile.end())
            throw CLI::ValidationError("profile", "Unknown profile");

        if (fan.SetPowerProfile(it->second)) {
            cout << "Power profile set to " << profile << "\n";
        } else {
            cout << "Failed to set power profile to " << profile << "\n";
        }
    });

    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);
    return 0;
}
