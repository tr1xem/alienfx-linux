#include <CLI/CLI.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <loguru.hpp>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "AlienFX_SDK.h"
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
    std::string desciption = std::format(
        "AlienFX CLI v{} \n Control all of your Alienware device from the "
        "comfort of your terminal",
        VERSION);

    CLI::App app{desciption, "alienfx_cli"};
#ifndef DEBUG
    loguru::g_stderr_verbosity = loguru::Verbosity_ERROR;
#else
    loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
#endif
    loguru::init(argc, argv);
    argv = app.ensure_utf8(argv);
    app.set_version_flag("-v", string("alienfx-cli v") + VERSION);

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

    int repeatCount = 1;
    int delayMs = 0;
    app.add_option("-r,--repeat", repeatCount, "Repeat count (0 = infinite)")
        ->default_val(1)
        ->check(CLI::NonNegativeNumber);
    app.add_option("-d,--delay", delayMs, "Delay between repeats (ms)")
        ->default_val(0)
        ->check(CLI::NonNegativeNumber);

    bool saveState = false;
    app.add_flag("-s,--save", saveState,
                 "Save to hardware (persist across reboots)");

    auto repeatCall = [&](auto&& fn) {
        for (int i = 0; repeatCount == 0 || i < repeatCount; i++) {
            fn();
            if (delayMs > 0 && (repeatCount == 0 || i + 1 < repeatCount))
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
    };

    // setall r g b
    auto* cmd_setall = app.add_subcommand("setall", "r g b - set all lights");
    int r = 0, g = 0, b = 0;
    cmd_setall->add_option("r", r)->required()->check(CLI::Range(0, 255));
    cmd_setall->add_option("g", g)->required()->check(CLI::Range(0, 255));
    cmd_setall->add_option("b", b)->required()->check(CLI::Range(0, 255));
    cmd_setall->callback([&]() {
        ensureInit();
        repeatCall([&]() {
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
                if (saveState) {
                    vector<AlienFX_SDK::Afx_lightblock> blocks;
                    for (auto& l : dev.lights) {
                        if (!(l.flags & ALIENFX_FLAG_POWER))
                            blocks.push_back({(uint8_t)l.lightid, {act}});
                    }
                    dev.dev->SaveLightsState(&blocks);
                    dev.dev->SaveLightsStateToStartup(&blocks);
                }
            }
            Update();
        });
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
        repeatCall([&]() {
            if ((size_t)devIndex >= afx_map.fxdevs.size())
                throw CLI::ValidationError("dev", "Device index out of range");

            auto& dev = afx_map.fxdevs[(size_t)devIndex];
            if (!dev.dev) throw std::runtime_error("Device not initialized");

            AlienFX_SDK::Afx_lightblock block{
                (uint8_t)lightId,
                {MakeColorAction(r2, g2, b2,
                                 AlienFX_SDK::Action::AlienFX_A_Color)}};
            dev.dev->SetAction(&block);
            if (saveState) {
                vector<AlienFX_SDK::Afx_lightblock> blocks = {block};
                dev.dev->SaveLightsState(&blocks);
                dev.dev->SaveLightsStateToStartup(&blocks);
            }
            dev.dev->UpdateColors();
        });
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
        repeatCall([&]() {
            unsigned zoneCode = GetZoneCodeFromString(zone);
            auto* grp = afx_map.GetGroupById(zoneCode);
            if (!grp) {
                LOG_F(ERROR, "Zone/group not found: %s", zone.c_str());
                return;
            }

            auto act = MakeColorAction(zr, zg, zb,
                                       AlienFX_SDK::Action::AlienFX_A_Color);
            for (auto& dev : afx_map.fxdevs) {
                if (!dev.dev || !dev.present) continue;
                vector<unsigned char> lights;
                for (auto& gl : grp->lights) {
                    if (gl.did == dev.pid) lights.push_back((uint8_t)gl.lid);
                }
                dev.dev->SetMultiColor(&lights, act);
                if (saveState) {
                    vector<AlienFX_SDK::Afx_lightblock> blocks;
                    for (auto& gl : grp->lights) {
                        if (gl.did == dev.pid)
                            blocks.push_back({(uint8_t)gl.lid, {act}});
                    }
                    dev.dev->SaveLightsState(&blocks);
                    dev.dev->SaveLightsStateToStartup(&blocks);
                }
            }
            Update();
        });
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
        repeatCall([&]() {
            if ((size_t)sa_dev >= afx_map.fxdevs.size())
                throw CLI::ValidationError("dev", "Device index out of range");

            auto actions = ParseActionList(sa_tokens);
            AlienFX_SDK::Afx_lightblock block{(uint8_t)sa_light, actions};
            afx_map.fxdevs[(size_t)sa_dev].dev->SetAction(&block);
            if (saveState) {
                vector<AlienFX_SDK::Afx_lightblock> blocks = {block};
                afx_map.fxdevs[(size_t)sa_dev].dev->SaveLightsState(&blocks);
                afx_map.fxdevs[(size_t)sa_dev].dev->SaveLightsStateToStartup(
                    &blocks);
            }
            Update();
        });
    });

    // setpower dev light r g b r2 g2 b2
    auto* cmd_setpower = app.add_subcommand(
        "setpower", "dev light r g b r2 g2 b2 - set power button colors");
    int sp_dev = 0, sp_light = 0, sp_r1 = 0, sp_g1 = 0, sp_b1 = 0;
    int sp_r2 = 0, sp_g2 = 0, sp_b2 = 0;
    cmd_setpower->add_option("dev", sp_dev)
        ->required()
        ->check(CLI::NonNegativeNumber);
    cmd_setpower->add_option("light", sp_light)
        ->required()
        ->check(CLI::Range(0, 255));
    cmd_setpower->add_option("r1", sp_r1)
        ->required()
        ->check(CLI::Range(0, 255));
    cmd_setpower->add_option("g1", sp_g1)
        ->required()
        ->check(CLI::Range(0, 255));
    cmd_setpower->add_option("b1", sp_b1)
        ->required()
        ->check(CLI::Range(0, 255));
    cmd_setpower->add_option("r2", sp_r2)
        ->required()
        ->check(CLI::Range(0, 255));
    cmd_setpower->add_option("g2", sp_g2)
        ->required()
        ->check(CLI::Range(0, 255));
    cmd_setpower->add_option("b2", sp_b2)
        ->required()
        ->check(CLI::Range(0, 255));
    cmd_setpower->callback([&]() {
        ensureInit();
        repeatCall([&]() {
            if ((size_t)sp_dev >= afx_map.fxdevs.size())
                throw CLI::ValidationError("dev", "Device index out of range");
            AlienFX_SDK::Afx_lightblock act{
                .index = (uint8_t)sp_light,
                .act = {{.type = AlienFX_SDK::AlienFX_A_Power,
                         .time = 3,
                         .tempo = 0x64,
                         .r = (uint8_t)sp_r1,
                         .g = (uint8_t)sp_g1,
                         .b = (uint8_t)sp_b1},
                        {.type = AlienFX_SDK::AlienFX_A_Power,
                         .time = 3,
                         .tempo = 0x64,
                         .r = (uint8_t)sp_r2,
                         .g = (uint8_t)sp_g2,
                         .b = (uint8_t)sp_b2}}};
            afx_map.fxdevs[(size_t)sp_dev].dev->SetPowerAction(&act);
            if (saveState) {
                vector<AlienFX_SDK::Afx_lightblock> blocks = {act};
                afx_map.fxdevs[(size_t)sp_dev].dev->SaveLightsState(&blocks);
                afx_map.fxdevs[(size_t)sp_dev].dev->SaveLightsStateToStartup(
                    &blocks);
            }
            Update();
        });
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
        repeatCall([&]() {
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
            if (saveState) {
                map<unsigned short, vector<AlienFX_SDK::Afx_lightblock>>
                    devBlocks;
                for (auto& gl : grp->lights) {
                    devBlocks[gl.did].push_back({(uint8_t)gl.lid, actions});
                }
                for (auto& [did, blocks] : devBlocks) {
                    auto* dev = afx_map.GetDeviceById(did);
                    if (dev && dev->dev) dev->dev->SaveLightsState(&blocks);
                    if (dev && dev->dev)
                        dev->dev->SaveLightsStateToStartup(&blocks);
                }
            }
            Update();
        });
    });

    // setdim [dev] br  (support both: "setdim br" and "setdim dev br")
    auto* cmd_setdim =
        app.add_subcommand("setdim", "[dev] br - set brightness");
    vector<int> dim_args;
    cmd_setdim->add_option("args", dim_args)->required()->expected(1, 2);
    cmd_setdim->callback([&]() {
        ensureInit();
        repeatCall([&]() {
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
                    throw CLI::ValidationError("dev",
                                               "Device index out of range");
                auto& dev = afx_map.fxdevs[(size_t)d];
                dev.brightness = (uint8_t)br;
                afx_map.SetDeviceBrightness(&dev, globalBright, false);
            }
        });
    });

    // setglobal dev type mode [r g b [r g b]]
    auto* cmd_setglobal =
        app.add_subcommand("setglobal", "dev type mode [r g b [r g b]]");
    vector<int> gargs;
    cmd_setglobal->add_option("args", gargs)->required()->expected(3, 9);
    cmd_setglobal->callback([&]() {
        ensureInit();
        repeatCall([&]() {
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
    });

    // status
    auto* cmd_status =
        app.add_subcommand("status", "Show devices, lights and zones");
    cmd_status->callback([&]() {
        ensureInit();
        repeatCall([&]() {
            for (auto it = afx_map.fxdevs.begin(); it < afx_map.fxdevs.end();
                 it++) {
                cout << "Device #" << (it - afx_map.fxdevs.begin()) << " - "
                     << it->name << ", VID#0x" << std::hex << it->vid
                     << std::dec << ", PID#0x" << std::hex << it->pid
                     << std::dec << ", APIv" << (int)it->version << ", "
                     << it->lights.size() << " lights"
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
#ifdef ALIENFX_HID_BACKEND_LIBUSB
        // This build was configured with -DALIENFX_HID_BACKEND=libusb, which
        // detaches the kernel driver from every HID interface it opens (see
        // the comment on that option in AlienFX-SDK/CMakeLists.txt) and only
        // reattaches it when the device is closed. `probe` opens every
        // detected device and then blocks on an interactive prompt -- on
        // hardware where an AlienFX-family VID also owns a HID input
        // interface (e.g. some Darfon RGB keyboards), that combination can
        // disable the very keyboard needed to answer the prompt, with no way
        // to recover except from another machine or SSH. Refuse to run
        // interactively.
        cerr << "probe is disabled on a libusb-backend build: opening "
                "devices detaches the kernel HID driver (disabling e.g. the "
                "keyboard), then this command blocks on a prompt with no way "
                "to answer it. Rebuild with the default "
                "-DALIENFX_HID_BACKEND=hidraw.\n";
        return;
#endif
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
                for (auto it = groups->begin(); it != groups->end(); ++it) {
                    if (it->name == zoneName) {
                        groups->erase(it);
                        break;
                    }
                }
                break;
            }
        }

        struct DevLight {
            int devIndex;
            int lightId;
        };
        vector<DevLight> devLights;

        while (true) {
            cout << "\nDevice index (from status, or :w to save): ";
            string devIdxStr = ReadLineTrimmed();

            if (devIdxStr == ":w") {
                break;
            }

            if (devIdxStr.empty()) {
                cout << "Device index required (or :w to save)\n";
                continue;
            }

            int devIndex = 0;
            try {
                devIndex = std::stoi(devIdxStr, nullptr, 0);
            } catch (...) {
                cout << "Invalid device index\n";
                continue;
            }

            if (devIndex < 0 || (size_t)devIndex >= afx_map.fxdevs.size()) {
                cout << "Device index out of range (0-"
                     << (afx_map.fxdevs.size() - 1) << ")\n";
                continue;
            }

            auto& dev = afx_map.fxdevs[(size_t)devIndex];
            cout << "Selected device: " << dev.name << " (PID 0x" << std::hex
                 << dev.pid << std::dec << ")\n";

            while (true) {
                cout << "  Light id (ENTER for next device, :q to switch "
                        "device, :w to save): ";
                string lightStr = ReadLineTrimmed();

                if (lightStr.empty()) {
                    break;
                }

                if (lightStr == ":q") {
                    break;
                }

                if (lightStr == ":w") {
                    goto save_zone;
                }

                int lid = 0;
                try {
                    lid = std::stoi(lightStr, nullptr, 0);
                } catch (...) {
                    cout << "  Invalid light id, try again.\n";
                    continue;
                }

                if (lid < 0 || lid > 255) {
                    cout << "  Light id must be 0-255\n";
                    continue;
                }

                devLights.push_back({devIndex, lid});
                cout << "  Added light #" << lid << " from device #" << devIndex
                     << "\n";
            }
        }

    save_zone:
        if (devLights.empty()) {
            LOG_F(ERROR, "No lights entered; nothing to save");
            std::exit(1);
        }

        unsigned long newGid = 1;
        if (!groups->empty()) {
            for (const auto& g : *groups) {
                if (g.gid >= newGid) newGid = g.gid + 1;
            }
        }

        AlienFX_SDK::Afx_group newGroup;
        newGroup.gid = newGid;
        newGroup.name = zoneName;
        newGroup.lights.clear();

        for (const auto& dl : devLights) {
            auto& dev = afx_map.fxdevs[(size_t)dl.devIndex];
            AlienFX_SDK::Afx_groupLight gl;
            gl.did = dev.pid;  // device PID
            gl.lid = (unsigned short)dl.lightId;
            newGroup.lights.push_back(gl);
        }

        groups->push_back(newGroup);
        afx_map.SaveMappings();

        cout << "\nCreated zone '" << zoneName << "' (gid=" << newGid
             << ") with " << devLights.size() << " light(s).\n";
    });

    // fan commands
    auto* cmd_getpp =
        app.add_subcommand("getpowerprofile", "Get current power profile");
    cmd_getpp->callback([&]() {
        repeatCall([&]() {
            fan.Probe();
            cout << "Current power profile: " << fan.GetPowerProfile().name
                 << "\n";
        });
    });

    auto* cmd_supported =
        app.add_subcommand("supportedprofiles", "Get supported power profiles");
    cmd_supported->callback([&]() {
        repeatCall([&]() {
            fan.Probe();
            cout << "Supported power profiles:\n";
            for (const auto& p : fan.profiles) cout << "  " << p.name << "\n";
        });
    });

    auto* cmd_setpp = app.add_subcommand("setpowerprofile",
                                         "Set power profile (requires root)");
    string profile;
    cmd_setpp->add_option("profile", profile)->required();
    cmd_setpp->callback([&]() {
        repeatCall([&]() {
            fan.Probe();
            std::map<std::string, AlienFan_SDK::ALIENFAN_PROFILE> strToProfile =
                {
                    {"custom", AlienFan_SDK::CUSTOM},
                    {"balanced", AlienFan_SDK::BALANCED},
                    {"quiet", AlienFan_SDK::QUIET},
                    {"balanced-performance",
                     AlienFan_SDK::BALANCED_PERFORMANCE},
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
    });

    // reset
    auto* cmd_reset =
        app.add_subcommand("reset", "Reset device (if supported)");
    cmd_reset->callback([&]() {
        ensureInit();
        uint8_t resetCommand[]{0x2, 0x3, 0xff};
        for (auto& d : afx_map.fxdevs) {
            if (d.present && d.version == AlienFX_SDK::API_V4) {
                bool ok = d.dev->PrepareAndSend(resetCommand);
                cout << "Reset " << d.name << ": "
                     << (ok ? "successful" : "failed") << "\n";
            }
        }
    });

    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);
    return 0;
}
