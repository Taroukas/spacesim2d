#pragma once

#include "core/Config.hpp"
#include "core/Vec2d.hpp"
#include "physics/CelestialBody.hpp"
#include "physics/Spacecraft.hpp"
#include <cstdint>
#include <random>
#include <raylib.h>
#include <vector>

namespace sim {

struct ShockwaveEvent {
    Vec2d position{ 0.0, 0.0 };
    float currentRadius = 2.0f;
    float maxRadius = 60.0f;
    float age = 0.0f;
    float duration = 0.65f;
    float flashRadius = 35.0f;
    Color color = WHITE;
    bool isSupernova = false;
};

struct ImpactSpark {
    Vec2d position{ 0.0, 0.0 };
    Vec2d velocity{ 0.0, 0.0 };
    float age = 0.0f;
    float maxLife = 1.0f;
    float size = 2.2f;
    Color color = ORANGE;
};

struct NebulaParticle {
    Vec2d position{ 0.0, 0.0 };
    Vec2d velocity{ 0.0, 0.0 };
    float age = 0.0f;
    float maxLife = 8.0f;
    float startRadius = 15.0f;
    float endRadius = 140.0f;
    Color color = PURPLE;
};

class CollisionSystem {
public:
    CollisionSystem();

    void resolveCollisions(std::vector<CelestialBody>& bodies,
                           Spacecraft& spacecraft,
                           std::vector<ShockwaveEvent>& shockwaves,
                           std::vector<ImpactSpark>& impactSparks,
                           CollisionMode mode,
                           double G,
                           double restitution,
                           double fragThreshold,
                           int maxDebrisCap,
                           uint32_t& nextBodyId,
                           int& selectedBodyId,
                           int& cameraFollowId);

    void processRocheLimitShredding(std::vector<CelestialBody>& bodies,
                                    std::vector<ShockwaveEvent>& shockwaves,
                                    std::vector<ImpactSpark>& impactSparks,
                                    double dt,
                                    double G,
                                    int maxDebrisCap,
                                    uint32_t& nextBodyId,
                                    int& selectedBodyId,
                                    int& cameraFollowId);

    void spawnImpactSparks(std::vector<ImpactSpark>& impactSparks,
                           const Vec2d& pos,
                           const Vec2d& baseVel,
                           double characteristicSpeed,
                           int count,
                           Color tint);

    void spawnRealisticImpactPlumes(std::vector<ImpactSpark>& impactSparks,
                                    std::vector<ShockwaveEvent>& shockwaves,
                                    const Vec2d& contactPoint,
                                    const Vec2d& impactNormal,
                                    const Vec2d& targetVel,
                                    const Vec2d& relImpactVel,
                                    double impactorRadius,
                                    Color impactorColor,
                                    Color targetColor,
                                    bool isCompactAbsorber);

private:
    std::mt19937 m_rng;
    std::vector<CelestialBody> m_spawnedFragments;

    static void checkAstrophysicalTransitions(CelestialBody& body,
                                              std::vector<ShockwaveEvent>& shockwaves);

    void mergeBodies(CelestialBody& primary,
                     CelestialBody& secondary,
                     std::vector<ShockwaveEvent>& shockwaves,
                     std::vector<ImpactSpark>& impactSparks,
                     int& selectedBodyId,
                     int& cameraFollowId);

    void bounceBodies(CelestialBody& a,
                      CelestialBody& b,
                      double restitution,
                      std::vector<ShockwaveEvent>& shockwaves,
                      std::vector<ImpactSpark>& impactSparks);

    void hybridCollision(CelestialBody& a,
                         CelestialBody& b,
                         double G,
                         double fragThreshold,
                         int currentTotalBodies,
                         int maxDebrisCap,
                         uint32_t& nextBodyId,
                         std::vector<ShockwaveEvent>& shockwaves,
                         std::vector<ImpactSpark>& impactSparks,
                         int& selectedBodyId,
                         int& cameraFollowId);

    static Color blendColorsWeighted(Color c1, double m1, Color c2, double m2);
};

} // namespace sim
