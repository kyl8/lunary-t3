#pragma once

#include <lunary/protocol.hpp>

#include <cstdint>

namespace lunary::ui {

class MotionPredictor {
public:
    bool push(const Snapshot& snapshot, std::uint64_t arrivalMs);
    void reset();

    [[nodiscard]] Snapshot predict(std::uint64_t nowMs, float leadMs = 0.0F,
                                   float maxFactor = 0.75F) const;
    [[nodiscard]] const Snapshot& latest() const { return latest_; }
    [[nodiscard]] bool hasMotion() const;
    [[nodiscard]] std::uint64_t latestArrivalMs() const { return latestArrivalMs_; }
    [[nodiscard]] std::uint64_t motionSequence() const;

private:
    std::uint64_t observedSequence_{};
    std::uint64_t latestArrivalMs_{};
    std::uint64_t motionArrivalMs_{};
    Snapshot latest_;
    Snapshot previousMotion_;
    Snapshot currentMotion_;
};

} // namespace lunary::ui
