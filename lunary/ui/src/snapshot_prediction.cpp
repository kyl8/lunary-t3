#include "snapshot_prediction.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace lunary::ui {

namespace {

const EntityTelemetry* findEntity(const Snapshot& snapshot, std::uint64_t address) {
    const auto found = std::find_if(snapshot.entities.begin(), snapshot.entities.end(),
        [address](const EntityTelemetry& entity) { return entity.address == address; });
    return found == snapshot.entities.end() ? nullptr : &*found;
}

bool snapshotHasMotion(const Snapshot& previous, const Snapshot& current) {
    if (previous.entities.empty() || current.entities.empty()) return false;
    for (const auto& entity : current.entities) {
        const auto* before = findEntity(previous, entity.address);
        if (!before) continue;
        const float dx = entity.screenPosition.x - before->screenPosition.x;
        const float dy = entity.screenPosition.y - before->screenPosition.y;
        float aimMotionSquared{};
        if (entity.hasHeadHitScreenPosition && before->hasHeadHitScreenPosition) {
            const float headDx = entity.headHitScreenPosition.x -
                                 before->headHitScreenPosition.x;
            const float headDy = entity.headHitScreenPosition.y -
                                 before->headHitScreenPosition.y;
            aimMotionSquared = std::max(aimMotionSquared, headDx * headDx + headDy * headDy);
        }
        if (entity.hasTorsoHitScreenPosition && before->hasTorsoHitScreenPosition) {
            const float torsoDx = entity.torsoHitScreenPosition.x -
                                  before->torsoHitScreenPosition.x;
            const float torsoDy = entity.torsoHitScreenPosition.y -
                                  before->torsoHitScreenPosition.y;
            aimMotionSquared = std::max(aimMotionSquared,
                                        torsoDx * torsoDx + torsoDy * torsoDy);
        }
        if (dx * dx + dy * dy > 1.0e-7F || aimMotionSquared > 1.0e-7F ||
            entity.behindCamera != before->behindCamera) {
            return true;
        }
    }
    return false;
}

Snapshot extrapolate(const Snapshot& previous, const Snapshot& currentMotion,
                     const Snapshot& latest, float factor, float maxFactor) {
    Snapshot result = latest;
    factor = std::clamp(factor, 0.0F, maxFactor);
    for (auto& entity : result.entities) {
        const auto* before = findEntity(previous, entity.address);
        const auto* current = findEntity(currentMotion, entity.address);
        if (!before || !current || before->behindCamera != current->behindCamera ||
            current->behindCamera != entity.behindCamera) {
            continue;
        }
        const auto project = [factor](float baseValue, float oldValue, float newValue) {
            return baseValue + (newValue - oldValue) * factor;
        };
        entity.position.x = project(entity.position.x, before->position.x, current->position.x);
        entity.position.y = project(entity.position.y, before->position.y, current->position.y);
        entity.position.z = project(entity.position.z, before->position.z, current->position.z);
        entity.screenPosition.x = project(entity.screenPosition.x, before->screenPosition.x,
                                          current->screenPosition.x);
        entity.screenPosition.y = project(entity.screenPosition.y, before->screenPosition.y,
                                          current->screenPosition.y);
        entity.headScreenPosition.x = project(entity.headScreenPosition.x,
                                              before->headScreenPosition.x,
                                              current->headScreenPosition.x);
        entity.headScreenPosition.y = project(entity.headScreenPosition.y,
                                              before->headScreenPosition.y,
                                              current->headScreenPosition.y);
        if (entity.hasHeadHitScreenPosition && before->hasHeadHitScreenPosition &&
            current->hasHeadHitScreenPosition) {
            entity.headHitScreenPosition.x = project(entity.headHitScreenPosition.x,
                                                     before->headHitScreenPosition.x,
                                                     current->headHitScreenPosition.x);
            entity.headHitScreenPosition.y = project(entity.headHitScreenPosition.y,
                                                     before->headHitScreenPosition.y,
                                                     current->headHitScreenPosition.y);
        }
        if (entity.hasTorsoHitScreenPosition && before->hasTorsoHitScreenPosition &&
            current->hasTorsoHitScreenPosition) {
            entity.torsoHitScreenPosition.x = project(entity.torsoHitScreenPosition.x,
                                                      before->torsoHitScreenPosition.x,
                                                      current->torsoHitScreenPosition.x);
            entity.torsoHitScreenPosition.y = project(entity.torsoHitScreenPosition.y,
                                                      before->torsoHitScreenPosition.y,
                                                      current->torsoHitScreenPosition.y);
        }
        entity.distance = project(entity.distance, before->distance, current->distance);
    }
    return result;
}

} // namespace

bool MotionPredictor::push(const Snapshot& snapshot, std::uint64_t arrivalMs) {
    if (snapshot.sequence == observedSequence_) return false;
    if (observedSequence_ != 0 && snapshot.sequence < observedSequence_) reset();

    const bool recoveringEntities = latest_.entities.empty() && !snapshot.entities.empty();
    if (currentMotion_.entities.empty() || recoveringEntities) {
        previousMotion_ = {};
        currentMotion_ = snapshot;
        motionArrivalMs_ = arrivalMs;
    } else if (snapshotHasMotion(currentMotion_, snapshot)) {
        previousMotion_ = std::move(currentMotion_);
        currentMotion_ = snapshot;
        motionArrivalMs_ = arrivalMs;
    }

    latest_ = snapshot;
    observedSequence_ = snapshot.sequence;
    latestArrivalMs_ = arrivalMs;
    return true;
}

void MotionPredictor::reset() {
    observedSequence_ = 0;
    latestArrivalMs_ = 0;
    motionArrivalMs_ = 0;
    latest_ = {};
    previousMotion_ = {};
    currentMotion_ = {};
}

bool MotionPredictor::hasMotion() const {
    return snapshotHasMotion(previousMotion_, currentMotion_);
}

std::uint64_t MotionPredictor::motionSequence() const {
    return currentMotion_.sequence != 0 ? currentMotion_.sequence : latest_.sequence;
}

Snapshot MotionPredictor::predict(std::uint64_t nowMs, float leadMs, float maxFactor) const {
    if (!hasMotion() || currentMotion_.timestampMs <= previousMotion_.timestampMs) return latest_;
    const auto frameDelta = currentMotion_.timestampMs - previousMotion_.timestampMs;
    const float ageMs = static_cast<float>(nowMs >= motionArrivalMs_
        ? nowMs - motionArrivalMs_ : 0);
    const float factor = (ageMs + std::max(leadMs, 0.0F)) /
                         static_cast<float>(frameDelta);
    return extrapolate(previousMotion_, currentMotion_, latest_, factor, maxFactor);
}

} // namespace lunary::ui
