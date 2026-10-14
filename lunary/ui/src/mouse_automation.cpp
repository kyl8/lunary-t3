#include "mouse_automation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lunary::ui {

namespace {

struct ScreenPoint {
    float x{};
    float y{};
    bool valid{};
};

ScreenPoint mapPoint(const Vec2& source, bool normalized, int width, int height,
                     float sourceWidth, float sourceHeight, bool flipScreenY) {
    if (!normalized && (sourceWidth <= 1.0F || sourceHeight <= 1.0F)) return {};
    float x = normalized ? source.x * static_cast<float>(width)
                         : source.x * static_cast<float>(width) / sourceWidth;
    float y = normalized ? source.y * static_cast<float>(height)
                         : source.y * static_cast<float>(height) / sourceHeight;
    if (flipScreenY) y = static_cast<float>(height) - y;
    const bool valid = std::isfinite(x) && std::isfinite(y) && x >= 0.0F && y >= 0.0F &&
                       x <= static_cast<float>(width) && y <= static_cast<float>(height);
    return {x, y, valid};
}

bool isAlive(const EntityTelemetry& entity) {
    return entity.health > 0 && entity.maxHealth > 0;
}

} // namespace

void MouseAutomation::reset() {
    std::scoped_lock lock(mutex_);
    status_ = {};
    status_.message = "Desativada";
    lockedAddress_ = 0;
    lastCommandSequence_ = 0;
    lastSampleAddress_ = 0;
    lastInputTick_ = 0;
    lastSampleX_ = lastSampleY_ = 0.0F;
    residualX_ = residualY_ = 0.0F;
}

AimStatus MouseAutomation::status() const {
    std::scoped_lock lock(mutex_);
    return status_;
}

void MouseAutomation::update(const AimSettings& settings, const Snapshot& snapshot,
                             bool connected, bool dataFresh, HWND targetWindow,
                             float sourceWidth, float sourceHeight, bool flipScreenY) {
    std::scoped_lock lock(mutex_);
    status_.activationHeld = settings.activationKey > 0 &&
        (GetAsyncKeyState(settings.activationKey) & 0x8000) != 0;
    const HWND foregroundRoot = GetAncestor(GetForegroundWindow(), GA_ROOT);
    const HWND targetRoot = targetWindow ? GetAncestor(targetWindow, GA_ROOT) : nullptr;
    status_.targetWindowFocused = targetRoot && foregroundRoot == targetRoot;

    const auto stop = [&](const char* message, bool resetTiming = false) {
        status_.hasTarget = false;
        status_.targetAddress = 0;
        status_.screenDistance = 0.0F;
        status_.targetName.clear();
        status_.message = message;
        lockedAddress_ = 0;
        lastCommandSequence_ = 0;
        lastSampleAddress_ = 0;
        lastSampleX_ = lastSampleY_ = 0.0F;
        residualX_ = residualY_ = 0.0F;
        if (resetTiming) lastInputTick_ = 0;
    };

    if (!settings.enabled) {
        stop("Desativada", true);
        return;
    }
    if (!connected) {
        stop("Aguardando conexao", true);
        return;
    }
    if (!targetWindow || !IsWindow(targetWindow)) {
        stop("LDPlayer nao encontrado", true);
        return;
    }
    if (!status_.targetWindowFocused) {
        stop("Aguardando foco no LDPlayer", true);
        return;
    }
    if (!status_.activationHeld) {
        stop("Aguardando tecla de mira", true);
        return;
    }
    if (!dataFresh) {
        stop("Telemetria desatualizada", true);
        return;
    }

    RECT bounds{};
    if (!GetWindowRect(targetWindow, &bounds)) {
        stop("Falha ao ler area do LDPlayer", true);
        return;
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (width <= 1 || height <= 1) {
        stop("Area de renderizacao invalida", true);
        return;
    }

    struct Candidate {
        const EntityTelemetry* entity{};
        ScreenPoint point{};
        float distanceSquared{std::numeric_limits<float>::max()};
    };
    Candidate nearest;
    Candidate locked;
    std::size_t exactHitPoints{};
    const float centerX = static_cast<float>(width) * 0.5F;
    const float centerY = static_cast<float>(height) * 0.5F;
    const float fov = std::clamp(settings.fovRadius, 10.0F,
                                 static_cast<float>(std::max(width, height)));
    const float fovSquared = fov * fov;

    for (const auto& entity : snapshot.entities) {
        if (!isAlive(entity) || entity.isLocal || entity.address == snapshot.localEntity ||
            entity.behindCamera || !entity.hasScreenPosition) continue;
        if (settings.onlyPlayers && !entity.isPlayer) continue;
        if (entity.teamKnown && settings.ignoreTeammates && entity.isTeammate) continue;
        if (!entity.teamKnown && !settings.allowUnknownTeams) continue;
        if (settings.maxDistance > 0.0F && entity.distance > settings.maxDistance) continue;

        const bool haveExactPoint = settings.point == AimPoint::Head
            ? entity.hasHeadHitScreenPosition
            : entity.hasTorsoHitScreenPosition;
        if (!haveExactPoint) continue;
        ++exactHitPoints;
        const auto& exactPoint = settings.point == AimPoint::Head
            ? entity.headHitScreenPosition : entity.torsoHitScreenPosition;
        const auto aim = mapPoint(exactPoint, entity.screenPositionNormalized,
                                  width, height, sourceWidth, sourceHeight, flipScreenY);
        if (!aim.valid) continue;
        const float dx = aim.x - centerX;
        const float dy = aim.y - centerY;
        const float distanceSquared = dx * dx + dy * dy;
        Candidate candidate{&entity, aim, distanceSquared};
        if (settings.lockTarget && entity.address == lockedAddress_) locked = candidate;
        if (distanceSquared <= fovSquared &&
            (!nearest.entity || distanceSquared < nearest.distanceSquared)) nearest = candidate;
    }

    const Candidate selected = settings.lockTarget && locked.entity ? locked : nearest;
    if (!selected.entity) {
        stop(exactHitPoints == 0 ? "Aguardando hitpoints exatos"
                                 : "Nenhum alvo dentro do FOV");
        return;
    }

    if (lockedAddress_ != selected.entity->address) {
        residualX_ = residualY_ = 0.0F;
        lastCommandSequence_ = 0;
        lastSampleAddress_ = 0;
    }
    lockedAddress_ = selected.entity->address;
    status_.hasTarget = true;
    status_.targetAddress = lockedAddress_;
    status_.screenDistance = std::sqrt(selected.distanceSquared);
    status_.targetName = selected.entity->displayName.empty()
        ? selected.entity->heroName : selected.entity->displayName;
    status_.message = "Alvo travado";

    const ULONGLONG now = GetTickCount64();
    const bool repeatedSample = lastSampleAddress_ == selected.entity->address &&
        std::abs(lastSampleX_ - selected.point.x) < 0.02F &&
        std::abs(lastSampleY_ - selected.point.y) < 0.02F;
    if (repeatedSample ||
        (snapshot.sequence != 0 && snapshot.sequence == lastCommandSequence_)) {
        return;
    }
    const ULONGLONG elapsed = lastInputTick_ == 0 ? 16 : now - lastInputTick_;
    if (lastInputTick_ != 0 &&
        elapsed < static_cast<ULONGLONG>(std::clamp(settings.updateIntervalMs, 1, 50))) {
        return;
    }
    lastInputTick_ = now;
    lastCommandSequence_ = snapshot.sequence;
    lastSampleAddress_ = selected.entity->address;
    lastSampleX_ = selected.point.x;
    lastSampleY_ = selected.point.y;

    const float smoothing = std::clamp(settings.smoothing, 0.0F, 30.0F);
    const float strength = std::clamp(settings.strength, 0.1F, 2.0F);
    const float smoothRatio = smoothing / 30.0F;
    const float responseAt16Ms = 0.90F + (0.08F - 0.90F) * smoothRatio;
    const float deltaScale = static_cast<float>(std::clamp<ULONGLONG>(elapsed, 1, 32)) / 16.0F;
    const float proximity = std::clamp(status_.screenDistance / 10.0F, 0.90F, 1.0F);
    const float response = responseAt16Ms * deltaScale * strength * proximity;
    const float maxStep = std::max(1.0F,
        std::clamp(settings.maxStep, 2.0F, 50.0F) * deltaScale);
    if (status_.screenDistance < 0.65F) {
        residualX_ = residualY_ = 0.0F;
        return;
    }
    const float desiredX = std::clamp((selected.point.x - centerX) * response + residualX_,
                                      -maxStep, maxStep);
    const float desiredY = std::clamp((selected.point.y - centerY) * response + residualY_,
                                      -maxStep, maxStep);
    const LONG moveX = static_cast<LONG>(std::round(desiredX));
    const LONG moveY = static_cast<LONG>(std::round(desiredY));
    residualX_ = desiredX - static_cast<float>(moveX);
    residualY_ = desiredY - static_cast<float>(moveY);
    if (moveX == 0 && moveY == 0) return;

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = moveX;
    input.mi.dy = moveY;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    if (SendInput(1, &input, sizeof(input)) != 1) {
        status_.message = "Falha ao enviar movimento";
    }
}

} // namespace lunary::ui
