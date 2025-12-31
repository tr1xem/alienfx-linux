#include "AlienFan-SDK.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <loguru.hpp>
#include <string>

namespace fs = std::filesystem;
namespace AlienFan_SDK {

void Control::EnumSensorsPath() {
    const fs::path base{"/sys/class/hwmon"};

    if (!fs::exists(base) || !fs::is_directory(base)) {
        sensorsSupported = false;
        return;
    }

    for (const auto &entry : fs::directory_iterator(base)) {
        if (!entry.is_directory())
            continue;

        fs::path nameFile = entry.path() / "name";
        if (!fs::exists(nameFile))
            continue;

        std::ifstream in(nameFile);
        if (!in)
            continue;

        std::string driver;
        std::getline(in, driver);

        // trim whitespace/newline
        driver.erase(std::remove_if(driver.begin(), driver.end(), ::isspace),
                     driver.end());

        if (driver == "alienware_wmi") {
            sensorBasePath = entry.path(); // ← THIS is what you want
#ifdef DEBUG
            LOG_S(INFO) << "alienware-wmi driver loaded at sensor: "
                        << sensorBasePath.c_str();
#endif
            sensorsSupported = true;
            return;
        }
    }
}

void Control::EnumProfilePath() {
    const fs::path base{"/sys/class/platform-profile"};

    if (!fs::exists(base) || !fs::is_directory(base)) {
        profilesSupported = false;
        return;
    }

    for (const auto &entry : fs::directory_iterator(base)) {
        if (!entry.is_directory())
            continue;

        fs::path nameFile = entry.path() / "name";
        if (!fs::exists(nameFile))
            continue;

        std::ifstream in(nameFile);
        if (!in)
            continue;

        std::string driver;
        std::getline(in, driver);

        // trim whitespace/newline
        driver.erase(std::remove_if(driver.begin(), driver.end(), ::isspace),
                     driver.end());

        if (driver == "alienware-wmi") {
            profileBasePath = entry.path(); // ← THIS is what you want
#ifdef DEBUG
            LOG_S(INFO) << "alienware-wmi driver loaded at profile: "
                        << profileBasePath.c_str();
#endif
            isAlienware = true;
            profilesSupported = true;
            return;
        }
    }
}
bool Control::Probe() {
#ifdef DEBUG
    LOG_S(INFO) << "Probing hardware for sensors, profiles and fans";
#endif

    profiles.clear();
    sensors.clear();
    fans.clear();

    // filling profiles
    if (!profileBasePath.empty()) {
        std::ifstream in(profileBasePath / "choices");
        if (in) {
            std::string line;
            std::getline(in, line);

            std::istringstream iss(line);
            std::string token;

            while (iss >> token) {
                for (int i = 0; i <= LOW_POWER; ++i) {
                    if (token == ALIENFAN_PROFILE_NAME[i]) {
                        profiles.push_back(
                            {token, static_cast<ALIENFAN_PROFILE>(i)});
                        if (token == ALIENFAN_PROFILE_NAME[PERFORMANCE])
                            isGmodeSupported = true;
                        break;
                    }
                }
            }
        }
    }

    // filling sensors
    if (!sensorBasePath.empty()) {
        for (const auto &entry : fs::directory_iterator(sensorBasePath)) {
            const std::string fname = entry.path().filename().string();

            if (!fname.starts_with("temp") || !fname.ends_with("_input"))
                continue;

            const size_t pos = fname.find('_');
            const int index = std::stoi(fname.substr(4, pos - 4));

            ALIENFAN_SENSOR info;
            info.index = static_cast<uint8_t>(index);
            info.filename = fname;

            fs::path label =
                sensorBasePath / ("temp" + std::to_string(index) + "_label");

            if (fs::exists(label)) {
                std::ifstream l(label);
                std::getline(l, info.name);
            } else {
                info.name = "Temp " + std::to_string(index);
            }

            sensors.push_back(info);
        }
    }
    if (!sensorBasePath.empty()) {
        for (const auto &entry : fs::directory_iterator(sensorBasePath)) {
            const std::string fname = entry.path().filename().string();

            if (!fname.starts_with("fan") || !fname.ends_with("_input"))
                continue;

            const size_t pos = fname.find('_');
            const int index = std::stoi(fname.substr(3, pos - 3));

            // info.id = static_cast<uint8_t>(index);

            std::string fanName = "generic";
            fs::path label =
                sensorBasePath / ("fan" + std::to_string(index) + "_label");

            if (fs::exists(label)) {
                std::ifstream l(label);
                std::getline(l, fanName);
            } else {
                fanName = "Fan " + std::to_string(index);
            }

            ALIENFAN_FAN info(index, fanName);
            fans.push_back(info);
        }
    }

    return !profiles.empty() || !sensors.empty() || !fans.empty();
}
Control::Control() {
    EnumSensorsPath();
    EnumProfilePath();
}

int AlienFan_SDK::Control::GetFanRPM(ALIENFAN_FAN fan) {
    const auto p = sensorBasePath / fan.fanSpeed;

    std::ifstream in(p);
    if (!in)
        return -1;

    int rpm;
    if (!(in >> rpm))
        return -1;

    return rpm;
}
int AlienFan_SDK::Control::GetMaxRPM(ALIENFAN_FAN fan) {
    // NOTE: fanID starts from 1

    const auto p = sensorBasePath / fan.maxRPM;

    std::ifstream in(p);
    if (!in)
        return -1;

    int rpm;
    if (!(in >> rpm))
        return -1;

    return rpm;
}
int AlienFan_SDK::Control::GetFanPercent(ALIENFAN_FAN fan) {
    // fanID is 1-based
    int rpm = GetFanRPM(fan);
    int maxRpm = GetMaxRPM(fan);

    if (rpm < 0 || maxRpm <= 0)
        return -1; // unknown or invalid

    int percent = static_cast<int>((rpm * 100) / maxRpm);
    if (percent > 100)
        percent = 100; // cap at 100%
    if (percent < 0)
        percent = 0; // sanity

    return percent;
}

int AlienFan_SDK::Control::GetFanBoost(ALIENFAN_FAN fan) {
    const auto p = sensorBasePath / fan.boost;

    std::ifstream in(p);
    if (!in)
        return -1;

    int boost;
    if (!(in >> boost))
        return -1;

    return boost;
}

bool AlienFan_SDK::Control::SetFanBoost(ALIENFAN_FAN fan, uint8_t value) {
    const auto p = sensorBasePath / fan.boost;

    std::ofstream out(p);
    if (!out)
        return 0;

    out << static_cast<int>(value);
    out.flush();
    return 1;
}

int AlienFan_SDK::Control::GetTempValue(ALIENFAN_SENSOR sensor) {
    const auto p = sensorBasePath / sensor.filename;

    std::ifstream in(p);
    if (!in)
        return -1;

    int temp;
    if (!(in >> temp))
        return -1;

    return temp / 1000;
}

bool AlienFan_SDK::Control::SetPowerProfile(ALIENFAN_PROFILE profile) {
    const auto p = profileBasePath / "profile";

    std::ofstream out(p);
    if (!out)
        return 0;

    for (const auto &p : profiles) {
        if (p.type == profile) {
            out << ALIENFAN_PROFILE_NAME[static_cast<int>(profile)];
            out.flush();
            return 1;
        }
    }
    return false;
}

AlienFan_SDK::ALIENFAN_PROFILE_INFO AlienFan_SDK::Control::GetPowerProfile() {
    const auto p = profileBasePath / "profile";

    std::ifstream in(p);
    if (!in)
        return {"", LOW_POWER};

    std::string profile;
    if (!(in >> profile))
        return {"", LOW_POWER};

    for (const auto &p : profiles) {
        if (p.name == profile)
            return p;
    }
    return {"", LOW_POWER};
}

bool AlienFan_SDK::Control::SetGMode(bool state) {
    auto current = GetPowerProfile();
    if (current.type == PERFORMANCE || state == true)
        return true;
    else if (current.type != PERFORMANCE || state == true)
        return SetPowerProfile(PERFORMANCE);
    else if (current.type == PERFORMANCE || state == false)
        return SetPowerProfile(BALANCED);
    else
        return false;
}

bool AlienFan_SDK::Control::GetGMode() {
    auto current = GetPowerProfile();
    return current.type == PERFORMANCE;
}

bool AlienFan_SDK::Control::Unlock() { return SetPowerProfile(CUSTOM); }

} // namespace AlienFan_SDK
