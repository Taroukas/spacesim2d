#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <raylib.h>

namespace sim {

enum class GravitySolverType {
    DirectNBody = 0,
    BarnesHut = 1
};

enum class IntegratorType {
    VelocityVerlet = 0,
    RungeKutta4 = 1
};

enum class CollisionMode {
    InelasticMerge = 0,
    ElasticBounce = 1,
    RealisticHybrid = 2
};

enum class CelestialBodyType : int {
    RockyPlanet = 0,
    CrateredMoon = 1,
    GasGiant = 2,
    YellowStar = 3,
    BlueHypergiant = 4,
    BlackHole = 5,
    NeutronStar = 6,
    Asteroid = 7
};

struct Config {
    // Window settings
    static constexpr int DEFAULT_SCREEN_WIDTH = 1600;
    static constexpr int DEFAULT_SCREEN_HEIGHT = 950;
    static constexpr int TARGET_FPS = 60;

    // Vast Deep-Space Scaling Mode (toggleable at runtime)
    // When true: uses a hybrid logarithmic/power curve so inner rocky planets remain distinct
    //            while outer gas giants sit far out in the vast dark void.
    // When false: uses true linear astronomical AU proportions across immense cosmic distances.
    inline static bool use_logarithmic_scale = true;

    static double scaleOrbitalDistanceAU(double au) {
        if (use_logarithmic_scale) {
            // Hybrid logarithmic + fractional power curve:
            // 0.39 AU -> ~175, 0.72 AU -> ~265, 1.0 AU -> ~345, 1.52 AU -> ~475,
            // 5.20 AU -> ~1190, 9.58 AU -> ~1835, 19.2 AU -> ~2960, 30.05 AU -> ~4020
            return 65.0 + 210.0 * std::log1p(au * 1.45) + 95.0 * std::pow(au, 0.78);
        } else {
            // True vast linear astronomical proportions (1 AU = 420 world units; 30 AU = 12,600 units)
            return 45.0 + au * 420.0;
        }
    }

    // Minimum Screen-Space Radius Clamping (ensures bodies are never lost when zooming far out)
    static constexpr float MIN_MOON_SCREEN_RADIUS   = 3.0f;  // 3px for moons & asteroids
    static constexpr float MIN_PLANET_SCREEN_RADIUS = 6.0f;  // 6px for rocky planets & gas giants
    static constexpr float MIN_STAR_SCREEN_RADIUS   = 16.0f; // 16px for stars & black holes

    // Physics defaults
    static constexpr double DEFAULT_G = 100.0;
    static constexpr double MIN_G = 0.01;
    static constexpr double MAX_G = 10000.0;

    static constexpr double DEFAULT_SOFTENING = 2.0;
    static constexpr double DEFAULT_BARNES_HUT_THETA = 0.5;
    static constexpr int BARNES_HUT_MAX_DEPTH = 24;

    // Astrophysical mass transition thresholds (in simulation mass units)
    static constexpr double PROTOPLANET_MASS_LIMIT  = 1.2;
    static constexpr double GAS_GIANT_MASS_LIMIT    = 35.0;
    static constexpr double STELLAR_IGNITION_LIMIT  = 1800.0;
    static constexpr double CHANDRASEKHAR_LIMIT     = 14400.0;
    static constexpr double TOV_BLACK_HOLE_LIMIT    = 24000.0;

    // Time stepping
    static constexpr double BASE_DT = 1.0 / 60.0;
    static constexpr double MIN_TIME_SCALE = 0.05;
    static constexpr double MAX_TIME_SCALE = 5000.0;
    static constexpr int MAX_SUBSTEPS = 32;

    // Collision, fragmentation & particle caps (pre-allocated pools)
    static constexpr int MAX_DEBRIS_CAP = 1500;
    static constexpr size_t MAX_IMPACT_SPARKS = 2400;
    static constexpr size_t MAX_NEBULA_PARTICLES = 600;
    static constexpr size_t MAX_SHOCKWAVES = 64;
    static constexpr double DEFAULT_RESTITUTION = 0.75;
    static constexpr double DEFAULT_FRAGMENTATION_THRESHOLD = 2.5;
    static constexpr double RADIUS_SCALE_FACTOR = 1.45;

    // Trail & prediction settings
    static constexpr size_t DEFAULT_TRAIL_CAPACITY = 650;
    static constexpr int DEFAULT_PREDICTION_STEPS = 520;
    static constexpr double DEFAULT_PREDICTION_DT = 0.05;
    static constexpr double PREDICTOR_ATTRACTOR_MIN_MASS = 5.0;

    // Energy graph history
    static constexpr size_t ENERGY_HISTORY_SIZE = 300;

    // Color Palette (True Pitch-Black Deep-Space Abyss)
    static constexpr Color BG_DEEP_SPACE        = { 2, 4, 8, 255 };
    static constexpr Color GRID_FLAT_COLOR      = { 0, 155, 235, 90 };
    static constexpr Color GRID_WELL_MID_COLOR  = { 190, 55, 255, 195 };
    static constexpr Color GRID_WELL_DEEP_COLOR = { 255, 195, 55, 250 };
    static constexpr Color QUADTREE_DEBUG_COLOR = { 40, 230, 110, 85 };
    static constexpr Color VELOCITY_VEC_COLOR   = { 0, 235, 255, 230 };
    static constexpr Color ACCEL_VEC_COLOR      = { 255, 65, 130, 230 };
    static constexpr Color PREDICTION_COLOR     = { 95, 255, 175, 225 };
    static constexpr Color IMPACT_MARKER_COLOR  = { 255, 55, 55, 255 };
};

} // namespace sim
