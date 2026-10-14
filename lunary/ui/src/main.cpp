#include "hook_controller.hpp"
#include "overlay.hpp"
#include "snapshot_prediction.hpp"
#include "telemetry_client.hpp"

#include <d3d11.h>
#include <windows.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

ID3D11Device* gDevice{};
ID3D11DeviceContext* gDeviceContext{};
IDXGISwapChain* gSwapChain{};
ID3D11RenderTargetView* gRenderTarget{};
UINT gResizeWidth{};
UINT gResizeHeight{};
HWND gWindow{};
ImFont* gTitleFont{};
bool gPanelVisible{true};

// Labels for tabs (no icons)
#define LBL_PLAY       "Play"
#define LBL_STOP       "Stop"
#define LBL_EYE        "Olho"
#define LBL_CROSSHAIR  "Mira"
#define LBL_BOLT       "Raio"
#define LBL_SHIELD     "Escudo"
#define LBL_COG        "Eng"
#define LBL_SYNC       "Sync"
#define LBL_CUBE       "Cubo"
#define LBL_USER       "User"
#define LBL_BULLSEYE   "Alvo"
#define LBL_SKULL      "Skull"
#define LBL_BONE       "Osso"

constexpr ImVec4 color(std::uint32_t rgba) {
    return ImVec4(((rgba >> 24) & 0xFF) / 255.0F,
                  ((rgba >> 16) & 0xFF) / 255.0F,
                  ((rgba >> 8) & 0xFF) / 255.0F,
                  (rgba & 0xFF) / 255.0F);
}

const ImVec4 kAccent    = color(0xE63946FF);
const ImVec4 kAccentDim = color(0x8B1A25FF);
const ImVec4 kGreen     = color(0x2ECC71FF);
const ImVec4 kAmber     = color(0xF39C12FF);
const ImVec4 kRed       = color(0xE74C3CFF);
const ImVec4 kMuted     = color(0x7F8C8DFF);
const ImVec4 kBgDark    = color(0x0A0A0CFF);
const ImVec4 kBgCard    = color(0x121215FF);
const ImVec4 kBorder    = color(0x1E1E24FF);
const ImVec4 kText      = color(0xEAEAECFF);

void createRenderTarget() {
    ID3D11Texture2D* backBuffer{};
    gSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        gDevice->CreateRenderTargetView(backBuffer, nullptr, &gRenderTarget);
        backBuffer->Release();
    }
}

void cleanupRenderTarget() {
    if (gRenderTarget) {
        gRenderTarget->Release();
        gRenderTarget = nullptr;
    }
}

bool createDevice(HWND window) {
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = window;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    constexpr D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0,
                                         D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL createdLevel{};
    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels,
            static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
            &description, &gSwapChain, &gDevice, &createdLevel,
            &gDeviceContext))) return false;
    createRenderTarget();
    return true;
}

void cleanupDevice() {
    cleanupRenderTarget();
    if (gSwapChain) { gSwapChain->Release(); gSwapChain = nullptr; }
    if (gDeviceContext) { gDeviceContext->Release(); gDeviceContext = nullptr; }
    if (gDevice) { gDevice->Release(); gDevice = nullptr; }
}

LRESULT WINAPI windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) return true;
    switch (message) {
    case WM_NCHITTEST: {
        POINT point{static_cast<short>(LOWORD(lparam)),
                    static_cast<short>(HIWORD(lparam))};
        ScreenToClient(window, &point);
        RECT area{};
        GetClientRect(window, &area);
        // Title bar: top 28px minus right 68px for window buttons
        if (point.y >= 0 && point.y < 28 && point.x < area.right - 68)
            return HTCAPTION;
        return HTCLIENT;
    }
    case WM_SIZE:
        if (wparam == SIZE_MINIMIZED) return 0;
        gResizeWidth = static_cast<UINT>(LOWORD(lparam));
        gResizeHeight = static_cast<UINT>(HIWORD(lparam));
        return 0;
    case WM_SYSCOMMAND:
        if ((wparam & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
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

std::wstring executableDirectory() {
    std::array<wchar_t, 32768> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                            static_cast<DWORD>(path.size()));
    std::wstring result(path.data(), length);
    const auto separator = result.find_last_of(L"\\/");
    return separator == std::wstring::npos ? L"." : result.substr(0, separator);
}

void loadFonts(ImGuiIO& io) {
    // Find project root by walking up from exe directory
    std::wstring dir = executableDirectory();
    std::wstring assetsDir;
    for (int depth = 0; depth < 10; ++depth) {
        assetsDir = dir + L"\\ui\\assets\\";
        if (GetFileAttributesW((assetsDir + L"BebasNeue-Regular.ttf").c_str()) != INVALID_FILE_ATTRIBUTES) {
            break;
        }
        const auto sep = dir.find_last_of(L"\\/");
        if (sep == std::wstring::npos) break;
        dir = dir.substr(0, sep);
    }
    const std::string bebas = wideToUtf8(assetsDir + L"BebasNeue-Regular.ttf");

    ImFont* body = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\segoeui.ttf", 19.0F);
    if (!body) body = io.Fonts->AddFontDefault();
    gTitleFont = io.Fonts->AddFontFromFileTTF(bebas.c_str(), 26.0F);
    if (!gTitleFont) gTitleFont = body;
}

void applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0F;
    style.ChildRounding = 6.0F;
    style.FrameRounding = 4.0F;
    style.PopupRounding = 4.0F;
    style.GrabRounding = 4.0F;
    style.TabRounding = 6.0F;
    style.WindowPadding = {8.0F, 6.0F};
    style.FramePadding = {5.0F, 3.0F};
    style.ItemSpacing = {5.0F, 4.0F};
    style.WindowBorderSize = 0.0F;
    style.ChildBorderSize = 0.0F;
    style.FrameBorderSize = 1.0F;
    style.ScrollbarSize = 5.0F;
    auto& colors = style.Colors;
    colors[ImGuiCol_Text]                = kText;
    colors[ImGuiCol_TextDisabled]        = kMuted;
    colors[ImGuiCol_WindowBg]            = kBgDark;
    colors[ImGuiCol_ChildBg]             = kBgCard;
    colors[ImGuiCol_PopupBg]             = color(0x16161AFF);
    colors[ImGuiCol_Border]              = kBorder;
    colors[ImGuiCol_BorderShadow]        = color(0x00000000);
    colors[ImGuiCol_FrameBg]             = color(0x1A1A20FF);
    colors[ImGuiCol_FrameBgHovered]      = color(0x24242BFF);
    colors[ImGuiCol_FrameBgActive]       = color(0x2E2E37FF);
    colors[ImGuiCol_CheckMark]           = kAccent;
    colors[ImGuiCol_SliderGrab]          = kAccent;
    colors[ImGuiCol_SliderGrabActive]    = color(0xFF4D5EFF);
    colors[ImGuiCol_Button]              = color(0xC0392BFF);
    colors[ImGuiCol_ButtonHovered]       = kAccent;
    colors[ImGuiCol_ButtonActive]        = color(0xA93226FF);
    colors[ImGuiCol_Header]              = color(0x1A1A20FF);
    colors[ImGuiCol_HeaderHovered]       = color(0x2A1520FF);
    colors[ImGuiCol_HeaderActive]        = color(0x3D1A28FF);
    colors[ImGuiCol_Tab]                 = kBgCard;
    colors[ImGuiCol_TabHovered]          = color(0x2A1520FF);
    colors[ImGuiCol_TabSelected]         = kAccent;
    colors[ImGuiCol_TabSelectedOverline] = kAccent;
    colors[ImGuiCol_TabDimmed]           = color(0x0E0E10FF);
    colors[ImGuiCol_TabDimmedSelected]   = color(0x3D1A28FF);
    colors[ImGuiCol_Separator]           = kBorder;
    colors[ImGuiCol_ScrollbarBg]         = color(0x00000000);
    colors[ImGuiCol_ScrollbarGrab]       = color(0x2E2E37FF);
    colors[ImGuiCol_ScrollbarGrabHovered]= color(0x3E3E47FF);
    colors[ImGuiCol_ScrollbarGrabActive] = color(0x4E4E57FF);
}

void statusDot(const ImVec4& value) {
    const auto cursor = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddCircleFilled(
        {cursor.x + 4.0F, cursor.y + 8.0F}, 4.0F,
        ImGui::ColorConvertFloat4ToU32(value));
    ImGui::Dummy({13.0F, 16.0F});
    ImGui::SameLine(0.0F, 0.0F);
}

ImVec4 stateColor(const lunary::ui::HookControllerView& view) {
    using State = lunary::ui::HookControllerState;
    switch (view.state) {
    case State::Active: return kGreen;
    case State::Activating:
    case State::Syncing: return kAmber;
    case State::Error: return kRed;
    case State::Disabled:
    case State::Ready: return kMuted;
    }
    return kMuted;
}

bool hotkeyPressed(int key) {
    static std::array<bool, 256> previous{};
    if (key < 0 || key >= static_cast<int>(previous.size())) return false;
    const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
    const bool pressed = down && !previous[static_cast<std::size_t>(key)];
    previous[static_cast<std::size_t>(key)] = down;
    return pressed;
}

void renderControlTab(lunary::ui::HookController& controller,
                      lunary::ui::HookSettings& settings,
                      lunary::ui::FeatureSettings& featureSettings,
                      char* packageName, std::size_t,
                      const lunary::ui::HookControllerView& view,
                      const lunary::ui::ClientView& telemetryView) {
    const float width = ImGui::GetContentRegionAvail().x;
    const bool connected =
        telemetryView.state == lunary::ui::ConnectionState::Connected;
    const bool hookAvailable = view.injected || connected;

    // Status + inject
    statusDot(connected ? kGreen : stateColor(view));
    ImGui::SameLine();
    if (connected) {
        ImGui::TextUnformatted(settings.enabled
            ? "Hook ativo - aimbot ligado"
            : "Hook ativo - painel sincronizado");
    } else {
        ImGui::TextUnformatted(view.status.c_str());
    }
    ImGui::Spacing();

    ImGui::BeginDisabled(view.busy);
    if (ImGui::Button(hookAvailable ? "APLICAR AGORA" : "ATIVAR",
                      {width, 32.0F})) {
        if (hookAvailable) {
            controller.applyFeatures(featureSettings);
        } else {
            controller.activate(settings, packageName);
        }
    }
    ImGui::EndDisabled();
    ImGui::Separator();

    // Stats
    ImGui::TextColored(kMuted, "Frames: %llu   Entidades: %zu   Modo: %d   Status: %d",
        static_cast<unsigned long long>(telemetryView.snapshot.sequence),
        telemetryView.snapshot.entities.size(),
        telemetryView.snapshot.matchMode,
        telemetryView.snapshot.matchStatus);
    ImGui::SameLine();
    statusDot(connected ? kGreen : kAmber);
    ImGui::SameLine();
    ImGui::TextColored(connected ? kGreen : kAmber, connected ? "ESP conectado" : "ESP offline");
    ImGui::Spacing();
    ImGui::TextColored(kMuted, "Pacote: %s", packageName);
}

bool renderAimTab(lunary::ui::HookSettings& settings) {
    bool changed{};
    const float width = ImGui::GetContentRegionAvail().x;

    changed |= ImGui::Checkbox("Ativar aimbot", &settings.enabled);
    ImGui::Separator();

    const char* points[]{"Cabeca", "Torso"};
    int point = settings.head ? 0 : 1;
    ImGui::TextColored(kMuted, "Ponto de mira");
    ImGui::SetNextItemWidth(width * 0.95F);
    if (ImGui::Combo("##aim_point", &point, points, 2)) {
        settings.head = point == 0;
        changed = true;
    }

    const auto slider = [&](const char* label, const char* id, float& value,
                            float minV, float maxV, const char* fmt) {
        ImGui::TextColored(kMuted, "%s", label);
        ImGui::SetNextItemWidth(width * 0.95F);
        if (ImGui::SliderFloat(id, &value, minV, maxV, fmt)) changed = true;
    };
    slider("FOV", "##fov", settings.fovDegrees, 1.0F, 180.0F, "%.0f");
    slider("Suavizacao", "##smooth", settings.smoothing, 1.0F, 30.0F, "%.1f");
    slider("Distancia max", "##dist", settings.maximumDistance, 0.0F, 200.0F, "%.0fm");
    slider("Alt. camera", "##eye", settings.eyeHeight, -1.0F, 3.0F, "%.2f");
    ImGui::Separator();
    changed |= ImGui::Checkbox("Ignorar aliados", &settings.ignoreTeammates);
    changed |= ImGui::Checkbox("Travar alvo", &settings.lockTarget);
    return changed;
}

void renderVisualTab(lunary::ui::OverlaySettings& settings) {
    const float width = ImGui::GetContentRegionAvail().x;

    ImGui::Checkbox("ESP ativado", &settings.enabled);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Elementos", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Linhas (rainbow)", &settings.lines);
        ImGui::SameLine(width * 0.5F);
        ImGui::Checkbox("Caixas", &settings.boxes);
        ImGui::Checkbox("Esqueleto", &settings.skeleton);
        ImGui::SameLine(width * 0.5F);
        ImGui::Checkbox("Wireframe", &settings.wireframeTargets);
    }
    if (ImGui::CollapsingHeader("Etiquetas", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Nomes", &settings.names);
        ImGui::SameLine(width * 0.5F);
        ImGui::Checkbox("Vida", &settings.health);
        ImGui::Checkbox("Distancia", &settings.distance);
        ImGui::SameLine(width * 0.5F);
        ImGui::Checkbox("Vida local", &settings.localHealth);
        ImGui::Checkbox("Marcador local", &settings.localMarker);
    }
    if (ImGui::CollapsingHeader("Filtros", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Inimigos", &settings.showEnemies);
        ImGui::SameLine(width * 0.5F);
        ImGui::Checkbox("Aliados", &settings.showTeammates);
        ImGui::Checkbox("Fora da tela", &settings.offscreenTargets);
    }
    if (ImGui::CollapsingHeader("Exibicao")) {
        ImGui::TextColored(kMuted, "FPS do overlay");
        ImGui::SetNextItemWidth(width * 0.85F);
        ImGui::SliderFloat("##ov_fps", &settings.overlayFps, 15.0F, 240.0F, "%.0f");
        ImGui::Checkbox("Fundo em etiquetas", &settings.labelBackground);
        ImGui::Checkbox("Sinc. monitor", &settings.syncToMonitor);
    }
}

// =============================================================================
// Reduced feature tabs: only supported, validated controls are exposed.
// =============================================================================

void renderCombatTab(lunary::ui::FeatureSettings& fcfg) {
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(kAmber,
        "Movimento local: o servidor pode corrigir a posicao.");
    bool fly = fcfg.enableFly;
    if (ImGui::Checkbox("Fly (local)", &fly)) {
        fcfg.enableFly = fly;
        fcfg.enableNoGravity = fly;
    }
    ImGui::Separator();

    bool speed = fcfg.moveSpeedMultiplier > 1.001F;
    if (ImGui::Checkbox("Speed (local)", &speed))
        fcfg.moveSpeedMultiplier = speed ? 3.0F : 1.0F;
    if (speed) {
        ImGui::TextColored(kMuted, "Multiplicador de velocidade");
        ImGui::SetNextItemWidth(width * 0.9F);
        ImGui::SliderFloat("##speed", &fcfg.moveSpeedMultiplier, 1.1F, 15.0F, "%.1fx");
    }
}

void renderWeaponsTab(lunary::ui::FeatureSettings& fcfg) {
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::Checkbox("Sem recoil", &fcfg.zeroRecoil);
    ImGui::Checkbox("Sem spread", &fcfg.zeroSpread);

    bool infiniteAmmo = fcfg.noReload || fcfg.clipSize > 0;
    if (ImGui::Checkbox("Municao infinita", &infiniteAmmo)) {
        fcfg.noReload = infiniteAmmo;
        fcfg.clipSize = infiniteAmmo ? 999 : 0;
    }

    bool rapidFire = fcfg.fireRateMultiplier > 1.001F;
    if (ImGui::Checkbox("Fire rate (cadencia local)", &rapidFire))
        fcfg.fireRateMultiplier = rapidFire ? 2.0F : 1.0F;
    if (rapidFire) {
        ImGui::TextColored(kMuted, "Multiplicador de fire rate");
        ImGui::SetNextItemWidth(width * 0.9F);
        ImGui::SliderFloat("##firerate", &fcfg.fireRateMultiplier, 1.1F, 15.0F, "%.1fx");
    }
}

void renderUtilityTab(lunary::ui::FeatureSettings& fcfg,
                      const lunary::ui::ClientView& telemetryView) {
    static std::string teleportFeedback;
    static ULONGLONG teleportFeedbackAt{};

    ImGui::Checkbox("Sem knockback", &fcfg.noKnockback);
    ImGui::Checkbox("Sem cooldown (animacao local)", &fcfg.noSkillCD);
    ImGui::Separator();
    ImGui::Checkbox("Atirar atraves de paredes", &fcfg.shootThroughWalls);
    ImGui::Checkbox("Ver atraves de paredes", &fcfg.seeThroughWalls);
    ImGui::Separator();
    ImGui::TextColored(kAccent, "TELEPORT");
    ImGui::TextColored(kMuted, "Jogadores detectados no mapa");
    if (!teleportFeedback.empty() &&
        GetTickCount64() - teleportFeedbackAt < 5000ULL) {
        ImGui::TextColored(kGreen, "%s", teleportFeedback.c_str());
    }

    int players{};
    ImGui::BeginChild("##teleport_players", ImVec2(0.0F, 0.0F), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (const auto& entity : telemetryView.snapshot.entities) {
        if (!entity.isPlayer || entity.isLocal || entity.health <= 0) continue;
        ++players;
        std::string playerName = entity.displayName;
        if (playerName.empty())
            playerName = "Jogador " + std::to_string(entity.entityId);
        if (playerName.size() > 30) playerName = playerName.substr(0, 27) + "...";

        const std::string id = std::to_string(entity.address);
        ImGui::PushID(id.c_str());
        ImGui::TextUnformatted(playerName.c_str());
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 48.0F);
        if (ImGui::Button("TP", ImVec2(44.0F, 0.0F))) {
            fcfg.teleportEnabled = true;
            fcfg.teleportX = entity.position.x;
            fcfg.teleportY = entity.position.y + 0.35F;
            fcfg.teleportZ = entity.position.z;
            // This token must remain unique after reopening the panel. A
            // process-local ++ counter could repeat a value already consumed
            // by the still-loaded DLL, making a valid click look ignored.
            std::uint64_t request = static_cast<std::uint64_t>(GetTickCount64());
            if (request <= fcfg.teleportRequest)
                request = fcfg.teleportRequest + 1;
            if (request == 0) request = 1;
            fcfg.teleportRequest = request;

            teleportFeedback = "TP enviado para " + playerName;
            if (!entity.heroName.empty())
                teleportFeedback += " (" + entity.heroName + ")";
            teleportFeedbackAt = GetTickCount64();
        }
        ImGui::TextColored(kMuted, "%s%s%s%.0fm  |  %d HP%s",
                           entity.heroName.empty() ? "" : "Hero: ",
                           entity.heroName.empty() ? "" : entity.heroName.c_str(),
                           entity.heroName.empty() ? "" : "  |  ",
                           entity.distance, entity.health,
                           entity.teamKnown && entity.isTeammate ? "  |  aliado" : "");
        ImGui::Separator();
        ImGui::PopID();
    }
    if (players == 0)
        ImGui::TextColored(kMuted, "Nenhum jogador disponivel. Entre em uma partida.");
    ImGui::EndChild();
}

void keepOnlySupportedFeatures(lunary::ui::FeatureSettings& fcfg) {
    fcfg.invincibleCount = 1;
    fcfg.damageMultiplier = 1.0F;
    fcfg.damageReduceMultiplier = 0.0F;
    fcfg.shieldValue = 0;
    fcfg.aimbotEnabled = false;
    fcfg.maxJumps = 1;
    fcfg.enableMultiJump = false;
    fcfg.autoShootEnabled = false;
    fcfg.invisibilityEnabled = false;
    fcfg.espEnabled = false;
    fcfg.builtinFly = false;
    fcfg.builtinGod = false;
    fcfg.builtinOneHit = false;
    fcfg.builtinInfAmmo = false;
    fcfg.builtinNoCD = false;
    fcfg.builtinAddDamage = 0.0F;
    fcfg.builtinAddHealth = 0;
    fcfg.builtinAddShield = 0;
    fcfg.builtinSpeedMult = 1.0F;

    fcfg.enabled = fcfg.enableFly || fcfg.moveSpeedMultiplier > 1.001F ||
        fcfg.noKnockback || fcfg.zeroRecoil || fcfg.zeroSpread ||
        fcfg.noReload || fcfg.clipSize > 0 || fcfg.fireRateMultiplier > 1.001F ||
        fcfg.shootThroughWalls || fcfg.seeThroughWalls || fcfg.noSkillCD ||
        fcfg.teleportRequest != 0;
}

void renderPanel(lunary::ui::HookController& controller,
                 lunary::ui::HookSettings& settings,
                 lunary::ui::OverlaySettings& overlaySettings,
                 lunary::ui::FeatureSettings& featureSettings,
                 char* packageName, std::size_t packageCapacity,
                 const lunary::ui::ClientView& telemetryView) {
    const auto view = controller.view();
    const auto featuresBefore = featureSettings;
    keepOnlySupportedFeatures(featureSettings);

    if (gTitleFont) ImGui::PushFont(gTitleFont);
    ImGui::TextColored(kAccent, "LUNARY");
    if (gTitleFont) ImGui::PopFont();
    // Right-aligned window control buttons
    const float btnW = 26.0F;
    const float btnH = 21.0F;
    ImGui::SameLine(ImGui::GetWindowWidth() - btnW * 2.0F - ImGui::GetStyle().WindowPadding.x - 4.0F);
    if (ImGui::Button("_", {btnW, btnH})) ShowWindow(gWindow, SW_MINIMIZE);
    ImGui::SameLine(0.0F, 1.0F);
    if (ImGui::Button("X", {btnW, btnH})) PostMessageW(gWindow, WM_CLOSE, 0, 0);
    ImGui::Separator();

    if (ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        if (ImGui::BeginTabItem("CTRL")) {
            renderControlTab(controller, settings, featureSettings, packageName, packageCapacity,
                             view, telemetryView);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("COMB")) {
            renderCombatTab(featureSettings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ARMA")) {
            renderWeaponsTab(featureSettings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("UTIL")) {
            renderUtilityTab(featureSettings, telemetryView);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("VISU")) {
            renderVisualTab(overlaySettings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("MIRA")) {
            if (renderAimTab(settings)) controller.apply(settings, packageName);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    keepOnlySupportedFeatures(featureSettings);
    if (std::memcmp(&featuresBefore, &featureSettings,
                    sizeof(featureSettings)) != 0) {
        controller.applyFeatures(featureSettings);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW windowClass{sizeof(windowClass), CS_CLASSDC, windowProc, 0, 0,
                            instance, nullptr, LoadCursor(nullptr, IDC_ARROW),
                            nullptr, nullptr, L"LunaryHookPanel", nullptr};
    RegisterClassExW(&windowClass);
    gWindow = CreateWindowExW(WS_EX_APPWINDOW, windowClass.lpszClassName,
                              L"Lunary", WS_POPUP, 90, 55, 500, 380,
                              nullptr, nullptr, instance, nullptr);
    if (!gWindow || !createDevice(gWindow)) {
        cleanupDevice();
        UnregisterClassW(windowClass.lpszClassName, instance);
        return 1;
    }
    ShowWindow(gWindow, SW_SHOW);
    UpdateWindow(gWindow);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "lunary-imgui.ini";
    loadFonts(io);
    applyTheme();
    ImGui_ImplWin32_Init(gWindow);
    ImGui_ImplDX11_Init(gDevice, gDeviceContext);

    lunary::ui::HookController controller;
    lunary::ui::HookSettings settings;
    lunary::ui::OverlaySettings overlaySettings;
    lunary::ui::FeatureSettings featureSettings;
    lunary::ui::TelemetryClient telemetryClient;
    telemetryClient.connect("127.0.0.1", 27183);
    lunary::ui::MotionPredictor predictor;
    lunary::ui::EmulatorOverlay overlay;
    overlay.initialize(instance);
    std::array<char, 128> packageName{"com.xd.t3.global"};
    std::uint64_t lastTelemetrySequence{};
    bool done{};
    while (!done) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (hotkeyPressed(VK_INSERT)) {
            gPanelVisible = !gPanelVisible;
            ShowWindow(gWindow, gPanelVisible ? SW_SHOW : SW_HIDE);
        }
        if (hotkeyPressed(VK_F1)) overlaySettings.enabled = !overlaySettings.enabled;
        if (hotkeyPressed(VK_F2)) overlaySettings.lines = !overlaySettings.lines;
        if (hotkeyPressed(VK_F3)) overlaySettings.boxes = !overlaySettings.boxes;
        if (hotkeyPressed(VK_F4)) overlaySettings.skeleton = !overlaySettings.skeleton;
        if (hotkeyPressed(VK_F6))
            overlaySettings.wireframeTargets = !overlaySettings.wireframeTargets;
        if (hotkeyPressed(VK_F7)) {
            settings.enabled = !settings.enabled;
            controller.apply(settings, packageName.data());
        }
        if (gResizeWidth && gResizeHeight) {
            cleanupRenderTarget();
            gSwapChain->ResizeBuffers(0, gResizeWidth, gResizeHeight,
                                      DXGI_FORMAT_UNKNOWN, 0);
            gResizeWidth = gResizeHeight = 0;
            createRenderTarget();
        }

        const auto telemetryView = telemetryClient.view();
        if (telemetryView.snapshot.sequence != lastTelemetrySequence) {
            predictor.push(telemetryView.snapshot, GetTickCount64());
            lastTelemetrySequence = telemetryView.snapshot.sequence;
        }
        const bool connected = telemetryView.state ==
                               lunary::ui::ConnectionState::Connected;
        lunary::Snapshot renderedSnapshot = predictor.predict(GetTickCount64());
        lunary::ui::AimSettings overlayAim;
        overlayAim.enabled = settings.enabled;
        overlayAim.ignoreTeammates = settings.ignoreTeammates;
        overlayAim.lockTarget = settings.lockTarget;
        overlayAim.point = settings.head ? lunary::ui::AimPoint::Head
                                         : lunary::ui::AimPoint::Torso;
        overlayAim.fovRadius = std::min(overlaySettings.sourceWidth,
                                        overlaySettings.sourceHeight) *
                               settings.fovDegrees / 180.0F;
        overlayAim.maxDistance = settings.maximumDistance;
        lunary::ui::AimStatus overlayAimStatus;
        overlayAimStatus.message = settings.enabled ? "Hook ativo" : "Desativada";
        overlay.render(overlaySettings, overlayAim, overlayAimStatus,
                       renderedSnapshot, connected);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({500.0F, 380.0F});
        ImGui::Begin("LunaryRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoResize);
        renderPanel(controller, settings, overlaySettings, featureSettings, packageName.data(),
                    packageName.size(), telemetryView);
        ImGui::End();
        ImGui::Render();

        constexpr float clearColor[4]{0.031F, 0.031F, 0.035F, 1.0F};
        gDeviceContext->OMSetRenderTargets(1, &gRenderTarget, nullptr);
        gDeviceContext->ClearRenderTargetView(gRenderTarget, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        gSwapChain->Present(1, 0);
    }

    overlay.shutdown();
    telemetryClient.disconnect();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanupDevice();
    if (gWindow) DestroyWindow(gWindow);
    UnregisterClassW(windowClass.lpszClassName, instance);
    return 0;
}
