#pragma once

#include "core/Config.hpp"
#include "core/Vec2d.hpp"
#include "physics/CelestialBody.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace sim {

enum class SASMode : int {
    Off = 0,
    Prograde = 1,
    Retrograde = 2
};

struct OrbitalTelemetry {
    uint32_t dominantBodyId = 0;
    std::string dominantBodyName = "None";
    double distanceToAttractor = 0.0;
    double relativeSpeed = 0.0;
    double specificMechanicalEnergy = 0.0;
    double semiMajorAxis = 0.0;
    Vec2d eccentricityVector{ 0.0, 0.0 };
    double eccentricity = 0.0;
    double apoapsis = 0.0;
    double periapsis = 0.0;
    double orbitalPeriod = 0.0;
    bool isBoundOrbit = false;
};

struct ExhaustParticle {
    Vec2d position;
    Vec2d velocity;
    float age = 0.0f;
    float maxLife = 0.35f;
    float size = 2.5f;
    Color color = SKYBLUE;
    bool isRcsPuff = false;
};

class Spacecraft {
public:
    Spacecraft() {
        exhaustParticles.reserve(512);
    }

    bool enabled = false;

    Vec2d position{ 0.0, 0.0 };
    Vec2d velocity{ 0.0, 0.0 };
    Vec2d acceleration{ 0.0, 0.0 };
    Vec2d prevAcceleration{ 0.0, 0.0 };

    double rotationAngle = 0.0; // Radians
    double angularVelocity = 0.0;
    double radius = 3.2;        // Nimble physical collision footprint

    double dryMass = 1.0;
    double fuelMass = 100.0;
    double maxFuel = 100.0;

    double mainThrustForce = 280.0;
    double retroThrustForce = 140.0;
    double rcsTorque = 5.5;
    double throttle = 1.0;
    bool infiniteFuel = true;

    bool firingMain = false;
    bool firingRetro = false;
    bool rotatingCCW = false;
    bool rotatingCW = false;
    SASMode sasMode = SASMode::Off;

    OrbitalTelemetry telemetry;
    std::deque<Vec2d> trail;
    std::vector<ExhaustParticle> exhaustParticles;

    double getTotalMass() const {
        return dryMass + (infiniteFuel ? maxFuel : std::max(0.0, fuelMass));
    }

    Vec2d getForwardHeading() const {
        return Vec2d(std::cos(rotationAngle), std::sin(rotationAngle));
    }

    void reset(const Vec2d& pos, const Vec2d& vel, double angle = 0.0) {
        enabled = true;
        position = pos;
        velocity = vel;
        acceleration = { 0.0, 0.0 };
        prevAcceleration = { 0.0, 0.0 };
        rotationAngle = angle;
        angularVelocity = 0.0;
        fuelMass = maxFuel;
        firingMain = false;
        firingRetro = false;
        rotatingCCW = false;
        rotatingCW = false;
        sasMode = SASMode::Prograde;
        trail.clear();
        exhaustParticles.clear();
    }

    void handleInput(double dt, bool uiCapturesKeyboard) {
        firingMain = false;
        firingRetro = false;
        rotatingCCW = false;
        rotatingCW = false;

        if (!enabled || uiCapturesKeyboard) {
            return;
        }

        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            throttle = std::clamp(throttle + 0.65 * dt, 0.0, 1.0);
        }
        if (IsKeyDown(KEY_LEFT_CONTROL)) {
            throttle = std::clamp(throttle - 0.65 * dt, 0.0, 1.0);
        }

        if (IsKeyPressed(KEY_R)) {
            if (sasMode == SASMode::Off) {
                sasMode = SASMode::Prograde;
            } else if (sasMode == SASMode::Prograde) {
                sasMode = SASMode::Retrograde;
            } else {
                sasMode = SASMode::Off;
            }
        }

        if (IsKeyDown(KEY_A)) {
            rotatingCCW = true;
            sasMode = SASMode::Off;
        }
        if (IsKeyDown(KEY_D)) {
            rotatingCW = true;
            sasMode = SASMode::Off;
        }

        bool hasFuel = infiniteFuel || (fuelMass > 0.0);
        if (hasFuel && throttle > 0.001) {
            if (IsKeyDown(KEY_W)) {
                firingMain = true;
            } else if (IsKeyDown(KEY_S)) {
                firingRetro = true;
            }
        }
    }

    Vec2d computeThrustAcceleration(double dt, const std::vector<CelestialBody>& bodies) {
        if (!enabled) {
            return Vec2d(0.0, 0.0);
        }

        // Attitude control via SAS Autopilot or manual RCS
        if (sasMode != SASMode::Off) {
            Vec2d refVel = velocity;
            for (const auto& b : bodies) {
                if (b.alive && b.id == telemetry.dominantBodyId) {
                    refVel = velocity - b.velocity;
                    break;
                }
            }
            if (refVel.lengthSq() > 1e-4) {
                double targetAngle = std::atan2(refVel.y, refVel.x);
                if (sasMode == SASMode::Retrograde) {
                    targetAngle += M_PI;
                }
                double diff = std::remainder(targetAngle - rotationAngle, 2.0 * M_PI);
                angularVelocity = std::clamp(diff * 6.5, -5.0, 5.0);
            }
        } else {
            double angularAccel = 0.0;
            if (rotatingCCW) angularAccel -= rcsTorque;
            if (rotatingCW)  angularAccel += rcsTorque;
            angularVelocity += angularAccel * dt;
            angularVelocity *= std::pow(0.18, dt); // Crisp SAS rotational damping
        }

        rotationAngle += angularVelocity * dt;
        rotationAngle = std::remainder(rotationAngle, 2.0 * M_PI);

        // Linear thrust force
        Vec2d thrustAcc{ 0.0, 0.0 };
        bool hasFuel = infiniteFuel || (fuelMass > 0.0);
        if (hasFuel && (firingMain || firingRetro)) {
            double forceMag = (firingMain ? mainThrustForce : -retroThrustForce) * throttle;
            double totalM = getTotalMass();
            thrustAcc = getForwardHeading() * (forceMag / totalM);

            if (!infiniteFuel) {
                double burnRate = (firingMain ? 5.5 : 2.8) * throttle;
                fuelMass = std::max(0.0, fuelMass - burnRate * dt);
            }
        }

        return thrustAcc;
    }

    void matchTargetVelocity(const std::vector<CelestialBody>& bodies, int selectedBodyId) {
        if (!enabled) return;
        const CelestialBody* target = nullptr;
        for (const auto& b : bodies) {
            if (b.alive && static_cast<int>(b.id) == selectedBodyId) {
                target = &b;
                break;
            }
        }
        if (!target) {
            for (const auto& b : bodies) {
                if (b.alive && b.id == telemetry.dominantBodyId) {
                    target = &b;
                    break;
                }
            }
        }
        if (target) {
            velocity = target->velocity;
            trail.clear();
        }
    }

    void autoCircularizeOrbit(const std::vector<CelestialBody>& bodies, double G, int preferredBodyId = -1) {
        if (!enabled || bodies.empty()) return;
        const CelestialBody* attr = nullptr;

        if (preferredBodyId >= 0) {
            for (const auto& b : bodies) {
                if (b.alive && static_cast<int>(b.id) == preferredBodyId) {
                    attr = &b;
                    break;
                }
            }
        }
        if (!attr) {
            for (const auto& b : bodies) {
                if (b.alive && b.id == telemetry.dominantBodyId) {
                    attr = &b;
                    break;
                }
            }
        }
        if (!attr) return;

        Vec2d rVec = position - attr->position;
        double r = rVec.length();
        if (r < attr->radius + 6.0) {
            Vec2d dir = (r > 1e-4) ? (rVec / r) : Vec2d(1.0, 0.0);
            r = attr->radius + 20.0;
            position = attr->position + dir * r;
            rVec = position - attr->position;
        }

        Vec2d radial = rVec.normalized();
        Vec2d tangent = radial.perpendicular();
        // Preserve current orbital handedness if moving
        Vec2d relVel = velocity - attr->velocity;
        if (relVel.lengthSq() > 1.0 && relVel.dot(tangent) < 0.0) {
            tangent = -tangent;
        }

        double vCirc = std::sqrt((G * (attr->mass + getTotalMass())) / r);
        velocity = attr->velocity + tangent * vCirc;
        rotationAngle = std::atan2(tangent.y, tangent.x);
        angularVelocity = 0.0;
        sasMode = SASMode::Prograde;
        trail.clear();
    }

    void updateParticlesAndTrail(double dt, size_t maxTrailCapacity) {
        if (!enabled) return;

        for (auto& p : exhaustParticles) {
            p.position += p.velocity * dt;
            p.age += static_cast<float>(dt);
        }
        exhaustParticles.erase(
            std::remove_if(exhaustParticles.begin(), exhaustParticles.end(),
                [](const ExhaustParticle& p) { return p.age >= p.maxLife; }),
            exhaustParticles.end());

        Vec2d heading = getForwardHeading();
        Vec2d right = heading.perpendicular();

        // Spawn twin-nacelle plasma plume particles when main engine fires
        if (firingMain && exhaustParticles.size() + 6 < 512) {
            for (int side = -1; side <= 1; side += 2) {
                Vec2d nacelleOffset = -heading * (radius * 0.9) + right * (side * radius * 0.36);
                for (int i = 0; i < 2; ++i) {
                    double jitter = (static_cast<double>(GetRandomValue(-100, 100)) / 100.0) * 0.16;
                    double speed = (95.0 + GetRandomValue(-15, 25)) * (0.6 + 0.4 * throttle);
                    Vec2d ejectDir = (-heading + right * jitter).normalized();

                    ExhaustParticle ep;
                    ep.position = position + nacelleOffset;
                    ep.velocity = velocity + ejectDir * speed;
                    ep.age = 0.0f;
                    ep.maxLife = 0.26f;
                    ep.size = static_cast<float>(1.8 + 1.6 * throttle);
                    ep.color = (i == 0) ? Color{ 210, 245, 255, 255 } : Color{ 65, 165, 255, 255 };
                    ep.isRcsPuff = false;
                    exhaustParticles.push_back(ep);
                }
            }
        }

        // Spawn white RCS gas puffs when braking or rotating
        if ((firingRetro || rotatingCCW || rotatingCW) && exhaustParticles.size() + 4 < 512) {
            Vec2d rcsOrigin = position + heading * (radius * 0.85);
            Vec2d rcsDir = heading;
            if (rotatingCCW) {
                rcsOrigin = position + heading * (radius * 0.5) + right * (radius * 0.65);
                rcsDir = right;
            } else if (rotatingCW) {
                rcsOrigin = position + heading * (radius * 0.5) - right * (radius * 0.65);
                rcsDir = -right;
            }

            ExhaustParticle rcs;
            rcs.position = rcsOrigin;
            rcs.velocity = velocity + rcsDir * 45.0;
            rcs.age = 0.0f;
            rcs.maxLife = 0.16f;
            rcs.size = 1.6f;
            rcs.color = Color{ 230, 245, 255, 215 };
            rcs.isRcsPuff = true;
            exhaustParticles.push_back(rcs);
        }

        if (maxTrailCapacity > 0) {
            if (trail.empty() || (position - trail.back()).lengthSq() > 0.35) {
                trail.push_back(position);
                while (trail.size() > maxTrailCapacity) {
                    trail.pop_front();
                }
            }
        } else {
            trail.clear();
        }
    }

    void computeTelemetry(const std::vector<CelestialBody>& bodies, double G) {
        telemetry = OrbitalTelemetry{};
        if (!enabled || bodies.empty()) return;

        double maxPull = -1.0;
        const CelestialBody* dom = nullptr;

        for (const auto& b : bodies) {
            if (!b.alive) continue;
            double distSq = std::max(1.0, (b.position - position).lengthSq());
            double pull = (G * b.mass) / distSq;
            if (pull > maxPull) {
                maxPull = pull;
                dom = &b;
            }
        }

        if (!dom) return;

        telemetry.dominantBodyId = dom->id;
        telemetry.dominantBodyName = dom->name;

        Vec2d rVec = position - dom->position;
        Vec2d vVec = velocity - dom->velocity;

        double r = std::max(1e-6, rVec.length());
        double vSq = vVec.lengthSq();
        double v = std::sqrt(vSq);
        double mu = G * (dom->mass + getTotalMass());

        telemetry.distanceToAttractor = r;
        telemetry.relativeSpeed = v;
        telemetry.specificMechanicalEnergy = 0.5 * vSq - (mu / r);

        double h = rVec.cross(vVec);
        Vec2d vCrossH(vVec.y * h, -vVec.x * h);
        telemetry.eccentricityVector = (vCrossH / std::max(1e-9, mu)) - (rVec / r);
        telemetry.eccentricity = telemetry.eccentricityVector.length();

        if (std::abs(telemetry.specificMechanicalEnergy) > 1e-9) {
            telemetry.semiMajorAxis = -mu / (2.0 * telemetry.specificMechanicalEnergy);
        }

        if (telemetry.specificMechanicalEnergy < 0.0 && telemetry.eccentricity < 1.0 && telemetry.semiMajorAxis > 0.0) {
            telemetry.isBoundOrbit = true;
            telemetry.apoapsis = telemetry.semiMajorAxis * (1.0 + telemetry.eccentricity);
            telemetry.periapsis = telemetry.semiMajorAxis * (1.0 - telemetry.eccentricity);
            telemetry.orbitalPeriod = 2.0 * M_PI * std::sqrt(
                (telemetry.semiMajorAxis * telemetry.semiMajorAxis * telemetry.semiMajorAxis) / std::max(1e-9, mu));
        } else {
            telemetry.isBoundOrbit = false;
            telemetry.periapsis = (h * h / std::max(1e-9, mu)) / (1.0 + telemetry.eccentricity);
            telemetry.apoapsis = -1.0;
            telemetry.orbitalPeriod = 0.0;
        }
    }
};

} // namespace sim
