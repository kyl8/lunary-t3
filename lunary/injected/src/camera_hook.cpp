#include "aim_math.hpp"
#include "feature_hooks.hpp"
#include "feature_manager.hpp"
#include "il2cpp_layout.hpp"
#include "inline_detour.hpp"
#include "telemetry_server.hpp"

#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <elf.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <ucontext.h>
#include <unistd.h>

namespace {
constexpr const char* kDiagnosticPath = "/data/local/tmp/lunary-hook.log";

void appendDiagnostic(const char* message) {
    if (!message) return;
    const int descriptor = open(kDiagnosticPath, O_WRONLY | O_APPEND | O_CLOEXEC);
    if (descriptor < 0) return;
    (void)write(descriptor, message, strlen(message));
    (void)write(descriptor, "\n", 1U);
    close(descriptor);
}
} // namespace

// Log during library load (BEFORE JNI_OnLoad)
__attribute__((constructor(101))) static void liblunary_init_early() {
    appendDiagnostic("CONSTRUCTOR(101) - library loading");
    __android_log_print(ANDROID_LOG_INFO, "LunaryHook", "CONSTRUCTOR(101) - library loading");
}

__attribute__((constructor(65535))) static void liblunary_init_late() {
    appendDiagnostic("CONSTRUCTOR(65535) - all ctors done");
    __android_log_print(ANDROID_LOG_INFO, "LunaryHook", "CONSTRUCTOR(65535) - all ctors done");
}

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lunary::injected {
namespace {

constexpr const char* kLogTag = "LunaryHook";

struct Offsets {
    // R_AARCH64_RELATIVE slots whose relocated values point at the classes.
    std::uintptr_t matchStartTypeInfo = 0x673EF80;
    std::uintptr_t cameraControllerTypeInfo = 0x66D55D0;
    // CameraController.UpdateCamera at +0x23CDBC8 remains protected/encrypted.
    // +0x23F5D4C is GameClient.UpdateVisualObjects, a stable outer frame
    // callback. Its self is not a CameraController; resolve that singleton
    // separately before touching m_ViewAngle.
    std::uintptr_t cameraUpdate = 0x23F5D4C;
    std::uintptr_t matchStartGetEntity = 0x2407ED0;
    std::uintptr_t entityGetHealth = 0x1800A98;
    std::uintptr_t entityGetTransform = 0x17D8394;
    std::uintptr_t entityGetPlayer = 0x17D59C0;
    std::uintptr_t entityHasHitPointContainer = 0x1801114;
    std::uintptr_t entityGetHitPointContainer = 0x180118C;
    std::uintptr_t transformGetChildCount = 0x4B6E27C;
    std::uintptr_t transformGetChild = 0x4B6E8DC;
    std::uintptr_t transformGetPosition = 0x4B6C388;
    std::uintptr_t matchStartUiHealthList = 0xC8;
    std::uintptr_t cameraViewAngle = 0x30;
    std::uintptr_t uiHealthEntity = 0x1B0;
    std::uintptr_t uiHealthEntityId = 0x1B8;
    std::uintptr_t uiHealthIsSameTeam = 0x1ED;
    std::uintptr_t uiHealthIsLocal = 0x1EE;
    std::uintptr_t uiHealthBloodPosition = 0x194;
    std::uintptr_t uiHealthCamera = 0x228;
    std::uintptr_t healthCurrent = 0x18;
    std::uintptr_t healthMaximum = 0x1C;
    std::uintptr_t transformPosition = 0x18;
    std::uintptr_t hitPoint0 = 0x1C;
    std::uintptr_t hitPoint1 = 0x3C;
    std::uintptr_t unityObjectCachedPointer = 0x10;
    std::uintptr_t cameraViewMatrix = 0x5C;
    std::uintptr_t cameraProjectionMatrix = 0x9C;
    std::uintptr_t cameraViewport = 0x404;
    std::uintptr_t matchStartMatchMode = 0x28;
    std::uintptr_t matchStartStatus = 0x38;
    std::uintptr_t matchStartAllVisualPlayerUIList = 0xD0;
    std::uintptr_t visualPlayerUIEntity = 0x1A0;
    std::uintptr_t visualPlayerUIHeroCfg = 0x1A8;
    std::uintptr_t visualPlayerUIIsInvisibilityBuff = 0x1DC;
    std::uintptr_t visualPlayerUIIsVanish = 0x1F0;
    std::uintptr_t visualPlayerUIIsAgonal = 0x1F1;
    std::uintptr_t heroConfigName = 0x7C;
    std::uintptr_t playerDisplayName = 0x20;
    std::uintptr_t playerHeroName = 0x38;
    std::uintptr_t uiHealthFriend = 0x1EF;
    std::uintptr_t uiHealthInitTeam = 0x1F4;
};

struct HitPoint {
    Vec3 position;
    float rotationX;
    float rotationY;
    float rotationZ;
    float rotationW;
    bool isHead;
    bool useGroundNormal;
    std::uint16_t entityId;
};

static_assert(sizeof(HitPoint) == 0x20);
static_assert(offsetof(HitPoint, isHead) == 0x1C);
static_assert(offsetof(HitPoint, entityId) == 0x1E);

using CameraUpdateFunction = void (*)(void*, const MethodInfo*);
using GetComponentFunction = void* (*)(void*, const MethodInfo*);
using HasComponentFunction = bool (*)(void*, const MethodInfo*);
using GetEntityFunction = void* (*)(std::uint16_t, bool, const MethodInfo*);
using TransformGetChildCountFunc = int (*)(void*, const MethodInfo*);
using TransformGetChildFunc = void* (*)(void*, int, const MethodInfo*);
using TransformGetPositionFunc = void (*)(void*, Vec3*, const MethodInfo*);
using SetAimDirectFunction = bool (*)(void*, Vec3, bool, const MethodInfo*);
using MaterialOutlineFunction = void (*)(void*, const MethodInfo*);

Offsets gOffsets;
InlineDetour gDetour;
std::atomic<void*> gOriginalCameraUpdate{};
std::atomic<bool> gTrampolineObserved{};
std::atomic<std::uint64_t> gHookFrames{};
std::atomic<std::uintptr_t> gResolvedCameraUpdate{};
std::atomic<std::uint64_t> gLastTelemetryMs{};
GetEntityFunction gGetEntity{};
GetComponentFunction gGetHealth{};
GetComponentFunction gGetTransform{};
GetComponentFunction gGetPlayer{};
GetComponentFunction gGetAim{};
GetComponentFunction gGetMaterialAdjust{};
SetAimDirectFunction gSetAimDirect{};
MaterialOutlineFunction gForceOpenOutline{};
MaterialOutlineFunction gForceCloseOutline{};
std::unordered_set<void*> gForcedOutlineManagers;
HasComponentFunction gHasHitPointContainer{};
GetComponentFunction gGetHitPointContainer{};
TransformGetChildCountFunc gTransformGetChildCount{};
TransformGetChildFunc gTransformGetChild{};
TransformGetPositionFunc gTransformGetPosition{};
using GetLocalPlayerEntityFunc = void* (*)(const MethodInfo*);
GetLocalPlayerEntityFunc gGetLocalPlayerEntity{};
std::uintptr_t gModuleBase{};
std::size_t gModuleSize{};
struct MemoryRange {
    std::uintptr_t begin{};
    std::uintptr_t end{};
    bool readable{};
    bool writable{};
};
std::vector<MemoryRange> gMemoryRanges;
std::atomic<bool> gAimEnabled{true};
std::atomic<bool> gUseHead{true};
std::atomic<bool> gIgnoreTeammates{true};
std::atomic<bool> gLockTarget{true};
std::atomic<float> gFovDegrees{25.0F};
std::atomic<float> gSmoothing{3.0F};
std::atomic<float> gMaximumDistance{90.0F};
std::atomic<float> gEyeHeight{1.4F};
std::atomic<void*> gLockedEntity{};
std::mutex gLifecycleMutex;
std::mutex gErrorMutex;
std::string gLastError;
std::once_flag gStartOnce;

void logMessage(int priority, const std::string& message) {
    appendDiagnostic(message.c_str());
    __android_log_print(priority, kLogTag, "%s", message.c_str());
}

void setError(std::string message) {
    {
        std::scoped_lock lock(gErrorMutex);
        gLastError = message;
    }
    logMessage(ANDROID_LOG_ERROR, message);
}

std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool parseBool(std::string value, bool fallback) {
    value = trim(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
    if (value == "0" || value == "false" || value == "no" || value == "off") return false;
    return fallback;
}

std::uintptr_t parseAddress(const std::string& value, std::uintptr_t fallback) {
    char* end{};
    const auto parsed = std::strtoull(value.c_str(), &end, 0);
    return end != value.c_str() && *end == '\0'
        ? static_cast<std::uintptr_t>(parsed) : fallback;
}

float parseFloat(const std::string& value, float fallback) {
    char* end{};
    const float parsed = std::strtof(value.c_str(), &end);
    return end != value.c_str() && *end == '\0' && std::isfinite(parsed)
        ? parsed : fallback;
}

std::string configurationPath() {
    const char* environmentPath = std::getenv("LUNARY_HOOK_CONFIG");
    return environmentPath && *environmentPath
        ? environmentPath : "/data/local/tmp/lunary-hook.ini";
}

std::string readConfiguration(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void applyConfiguration(const std::string& contents, bool loadRvas) {
    std::istringstream input(contents);
    std::string line;
    std::size_t loadedFields = 0;
    while (std::getline(input, line)) {
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) line.erase(comment);
        const auto separator = line.find('=');
        if (separator == std::string::npos) continue;
        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        if (key == "enabled") gAimEnabled = parseBool(value, gAimEnabled.load());
        else if (key == "head") gUseHead = parseBool(value, gUseHead.load());
        else if (key == "ignore_teammates")
            gIgnoreTeammates = parseBool(value, gIgnoreTeammates.load());
        else if (key == "lock_target") gLockTarget = parseBool(value, gLockTarget.load());
        else if (key == "fov_degrees")
            gFovDegrees = std::clamp(parseFloat(value, gFovDegrees.load()), 1.0F, 180.0F);
        else if (key == "smoothing")
            gSmoothing = std::clamp(parseFloat(value, gSmoothing.load()), 1.0F, 30.0F);
        else if (key == "max_distance")
            gMaximumDistance = std::max(0.0F, parseFloat(value, gMaximumDistance.load()));
        else if (key == "eye_height")
            gEyeHeight = std::clamp(parseFloat(value, gEyeHeight.load()), -2.0F, 4.0F);
        else if (loadRvas && key == "rva_match_start_typeinfo")
            gOffsets.matchStartTypeInfo = parseAddress(value, gOffsets.matchStartTypeInfo);
        else if (loadRvas && key == "rva_camera_controller_typeinfo")
            gOffsets.cameraControllerTypeInfo = parseAddress(value, gOffsets.cameraControllerTypeInfo);
        else if (loadRvas && key == "rva_camera_update")
            gOffsets.cameraUpdate = parseAddress(value, gOffsets.cameraUpdate);
        else if (loadRvas && key == "rva_match_start_get_entity")
            gOffsets.matchStartGetEntity = parseAddress(value, gOffsets.matchStartGetEntity);
        else if (loadRvas && key == "rva_entity_get_health")
            gOffsets.entityGetHealth = parseAddress(value, gOffsets.entityGetHealth);
        else if (loadRvas && key == "rva_entity_get_transform")
            gOffsets.entityGetTransform = parseAddress(value, gOffsets.entityGetTransform);
        else if (loadRvas && key == "rva_entity_get_player")
            gOffsets.entityGetPlayer = parseAddress(value, gOffsets.entityGetPlayer);
        else if (loadRvas && key == "rva_entity_has_hitpoint_container")
            gOffsets.entityHasHitPointContainer =
                parseAddress(value, gOffsets.entityHasHitPointContainer);
        else if (loadRvas && key == "rva_entity_get_hitpoint_container")
            gOffsets.entityGetHitPointContainer =
                parseAddress(value, gOffsets.entityGetHitPointContainer);
        else if (loadRvas && key == "rva_transform_get_child_count")
            gOffsets.transformGetChildCount =
                parseAddress(value, gOffsets.transformGetChildCount);
        else if (loadRvas && key == "rva_transform_get_child")
            gOffsets.transformGetChild =
                parseAddress(value, gOffsets.transformGetChild);
        else if (loadRvas && key == "rva_transform_get_position")
            gOffsets.transformGetPosition =
                parseAddress(value, gOffsets.transformGetPosition);
        else if (loadRvas && key == "field_match_start_ui_health_list")
            gOffsets.matchStartUiHealthList =
                parseAddress(value, gOffsets.matchStartUiHealthList), ++loadedFields;
        else if (loadRvas && key == "field_camera_view_angle")
            gOffsets.cameraViewAngle =
                parseAddress(value, gOffsets.cameraViewAngle), ++loadedFields;
        else if (loadRvas && key == "field_ui_health_entity")
            gOffsets.uiHealthEntity =
                parseAddress(value, gOffsets.uiHealthEntity), ++loadedFields;
        else if (loadRvas && key == "field_ui_health_entity_id")
            gOffsets.uiHealthEntityId =
                parseAddress(value, gOffsets.uiHealthEntityId), ++loadedFields;
        else if (loadRvas && key == "field_ui_health_is_same_team")
            gOffsets.uiHealthIsSameTeam =
                parseAddress(value, gOffsets.uiHealthIsSameTeam), ++loadedFields;
        else if (loadRvas && key == "field_ui_health_is_local")
            gOffsets.uiHealthIsLocal =
                parseAddress(value, gOffsets.uiHealthIsLocal), ++loadedFields;
        else if (loadRvas && key == "field_ui_health_blood_position")
            gOffsets.uiHealthBloodPosition =
                parseAddress(value, gOffsets.uiHealthBloodPosition), ++loadedFields;
        else if (loadRvas && key == "field_ui_health_camera")
            gOffsets.uiHealthCamera =
                parseAddress(value, gOffsets.uiHealthCamera), ++loadedFields;
        else if (loadRvas && key == "field_health_current")
            gOffsets.healthCurrent =
                parseAddress(value, gOffsets.healthCurrent), ++loadedFields;
        else if (loadRvas && key == "field_health_maximum")
            gOffsets.healthMaximum =
                parseAddress(value, gOffsets.healthMaximum), ++loadedFields;
        else if (loadRvas && key == "field_transform_position")
            gOffsets.transformPosition =
                parseAddress(value, gOffsets.transformPosition), ++loadedFields;
        else if (loadRvas && key == "field_hitpoint0")
            gOffsets.hitPoint0 =
                parseAddress(value, gOffsets.hitPoint0), ++loadedFields;
        else if (loadRvas && key == "field_hitpoint1")
            gOffsets.hitPoint1 =
                parseAddress(value, gOffsets.hitPoint1), ++loadedFields;
        else if (loadRvas && key == "field_unity_object_cached_ptr")
            gOffsets.unityObjectCachedPointer =
                parseAddress(value, gOffsets.unityObjectCachedPointer), ++loadedFields;
        else if (loadRvas && key == "field_camera_view_matrix")
            gOffsets.cameraViewMatrix =
                parseAddress(value, gOffsets.cameraViewMatrix), ++loadedFields;
        else if (loadRvas && key == "field_camera_projection_matrix")
            gOffsets.cameraProjectionMatrix =
                parseAddress(value, gOffsets.cameraProjectionMatrix), ++loadedFields;
        else if (loadRvas && key == "field_camera_viewport")
            gOffsets.cameraViewport =
                parseAddress(value, gOffsets.cameraViewport), ++loadedFields;
        else if (loadRvas && key == "field_match_start_match_mode")
            gOffsets.matchStartMatchMode =
                parseAddress(value, gOffsets.matchStartMatchMode), ++loadedFields;
        else if (loadRvas && key == "field_match_start_status")
            gOffsets.matchStartStatus =
                parseAddress(value, gOffsets.matchStartStatus), ++loadedFields;
        else if (loadRvas && key == "field_match_start_all_visual_player_ui_list")
            gOffsets.matchStartAllVisualPlayerUIList =
                parseAddress(value, gOffsets.matchStartAllVisualPlayerUIList), ++loadedFields;
        else if (loadRvas && key == "field_visual_player_ui_entity")
            gOffsets.visualPlayerUIEntity =
                parseAddress(value, gOffsets.visualPlayerUIEntity), ++loadedFields;
        else if (loadRvas && key == "field_visual_player_ui_hero_cfg")
            gOffsets.visualPlayerUIHeroCfg =
                parseAddress(value, gOffsets.visualPlayerUIHeroCfg), ++loadedFields;
        else if (loadRvas && key == "field_visual_player_ui_is_invisibility_buff")
            gOffsets.visualPlayerUIIsInvisibilityBuff =
                parseAddress(value, gOffsets.visualPlayerUIIsInvisibilityBuff), ++loadedFields;
        else if (loadRvas && key == "field_visual_player_ui_is_vanish")
            gOffsets.visualPlayerUIIsVanish =
                parseAddress(value, gOffsets.visualPlayerUIIsVanish), ++loadedFields;
        else if (loadRvas && key == "field_visual_player_ui_is_agonal")
            gOffsets.visualPlayerUIIsAgonal =
                parseAddress(value, gOffsets.visualPlayerUIIsAgonal), ++loadedFields;
        else if (loadRvas && key == "field_hero_config_name")
            gOffsets.heroConfigName =
                parseAddress(value, gOffsets.heroConfigName), ++loadedFields;
        else if (loadRvas && key == "field_ui_health_is_friend")
            gOffsets.uiHealthFriend =
                parseAddress(value, gOffsets.uiHealthFriend), ++loadedFields;
        else if (loadRvas && key == "field_ui_health_init_team")
            gOffsets.uiHealthInitTeam =
                parseAddress(value, gOffsets.uiHealthInitTeam), ++loadedFields;
        else if (loadRvas && key == "field_player_display_name")
            gOffsets.playerDisplayName =
                parseAddress(value, gOffsets.playerDisplayName), ++loadedFields;
        else if (loadRvas && key == "field_player_hero_name")
            gOffsets.playerHeroName =
                parseAddress(value, gOffsets.playerHeroName), ++loadedFields;
        // Feature config fields (from panel)
        else if (key == "feature_enabled") {
            bool enabled = parseBool(value, false);
            FeatureManager::instance().config().enabled = enabled;
            // featureEnabled_[i] is set per-feature below, NOT all at once
        }
        else if (key == "feature_god_mode") FeatureManager::instance().config().invincibleCount = parseBool(value, false) ? 99 : 1;
        else if (key == "feature_invincible_count") FeatureManager::instance().config().invincibleCount = std::atoi(value.c_str());
        else if (key == "feature_damage_multiplier") FeatureManager::instance().config().damageMultiplier = parseFloat(value, 1.0f);
        else if (key == "feature_damage_reduce") FeatureManager::instance().config().damageReduceMultiplier = parseFloat(value, 0.0f);
        else if (key == "feature_speed_multiplier") FeatureManager::instance().config().moveSpeedMultiplier = parseFloat(value, 1.0f);
        else if (key == "feature_enable_fly") FeatureManager::instance().config().enableFly = parseBool(value, false);
        else if (key == "feature_no_gravity") FeatureManager::instance().config().enableNoGravity = parseBool(value, false);
        else if (key == "feature_zero_recoil") FeatureManager::instance().config().zeroRecoil = parseBool(value, false);
        else if (key == "feature_zero_spread") FeatureManager::instance().config().zeroSpread = parseBool(value, false);
        else if (key == "feature_clip_size") FeatureManager::instance().config().clipSize = std::atoi(value.c_str());
        else if (key == "feature_no_reload") FeatureManager::instance().config().noReload = parseBool(value, false);
        else if (key == "feature_fire_rate_mult") FeatureManager::instance().config().fireRateMultiplier = parseFloat(value, 1.0f);
        else if (key == "feature_shoot_through_walls") FeatureManager::instance().config().shootThroughWalls = parseBool(value, false);
        else if (key == "feature_see_through_walls") FeatureManager::instance().config().seeThroughWalls = parseBool(value, false);
        else if (key == "feature_esp_enabled") FeatureManager::instance().config().espEnabled = parseBool(value, false);
        else if (key == "feature_no_skill_cd") FeatureManager::instance().config().noSkillCD = parseBool(value, false);
        else if (key == "feature_teleport_enabled") FeatureManager::instance().config().teleportEnabled = parseBool(value, false);
        else if (key == "feature_shield_value") FeatureManager::instance().config().shieldValue = std::atoi(value.c_str());
        else if (key == "feature_aimbot_enabled") FeatureManager::instance().config().aimbotEnabled = parseBool(value, false);
        else if (key == "feature_max_jumps") FeatureManager::instance().config().maxJumps = std::atoi(value.c_str());
        else if (key == "feature_multi_jump") FeatureManager::instance().config().enableMultiJump = parseBool(value, false);
        else if (key == "feature_auto_shoot") FeatureManager::instance().config().autoShootEnabled = parseBool(value, false);
        else if (key == "feature_invisibility") FeatureManager::instance().config().invisibilityEnabled = parseBool(value, false);
        else if (key == "feature_no_knockback") FeatureManager::instance().config().noKnockback = parseBool(value, false);
        // feature_builtin_* belonged to the removed legacy tabs.  Do not map
        // them onto the direct-hook settings: old files commonly contain
        // "false"/1.0 defaults after the real settings and would therefore
        // silently disable fly, infinite ammo, skill cooldown and speed.
        else if (key == "feature_aimbot_fov") FeatureManager::instance().config().aimbotFov = std::clamp(parseFloat(value, 25.0f), 1.0f, 180.0f);
        else if (key == "feature_aimbot_smoothing") FeatureManager::instance().config().aimbotSmoothing = std::clamp(parseFloat(value, 3.0f), 1.0f, 30.0f);
        else if (key == "feature_aimbot_max_dist") FeatureManager::instance().config().aimbotMaxDistance = std::max(0.0f, parseFloat(value, 90.0f));
        else if (key == "feature_teleport_x") FeatureManager::instance().config().teleportX = parseFloat(value, 0.0f);
        else if (key == "feature_teleport_y") FeatureManager::instance().config().teleportY = parseFloat(value, 0.0f);
        else if (key == "feature_teleport_z") FeatureManager::instance().config().teleportZ = parseFloat(value, 0.0f);
        else if (key == "feature_teleport_request")
            FeatureManager::instance().config().teleportRequest =
                std::strtoull(value.c_str(), nullptr, 10);
    }
    FeatureManager::instance().syncFeatureEnabled();

    if (!gAimEnabled.load(std::memory_order_relaxed)) {
        gLockedEntity.store(nullptr, std::memory_order_relaxed);
    }
    if (loadRvas && loadedFields > 0) {
        std::ostringstream msg;
        msg << "loaded " << loadedFields << " field offsets from configuration";
        logMessage(ANDROID_LOG_INFO, msg.str());
    }
}

std::string loadConfiguration() {
    const std::string path = configurationPath();
    const std::string contents = readConfiguration(path);
    if (contents.empty()) {
        logMessage(ANDROID_LOG_WARN, "configuration not found or empty at " + path +
                                     "; using built-in defaults");
        return {};
    }
    applyConfiguration(contents, true);
    return contents;
}

void watchConfiguration(std::string previous) {
    const std::string path = configurationPath();
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const std::string current = readConfiguration(path);
        if (current.empty() || current == previous) continue;
        applyConfiguration(current, false);
        previous = current;
        logMessage(ANDROID_LOG_INFO, "runtime configuration reloaded");
    }
}

bool isReadable(std::uintptr_t address, std::size_t size) {
    if (!address || size == 0 || address > std::numeric_limits<std::uintptr_t>::max() - size)
        return false;
    const auto iterator = std::upper_bound(
        gMemoryRanges.begin(), gMemoryRanges.end(), address,
        [](std::uintptr_t value, const MemoryRange& range) {
            return value < range.begin;
        });
    if (iterator == gMemoryRanges.begin()) return false;
    const auto& range = *std::prev(iterator);
    return range.readable && address >= range.begin && address + size <= range.end;
}

bool safeCopy(std::uintptr_t address, void* destination, std::size_t size) {
    if (!address || !destination || size == 0) return false;
    iovec local{destination, size};
    iovec remote{reinterpret_cast<void*>(address), size};
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) ==
           static_cast<ssize_t>(size);
}

bool findModule() {
    std::ifstream maps("/proc/self/maps");
    if (!maps) return false;
    gMemoryRanges.clear();
    std::uintptr_t selectedBase{};
    std::string line;
    while (std::getline(maps, line)) {
        std::istringstream parser(line);
        std::string addresses;
        std::string permissions;
        std::string offsetText;
        std::string device;
        std::string inode;
        if (!(parser >> addresses >> permissions >> offsetText >> device >> inode)) continue;
        const auto dash = addresses.find('-');
        if (dash == std::string::npos) continue;
        const auto begin = parseAddress("0x" + addresses.substr(0, dash), 0);
        const auto end = parseAddress("0x" + addresses.substr(dash + 1), 0);
        if (!begin || end <= begin) continue;
        gMemoryRanges.push_back({
            begin, end,
            !permissions.empty() && permissions[0] == 'r',
            permissions.size() > 1 && permissions[1] == 'w',
        });

        std::string path;
        std::getline(parser, path);
        path = trim(std::move(path));
        const auto fileOffset = parseAddress("0x" + offsetText, 1);
        if (fileOffset != 0 || !std::string_view(path).ends_with("/libil2cpp.so"))
            continue;
        const auto* header = reinterpret_cast<const Elf64_Ehdr*>(begin);
        if (end - begin < sizeof(*header) ||
            std::memcmp(header->e_ident, ELFMAG, SELFMAG) != 0 ||
            header->e_ident[EI_CLASS] != ELFCLASS64 || header->e_machine != EM_AARCH64)
            continue;
        // NativeBridge keeps both the original file view and an emulated ELF view.
        // The latter is the highest offset-zero mapping and contains relocations.
        selectedBase = std::max(selectedBase, begin);
    }
    if (!selectedBase || !isReadable(selectedBase, sizeof(Elf64_Ehdr))) return false;
    const auto* header = reinterpret_cast<const Elf64_Ehdr*>(selectedBase);
    const auto programBytes = static_cast<std::size_t>(header->e_phnum) * header->e_phentsize;
    const auto programAddress = selectedBase + header->e_phoff;
    if (header->e_phentsize != sizeof(Elf64_Phdr) ||
        !isReadable(programAddress, programBytes)) return false;
    const auto* programs = reinterpret_cast<const Elf64_Phdr*>(programAddress);
    std::uintptr_t maximum{};
    for (std::uint16_t index = 0; index < header->e_phnum; ++index) {
        if (programs[index].p_type != PT_LOAD) continue;
        maximum = std::max(maximum, static_cast<std::uintptr_t>(
            programs[index].p_vaddr + programs[index].p_memsz));
    }
    gModuleBase = selectedBase;
    gModuleSize = maximum;
    return gModuleSize != 0;
}

// Forward declaration — defined below.
bool classHasName(Il2CppClass* klass, std::string_view expectedNamespace,
                  std::string_view expectedName);

Il2CppClass* classFromSlot(std::uintptr_t rva, std::string_view expectedName) {
    if (!gModuleBase || rva >= gModuleSize) return nullptr;
    const std::uintptr_t slotAddr = gModuleBase + rva;
    // Read the TypeInfo pointer safely — NativeBridge may not support
    // direct dereference of all guest addresses from the x86_64 host.
    Il2CppClass* klass = nullptr;
    if (safeCopy(slotAddr, &klass, sizeof(klass)) &&
        classHasName(klass, "T3Game", expectedName)) return klass;

    // Compatibility with configs that contain the class-object RVA instead
    // of the relocation-slot RVA.
    klass = reinterpret_cast<Il2CppClass*>(slotAddr);
    return classHasName(klass, "T3Game", expectedName) ? klass : nullptr;
}

bool classHasName(Il2CppClass* klass, std::string_view expectedNamespace,
                  std::string_view expectedName) {
    const auto classAddress = reinterpret_cast<std::uintptr_t>(klass);
    // NativeBridge address ranges vary by emulator. Accessibility is verified
    // with process_vm_readv; a fixed >=1 TiB cutoff rejects valid Android heaps.
    Il2CppClass value{};
    if ((classAddress & 7U) != 0 ||
        !safeCopy(classAddress, &value, sizeof(value)) ||
        !value.part1.name || !value.part1.namespaze) return false;
    std::string name(expectedName.size() + 1U, '\0');
    std::string namespaze(expectedNamespace.size() + 1U, '\0');
    if (!safeCopy(reinterpret_cast<std::uintptr_t>(value.part1.name),
                  name.data(), name.size()) ||
        !safeCopy(reinterpret_cast<std::uintptr_t>(value.part1.namespaze),
                  namespaze.data(), namespaze.size())) return false;
    return std::memcmp(name.data(), expectedName.data(), expectedName.size()) == 0 &&
           name[expectedName.size()] == '\0' &&
           std::memcmp(namespaze.data(), expectedNamespace.data(),
                       expectedNamespace.size()) == 0 &&
           namespaze[expectedNamespace.size()] == '\0';
}

Il2CppClass* classFromRuntime(const char* expectedNamespace,
                             const char* expectedName) {
    using DomainGet = void* (*)();
    using DomainGetAssemblies = const void** (*)(const void*, std::size_t*);
    using AssemblyGetImage = const void* (*)(const void*);
    using ClassFromName = Il2CppClass* (*)(const void*, const char*, const char*);
    struct RuntimeApi {
        DomainGet domainGet{};
        DomainGetAssemblies domainGetAssemblies{};
        AssemblyGetImage assemblyGetImage{};
        ClassFromName classFromName{};
    };
    static const RuntimeApi api{
        reinterpret_cast<DomainGet>(dlsym(RTLD_DEFAULT, "il2cpp_domain_get")),
        reinterpret_cast<DomainGetAssemblies>(
            dlsym(RTLD_DEFAULT, "il2cpp_domain_get_assemblies")),
        reinterpret_cast<AssemblyGetImage>(
            dlsym(RTLD_DEFAULT, "il2cpp_assembly_get_image")),
        reinterpret_cast<ClassFromName>(
            dlsym(RTLD_DEFAULT, "il2cpp_class_from_name")),
    };
    if (!api.domainGet || !api.domainGetAssemblies || !api.assemblyGetImage ||
        !api.classFromName) return nullptr;

    void* domain = api.domainGet();
    if (!domain) return nullptr;
    std::size_t assemblyCount{};
    const void** assemblies =
        api.domainGetAssemblies(domain, &assemblyCount);
    if (!assemblies || assemblyCount == 0 || assemblyCount > 1024)
        return nullptr;
    for (std::size_t index = 0; index < assemblyCount; ++index) {
        const void* image = api.assemblyGetImage(assemblies[index]);
        if (!image) continue;
        if (auto* klass =
                api.classFromName(image, expectedNamespace, expectedName))
            return klass;
    }
    return nullptr;
}

Il2CppClass* matchStartClass() {
    static std::atomic<Il2CppClass*> cached{};
    if (auto* klass = cached.load(std::memory_order_acquire)) return klass;

    auto* klass = classFromSlot(gOffsets.matchStartTypeInfo, "MatchStart");
    if (!klass) klass = classFromRuntime("T3Game", "MatchStart");
    if (klass) {
        cached.store(klass, std::memory_order_release);
        logMessage(ANDROID_LOG_INFO,
                   "MatchStart resolved through IL2CPP runtime and cached");
    }
    return klass;
}

Il2CppClass* findClassInModule(std::string_view expectedNamespace,
                              std::string_view expectedName,
                              Il2CppClass* anchorClass,
                              bool requireWritable = true) {
    const std::uintptr_t moduleEnd = gModuleBase + gModuleSize;
    const auto anchorAddress = reinterpret_cast<std::uintptr_t>(anchorClass);
    constexpr std::uintptr_t maximumClassArenaDistance = 512ULL * 1024ULL * 1024ULL;
    // Collect nearby class names for diagnostics when the target is missing.
    std::vector<std::string> nearbyClasses;
    for (const auto& range : gMemoryRanges) {
        if (!range.readable) continue;
        if (requireWritable && !range.writable) continue;
        std::uintptr_t begin = std::max(range.begin, gModuleBase);
        const std::uintptr_t end = std::min(range.end, moduleEnd);
        begin = (begin + sizeof(void*) - 1U) & ~(sizeof(void*) - 1U);
        if (begin >= end) continue;
        for (std::uintptr_t slot = begin; slot + sizeof(void*) <= end;
             slot += sizeof(void*)) {
            auto* klass = *reinterpret_cast<Il2CppClass**>(slot);
            const auto classAddress = reinterpret_cast<std::uintptr_t>(klass);
            const auto arenaDistance = classAddress > anchorAddress
                ? classAddress - anchorAddress : anchorAddress - classAddress;
            if ((classAddress & 7U) != 0 ||
                arenaDistance > maximumClassArenaDistance) continue;
            // Initialized non-generic IL2CPP classes point their `klass` field
            // back to themselves. This rejects ordinary code/data pointers
            // before we inspect any string fields.
            Il2CppClass probe{};
            if (!safeCopy(classAddress, &probe, sizeof(probe)) ||
                probe.part1.klass != klass) continue;
            if (classHasName(klass, expectedNamespace, expectedName)) return klass;
            // Collect class names for diagnostics (limited to avoid log spam).
            // Only include clean ASCII names to avoid garbage entries.
            if (nearbyClasses.size() < 64) {
                std::string ns(64, '\0');
                std::string nm(64, '\0');
                if (probe.part1.namespaze &&
                    safeCopy(reinterpret_cast<std::uintptr_t>(probe.part1.namespaze),
                             ns.data(), 63) &&
                    probe.part1.name &&
                    safeCopy(reinterpret_cast<std::uintptr_t>(probe.part1.name),
                             nm.data(), 63)) {
                    ns[63] = '\0'; nm[63] = '\0';
                    auto* nsEnd = std::strchr(ns.data(), '\0');
                    auto* nmEnd = std::strchr(nm.data(), '\0');
                    if (nsEnd && nmEnd) {
                        std::string full = std::string(ns.data()) + "." + nm.data();
                        // Only include entries that look like valid identifiers.
                        bool valid = true;
                        for (char c : full) {
                            if (c == '\0') break;
                            if (!std::isalnum(static_cast<unsigned char>(c)) &&
                                c != '.' && c != '_' && c != '`' && c != '[' && c != ']')
                                { valid = false; break; }
                        }
                        if (valid) nearbyClasses.push_back(full);
                    }
                }
            }
        }
    }
    // Log nearby classes to help debug renames.
    if (!nearbyClasses.empty()) {
        std::ostringstream msg;
        msg << "Class not found: " << expectedNamespace << "." << expectedName
            << " — nearby classes:";
        for (const auto& c : nearbyClasses) msg << ' ' << c;
        logMessage(ANDROID_LOG_WARN, msg.str());
    }
    return nullptr;
}

Il2CppClass* findAnyInitializedClassInModule() {
    if (!gModuleBase || !gModuleSize) return nullptr;
    const std::uintptr_t moduleEnd = gModuleBase + gModuleSize;
    for (const auto& range : gMemoryRanges) {
        if (!range.readable || !range.writable) continue;
        std::uintptr_t begin = std::max(range.begin, gModuleBase);
        const std::uintptr_t end = std::min(range.end, moduleEnd);
        begin = (begin + sizeof(void*) - 1U) & ~(sizeof(void*) - 1U);
        for (std::uintptr_t slot = begin; slot + sizeof(void*) <= end;
             slot += sizeof(void*)) {
            Il2CppClass* klass{};
            if (!safeCopy(slot, &klass, sizeof(klass)) || !klass) continue;
            const auto address = reinterpret_cast<std::uintptr_t>(klass);
            if ((address & 7U) != 0) continue;
            Il2CppClass probe{};
            if (safeCopy(address, &probe, sizeof(probe)) &&
                probe.part1.klass == klass && probe.part1.name &&
                probe.part1.namespaze) return klass;
        }
    }
    return nullptr;
}

void* cameraControllerInstance() {
    static Il2CppClass* cachedClass{};
    Il2CppClass* klass = cachedClass;
    if (!klass) {
        klass = classFromSlot(gOffsets.cameraControllerTypeInfo,
                              "CameraController");
        if (!klass)
            klass = classFromRuntime("T3Game", "CameraController");
        if (klass) cachedClass = klass;
    }
    if (!klass) return nullptr;

    Il2CppClass value{};
    if (!safeCopy(reinterpret_cast<std::uintptr_t>(klass), &value, sizeof(value)) ||
        !value.staticFields) return nullptr;
    void* instance{};
    if (!safeCopy(reinterpret_cast<std::uintptr_t>(value.staticFields),
                  &instance, sizeof(instance)) || !instance) return nullptr;
    Il2CppClass* instanceClass{};
    if (!safeCopy(reinterpret_cast<std::uintptr_t>(instance), &instanceClass,
                  sizeof(instanceClass)) ||
        (instanceClass != klass &&
         !classHasName(instanceClass, "T3Game", "CameraController"))) return nullptr;
    return instance;
}

void updateVisualOutline(void* visualPlayerUi, bool forceVisible) {
    if (!visualPlayerUi || !gForceOpenOutline || !gForceCloseOutline) return;

    // VisualPlayerUI derives from VisualComponent. Use the game's getter so
    // its lazy UObjectRef is initialized before the stable ForceOpen/Close
    // methods are called.
    void* manager = gGetMaterialAdjust
        ? gGetMaterialAdjust(visualPlayerUi, nullptr) : nullptr;
    if (!manager) return;
    Il2CppClass* managerClass{};
    if (!safeCopy(reinterpret_cast<std::uintptr_t>(manager), &managerClass,
                  sizeof(managerClass)) ||
        !classHasName(managerClass, "T3Game", "MaterialAdjustManager")) return;

    if (forceVisible) {
        gForceOpenOutline(manager, nullptr);
        if (gForcedOutlineManagers.insert(manager).second) {
            static std::atomic<bool> logged{};
            if (!logged.exchange(true, std::memory_order_relaxed))
                appendDiagnostic(
                    "see through walls: stable ForceOpenOutline route confirmed");
        }
    } else if (gForcedOutlineManagers.erase(manager) != 0U) {
        gForceCloseOutline(manager, nullptr);
    }
}

const MethodInfo* safeFindMethod(Il2CppClass* klass, const char* name,
                                 std::uint8_t parameterCount) {
    Il2CppClass classValue{};
    if (!klass || !safeCopy(reinterpret_cast<std::uintptr_t>(klass),
                            &classValue, sizeof(classValue)) ||
        !classValue.part1.methods || classValue.part2.methodCount == 0 ||
        classValue.part2.methodCount > 2048) return nullptr;
    for (std::uint16_t index = 0; index < classValue.part2.methodCount; ++index) {
        const MethodInfo* method{};
        if (!safeCopy(reinterpret_cast<std::uintptr_t>(classValue.part1.methods) +
                          sizeof(method) * index,
                      &method, sizeof(method))) continue;
        MethodInfo methodValue{};
        if (!method || !safeCopy(reinterpret_cast<std::uintptr_t>(method),
                                 &methodValue, sizeof(methodValue)) ||
            !methodValue.name || methodValue.parametersCount != parameterCount) continue;
        const std::size_t length = std::strlen(name);
        std::string methodName(length + 1U, '\0');
        if (!safeCopy(reinterpret_cast<std::uintptr_t>(methodValue.name),
                      methodName.data(), methodName.size())) continue;
        if (std::memcmp(methodName.data(), name, length) == 0 &&
            methodName[length] == '\0')
            return method;
    }
    return nullptr;
}

bool hasHitPointContainer(void* entity) {
    return entity && gHasHitPointContainer && gHasHitPointContainer(entity, nullptr);
}

bool finite(const Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct Matrix4x4 {
    float values[16]{};
};

struct CameraProjection {
    Matrix4x4 view;
    Matrix4x4 projection;
    float viewport[4]{0.0F, 0.0F, 1.0F, 1.0F};
};

struct Vec4 {
    float x{};
    float y{};
    float z{};
    float w{};
};

Vec4 multiply(const Matrix4x4& matrix, const Vec4& value) {
    const auto& m = matrix.values;
    return {
        m[0] * value.x + m[4] * value.y + m[8] * value.z + m[12] * value.w,
        m[1] * value.x + m[5] * value.y + m[9] * value.z + m[13] * value.w,
        m[2] * value.x + m[6] * value.y + m[10] * value.z + m[14] * value.w,
        m[3] * value.x + m[7] * value.y + m[11] * value.z + m[15] * value.w,
    };
}

bool validMatrix(const Matrix4x4& matrix) {
    float magnitude{};
    for (const float value : matrix.values) {
        if (!std::isfinite(value) || std::abs(value) > 1.0e8F) return false;
        magnitude += std::abs(value);
    }
    return magnitude > 0.01F;
}

bool readProjection(void* managedCamera, CameraProjection& projection) {
    if (!managedCamera) return false;
    void* nativeCamera = *reinterpret_cast<void**>(
        static_cast<std::byte*>(managedCamera) + gOffsets.unityObjectCachedPointer);
    const auto nativeAddress = reinterpret_cast<std::uintptr_t>(nativeCamera);
    if (!nativeCamera) return false;
    std::memcpy(&projection.view,
                static_cast<std::byte*>(nativeCamera) + gOffsets.cameraViewMatrix,
                sizeof(Matrix4x4));
    std::memcpy(&projection.projection,
                static_cast<std::byte*>(nativeCamera) + gOffsets.cameraProjectionMatrix,
                sizeof(Matrix4x4));
    if (!validMatrix(projection.view) || !validMatrix(projection.projection)) return false;
    if (isReadable(nativeAddress + gOffsets.cameraViewport,
                   sizeof(projection.viewport))) {
        float viewport[4]{};
        std::memcpy(viewport,
                    static_cast<std::byte*>(nativeCamera) + gOffsets.cameraViewport,
                    sizeof(viewport));
        if (std::isfinite(viewport[0]) && std::isfinite(viewport[1]) &&
            std::isfinite(viewport[2]) && std::isfinite(viewport[3]) &&
            viewport[2] > 0.001F && viewport[3] > 0.001F &&
            std::abs(viewport[0]) < 4.0F && std::abs(viewport[1]) < 4.0F &&
            viewport[2] < 4.0F && viewport[3] < 4.0F) {
            std::memcpy(projection.viewport, viewport, sizeof(viewport));
        }
    }
    return true;
}

bool worldToScreen(const CameraProjection& camera, const Vec3& world,
                   lunary::Vec2& screen, bool& behind) {
    const Vec4 view = multiply(camera.view, {world.x, world.y, world.z, 1.0F});
    const Vec4 clip = multiply(camera.projection, view);
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
        !std::isfinite(clip.w) || std::abs(clip.w) < 1.0e-5F) return false;
    behind = view.z > -0.01F;
    float x = clip.x / clip.w;
    float y = clip.y / clip.w;
    if (!std::isfinite(x) || !std::isfinite(y) ||
        std::abs(x) > 1.0e6F || std::abs(y) > 1.0e6F) return false;
    if (behind) {
        x = -x;
        y = -y;
        if (std::abs(x) < 0.02F && std::abs(y) < 0.02F) y = -2.0F;
    }
    screen.x = camera.viewport[0] + (x + 1.0F) * 0.5F * camera.viewport[2];
    screen.y = camera.viewport[1] + (y + 1.0F) * 0.5F * camera.viewport[3];
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

bool alive(void* entity) {
    if (!entity || !gGetHealth) return false;
    auto* health = static_cast<std::byte*>(gGetHealth(entity, nullptr));
    if (!health) return false;
    const int current = *reinterpret_cast<int*>(health + gOffsets.healthCurrent);
    const int maximum = *reinterpret_cast<int*>(health + gOffsets.healthMaximum);
    return current > 0 && maximum > 0 && current <= maximum * 4;
}

bool entityPosition(void* entity, Vec3& position) {
    if (!entity || !gGetTransform) return false;
    auto* transform = static_cast<std::byte*>(gGetTransform(entity, nullptr));
    if (!transform) return false;
    std::memcpy(&position, transform + gOffsets.transformPosition, sizeof(position));
    return finite(position);
}

bool exactHitPosition(void* entity, bool wantHead, const Vec3& body, Vec3& position) {
    if (!hasHitPointContainer(entity)) return false;
    if (!gGetHitPointContainer || !gGetEntity) return false;
    auto* container = static_cast<std::byte*>(gGetHitPointContainer(entity, nullptr));
    if (!container) return false;

    HitPoint points[2]{};
    std::memcpy(&points[0], container + gOffsets.hitPoint0, sizeof(HitPoint));
    std::memcpy(&points[1], container + gOffsets.hitPoint1, sizeof(HitPoint));
    for (const auto& point : points) {
        if (point.isHead != wantHead || point.entityId == 0) continue;
        void* hitEntity = gGetEntity(point.entityId, false, nullptr);
        Vec3 exact{};
        if (!hitEntity || !entityPosition(hitEntity, exact)) continue;
        if (distanceSquared(body, exact) > 25.0F) continue;
        position = exact;
        return true;
    }
    return false;
}

bool healthValues(void* entity, int& current, int& maximum) {
    if (!entity || !gGetHealth) return false;
    auto* health = static_cast<std::byte*>(gGetHealth(entity, nullptr));
    if (!health) return false;
    current = *reinterpret_cast<int*>(health + gOffsets.healthCurrent);
    maximum = *reinterpret_cast<int*>(health + gOffsets.healthMaximum);
    return maximum > 0 && maximum < 100000000 && current >= 0 &&
           current <= maximum * 4;
}

// Reads a standard Unity Il2CppString* from memory.
// Il2CppString layout (64-bit ARM Android): length at +0x10, UTF-16 chars at +0x14.
// The strPtr parameter is the Il2CppString* object (not a pointer to the chars).
std::string il2cppReadString(void* strPtr) {
    if (!strPtr) return {};
    const auto addr = reinterpret_cast<std::uintptr_t>(strPtr);
    if (addr < 0x100000 || addr > 0x7FFFFFFFFFFFULL) return {};
    // Read int32 length at offset 0x10 from the Il2CppString object
    std::int32_t length = 0;
    if (!safeCopy(addr + 0x10, &length, sizeof(length))) return {};
    if (length <= 0 || length > 256) return {};
    // Read UTF-16 chars at offset 0x14 from the Il2CppString object
    std::vector<char16_t> utf16(static_cast<std::size_t>(length));
    if (!safeCopy(addr + 0x14, utf16.data(), static_cast<std::size_t>(length) * sizeof(char16_t)))
        return {};
    // Convert UTF-16 to UTF-8 (including surrogate pairs)
    std::string result;
    result.reserve(static_cast<std::size_t>(length));
    for (std::size_t i = 0; i < utf16.size(); ++i) {
        std::uint32_t cp = utf16[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < utf16.size()) {
            std::uint32_t low = utf16[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        if (cp < 0x80) {
            result.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return result;
}

void readBonePositions(void* transform, const CameraProjection& camera,
        std::ostringstream& boneOut, int maxBones) {
    if (!transform || !gTransformGetChildCount || !gTransformGetChild || !gTransformGetPosition)
        return;
    const int childCount = gTransformGetChildCount(transform, nullptr);
    if (childCount <= 0 || childCount > 64) return;
    const int limit = std::min(childCount, maxBones);
    for (int i = 0; i < limit; ++i) {
        void* child = gTransformGetChild(transform, i, nullptr);
        if (!child) continue;
        Vec3 pos{};
        gTransformGetPosition(child, &pos, nullptr);
        if (!finite(pos)) continue;
        lunary::Vec2 screen{};
        bool behind{};
        worldToScreen(camera, pos, screen, behind);
        if (!std::isfinite(screen.x) || !std::isfinite(screen.y)) continue;
        // Hex-encode screen position
        auto writeHex = [&](float v) {
            std::uint32_t raw{};
            std::memcpy(&raw, &v, sizeof(raw));
            constexpr char digits[] = "0123456789ABCDEF";
            boneOut << digits[(raw >> 28) & 0xF] << digits[(raw >> 24) & 0xF]
                    << digits[(raw >> 20) & 0xF] << digits[(raw >> 16) & 0xF]
                    << digits[(raw >> 12) & 0xF] << digits[(raw >> 8) & 0xF]
                    << digits[(raw >> 4) & 0xF] << digits[raw & 0xF];
        };
        writeHex(screen.x);
        writeHex(screen.y);
    }
}

void publishTelemetrySnapshot() {
    lunary::Snapshot snapshot;
    snapshot.sequence = gHookFrames.load(std::memory_order_relaxed);
    snapshot.timestampMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    auto* matchClass = matchStartClass();
    if (!matchClass || !matchClass->staticFields) {
        telemetry::publish(std::move(snapshot));
        static std::atomic<int> warnCount{};
        if ((warnCount.fetch_add(1) & 63) == 0) {
            logMessage(ANDROID_LOG_WARN,
                       "telemetry: MatchStart TypeInfo not ready (staticFields missing)");
        }
        return;
    }
    void* matchStart = *static_cast<void**>(matchClass->staticFields);
    {
        static std::atomic<int> diagCount{};
        if ((diagCount.fetch_add(1) & 255) == 0) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "telemetry diag: matchClass=%p staticFields=%p matchStart=%p +0xC8=%p "
                     "readable=%d",
                     reinterpret_cast<void*>(matchClass),
                     matchClass->staticFields, matchStart,
                     matchStart ? reinterpret_cast<void*>(
                         reinterpret_cast<std::uintptr_t>(matchStart) +
                         gOffsets.matchStartUiHealthList)
                                : nullptr,
                     matchStart ? isReadable(reinterpret_cast<std::uintptr_t>(matchStart) +
                                                 gOffsets.matchStartUiHealthList,
                                             sizeof(void*))
                                : 0);
            logMessage(ANDROID_LOG_INFO, buf);
        }
    }
    if (!matchStart) {
        telemetry::publish(std::move(snapshot));
        return;
    }
    // Read match mode and status from static fields
    snapshot.matchMode = *reinterpret_cast<int*>(
        static_cast<std::byte*>(matchClass->staticFields) + gOffsets.matchStartMatchMode);
    snapshot.matchStatus = *reinterpret_cast<int*>(
        static_cast<std::byte*>(matchClass->staticFields) + gOffsets.matchStartStatus);

    // Build entity ? VisualPlayerUI map for hero names and status flags
    struct VPLink { std::uint64_t entityAddr; void* vp; };
    std::vector<VPLink> vpLinks;
    auto* vpList = *reinterpret_cast<ManagedList**>(
        static_cast<std::byte*>(matchStart) + gOffsets.matchStartAllVisualPlayerUIList);
    if (vpList && vpList->items && vpList->size > 0 && vpList->size <= 128 &&
        vpList->items->maxLength >= static_cast<std::uintptr_t>(vpList->size)) {
        vpLinks.reserve(static_cast<std::size_t>(vpList->size));
        for (int i = 0; i < vpList->size; ++i) {
            auto* vp = static_cast<std::byte*>(vpList->items->values[i]);
            if (!vp) continue;
            void* vpEntity = *reinterpret_cast<void**>(vp + gOffsets.visualPlayerUIEntity);
            if (!vpEntity) continue;
            vpLinks.push_back({reinterpret_cast<std::uint64_t>(vpEntity), vp});
        }
    }
    auto* list = *reinterpret_cast<ManagedList**>(
        static_cast<std::byte*>(matchStart) + gOffsets.matchStartUiHealthList);
    if (!list || !list->items || list->size <= 0 || list->size > 128 ||
        list->items->maxLength < static_cast<std::uintptr_t>(list->size)) {
        telemetry::publish(std::move(snapshot));
        static std::atomic<int> warnCount3{};
        if ((warnCount3.fetch_add(1) & 63) == 0) {
            logMessage(ANDROID_LOG_WARN,
                       "telemetry: uiHealthList invalid (size=" +
                       (list ? std::to_string(list->size) : "null") + ")");
        }
        return;
    }

    Vec3 localPosition{};
    bool haveLocalPosition{};
    CameraProjection camera{};
    bool haveCamera{};
    for (int index = 0; index < list->size; ++index) {
        auto* ui = static_cast<std::byte*>(list->items->values[index]);
        if (!ui) continue;
        void* entity = *reinterpret_cast<void**>(ui + gOffsets.uiHealthEntity);
        if (!entity) continue;
        if (*(ui + gOffsets.uiHealthIsLocal) != std::byte{0}) {
            snapshot.localEntity = reinterpret_cast<std::uint64_t>(entity);
            haveLocalPosition = entityPosition(entity, localPosition);
        }
        if (!haveCamera) {
            void* managedCamera = *reinterpret_cast<void**>(ui + gOffsets.uiHealthCamera);
            haveCamera = readProjection(managedCamera, camera);
        }
    }

    snapshot.entities.reserve(static_cast<std::size_t>(list->size));
    int diagTotal{};
    int diagHasEntity{};
    int diagPassPos{};
    int diagPassHealth{};
    int diagAlive{};
    for (int index = 0; index < list->size; ++index) {
        auto* ui = static_cast<std::byte*>(list->items->values[index]);
        if (!ui) continue;
        ++diagTotal;
        void* entity = *reinterpret_cast<void**>(ui + gOffsets.uiHealthEntity);
        if (!entity) continue;
        ++diagHasEntity;
        Vec3 position{};
        int current{};
        int maximum{};
        if (!entityPosition(entity, position)) continue;
        ++diagPassPos;
        if (!healthValues(entity, current, maximum)) continue;
        ++diagPassHealth;
        if (current <= 0) continue;
        ++diagAlive;

        lunary::EntityTelemetry output;
        output.address = reinterpret_cast<std::uint64_t>(entity);
        output.entityId = *reinterpret_cast<std::uint16_t*>(
            ui + gOffsets.uiHealthEntityId);
        output.isLocal = *(ui + gOffsets.uiHealthIsLocal) != std::byte{0};
        output.isPlayer = true;
        output.health = current;
        output.maxHealth = maximum;
        output.position = {position.x, position.y, position.z};
        output.teamKnown = true;
        output.isTeammate = !output.isLocal &&
            *(ui + gOffsets.uiHealthIsSameTeam) != std::byte{0};
        output.isFriend = *(ui + gOffsets.uiHealthFriend) != std::byte{0};
        output.teamIndex = *reinterpret_cast<std::int32_t*>(
            ui + gOffsets.uiHealthInitTeam);

        // Nome e heroi pertencem ao PlayerComponent da Entity. Os offsets
        // +0x20/+0x38 que eram lidos em UIPlayerHealthComponent apontavam para
        // outro layout e, por isso, sempre produziam strings vazias.
        if (gGetPlayer) {
            auto* player = static_cast<std::byte*>(gGetPlayer(entity, nullptr));
            if (player) {
                void* displayStr{};
                void* heroStr{};
                safeCopy(reinterpret_cast<std::uintptr_t>(player) +
                             gOffsets.playerDisplayName,
                         &displayStr, sizeof(displayStr));
                safeCopy(reinterpret_cast<std::uintptr_t>(player) +
                             gOffsets.playerHeroName,
                         &heroStr, sizeof(heroStr));
                output.displayName = il2cppReadString(displayStr);
                output.heroName = il2cppReadString(heroStr);
            }
        }

        // Entity ID comes from UIHealth. Feature updates run once in the main
        // frame hook; running them here duplicated all component work at the
        // telemetry rate and caused periodic frame stalls.
        if (output.isLocal) {
            auto& featureManager =
                lunary::injected::FeatureManager::instance();
            featureManager.observeLocalEntityId(output.entityId);
        }
        if (haveLocalPosition) output.distance = std::sqrt(distanceSquared(localPosition, position));

        if (haveCamera) {
            bool behind{};
            if (worldToScreen(camera, position, output.screenPosition, behind)) {
                output.hasScreenPosition = true;
                output.screenPositionNormalized = true;
                output.behindCamera = behind;
            }
            Vec3 head = position;
            head.y += 2.0F;
            bool headBehind{};
            if (worldToScreen(camera, head, output.headScreenPosition, headBehind) &&
                headBehind == behind) output.hasHeadScreenPosition = true;
        } else {
            lunary::Vec2 blood{};
            std::memcpy(&blood, ui + gOffsets.uiHealthBloodPosition, sizeof(blood));
            if (std::isfinite(blood.x) && std::isfinite(blood.y) &&
                std::abs(blood.x) < 100000.0F && std::abs(blood.y) < 100000.0F &&
                (blood.x != 0.0F || blood.y != 0.0F)) {
                output.screenPosition = blood;
                output.hasScreenPosition = true;
            }
        }
        // Look up VisualPlayerUI for hero name and status flags
        for (const auto& link : vpLinks) {
            if (link.entityAddr != reinterpret_cast<std::uint64_t>(entity)) continue;
            auto* vp = static_cast<std::byte*>(link.vp);
            if (output.isLocal) {
                lunary::injected::FeatureManager::instance()
                    .observeLocalVisualPlayer(vp);
            }
            const auto& visualConfig =
                lunary::injected::FeatureManager::instance().config();
            updateVisualOutline(
                vp, visualConfig.enabled && visualConfig.seeThroughWalls &&
                        !output.isLocal);
            // Invisibility / vanish / agonal flags
            output.isInvisible = *(vp + gOffsets.visualPlayerUIIsInvisibilityBuff) != std::byte{0};
            output.isVanish = *(vp + gOffsets.visualPlayerUIIsVanish) != std::byte{0};
            if (*(vp + gOffsets.visualPlayerUIIsAgonal) != std::byte{0})
                output.isVanish = true; // treat agonal same as vanish for rendering
            // Fallback do heroi para modos onde PlayerComponent ainda nao foi
            // replicado, mas VisualPlayerUI.HeroCfg ja esta disponivel.
            if (output.heroName.empty()) {
                void* heroCfg{};
                void* heroStr{};
                safeCopy(reinterpret_cast<std::uintptr_t>(vp) +
                             gOffsets.visualPlayerUIHeroCfg,
                         &heroCfg, sizeof(heroCfg));
                if (heroCfg) {
                    safeCopy(reinterpret_cast<std::uintptr_t>(heroCfg) +
                                 gOffsets.heroConfigName,
                             &heroStr, sizeof(heroStr));
                    output.heroName = il2cppReadString(heroStr);
                }
            }
            break;
        }
        // Read bone positions from entity Transform (only with camera for projection)
        const auto& visualConfig =
            lunary::injected::FeatureManager::instance().config();
        if (visualConfig.enabled && visualConfig.espEnabled &&
            visualConfig.espSkeleton && entity && gGetTransform && haveCamera) {
            void* entityTransform = gGetTransform(entity, nullptr);
            if (entityTransform) {
                std::ostringstream boneStr;
                readBonePositions(entityTransform, camera, boneStr, 8);
                output.boneData = boneStr.str();
            }
        }

        snapshot.entities.push_back(std::move(output));
    }
    {
        static std::atomic<int> diagCounter{};
        if ((diagCounter.fetch_add(1) & 63) == 0) {
            std::ostringstream msg;
            msg << "telemetry: " << snapshot.entities.size() << " entities, local="
                << (haveLocalPosition ? "yes" : "no")
                << " camera=" << (haveCamera ? "yes" : "no")
                << " | list=" << diagTotal << " ent=" << diagHasEntity
                << " pos=" << diagPassPos << " hp=" << diagPassHealth
                << " alive=" << diagAlive;
            logMessage(ANDROID_LOG_INFO, msg.str());
        }
    }
    telemetry::publish(std::move(snapshot));
}

struct Candidate {
    void* entity{};
    Vec2 desired{};
    Vec3 exact{};
    float angularDistance{std::numeric_limits<float>::max()};
    bool swappedAxes{};
};

bool aimAtBestTarget(void* camera) {
    static std::atomic<int> aimbotDiagCount{};
    const bool diag = (aimbotDiagCount.fetch_add(1) & 1023) == 0;

    if (diag) {
        __android_log_print(ANDROID_LOG_INFO, "LunaryAim", "aimAtBestTarget: ENTER camera=0x%zx", (uintptr_t)camera);
    }

    auto* matchClass = matchStartClass();
    if (!matchClass || !matchClass->staticFields) {
        if (diag) __android_log_print(
            ANDROID_LOG_INFO, "LunaryAim",
            "aimAtBestTarget: FAIL no matchClass or staticFields");
        return false;
    }
    void* matchStart = *static_cast<void**>(matchClass->staticFields);
    if (!matchStart) {
        if (diag) __android_log_print(ANDROID_LOG_INFO, "LunaryAim", "aimAtBestTarget: FAIL matchStart is NULL");
        return false;
    }
    auto* list = *reinterpret_cast<ManagedList**>(
        static_cast<std::byte*>(matchStart) + gOffsets.matchStartUiHealthList);
    if (!list || !list->items || list->size <= 0 || list->size > 128 ||
        list->items->maxLength < static_cast<std::uintptr_t>(list->size)) {
        if (diag) {
            __android_log_print(ANDROID_LOG_INFO, "LunaryAim", "aimAtBestTarget: FAIL uiHealthList invalid list=%p items=%p size=%d",
                       (void*)list, list ? (void*)list->items : nullptr, list ? list->size : -1);
        }
        return false;
    }

    void* localEntity{};
    Vec3 localPosition{};
    for (int index = 0; index < list->size; ++index) {
        auto* ui = static_cast<std::byte*>(list->items->values[index]);
        if (!ui || *(ui + gOffsets.uiHealthIsLocal) == std::byte{0}) continue;
        localEntity = *reinterpret_cast<void**>(ui + gOffsets.uiHealthEntity);
        if (localEntity && entityPosition(localEntity, localPosition)) break;
        localEntity = nullptr;
    }
    if (!localEntity) {
        if (diag) __android_log_print(ANDROID_LOG_INFO, "LunaryAim", "aimAtBestTarget: FAIL local player not found in uiHealthList (size=%d)", list->size);
        return false;
    }
    localPosition.y += gEyeHeight.load(std::memory_order_relaxed);

    Vec2 current{};
    std::memcpy(&current, static_cast<std::byte*>(camera) + gOffsets.cameraViewAngle,
                sizeof(current));
    if (!std::isfinite(current.x) || !std::isfinite(current.y)) {
        if (diag) __android_log_print(ANDROID_LOG_INFO, "LunaryAim", "aimAtBestTarget: FAIL non-finite view angle (%.4f, %.4f)", current.x, current.y);
        return false;
    }

    Candidate nearest;
    Candidate locked;
    Candidate closestAny;
    const float fov = gFovDegrees.load(std::memory_order_relaxed);
    const float maximumDistance = gMaximumDistance.load(std::memory_order_relaxed);
    const bool wantHead = gUseHead.load(std::memory_order_relaxed);

    for (int index = 0; index < list->size; ++index) {
        auto* ui = static_cast<std::byte*>(list->items->values[index]);
        if (!ui || *(ui + gOffsets.uiHealthIsLocal) != std::byte{0}) continue;
        if (gIgnoreTeammates.load(std::memory_order_relaxed) &&
            *(ui + gOffsets.uiHealthIsSameTeam) != std::byte{0}) continue;
        void* entity = *reinterpret_cast<void**>(ui + gOffsets.uiHealthEntity);
        if (!entity || entity == localEntity || !alive(entity)) continue;

        Vec3 body{};
        Vec3 exact{};
        if (!entityPosition(entity, body)) continue;
        // Prefer the animated collision entity. Some heroes/modes do not
        // expose HitPointContainer on every frame; keep the aim operational
        // with the same body/head anchors used by the ESP until it appears.
        if (!exactHitPosition(entity, wantHead, body, exact)) {
            exact = body;
            exact.y += wantHead ? 2.0F : 1.0F;
        }
        const float worldDistanceSquared = distanceSquared(localPosition, exact);
        if (maximumDistance > 0.0F &&
            worldDistanceSquared > maximumDistance * maximumDistance) continue;
        const Vec2 yawPitch = lookAngle(localPosition, exact);
        const Vec2 pitchYaw{yawPitch.y, yawPitch.x};
        const Vec2 yawPitchDelta = angleDelta(current, yawPitch);
        const Vec2 pitchYawDelta = angleDelta(current, pitchYaw);
        const float yawPitchDistance =
            std::hypot(yawPitchDelta.x, yawPitchDelta.y);
        const float pitchYawDistance =
            std::hypot(pitchYawDelta.x, pitchYawDelta.y);
        const bool swappedAxes = pitchYawDistance < yawPitchDistance;
        const Vec2 desired = swappedAxes ? pitchYaw : yawPitch;
        const float angularDistance =
            swappedAxes ? pitchYawDistance : yawPitchDistance;
        Candidate candidate{entity, desired, exact, angularDistance, swappedAxes};
        if (angularDistance < closestAny.angularDistance) closestAny = candidate;
        if (entity == gLockedEntity.load(std::memory_order_relaxed)) locked = candidate;
        if (angularDistance <= fov && angularDistance < nearest.angularDistance)
            nearest = candidate;
    }

    Candidate selected = gLockTarget.load(std::memory_order_relaxed) && locked.entity
        ? locked : nearest;
    if (!selected.entity) {
        gLockedEntity.store(nullptr, std::memory_order_relaxed);
        if (diag) {
            __android_log_print(
                ANDROID_LOG_INFO, "LunaryAim",
                "aimAtBestTarget: no enemy in FOV=%.1f current=(%.1f,%.1f) "
                "closest=%.1f desired=(%.1f,%.1f) axes=%s list=%d",
                fov, current.x, current.y, closestAny.angularDistance,
                closestAny.desired.x, closestAny.desired.y,
                closestAny.swappedAxes ? "pitch/yaw" : "yaw/pitch", list->size);
        }
        return false;
    }
    gLockedEntity.store(selected.entity, std::memory_order_relaxed);
    const auto corrected = smoothAngle(
        current, selected.desired, gSmoothing.load(std::memory_order_relaxed));
    std::memcpy(static_cast<std::byte*>(camera) + gOffsets.cameraViewAngle,
                &corrected, sizeof(corrected));

    // CameraController only changes what the player sees. AimComponent.direct
    // is the simulation/shoot direction consumed by weapon actions. Use the
    // game's own extension with needDirty=true so the ECS change follows its
    // normal serialization path.
    const Vec3 toTarget{selected.exact.x - localPosition.x,
                        selected.exact.y - localPosition.y,
                        selected.exact.z - localPosition.z};
    const float directionLength = std::sqrt(distanceSquared({}, toTarget));
    if (directionLength > 0.0001F && gGetAim && gSetAimDirect) {
        void* aim = gGetAim(localEntity, nullptr);
        if (aim) {
            const Vec3 direction{toTarget.x / directionLength,
                                 toTarget.y / directionLength,
                                 toTarget.z / directionLength};
            (void)gSetAimDirect(aim, direction, true, nullptr);
            static std::atomic<bool> directLogged{};
            if (!directLogged.exchange(true))
                appendDiagnostic("aimbot: AimComponent.SetDirect dirty route confirmed");
        }
    }
    if (diag) {
        __android_log_print(ANDROID_LOG_INFO, "LunaryAim", "aimAtBestTarget: LOCKED target=0x%zx angDist=%.2f smooth=(%.2f,%.2f)",
                   (uintptr_t)selected.entity, selected.angularDistance, corrected.x, corrected.y);
    }
    return true;
}

void hookedCameraUpdate(void* frameOwner, const MethodInfo* method) {
    const bool diagnosticFrame =
        gHookFrames.fetch_add(1, std::memory_order_relaxed) == 0;
    if (diagnosticFrame) appendDiagnostic("frame hook: entry");

    auto& fm = lunary::injected::FeatureManager::instance();
    auto& cfg = fm.config();
    if (diagnosticFrame) appendDiagnostic("frame hook: feature manager ready");
    void* camera = cameraControllerInstance();
    static std::atomic<unsigned> missingCameraFrames{};
    if (!camera &&
        (missingCameraFrames.fetch_add(1, std::memory_order_relaxed) & 255U) == 0U)
        appendDiagnostic("aimbot: CameraController.Instance not ready");

    // ---- PRE-CAMERA: Save local view angle for visual no-recoil ----
    float savedViewAngleX = 0.0f, savedViewAngleY = 0.0f;
    bool needRestoreViewAngle = false;
    if (cfg.enabled && camera &&
        fm.isEnabled(lunary::injected::FeatureId::NoRecoil)) {
        auto* camBytes = static_cast<std::byte*>(camera);
        std::memcpy(&savedViewAngleX, camBytes + gOffsets.cameraViewAngle, sizeof(float));
        std::memcpy(&savedViewAngleY, camBytes + gOffsets.cameraViewAngle + 4, sizeof(float));
        needRestoreViewAngle = true;
    }

    // ---- Call original GameClient.UpdateVisualObjects frame callback ----
    const auto original = reinterpret_cast<CameraUpdateFunction>(
        gOriginalCameraUpdate.load(std::memory_order_acquire));
    if (diagnosticFrame) appendDiagnostic("frame hook: calling trampoline");
    if (original) original(frameOwner, method);
    if (diagnosticFrame) appendDiagnostic("frame hook: trampoline returned");

    // UpdateCamera consome a entrada e reescreve m_ViewAngle. Aplicar a
    // correcao antes do trampoline fazia a mira ser perdida no mesmo frame.
    if (camera && gAimEnabled.load(std::memory_order_relaxed)) {
        aimAtBestTarget(camera);
    }

    // ---- POST-CAMERA: Restore only local camera state ----
    if (needRestoreViewAngle) {
        auto* camBytes = static_cast<std::byte*>(camera);
        // Only restore if not using aimbot (avoid fighting each other)
        if (!gAimEnabled.load(std::memory_order_relaxed)) {
            std::memcpy(camBytes + gOffsets.cameraViewAngle, &savedViewAngleX, sizeof(float));
            std::memcpy(camBytes + gOffsets.cameraViewAngle + 4, &savedViewAngleY, sizeof(float));
        }
    }

    // ---- Feature update on local player (every frame) ----
    if (cfg.enabled && gGetLocalPlayerEntity) {
        void* localPlayer = gGetLocalPlayerEntity(nullptr);
        if (localPlayer) {
            fm.onFrame(localPlayer);
        }
    }

    if (!gTrampolineObserved.exchange(true, std::memory_order_relaxed)) {
        logMessage(ANDROID_LOG_INFO, "hook entered; original trampoline returned");
    }

    // ---- Telemetry publishing (20fps) ----
    const auto nowMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    auto previousTelemetry = gLastTelemetryMs.load(std::memory_order_relaxed);
    if (nowMs - previousTelemetry >= 50U &&
        gLastTelemetryMs.compare_exchange_strong(
            previousTelemetry, nowMs, std::memory_order_relaxed)) {
        publishTelemetrySnapshot();
    }
}

bool resolveAndInstall(std::string& error) {
    if (!findModule()) {
        error = "libil2cpp.so is not loaded";
        return false;
    }
    telemetry::start(gModuleBase);
    // Telemetry resolves MatchStart independently on every published frame.
    // A class that is not initialized yet must not block the camera detour.
    if (!classFromSlot(gOffsets.matchStartTypeInfo, "MatchStart")) {
        logMessage(ANDROID_LOG_WARN,
                   "MatchStart TypeInfo not ready; installing frame hook anyway");
    }

    const std::uintptr_t requiredRvas[] = {
        gOffsets.matchStartGetEntity,
        gOffsets.entityGetHealth,
        gOffsets.entityGetTransform,
        gOffsets.entityGetPlayer,
        gOffsets.entityHasHitPointContainer,
        gOffsets.entityGetHitPointContainer,
        gOffsets.transformGetChildCount,
        gOffsets.transformGetChild,
        gOffsets.transformGetPosition,
        lunary::injected::offsets::RVA_Entity_GetAim,
        lunary::injected::offsets::RVA_AimComponent_SetDirect,
        lunary::injected::offsets::RVA_VisualComponent_GetMaterialAdjust,
    };
    for (const auto rva : requiredRvas) {
        if (rva >= gModuleSize || !isReadable(gModuleBase + rva, sizeof(std::uint32_t))) {
            error = "one or more configured function RVAs are outside libil2cpp.so";
            return false;
        }
    }

    // The protected CameraController.UpdateCamera body is encrypted in the
    // mapping. Hook a stable outer frame method and resolve the camera
    // singleton inside the callback instead.
    const std::uintptr_t target = gModuleBase + gOffsets.cameraUpdate;
    logMessage(ANDROID_LOG_INFO,
               "frame callback: hooking GameClient.UpdateVisualObjects via RVA");

    if (target < gModuleBase || target >= gModuleBase + gModuleSize) {
        error = "frame callback RVA is outside libil2cpp.so";
        return false;
    }
    gGetEntity = reinterpret_cast<GetEntityFunction>(
        gModuleBase + gOffsets.matchStartGetEntity);
    gGetHealth = reinterpret_cast<GetComponentFunction>(
        gModuleBase + gOffsets.entityGetHealth);
    gGetTransform = reinterpret_cast<GetComponentFunction>(
        gModuleBase + gOffsets.entityGetTransform);
    gGetPlayer = reinterpret_cast<GetComponentFunction>(
        gModuleBase + gOffsets.entityGetPlayer);
    gGetAim = reinterpret_cast<GetComponentFunction>(
        gModuleBase + lunary::injected::offsets::RVA_Entity_GetAim);
    gGetMaterialAdjust = reinterpret_cast<GetComponentFunction>(
        gModuleBase +
        lunary::injected::offsets::RVA_VisualComponent_GetMaterialAdjust);
    gSetAimDirect = reinterpret_cast<SetAimDirectFunction>(
        gModuleBase + lunary::injected::offsets::RVA_AimComponent_SetDirect);
    gForceOpenOutline = reinterpret_cast<MaterialOutlineFunction>(
        gModuleBase + lunary::injected::offsets::RVA_MaterialAdjust_ForceOpenOutline);
    gForceCloseOutline = reinterpret_cast<MaterialOutlineFunction>(
        gModuleBase + lunary::injected::offsets::RVA_MaterialAdjust_ForceCloseOutline);
    gHasHitPointContainer = reinterpret_cast<HasComponentFunction>(
        gModuleBase + gOffsets.entityHasHitPointContainer);
    gGetHitPointContainer = reinterpret_cast<GetComponentFunction>(
        gModuleBase + gOffsets.entityGetHitPointContainer);
    gTransformGetChildCount = reinterpret_cast<TransformGetChildCountFunc>(
        gModuleBase + gOffsets.transformGetChildCount);
    gTransformGetChild = reinterpret_cast<TransformGetChildFunc>(
        gModuleBase + gOffsets.transformGetChild);
    gTransformGetPosition = reinterpret_cast<TransformGetPositionFunc>(
        gModuleBase + gOffsets.transformGetPosition);
    gGetLocalPlayerEntity = reinterpret_cast<GetLocalPlayerEntityFunc>(
        gModuleBase + lunary::injected::offsets::RVA_GetLocalPlayerEntity_Static);
    FeatureManager::instance().initializeRuntime(gModuleBase);
    if (!gDetour.install(reinterpret_cast<void*>(target),
                         reinterpret_cast<void*>(&hookedCameraUpdate),
                         &gOriginalCameraUpdate, error)) return false;
    gResolvedCameraUpdate.store(target, std::memory_order_release);

    std::ostringstream message;
    message << "GameClient.UpdateVisualObjects detoured at 0x" << std::hex << target
            << "; trampoline 0x"
            << reinterpret_cast<std::uintptr_t>(
                   gOriginalCameraUpdate.load(std::memory_order_acquire));
    logMessage(ANDROID_LOG_INFO, message.str());

    // Recoil/spread use direct function-entry detours. Gameplay state and the
    // server path remain original; replacements only suppress client visuals
    // or compensate the returned shot rotation.
    if (!installFeatureHooks(gModuleBase, error)) {
        logMessage(ANDROID_LOG_WARN, "feature hooks: " + error);
        // Non-fatal: visual no-recoil still has the CameraController restore.
    }

    return true;
}

bool installWithRetry() {
    std::scoped_lock lifecycleLock(gLifecycleMutex);
    if (gDetour.installed()) return true;
    std::string error;
    for (int attempt = 0; attempt < 600; ++attempt) {
        if (resolveAndInstall(error)) return true;
        if (attempt == 0 || (attempt + 1) % 100 == 0) {
            logMessage(ANDROID_LOG_WARN, "detour pending: " + error);
            appendDiagnostic(("detour pending: " + error).c_str());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    setError(error);
    return false;
}

void startInstaller() {
    std::call_once(gStartOnce, [] {
        // Install crash handler early
        struct sigaction sa{};
        sa.sa_sigaction = [](int sig, siginfo_t* info, void* context) {
            std::uintptr_t pc{};
            std::uintptr_t sp{};
            std::uintptr_t lr{};
#if defined(__aarch64__)
            if (context) {
                const auto* ucontext = static_cast<const ucontext_t*>(context);
                pc = static_cast<std::uintptr_t>(ucontext->uc_mcontext.pc);
                sp = static_cast<std::uintptr_t>(ucontext->uc_mcontext.sp);
                lr = static_cast<std::uintptr_t>(ucontext->uc_mcontext.regs[30]);
            }
#elif defined(__x86_64__) && defined(REG_RIP)
            if (context) {
                const auto* ucontext = static_cast<const ucontext_t*>(context);
                pc = static_cast<std::uintptr_t>(ucontext->uc_mcontext.gregs[REG_RIP]);
                sp = static_cast<std::uintptr_t>(ucontext->uc_mcontext.gregs[REG_RSP]);
            }
#endif
            char diagnostic[320]{};
            const auto pcRva = pc >= gModuleBase ? pc - gModuleBase : 0;
            const auto lrRva = lr >= gModuleBase ? lr - gModuleBase : 0;
            snprintf(diagnostic, sizeof(diagnostic),
                     "CRASH: signal=%d addr=%p code=%d pc=%p pc_rva=0x%zx "
                     "lr=%p lr_rva=0x%zx sp=%p",
                     sig, info ? info->si_addr : nullptr, info ? info->si_code : 0,
                     reinterpret_cast<void*>(pc), pcRva,
                     reinterpret_cast<void*>(lr), lrRva,
                     reinterpret_cast<void*>(sp));
            appendDiagnostic("CRASH: injected signal handler entered");
            appendDiagnostic(diagnostic);
            __android_log_print(ANDROID_LOG_ERROR, "LunaryHook", "%s", diagnostic);

            // Let Android's debuggerd receive the original fatal signal after
            // our compact context line, otherwise _exit(1) suppresses the
            // tombstone and hides the crashing instruction.
            struct sigaction defaultAction{};
            defaultAction.sa_handler = SIG_DFL;
            sigemptyset(&defaultAction.sa_mask);
            sigaction(sig, &defaultAction, nullptr);
            syscall(SYS_tgkill, getpid(), syscall(SYS_gettid), sig);
            _exit(128 + sig);
        };
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGBUS, &sa, nullptr);
        sigaction(SIGILL, &sa, nullptr);

        logMessage(ANDROID_LOG_INFO,
                   "hook process pid=" + std::to_string(getpid()));
        std::string initialConfiguration = loadConfiguration();
        logMessage(ANDROID_LOG_INFO, "starting in-process CameraController detour");
        std::thread([] { installWithRetry(); }).detach();
        std::thread([configuration = std::move(initialConfiguration)]() mutable {
            watchConfiguration(std::move(configuration));
        }).detach();
    });
}

} // namespace
} // namespace lunary::injected

extern "C" __attribute__((visibility("default"))) bool lunary_install_hook() {
    return lunary::injected::installWithRetry();
}

extern "C" __attribute__((visibility("default"))) bool lunary_uninstall_hook() {
    using namespace lunary::injected;
    std::scoped_lock lifecycleLock(gLifecycleMutex);
    gAimEnabled = false;
    std::string error;
    uninstallFeatureHooks(error);
    if (!error.empty()) {
        setError(error);
        return false;
    }
    if (!gDetour.uninstall(error)) {
        setError(error);
        return false;
    }
    gOriginalCameraUpdate.store(nullptr, std::memory_order_release);
    gLockedEntity.store(nullptr, std::memory_order_relaxed);
    return true;
}

extern "C" __attribute__((visibility("default"))) void
lunary_set_aim_enabled(bool enabled) {
    lunary::injected::gAimEnabled.store(enabled, std::memory_order_relaxed);
    if (!enabled)
        lunary::injected::gLockedEntity.store(nullptr, std::memory_order_relaxed);
}

extern "C" __attribute__((visibility("default"))) void
lunary_set_aim_options(float fovDegrees, float smoothing, float maximumDistance,
                       bool useHead) {
    using namespace lunary::injected;
    if (std::isfinite(fovDegrees)) gFovDegrees = std::clamp(fovDegrees, 1.0F, 180.0F);
    if (std::isfinite(smoothing)) gSmoothing = std::clamp(smoothing, 1.0F, 30.0F);
    if (std::isfinite(maximumDistance)) gMaximumDistance = std::max(0.0F, maximumDistance);
    gUseHead = useHead;
}

extern "C" __attribute__((visibility("default"))) const char* lunary_hook_last_error() {
    using namespace lunary::injected;
    std::scoped_lock lock(gErrorMutex);
    return gLastError.c_str();
}

extern "C" __attribute__((visibility("default"))) jint
JNI_OnLoad(JavaVM* vm, void*) {
    appendDiagnostic("JNI_OnLoad ENTRY");
    __android_log_print(ANDROID_LOG_INFO, "LunaryHook", "JNI_OnLoad ENTRY vm=%p", (void*)vm);

    appendDiagnostic("JNI_OnLoad calling startInstaller");
    __android_log_print(ANDROID_LOG_INFO, "LunaryHook", "Calling startInstaller()...");
    lunary::injected::startInstaller();
    appendDiagnostic("JNI_OnLoad startInstaller returned");
    __android_log_print(ANDROID_LOG_INFO, "LunaryHook", "startInstaller() returned");

    appendDiagnostic("JNI_OnLoad EXIT");
    __android_log_print(ANDROID_LOG_INFO, "LunaryHook", "JNI_OnLoad EXIT returning 0x%x", JNI_VERSION_1_6);
    return JNI_VERSION_1_6;
}
