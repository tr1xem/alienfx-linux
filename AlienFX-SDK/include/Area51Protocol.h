#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Darfon keyboard on the Alienware 16 Area-51 AA16250. No other USB id uses
// this sequence. The generic APIv5 path adds one to the light index and then
// sends loop 0x13; this firmware ignores that and keeps the old animation.

inline bool IsArea51Usb(uint16_t vid, uint16_t pid) {
    return vid == 0x0d62 && pid == 0x1bbc;
}

struct Area51KeyColor {
    uint8_t id = 0;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

// Fifteen 4-byte groups fit in a 64-byte feature report after the header.
inline constexpr int kArea51KeysPerReport = 15;

// 0x13 is the loop command and must not be sent. 0x01 is the block the
// function row stays on after 0x05, 0x06, and 0x07 have stopped.
inline constexpr uint8_t kArea51StopSubs[] = {0x05, 0x06, 0x07, 0x01};

struct Area51Report {
    std::vector<Area51KeyColor> keys;
};

inline std::vector<Area51Report>
PackArea51Reports(const std::vector<Area51KeyColor> &keys) {
    std::vector<Area51Report> reports;
    for (size_t i = 0; i < keys.size();) {
        Area51Report report;
        for (int n = 0; n < kArea51KeysPerReport && i < keys.size(); ++n, ++i)
            report.keys.push_back(keys[i]);
        if (!report.keys.empty())
            reports.push_back(std::move(report));
    }
    return reports;
}

enum class Area51StepKind { Stop, Color };

struct Area51Step {
    Area51StepKind kind = Area51StepKind::Stop;
    uint8_t stopSub = 0;
    size_t report = 0;
};

// Stops, then two tight passes. Each pass rewrites the first color report
// last, because that report is the one the firmware puts the old effect on.
inline std::vector<Area51Step> Area51SendOrder(size_t reportCount) {
    std::vector<Area51Step> steps;
    for (uint8_t sub : kArea51StopSubs)
        steps.push_back({Area51StepKind::Stop, sub, 0});
    if (reportCount == 0)
        return steps;
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0; i < reportCount; ++i)
            steps.push_back({Area51StepKind::Color, 0, i});
        steps.push_back({Area51StepKind::Color, 0, 0});
    }
    return steps;
}

// The power button is ELC light 0x1b, not a keyboard id. Phase time 3 strobes.
// A rainbow action with no color leaves the LED off.
inline constexpr uint8_t kPowerRainbowHold = 40;
inline constexpr uint8_t kPowerRainbowTempo = 0x64;

struct PowerRainbowStep {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

inline constexpr std::array<PowerRainbowStep, 7> kPowerRainbowSteps{{
    {255, 0, 0},   {255, 165, 0}, {255, 255, 0}, {0, 255, 0},
    {0, 191, 255}, {0, 0, 255},   {128, 0, 128},
}};
