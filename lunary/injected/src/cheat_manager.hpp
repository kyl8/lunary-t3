// =============================================================================
// CheatManager - Wrapper for T3XLuaClientCheatManagerWrap built-in system
// Exposes the game's native cheat methods (admin cheats, debug features)
// T3XLuaClientCheatManagerWrap is a Lua-accessible built-in cheat class
// =============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <atomic>

namespace lunary::injected {

// Available cheat methods from T3XLuaClientCheatManager
enum class CheatCommand : std::uint8_t {
    Fly = 0,            // Toggle fly/noclip
    GodMode,            // Toggle invincibility
    OneHitKill,         // Toggle one-hit-kill
    InfiniteAmmo,       // Toggle infinite ammo
    NoCooldown,         // Toggle no skill cooldown
    AddDamage,          // Add flat damage
    AddHealth,          // Add flat health
    AddShield,          // Add flat shield
    SpeedUp,            // Multiply move speed
    Teleport,           // Teleport to coordinates
    Respawn,            // Force respawn
    KillAll,            // Kill all enemies
    Freeze,             // Freeze all enemies
    SetLevel,           // Set player level
    UnlockAll,          // Unlock all heroes/skins
    AddGold,            // Add in-game currency
    SpawnBot,           // Spawn a bot
    CustomGame,         // Enter custom game mode
    Count               // Must be last
};

inline constexpr std::uint8_t kCheatCommandCount = static_cast<std::uint8_t>(CheatCommand::Count);

struct CheatConfig {
    // Generic cheat toggle
    bool flyEnabled{false};
    bool godModeEnabled{false};
    bool oneHitKillEnabled{false};
    bool infiniteAmmoEnabled{false};
    bool noCooldownEnabled{false};

    // Value-based cheats
    float addDamageValue{9999.0f};
    int   addHealthValue{99999};
    int   addShieldValue{99999};
    float speedMultiplier{3.0f};

    // Teleport destination
    float teleportX{0.0f};
    float teleportY{0.0f};
    float teleportZ{0.0f};

    // Misc
    int setLevelValue{99};
    int addGoldValue{999999};
    int spawnBotCount{1};
};

class CheatManager {
public:
    static CheatManager& instance() {
        static CheatManager mgr;
        return mgr;
    }

    void setConfig(const CheatConfig& cfg) { config_ = cfg; }
    CheatConfig& config() { return config_; }
    const CheatConfig& config() const { return config_; }

    // Execute a cheat command via the built-in T3XLuaClientCheatManager
    // Returns true if the command was recognized and executed
    bool executeCommand(CheatCommand cmd);

    // Apply all enabled cheats at once (for frame-based application)
    void applyEnabled(void* entity);

    // Register cheat manager with the Lua runtime
    // Must be called once after game initialization
    bool initialize(void* luaState);

    // Get cheat command name for UI
    static const char* commandName(CheatCommand cmd);

private:
    CheatManager() = default;

    CheatConfig config_;
    bool initialized_{false};

    // Internal command implementations
    bool execFly(bool enable);
    bool execGodMode(bool enable);
    bool execOneHitKill(bool enable);
    bool execInfiniteAmmo(bool enable);
    bool execNoCooldown(bool enable);
    bool execAddDamage(float amount);
    bool execAddHealth(int amount);
    bool execAddShield(int amount);
    bool execSpeedUp(float multiplier);
    bool execTeleport(float x, float y, float z);
    bool execRespawn();
    bool execKillAll();
    bool execFreeze();
    bool execSetLevel(int level);
    bool execUnlockAll();
    bool execAddGold(int amount);
    bool execSpawnBot(int count);
    bool execCustomGame();
};

}  // namespace lunary::injected
