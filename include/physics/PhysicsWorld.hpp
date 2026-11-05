#pragma once

#include "core/Config.hpp"
#include "core/Vec2d.hpp"
#include "physics/BarnesHutTree.hpp"
#include "physics/CelestialBody.hpp"
#include "physics/CollisionSystem.hpp"
#include "physics/Integrator.hpp"
#include "physics/Spacecraft.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sim {

struct PredictedTrajectory {
    std::vector<Vec2d> points;
    bool hasImpact = false;
    Vec2d impactPoint{ 0.0, 0.0 };
    std::string impactBodyName;

    bool hasPeriapsis = false;
    Vec2d periapsisPoint{ 0.0, 0.0 };
    double periapsisDist = 0.0;

    bool hasApoapsis = false;
    Vec2d apoapsisPoint{ 0.0, 0.0 };
    double apoapsisDist = 0.0;
};

struct SystemDiagnostics {
    double kineticEnergy = 0.0;
    double potentialEnergy = 0.0;
    double totalEnergy = 0.0;
    double initialTotalEnergy = 0.0;
    double energyDriftPercent = 0.0;
    double totalMomentumMag = 0.0;
    Vec2d barycenter{ 0.0, 0.0 };
    Vec2d barycentricVelocity{ 0.0, 0.0 };
    double physicsStepTimeMs = 0.0;
    std::vector<float> energyHistory;
    bool baselineCaptured = false;
};

class PhysicsWorld {
public:
    PhysicsWorld();

    // Simulation state & pre-allocated visual effect pools
    std::vector<CelestialBody> bodies;
    Spacecraft spacecraft;
    std::vector<ShockwaveEvent> shockwaves;
    std::vector<ImpactSpark> impactSparks;
    std::vector<NebulaParticle> nebulaParticles;

    // Configurable physics settings
    bool paused = false;
    double G = Config::DEFAULT_G;
    double softening = Config::DEFAULT_SOFTENING;
    double barnesHutTheta = Config::DEFAULT_BARNES_HUT_THETA;
    double timeScale = 1.0;
    int subSteps = 4;
    bool autoSubStepping = true;

    GravitySolverType solverType = GravitySolverType::DirectNBody;
    IntegratorType integratorType = IntegratorType::VelocityVerlet;
    CollisionMode collisionMode = CollisionMode::RealisticHybrid;

    double restitution = Config::DEFAULT_RESTITUTION;
    double fragmentationThreshold = Config::DEFAULT_FRAGMENTATION_THRESHOLD;
    int maxDebrisCap = Config::MAX_DEBRIS_CAP;

    // Trail & prediction settings
    bool showTrails = true;
    size_t maxTrailLength = Config::DEFAULT_TRAIL_CAPACITY;
    int referenceFrameBodyId = -1; // -1 = World space

    bool showTrajectoryPrediction = true;
    int predictionSteps = Config::DEFAULT_PREDICTION_STEPS;
    double predictionStepDt = Config::DEFAULT_PREDICTION_DT;

    // Selection & camera tracking IDs (-1 = none, -2 = spacecraft)
    int selectedBodyId = -1;
    int cameraFollowId = -1;
    bool trackBarycenter = false;

    // Diagnostics & Quadtree
    SystemDiagnostics diagnostics;
    BarnesHutTree barnesHutTree;

    // Lifecycle & stepping
    void clear();
    uint32_t allocateBodyId();
    CelestialBody& addBody(const CelestialBody& prototype);
    CelestialBody* findBodyById(int id);
    const CelestialBody* findBodyById(int id) const;
    const CelestialBody* findNearestStarOrHeavyBody(const Vec2d& pos, int excludeId = -1) const;

    void update(double frameDt, bool uiCapturesKeyboard);
    void stepSingleFrame();

    void computeEnergyAndMomentum(bool resetBaseline = false);

    // Live RK4 / Verlet trajectory lookahead
    PredictedTrajectory computePredictedTrajectory(const CelestialBody& probe,
                                                   int steps,
                                                   double stepDt,
                                                   int referenceBodyId) const;

    // Action utilities for Game HUD & Inspector
    void setCircularOrbitAroundNearest(int bodyId, bool clockwise = false);
    void splitSelectedBody(int bodyId);
    void triggerSupernova(int bodyId = -1);
    void spawnAsteroidBelt(int centerBodyId = -1, int count = 160);
    void spawnMeteorBarrage(int targetBodyId = -1);
    void applyCosmicScaleMode(bool logarithmicScale);
    void deleteBody(int bodyId);
    void clearAllTrails();

private:
    uint32_t m_nextBodyId = 1;
    Integrator m_integrator;
    CollisionSystem m_collisionSystem;
    double m_diagnosticsTimer = 0.0;
};

} // namespace sim
