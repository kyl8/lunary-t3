#include "overlay.hpp"

#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>

namespace lunary::ui {

namespace {

constexpr wchar_t kOverlayClass[] = L"LunaryOverlayWindow";

Gdiplus::Color toColor(const std::array<float, 4>& value) {
    const auto channel = [](float component) {
        return static_cast<BYTE>(std::clamp(component, 0.0F, 1.0F) * 255.0F + 0.5F);
    };
    return {channel(value[3]), channel(value[0]), channel(value[1]), channel(value[2])};
}

std::wstring utf8ToWide(std::string_view text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                          static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), result.data(), count);
    return result;
}

void drawText(Gdiplus::Graphics& graphics, const std::wstring& text,
              const Gdiplus::Font& font, float x, float y,
              const Gdiplus::Color& foreground, bool background) {
    if (text.empty()) return;
    Gdiplus::RectF bounds;
    graphics.MeasureString(text.c_str(), static_cast<INT>(text.size()), &font,
                           Gdiplus::PointF{x, y}, &bounds);
    if (background) {
        Gdiplus::SolidBrush panel(Gdiplus::Color(150, 8, 10, 16));
        graphics.FillRectangle(&panel, bounds.X - 4.0F, bounds.Y - 2.0F,
                               bounds.Width + 8.0F, bounds.Height + 4.0F);
    }
    Gdiplus::SolidBrush shadow(Gdiplus::Color(220, 0, 0, 0));
    graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &font,
                        Gdiplus::PointF{x + 1.0F, y + 1.0F}, &shadow);
    Gdiplus::SolidBrush brush(foreground);
    graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &font,
                        Gdiplus::PointF{x, y}, &brush);
}

} // namespace

EmulatorOverlay::~EmulatorOverlay() {
    shutdown();
}

bool EmulatorOverlay::initialize(HINSTANCE instance) {
    if (window_) return true;
    instance_ = instance;
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&gdiplusToken_, &input, nullptr) != Gdiplus::Ok) return false;

    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kOverlayClass;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&windowClass);
    window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW |
                                  WS_EX_NOACTIVATE,
                              kOverlayClass, L"Lunary Overlay", WS_POPUP,
                              0, 0, 1, 1, nullptr, nullptr, instance_, nullptr);
    if (!window_) {
        shutdown();
        return false;
    }
    refreshTarget();
    return true;
}

void EmulatorOverlay::shutdown() {
    hide();
    destroySurface();
    if (window_) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
    if (instance_) {
        UnregisterClassW(kOverlayClass, instance_);
        instance_ = nullptr;
    }
    if (gdiplusToken_) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
    }
    mainWindow_ = nullptr;
    targetWindow_ = nullptr;
}

LRESULT CALLBACK EmulatorOverlay::windowProc(HWND window, UINT message, WPARAM wparam,
                                             LPARAM lparam) {
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return DefWindowProcW(window, message, wparam, lparam);
}

void EmulatorOverlay::refreshTarget() {
    lastTargetSearch_ = GetTickCount64();
    if (targetWindow_ && IsWindow(targetWindow_) && mainWindow_ && IsWindow(mainWindow_)) return;

    // Try multiple known emulator window class/child combinations.
    struct EmulatorPattern {
        const wchar_t* mainClass;
        const wchar_t* childClasses[3];
    };
    constexpr EmulatorPattern patterns[] = {
        {L"LDPlayerMainFrame", {L"RenderWindow", L"subWin", nullptr}},
        {L"BlueStacksApp", {L"BlueStacksApp", L"_ctl.Window", nullptr}},
        {L"Qt5QWindowIcon", {L"ScreenBoardClassWindow", L"Qt5QWindowIcon", nullptr}}, // Nox/Memu
        {L"TXGuiFoundation", {L"AEngineRenderWindowClass", L"TXGuiFoundation", nullptr}}, // Gameloop/Tencent
        {L"BS2CHINAWIN32", {L"BS2CHINAWIN32", L"_ctl.Window", nullptr}}, // BlueStacks CN
    };

    for (const auto& pattern : patterns) {
        HWND main = FindWindowW(pattern.mainClass, nullptr);
        if (!main) continue;
        HWND child = nullptr;
        for (int i = 0; pattern.childClasses[i]; ++i) {
            child = FindWindowExW(main, nullptr, pattern.childClasses[i], nullptr);
            if (child) break;
        }
        if (!child) {
            // Fallback: try to find any direct child window
            child = FindWindowExW(main, nullptr, nullptr, nullptr);
        }
        if (child) {
            mainWindow_ = main;
            targetWindow_ = child;
            return;
        }
    }

    // Keep the original LDPlayer-specific logic as final fallback.
    mainWindow_ = FindWindowW(L"LDPlayerMainFrame", nullptr);
    targetWindow_ = mainWindow_
        ? FindWindowExW(mainWindow_, nullptr, L"RenderWindow", nullptr)
        : nullptr;
    if (!targetWindow_ && mainWindow_) {
        targetWindow_ = FindWindowExW(mainWindow_, nullptr, L"subWin", nullptr);
    }
    if (!targetWindow_ && mainWindow_) {
        targetWindow_ = FindWindowExW(mainWindow_, nullptr, nullptr, nullptr);
    }
}

bool EmulatorOverlay::targetFound() const {
    return mainWindow_ && targetWindow_ && IsWindow(mainWindow_) && IsWindow(targetWindow_);
}

std::wstring EmulatorOverlay::targetLabel() const {
    if (!targetFound()) return L"LDPlayer nao encontrado";
    wchar_t title[256]{};
    GetWindowTextW(mainWindow_, title, static_cast<int>(std::size(title)));
    return title[0] ? std::wstring(title) : std::wstring(L"LDPlayer");
}

int EmulatorOverlay::targetRefreshRate() const {
    if (!targetFound()) return 60;
    const HMONITOR monitor = MonitorFromWindow(mainWindow_, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return 60;
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettingsExW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode, 0)) return 60;
    const int frequency = static_cast<int>(mode.dmDisplayFrequency);
    return frequency >= 24 && frequency <= 500 ? frequency : 60;
}

HWND EmulatorOverlay::targetWindow() const {
    return targetFound() ? targetWindow_ : nullptr;
}

void EmulatorOverlay::hide() {
    if (window_ && IsWindowVisible(window_)) ShowWindow(window_, SW_HIDE);
}

void EmulatorOverlay::destroySurface() {
    if (memoryDc_) {
        if (previousBitmap_) SelectObject(memoryDc_, previousBitmap_);
        if (bitmap_) DeleteObject(bitmap_);
        DeleteDC(memoryDc_);
    }
    memoryDc_ = nullptr;
    bitmap_ = nullptr;
    previousBitmap_ = nullptr;
    pixels_ = nullptr;
    surfaceWidth_ = surfaceHeight_ = 0;
}

bool EmulatorOverlay::ensureSurface(int width, int height) {
    if (memoryDc_ && surfaceWidth_ == width && surfaceHeight_ == height) return true;
    destroySurface();
    HDC screen = GetDC(nullptr);
    memoryDc_ = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap_ = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels_, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!memoryDc_ || !bitmap_ || !pixels_) {
        destroySurface();
        return false;
    }
    previousBitmap_ = SelectObject(memoryDc_, bitmap_);
    surfaceWidth_ = width;
    surfaceHeight_ = height;
    return true;
}

void EmulatorOverlay::render(const OverlaySettings& settings, const AimSettings& aimSettings,
                             const AimStatus& aimStatus, const Snapshot& snapshot,
                             bool connected) {
    if (!window_) return;
    if (!targetFound() || GetTickCount64() - lastTargetSearch_ > 1000) refreshTarget();
    if (!settings.enabled || !connected || !targetFound() || IsIconic(mainWindow_)) {
        hide();
        return;
    }
    if (settings.onlyWhenFocused &&
        GetAncestor(GetForegroundWindow(), GA_ROOT) != GetAncestor(mainWindow_, GA_ROOT)) {
        hide();
        return;
    }

    RECT targetRect{};
    if (!GetWindowRect(targetWindow_, &targetRect)) {
        hide();
        return;
    }
    const int width = targetRect.right - targetRect.left;
    const int height = targetRect.bottom - targetRect.top;
    if (width <= 0 || height <= 0 || !ensureSurface(width, height)) {
        hide();
        return;
    }
    std::memset(pixels_, 0, static_cast<std::size_t>(width) *
                              static_cast<std::size_t>(height) * 4U);

    const auto alive = [](const EntityTelemetry& entity) {
        return entity.health > 0 && entity.maxHealth > 0;
    };
    auto localIt = std::find_if(snapshot.entities.begin(), snapshot.entities.end(),
        [&](const EntityTelemetry& entity) {
            return alive(entity) && snapshot.localEntity != 0 &&
                   entity.address == snapshot.localEntity;
        });
    if (localIt == snapshot.entities.end()) {
        localIt = std::find_if(snapshot.entities.begin(), snapshot.entities.end(),
            [&](const EntityTelemetry& entity) { return alive(entity) && entity.isLocal; });
    }
    const EntityTelemetry* local = localIt == snapshot.entities.end() ? nullptr : &*localIt;
    const Gdiplus::PointF fallbackLocal{width * 0.5F, height * 0.78F};
    struct ProjectedPoint {
        Gdiplus::PointF point{};
        bool valid{};
        bool edge{};
    };
    const auto clampToEdge = [&](float x, float y, bool forceEdge) {
        constexpr float margin = 12.0F;
        const float centerX = static_cast<float>(width) * 0.5F;
        const float centerY = static_cast<float>(height) * 0.5F;
        float dx = x - centerX;
        float dy = y - centerY;
        if (forceEdge && std::abs(dx) < 0.01F && std::abs(dy) < 0.01F) {
            dy = static_cast<float>(height);
        }
        const float limitX = std::max(centerX - margin, 1.0F);
        const float limitY = std::max(centerY - margin, 1.0F);
        float scale = 1.0F;
        if (forceEdge || std::abs(dx) > limitX || std::abs(dy) > limitY) {
            const float xScale = std::abs(dx) > 0.001F ? limitX / std::abs(dx)
                                                        : std::numeric_limits<float>::max();
            const float yScale = std::abs(dy) > 0.001F ? limitY / std::abs(dy)
                                                        : std::numeric_limits<float>::max();
            scale = std::min(xScale, yScale);
        }
        return ProjectedPoint{{centerX + dx * scale, centerY + dy * scale}, true,
                              forceEdge || scale < 1.0F};
    };
    const auto mapScreen = [&](const Vec2& source, bool normalized, bool behind,
                               bool isLocal, bool clampEdge) {
        if (normalized || (settings.sourceWidth > 1.0F && settings.sourceHeight > 1.0F)) {
            float x = normalized
                ? source.x * static_cast<float>(width)
                : source.x * static_cast<float>(width) / settings.sourceWidth;
            float y = normalized
                ? source.y * static_cast<float>(height)
                : source.y * static_cast<float>(height) / settings.sourceHeight;
            y = static_cast<float>(height) - y;
            if (isLocal && behind) return ProjectedPoint{fallbackLocal, false, false};
            if (clampEdge) return clampToEdge(x, y, behind);
            const bool visible = !behind && x >= 0.0F && y >= 0.0F &&
                                 x <= static_cast<float>(width) &&
                                 y <= static_cast<float>(height);
            return ProjectedPoint{{x, y}, visible, false};
        }
        return ProjectedPoint{};
    };
    const auto screenPoint = [&](const EntityTelemetry& entity, bool isLocal) {
        if (entity.hasScreenPosition &&
            (entity.screenPositionNormalized ||
             (settings.sourceWidth > 1.0F && settings.sourceHeight > 1.0F))) {
            return mapScreen(entity.screenPosition, entity.screenPositionNormalized,
                             entity.behindCamera, isLocal, true);
        }
        return ProjectedPoint{fallbackLocal, isLocal, false};
    };
    const auto headPoint = [&](const EntityTelemetry& entity, bool isLocal) {
        if (!entity.hasHeadScreenPosition) {
            return ProjectedPoint{};
        }
        return mapScreen(entity.headScreenPosition, entity.screenPositionNormalized,
                         entity.behindCamera, isLocal, false);
    };
    const auto projectedLocal = local ? screenPoint(*local, true)
                                      : ProjectedPoint{fallbackLocal, false, false};
    const auto localPoint = projectedLocal.valid ? projectedLocal.point : fallbackLocal;

    Gdiplus::Graphics graphics(memoryDc_);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighSpeed);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighSpeed);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    const Gdiplus::Color teammateColor = toColor(settings.teammateColor);
    const Gdiplus::Color localColor = toColor(settings.localColor);
    const Gdiplus::Color textColor = toColor(settings.textColor);
    const Gdiplus::Color skeletonColor = toColor(settings.skeletonColor);
    const Gdiplus::Color wireframeColor = toColor(settings.wireframeColor);

    const auto rainbowColor = [&](float offset) {
        const float hue = std::fmod(static_cast<float>(GetTickCount64()) * 0.0005F *
                                    settings.rainbowSpeed + offset, 1.0F);
        const float r = std::abs(hue * 6.0F - 3.0F) - 1.0F;
        const float g = 2.0F - std::abs(hue * 6.0F - 2.0F);
        const float b = 2.0F - std::abs(hue * 6.0F - 4.0F);
        return Gdiplus::Color(210,
            static_cast<BYTE>(std::clamp(r, 0.0F, 1.0F) * 255.0F),
            static_cast<BYTE>(std::clamp(g, 0.0F, 1.0F) * 255.0F),
            static_cast<BYTE>(std::clamp(b, 0.0F, 1.0F) * 255.0F));
    };
    Gdiplus::FontFamily family(L"Segoe UI");
    Gdiplus::Font font(&family, std::clamp(settings.fontSize, 9.0F, 32.0F),
                       Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    if (aimSettings.enabled && aimSettings.showFov) {
        const float radius = std::clamp(aimSettings.fovRadius, 10.0F,
                                        static_cast<float>(std::max(width, height)));
        const float centerX = static_cast<float>(width) * 0.5F;
        const float centerY = static_cast<float>(height) * 0.5F;
        Gdiplus::Pen fovPen(aimStatus.activationHeld
            ? Gdiplus::Color(205, 226, 27, 45)
            : Gdiplus::Color(125, 180, 180, 188), 1.25F);
        graphics.DrawEllipse(&fovPen, centerX - radius, centerY - radius,
                             radius * 2.0F, radius * 2.0F);
    }

    struct BodyRect {
        float left{};
        float top{};
        float width{};
        float height{};
        bool valid{};
    };
    const auto bodyRect = [&](const EntityTelemetry& entity, const ProjectedPoint& anchor,
                              bool isLocal) {
        const auto head = headPoint(entity, isLocal);
        if (!anchor.valid || anchor.edge || !head.valid) return BodyRect{};
        const float projectedUnit = std::abs(anchor.point.Y - head.point.Y);
        if (projectedUnit < 4.0F || projectedUnit > static_cast<float>(height)) {
            return BodyRect{};
        }
        const float top = anchor.point.Y - projectedUnit *
            std::clamp(settings.boxTopScale, 0.2F, 1.4F);
        const float bottom = anchor.point.Y + projectedUnit *
            std::clamp(settings.boxBottomScale, 0.2F, 2.0F);
        const float bodyHeight = bottom - top;
        if (bodyHeight < 8.0F || bodyHeight > static_cast<float>(height) * 1.5F) {
            return BodyRect{};
        }
        const float bodyWidth = bodyHeight *
            std::clamp(settings.boxWidthRatio, 0.2F, 1.0F);
        return BodyRect{anchor.point.X - bodyWidth * 0.5F, top,
                        bodyWidth, bodyHeight, true};
    };
    const auto drawWireframe = [&](const BodyRect& body, const Gdiplus::Color& value) {
        if (!body.valid) return;
        Gdiplus::Pen pen(value, std::clamp(settings.lineThickness, 0.5F, 4.0F));
        const float shiftX = body.width * 0.18F;
        const float shiftY = -body.height * 0.08F;
        graphics.DrawRectangle(&pen, body.left, body.top, body.width, body.height);
        graphics.DrawRectangle(&pen, body.left + shiftX, body.top + shiftY,
                               body.width, body.height);
        graphics.DrawLine(&pen, body.left, body.top, body.left + shiftX, body.top + shiftY);
        graphics.DrawLine(&pen, body.left + body.width, body.top,
                          body.left + body.width + shiftX, body.top + shiftY);
        graphics.DrawLine(&pen, body.left, body.top + body.height,
                          body.left + shiftX, body.top + body.height + shiftY);
        graphics.DrawLine(&pen, body.left + body.width, body.top + body.height,
                          body.left + body.width + shiftX, body.top + body.height + shiftY);
        graphics.DrawLine(&pen, body.left, body.top, body.left + body.width,
                          body.top + body.height);
        graphics.DrawLine(&pen, body.left + body.width, body.top, body.left,
                          body.top + body.height);
    };
    const auto drawSkeleton = [&](const EntityTelemetry& entity, const Gdiplus::Color& value) {
        if (entity.boneData.size() < 16) return; // need at least 2 bones (8 hex chars each)
        struct BonePoint { float x; float y; };
        std::vector<BonePoint> bones;
        const auto& hex = entity.boneData;
        // Each bone position: 2 floats = 16 hex chars
        const int boneCount = static_cast<int>(hex.size()) / 16;
        bones.reserve(static_cast<std::size_t>(boneCount));
        for (int i = 0; i < boneCount; ++i) {
            std::uint32_t rawX{};
            std::uint32_t rawY{};
            auto hexToU32 = [](char c) -> std::uint32_t {
                if (c >= '0' && c <= '9') return static_cast<std::uint32_t>(c - '0');
                if (c >= 'A' && c <= 'F') return static_cast<std::uint32_t>(c - 'A' + 10);
                if (c >= 'a' && c <= 'f') return static_cast<std::uint32_t>(c - 'a' + 10);
                return 0;
            };
            const auto* p = hex.data() + static_cast<std::size_t>(i) * 16;
            for (int j = 0; j < 8; ++j) {
                rawX = (rawX << 4) | hexToU32(p[j]);
                rawY = (rawY << 4) | hexToU32(p[j + 8]);
            }
            float fx{}; float fy{};
            std::memcpy(&fx, &rawX, sizeof(fx));
            std::memcpy(&fy, &rawY, sizeof(fy));
            if (!entity.screenPositionNormalized && settings.sourceWidth > 1.0F && settings.sourceHeight > 1.0F) {
                fx = fx * static_cast<float>(width) / settings.sourceWidth;
                fy = static_cast<float>(height) - fy * static_cast<float>(height) / settings.sourceHeight;
            } else {
                fx = fx * static_cast<float>(width);
                fy = static_cast<float>(height) - fy * static_cast<float>(height);
            }
            if (std::isfinite(fx) && std::isfinite(fy) && fx >= -1000.0F && fy >= -1000.0F) {
                bones.push_back({fx, fy});
            }
        }
        if (bones.size() < 2) return;
        Gdiplus::Pen pen(value, std::clamp(settings.lineThickness, 0.5F, 4.0F));
        // Draw bone connection lines
        constexpr int connections[][2] = {
            {0, 1}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}
        };
        for (const auto& conn : connections) {
            if (conn[0] >= static_cast<int>(bones.size()) || conn[1] >= static_cast<int>(bones.size())) continue;
            const auto& a = bones[static_cast<std::size_t>(conn[0])];
            const auto& b = bones[static_cast<std::size_t>(conn[1])];
            graphics.DrawLine(&pen, a.x, a.y, b.x, b.y);
        }
        // Draw circles at bone joints
        const float radius = std::max(2.0F, 3.0F);
        for (const auto& bone : bones) {
            graphics.DrawEllipse(&pen, bone.x - radius, bone.y - radius, radius * 2.0F, radius * 2.0F);
        }
    };

    const auto isTarget = [&](const EntityTelemetry& entity) {
        if (!alive(entity) || entity.address == snapshot.localEntity || entity.isLocal) return false;
        if (!entity.isPlayer) return false;
        if (entity.teamKnown) {
            if (entity.isTeammate && !settings.showTeammates) return false;
            if (!entity.isTeammate && !settings.showEnemies) return false;
        }
        if (local && !local->displayName.empty() && entity.displayName == local->displayName) {
            return false;
        }
        return true;
    };

    for (const auto& entity : snapshot.entities) {
        if (!isTarget(entity)) continue;
        const auto projected = screenPoint(entity, false);
        if (!projected.valid) continue;
        if (projected.edge && !settings.offscreenTargets) continue;
        const auto point = projected.point;
        const auto body = bodyRect(entity, projected, false);
        const Gdiplus::Color entityRainbow = entity.isTeammate ? teammateColor
            : rainbowColor(static_cast<float>(entity.address & 0xFFFF) * 0.001F);
        const Gdiplus::Color entTargetColor = entity.isTeammate ? teammateColor : entityRainbow;
        if (aimSettings.enabled && aimSettings.showLockedTarget && aimStatus.hasTarget &&
            entity.address == aimStatus.targetAddress && !projected.edge) {
            auto lockPoint = point;
            const bool haveExactPoint = aimSettings.point == AimPoint::Head
                ? entity.hasHeadHitScreenPosition
                : entity.hasTorsoHitScreenPosition;
            if (haveExactPoint) {
                const auto& exactPoint = aimSettings.point == AimPoint::Head
                    ? entity.headHitScreenPosition : entity.torsoHitScreenPosition;
                const auto exact = mapScreen(exactPoint, entity.screenPositionNormalized,
                                             false, false, false);
                if (exact.valid) lockPoint = exact.point;
            }
            Gdiplus::Pen lockPen(Gdiplus::Color(245, 255, 220, 70), 2.0F);
            graphics.DrawEllipse(&lockPen, lockPoint.X - 10.0F, lockPoint.Y - 10.0F,
                                 20.0F, 20.0F);
        }
        if (settings.lines) {
            Gdiplus::Pen pen(entityRainbow,
                             std::clamp(settings.lineThickness, 0.5F, 6.0F));
            if (projected.edge) pen.SetDashStyle(Gdiplus::DashStyleDash);
            const float topX = static_cast<float>(width) * 0.5F;
            graphics.DrawLine(&pen, topX, 0.0F, point.X, point.Y);
        }
        if (settings.boxes && body.valid) {
            Gdiplus::Pen boxPen(entTargetColor, std::clamp(settings.lineThickness, 0.5F, 4.0F));
            graphics.DrawRectangle(&boxPen, body.left, body.top, body.width, body.height);
        }
        if (settings.skeleton) drawSkeleton(entity, entity.isTeammate ? teammateColor : skeletonColor);
        if (settings.wireframeTargets) {
            drawWireframe(body, entity.isTeammate ? teammateColor : wireframeColor);
        }

        std::wostringstream label;
        bool haveLabel = false;
        if (settings.names) {
            const auto heroW = utf8ToWide(entity.heroName);
            const auto dispW = utf8ToWide(entity.displayName);
            if (!dispW.empty()) label << dispW;
            else label << L"Jogador " << entity.entityId;
            if (!heroW.empty()) label << L"  |  " << heroW;
            if (entity.isInvisible) label << L" [INVIS]";
            else if (entity.isVanish) label << L" [VANISH]";
            if (entity.isFriend) label << L" [FRIEND]";
            haveLabel = true;
        }
        if (settings.distance) {
            if (haveLabel) label << L"  ";
            label << std::fixed << std::setprecision(1) << entity.distance << L"m";
            haveLabel = true;
        }
        if (settings.health) {
            if (haveLabel) label << L"  ";
            label << entity.health << L"/" << entity.maxHealth;
            haveLabel = true;
        }
        if (settings.health && entity.maxHealth > 0) {
            const float barWidth = body.valid ? body.width : 76.0F;
            const float barX = body.valid ? body.left : point.X + 8.0F;
            const float barY = body.valid ? body.top - 7.0F : point.Y - 8.0F;
            const float ratio = std::clamp(static_cast<float>(entity.health) /
                                           static_cast<float>(entity.maxHealth), 0.0F, 1.0F);
            Gdiplus::SolidBrush barBack(Gdiplus::Color(180, 15, 17, 24));
            Gdiplus::SolidBrush barFill(ratio > 0.55F ? Gdiplus::Color(235, 67, 217, 163)
                : ratio > 0.25F ? Gdiplus::Color(235, 242, 184, 75)
                                : Gdiplus::Color(235, 255, 80, 105));
            graphics.FillRectangle(&barBack, barX, barY, barWidth, 4.0F);
            graphics.FillRectangle(&barFill, barX, barY,
                                   barWidth * ratio, 4.0F);
        }
        if (haveLabel) {
            const float labelX = body.valid ? body.left + body.width + 6.0F : point.X + 8.0F;
            const float labelY = body.valid ? body.top : point.Y;
            drawText(graphics, label.str(), font, labelX, labelY,
                     textColor, settings.labelBackground);
        }
    }

    if (local) {
        if (settings.localMarker) {
            Gdiplus::Pen ring(localColor, 2.0F);
            graphics.DrawEllipse(&ring, localPoint.X - 7.0F, localPoint.Y - 7.0F, 14.0F, 14.0F);
        }
        if (settings.localMarker && settings.localHealth) {
            const std::wstring text = L"Voce  " + std::to_wstring(local->health) + L"/" +
                                      std::to_wstring(local->maxHealth);
            drawText(graphics, text, font, localPoint.X + 10.0F, localPoint.Y - 10.0F,
                     localColor, settings.labelBackground);
        }
    }

    POINT destination{targetRect.left, targetRect.top};
    SIZE size{width, height};
    POINT source{};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(window_, screen, &destination, &size, memoryDc_, &source,
                        0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

} // namespace lunary::ui
