#pragma once
// =============================================================================
// Feature Manager - Centralized cheat feature control
// Manages all 18 cheat features with enable/disable/update lifecycle
// =============================================================================

#include "offsets.hpp"

#include <cstdint>
#include <cstring>
#include <atomic>

namespace lunary::injected {

enum class FeatureId : std::uint8_t {
    GodMode = 0,
    DamageHack,
    SpeedHack,
    FlyHack,
    NoRecoil,
    NoSpread,
    InfiniteAmmo,
    RapidFire,
    Wallhack,
    ESP,
    NoSkillCooldown,
    Teleport,
    InfiniteShield,
    Aimbot,
    InfiniteJump,
    AutoShoot,
    Invisibility,
    NoKnockback,
    Count  // must be last
};

inline constexpr std::uint8_t kFeatureCount = static_cast<std::uint8_t>(FeatureId::Count);

struct FeatureConfig {
    bool enabled{false};

    // God Mode
    int invincibleCount{1};

    // Damage Hack
    float damageMultiplier{1.0f};      // >1 = more damage, 999 = one-hit-kill
    float damageReduceMultiplier{0.0f}; // 1.0 = god mode (no damage taken)

    // Speed Hack
    float moveSpeedMultiplier{1.0f};

    // Fly Hack
    bool enableFly{false};
    bool enableNoGravity{false};
    float flySpeed{10.0f};

    // No Recoil
    bool zeroRecoil{false};

    // No Spread
    bool zeroSpread{false};

    // Infinite Ammo
    int clipSize{0};
    bool noReload{false};

    // Rapid Fire
    float fireRateMultiplier{1.0f};  // 1x = normal

    // Wallhack
    bool shootThroughWalls{false};
    bool seeThroughWalls{false};

    // ESP
    bool espEnabled{false};
    bool espBoxes{true};
    bool espLines{true};
    bool espNames{true};
    bool espHealth{true};
    bool espDistance{true};
    bool espSkeleton{false};
    bool espHeadDot{true};

    // No Skill Cooldown
    bool noSkillCD{false};

    // Teleport
    bool teleportEnabled{false};
    float teleportX{0.0f};
    float teleportY{0.0f};
    float teleportZ{0.0f};
    bool teleportToTarget{false};
    std::uint64_t teleportRequest{};

    // Infinite Shield
    int shieldValue{0};

    // Aimbot
    bool aimbotEnabled{false};
    float aimbotFov{25.0f};
    float aimbotSmoothing{3.0f};
    bool aimbotLockTarget{true};
    bool aimbotIgnoreTeammates{true};
    bool aimbotVisibleCheck{true};
    float aimbotMaxDistance{90.0f};

    // Infinite Jump
    int maxJumps{1};
    bool enableMultiJump{false};

    // Auto Shoot
    bool autoShootEnabled{false};
    float autoShootFov{15.0f};

    // Invisibility
    bool invisibilityEnabled{false};

    // No Knockback
    bool noKnockback{false};
};

class FeatureManager {
public:
    static FeatureManager& instance() {
        static FeatureManager mgr;
        return mgr;
    }

    void setConfig(const FeatureConfig& cfg) {
        config_ = cfg;
    }

    FeatureConfig& config() { return config_; }
    const FeatureConfig& config() const { return config_; }

    bool isEnabled(FeatureId id) const {
        return config_.enabled && featureEnabled_[static_cast<std::uint8_t>(id)];
    }

    void setFeatureEnabled(FeatureId id, bool enabled) {
        featureEnabled_[static_cast<std::uint8_t>(id)] = enabled;
    }

    // Resolve stable Entity getters after libil2cpp.so is available.  Direct
    // getters are required because Entity's static component fields contain
    // runtime IDs; their field offsets are not component IDs.
    void initializeRuntime(std::uintptr_t moduleBase);
    [[nodiscard]] bool isLocalEntity(void* entity) const;
    [[nodiscard]] bool isLocalWeaponShoot(void* component) const;
    void observePhysicsSystem(void* system);
    void observeLocalEntityId(std::uint16_t entityId);
    void observeLocalVisualPlayer(void* visualPlayer);

    // Sync featureEnabled_[] from individual config flags
    void syncFeatureEnabled();


    // Called every frame from the main game loop hook
    void onFrame(void* entity);

    // Called BEFORE CameraUpdate to zero recoil/spread values
    // so ApplyRecoildToCamera reads zeroed values
    void preCameraFrame(void* entity);

    // Called when entering a match
    void onMatchStart();

    // Called when leaving a match
    void onMatchEnd();

    // Get feature name for UI
    static const char* featureName(FeatureId id);

private:
    FeatureManager() {
        for (std::uint8_t i = 0; i < kFeatureCount; ++i) {
            featureEnabled_[i] = false;
        }
    }

    FeatureConfig config_;
    bool featureEnabled_[kFeatureCount]{};

    // Individual feature update functions
    void updateGodMode(void* entity);
    void updateDamageHack(void* entity);
    void updateSpeedHack(void* entity);
    void updateFlyHack(void* entity);
    void updateNoRecoil(void* entity);
    void updateNoSpread(void* entity);
    void updateInfiniteAmmo(void* entity);
    void updateRapidFire(void* entity);
    void updateWallhack(void* entity);
    void updateESP(void* entity);
    void updateNoSkillCooldown(void* entity);
    void updateTeleport(void* entity);
    void updateInfiniteShield(void* entity);
    void updateAimbot(void* entity);
    void updateInfiniteJump(void* entity);
    void updateAutoShoot(void* entity);
    void updateInvisibility(void* entity);
    void updateNoKnockback(void* entity);
};

}  // namespace lunary::injected
