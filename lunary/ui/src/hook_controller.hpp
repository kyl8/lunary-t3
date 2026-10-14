#pragma once

#include <lunary/protocol.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace lunary::ui {

struct HookSettings {
    bool enabled{false};
    bool head{true};
    bool ignoreTeammates{true};
    bool lockTarget{false};
    float fovDegrees{25.0F};
    float smoothing{3.0F};
    float maximumDistance{90.0F};
    float eyeHeight{1.4F};
};

// Full cheat feature config (mirrors injected side + protocol)
using FeatureSettings = FeatureConfigData;

enum class HookControllerState {
    Ready,
    Activating,
    Syncing,
    Active,
    Disabled,
    Error,
};

struct HookControllerView {
    HookControllerState state{HookControllerState::Ready};
    std::string status{"Pronto para ativar"};
    std::string log;
    bool busy{};
    bool injected{};
};

class HookController {
public:
    HookController();
    ~HookController();

    HookController(const HookController&) = delete;
    HookController& operator=(const HookController&) = delete;

    void activate(const HookSettings& settings, std::string packageName);
    void apply(const HookSettings& settings, std::string packageName);
    void applyFeatures(const FeatureSettings& features);
    void disable(const HookSettings& settings, std::string packageName);

    [[nodiscard]] HookControllerView view() const;
    [[nodiscard]] std::string projectRoot() const;

private:
    void workerLoop();
    bool writeConfiguration(const HookSettings& settings, std::string& error);
    bool runScript(const std::filesystem::path& script,
                   const std::wstring& arguments, std::string& error);
    void setState(HookControllerState state, std::string status);
    void appendLog(const std::string& text);

    std::filesystem::path root_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::thread worker_;
    HookSettings desired_;
    FeatureSettings featureSettings_;
    std::string packageName_{"com.xd.t3.global"};
    HookControllerState state_{HookControllerState::Ready};
    std::string status_{"Pronto para ativar"};
    std::string log_;
    std::uint64_t revision_{};
    std::chrono::steady_clock::time_point syncDeadline_{};
    bool activateRequested_{};
    bool syncRequested_{};
    bool injected_{};
    bool stopping_{};
};

} // namespace lunary::ui
