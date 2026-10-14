#include "hook_controller.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace lunary::ui {
namespace {

std::filesystem::path executableDirectory() {
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                            static_cast<DWORD>(path.size()));
    return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
}

std::wstring detectAdb() {
    // Tenta via PATH
    std::array<wchar_t, 32768> buf{};
    const DWORD len = SearchPathW(nullptr, L"adb.exe", nullptr,
                                  static_cast<DWORD>(buf.size()), buf.data(), nullptr);
    if (len > 0 && len < static_cast<DWORD>(buf.size())) return std::wstring(buf.data(), len);
    // Locais comuns
    const std::vector<std::wstring> candidates = {
        L"D:\\Android\\platform-tools\\adb.exe",
        std::wstring(_wgetenv(L"LOCALAPPDATA")) + L"\\Android\\Sdk\\platform-tools\\adb.exe",
        std::wstring(_wgetenv(L"ANDROID_HOME") ? _wgetenv(L"ANDROID_HOME") : L"") + L"\\platform-tools\\adb.exe",
        std::wstring(_wgetenv(L"ANDROID_SDK_ROOT") ? _wgetenv(L"ANDROID_SDK_ROOT") : L"") + L"\\platform-tools\\adb.exe",
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    return {};
}

std::filesystem::path locateProjectRoot() {
    // Release packages also contain scripts/config, so that pair alone is not
    // enough to identify the source tree.  Returning build/windows/ui/Release
    // made the panel edit and push a stale copied hook.ini instead of the one
    // shown to the developer in the workspace.
    const auto isSourceRoot = [](const std::filesystem::path& candidate) {
        return std::filesystem::exists(candidate / "CMakeLists.txt") &&
               std::filesystem::exists(candidate / "scripts" / "activate-hook.ps1") &&
               std::filesystem::exists(candidate / "injected" / "src" /
                                       "camera_hook.cpp") &&
               std::filesystem::exists(candidate / "ui" / "src" / "main.cpp");
    };
    const auto searchAncestors = [&](std::filesystem::path candidate) {
        for (int depth = 0; depth < 12; ++depth) {
            if (isSourceRoot(candidate)) return candidate;
            if (!candidate.has_parent_path() || candidate.parent_path() == candidate)
                break;
            candidate = candidate.parent_path();
        }
        return std::filesystem::path{};
    };

    if (auto root = searchAncestors(executableDirectory()); !root.empty()) return root;
    if (auto root = searchAncestors(std::filesystem::current_path()); !root.empty())
        return root;
    return executableDirectory();
}

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length);
    return result;
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()), nullptr, 0,
                                           nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring quote(const std::wstring& value) {
    std::wstring result = L"\"";
    for (const wchar_t character : value) {
        if (character == L'\"') result += L'\\';
        result += character;
    }
    result += L'\"';
    return result;
}

bool validPackageName(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-';
    });
}

} // namespace

HookController::HookController() : root_(locateProjectRoot()) {
    worker_ = std::thread([this] { workerLoop(); });
}

HookController::~HookController() {
    {
        std::scoped_lock lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void HookController::activate(const HookSettings& settings, std::string packageName) {
    {
        std::scoped_lock lock(mutex_);
        desired_ = settings;
        packageName_ = std::move(packageName);
        ++revision_;
        activateRequested_ = true;
        syncRequested_ = false;
        status_ = "Ativacao agendada";
    }
    condition_.notify_all();
}

void HookController::apply(const HookSettings& settings, std::string packageName) {
    {
        std::scoped_lock lock(mutex_);
        desired_ = settings;
        packageName_ = std::move(packageName);
        ++revision_;
        syncRequested_ = true;
    syncDeadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
        status_ = injected_ ? "Aplicando configuracao..." : "Configuracao preparada";
    }
    condition_.notify_all();
}

void HookController::applyFeatures(const FeatureSettings& features) {
    {
        std::scoped_lock lock(mutex_);
        featureSettings_ = features;
    }
    // Trigger config write with updated features
    std::string package;
    HookSettings settings;
    {
        std::scoped_lock lock(mutex_);
        settings = desired_;
        package = packageName_;
    }
    apply(settings, std::move(package));
}

void HookController::disable(const HookSettings& settings, std::string packageName) {
    HookSettings disabled = settings;
    disabled.enabled = false;
    apply(disabled, std::move(packageName));
}

HookControllerView HookController::view() const {
    std::scoped_lock lock(mutex_);
    const bool busy = state_ == HookControllerState::Activating ||
                      state_ == HookControllerState::Syncing;
    return {state_, status_, log_, busy, injected_};
}

std::string HookController::projectRoot() const {
    return wideToUtf8(root_.wstring());
}

void HookController::setState(HookControllerState state, std::string status) {
    std::scoped_lock lock(mutex_);
    state_ = state;
    status_ = std::move(status);
}

void HookController::appendLog(const std::string& text) {
    if (text.empty()) return;
    std::scoped_lock lock(mutex_);
    log_ += text;
    constexpr std::size_t maximum = 64U * 1024U;
    if (log_.size() > maximum) log_.erase(0, log_.size() - maximum);
}

bool HookController::writeConfiguration(const HookSettings& settings,
                                        std::string& error) {
    const auto path = root_ / "config" / "hook.ini";
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = "Nao foi possivel escrever " + wideToUtf8(path.wstring());
        return false;
    }
    output << "# Gerado pelo painel Lunary.\n"
           << "enabled=" << (settings.enabled ? "true" : "false") << '\n'
           << "head=" << (settings.head ? "true" : "false") << '\n'
           << "ignore_teammates=" << (settings.ignoreTeammates ? "true" : "false") << '\n'
           << "lock_target=" << (settings.lockTarget ? "true" : "false") << '\n'
           << std::fixed << std::setprecision(2)
           << "fov_degrees=" << settings.fovDegrees << '\n'
           << "smoothing=" << settings.smoothing << '\n'
           << "max_distance=" << settings.maximumDistance << '\n'
           << "eye_height=" << settings.eyeHeight << "\n\n"
           << "# Feature config (injected side reads these)\n"
           << "feature_enabled=" << (featureSettings_.enabled ? "true" : "false") << '\n'
           << "feature_speed_multiplier=" << featureSettings_.moveSpeedMultiplier << '\n'
           << "feature_enable_fly=" << (featureSettings_.enableFly ? "true" : "false") << '\n'
           << "feature_no_gravity=" << (featureSettings_.enableNoGravity ? "true" : "false") << '\n'
           << "feature_zero_recoil=" << (featureSettings_.zeroRecoil ? "true" : "false") << '\n'
           << "feature_zero_spread=" << (featureSettings_.zeroSpread ? "true" : "false") << '\n'
           << "feature_clip_size=" << featureSettings_.clipSize << '\n'
           << "feature_no_reload=" << (featureSettings_.noReload ? "true" : "false") << '\n'
           << "feature_fire_rate_mult=" << featureSettings_.fireRateMultiplier << '\n'
           << "feature_shoot_through_walls=" << (featureSettings_.shootThroughWalls ? "true" : "false") << '\n'
           << "feature_see_through_walls=" << (featureSettings_.seeThroughWalls ? "true" : "false") << '\n'
           << "feature_no_skill_cd=" << (featureSettings_.noSkillCD ? "true" : "false") << '\n'
           << "feature_teleport_enabled=" << (featureSettings_.teleportEnabled ? "true" : "false") << '\n'
           << "feature_teleport_x=" << featureSettings_.teleportX << '\n'
           << "feature_teleport_y=" << featureSettings_.teleportY << '\n'
           << "feature_teleport_z=" << featureSettings_.teleportZ << '\n'
           << "feature_teleport_request=" << featureSettings_.teleportRequest << '\n'
           << "feature_no_knockback=" << (featureSettings_.noKnockback ? "true" : "false") << '\n'
           << "\n# RVAs de funcoes no libil2cpp.so\n"
           << "rva_match_start_typeinfo=0x673EF80\n"
           << "rva_camera_controller_typeinfo=0x66D55D0\n"
           << "rva_camera_update=0x23F5D4C\n"
           << "rva_match_start_get_entity=0x2407ED0\n"
           << "rva_entity_get_health=0x1800A98\n"
           << "rva_entity_get_transform=0x17D8394\n"
           << "rva_entity_get_player=0x17D59C0\n"
           << "rva_entity_has_hitpoint_container=0x1801114\n"
           << "rva_entity_get_hitpoint_container=0x180118C\n\n"
           << "# Offsets de campos IL2CPP (atualize estes valores quando o jogo atualizar)\n"
           << "field_match_start_ui_health_list=0xC8\n"
           << "field_camera_view_angle=0x30\n"
           << "field_ui_health_entity=0x1B0\n"
           << "field_ui_health_entity_id=0x1B8\n"
           << "field_ui_health_is_same_team=0x1ED\n"
           << "field_ui_health_is_local=0x1EE\n"
           << "field_ui_health_blood_position=0x194\n"
           << "field_ui_health_camera=0x228\n"
           << "field_player_display_name=0x20\n"
           << "field_player_hero_name=0x38\n"
           << "field_health_current=0x18\n"
           << "field_health_maximum=0x1C\n"
           << "field_transform_position=0x18\n"
           << "field_hitpoint0=0x1C\n"
           << "field_hitpoint1=0x3C\n"
           << "field_unity_object_cached_ptr=0x10\n"
           << "field_camera_view_matrix=0x5C\n"
           << "field_camera_projection_matrix=0x9C\n"
           << "field_camera_viewport=0x404\n";
    if (!output) {
        error = "Falha ao finalizar hook.ini";
        return false;
    }
    return true;
}

bool HookController::runScript(const std::filesystem::path& script,
                               const std::wstring& arguments, std::string& error) {
    if (!std::filesystem::exists(script)) {
        error = "Script nao encontrado: " + wideToUtf8(script.wstring());
        return false;
    }

    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE readPipe{};
    HANDLE writePipe{};
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        error = "CreatePipe falhou";
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::wstring command = L"powershell.exe -NoLogo -NoProfile -NonInteractive "
                           L"-ExecutionPolicy Bypass -File " +
                           quote(script.wstring()) + arguments;

    const BOOL created = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, root_.c_str(),
                                        &startup, &process);
    CloseHandle(writePipe);
    if (!created) {
        CloseHandle(readPipe);
        error = "Nao foi possivel iniciar PowerShell (erro " +
                std::to_string(GetLastError()) + ")";
        return false;
    }

    std::array<char, 4096> buffer{};
    DWORD bytesRead{};
    while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()),
                    &bytesRead, nullptr) && bytesRead > 0) {
        appendLog(std::string(buffer.data(), bytesRead));
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode{};
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(readPipe);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (exitCode != 0) {
        error = "PowerShell terminou com codigo " + std::to_string(exitCode);
        return false;
    }
    return true;
}

void HookController::workerLoop() {
    for (;;) {
        HookSettings settings;
        std::string packageName;
        std::uint64_t revision{};
        bool activate{};
        {
            std::unique_lock lock(mutex_);
            condition_.wait(lock, [this] {
                return stopping_ || activateRequested_ || syncRequested_;
            });
            if (stopping_) return;

            if (!activateRequested_ && syncRequested_) {
                const auto deadline = syncDeadline_;
                condition_.wait_until(lock, deadline, [this, deadline] {
                    return stopping_ || activateRequested_ || syncDeadline_ != deadline;
                });
                if (stopping_) return;
                if (activateRequested_ || syncDeadline_ != deadline) continue;
            }

            activate = activateRequested_;
            if (activate) {
                activateRequested_ = false;
                syncRequested_ = false;
                state_ = HookControllerState::Activating;
                status_ = "Compilando, reiniciando o jogo e injetando...";
            } else {
                syncRequested_ = false;
                state_ = injected_ ? HookControllerState::Syncing
                                   : HookControllerState::Ready;
                status_ = injected_ ? "Sincronizando configuracao..."
                                    : "Salvando configuracao...";
            }
            settings = desired_;
            packageName = packageName_;
            revision = revision_;
        }

        std::string error;
        if (!validPackageName(packageName)) {
            setState(HookControllerState::Error, "Package ID invalido");
            continue;
        }
        if (!writeConfiguration(settings, error)) {
            setState(HookControllerState::Error, error);
            continue;
        }

        // Pre-flight: verifica ADB, dispositivo e root antes de tentar injetar
        bool success = true;
        if (activate) {
            std::ostringstream preflightLog;
            const std::wstring adbPath = detectAdb();
            if (adbPath.empty()) {
                setState(HookControllerState::Error,
                         "ADB nao encontrado. Instale Android platform-tools.");
                continue;
            }
            // Checa se ha dispositivo conectado
            {
                SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
                HANDLE outRead{}, outWrite{};
                CreatePipe(&outRead, &outWrite, &sa, 0);
                STARTUPINFOW si{sizeof(si)};
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdOutput = outWrite;
                si.hStdError = outWrite;
                PROCESS_INFORMATION pi{};
                const std::wstring cmd = L"\"" + adbPath + L"\" devices";
                std::wstring cmdline(cmd);
                if (CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                    CloseHandle(outWrite);
                    WaitForSingleObject(pi.hProcess, 5000);
                    std::array<char, 4096> buf{};
                    DWORD read{};
                    ReadFile(outRead, buf.data(), static_cast<DWORD>(buf.size()) - 1, &read, nullptr);
                    const std::string output(buf.data(), read);
                    CloseHandle(outRead);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    // Conta dispositivos (pula cabecalho "List of devices attached")
                    int deviceCount = 0;
                    std::istringstream iss(output);
                    std::string line;
                    while (std::getline(iss, line)) {
                        if (line.find("List of devices") != std::string::npos) continue;
                        if (line.empty()) continue;
                        if (line.find('\t') != std::string::npos) ++deviceCount;
                    }
                    if (deviceCount == 0) {
                        setState(HookControllerState::Error,
                                 "Nenhum dispositivo Android conectado. Conecte via USB ou inicie o emulador.");
                        continue;
                    }
                }
            }
            // Checa root
            {
                SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
                HANDLE outRead{}, outWrite{};
                CreatePipe(&outRead, &outWrite, &sa, 0);
                STARTUPINFOW si{sizeof(si)};
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdOutput = outWrite;
                si.hStdError = outWrite;
                PROCESS_INFORMATION pi{};
                const std::wstring cmd = L"\"" + adbPath + L"\" shell \"su -c 'echo ok'\"";
                std::wstring cmdline(cmd);
                if (CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                    CloseHandle(outWrite);
                    WaitForSingleObject(pi.hProcess, 5000);
                    std::array<char, 256> buf{};
                    DWORD read{};
                    ReadFile(outRead, buf.data(), static_cast<DWORD>(buf.size()) - 1, &read, nullptr);
                    const std::string output(buf.data(), read);
                    CloseHandle(outRead);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    if (output.find("ok") == std::string::npos) {
                        setState(HookControllerState::Error,
                                 "Root negado. Abra o Magisk no dispositivo e conceda acesso root ao Shell/ADB.");
                        continue;
                    }
                }
            }
            appendLog("\r\n=== ATIVAR LUNARY ===\r\n");
            const std::wstring arguments = L" -Package " + quote(utf8ToWide(packageName));
            success = runScript(root_ / "scripts" / "activate-hook.ps1",
                                arguments, error);
            {
                std::scoped_lock lock(mutex_);
                injected_ = success;
            }
        } else {
            // A DLL may already be loaded even when this panel process has
            // just been reopened.  Gating the push on the in-memory injected_
            // flag made every toggle (including TP) silently write only the
            // Windows copy of hook.ini.  Always publish the configuration;
            // the device-side watcher will consume it when the hook is live.
            appendLog("\r\n=== APLICAR CONFIGURACAO ===\r\n");
            const std::wstring arguments =
                L" -Configuration " + quote((root_ / "config" / "hook.ini").wstring());
            success = runScript(root_ / "scripts" / "sync-hook-config.ps1",
                                arguments, error);
        }

        if (!success) {
            appendLog("\r\nERRO: " + error + "\r\n");
            setState(HookControllerState::Error, error);
            continue;
        }

        {
            std::scoped_lock lock(mutex_);
            if (revision_ != revision) {
                syncRequested_ = true;
                syncDeadline_ = std::chrono::steady_clock::now();
                condition_.notify_all();
            }
            if (injected_) {
                state_ = HookControllerState::Active;
                status_ = settings.enabled ? "Hook ativo - aimbot ligado"
                                           : "Hook ativo - painel sincronizado";
            } else {
                state_ = HookControllerState::Ready;
                status_ = "Configuracao enviada. Clique ATIVAR se o hook nao estiver carregado.";
            }
        }
    }
}

} // namespace lunary::ui
