#pragma once

#include <lunary/protocol.hpp>

#include <windows.h>

#include <cstdint>
#include <mutex>
#include <string>

namespace lunary::ui {

enum class AimPoint {
    Head,
    Torso,
};

struct AimSettings {
    bool enabled{};
    bool onlyPlayers{true};
    bool ignoreTeammates{true};
    bool allowUnknownTeams{true};
    bool lockTarget{true};
    bool showFov{true};
    bool showLockedTarget{true};
    int activationKey{VK_RBUTTON};
    int updateIntervalMs{1};
    AimPoint point{AimPoint::Head};
    float fovRadius{165.0F};
    float smoothing{1.0F};
    float strength{1.15F};
    float maxStep{50.0F};
    float maxDistance{90.0F};
};

struct AimStatus {
    bool activationHeld{};
    bool targetWindowFocused{};
    bool hasTarget{};
    std::uint64_t targetAddress{};
    float screenDistance{};
    std::string targetName;
    std::string message{"Desativada"};
};

class MouseAutomation {
public:
    void update(const AimSettings& settings, const Snapshot& snapshot,
                bool connected, bool dataFresh, HWND targetWindow,
                float sourceWidth, float sourceHeight, bool flipScreenY);
    void reset();

    [[nodiscard]] AimStatus status() const;

private:
    mutable std::mutex mutex_;
    AimStatus status_;
    std::uint64_t lockedAddress_{};
    std::uint64_t lastCommandSequence_{};
    std::uint64_t lastSampleAddress_{};
    ULONGLONG lastInputTick_{};
    float lastSampleX_{};
    float lastSampleY_{};
    float residualX_{};
    float residualY_{};
};

} // namespace lunary::ui
