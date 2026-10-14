#pragma once

#include <algorithm>
#include <cmath>

namespace lunary::injected {

struct Vec2 {
    float x{};
    float y{};
};

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

inline float normalizeAngle(float degrees) {
    if (!std::isfinite(degrees)) return 0.0F;
    degrees = std::fmod(degrees + 180.0F, 360.0F);
    if (degrees < 0.0F) degrees += 360.0F;
    return degrees - 180.0F;
}

inline Vec2 lookAngle(const Vec3& origin, const Vec3& target) {
    constexpr float radiansToDegrees = 57.29577951308232F;
    const float dx = target.x - origin.x;
    const float dy = target.y - origin.y;
    const float dz = target.z - origin.z;
    const float horizontal = std::hypot(dx, dz);
    return {
        std::atan2(dx, dz) * radiansToDegrees,
        -std::atan2(dy, horizontal) * radiansToDegrees,
    };
}

inline Vec2 angleDelta(const Vec2& current, const Vec2& target) {
    return {normalizeAngle(target.x - current.x),
            normalizeAngle(target.y - current.y)};
}

inline Vec2 smoothAngle(const Vec2& current, const Vec2& target, float smoothing) {
    smoothing = std::max(1.0F, smoothing);
    const auto delta = angleDelta(current, target);
    return {normalizeAngle(current.x + delta.x / smoothing),
            std::clamp(current.y + delta.y / smoothing, -89.0F, 89.0F)};
}

inline float distanceSquared(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace lunary::injected
