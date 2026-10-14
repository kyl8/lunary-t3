#include "feature_hooks.hpp"

#include "feature_manager.hpp"
#include "il2cpp_layout.hpp"
#include "inline_detour.hpp"
#include "offsets.hpp"

#include <android/log.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>

namespace lunary::injected {
namespace {

#define FH_LOG(fmt, ...) \
    __android_log_print(ANDROID_LOG_INFO, "LunaryFH", fmt, ##__VA_ARGS__)

void featureDiagnostic(const char* message) {
    const int descriptor = open("/data/local/tmp/lunary-hook.log",
                                O_WRONLY | O_APPEND | O_CLOEXEC);
    if (descriptor < 0) return;
    (void)write(descriptor, message, strlen(message));
    (void)write(descriptor, "\n", 1U);
    close(descriptor);
}

struct Quaternion {
    float x{};
    float y{};
    float z{};
    float w{};
};

struct Vector3 {
    float x{};
    float y{};
    float z{};
};

static_assert(sizeof(Quaternion) == 16);

InlineDetour gRecoilVDetour;
InlineDetour gScatterVDetour;
InlineDetour gScatterRotationDetour;
InlineDetour gClientCameraRecoilDetour;
InlineDetour gServerCameraRecoilDetour;
InlineDetour gShootGapDetour;
InlineDetour gShootDelayDetour;
InlineDetour gSkillCooldownDetour;
InlineDetour gBulletPhysDetour;
InlineDetour gBulletPassDetour;
InlineDetour gNativeBulletPhysDetour;
InlineDetour gNativeBulletPassDetour;
InlineDetour gStaticAcrossDetour;
InlineDetour gCheckStaticBlockDetour;
InlineDetour gPhysicsExecuteDetour;
InlineDetour gSightCoverDetour;
InlineDetour gKnockBackActiveDetour;

std::atomic<void*> gOriginalRecoilV{};
std::atomic<void*> gOriginalScatterV{};
std::atomic<void*> gOriginalScatterRotation{};
std::atomic<void*> gOriginalClientCameraRecoil{};
std::atomic<void*> gOriginalServerCameraRecoil{};
std::atomic<void*> gOriginalShootGap{};
std::atomic<void*> gOriginalShootDelay{};
std::atomic<void*> gOriginalSkillCooldown{};
std::atomic<void*> gOriginalBulletPhys{};
std::atomic<void*> gOriginalBulletPass{};
std::atomic<void*> gOriginalNativeBulletPhys{};
std::atomic<void*> gOriginalNativeBulletPass{};
std::atomic<void*> gOriginalStaticAcross{};
std::atomic<void*> gOriginalCheckStaticBlock{};
std::atomic<void*> gOriginalPhysicsExecute{};
std::atomic<void*> gOriginalSightCover{};
std::atomic<void*> gOriginalKnockBackActive{};
std::atomic<bool> gHooksInstalled{};

using RecoilVFunction = void (*)(void*, void*, int, int, const MethodInfo*);
using ScatterVFunction = void (*)(void*, void*, int, int, const MethodInfo*);
using ScatterRotationFunction = Quaternion (*)(void*, Quaternion, const MethodInfo*);
using ApplyCameraRecoilFunction = void (*)(void*, void*, const MethodInfo*);
using WeaponTimingFunction = float (*)(void*, void*, const MethodInfo*);
using SkillCooldownFunction = bool (*)(void*, int, int, const MethodInfo*);
using BulletPassFunction = bool (*)(void*, void*, bool*, const MethodInfo*);
using NativeBulletPassFunction = bool (*)(void*, void*, void*, bool*, const MethodInfo*);
using StaticAcrossFunction = bool (*)(void*, void*, int, const MethodInfo*);
using CheckStaticBlockFunction = bool (*)(void*, void*, Vector3, int, bool,
                                          Vector3, void*, void*,
                                          const MethodInfo*);
using PhysicsExecuteFunction = void (*)(void*, const MethodInfo*);
using SightCoverFunction = bool (*)(void*, void*, const MethodInfo*);
using KnockBackActiveFunction = bool (*)(void*, int, const MethodInfo*);

void hookedRecoilV(void* recoilConfig, void* recoil, int time, int deltaTime,
                   const MethodInfo* method) {
    static std::atomic<bool> first{true};
    const bool diagnostic = first.exchange(false, std::memory_order_relaxed);
    if (diagnostic) featureDiagnostic("feature hook: DoRecoilV entry");
    const auto original = reinterpret_cast<RecoilVFunction>(
        gOriginalRecoilV.load(std::memory_order_acquire));
    if (original) original(recoilConfig, recoil, time, deltaTime, method);
    if (diagnostic) featureDiagnostic("feature hook: DoRecoilV trampoline returned");
}

void hookedScatterV(void* scatterConfig, void* recoil, int time, int deltaTime,
                    const MethodInfo* method) {
    static std::atomic<bool> first{true};
    const bool diagnostic = first.exchange(false, std::memory_order_relaxed);
    if (diagnostic) featureDiagnostic("feature hook: DoScatterV entry");
    const auto original = reinterpret_cast<ScatterVFunction>(
        gOriginalScatterV.load(std::memory_order_acquire));
    if (original) original(scatterConfig, recoil, time, deltaTime, method);
    if (diagnostic) featureDiagnostic("feature hook: DoScatterV trampoline returned");
}

Quaternion hookedScatterRotation(void* recoil, Quaternion rotation,
                                 const MethodInfo* method) {
    static std::atomic<bool> first{true};
    static std::atomic<bool> compensatedLogged{};
    const bool diagnostic = first.exchange(false, std::memory_order_relaxed);
    if (diagnostic) featureDiagnostic("feature hook: DoScatter entry");
    const auto original = reinterpret_cast<ScatterRotationFunction>(
        gOriginalScatterRotation.load(std::memory_order_acquire));
    const auto result = original ? original(recoil, rotation, method) : rotation;
    // DoScatter consumes Unity's random stream. Skipping it desynchronizes the
    // next shots from the authoritative simulation. Preserve all side effects,
    // then compensate only the returned client shot rotation.
    if (FeatureManager::instance().isEnabled(FeatureId::NoSpread)) {
        if (!compensatedLogged.exchange(true, std::memory_order_relaxed))
            featureDiagnostic("no spread: state-preserving rotation compensation confirmed");
        return rotation;
    }
    if (diagnostic) featureDiagnostic("feature hook: DoScatter trampoline returned");
    return result;
}

void hookedClientCameraRecoil(void* self, void* recoil,
                             const MethodInfo* method) {
    static std::atomic<bool> first{true};
    static std::atomic<bool> bypassLogged{};
    const bool diagnostic = first.exchange(false, std::memory_order_relaxed);
    if (diagnostic) featureDiagnostic("feature hook: client camera recoil entry");
    if (FeatureManager::instance().isEnabled(FeatureId::NoRecoil)) {
        if (!bypassLogged.exchange(true, std::memory_order_relaxed))
            featureDiagnostic("no recoil: client camera bypass confirmed");
        return;
    }
    const auto original = reinterpret_cast<ApplyCameraRecoilFunction>(
        gOriginalClientCameraRecoil.load(std::memory_order_acquire));
    if (original) original(self, recoil, method);
    if (diagnostic) featureDiagnostic("feature hook: client recoil trampoline returned");
}

void hookedServerCameraRecoil(void* self, void* recoil,
                             const MethodInfo* method) {
    static std::atomic<bool> first{true};
    const bool diagnostic = first.exchange(false, std::memory_order_relaxed);
    if (diagnostic) featureDiagnostic("feature hook: server camera recoil entry");
    const auto original = reinterpret_cast<ApplyCameraRecoilFunction>(
        gOriginalServerCameraRecoil.load(std::memory_order_acquire));
    if (original) original(self, recoil, method);
    if (diagnostic) featureDiagnostic("feature hook: server recoil trampoline returned");
}

float scaleLocalWeaponTiming(std::atomic<void*>& originalSlot, void* weaponShoot,
                             void* plugin, const MethodInfo* method,
                             const char* diagnosticMessage) {
    const auto original = reinterpret_cast<WeaponTimingFunction>(
        originalSlot.load(std::memory_order_acquire));
    const float result = original ? original(weaponShoot, plugin, method) : 0.0F;
    auto& manager = FeatureManager::instance();
    // Weapon timings are commonly cached while a match is being constructed,
    // before CameraController publishes the local Entity. Filtering by the
    // current local WeaponShoot pointer made the toggle a no-op for that match.
    if (!manager.isEnabled(FeatureId::RapidFire)) return result;
    static std::atomic<unsigned> logMask{};
    if ((logMask.fetch_or(diagnosticMessage[0] == 'g' ? 1U : 2U) &
         (diagnosticMessage[0] == 'g' ? 1U : 2U)) == 0)
        featureDiagnostic(diagnosticMessage);
    const float multiplier = std::clamp(
        manager.config().fireRateMultiplier, 1.0F, 100.0F);
    return std::isfinite(result) ? result / multiplier : result;
}

float hookedShootGap(void* weaponShoot, void* plugin, const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true)) featureDiagnostic("feature hook: shoot gap entry");
    return scaleLocalWeaponTiming(gOriginalShootGap, weaponShoot, plugin, method,
                                  "gap timing: fire rate bypass confirmed");
}

float hookedShootDelay(void* weaponShoot, void* plugin, const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true)) featureDiagnostic("feature hook: shoot delay entry");
    return scaleLocalWeaponTiming(gOriginalShootDelay, weaponShoot, plugin, method,
                                  "delay timing: fire rate bypass confirmed");
}

bool hookedSkillCooldown(void* entity, int skillType, int time,
                         const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true)) featureDiagnostic("feature hook: skill cooldown entry");
    auto& manager = FeatureManager::instance();
    if (manager.isEnabled(FeatureId::NoSkillCooldown) &&
        manager.isLocalEntity(entity)) {
        static std::atomic<bool> logged{};
        if (!logged.exchange(true)) featureDiagnostic("no skill cooldown: check bypass confirmed");
        return true;
    }
    const auto original = reinterpret_cast<SkillCooldownFunction>(
        gOriginalSkillCooldown.load(std::memory_order_acquire));
    return original && original(entity, skillType, time, method);
}

bool bypassBulletCondition(std::atomic<void*>& originalSlot, void* source,
                           void* target, bool* check,
                           const MethodInfo* method) {
    auto& manager = FeatureManager::instance();
    if (manager.isEnabled(FeatureId::Wallhack) &&
        manager.config().shootThroughWalls) {
        // O retorno informa que o plugin tratou o par; bCheck informa se ele
        // deve bloquear. true/true confirmava a colisao em vez de ignora-la.
        if (check) *check = false;
        static std::atomic<bool> logged{};
        if (!logged.exchange(true)) featureDiagnostic("shoot through walls: condition bypass confirmed");
        return true;
    }
    const auto original = reinterpret_cast<BulletPassFunction>(
        originalSlot.load(std::memory_order_acquire));
    return original && original(source, target, check, method);
}

bool hookedBulletPhys(void* source, void* target, bool* check,
                      const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true)) featureDiagnostic("feature hook: bullet phys entry");
    return bypassBulletCondition(gOriginalBulletPhys, source, target, check, method);
}

bool hookedBulletPass(void* source, void* target, bool* check,
                      const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true)) featureDiagnostic("feature hook: bullet pass entry");
    return bypassBulletCondition(gOriginalBulletPass, source, target, check, method);
}

bool bypassNativeBulletCondition(std::atomic<void*>& originalSlot, void* self,
                                 void* source, void* target, bool* check,
                                 const MethodInfo* method,
                                 const char* diagnosticMessage) {
    auto& manager = FeatureManager::instance();
    manager.observePhysicsSystem(self);
    if (manager.isEnabled(FeatureId::Wallhack) &&
        manager.config().shootThroughWalls) {
        if (check) *check = false;
        static std::atomic<unsigned> logMask{};
        const unsigned bit = diagnosticMessage[0] == 'n' ? 1U : 2U;
        if ((logMask.fetch_or(bit) & bit) == 0)
            featureDiagnostic(diagnosticMessage);
        return true;
    }
    const auto original = reinterpret_cast<NativeBulletPassFunction>(
        originalSlot.load(std::memory_order_acquire));
    return original && original(self, source, target, check, method);
}

bool hookedNativeBulletPhys(void* self, void* source, void* target, bool* check,
                            const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true))
        featureDiagnostic("feature hook: native bullet phys entry");
    return bypassNativeBulletCondition(
        gOriginalNativeBulletPhys, self, source, target, check, method,
        "native bullet phys: shoot-through bypass confirmed");
}

bool hookedNativeBulletPass(void* self, void* source, void* target, bool* check,
                            const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true))
        featureDiagnostic("feature hook: native bullet pass entry");
    return bypassNativeBulletCondition(
        gOriginalNativeBulletPass, self, source, target, check, method,
        "pass-block condition: shoot-through bypass confirmed");
}

bool hookedStaticAcross(void* caster, void* target, int acrossMask,
                        const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true))
        featureDiagnostic("feature hook: static-across test entry");
    auto& manager = FeatureManager::instance();
    if (manager.isEnabled(FeatureId::Wallhack) &&
        manager.config().shootThroughWalls) {
        static std::atomic<bool> logged{};
        if (!logged.exchange(true))
            featureDiagnostic("static-across: shoot-through bypass confirmed");
        return true;
    }
    const auto original = reinterpret_cast<StaticAcrossFunction>(
        gOriginalStaticAcross.load(std::memory_order_acquire));
    return original && original(caster, target, acrossMask, method);
}

bool hookedCheckStaticBlock(void* self, void* caster, Vector3 startPos,
                            int acrossMask, bool ignoreStatic, Vector3 objectPos,
                            void* object, void* entity,
                            const MethodInfo* method) {
    auto& manager = FeatureManager::instance();
    manager.observePhysicsSystem(self);
    static std::atomic<bool> entered{};
    if (!entered.exchange(true))
        featureDiagnostic("feature hook: CheckStaticBlock entry");
    if (manager.isEnabled(FeatureId::Wallhack) &&
        manager.config().shootThroughWalls) {
        ignoreStatic = true;
        static std::atomic<bool> logged{};
        if (!logged.exchange(true))
            featureDiagnostic("static block: shoot-through bypass confirmed");
    }
    const auto original = reinterpret_cast<CheckStaticBlockFunction>(
        gOriginalCheckStaticBlock.load(std::memory_order_acquire));
    return original && original(self, caster, startPos, acrossMask, ignoreStatic,
                                objectPos, object, entity, method);
}

void hookedPhysicsExecute(void* self, const MethodInfo* method) {
    FeatureManager::instance().observePhysicsSystem(self);
    static std::atomic<bool> entered{};
    if (!entered.exchange(true))
        featureDiagnostic("feature hook: PhysicsSystem.Execute entry");
    const auto original = reinterpret_cast<PhysicsExecuteFunction>(
        gOriginalPhysicsExecute.load(std::memory_order_acquire));
    if (original) original(self, method);
}

bool hookedSightCover(void* selfTransform, void* entity,
                      const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true)) featureDiagnostic("feature hook: sight cover entry");
    auto& manager = FeatureManager::instance();
    if (manager.isEnabled(FeatureId::Wallhack) &&
        manager.config().seeThroughWalls) {
        static std::atomic<bool> logged{};
        if (!logged.exchange(true))
            featureDiagnostic("see through walls: cover raycast bypass confirmed");
        return false;
    }
    const auto original = reinterpret_cast<SightCoverFunction>(
        gOriginalSightCover.load(std::memory_order_acquire));
    return original && original(selfTransform, entity, method);
}

bool hookedKnockBackActive(void* entity, int currentTime,
                           const MethodInfo* method) {
    static std::atomic<bool> entered{};
    if (!entered.exchange(true)) featureDiagnostic("feature hook: knockback active entry");
    auto& manager = FeatureManager::instance();
    if (manager.isEnabled(FeatureId::NoKnockback) &&
        manager.isLocalEntity(entity)) {
        static std::atomic<bool> logged{};
        if (!logged.exchange(true)) featureDiagnostic("no knockback: active-state bypass confirmed");
        return false;
    }
    const auto original = reinterpret_cast<KnockBackActiveFunction>(
        gOriginalKnockBackActive.load(std::memory_order_acquire));
    return original && original(entity, currentTime, method);
}

struct HookSpec {
    const char* name{};
    std::uintptr_t rva{};
    void* replacement{};
    InlineDetour* detour{};
    std::atomic<void*>* original{};
};

std::array<HookSpec, 17> hookSpecs() {
    using namespace offsets;
    return {{
        {"WeaponSystem.DoRecoilV", RVA_WeaponSystem_DoRecoilV,
         reinterpret_cast<void*>(&hookedRecoilV), &gRecoilVDetour,
         &gOriginalRecoilV},
        {"WeaponSystem.DoScatterV", RVA_WeaponSystem_DoScatterV,
         reinterpret_cast<void*>(&hookedScatterV), &gScatterVDetour,
         &gOriginalScatterV},
        {"WeaponSystem.DoScatter", RVA_WeaponSystem_DoScatter,
         reinterpret_cast<void*>(&hookedScatterRotation), &gScatterRotationDetour,
         &gOriginalScatterRotation},
        {"CameraControllerClient.ApplyRecoildToCamera",
         RVA_CameraControllerClient_ApplyRecoildToCamera,
         reinterpret_cast<void*>(&hookedClientCameraRecoil),
         &gClientCameraRecoilDetour, &gOriginalClientCameraRecoil},
        {"CameraControllerServer.ApplyRecoildToCamera",
         RVA_CameraControllerServer_ApplyRecoildToCamera,
         reinterpret_cast<void*>(&hookedServerCameraRecoil),
         &gServerCameraRecoilDetour, &gOriginalServerCameraRecoil},
        {"WeaponSystem.GetShootGapTime", RVA_WeaponSystem_GetShootGapTime,
         reinterpret_cast<void*>(&hookedShootGap), &gShootGapDetour,
         &gOriginalShootGap},
        {"WeaponSystem.GetShootDelayTime", RVA_WeaponSystem_GetShootDelayTime,
         reinterpret_cast<void*>(&hookedShootDelay), &gShootDelayDetour,
         &gOriginalShootDelay},
        {"SkillSystem.CheckSkillCD", RVA_SkillSystem_CheckSkillCD,
         reinterpret_cast<void*>(&hookedSkillCooldown), &gSkillCooldownDetour,
         &gOriginalSkillCooldown},
        {"BulletPhysConditionPlugin", RVA_BulletPhysCondition,
         reinterpret_cast<void*>(&hookedBulletPhys), &gBulletPhysDetour,
         &gOriginalBulletPhys},
        {"BulletPassBlockPhysConditionPlugin", RVA_BulletPassBlockCondition,
         reinterpret_cast<void*>(&hookedBulletPass), &gBulletPassDetour,
         &gOriginalBulletPass},
        {"PhysicsSystem.BulletPhysConditionPlugin",
         RVA_PhysicsSystem_BulletPhysCondition,
         reinterpret_cast<void*>(&hookedNativeBulletPhys),
         &gNativeBulletPhysDetour, &gOriginalNativeBulletPhys},
        {"PhysicsSystem.BulletPassBlockPhysConditionPlugin",
         RVA_PhysicsSystem_BulletPassBlockCondition,
         reinterpret_cast<void*>(&hookedNativeBulletPass),
         &gNativeBulletPassDetour, &gOriginalNativeBulletPass},
        {"PhysicsSystem.RaycastStaticAcrossTest",
         RVA_PhysicsSystem_RaycastStaticAcrossTest,
         reinterpret_cast<void*>(&hookedStaticAcross),
         &gStaticAcrossDetour, &gOriginalStaticAcross},
        {"PhysicsSystem.CheckStaticBlock",
         RVA_PhysicsSystem_CheckStaticBlock,
         reinterpret_cast<void*>(&hookedCheckStaticBlock),
         &gCheckStaticBlockDetour, &gOriginalCheckStaticBlock},
        {"PhysicsSystem.Execute", RVA_PhysicsSystem_Execute,
         reinterpret_cast<void*>(&hookedPhysicsExecute),
         &gPhysicsExecuteDetour, &gOriginalPhysicsExecute},
        {"VisualPlayerBlockComponent.IsCoveredBySightCover",
         RVA_IsCoveredBySightCover,
         reinterpret_cast<void*>(&hookedSightCover), &gSightCoverDetour,
         &gOriginalSightCover},
        {"KnockBackComponent.IsKnockBack", RVA_KnockBack_IsActive,
         reinterpret_cast<void*>(&hookedKnockBackActive), &gKnockBackActiveDetour,
         &gOriginalKnockBackActive},
    }};
}

void clearOriginals() {
    gOriginalRecoilV.store(nullptr, std::memory_order_release);
    gOriginalScatterV.store(nullptr, std::memory_order_release);
    gOriginalScatterRotation.store(nullptr, std::memory_order_release);
    gOriginalClientCameraRecoil.store(nullptr, std::memory_order_release);
    gOriginalServerCameraRecoil.store(nullptr, std::memory_order_release);
    gOriginalShootGap.store(nullptr, std::memory_order_release);
    gOriginalShootDelay.store(nullptr, std::memory_order_release);
    gOriginalSkillCooldown.store(nullptr, std::memory_order_release);
    gOriginalBulletPhys.store(nullptr, std::memory_order_release);
    gOriginalBulletPass.store(nullptr, std::memory_order_release);
    gOriginalNativeBulletPhys.store(nullptr, std::memory_order_release);
    gOriginalNativeBulletPass.store(nullptr, std::memory_order_release);
    gOriginalStaticAcross.store(nullptr, std::memory_order_release);
    gOriginalCheckStaticBlock.store(nullptr, std::memory_order_release);
    gOriginalPhysicsExecute.store(nullptr, std::memory_order_release);
    gOriginalSightCover.store(nullptr, std::memory_order_release);
    gOriginalKnockBackActive.store(nullptr, std::memory_order_release);
}

} // namespace

bool installFeatureHooks(std::uintptr_t moduleBase, std::string& error) {
    if (gHooksInstalled.load(std::memory_order_acquire)) return true;
    if (!moduleBase) {
        error = "feature hooks: invalid libil2cpp base";
        return false;
    }

    auto specs = hookSpecs();
    std::size_t installed{};
    for (auto& spec : specs) {
        std::string hookError;
        if (!spec.detour->install(reinterpret_cast<void*>(moduleBase + spec.rva),
                                  spec.replacement, spec.original, hookError)) {
            std::ostringstream message;
            message << spec.name << " @ +0x" << std::hex << spec.rva
                    << ": " << hookError;
            error = message.str();

            for (std::size_t index = installed; index > 0; --index) {
                std::string rollbackError;
                specs[index - 1].detour->uninstall(rollbackError);
            }
            clearOriginals();
            FH_LOG("install failed: %s", error.c_str());
            return false;
        }
        ++installed;
        FH_LOG("detoured %s at +0x%zx", spec.name, spec.rva);
    }

    gHooksInstalled.store(true, std::memory_order_release);
    featureDiagnostic("direct ARM64 hooks installed: 17/17");
    FH_LOG("direct ARM64 hooks installed: %zu/%zu", installed, specs.size());
    return true;
}

void uninstallFeatureHooks(std::string& error) {
    auto specs = hookSpecs();
    bool ok = true;
    for (std::size_t index = specs.size(); index > 0; --index) {
        std::string hookError;
        if (!specs[index - 1].detour->uninstall(hookError)) {
            ok = false;
            if (error.empty()) error = specs[index - 1].name +
                std::string(": ") + hookError;
        }
    }
    if (ok) {
        clearOriginals();
        gHooksInstalled.store(false, std::memory_order_release);
    }
}

} // namespace lunary::injected
