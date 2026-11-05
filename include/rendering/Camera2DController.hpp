#pragma once

#include "core/Vec2d.hpp"
#include "physics/PhysicsWorld.hpp"
#include <raylib.h>

namespace sim {

class Camera2DController {
public:
    Camera2DController(int screenWidth, int screenHeight);

    void update(double dt, PhysicsWorld& world, bool uiCapturesMouse, bool uiCapturesKeyboard);
    void onResize(int width, int height);

    void focusOnPosition(const Vec2d& pos, double targetZoom = -1.0);
    void resetView();

    // Coordinate transformations using 64-bit precision relative to camera center
    Vec2d screenToWorld(Vector2 screenPos) const;
    Vector2 worldToScreen(const Vec2d& worldPos) const;

    Camera2D getRaylibCamera() const;

    Vec2d position{ 0.0, 0.0 };
    Vec2d targetPosition{ 0.0, 0.0 };
    Vec2d panVelocity{ 0.0, 0.0 };
    double zoom = 0.45;
    double targetZoom = 0.45;

    static constexpr double MIN_ZOOM = 0.0005;
    static constexpr double MAX_ZOOM = 50.0;

private:
    int m_screenWidth = 1600;
    int m_screenHeight = 950;
    bool m_isPanning = false;
    Vector2 m_lastMousePos{ 0.0f, 0.0f };
    Vector2 m_zoomAnchorScreen{ 800.0f, 475.0f };
    Vec2d m_zoomAnchorWorld{ 0.0, 0.0 };
    bool m_hasZoomAnchor = false;
    float m_shakeIntensity = 0.0f;
    Vector2 m_shakeOffset{ 0.0f, 0.0f };
};

} // namespace sim
