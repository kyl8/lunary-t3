#pragma once

#include "mouse_automation.hpp"

#include <lunary/protocol.hpp>

#include <windows.h>

#include <array>
#include <string>

namespace lunary::ui {

struct OverlaySettings {
    bool enabled{};
    bool lines{};
    bool boxes{};
    bool skeleton{};
    bool wireframeTargets{};
    bool names{};
    bool health{};
    bool distance{};
    bool localMarker{};
    bool localHealth{};
    bool showEnemies{true};
    bool showTeammates{};
    bool labelBackground{};
    bool onlyWhenFocused{};
    bool offscreenTargets{};
    bool syncToMonitor{};
    float sourceWidth{1600.0F};
    float sourceHeight{900.0F};
    float lineThickness{1.5F};
    float fontSize{15.0F};
    float overlayFps{60.0F};
    float boxTopScale{0.52F};
    float boxBottomScale{0.50F};
    float boxWidthRatio{0.68F};
    float rainbowSpeed{1.5F};
    std::array<float, 4> teammateColor{0.20F, 0.58F, 1.0F, 1.0F};
    std::array<float, 4> localColor{1.0F, 0.82F, 0.22F, 1.0F};
    std::array<float, 4> textColor{0.96F, 0.97F, 1.0F, 1.0F};
    std::array<float, 4> skeletonColor{1.0F, 0.34F, 0.36F, 0.95F};
    std::array<float, 4> wireframeColor{0.95F, 0.12F, 0.15F, 0.78F};
};

class EmulatorOverlay {
public:
    EmulatorOverlay() = default;
    ~EmulatorOverlay();

    EmulatorOverlay(const EmulatorOverlay&) = delete;
    EmulatorOverlay& operator=(const EmulatorOverlay&) = delete;

    bool initialize(HINSTANCE instance);
    void shutdown();
    void refreshTarget();
    void render(const OverlaySettings& settings, const AimSettings& aimSettings,
                const AimStatus& aimStatus, const Snapshot& snapshot, bool connected);

    [[nodiscard]] bool targetFound() const;
    [[nodiscard]] std::wstring targetLabel() const;
    [[nodiscard]] int targetRefreshRate() const;
    [[nodiscard]] HWND targetWindow() const;

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void hide();
    bool ensureSurface(int width, int height);
    void destroySurface();

    HINSTANCE instance_{};
    HWND window_{};
    HWND mainWindow_{};
    HWND targetWindow_{};
    HDC memoryDc_{};
    HBITMAP bitmap_{};
    HGDIOBJ previousBitmap_{};
    void* pixels_{};
    int surfaceWidth_{};
    int surfaceHeight_{};
    ULONG_PTR gdiplusToken_{};
    ULONGLONG lastTargetSearch_{};
};

} // namespace lunary::ui
