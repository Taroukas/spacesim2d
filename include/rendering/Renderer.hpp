#pragma once

#include "core/Config.hpp"
#include "core/Vec2d.hpp"
#include "physics/PhysicsWorld.hpp"
#include "rendering/Camera2DController.hpp"
#include "rendering/SpacetimeGrid.hpp"
#include "rendering/Starfield.hpp"
#include <raylib.h>
#include <vector>

namespace sim {

struct SpawnerPreviewState {
    bool active = false;
    Vec2d spawnWorldPos{ 0.0, 0.0 };
    Vec2d launchVelocity{ 0.0, 0.0 };
    CelestialBody prototypeBody;
    PredictedTrajectory trajectory;
    bool circularSnapActive = false;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    void initialize();
    void unload();

    void render(const PhysicsWorld& world,
                const Camera2DController& camera,
                const SpawnerPreviewState& spawnerPreview,
                int screenWidth,
                int screenHeight,
                double elapsedSeconds);

    // Visual overlay toggles
    SpacetimeGrid spacetimeGrid;
    bool showQuadtreeDebug = false;
    bool showVelocityVectors = false;
    bool showAccelerationVectors = false;
    bool showBodyLabels = true;
    bool showBarycenter = false;
    bool showBloom = true;
    bool showRadar = true;
    float starGlowIntensity = 1.30f;

    void addCameraShake(float intensity);

private:
    Starfield m_starfield;
    mutable std::vector<Rectangle> m_quadtreeRects;

    // Full-Screen Offscreen Target & Einstein Relativistic Gravitational Lensing Shader (ported from SpaceSim2D)
    RenderTexture2D m_sceneTarget{};
    int m_targetWidth = 0;
    int m_targetHeight = 0;
    Shader m_postShader{};
    int m_locLensCount = -1;
    int m_locLensPos = -1;
    int m_locLensRadius = -1;
    int m_locAspectRatio = -1;
    int m_locBloomEnabled = -1;
    mutable float m_cameraShake = 0.0f;

    // GLSL 3D Spherical Normal-Mapping & Atmospheric Scattering Shader (ported from SpaceSim2D)
    Shader m_bodyShader{};
    int m_locBodyTime = -1;
    int m_locBodyMode = -1;
    int m_locLightDir = -1;
    int m_locMergeGeom = -1;
    int m_locMergeColorAndOsc = -1;

    void checkTargetResize(int screenWidth, int screenHeight);
    void updateLensingShaderUniforms(const PhysicsWorld& world, const Camera2DController& camera, int screenWidth, int screenHeight);
    void drawTacticalRadar(const PhysicsWorld& world, const Camera2DController& camera, int screenWidth, int screenHeight) const;

    static void drawPlanetaryRings3D(Vector2 center, float drawR, Color col, Vec2d toLight, bool frontHalf);
    static void drawTaperedNeedleSpike(Vector2 center, float angle, float halfLen, float maxHalfThick, Color col, float alpha);

    void renderQuadtreeDebug(const PhysicsWorld& world, const Camera2DController& camera) const;
    void renderNebulaClouds(const PhysicsWorld& world, const Camera2DController& camera) const;
    void renderOrbitalTrails(const PhysicsWorld& world, const Camera2DController& camera) const;
    void renderPredictedTrajectory(const PredictedTrajectory& traj,
                                   const PhysicsWorld& world,
                                   const Camera2DController& camera,
                                   Color curveColor) const;
    void renderCelestialBodies(const PhysicsWorld& world,
                               const Camera2DController& camera,
                               int screenWidth,
                               int screenHeight,
                               double elapsedSeconds) const;
    void renderBlackHole(const CelestialBody& b,
                         Vector2 screenPos,
                         float screenRadius,
                         double elapsedSeconds) const;
    void renderStarBody(const CelestialBody& b,
                        Vector2 screenPos,
                        float screenRadius,
                        double elapsedSeconds) const;
    void renderPlanetBody(const CelestialBody& b,
                          Vector2 screenPos,
                          float screenRadius,
                          bool hasLightSource,
                          Vec2d lightDir,
                          double elapsedSeconds) const;
    void renderSpacecraft(const Spacecraft& ship,
                          const Camera2DController& camera,
                          double elapsedSeconds) const;
    void renderVectorOverlays(const PhysicsWorld& world,
                              const Camera2DController& camera) const;
    void renderImpactSparksAndShockwaves(const PhysicsWorld& world,
                                         const Camera2DController& camera) const;
    void renderSpawnerPreview(const SpawnerPreviewState& preview,
                              const PhysicsWorld& world,
                              const Camera2DController& camera,
                              double elapsedSeconds) const;

    static void drawArrowScreen(Vector2 start, Vector2 end, float thickness, Color color);
    static void drawDashedLineScreen(Vector2 start, Vector2 end, float dashLen, float thickness, Color color);
};

} // namespace sim
