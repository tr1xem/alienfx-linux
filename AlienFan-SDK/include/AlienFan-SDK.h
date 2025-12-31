#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace AlienFan_SDK {

struct ALIENFAN_FAN {
    uint8_t id{};
    std::string name;
    std::string fanSpeed{};
    std::string boost{};
    std::string maxRPM{};
    ALIENFAN_FAN(uint8_t fanId, const std::string &fanName)
        : id(fanId), name(fanName) {
        fanSpeed = "fan" + std::to_string(id) + "_input";
        boost = "fan" + std::to_string(id) + "_boost";
        maxRPM = "fan" + std::to_string(id) + "_max";
    }
};

struct ALIENFAN_SENSOR {
    uint8_t index{};
    std::string filename{};
    std::string name{};
};

inline const char *ALIENFAN_PROFILE_NAME[] = {
    "custom",      "balanced", "quiet",    "balanced-performance",
    "performance", "cool",     "low-power"};

enum ALIENFAN_PROFILE {
    CUSTOM = 0,
    BALANCED = 1,
    QUIET = 2,
    BALANCED_PERFORMANCE = 3,
    PERFORMANCE = 4,
    COOL = 5,
    LOW_POWER = 6
};

struct ALIENFAN_PROFILE_INFO {
    std::string name;
    ALIENFAN_PROFILE type;
};

class Control {
  private:
    uint8_t sysType;
    std::filesystem::path sensorBasePath{};
    std::filesystem::path profileBasePath{};
    void EnumSensorsPath();
    void EnumProfilePath();

  public:
    bool isAlienware = false, profilesSupported = false,
         isGmodeSupported = false, sensorsSupported = false;
    uint8_t maxTCC, maxOffset;
    unsigned long systemID;
    Control();

    // Probe hardware, sensors, fans, power modes and fill structures.
    // Result: true - compatible hardware found, false - not found.
    bool Probe();

    // Get RPM for the fan index fanID at fans[]
    // Result: fan RPM
    int GetFanRPM(ALIENFAN_FAN fan);

    // Get Max. RPM for fan index fanID
    int GetMaxRPM(ALIENFAN_FAN fan);

    // Get fan RPMs as a percent of RPM
    // Result: percent of the fan speed
    int GetFanPercent(ALIENFAN_FAN fan);

    // Get boost value for the fan index fanID at fans[].
    // Result: Error or raw value if forced, otherwise cooked by boost.
    int GetFanBoost(ALIENFAN_FAN fan);

    // Set boost value for the fan index fanID at fans[].
    // Result:  true or false
    bool SetFanBoost(ALIENFAN_FAN fan, uint8_t value);

    // Get temperature value for the sensor index TanID at sensors[]
    // Result: temperature value or error
    int GetTempValue(ALIENFAN_SENSOR sensor);

    // Unlock manual fan operations. The same as SetPower(0)
    // Result: true or false
    bool Unlock();

    // Set system power profile to defined power profile
    // Result:  true or false
    bool SetPowerProfile(ALIENFAN_PROFILE profile);

    // Get current system power value.
    // Result: power value (raw true) or index in powers[] (raw false) or error
    ALIENFAN_PROFILE_INFO GetPowerProfile();

    // Toggle G-mode on some systems
    bool SetGMode(bool state);

    // Check G-mode state
    bool GetGMode();

    // NOTE: Not currently supported in linux
    // Get current TCC value
    //
    // int GetTCC();
    //
    // // Set TCC value, if possible
    // int SetTCC(uint8_t tccValue);
    //
    // // Get current XMP profile
    // int GetXMP();
    //
    // // Set current XMP profile
    // int SetXMP(uint8_t memXMP);
    //

    // Arrays of sensors, fans, max. boosts and power values detected at Probe()
    std::vector<ALIENFAN_SENSOR> sensors;
    std::vector<ALIENFAN_FAN> fans;
    std::vector<ALIENFAN_PROFILE_INFO> profiles;
};

} // namespace AlienFan_SDK
