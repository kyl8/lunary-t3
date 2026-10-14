#pragma once

#include "mouse_automation.hpp"
#include "telemetry_client.hpp"

#include <windows.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace lunary::ui {

struct AimControllerConfig {
    AimSettings aim;
    HWND targetWindow{};
    float sourceWidth{1600.0F};
    float sourceHeight{900.0F};
    bool flipScreenY{true};
};

class AimController {
public:
    AimController(TelemetryClient& client, MouseAutomation& automation);
    ~AimController();

    AimController(const AimController&) = delete;
    AimController& operator=(const AimController&) = delete;

    void start();
    void stop();
    void configure(const AimControllerConfig& config);

private:
    void run();

    TelemetryClient& client_;
    MouseAutomation& automation_;
    std::mutex configMutex_;
    AimControllerConfig config_;
    std::atomic<bool> running_{};
    std::thread worker_;
};

} // namespace lunary::ui
