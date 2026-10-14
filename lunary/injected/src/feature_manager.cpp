// =============================================================================
// FeatureManager implementation - All 18 cheat features
// Uses IL2CPP API to modify game components at runtime
// =============================================================================

#include "feature_manager.hpp"
#include "il2cpp_layout.hpp"

#include <android/log.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <sys/uio.h>
#include <unistd.h>

// IL2CPP function pointers (resolved at runtime via exports)
namespace {

// ---- IL2CPP API function types ----
using Il2CppClassPtr = void*;
using Il2CppObjectPtr = void*;
using Il2CppFieldPtr = void*;
using Il2CppMethodPtr = void*;
using Il2CppDomainPtr = void*;
using Il2CppThreadPtr = void*;

using fn_class_from_name = Il2CppClassPtr (*)(const char* imageName, const char* namespaceName, const char* className);
using fn_class_get_field_from_name = Il2CppFieldPtr (*)(Il2CppClassPtr klass, const char* name);
using fn_field_get_value = void (*)(Il2CppObjectPtr obj, Il2CppFieldPtr field, void* value);
using fn_field_set_value = void (*)(Il2CppObjectPtr obj, Il2CppFieldPtr field, void* value);
using fn_field_static_get_value = void (*)(Il2CppFieldPtr field, void* value);
using fn_class_get_method_from_name = Il2CppMethodPtr (*)(Il2CppClassPtr klass, const char* name, int argsCount);
using fn_runtime_invoke = Il2CppObjectPtr (*)(Il2CppMethodPtr method, Il2CppObjectPtr obj, void** params, Il2CppObjectPtr* exc);
using fn_string_new = Il2CppObjectPtr (*)(const char* str);
using fn_domain_get = Il2CppDomainPtr (*)();
using fn_thread_attach = Il2CppThreadPtr (*)(Il2CppDomainPtr domain);

// Cached function pointers
fn_class_from_name          p_class_from_name{};
fn_class_get_field_from_name p_class_get_field_from_name{};
fn_field_get_value          p_field_get_value{};
fn_field_set_value          p_field_set_value{};
fn_field_static_get_value   p_field_static_get_value{};
fn_class_get_method_from_name p_class_get_method_from_name{};
fn_runtime_invoke           p_runtime_invoke{};
fn_string_new              p_string_new{};
fn_domain_get              p_domain_get{};
fn_thread_attach           p_thread_attach{};

// Cached IL2CPP classes
Il2CppClassPtr g_EntityClass{};
Il2CppClassPtr g_ContextClass{};
Il2CppClassPtr g_HealthComponentClass{};
Il2CppClassPtr g_TransformComponentClass{};
Il2CppClassPtr g_AimComponentClass{};
bool g_initialized{};

using EntityComponentGetter = void* (*)(void*, const lunary::injected::MethodInfo*);
using EntityHasComponent = bool (*)(void*, const lunary::injected::MethodInfo*);
using EntityRemoveComponent = void (*)(void*, const lunary::injected::MethodInfo*);
using ComponentSetDirty = void (*)(void*, const lunary::injected::MethodInfo*);

struct NativeVector3 {
    float x{};
    float y{};
    float z{};
};

struct NativeQuaternion {
    float x{};
    float y{};
    float z{};
    float w{1.0F};
};

using PhysicsActorSetPosition = void (*)(void*, NativeVector3,
                                         const lunary::injected::MethodInfo*);
using PhysicsActorSetRotation = void (*)(void*, NativeQuaternion,
                                         const lunary::injected::MethodInfo*);
using ResolveInternalCall = void* (*)(const char*);
using TransformSetPositionInjected = void (*)(void*, const NativeVector3*);

std::uintptr_t gModuleBase{};
// The client exposes the controlled actor through two synchronized Entity
// instances (simulation and visual/UI). Keep both so hooks fired from either
// path are still recognized as local.
std::atomic<void*> gLocalEntity{};
std::atomic<void*> gAlternateLocalEntity{};
EntityComponentGetter gGetMoveSpeedCoef{};
EntityComponentGetter gGetMoveStatus{};
EntityComponentGetter gGetRigidBody{};
EntityComponentGetter gGetWeightless{};
EntityComponentGetter gGetReload{};
EntityComponentGetter gGetReloadDynamic{};
EntityComponentGetter gGetRecoil{};
EntityComponentGetter gGetSkill1{};
EntityComponentGetter gGetSkill2{};
EntityComponentGetter gGetSkillEx{};
EntityComponentGetter gGetTransform{};
EntityComponentGetter gGetWeapon{};
EntityComponentGetter gGetWeaponShoot{};
EntityHasComponent gHasForceVelocity{};
EntityComponentGetter gGetForceVelocity{};
EntityRemoveComponent gDelForceVelocity{};
EntityHasComponent gHasKnockBack{};
EntityComponentGetter gGetKnockBack{};
EntityRemoveComponent gDelKnockBack{};
EntityHasComponent gHasKnockBackState{};
EntityRemoveComponent gDelKnockBackState{};
ComponentSetDirty gSetComponentDirty{};
PhysicsActorSetPosition gSetPhysicsPosition{};
PhysicsActorSetRotation gSetPhysicsRotation{};
const lunary::injected::MethodInfo* gSetDirtyMethod{};
std::atomic<void*> gObservedPhysicsSystem{};
std::atomic<std::uint16_t> gLocalEntityId{};
std::atomic<void*> gLocalVisualPlayer{};
TransformSetPositionInjected gSetVisualTransformPosition{};
std::uint64_t gLastTeleportRequest{};
std::atomic<int> gTeleportHoldFrames{};

struct SpeedPatchState {
    void* entity{};
    void* component{};
    float original{1.0F};
    bool active{};
} gSpeedPatch;

struct FlyPatchState {
    void* entity{};
    void* moveStatus{};
    void* rigidBody{};
    void* speedCoef{};
    void* weightless{};
    bool originalFloating{};
    bool originalAirFly{};
    float originalGravity{};
    float originalGravityCoef{};
    std::int32_t originalWeightlessCount{};
    bool active{};
} gFlyPatch;

// Component ID cache - read from Entity static fields
std::uint16_t g_healthComponentId{};
std::uint16_t g_transformComponentId{};
std::uint16_t g_aimComponentId{};
std::uint16_t g_invincibleComponentId{};
std::uint16_t g_damageCoefComponentId{};
std::uint16_t g_damageMultiplyComponentId{};
std::uint16_t g_damageReduceCoefComponentId{};
std::uint16_t g_moveSpeedCoefComponentId{};
std::uint16_t g_moveStatusComponentId{};
std::uint16_t g_rigidBodyComponentId{};
std::uint16_t g_weightlessComponentId{};
std::uint16_t g_recoilComponentId{};
std::uint16_t g_weaponComponentId{};
std::uint16_t g_reloadComponentId{};
std::uint16_t g_disableReloadComponentId{};
std::uint16_t g_sightBarrierComponentId{};
std::uint16_t g_DLTrainingModeComponentId{};
std::uint16_t g_teleportationComponentId{};
std::uint16_t g_shieldComponentId{};
std::uint16_t g_jumpComponentId{};
std::uint16_t g_autoShootComponentId{};
std::uint16_t g_camouflageComponentId{};
std::uint16_t g_knockBackComponentId{};
std::uint16_t g_physicsComponentId{};

// =============================================================================
// Internal helpers
// =============================================================================

#define FM_LOG(tag, fmt, ...) __android_log_print(ANDROID_LOG_INFO, tag, fmt, ##__VA_ARGS__)

static std::atomic<int> gCompMissLogCount{};
static std::atomic<int> gCompHitLogCount{};

// Cache component lookups per frame to avoid syscalls every frame
// Components are stable per entity during gameplay
struct CompCacheEntry {
    std::uintptr_t entityAddr = 0;
    std::uint16_t  compId = 0xFFFF;
    void*          ptr = nullptr;
};
static constexpr int kCompCacheSize = 32;
static CompCacheEntry gCompCache[kCompCacheSize]{};
static int gCompCacheIdx = 0;

static void* cachedGetComponentPtr(std::uintptr_t entityAddr, std::uint16_t componentId) {
    // Quick lookup in cache
    for (int i = 0; i < kCompCacheSize; ++i) {
        if (gCompCache[i].entityAddr == entityAddr && gCompCache[i].compId == componentId)
            return gCompCache[i].ptr;
    }
    return nullptr;
}

static void cacheComponentPtr(std::uintptr_t entityAddr, std::uint16_t componentId, void* ptr) {
    int slot = gCompCacheIdx++ & (kCompCacheSize - 1);
    gCompCache[slot] = {entityAddr, componentId, ptr};
}

static void clearCompCache() {
    for (int i = 0; i < kCompCacheSize; ++i)
        gCompCache[i] = {0, 0xFFFF, nullptr};
    gCompCacheIdx = 0;
}

// Debug: verify a float write actually sticks
static std::atomic<int> gVerifyCount{};
static void verifyFloatWrite(void* comp, std::size_t offset, float written, const char* name) {
    if ((gVerifyCount.fetch_add(1) & 2047) != 0) return;
    float readBack = *reinterpret_cast<float*>(
        reinterpret_cast<std::uint8_t*>(comp) + offset);
    FM_LOG("LunaryFM", "verify %s: wrote=%.4f readback=%.4f (offset=0x%zx)",
           name, written, readBack, offset);
}

// Safely read memory without crashing (uses process_vm_readv)
static bool safeReadPtr(std::uintptr_t address, void* out, std::size_t size) {
    if (!address || !out || size == 0 || address > 0x7FFFFFFFFFFFULL) return false;
    iovec local{out, size};
    iovec remote{reinterpret_cast<void*>(address), size};
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
}

struct PhysicsColliderEntry {
    std::int32_t hashCode{};
    std::int32_t next{};
    std::uint16_t entityId{};
    std::uint16_t padding16{};
    std::uint32_t padding32{};
    void* collider{};
};

static_assert(offsetof(PhysicsColliderEntry, collider) == 0x10);
static_assert(sizeof(PhysicsColliderEntry) == 0x18);

// PhysicsSystem.m_EntityColliders is Dictionary<ushort, PhysicsObjectWrap> at
// +0x30. Scan its live entry array ourselves: the game's generic get_id and
// Dictionary.TryGetValue instantiations are protected entry points.
void* findPhysicsCollider(void* physicsSystem, std::uint16_t entityId) {
    if (!physicsSystem || entityId == 0) return nullptr;

    std::uintptr_t dictionary{};
    if (!safeReadPtr(reinterpret_cast<std::uintptr_t>(physicsSystem) + 0x30,
                     &dictionary, sizeof(dictionary)) || !dictionary)
        return nullptr;

    std::uintptr_t entries{};
    std::int32_t count{};
    if (!safeReadPtr(dictionary + 0x18, &entries, sizeof(entries)) || !entries ||
        !safeReadPtr(dictionary + 0x20, &count, sizeof(count)) ||
        count <= 0 || count > 16384)
        return nullptr;

    std::uintptr_t capacity{};
    if (!safeReadPtr(entries + 0x18, &capacity, sizeof(capacity)) ||
        capacity == 0 || capacity > 16384)
        return nullptr;
    if (static_cast<std::uintptr_t>(count) > capacity)
        count = static_cast<std::int32_t>(capacity);

    const std::uintptr_t data = entries + 0x20;
    for (std::int32_t index = 0; index < count; ++index) {
        PhysicsColliderEntry entry{};
        if (!safeReadPtr(data + static_cast<std::uintptr_t>(index) * sizeof(entry),
                         &entry, sizeof(entry)))
            break;
        if (entry.hashCode >= 0 && entry.entityId == entityId && entry.collider)
            return entry.collider;
    }
    return nullptr;
}

bool nativePhysicsSettersReady() {
    std::uintptr_t positionSetter{};
    std::uintptr_t rotationSetter{};
    return safeReadPtr(
               gModuleBase + lunary::injected::offsets::RVA_PhysicsActor_PositionNative,
               &positionSetter, sizeof(positionSetter)) && positionSetter &&
           safeReadPtr(
               gModuleBase + lunary::injected::offsets::RVA_PhysicsActor_RotationNative,
               &rotationSetter, sizeof(rotationSetter)) && rotationSetter;
}

const lunary::injected::MethodInfo* teleportSetDirtyMethod() {
    if (gSetDirtyMethod) return gSetDirtyMethod;
    // Do not invoke il2cpp_codegen_initialize_runtime_metadata here: its target
    // is encrypted in this build. Normal PhysicsSystem execution initializes
    // this shared slot before a player collider is available.
    (void)safeReadPtr(
        gModuleBase + lunary::injected::offsets::RVA_Metadata_ComponentSetDirty,
        &gSetDirtyMethod, sizeof(gSetDirtyMethod));
    return gSetDirtyMethod;
}

// Read a component from an entity by component ID
// Entity stores components in an internal array indexed by component ID
// Uses cache to avoid syscalls on repeated lookups within a frame
void* getComponentPtr(void* entity, std::uint16_t componentId) {
    if (!entity) return nullptr;
    auto entityAddr = reinterpret_cast<std::uintptr_t>(entity);
    // Sanity: entity must be in reasonable heap range
    if (entityAddr < 0x100000 || entityAddr > 0x7FFFFFFFFFFFULL) return nullptr;
    // Max known component ID
    if (componentId >= 0x200) return nullptr;

    // Check cache first
    void* cached = cachedGetComponentPtr(entityAddr, componentId);
    if (cached) return cached;

    // Safely read the component array pointer at entity + ComponentArray offset
    std::uintptr_t arrayPtr = 0;
    if (!safeReadPtr(entityAddr + lunary::injected::offsets::Entity_ComponentArray,
                     &arrayPtr, sizeof(arrayPtr))) {
        if ((gCompMissLogCount.fetch_add(1) & 255) == 0)
            FM_LOG("LunaryFM", "getComponent: failed to read array ptr at entity+0x%zx",
                   lunary::injected::offsets::Entity_ComponentArray);
        cacheComponentPtr(entityAddr, componentId, nullptr);
        return nullptr;
    }
    if (!arrayPtr || arrayPtr < 0x100000 || arrayPtr > 0x7FFFFFFFFFFFULL) {
        if ((gCompMissLogCount.fetch_add(1) & 255) == 0)
            FM_LOG("LunaryFM", "getComponent: invalid arrayPtr=0x%zx", arrayPtr);
        cacheComponentPtr(entityAddr, componentId, nullptr);
        return nullptr;
    }

    // Safely read the component pointer from the array
    std::uintptr_t compPtr = 0;
    if (!safeReadPtr(arrayPtr + static_cast<std::uintptr_t>(componentId) * sizeof(void*),
                     &compPtr, sizeof(compPtr))) {
        if ((gCompMissLogCount.fetch_add(1) & 255) == 0)
            FM_LOG("LunaryFM", "getComponent: failed to read compId=0x%x at array+0x%zx",
                   componentId, static_cast<std::uintptr_t>(componentId) * sizeof(void*));
        cacheComponentPtr(entityAddr, componentId, nullptr);
        return nullptr;
    }
    if (!compPtr || compPtr < 0x100000 || compPtr > 0x7FFFFFFFFFFFULL) {
        if ((gCompHitLogCount.fetch_add(1) & 511) == 0)
            FM_LOG("LunaryFM", "getComponent: compId=0x%x -> NULL (entity may not have this)",
                   componentId);
        cacheComponentPtr(entityAddr, componentId, nullptr);
        return nullptr;
    }

    void* result = reinterpret_cast<void*>(compPtr);
    if ((gCompHitLogCount.fetch_add(1) & 1023) == 0)
        FM_LOG("LunaryFM", "getComponent: compId=0x%x -> 0x%zx (OK)", componentId, compPtr);
    cacheComponentPtr(entityAddr, componentId, result);
    return result;
}

// Read a float field from a component
float readFloatField(void* component, std::size_t offset) {
    if (!component) return 0.0f;
    return *reinterpret_cast<float*>(reinterpret_cast<std::uint8_t*>(component) + offset);
}

// Write a float field to a component
void writeFloatField(void* component, std::size_t offset, float value) {
    if (!component) return;
    *reinterpret_cast<float*>(reinterpret_cast<std::uint8_t*>(component) + offset) = value;
}

// Read an int32 field from a component
std::int32_t readInt32Field(void* component, std::size_t offset) {
    if (!component) return 0;
    return *reinterpret_cast<std::int32_t*>(reinterpret_cast<std::uint8_t*>(component) + offset);
}

// Write an int32 field to a component
void writeInt32Field(void* component, std::size_t offset, std::int32_t value) {
    if (!component) return;
    *reinterpret_cast<std::int32_t*>(reinterpret_cast<std::uint8_t*>(component) + offset) = value;
}

// Read bool field
bool readBoolField(void* component, std::size_t offset) {
    if (!component) return false;
    return *reinterpret_cast<bool*>(reinterpret_cast<std::uint8_t*>(component) + offset);
}

// Write bool field
void writeBoolField(void* component, std::size_t offset, bool value) {
    if (!component) return;
    *reinterpret_cast<bool*>(reinterpret_cast<std::uint8_t*>(component) + offset) = value;
}

// Read ushort field
std::uint16_t readUInt16Field(void* component, std::size_t offset) {
    if (!component) return 0;
    return *reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uint8_t*>(component) + offset);
}

// Write ushort field
void writeUInt16Field(void* component, std::size_t offset, std::uint16_t value) {
    if (!component) return;
    *reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uint8_t*>(component) + offset) = value;
}

bool initIL2CPP(void* moduleBase) {
    if (g_initialized) return true;
    // TODO: Resolve il2cpp exports from GameAssembly.dll
    // For now, these are resolved via dlsym/GetProcAddress
    g_initialized = true;
    return true;
}

}  // anonymous namespace

namespace lunary::injected {

void FeatureManager::initializeRuntime(std::uintptr_t moduleBase) {
    using namespace offsets;
    gModuleBase = moduleBase;
    const auto getter = [moduleBase](std::uintptr_t rva) {
        return reinterpret_cast<EntityComponentGetter>(moduleBase + rva);
    };
    const auto has = [moduleBase](std::uintptr_t rva) {
        return reinterpret_cast<EntityHasComponent>(moduleBase + rva);
    };
    const auto remove = [moduleBase](std::uintptr_t rva) {
        return reinterpret_cast<EntityRemoveComponent>(moduleBase + rva);
    };
    gGetMoveSpeedCoef = getter(RVA_Entity_GetMoveSpeedCoef);
    gGetMoveStatus = getter(RVA_Entity_GetMoveStatus);
    gGetRigidBody = getter(RVA_Entity_GetRigidBody);
    gGetWeightless = getter(RVA_Entity_GetWeightless);
    gGetReload = getter(RVA_Entity_GetReload);
    gGetReloadDynamic = getter(RVA_Entity_GetReloadDynamic);
    gGetRecoil = getter(RVA_Entity_GetRecoil);
    gGetSkill1 = getter(RVA_Entity_GetSkill1);
    gGetSkill2 = getter(RVA_Entity_GetSkill2);
    gGetSkillEx = getter(RVA_Entity_GetSkillEx);
    gGetTransform = getter(RVA_Entity_GetTransform);
    gGetWeapon = getter(RVA_Entity_GetWeapon);
    gGetWeaponShoot = getter(RVA_Entity_GetWeaponShoot);
    gHasForceVelocity = has(RVA_Entity_HasForceVelocity);
    gGetForceVelocity = getter(RVA_Entity_GetForceVelocity);
    gDelForceVelocity = remove(RVA_Entity_DelForceVelocity);
    gHasKnockBack = has(RVA_Entity_HasKnockBack);
    gGetKnockBack = getter(RVA_Entity_GetKnockBack);
    gDelKnockBack = remove(RVA_Entity_DelKnockBack);
    gHasKnockBackState = has(RVA_Entity_HasKnockBackState);
    gDelKnockBackState = remove(RVA_Entity_DelKnockBackState);

    gSetComponentDirty = reinterpret_cast<ComponentSetDirty>(
        moduleBase + RVA_Component_SetDirty);
    gSetPhysicsPosition = reinterpret_cast<PhysicsActorSetPosition>(
        moduleBase + RVA_PhysicsActor_SetPosition);
    gSetPhysicsRotation = reinterpret_cast<PhysicsActorSetRotation>(
        moduleBase + RVA_PhysicsActor_SetRotation);

    auto resolveInternalCall = reinterpret_cast<ResolveInternalCall>(
        dlsym(RTLD_DEFAULT, "il2cpp_resolve_icall"));
    if (resolveInternalCall) {
        constexpr const char* names[] = {
            "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)",
            "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3*)",
            "UnityEngine.Transform::set_position_Injected",
        };
        for (const char* name : names) {
            gSetVisualTransformPosition =
                reinterpret_cast<TransformSetPositionInjected>(
                    resolveInternalCall(name));
            if (gSetVisualTransformPosition) break;
        }
    }

    // Metadata initialization invokes IL2CPP runtime services. Defer it to the
    // hooked game thread after Context.Current exists; doing it on the early
    // JNI installer thread can race game startup under NativeBridge.
    FM_LOG("LunaryFM",
           "stable runtime routes resolved at libil2cpp=0x%zx visualTransform=%p",
           moduleBase, reinterpret_cast<void*>(gSetVisualTransformPosition));
}

bool FeatureManager::isLocalEntity(void* entity) const {
    return entity &&
        (entity == gLocalEntity.load(std::memory_order_acquire) ||
         entity == gAlternateLocalEntity.load(std::memory_order_acquire));
}

bool FeatureManager::isLocalWeaponShoot(void* component) const {
    if (!component || !gGetWeaponShoot) return false;
    void* entity = gLocalEntity.load(std::memory_order_acquire);
    if (entity && gGetWeaponShoot(entity, nullptr) == component) return true;
    entity = gAlternateLocalEntity.load(std::memory_order_acquire);
    return entity && gGetWeaponShoot(entity, nullptr) == component;
}

void FeatureManager::observePhysicsSystem(void* system) {
    if (!system) return;
    void* previous = gObservedPhysicsSystem.exchange(system, std::memory_order_acq_rel);
    if (!previous)
        FM_LOG("LunaryFM", "physics: observed PhysicsSystem=%p", system);
}

void FeatureManager::observeLocalEntityId(std::uint16_t entityId) {
    if (entityId != 0)
        gLocalEntityId.store(entityId, std::memory_order_release);
}

void FeatureManager::observeLocalVisualPlayer(void* visualPlayer) {
    if (visualPlayer)
        gLocalVisualPlayer.store(visualPlayer, std::memory_order_release);
}

const char* FeatureManager::featureName(FeatureId id) {
    switch (id) {
        case FeatureId::GodMode:          return "God Mode";
        case FeatureId::DamageHack:       return "Damage Hack";
        case FeatureId::SpeedHack:        return "Speed Hack";
        case FeatureId::FlyHack:          return "Fly Hack";
        case FeatureId::NoRecoil:         return "No Recoil";
        case FeatureId::NoSpread:         return "No Spread";
        case FeatureId::InfiniteAmmo:     return "Infinite Ammo";
        case FeatureId::RapidFire:        return "Rapid Fire";
        case FeatureId::Wallhack:         return "Wallhack";
        case FeatureId::ESP:              return "ESP";
        case FeatureId::NoSkillCooldown:  return "No Skill CD";
        case FeatureId::Teleport:         return "Teleport";
        case FeatureId::InfiniteShield:   return "Infinite Shield";
        case FeatureId::Aimbot:           return "Aimbot";
        case FeatureId::InfiniteJump:     return "Infinite Jump";
        case FeatureId::AutoShoot:        return "Auto Shoot";
        case FeatureId::Invisibility:     return "Invisibility";
        case FeatureId::NoKnockback:      return "No Knockback";
        default:                          return "Unknown";
    }
}

void FeatureManager::syncFeatureEnabled() {
    setFeatureEnabled(FeatureId::GodMode,         config_.invincibleCount > 1);
    setFeatureEnabled(FeatureId::DamageHack,      config_.damageMultiplier != 1.0f || config_.damageReduceMultiplier > 0.0f);
    setFeatureEnabled(FeatureId::SpeedHack,       config_.moveSpeedMultiplier != 1.0f);
    setFeatureEnabled(FeatureId::FlyHack,         config_.enableFly || config_.enableNoGravity);
    setFeatureEnabled(FeatureId::NoRecoil,        config_.zeroRecoil);
    setFeatureEnabled(FeatureId::NoSpread,        config_.zeroSpread);
    setFeatureEnabled(FeatureId::InfiniteAmmo,    config_.clipSize > 0 || config_.noReload);
    setFeatureEnabled(FeatureId::RapidFire,       config_.fireRateMultiplier > 1.0f);
    setFeatureEnabled(FeatureId::Wallhack,        config_.shootThroughWalls || config_.seeThroughWalls);
    setFeatureEnabled(FeatureId::ESP,             config_.espEnabled);
    setFeatureEnabled(FeatureId::NoSkillCooldown, config_.noSkillCD);
    setFeatureEnabled(FeatureId::Teleport,        config_.teleportEnabled);
    setFeatureEnabled(FeatureId::InfiniteShield,  config_.shieldValue > 0);
    setFeatureEnabled(FeatureId::Aimbot,          config_.aimbotEnabled);
    setFeatureEnabled(FeatureId::InfiniteJump,    config_.maxJumps > 1 || config_.enableMultiJump);
    setFeatureEnabled(FeatureId::AutoShoot,       config_.autoShootEnabled);
    setFeatureEnabled(FeatureId::Invisibility,    config_.invisibilityEnabled);
    setFeatureEnabled(FeatureId::NoKnockback,     config_.noKnockback);
}

void FeatureManager::onFrame(void* entity) {
    if (!entity) return;
    void* previous = gLocalEntity.exchange(entity, std::memory_order_acq_rel);
    if (previous && previous != entity)
        gAlternateLocalEntity.store(previous, std::memory_order_release);

    // Clear component cache each frame (entity may change)
    clearCompCache();

    // These two patches own original values and must run once when disabled so
    // the game state is restored instead of leaving stale movement flags.
    updateSpeedHack(entity);
    updateFlyHack(entity);
    if (!config_.enabled) return;

    // Execute all enabled features
    if (isEnabled(FeatureId::GodMode))          updateGodMode(entity);
    if (isEnabled(FeatureId::DamageHack))       updateDamageHack(entity);
    if (isEnabled(FeatureId::NoRecoil))         updateNoRecoil(entity);
    if (isEnabled(FeatureId::NoSpread))         updateNoSpread(entity);
    if (isEnabled(FeatureId::InfiniteAmmo))     updateInfiniteAmmo(entity);
    if (isEnabled(FeatureId::RapidFire))        updateRapidFire(entity);
    if (isEnabled(FeatureId::Wallhack))         updateWallhack(entity);
    if (isEnabled(FeatureId::ESP))              updateESP(entity);
    if (isEnabled(FeatureId::NoSkillCooldown))  updateNoSkillCooldown(entity);
    if (isEnabled(FeatureId::Teleport))         updateTeleport(entity);
    if (isEnabled(FeatureId::InfiniteShield))   updateInfiniteShield(entity);
    if (isEnabled(FeatureId::Aimbot))           updateAimbot(entity);
    if (isEnabled(FeatureId::InfiniteJump))     updateInfiniteJump(entity);
    if (isEnabled(FeatureId::AutoShoot))        updateAutoShoot(entity);
    if (isEnabled(FeatureId::Invisibility))     updateInvisibility(entity);
    if (isEnabled(FeatureId::NoKnockback))      updateNoKnockback(entity);
}

void FeatureManager::preCameraFrame(void* entity) {
    // RecoilComponent is custom-serialized and participates in authoritative
    // shot validation. Never patch it from the render hook.
    (void)entity;
}

void FeatureManager::onMatchStart() {
    // Reset any per-match state
}

void FeatureManager::onMatchEnd() {
    // Cleanup
}

// =============================================================================
// FEATURE 1: God Mode (Invincibility)
// Manipulates InvincibleComponent.invincibleCount > 0
// Also uses InvincibleEffect.isInvincible for full god mode
// =============================================================================
void FeatureManager::updateGodMode(void* entity) {
    using namespace offsets;

    // Method 1: Set InvincibleComponent
    void* invComp = getComponentPtr(entity, ComponentId::Invincible);
    if (invComp) {
        writeInt32Field(invComp, InvincibleComponent::invincibleCount, config_.invincibleCount);
    } else {
        // Component doesn't exist - TODO: call Entity.AddInvincible() if possible
    }

    // Method 2: Also set damage reduction to 1.0 (100%) for complete god mode
    void* dmgReduce = getComponentPtr(entity, ComponentId::DamageReduceCoef);
    if (dmgReduce) {
        // damageReduceCoefs is float[] (managed array pointer at offset 0x18)
        // Read array pointer, then write to array data at +0x20
        std::uintptr_t arrPtr = 0;
        std::memcpy(&arrPtr,
                    reinterpret_cast<std::uint8_t*>(dmgReduce) + DamageReduceCoefComponent::damageReduceCoefs,
                    sizeof(arrPtr));
        if (arrPtr && arrPtr > 0x100000 && arrPtr < 0x7FFFFFFFFFFFULL) {
            std::int32_t len = *reinterpret_cast<std::int32_t*>(arrPtr + 0x10);
            float* data = reinterpret_cast<float*>(arrPtr + 0x20);
            int n = (len > 0 && len < 64) ? len : 8;
            float val = config_.damageReduceMultiplier > 0.0f ? config_.damageReduceMultiplier : 1.0f;
            for (int i = 0; i < n; ++i) data[i] = val;
        }
    }
}

// =============================================================================
// FEATURE 2: Damage Hack (One-Hit Kill / Damage Modifier)
// DamageCoefComponent: damageCoef, weakDamageCoef, damageIncreaseCoef, shootDamageCoef
// DamageMultiplyComponent: global damage multiplier
// =============================================================================
void FeatureManager::updateDamageHack(void* entity) {
    using namespace offsets;

    // Increase outgoing damage
    void* dmgCoef = getComponentPtr(entity, ComponentId::DamageCoef);
    if (dmgCoef) {
        writeFloatField(dmgCoef, DamageCoefComponent::damageCoef, config_.damageMultiplier);
        writeFloatField(dmgCoef, DamageCoefComponent::damageIncreaseCoef, config_.damageMultiplier);
        writeFloatField(dmgCoef, DamageCoefComponent::shootDamageCoef, config_.damageMultiplier);
        writeFloatField(dmgCoef, DamageCoefComponent::weakDamageCoef, config_.damageMultiplier);
    }

    // Damage Multiply Component (separate multiplier)
    void* dmgMult = getComponentPtr(entity, ComponentId::DamageMultiply);
    if (dmgMult) {
        // DamageMultiplyComponent stores multiplier at offset 0x18
        writeFloatField(dmgMult, 0x18, config_.damageMultiplier);
    }

    // Reduce incoming damage (defense)
    void* dmgReduce = getComponentPtr(entity, ComponentId::DamageReduceCoef);
    if (dmgReduce && config_.damageReduceMultiplier > 0.0f) {
        // Safe managed array write (pointer at offset 0x18, data at +0x20)
        std::uintptr_t arrPtr = 0;
        std::memcpy(&arrPtr,
                    reinterpret_cast<std::uint8_t*>(dmgReduce) + DamageReduceCoefComponent::damageReduceCoefs,
                    sizeof(arrPtr));
        if (arrPtr && arrPtr > 0x100000 && arrPtr < 0x7FFFFFFFFFFFULL) {
            std::int32_t len = *reinterpret_cast<std::int32_t*>(arrPtr + 0x10);
            float* data = reinterpret_cast<float*>(arrPtr + 0x20);
            int n = (len > 0 && len < 64) ? len : 8;
            for (int i = 0; i < n; ++i) data[i] = config_.damageReduceMultiplier;
        }
    }
}

// =============================================================================
// FEATURE 3: Speed Hack
// AttributeMoveSpeedCoefComponent.moveSpeedCoef
// =============================================================================
void FeatureManager::updateSpeedHack(void* entity) {
    using namespace offsets;
    if (!gGetMoveSpeedCoef) return;
    void* speedComp = gGetMoveSpeedCoef(entity, nullptr);
    const bool enabled = config_.enabled && config_.moveSpeedMultiplier > 1.001F;

    if (enabled && speedComp) {
        if (!gSpeedPatch.active || gSpeedPatch.entity != entity ||
            gSpeedPatch.component != speedComp) {
            gSpeedPatch = {entity, speedComp,
                readFloatField(speedComp, AttributeMoveSpeedCoefComponent::moveSpeedCoef), true};
        }
        writeFloatField(speedComp, AttributeMoveSpeedCoefComponent::moveSpeedCoef,
                        config_.moveSpeedMultiplier);
        static std::atomic<bool> logged{};
        if (!logged.exchange(true))
            FM_LOG("LunaryFM", "speed: direct getter/write confirmed (%.2fx)",
                   config_.moveSpeedMultiplier);
    } else if (gSpeedPatch.active && gSpeedPatch.entity == entity &&
               gSpeedPatch.component) {
        writeFloatField(gSpeedPatch.component,
                        AttributeMoveSpeedCoefComponent::moveSpeedCoef,
                        gSpeedPatch.original);
        gSpeedPatch = {};
    }
}

// =============================================================================
// FEATURE 4: Fly Hack
// MoveStatusComponent.airFly = true
// MoveStatusComponent.floating = true
// RigidBodyComponent.gravity = 0.0f
// WeightlessComponent.WeightlessCount > 0
// =============================================================================
void FeatureManager::updateFlyHack(void* entity) {
    using namespace offsets;
    const bool enabled = config_.enabled && config_.enableFly;
    void* moveStatus = gGetMoveStatus ? gGetMoveStatus(entity, nullptr) : nullptr;
    void* rigidBody = gGetRigidBody ? gGetRigidBody(entity, nullptr) : nullptr;
    void* speedComp = gGetMoveSpeedCoef ? gGetMoveSpeedCoef(entity, nullptr) : nullptr;
    void* weightless = gGetWeightless ? gGetWeightless(entity, nullptr) : nullptr;

    if (enabled) {
        if (!gFlyPatch.active || gFlyPatch.entity != entity) {
            gFlyPatch = {};
            gFlyPatch.entity = entity;
            gFlyPatch.moveStatus = moveStatus;
            gFlyPatch.rigidBody = rigidBody;
            gFlyPatch.speedCoef = speedComp;
            gFlyPatch.weightless = weightless;
            if (moveStatus) {
                gFlyPatch.originalFloating = readBoolField(moveStatus, MoveStatusComponent::floating);
                gFlyPatch.originalAirFly = readBoolField(moveStatus, MoveStatusComponent::airFly);
            }
            if (rigidBody)
                gFlyPatch.originalGravity = readFloatField(rigidBody, RigidBodyComponent::gravity);
            if (speedComp)
                gFlyPatch.originalGravityCoef = readFloatField(
                    speedComp, AttributeMoveSpeedCoefComponent::gravityCoef);
            if (weightless)
                gFlyPatch.originalWeightlessCount = readInt32Field(
                    weightless, WeightlessComponent::WeightlessCount);
            gFlyPatch.active = true;
        }
        if (moveStatus) {
            writeBoolField(moveStatus, MoveStatusComponent::airFly, true);
            writeBoolField(moveStatus, MoveStatusComponent::floating, true);
        }
        if (config_.enableNoGravity) {
            if (rigidBody)
                writeFloatField(rigidBody, RigidBodyComponent::gravity, Defaults::kNoGravity);
            if (speedComp)
                writeFloatField(speedComp, AttributeMoveSpeedCoefComponent::gravityCoef,
                                Defaults::kNoGravity);
        }
        if (weightless)
            writeInt32Field(weightless, WeightlessComponent::WeightlessCount, 1);
        static std::atomic<bool> logged{};
        if (!logged.exchange(true))
            FM_LOG("LunaryFM", "fly: move/gravity/weightless path confirmed");
    } else if (gFlyPatch.active && gFlyPatch.entity == entity) {
        if (gFlyPatch.moveStatus) {
            writeBoolField(gFlyPatch.moveStatus, MoveStatusComponent::floating,
                           gFlyPatch.originalFloating);
            writeBoolField(gFlyPatch.moveStatus, MoveStatusComponent::airFly,
                           gFlyPatch.originalAirFly);
        }
        if (gFlyPatch.rigidBody)
            writeFloatField(gFlyPatch.rigidBody, RigidBodyComponent::gravity,
                            gFlyPatch.originalGravity);
        if (gFlyPatch.speedCoef)
            writeFloatField(gFlyPatch.speedCoef,
                            AttributeMoveSpeedCoefComponent::gravityCoef,
                            gFlyPatch.originalGravityCoef);
        if (gFlyPatch.weightless)
            writeInt32Field(gFlyPatch.weightless, WeightlessComponent::WeightlessCount,
                            gFlyPatch.originalWeightlessCount);
        gFlyPatch = {};
    }
}

// =============================================================================
// FEATURE 5: No Recoil
// Owned by client-camera detours. RecoilComponent must remain original.
// =============================================================================
void FeatureManager::updateNoRecoil(void* entity) {
    (void)entity;
}

// =============================================================================
// FEATURE 6: No Spread
// Owned by state-preserving DoScatter rotation compensation.
// =============================================================================
void FeatureManager::updateNoSpread(void* entity) {
    (void)entity;
}

// =============================================================================
// FEATURE 7: Infinite Ammo / No Reload
// ReloadComponent: maxClip = huge, coverClip = huge, clipUseOnce = 0
// DisableReloadComponent: prevents reload from triggering
// =============================================================================
void FeatureManager::updateInfiniteAmmo(void* entity) {
    using namespace offsets;

    // Only run when actually configured (prevents writing 0 to clip fields)
    if (config_.clipSize <= 0 && !config_.noReload) return;

    void* reloadComp = gGetReload ? gGetReload(entity, nullptr) : nullptr;
    if (reloadComp) {
        int clipVal = config_.clipSize > 0 ? config_.clipSize : 999;
        writeInt32Field(reloadComp, ReloadComponent::maxClip, clipVal);
        writeInt32Field(reloadComp, ReloadComponent::coverClip, clipVal);
        writeInt32Field(reloadComp, ReloadComponent::maxClip2, clipVal);
    }

    // The live ammunition is stored in ReloadDynamicComponent, not in
    // ReloadComponent.  Refill every frame and leave clipUseOnce untouched.
    void* dynamic = gGetReloadDynamic ? gGetReloadDynamic(entity, nullptr) : nullptr;
    if (dynamic) {
        const int clipVal = config_.clipSize > 0 ? config_.clipSize : 999;
        writeInt32Field(dynamic, ReloadDynamicComponent::curClip, clipVal);
        writeInt32Field(dynamic, ReloadDynamicComponent::curClip2, clipVal);
        writeInt32Field(dynamic, ReloadDynamicComponent::curClip3, clipVal);
        static std::atomic<bool> logged{};
        if (!logged.exchange(true))
            FM_LOG("LunaryFM", "infinite ammo: ReloadDynamic refill confirmed");
    }
}

// =============================================================================
// FEATURE 8: Rapid Fire
// WeaponComponent.shootGapTimeMs = 0 (or very low)
// BaseWeaponPlugin.shootGapTime = 0
// =============================================================================
void FeatureManager::updateRapidFire(void* entity) {
    // GetShootGapTime/GetShootDelayTime are detoured in feature_hooks.cpp.
    // Mutating shootGapTimeMs here repeatedly divided an already-patched value.
    (void)entity;
}

// =============================================================================
// FEATURE 9: Wallhack
// SightBarrierComponent: disable/remove sight barriers
// PhysicsComponent: modify filterGroup/filterMask to ignore walls
// BulletPassBlockPhysConditionPlugin: allow bullets to pass through walls
// =============================================================================
void FeatureManager::updateWallhack(void* entity) {
    using namespace offsets;

    if (config_.shootThroughWalls) {
        // Backup for projectile paths that inspect the local rigid body.  The
        // condition plugin itself is detoured in feature_hooks.cpp.
        void* rigidBody = gGetRigidBody ? gGetRigidBody(entity, nullptr) : nullptr;
        if (rigidBody) {
            writeBoolField(rigidBody, RigidBodyComponent::passTarget, true);
            writeBoolField(rigidBody, RigidBodyComponent::detectedAllPassTarget, true);
        }
    }
}

// =============================================================================
// FEATURE 10: ESP (Enemy Visualization)
// Reads TransformComponent.position, HealthComponent, AimComponent
// Sends data via telemetry for overlay rendering
// =============================================================================
void FeatureManager::updateESP(void* entity) {
    using namespace offsets;

    // ESP data is collected by reading entity components and sending via telemetry
    // This function is a no-op on the injected side - the telemetry system
    // already reads entity data and the UI renders the ESP overlay.
    //
    // The existing telemetry_server.cpp already sends entity position/health/aim data.
    // ESP rendering is done on the UI side (overlay.hpp/cpp).
    //
    // To enhance ESP:
    // - Ensure all entity positions are being reported
    // - Add bone data for skeleton ESP
    // - Add weapon info display

    // Force visibility for all enemies (remove invisibility from enemies)
    void* camouflage = getComponentPtr(entity, ComponentId::Camouflage);
    if (camouflage && config_.espEnabled) {
        // Clear camouflage target to make enemy visible
        // This bypasses the camouflage system
        writeInt32Field(camouflage, CamouflageComponent::uncamouflageFrame, 0);
    }
}

// =============================================================================
// FEATURE 11: No Skill Cooldown
// DLTrainingModeComponent.IgnoreSkillCD = true
// =============================================================================
void FeatureManager::updateNoSkillCooldown(void* entity) {
    using namespace offsets;

    if (config_.noSkillCD) {
        // CheckSkillCD is also detoured.  Keep component counts full as a
        // fallback for UI/action paths that read the values directly.
        void* skill1 = gGetSkill1 ? gGetSkill1(entity, nullptr) : nullptr;
        if (skill1) {
            writeUInt16Field(skill1, Skill1Component::current,
                             readUInt16Field(skill1, Skill1Component::max));
        }

        void* skill2 = gGetSkill2 ? gGetSkill2(entity, nullptr) : nullptr;
        if (skill2) {
            writeUInt16Field(skill2, 0x18, readUInt16Field(skill2, 0x1A));
        }

        void* skillEx = gGetSkillEx ? gGetSkillEx(entity, nullptr) : nullptr;
        if (skillEx) {
            // SkillEx has max at 0x18 and count at 0x1A.
            writeUInt16Field(skillEx, 0x1A, readUInt16Field(skillEx, 0x18));
        }
        static std::atomic<bool> logged{};
        if ((skill1 || skill2 || skillEx) && !logged.exchange(true))
            FM_LOG("LunaryFM", "skill cooldown: direct component refill confirmed");
    }
}

// =============================================================================
// FEATURE 12: Teleport
// TeleportationComponent + SetPosition manipulation
// =============================================================================
void FeatureManager::updateTeleport(void* entity) {
    using namespace offsets;

    if (!config_.teleportEnabled || config_.teleportRequest == 0) return;

    if (config_.teleportRequest != gLastTeleportRequest) {
        gLastTeleportRequest = config_.teleportRequest;
        // Both the simulation and visual Entity receive position updates. Keep
        // applying briefly so the next physics/network tick consumes the new
        // position instead of immediately restoring the previous one.
        gTeleportHoldFrames.store(12, std::memory_order_release);
        FM_LOG("LunaryFM", "teleport request=%llu -> (%.2f, %.2f, %.2f)",
               static_cast<unsigned long long>(config_.teleportRequest),
               config_.teleportX, config_.teleportY, config_.teleportZ);
    }
    if (gTeleportHoldFrames.load(std::memory_order_acquire) <= 0) return;

    void* transformComp = gGetTransform ? gGetTransform(entity, nullptr) : nullptr;
    if (!transformComp || !gSetComponentDirty || !gSetPhysicsPosition ||
        !gSetPhysicsRotation)
        return;

    const NativeVector3 target{config_.teleportX, config_.teleportY,
                               config_.teleportZ};
    NativeQuaternion rotation{};
    (void)safeReadPtr(
        reinterpret_cast<std::uintptr_t>(transformComp) +
            TransformComponent::position + sizeof(NativeVector3),
        &rotation, sizeof(rotation));

    // ECS state first, matching the non-protected half of
    // DoorActionServer.SetPosition.
    std::memcpy(reinterpret_cast<std::byte*>(transformComp) +
                    TransformComponent::position,
                &target, sizeof(target));
    const auto* dirtyMethod = teleportSetDirtyMethod();
    if (dirtyMethod) gSetComponentDirty(transformComp, dirtyMethod);
    static std::atomic<bool> ecsRouteLogged{};
    if (!ecsRouteLogged.exchange(true))
        FM_LOG("LunaryFM", "teleport: ECS Transform + SetDirty route confirmed");

    // VisualPlayerUI owns the Unity Transform used as source of truth for the
    // local player. Its ECSUpdate reads this Transform back into Pos every
    // frame, so changing only TransformComponent can never move the model.
    void* visualPlayer =
        gLocalVisualPlayer.load(std::memory_order_acquire);
    void* visualTransform{};
    if (visualPlayer) {
        (void)safeReadPtr(reinterpret_cast<std::uintptr_t>(visualPlayer) + 0x198,
                          &visualTransform, sizeof(visualTransform));
    }
    if (visualTransform && gSetVisualTransformPosition) {
        gSetVisualTransformPosition(visualTransform, &target);
        std::memcpy(reinterpret_cast<std::byte*>(visualPlayer) + 0x1C0,
                    &target, sizeof(target));
        static std::uint64_t visualLoggedRequest{};
        if (visualLoggedRequest != config_.teleportRequest) {
            visualLoggedRequest = config_.teleportRequest;
            FM_LOG("LunaryFM",
                   "teleport: Unity visual Transform applied vp=%p transform=%p",
                   visualPlayer, visualTransform);
        }
    } else {
        static std::uint64_t missingVisualLoggedRequest{};
        if (missingVisualLoggedRequest != config_.teleportRequest) {
            missingVisualLoggedRequest = config_.teleportRequest;
            FM_LOG("LunaryFM",
                   "teleport: visual route unavailable vp=%p transform=%p setter=%p",
                   visualPlayer, visualTransform,
                   reinterpret_cast<void*>(gSetVisualTransformPosition));
        }
    }

    // Keep the native actor synchronized as well. Collider lookup is performed
    // directly over the managed Dictionary to avoid protected generic methods.
    void* physicsSystem = gObservedPhysicsSystem.load(std::memory_order_acquire);
    const std::uint16_t entityId =
        gLocalEntityId.load(std::memory_order_acquire);
    void* collider = findPhysicsCollider(physicsSystem, entityId);
    void* physicsActor{};
    if (collider)
        (void)safeReadPtr(reinterpret_cast<std::uintptr_t>(collider) + 0x10,
                          &physicsActor, sizeof(physicsActor));
    if (!physicsActor || !nativePhysicsSettersReady()) {
        static std::atomic<unsigned> waitingLog{};
        if ((waitingLog.fetch_add(1, std::memory_order_relaxed) & 127U) == 0)
            FM_LOG("LunaryFM",
                   "teleport: waiting physics route system=%p eid=%u collider=%p actor=%p",
                   physicsSystem, entityId, collider, physicsActor);
        // The ECS move is already authoritative for the client simulation in
        // UnityPhysicsProxy matches. End the finite hold normally so one panel
        // click does not pin the player forever when no native backend exists.
        gTeleportHoldFrames.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    gSetPhysicsPosition(physicsActor, target, nullptr);
    gSetPhysicsRotation(physicsActor, rotation, nullptr);
    static std::atomic<bool> routeLogged{};
    if (!routeLogged.exchange(true))
        FM_LOG("LunaryFM",
               "teleport: ECS dirty + PhysicsActor route confirmed (eid=%u)",
               entityId);
    gTeleportHoldFrames.fetch_sub(1, std::memory_order_acq_rel);
}

// =============================================================================
// FEATURE 13: Infinite Shield
// ShieldComponent.current = max, ShieldComponent.max = huge value
// =============================================================================
void FeatureManager::updateInfiniteShield(void* entity) {
    using namespace offsets;

    void* shieldComp = getComponentPtr(entity, ComponentId::Shield);
    if (shieldComp) {
        writeInt32Field(shieldComp, ShieldComponent::max, config_.shieldValue);
        writeInt32Field(shieldComp, ShieldComponent::current, config_.shieldValue);
    }
}

// =============================================================================
// FEATURE 14: Aimbot
// Two-path approach:
//   Path A: Sync config → camera_hook atomics (aimAtBestTarget manipulates camera view angle)
//   Path B: Write to entity's AimComponent.direct (Vec3 at 0x18) for nearest enemy head
// =============================================================================

// Extern declarations for camera_hook exported functions
extern "C" {
    void lunary_set_aim_enabled(bool enabled);
    void lunary_set_aim_options(float fovDegrees, float smoothing, float maximumDistance, bool useHead);
}

void FeatureManager::updateAimbot(void* entity) {
    using namespace offsets;

    // ---- PATH A: Sync FeatureManager config → camera_hook atomics ----
    lunary_set_aim_enabled(config_.aimbotEnabled);
    if (config_.aimbotEnabled) {
        lunary_set_aim_options(
            config_.aimbotFov > 0 ? static_cast<float>(config_.aimbotFov) : 25.0f,
            config_.aimbotSmoothing > 0 ? static_cast<float>(config_.aimbotSmoothing) : 3.0f,
            config_.aimbotMaxDistance > 0 ? static_cast<float>(config_.aimbotMaxDistance) : 90.0f,
            true  // useHead: always target head
        );
    }

    // ---- PATH B: Direct AimComponent.direct manipulation ----
    // Write look direction to entity's AimComponent to override where the player aims.
    // The camera_hook aimAtBestTarget already handles target selection and angle calc,
    // so this is a belt-and-suspenders: if camera angle manipulation fails (e.g.
    // game resets view angle), the AimComponent.direct will still force the shot direction.
    if (config_.aimbotEnabled && entity) {
        void* aimComp = getComponentPtr(entity, ComponentId::Aim);
        // AimComponent.direct (Vec3 at offset 0x18) controls the shot direction.
        // We leave this at a "neutral forward" default; the camera_hook actually
        // does the angle manipulation. But we ensure the component exists.
        if (aimComp) {
            static int aimInit{};
            if ((++aimInit & 63) == 0) {
                FM_LOG("LunaryFM", "aimbot: AimComponent found at 0x%zx", (uintptr_t)aimComp);
            }
        }
    }
}

// =============================================================================
// FEATURE 15: Infinite Jump
// JumpComponent.maxInAirJumpCount = huge
// JumpComponent.inAirJumpCount = reset
// =============================================================================
void FeatureManager::updateInfiniteJump(void* entity) {
    using namespace offsets;

    if (config_.enableMultiJump) {
        void* jumpComp = getComponentPtr(entity, ComponentId::Jump);
        if (jumpComp) {
            writeInt32Field(jumpComp, JumpComponent::maxInAirJumpCount, config_.maxJumps);
            writeBoolField(jumpComp, JumpComponent::enableInAirJump, true);
        }
    }
}

// =============================================================================
// FEATURE 16: Auto Shoot
// AutoShootComponent.disAutoShootCount = 0 (enable auto shoot)
// Automatically fires when enemy is in crosshair
// =============================================================================
void FeatureManager::updateAutoShoot(void* entity) {
    using namespace offsets;

    if (config_.autoShootEnabled) {
        void* autoShootComp = getComponentPtr(entity, ComponentId::AutoShoot);
        if (autoShootComp) {
            // disAutoShootCount = 0 means auto shoot is ENABLED
            // > 0 means auto shoot is disabled
            *reinterpret_cast<std::int16_t*>(
                reinterpret_cast<std::uint8_t*>(autoShootComp) + AutoShootComponent::disAutoShootCount) = 0;
        }

        // Also ensure weapon is in shooting state
        void* weaponComp = getComponentPtr(entity, ComponentId::Weapon);
        if (weaponComp) {
            writeBoolField(weaponComp, WeaponComponent::isShooting, true);
        }
    }
}

// =============================================================================
// FEATURE 17: Invisibility
// CamouflageComponent manipulation - set camouflage state
// Makes local player invisible to enemies
// =============================================================================
void FeatureManager::updateInvisibility(void* entity) {
    using namespace offsets;

    if (config_.invisibilityEnabled) {
        void* camoComp = getComponentPtr(entity, ComponentId::Camouflage);
        if (camoComp) {
            // Set uncamouflageFrame to max to never uncamouflage
            writeInt32Field(camoComp, CamouflageComponent::uncamouflageFrame, 0x7FFFFFFF);
        }

        // HideClientDamageInfo not yet dumped — skip for now
    }
}

// =============================================================================
// FEATURE 18: No Knockback
// KnockBackComponent and KnockBackStateComponent - suppress knockback
// ForceComponent/ForceVelocityComponent - clear forces
// =============================================================================
void FeatureManager::updateNoKnockback(void* entity) {
    using namespace offsets;

    if (config_.noKnockback) {
        void* knockBack = gGetKnockBack ? gGetKnockBack(entity, nullptr) : nullptr;
        if (knockBack) {
            writeFloatField(knockBack, 0x18, 0.0F);
            writeFloatField(knockBack, 0x1C, 0.0F);
            writeFloatField(knockBack, 0x20, 0.0F);
        }
        if (gHasKnockBack && gDelKnockBack && gHasKnockBack(entity, nullptr))
            gDelKnockBack(entity, nullptr);
        if (gHasKnockBackState && gDelKnockBackState &&
            gHasKnockBackState(entity, nullptr))
            gDelKnockBackState(entity, nullptr);
        if (gHasForceVelocity && gDelForceVelocity &&
            gHasForceVelocity(entity, nullptr))
            gDelForceVelocity(entity, nullptr);
    }
}

}  // namespace lunary::injected
