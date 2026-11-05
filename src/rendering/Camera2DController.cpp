#include "rendering/Camera2DController.hpp"
#include <algorithm>
#include <cmath>

namespace sim {

Camera2DController::Camera2DController(int screenWidth, int screenHeight)
    : m_screenWidth(screenWidth), m_screenHeight(screenHeight) {}

void Camera2DController::onResize(int width, int height) {
    m_screenWidth = width;
    m_screenHeight = height;
}

void Camera2DController::focusOnPosition(const Vec2d& pos, double newTargetZoom) {
    targetPosition = pos;
    position = pos;
    panVelocity = { 0.0, 0.0 };
    m_hasZoomAnchor = false;
    if (newTargetZoom > 0.0) {
        targetZoom = std::clamp(newTargetZoom, MIN_ZOOM, MAX_ZOOM);
        zoom = targetZoom;
    }
}

void Camera2DController::resetView() {
    targetPosition = { 0.0, 0.0 };
    panVelocity = { 0.0, 0.0 };
    m_hasZoomAnchor = false;
    targetZoom = Config::use_logarithmic_scale ? 0.36 : 0.14;
}

void Camera2DController::update(double dt, PhysicsWorld& world, bool uiCapturesMouse, bool uiCapturesKeyboard) {
    double clampedDt = std::min(dt, 0.1);

    // Keyboard shortcuts when UI does not capture keyboard
    if (!uiCapturesKeyboard) {
        if (IsKeyPressed(KEY_F)) {
            if (world.selectedBodyId != -1) {
                world.cameraFollowId = world.selectedBodyId;
                world.trackBarycenter = false;
                panVelocity = { 0.0, 0.0 };
                m_hasZoomAnchor = false;
            } else if (world.spacecraft.enabled) {
                world.cameraFollowId = -2; // -2 = follow spacecraft
                world.trackBarycenter = false;
                panVelocity = { 0.0, 0.0 };
                m_hasZoomAnchor = false;
            }
        }
        if (IsKeyPressed(KEY_HOME)) {
            world.cameraFollowId = -1;
            world.trackBarycenter = false;
            resetView();
        }
        if (IsKeyPressed(KEY_B)) {
            world.trackBarycenter = !world.trackBarycenter;
            if (world.trackBarycenter) {
                world.cameraFollowId = -1;
                panVelocity = { 0.0, 0.0 };
            }
        }

        // Smooth inertial arrow-key / WASD camera panning when not piloting spacecraft
        if (!world.spacecraft.enabled) {
            Vec2d keyDir{ 0.0, 0.0 };
            if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) keyDir.x -= 1.0;
            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) keyDir.x += 1.0;
            if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) keyDir.y -= 1.0;
            if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) keyDir.y += 1.0;

            if (keyDir.lengthSq() > 0.0) {
                world.cameraFollowId = -1;
                world.trackBarycenter = false;
                m_hasZoomAnchor = false;
                double panAccel = 2200.0 / std::max(MIN_ZOOM, zoom);
                panVelocity += keyDir.normalized() * (panAccel * clampedDt);
            }
        }
    }

    // Panning via Right-Click drag or Middle-Click drag with release momentum inertia
    if (!uiCapturesMouse) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
            m_isPanning = true;
            m_lastMousePos = GetMousePosition();
            panVelocity = { 0.0, 0.0 };
            m_hasZoomAnchor = false;
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) || IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) {
        m_isPanning = false;
    }

    if (m_isPanning) {
        Vector2 currentMouse = GetMousePosition();
        Vector2 delta{ currentMouse.x - m_lastMousePos.x, currentMouse.y - m_lastMousePos.y };
        m_lastMousePos = currentMouse;

        if (std::abs(delta.x) > 0.2f || std::abs(delta.y) > 0.2f) {
            // Detach camera tracking on manual drag
            world.cameraFollowId = -1;
            world.trackBarycenter = false;
            m_hasZoomAnchor = false;
            Vec2d worldDelta(static_cast<double>(delta.x) / zoom,
                             static_cast<double>(delta.y) / zoom);
            position -= worldDelta;
            targetPosition = position;

            // Record inertial velocity so releasing the mouse glides weightlessly
            if (clampedDt > 1e-4) {
                Vec2d instVel = -worldDelta / clampedDt;
                panVelocity = panVelocity * 0.45 + instVel * 0.55;
            }
        } else {
            panVelocity *= std::exp(-12.0 * clampedDt);
        }
    } else if (world.cameraFollowId == -1 && !world.trackBarycenter) {
        // Apply inertial glide & exponential friction damping in free-cam mode
        if (panVelocity.lengthSq() > 1e-3) {
            targetPosition += panVelocity * clampedDt;
            panVelocity *= std::exp(-5.8 * clampedDt);
        } else {
            panVelocity = { 0.0, 0.0 };
        }
    }

    // Cursor-centered exponential zoom when not holding Left-Click (which scales spawner mass)
    if (!uiCapturesMouse && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.001f) {
            m_zoomAnchorScreen = GetMousePosition();
            m_zoomAnchorWorld = screenToWorld(m_zoomAnchorScreen);

            double zoomFactor = std::exp(static_cast<double>(wheel) * 0.18);
            targetZoom = std::clamp(targetZoom * zoomFactor, MIN_ZOOM, MAX_ZOOM);

            if (world.cameraFollowId == -1 && !world.trackBarycenter) {
                m_hasZoomAnchor = true;
                panVelocity = { 0.0, 0.0 };
            }
        }
    }

    // Smooth exponential zoom interpolation
    double zoomLerp = 1.0 - std::exp(-12.5 * clampedDt);
    zoom += (targetZoom - zoom) * zoomLerp;

    if (m_hasZoomAnchor && world.cameraFollowId == -1 && !world.trackBarycenter) {
        Vec2d screenOffset(static_cast<double>(m_zoomAnchorScreen.x) - m_screenWidth * 0.5,
                           static_cast<double>(m_zoomAnchorScreen.y) - m_screenHeight * 0.5);
        position = m_zoomAnchorWorld - screenOffset / zoom;
        targetPosition = position;
        if (std::abs(targetZoom - zoom) / std::max(MIN_ZOOM, targetZoom) < 0.002) {
            m_hasZoomAnchor = false;
        }
    }

    // Target Tracking
    if (world.trackBarycenter) {
        targetPosition = world.diagnostics.barycenter;
        m_hasZoomAnchor = false;
    } else if (world.cameraFollowId == -2 && world.spacecraft.enabled) {
        targetPosition = world.spacecraft.position;
        m_hasZoomAnchor = false;
    } else if (world.cameraFollowId >= 0) {
        if (const CelestialBody* followed = world.findBodyById(world.cameraFollowId)) {
            targetPosition = followed->position;
            m_hasZoomAnchor = false;
        } else {
            world.cameraFollowId = -1;
        }
    }

    // Smooth exponential position interpolation
    double posLerp = 1.0 - std::exp(-11.5 * clampedDt);
    if (world.cameraFollowId != -1 || world.trackBarycenter) {
        // Inertial smooth lock onto moving bodies
        position += (targetPosition - position) * std::min(1.0, posLerp * 1.45);
    } else if (!m_hasZoomAnchor) {
        position += (targetPosition - position) * posLerp;
    }

    // Subtle camera shake on major cataclysmic shockwaves (ported from SpaceSim2D)
    float targetShake = 0.0f;
    if (!world.paused) {
        for (const auto& sw : world.shockwaves) {
            float progress = std::clamp(sw.age / sw.duration, 0.0f, 1.0f);
            if (progress < 0.45f) {
                float ratio = 1.0f - (progress / 0.45f);
                Vector2 sp = worldToScreen(sw.position);
                float dx = (sp.x - m_screenWidth * 0.5f) / static_cast<float>(std::max(1, m_screenWidth));
                float dy = (sp.y - m_screenHeight * 0.5f) / static_cast<float>(std::max(1, m_screenHeight));
                float distAtten = 1.0f / (1.0f + (dx * dx + dy * dy) * 2.2f);
                float screenRad = static_cast<float>(sw.maxRadius * zoom);
                float power = std::clamp(screenRad / 65.0f, 0.15f, sw.isSupernova ? 1.6f : 1.1f);
                targetShake = std::max(targetShake, 7.5f * power * ratio * ratio * distAtten);
            }
        }
    }
    m_shakeIntensity = std::max(targetShake, m_shakeIntensity - static_cast<float>(clampedDt) * 24.0f);
    if (m_shakeIntensity > 0.05f && !world.paused) {
        float t = static_cast<float>(GetTime());
        m_shakeOffset.x = (std::sin(t * 67.0f) * 0.65f + std::cos(t * 113.0f) * 0.35f) * m_shakeIntensity;
        m_shakeOffset.y = (std::cos(t * 73.0f) * 0.65f + std::sin(t * 127.0f) * 0.35f) * m_shakeIntensity;
    } else {
        m_shakeOffset = { 0.0f, 0.0f };
    }
}

Vec2d Camera2DController::screenToWorld(Vector2 screenPos) const {
    double dx = (static_cast<double>(screenPos.x - m_shakeOffset.x) - m_screenWidth * 0.5) / zoom;
    double dy = (static_cast<double>(screenPos.y - m_shakeOffset.y) - m_screenHeight * 0.5) / zoom;
    return Vec2d(position.x + dx, position.y + dy);
}

Vector2 Camera2DController::worldToScreen(const Vec2d& worldPos) const {
    // Subtract 64-bit camera position first to avoid float precision loss at large coordinates
    Vec2d rel = (worldPos - position) * zoom;
    return Vector2{
        static_cast<float>(rel.x + m_screenWidth * 0.5) + m_shakeOffset.x,
        static_cast<float>(rel.y + m_screenHeight * 0.5) + m_shakeOffset.y
    };
}

Camera2D Camera2DController::getRaylibCamera() const {
    Camera2D cam{};
    cam.offset = Vector2{ m_screenWidth * 0.5f, m_screenHeight * 0.5f };
    cam.target = Vector2{ static_cast<float>(position.x), static_cast<float>(position.y) };
    cam.rotation = 0.0f;
    cam.zoom = static_cast<float>(zoom);
    return cam;
}

} // namespace sim
