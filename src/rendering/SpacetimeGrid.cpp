#include "rendering/SpacetimeGrid.hpp"
#include <algorithm>
#include <cmath>

namespace sim {

Color SpacetimeGrid::lerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t)
    };
}

void SpacetimeGrid::render(const Camera2DController& camera,
                           const std::vector<CelestialBody>& bodies,
                           double G,
                           int screenWidth,
                           int screenHeight) {
    if (!enabled) return;

    m_heavyBodyIndices.clear();
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].alive && (bodies[i].isLuminousStar() || bodies[i].isBlackHole() || bodies[i].mass >= 3.0)) {
            m_heavyBodyIndices.push_back(static_cast<int>(i));
        }
    }

    // Dynamic zoom-adaptive grid cell size
    double rawWorldStep = static_cast<double>(baseSpacing) / std::max(0.001, camera.zoom);
    double log2Step = std::log2(std::max(1e-4, rawWorldStep / 48.0));
    double powerOfTwo = std::pow(2.0, std::floor(log2Step));
    double worldStep = powerOfTwo * 48.0;

    Vec2d topLeftWorld = camera.screenToWorld(Vector2{ 0.0f, 0.0f });
    Vec2d bottomRightWorld = camera.screenToWorld(Vector2{ static_cast<float>(screenWidth), static_cast<float>(screenHeight) });

    double margin = worldStep * 3.0;
    double startX = std::floor((topLeftWorld.x - margin) / worldStep) * worldStep;
    double startY = std::floor((topLeftWorld.y - margin) / worldStep) * worldStep;
    double endX = bottomRightWorld.x + margin;
    double endY = bottomRightWorld.y + margin;

    int cols = std::clamp(static_cast<int>((endX - startX) / worldStep) + 1, 2, 76);
    int rows = std::clamp(static_cast<int>((endY - startY) / worldStep) + 1, 2, 54);

    m_vertices.resize(static_cast<size_t>(cols * rows));

    const double gridSofteningSq = (worldStep * 0.55) * (worldStep * 0.55) + 64.0;
    const double maxRadialPull = worldStep * 0.58 * static_cast<double>(warpStrength);
    const double maxFunnelDip = worldStep * 1.15 * static_cast<double>(warpStrength);

    for (int r = 0; r < rows; ++r) {
        double wy = startY + r * worldStep;
        for (int c = 0; c < cols; ++c) {
            double wx = startX + c * worldStep;
            Vec2d gridPos(wx, wy);

            Vec2d gradient{ 0.0, 0.0 };
            double potentialMag = 0.0;

            for (int idx : m_heavyBodyIndices) {
                const CelestialBody& b = bodies[idx];
                Vec2d delta = b.position - gridPos;
                double dSq = delta.lengthSq() + gridSofteningSq;
                double d = std::sqrt(dSq);

                // Black holes & neutron stars produce extra-steep relativistic wells
                double wellBoost = b.isBlackHole() ? 2.4 : (b.bodyType == CelestialBodyType::NeutronStar ? 1.8 : 1.0);
                double localPot = (G * b.mass * wellBoost) / d;
                potentialMag += localPot;

                gradient += delta * (localPot / (dSq + b.radius * b.radius));
            }

            double gradLen = gradient.length();
            Vec2d displacedWorld = gridPos;
            if (gradLen > 1e-6) {
                double pullAmount = maxRadialPull * std::tanh(gradLen * 0.018 * static_cast<double>(warpStrength));
                displacedWorld += (gradient / gradLen) * pullAmount;
            }

            // 2.5D gravity well vertical funnel displacement
            double funnelDip = maxFunnelDip * std::tanh(potentialMag * 0.00018 * static_cast<double>(warpStrength));
            displacedWorld.y += funnelDip;

            float normPot = static_cast<float>(std::clamp(std::log1p(potentialMag * 0.0018) * 0.36, 0.0, 1.0));
            Color vColor;
            if (normPot < 0.50f) {
                vColor = lerpColor(Config::GRID_FLAT_COLOR, Config::GRID_WELL_MID_COLOR, normPot / 0.50f);
            } else {
                vColor = lerpColor(Config::GRID_WELL_MID_COLOR, Config::GRID_WELL_DEEP_COLOR, (normPot - 0.50f) / 0.50f);
            }

            GridVertex& gv = m_vertices[static_cast<size_t>(r * cols + c)];
            gv.screenPos = camera.worldToScreen(displacedWorld);
            gv.normalizedPotential = normPot;
            gv.color = vColor;
        }
    }

    // Pass 1: Crisp base spacetime mesh lines
    for (int r = 0; r < rows; ++r) {
        int worldRowIdx = static_cast<int>(std::round((startY + r * worldStep) / worldStep));
        bool isMajorRow = (worldRowIdx % 4) == 0;

        for (int c = 0; c < cols; ++c) {
            int worldColIdx = static_cast<int>(std::round((startX + c * worldStep) / worldStep));
            bool isMajorCol = (worldColIdx % 4) == 0;

            const GridVertex& curr = m_vertices[static_cast<size_t>(r * cols + c)];

            if (c + 1 < cols) {
                const GridVertex& right = m_vertices[static_cast<size_t>(r * cols + (c + 1))];
                float pot = std::max(curr.normalizedPotential, right.normalizedPotential);
                Color segColor = (curr.normalizedPotential > right.normalizedPotential) ? curr.color : right.color;
                if (!isMajorRow && pot < 0.15f) {
                    segColor.a = static_cast<unsigned char>(segColor.a * 0.58f);
                }
                DrawLineV(curr.screenPos, right.screenPos, segColor);
            }
            if (r + 1 < rows) {
                const GridVertex& down = m_vertices[static_cast<size_t>((r + 1) * cols + c)];
                float pot = std::max(curr.normalizedPotential, down.normalizedPotential);
                Color segColor = (curr.normalizedPotential > down.normalizedPotential) ? curr.color : down.color;
                if (!isMajorCol && pot < 0.15f) {
                    segColor.a = static_cast<unsigned char>(segColor.a * 0.58f);
                }
                DrawLineV(curr.screenPos, down.screenPos, segColor);
            }
        }
    }

    // Pass 2: Additive neon glow on deeply warped gravity well funnels
    BeginBlendMode(BLEND_ADDITIVE);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const GridVertex& curr = m_vertices[static_cast<size_t>(r * cols + c)];
            if (curr.normalizedPotential < 0.25f) continue;

            if (c + 1 < cols) {
                const GridVertex& right = m_vertices[static_cast<size_t>(r * cols + (c + 1))];
                Color glowCol = curr.color;
                glowCol.a = static_cast<unsigned char>(std::clamp((curr.normalizedPotential - 0.2f) * 145.0f, 0.0f, 130.0f));
                DrawLineEx(curr.screenPos, right.screenPos, 2.0f, glowCol);
            }
            if (r + 1 < rows) {
                const GridVertex& down = m_vertices[static_cast<size_t>((r + 1) * cols + c)];
                Color glowCol = curr.color;
                glowCol.a = static_cast<unsigned char>(std::clamp((curr.normalizedPotential - 0.2f) * 145.0f, 0.0f, 130.0f));
                DrawLineEx(curr.screenPos, down.screenPos, 2.0f, glowCol);
            }
        }
    }
    EndBlendMode();
}

} // namespace sim
