#pragma once

#include "core/Config.hpp"
#include "core/Vec2d.hpp"
#include "physics/CelestialBody.hpp"
#include "rendering/Camera2DController.hpp"
#include <raylib.h>
#include <vector>

namespace sim {

class SpacetimeGrid {
public:
    SpacetimeGrid() {
        m_vertices.reserve(4096);
        m_heavyBodyIndices.reserve(64);
    }

    bool enabled = true;
    float baseSpacing = 48.0f;
    float warpStrength = 1.25f;

    void render(const Camera2DController& camera,
                const std::vector<CelestialBody>& bodies,
                double G,
                int screenWidth,
                int screenHeight);

private:
    struct GridVertex {
        Vector2 screenPos{ 0.0f, 0.0f };
        float normalizedPotential = 0.0f;
        Color color = Config::GRID_FLAT_COLOR;
    };

    std::vector<GridVertex> m_vertices;
    std::vector<int> m_heavyBodyIndices;

    static Color lerpColor(Color a, Color b, float t);
};

} // namespace sim
