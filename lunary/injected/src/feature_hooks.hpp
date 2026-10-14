#pragma once
// =============================================================================
// Direct ARM64 detours for recoil/spread code paths.
// =============================================================================

#include <atomic>
#include <cstdint>
#include <string>

namespace lunary::injected {

// Install all feature hooks after the libil2cpp base is known.
bool installFeatureHooks(std::uintptr_t moduleBase, std::string& error);

// Uninstall all feature hooks and restore their original instructions.
void uninstallFeatureHooks(std::string& error);

} // namespace lunary::injected
