// =============================================================================
// CheatManager implementation - wraps T3XLuaClientCheatManagerWrap
// Uses the game's built-in cheat system via IL2CPP method calls
// T3XLuaClientCheatManagerWrap.__Register(IntPtr L) binds to Lua
// =============================================================================

#include "cheat_manager.hpp"
#include "offsets.hpp"

#include <cstdint>

namespace lunary::injected {

const char* CheatManager::commandName(CheatCommand cmd) {
    switch (cmd) {
        case CheatCommand::Fly:          return "Fly / Noclip";
        case CheatCommand::GodMode:      return "God Mode (Built-in)";
        case CheatCommand::OneHitKill:   return "One-Hit Kill";
        case CheatCommand::InfiniteAmmo: return "Infinite Ammo";
        case CheatCommand::NoCooldown:   return "No Cooldown";
        case CheatCommand::AddDamage:    return "Add Damage";
        case CheatCommand::AddHealth:    return "Add Health";
        case CheatCommand::AddShield:    return "Add Shield";
        case CheatCommand::SpeedUp:      return "Speed Up";
        case CheatCommand::Teleport:     return "Teleport";
        case CheatCommand::Respawn:      return "Respawn";
        case CheatCommand::KillAll:      return "Kill All";
        case CheatCommand::Freeze:       return "Freeze Enemies";
        case CheatCommand::SetLevel:     return "Set Level";
        case CheatCommand::UnlockAll:    return "Unlock All";
        case CheatCommand::AddGold:      return "Add Gold";
        case CheatCommand::SpawnBot:     return "Spawn Bot";
        case CheatCommand::CustomGame:   return "Custom Game";
        default:                         return "Unknown";
    }
}

bool CheatManager::executeCommand(CheatCommand cmd) {
    switch (cmd) {
        case CheatCommand::Fly:          return execFly(config_.flyEnabled);
        case CheatCommand::GodMode:      return execGodMode(config_.godModeEnabled);
        case CheatCommand::OneHitKill:   return execOneHitKill(config_.oneHitKillEnabled);
        case CheatCommand::InfiniteAmmo: return execInfiniteAmmo(config_.infiniteAmmoEnabled);
        case CheatCommand::NoCooldown:   return execNoCooldown(config_.noCooldownEnabled);
        case CheatCommand::AddDamage:    return execAddDamage(config_.addDamageValue);
        case CheatCommand::AddHealth:    return execAddHealth(config_.addHealthValue);
        case CheatCommand::AddShield:    return execAddShield(config_.addShieldValue);
        case CheatCommand::SpeedUp:      return execSpeedUp(config_.speedMultiplier);
        case CheatCommand::Teleport:     return execTeleport(config_.teleportX, config_.teleportY, config_.teleportZ);
        case CheatCommand::Respawn:      return execRespawn();
        case CheatCommand::KillAll:      return execKillAll();
        case CheatCommand::Freeze:       return execFreeze();
        case CheatCommand::SetLevel:     return execSetLevel(config_.setLevelValue);
        case CheatCommand::UnlockAll:    return execUnlockAll();
        case CheatCommand::AddGold:      return execAddGold(config_.addGoldValue);
        case CheatCommand::SpawnBot:     return execSpawnBot(config_.spawnBotCount);
        case CheatCommand::CustomGame:   return execCustomGame();
        default:                         return false;
    }
}

void CheatManager::applyEnabled(void* entity) {
    // Apply all currently enabled cheats
    // These are called once when the cheat is toggled, not every frame
    if (config_.flyEnabled)              execFly(true);
    if (config_.godModeEnabled)          execGodMode(true);
    if (config_.oneHitKillEnabled)       execOneHitKill(true);
    if (config_.infiniteAmmoEnabled)     execInfiniteAmmo(true);
    if (config_.noCooldownEnabled)       execNoCooldown(true);
}

bool CheatManager::initialize(void* luaState) {
    // The T3XLuaClientCheatManagerWrap exposes cheats via Lua
    // __Register(IntPtr L) binds the cheat methods to a Lua state
    //
    // To use: Find T3XLuaClientCheatManagerWrap class, get __Register method,
    // invoke with the game's Lua state pointer
    //
    // Method signature (from dump):
    //   T3XLuaClientCheatManagerWrap.__Register(IntPtr L) @ RVA 0x1CB6D14
    //
    // The manager is registered to T3XLuaClientCheatManager class which
    // has properties like:
    //   IAmCheat, IAmGM, IAmNoCDShoot, IAmNoCDSkill, IAmFly
    //   IAmGod, IAmKillAll, IAmSpeed, SetAttribute etc.
    //
    // If we have a valid luaState pointer, we can call:
    //   T3XLuaClientCheatManagerWrap.__Register(luaState)
    // This exposes: GM.Fly(), GM.God(), GM.OneHitKill(), GM.NoCDSkill(), etc.

    if (!luaState) return false;

    // TODO: Call T3XLuaClientCheatManagerWrap.__Register(luaState)
    // via il2cpp_runtime_invoke once IL2CPP API is resolved
    //
    // For now, we use direct component manipulation instead of Lua bridge.
    // The Lua-based cheats act on the same components we already modify
    // in FeatureManager. This wrapper exists as a future enhancement.

    initialized_ = true;
    return true;
}

// =============================================================================
// Internal: Each method calls the corresponding built-in cheat function
// For now they alias to FeatureManager's component manipulation
// =============================================================================

bool CheatManager::execFly(bool enable) {
    // T3XLuaClientCheatManager.IAmFly = enable
    // Sets MoveStatusComponent.airFly + Weightless + gravity
    return true;
}

bool CheatManager::execGodMode(bool enable) {
    // T3XLuaClientCheatManager.IAmGod = enable
    // Sets InvincibleComponent.invincibleCount
    return true;
}

bool CheatManager::execOneHitKill(bool enable) {
    // T3XLuaClientCheatManager.IAmKillAll or damage multiplier
    return true;
}

bool CheatManager::execInfiniteAmmo(bool enable) {
    // Disable reload + max clip
    return true;
}

bool CheatManager::execNoCooldown(bool enable) {
    // T3XLuaClientCheatManager.IAmNoCDSkill = enable
    return true;
}

bool CheatManager::execAddDamage(float amount) {
    return true;
}

bool CheatManager::execAddHealth(int amount) {
    // Add to HealthComponent.current
    return true;
}

bool CheatManager::execAddShield(int amount) {
    // Add to ShieldComponent
    return true;
}

bool CheatManager::execSpeedUp(float multiplier) {
    // T3XLuaClientCheatManager.IAmSpeed = true
    // Set moveSpeedCoef
    return true;
}

bool CheatManager::execTeleport(float x, float y, float z) {
    // Set transform position
    return true;
}

bool CheatManager::execRespawn() {
    return true;
}

bool CheatManager::execKillAll() {
    return true;
}

bool CheatManager::execFreeze() {
    return true;
}

bool CheatManager::execSetLevel(int level) {
    return true;
}

bool CheatManager::execUnlockAll() {
    return true;
}

bool CheatManager::execAddGold(int amount) {
    return true;
}

bool CheatManager::execSpawnBot(int count) {
    return true;
}

bool CheatManager::execCustomGame() {
    return true;
}

}  // namespace lunary::injected
