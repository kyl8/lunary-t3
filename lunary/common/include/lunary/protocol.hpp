#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace lunary {

inline constexpr int kProtocolVersion = 7;

struct Vec2 {
    float x{};
    float y{};
};

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

struct EntityTelemetry {
    std::uint64_t address{};
    std::uint16_t entityId{};
    bool isLocal{};
    bool isPlayer{};
    int health{};
    int maxHealth{};
    Vec3 position{};
    float distance{};
    std::string displayName;
    std::string heroName;
    bool hasScreenPosition{};
    bool screenPositionNormalized{};
    bool behindCamera{};
    Vec2 screenPosition{};
    bool hasHeadScreenPosition{};
    Vec2 headScreenPosition{};
    bool teamKnown{};
    bool isTeammate{};
    bool hasHeadHitScreenPosition{};
    Vec2 headHitScreenPosition{};
    bool hasTorsoHitScreenPosition{};
    Vec2 torsoHitScreenPosition{};
    bool isInvisible{};
    bool isVanish{};
    bool isFriend{};
    int teamIndex{};
    std::string boneData;
};

struct Snapshot {
    std::uint64_t sequence{};
    std::uint64_t timestampMs{};
    std::uint64_t localEntity{};
    int matchMode{};
    int matchStatus{};
    std::vector<EntityTelemetry> entities;
};

struct Hello {
    int protocolVersion{};
    int pid{};
    std::uint64_t moduleBase{};
    std::string packageName;
    std::string status;
};

inline std::vector<std::string_view> splitTabs(std::string_view line) {
    std::vector<std::string_view> parts;
    std::size_t begin = 0;
    for (;;) {
        const auto end = line.find('\t', begin);
        parts.emplace_back(line.substr(begin, end == std::string_view::npos ? end : end - begin));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return parts;
}

inline std::string hexEncode(std::string_view text) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size() * 2);
    for (const unsigned char c : text) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 0x0F]);
    }
    return out;
}

inline std::optional<std::string> hexDecode(std::string_view text) {
    if ((text.size() & 1U) != 0) {
        return std::nullopt;
    }
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int high = nibble(text[i]);
        const int low = nibble(text[i + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        out.push_back(static_cast<char>((high << 4) | low));
    }
    return out;
}

template <typename T>
inline bool parseInteger(std::string_view text, T& value, int base = 10) {
    const char* first = text.data();
    const char* last = first + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value, base);
    return ec == std::errc{} && ptr == last;
}

inline bool parseFloat(std::string_view text, float& value) {
    std::string copy(text);
    char* end = nullptr;
    value = std::strtof(copy.c_str(), &end);
    return end == copy.c_str() + copy.size() && std::isfinite(value);
}

inline std::string addressHex(std::uint64_t address) {
    std::ostringstream stream;
    stream << std::hex << std::uppercase << address;
    return stream.str();
}

inline std::string makeHelloLine(const Hello& hello) {
    std::ostringstream out;
    out << "H\t" << hello.protocolVersion << '\t' << hello.pid << '\t'
        << addressHex(hello.moduleBase) << '\t' << hexEncode(hello.packageName) << '\t'
        << hexEncode(hello.status) << '\n';
    return out.str();
}

inline std::string makeFrameHeader(const Snapshot& snapshot) {
    std::ostringstream out;
    out << "F\t" << snapshot.sequence << '\t' << snapshot.timestampMs << '\t'
        << addressHex(snapshot.localEntity) << '\t' << snapshot.entities.size() << '\t'
        << snapshot.matchMode << '\t' << snapshot.matchStatus << '\n';
    return out.str();
}

// =============================================================================
// Feature Config serialization (Cmd "C" line: config sent from UI to DLL)
// =============================================================================
struct FeatureConfigData {
    bool enabled{false};
    // God Mode
    int invincibleCount{1};
    // Damage
    float damageMultiplier{1.0f};
    float damageReduceMultiplier{0.0f};
    // Speed
    float moveSpeedMultiplier{1.0f};
    // Fly
    bool enableFly{false};
    bool enableNoGravity{false};
    // Recoil/Spread
    bool zeroRecoil{false};
    bool zeroSpread{false};
    // Ammo
    int clipSize{0};
    bool noReload{false};
    // Rapid Fire
    float fireRateMultiplier{1.0f};
    // Wallhack
    bool shootThroughWalls{false};
    bool seeThroughWalls{false};
    // ESP flags
    bool espEnabled{false};
    // Skill CD
    bool noSkillCD{false};
    // Teleport
    bool teleportEnabled{false};
    float teleportX{0.0f};
    float teleportY{0.0f};
    float teleportZ{0.0f};
    std::uint64_t teleportRequest{};
    // Shield
    int shieldValue{0};
    // Aimbot
    bool aimbotEnabled{false};
    float aimbotFov{25.0f};
    float aimbotSmoothing{3.0f};
    float aimbotMaxDistance{90.0f};
    // Jump
    int maxJumps{1};
    bool enableMultiJump{false};
    // Auto Shoot
    bool autoShootEnabled{false};
    // Invisibility
    bool invisibilityEnabled{false};
    // Knockback
    bool noKnockback{false};
    // Built-in cheat toggles
    bool builtinFly{false};
    bool builtinGod{false};
    bool builtinOneHit{false};
    bool builtinInfAmmo{false};
    bool builtinNoCD{false};
    float builtinAddDamage{0.0f};
    int builtinAddHealth{0};
    int builtinAddShield{0};
    float builtinSpeedMult{1.0f};
};

inline std::string makeConfigLine(const FeatureConfigData& cfg) {
    std::ostringstream out;
    out << std::setprecision(6)
        << "C"
        << '\t' << (cfg.enabled ? 1 : 0)
        << '\t' << cfg.invincibleCount
        << '\t' << cfg.damageMultiplier
        << '\t' << cfg.damageReduceMultiplier
        << '\t' << cfg.moveSpeedMultiplier
        << '\t' << (cfg.enableFly ? 1 : 0)
        << '\t' << (cfg.enableNoGravity ? 1 : 0)
        << '\t' << (cfg.zeroRecoil ? 1 : 0)
        << '\t' << (cfg.zeroSpread ? 1 : 0)
        << '\t' << cfg.clipSize
        << '\t' << (cfg.noReload ? 1 : 0)
        << '\t' << cfg.fireRateMultiplier
        << '\t' << (cfg.shootThroughWalls ? 1 : 0)
        << '\t' << (cfg.seeThroughWalls ? 1 : 0)
        << '\t' << (cfg.espEnabled ? 1 : 0)
        << '\t' << (cfg.noSkillCD ? 1 : 0)
        << '\t' << (cfg.teleportEnabled ? 1 : 0)
        << '\t' << cfg.teleportX
        << '\t' << cfg.teleportY
        << '\t' << cfg.teleportZ
        << '\t' << cfg.shieldValue
        << '\t' << (cfg.aimbotEnabled ? 1 : 0)
        << '\t' << cfg.aimbotFov
        << '\t' << cfg.aimbotSmoothing
        << '\t' << cfg.aimbotMaxDistance
        << '\t' << cfg.maxJumps
        << '\t' << (cfg.enableMultiJump ? 1 : 0)
        << '\t' << (cfg.autoShootEnabled ? 1 : 0)
        << '\t' << (cfg.invisibilityEnabled ? 1 : 0)
        << '\t' << (cfg.noKnockback ? 1 : 0)
        << '\t' << (cfg.builtinFly ? 1 : 0)
        << '\t' << (cfg.builtinGod ? 1 : 0)
        << '\t' << (cfg.builtinOneHit ? 1 : 0)
        << '\t' << (cfg.builtinInfAmmo ? 1 : 0)
        << '\t' << (cfg.builtinNoCD ? 1 : 0)
        << '\t' << cfg.builtinAddDamage
        << '\t' << cfg.builtinAddHealth
        << '\t' << cfg.builtinAddShield
        << '\t' << cfg.builtinSpeedMult
        << '\t' << cfg.teleportRequest
        << '\n';
    return out.str();
}

inline bool parseConfigLine(std::string_view line, FeatureConfigData& cfg) {
    const auto p = splitTabs(line);
    if (p.size() != 41 || p[0] != "C") return false;
    int v{};
    parseInteger(p[1], v);  cfg.enabled = v != 0;
    parseInteger(p[2], cfg.invincibleCount);
    parseFloat(p[3], cfg.damageMultiplier);
    parseFloat(p[4], cfg.damageReduceMultiplier);
    parseFloat(p[5], cfg.moveSpeedMultiplier);
    parseInteger(p[6], v);  cfg.enableFly = v != 0;
    parseInteger(p[7], v);  cfg.enableNoGravity = v != 0;
    parseInteger(p[8], v);  cfg.zeroRecoil = v != 0;
    parseInteger(p[9], v);  cfg.zeroSpread = v != 0;
    parseInteger(p[10], cfg.clipSize);
    parseInteger(p[11], v); cfg.noReload = v != 0;
    parseFloat(p[12], cfg.fireRateMultiplier);
    parseInteger(p[13], v); cfg.shootThroughWalls = v != 0;
    parseInteger(p[14], v); cfg.seeThroughWalls = v != 0;
    parseInteger(p[15], v); cfg.espEnabled = v != 0;
    parseInteger(p[16], v); cfg.noSkillCD = v != 0;
    parseInteger(p[17], v); cfg.teleportEnabled = v != 0;
    parseFloat(p[18], cfg.teleportX);
    parseFloat(p[19], cfg.teleportY);
    parseFloat(p[20], cfg.teleportZ);
    parseInteger(p[21], cfg.shieldValue);
    parseInteger(p[22], v); cfg.aimbotEnabled = v != 0;
    parseFloat(p[23], cfg.aimbotFov);
    parseFloat(p[24], cfg.aimbotSmoothing);
    parseFloat(p[25], cfg.aimbotMaxDistance);
    parseInteger(p[26], cfg.maxJumps);
    parseInteger(p[27], v); cfg.enableMultiJump = v != 0;
    parseInteger(p[28], v); cfg.autoShootEnabled = v != 0;
    parseInteger(p[29], v); cfg.invisibilityEnabled = v != 0;
    parseInteger(p[30], v); cfg.noKnockback = v != 0;
    parseInteger(p[31], v); cfg.builtinFly = v != 0;
    parseInteger(p[32], v); cfg.builtinGod = v != 0;
    parseInteger(p[33], v); cfg.builtinOneHit = v != 0;
    parseInteger(p[34], v); cfg.builtinInfAmmo = v != 0;
    parseInteger(p[35], v); cfg.builtinNoCD = v != 0;
    parseFloat(p[36], cfg.builtinAddDamage);
    parseInteger(p[37], cfg.builtinAddHealth);
    parseInteger(p[38], cfg.builtinAddShield);
    parseFloat(p[39], cfg.builtinSpeedMult);
    parseInteger(p[40], cfg.teleportRequest);
    return true;
}

inline std::string makeEntityLine(const EntityTelemetry& entity) {
    const unsigned flags = (entity.isLocal ? 1U : 0U) | (entity.isPlayer ? 2U : 0U) |
                           (entity.hasScreenPosition ? 4U : 0U) |
                           (entity.screenPositionNormalized ? 8U : 0U) |
                           (entity.behindCamera ? 16U : 0U) |
                           (entity.hasHeadScreenPosition ? 32U : 0U) |
                           (entity.teamKnown ? 64U : 0U) |
                           (entity.isTeammate ? 128U : 0U) |
                           (entity.hasHeadHitScreenPosition ? 256U : 0U) |
                           (entity.hasTorsoHitScreenPosition ? 512U : 0U) |
                           (entity.isInvisible ? 1024U : 0U) |
                           (entity.isVanish ? 2048U : 0U) |
                           (entity.isFriend ? 4096U : 0U);
    std::ostringstream out;
    out << std::setprecision(7) << "E\t" << addressHex(entity.address) << '\t'
        << entity.entityId << '\t' << flags << '\t' << entity.health << '\t'
        << entity.maxHealth << '\t' << entity.position.x << '\t' << entity.position.y
        << '\t' << entity.position.z << '\t' << entity.distance << '\t'
        << entity.screenPosition.x << '\t' << entity.screenPosition.y << '\t'
        << entity.headScreenPosition.x << '\t' << entity.headScreenPosition.y << '\t'
        << entity.headHitScreenPosition.x << '\t' << entity.headHitScreenPosition.y << '\t'
        << entity.torsoHitScreenPosition.x << '\t' << entity.torsoHitScreenPosition.y << '\t'
        << hexEncode(entity.displayName) << '\t' << hexEncode(entity.heroName) << '\t'
        << entity.teamIndex << '\t' << hexEncode(entity.boneData) << '\n';
    return out.str();
}

inline bool parseHelloLine(std::string_view line, Hello& hello) {
    const auto p = splitTabs(line);
    if (p.size() != 6 || p[0] != "H") return false;
    std::uint64_t base{};
    if (!parseInteger(p[1], hello.protocolVersion) || !parseInteger(p[2], hello.pid) ||
        !parseInteger(p[3], base, 16)) return false;
    const auto packageName = hexDecode(p[4]);
    const auto status = hexDecode(p[5]);
    if (!packageName || !status) return false;
    hello.moduleBase = base;
    hello.packageName = *packageName;
    hello.status = *status;
    return true;
}

inline bool parseFrameHeader(std::string_view line, Snapshot& snapshot, std::size_t& expected) {
    const auto p = splitTabs(line);
    if (p.size() != 7 || p[0] != "F") return false;
    std::uint64_t local{};
    if (!parseInteger(p[1], snapshot.sequence) || !parseInteger(p[2], snapshot.timestampMs) ||
        !parseInteger(p[3], local, 16) || !parseInteger(p[4], expected) ||
        !parseInteger(p[5], snapshot.matchMode) || !parseInteger(p[6], snapshot.matchStatus)) return false;
    snapshot.localEntity = local;
    snapshot.entities.clear();
    snapshot.entities.reserve(expected);
    return true;
}

inline bool parseEntityLine(std::string_view line, EntityTelemetry& entity) {
    const auto p = splitTabs(line);
    if (p.size() != 22 || p[0] != "E") return false;
    unsigned flags{};
    if (!parseInteger(p[1], entity.address, 16) || !parseInteger(p[2], entity.entityId) ||
        !parseInteger(p[3], flags) || !parseInteger(p[4], entity.health) ||
        !parseInteger(p[5], entity.maxHealth) || !parseFloat(p[6], entity.position.x) ||
        !parseFloat(p[7], entity.position.y) || !parseFloat(p[8], entity.position.z) ||
        !parseFloat(p[9], entity.distance) || !parseFloat(p[10], entity.screenPosition.x) ||
        !parseFloat(p[11], entity.screenPosition.y) ||
        !parseFloat(p[12], entity.headScreenPosition.x) ||
        !parseFloat(p[13], entity.headScreenPosition.y) ||
        !parseFloat(p[14], entity.headHitScreenPosition.x) ||
        !parseFloat(p[15], entity.headHitScreenPosition.y) ||
        !parseFloat(p[16], entity.torsoHitScreenPosition.x) ||
        !parseFloat(p[17], entity.torsoHitScreenPosition.y)) return false;
    const auto displayName = hexDecode(p[18]);
    const auto heroName = hexDecode(p[19]);
    int teamIndex{};
    if (!displayName || !heroName || !parseInteger(p[20], teamIndex)) return false;
    const auto boneData = hexDecode(p[21]);
    entity.isLocal = (flags & 1U) != 0;
    entity.isPlayer = (flags & 2U) != 0;
    entity.hasScreenPosition = (flags & 4U) != 0;
    entity.screenPositionNormalized = (flags & 8U) != 0;
    entity.behindCamera = (flags & 16U) != 0;
    entity.hasHeadScreenPosition = (flags & 32U) != 0;
    entity.teamKnown = (flags & 64U) != 0;
    entity.isTeammate = (flags & 128U) != 0;
    entity.hasHeadHitScreenPosition = (flags & 256U) != 0;
    entity.hasTorsoHitScreenPosition = (flags & 512U) != 0;
    entity.isInvisible = (flags & 1024U) != 0;
    entity.isVanish = (flags & 2048U) != 0;
    entity.isFriend = (flags & 4096U) != 0;
    entity.teamIndex = teamIndex;
    entity.displayName = *displayName;
    entity.heroName = *heroName;
    if (boneData) entity.boneData = *boneData;
    return true;
}

} // namespace lunary
