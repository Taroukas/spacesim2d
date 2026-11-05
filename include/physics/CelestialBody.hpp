#pragma once

#include "core/Config.hpp"
#include "core/Vec2d.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>

namespace sim {

struct CelestialBody {
    uint32_t id = 0;
    std::string name = "Body";
    CelestialBodyType bodyType = CelestialBodyType::RockyPlanet;
    uint32_t textureSeed = 101u;

    Vec2d position{ 0.0, 0.0 };
    Vec2d velocity{ 0.0, 0.0 };
    Vec2d acceleration{ 0.0, 0.0 };
    Vec2d prevAcceleration{ 0.0, 0.0 };

    double mass = 10.0;
    double radius = 6.0;
    double density = 1.0;

    // Thermal, rotational & localized impact crater properties (ported from SpaceSim2D)
    double temperature = 288.0; // Kelvin (above 900K glows with molten lava; below 200K freezes)
    double rotationAngle = 0.0;
    double angularVelocity = 0.35;
    double impactHeat = 0.0;      // [0.0, 1.0] localized post-impact magma crater intensity
    double lastImpactAngle = 0.0; // Contact normal angle (radians) of most recent collision
    double semiMajorAxisAU = 0.0; // Astronomical semi-major axis (AU) for live cosmic scale toggling

    // Planetary ring system (Saturn / Gas Giants / Accretion rings)
    bool hasRings = false;
    Color ringColor = { 225, 200, 155, 175 };
    double ringInnerRadiusRatio = 1.45;
    double ringOuterRadiusRatio = 2.40;

    // Roche limit tidal shredding accumulator
    double rocheDamageTimer = 0.0;

    Color color = WHITE;
    bool isPinned = false;
    bool isStar = false;
    bool alive = true;

    std::deque<Vec2d> trail;
    std::deque<Vec2d> relativeTrail;

    bool isBlackHole() const {
        return bodyType == CelestialBodyType::BlackHole;
    }

    bool isLuminousStar() const {
        return isStar ||
               bodyType == CelestialBodyType::YellowStar ||
               bodyType == CelestialBodyType::BlueHypergiant ||
               bodyType == CelestialBodyType::NeutronStar;
    }

    bool hasAtmosphere() const {
        return bodyType == CelestialBodyType::RockyPlanet ||
               bodyType == CelestialBodyType::GasGiant;
    }

    float getMinScreenRadius() const {
        if (isBlackHole() || isLuminousStar()) {
            return Config::MIN_STAR_SCREEN_RADIUS;   // 16px minimum for stars & black holes
        }
        if (bodyType == CelestialBodyType::RockyPlanet || bodyType == CelestialBodyType::GasGiant) {
            return Config::MIN_PLANET_SCREEN_RADIUS; // 6px minimum for planets
        }
        return Config::MIN_MOON_SCREEN_RADIUS;       // 3px minimum for moons & asteroids
    }

    const char* getTypeBadgeName() const {
        switch (bodyType) {
            case CelestialBodyType::RockyPlanet:    return "TERRAN PLANET";
            case CelestialBodyType::CrateredMoon:   return "CRATERED WORLD";
            case CelestialBodyType::GasGiant:       return "GAS GIANT";
            case CelestialBodyType::YellowStar:     return "MAIN-SEQUENCE STAR";
            case CelestialBodyType::BlueHypergiant: return "BLUE HYPERGIANT";
            case CelestialBodyType::BlackHole:      return "SUPERMASSIVE BLACK HOLE";
            case CelestialBodyType::NeutronStar:    return "NEUTRON PULSAR";
            case CelestialBodyType::Asteroid:       return "ROCKY ASTEROID";
            default:                                return "CELESTIAL BODY";
        }
    }

    void recomputeRadiusFromMass() {
        if (bodyType == CelestialBodyType::BlackHole) {
            // Compact event horizon radius
            radius = std::clamp(1.2 * std::cbrt(std::max(1.0, mass / 80.0)), 6.0, 32.0);
            return;
        }
        if (bodyType == CelestialBodyType::NeutronStar) {
            radius = std::clamp(0.8 * std::cbrt(std::max(1.0, mass / 120.0)), 4.5, 10.0);
            return;
        }
        double safeDensity = std::max(0.1, density);
        double effectiveMass = std::max(0.01, mass / safeDensity);
        radius = std::max(1.8, Config::RADIUS_SCALE_FACTOR * std::cbrt(effectiveMass));
    }

    void recordTrailPoint(const Vec2d& referencePos, bool hasReference, size_t maxTrailCapacity) {
        if (maxTrailCapacity == 0) {
            trail.clear();
            relativeTrail.clear();
            return;
        }

        if (trail.empty() || (position - trail.back()).lengthSq() > 0.35) {
            trail.push_back(position);
            while (trail.size() > maxTrailCapacity) {
                trail.pop_front();
            }

            if (hasReference) {
                relativeTrail.push_back(position - referencePos);
                while (relativeTrail.size() > maxTrailCapacity) {
                    relativeTrail.pop_front();
                }
            }
        }
    }

    void clearTrails() {
        trail.clear();
        relativeTrail.clear();
    }
};

} // namespace sim
