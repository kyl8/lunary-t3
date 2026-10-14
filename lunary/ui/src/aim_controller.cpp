#include "aim_controller.hpp"

#include "snapshot_prediction.hpp"

#include <chrono>

namespace lunary::ui {

AimController::AimController(TelemetryClient& client, MouseAutomation& automation)
    : client_(client), automation_(automation) {}

AimController::~AimController() {
    stop();
}

void AimController::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread(&AimController::run, this);
}

void AimController::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
    automation_.reset();
}

void AimController::configure(const AimControllerConfig& config) {
    std::scoped_lock lock(configMutex_);
    config_ = config;
}

void AimController::run() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    MotionPredictor predictor;
    std::uint64_t observedSequence{};

    while (running_) {
        const auto view = client_.waitForUpdate(observedSequence,
                                                std::chrono::milliseconds(4));
        const auto now = std::chrono::steady_clock::now();
        const auto nowMs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
        if (view.snapshot.sequence != observedSequence) {
            predictor.push(view.snapshot, nowMs);
            observedSequence = view.snapshot.sequence;
        }

        AimControllerConfig config;
        {
            std::scoped_lock lock(configMutex_);
            config = config_;
        }

        const bool connected = view.state == ConnectionState::Connected;
        const bool fresh = view.lastFrame != std::chrono::steady_clock::time_point{} &&
                           now - view.lastFrame <= std::chrono::milliseconds(250);
        auto snapshot = predictor.predict(nowMs, 3.0F, 0.55F);
        snapshot.sequence = predictor.motionSequence();
        automation_.update(config.aim, snapshot, connected, fresh, config.targetWindow,
                           config.sourceWidth, config.sourceHeight, config.flipScreenY);
    }
}

} // namespace lunary::ui
