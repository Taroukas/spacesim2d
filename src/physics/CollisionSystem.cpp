#include "physics/CollisionSystem.hpp"
#include <algorithm>
#include <cmath>

namespace sim {

CollisionSystem::CollisionSystem() : m_rng(1337u) {
    m_spawnedFragments.reserve(160);
}

Color CollisionSystem::blendColorsWeighted(Color c1, double m1, Color c2, double m2) {
    double total = std::max(1e-9, m1 + m2);
    double w1 = m1 / total;
    double w2 = m2 / total;

    return Color{
        static_cast<unsigned char>(std::clamp(c1.r * w1 + c2.r * w2, 0.0, 255.0)),
        static_cast<unsigned char>(std::clamp(c1.g * w1 + c2.g * w2, 0.0, 255.0)),
        static_cast<unsigned char>(std::clamp(c1.b * w1 + c2.b * w2, 0.0, 255.0)),
        255
    };
}

void CollisionSystem::spawnImpactSparks(std::vector<ImpactSpark>& impactSparks,
                                        const Vec2d& pos,
                                        const Vec2d& baseVel,
                                        double characteristicSpeed,
                                        int count,
                                        Color tint) {
    std::uniform_real_distribution<double> angleDist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> speedMult(0.25, 1.65);
    std::uniform_real_distribution<float> lifeDist(0.45f, 1.45f);
    std::uniform_real_distribution<float> sizeDist(1.6f, 3.8f);
    std::uniform_int_distribution<int> palettePick(0, 3);

    const Color sparkPalette[4] = {
        { 255, 245, 200, 255 }, // White-hot core spark
        { 255, 195,  65, 255 }, // Molten gold
        { 255, 105,  30, 255 }, // Magma orange
        tint
    };

    double baseSpd = std::clamp(characteristicSpeed, 25.0, 340.0);

    for (int i = 0; i < count; ++i) {
        if (impactSparks.size() >= Config::MAX_IMPACT_SPARKS) {
            break;
        }
        double a = angleDist(m_rng);
        double s = baseSpd * speedMult(m_rng);
        Vec2d dir(std::cos(a), std::sin(a));

        ImpactSpark sp;
        sp.position = pos + dir * 2.0;
        sp.velocity = baseVel + dir * s;
        sp.age = 0.0f;
        sp.maxLife = lifeDist(m_rng);
        sp.size = sizeDist(m_rng);
        sp.color = sparkPalette[palettePick(m_rng)];
        impactSparks.push_back(sp);
    }
}

void CollisionSystem::spawnRealisticImpactPlumes(std::vector<ImpactSpark>& impactSparks,
                                                 std::vector<ShockwaveEvent>& shockwaves,
                                                 const Vec2d& contactPoint,
                                                 const Vec2d& impactNormal,
                                                 const Vec2d& targetVel,
                                                 const Vec2d& relImpactVel,
                                                 double impactorRadius,
                                                 Color impactorColor,
                                                 Color targetColor,
                                                 bool isCompactAbsorber) {
    double impactSpeed = relImpactVel.length();
    double normalAngle = std::atan2(impactNormal.y, impactNormal.x);
    double tangentAngle = normalAngle + M_PI * 0.5;

    double speedFactor = std::clamp(impactSpeed / 120.0, 0.9, 2.6);
    double blastScale = std::max(impactorRadius, 1.5) * speedFactor;

    // 1. Double Taylor-Sedov expanding shockwaves & blinding impact flash (ported from SpaceSim2D)
    if (shockwaves.size() < Config::MAX_SHOCKWAVES) {
        ShockwaveEvent swPrimary;
        swPrimary.position = contactPoint;
        swPrimary.currentRadius = static_cast<float>(impactorRadius * 0.35);
        swPrimary.maxRadius = static_cast<float>(std::clamp(blastScale * 5.2 + 16.0, 22.0, 920.0));
        swPrimary.flashRadius = static_cast<float>(std::clamp(blastScale * 2.4 + 10.0, 14.0, 420.0));
        swPrimary.age = 0.0f;
        swPrimary.duration = 0.72f;
        swPrimary.color = Color{ 255, 210, 85, 255 };
        swPrimary.isSupernova = false;
        shockwaves.push_back(swPrimary);
    }
    if (shockwaves.size() < Config::MAX_SHOCKWAVES) {
        ShockwaveEvent swSecondary;
        swSecondary.position = contactPoint;
        swSecondary.currentRadius = static_cast<float>(impactorRadius * 0.20);
        swSecondary.maxRadius = static_cast<float>(std::clamp(blastScale * 3.2 + 10.0, 16.0, 580.0));
        swSecondary.flashRadius = static_cast<float>(std::clamp(blastScale * 1.5 + 8.0, 10.0, 260.0));
        swSecondary.age = 0.0f;
        swSecondary.duration = 0.48f;
        swSecondary.color = Color{ 255, 115, 35, 255 };
        swSecondary.isSupernova = false;
        shockwaves.push_back(swSecondary);
    }

    // 2. Seven directional ejecta plumes & lateral molten matter jets
    constexpr int NUM_PLUMES = 7;
    double plumeAngles[NUM_PLUMES];
    double plumeSpeeds[NUM_PLUMES];
    std::uniform_real_distribution<double> unitDist(0.0, 1.0);
    auto randRange = [&](double lo, double hi) {
        return lo + unitDist(m_rng) * (hi - lo);
    };

    for (int p = 0; p < NUM_PLUMES; ++p) {
        double frac = (static_cast<double>(p) / (NUM_PLUMES - 1)) - 0.5;
        plumeAngles[p] = normalAngle + frac * 2.4 + randRange(-0.12, 0.12);
        plumeSpeeds[p] = randRange(0.75, 1.35);
    }

    int fireballCount = std::clamp(static_cast<int>(65 + blastScale * 4.2), 65, 195);
    float minDebrisSize = static_cast<float>(std::clamp(impactorRadius * 0.22, 1.4, 3.2));
    float maxDebrisSize = static_cast<float>(std::clamp(impactorRadius * 0.45, 2.2, 5.8));

    double minSpeed = std::max(28.0, impactorRadius * 1.8);
    double maxSpeed = std::max(380.0, impactorRadius * 7.5);

    for (int i = 0; i < fireballCount && impactSparks.size() < Config::MAX_IMPACT_SPARKS; ++i) {
        double ejectAngle = normalAngle;
        double speedMult = 1.0;
        float sizeMult = 1.0f;
        float life = 1.0f;

        if (i % 4 == 0) {
            double sideSign = (i % 8 == 0) ? 1.0 : -1.0;
            ejectAngle = normalAngle + sideSign * (M_PI * 0.46) + randRange(-0.20, 0.20);
            speedMult = randRange(0.85, 1.45);
            sizeMult = static_cast<float>(randRange(0.85, 1.35));
            life = static_cast<float>(randRange(0.55, 1.45));
        } else if (i % 3 == 0) {
            int pIdx = i % NUM_PLUMES;
            double streamFrac = randRange(0.25, 1.0);
            ejectAngle = plumeAngles[pIdx] + randRange(-0.06, 0.06) * (1.2 - streamFrac);
            speedMult = plumeSpeeds[pIdx] * streamFrac * 1.25;
            sizeMult = static_cast<float>(0.70 + 0.55 * (1.0 - streamFrac));
            life = static_cast<float>(randRange(0.60, 1.55));
        } else {
            ejectAngle = normalAngle + randRange(-1.35, 1.35);
            speedMult = randRange(0.35, 1.15);
            sizeMult = static_cast<float>(randRange(0.75, 1.30));
            life = static_cast<float>(randRange(0.50, 1.35));
        }

        double baseSpeed = std::clamp(
            (impactSpeed * randRange(0.28, 0.95) + impactorRadius * randRange(1.4, 3.8)) * speedMult,
            minSpeed, maxSpeed
        );

        double lateralSpread = randRange(-0.35, 0.35) * impactorRadius;
        Vec2d spawnOffset(
            std::cos(ejectAngle) * impactorRadius * randRange(0.05, 0.30) + std::cos(tangentAngle) * lateralSpread,
            std::sin(ejectAngle) * impactorRadius * randRange(0.05, 0.30) + std::sin(tangentAngle) * lateralSpread
        );

        Color col;
        if (i % 6 == 0) col = RAYWHITE;
        else if (i % 4 == 0) col = Color{ 255, 225, 105, 255 };
        else if (i % 3 == 0) col = impactorColor;
        else if (i % 2 == 0) col = Color{ 255, 105, 25, 255 };
        else col = targetColor;

        ImpactSpark sp;
        sp.position = contactPoint + spawnOffset;
        sp.velocity = targetVel + Vec2d(std::cos(ejectAngle) * baseSpeed, std::sin(ejectAngle) * baseSpeed);
        sp.age = 0.0f;
        sp.maxLife = life;
        sp.size = static_cast<float>(randRange(minDebrisSize, maxDebrisSize)) * sizeMult;
        sp.color = col;
        impactSparks.push_back(sp);
    }

    // 3. Relativistic bipolar astrophysical jets if absorbed by a Black Hole or Neutron Star
    if (isCompactAbsorber) {
        double jetAngle = normalAngle + M_PI * 0.5;
        for (int i = 0; i < 48 && impactSparks.size() < Config::MAX_IMPACT_SPARKS; ++i) {
            double dirSign = (i % 2 == 0) ? 1.0 : -1.0;
            double a = jetAngle + dirSign * (M_PI * 0.5) + randRange(-0.06, 0.06);
            double jetSpeed = randRange(210.0, 520.0);

            ImpactSpark jet;
            jet.position = contactPoint;
            jet.velocity = targetVel + Vec2d(std::cos(a) * jetSpeed, std::sin(a) * jetSpeed);
            jet.age = 0.0f;
            jet.maxLife = static_cast<float>(randRange(0.40, 0.95));
            jet.size = 2.6f;
            jet.color = (i % 2 == 0) ? Color{ 110, 225, 255, 255 } : Color{ 195, 110, 255, 255 };
            impactSparks.push_back(jet);
        }
    }
}

void CollisionSystem::checkAstrophysicalTransitions(CelestialBody& body,
                                                    std::vector<ShockwaveEvent>& shockwaves) {
    // Natural mass-accretion evolution ported from SpaceSim2D:
    // Asteroid -> CrateredMoon/RockyPlanet -> GasGiant -> Main-Sequence Star -> NeutronStar -> BlackHole
    if (body.bodyType == CelestialBodyType::Asteroid && body.mass >= Config::PROTOPLANET_MASS_LIMIT) {
        body.bodyType = CelestialBodyType::RockyPlanet;
        body.name = "Protoplanet-" + std::to_string(body.id);
    }
    if ((body.bodyType == CelestialBodyType::RockyPlanet || body.bodyType == CelestialBodyType::CrateredMoon) &&
        body.mass >= Config::GAS_GIANT_MASS_LIMIT) {
        body.bodyType = CelestialBodyType::GasGiant;
    }
    if ((body.bodyType == CelestialBodyType::RockyPlanet ||
         body.bodyType == CelestialBodyType::CrateredMoon ||
         body.bodyType == CelestialBodyType::GasGiant) &&
        body.mass >= Config::STELLAR_IGNITION_LIMIT) {
        body.bodyType = CelestialBodyType::YellowStar;
        body.isStar = true;
        body.hasRings = false;
        body.color = Color{ 255, 195, 80, 255 };
        body.temperature = std::max(body.temperature, 4200.0);
    }
    if (body.bodyType == CelestialBodyType::YellowStar && body.mass >= Config::CHANDRASEKHAR_LIMIT) {
        body.bodyType = CelestialBodyType::BlueHypergiant;
        body.isStar = true;
        body.color = Color{ 120, 210, 255, 255 };
        body.temperature = std::max(body.temperature, 14000.0);
    }
    if ((body.bodyType == CelestialBodyType::NeutronStar || body.bodyType == CelestialBodyType::BlueHypergiant) &&
        body.mass >= Config::TOV_BLACK_HOLE_LIMIT) {
        body.bodyType = CelestialBodyType::BlackHole;
        body.isStar = false;
        body.hasRings = false;
        body.color = Color{ 255, 160, 65, 255 };
        body.recomputeRadiusFromMass();
        if (shockwaves.size() < Config::MAX_SHOCKWAVES) {
            ShockwaveEvent sw;
            sw.position = body.position;
            sw.currentRadius = static_cast<float>(body.radius);
            sw.maxRadius = static_cast<float>(body.radius * 18.0 + 180.0);
            sw.flashRadius = static_cast<float>(body.radius * 9.0 + 90.0);
            sw.age = 0.0f;
            sw.duration = 1.4f;
            sw.color = Color{ 185, 115, 255, 255 };
            sw.isSupernova = true;
            shockwaves.push_back(sw);
        }
    }
}

void CollisionSystem::resolveCollisions(std::vector<CelestialBody>& bodies,
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
                                        int& cameraFollowId) {
    m_spawnedFragments.clear();
    const size_t n = bodies.size();
    int currentAliveCount = static_cast<int>(n);

    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive) continue;

        for (size_t j = i + 1; j < n; ++j) {
            if (!bodies[j].alive) continue;

            Vec2d delta = bodies[j].position - bodies[i].position;
            double distSq = delta.lengthSq();
            double sumRadii = bodies[i].radius + bodies[j].radius;

            if (distSq <= sumRadii * sumRadii) {
                if (mode == CollisionMode::InelasticMerge || bodies[i].isBlackHole() || bodies[j].isBlackHole()) {
                    bool iPrimary = (bodies[i].isBlackHole() && !bodies[j].isBlackHole()) ||
                                    (!bodies[j].isBlackHole() && (bodies[i].mass >= bodies[j].mass || bodies[i].isPinned));
                    if (iPrimary) {
                        mergeBodies(bodies[i], bodies[j], shockwaves, impactSparks, selectedBodyId, cameraFollowId);
                    } else {
                        mergeBodies(bodies[j], bodies[i], shockwaves, impactSparks, selectedBodyId, cameraFollowId);
                        break;
                    }
                } else if (mode == CollisionMode::ElasticBounce) {
                    bounceBodies(bodies[i], bodies[j], restitution, shockwaves, impactSparks);
                } else {
                    hybridCollision(bodies[i], bodies[j], G, fragThreshold,
                                    currentAliveCount + static_cast<int>(m_spawnedFragments.size()),
                                    maxDebrisCap, nextBodyId, shockwaves, impactSparks, selectedBodyId, cameraFollowId);
                    if (!bodies[i].alive) {
                        break;
                    }
                }
            }
        }
    }

    // Check spacecraft collisions with celestial bodies
    if (spacecraft.enabled) {
        for (const auto& b : bodies) {
            if (!b.alive) continue;
            Vec2d delta = spacecraft.position - b.position;
            double minDist = b.radius + spacecraft.radius;
            if (delta.lengthSq() < minDist * minDist) {
                Vec2d normal = delta.normalized();
                if (normal.lengthSq() < 1e-6) normal = Vec2d(1.0, 0.0);
                spacecraft.position = b.position + normal * (minDist + 0.5);

                Vec2d relVel = spacecraft.velocity - b.velocity;
                double vn = relVel.dot(normal);
                if (vn < 0.0) {
                    spacecraft.velocity -= normal * (1.5 * vn);
                }
            }
        }
    }

    bodies.erase(
        std::remove_if(bodies.begin(), bodies.end(),
            [](const CelestialBody& b) { return !b.alive; }),
        bodies.end());

    if (!m_spawnedFragments.empty()) {
        bodies.insert(bodies.end(), m_spawnedFragments.begin(), m_spawnedFragments.end());
    }
}

void CollisionSystem::mergeBodies(CelestialBody& primary,
                                  CelestialBody& secondary,
                                  std::vector<ShockwaveEvent>& shockwaves,
                                  std::vector<ImpactSpark>& impactSparks,
                                  int& selectedBodyId,
                                  int& cameraFollowId) {
    double m1 = std::max(1e-6, primary.mass);
    double m2 = std::max(1e-6, secondary.mass);
    double totalMass = m1 + m2;

    Vec2d relVel = secondary.velocity - primary.velocity;
    double relSpeed = relVel.length();

    Vec2d contactNormal = (secondary.position - primary.position).normalized();
    if (contactNormal.lengthSq() < 1e-6) {
        contactNormal = Vec2d(1.0, 0.0);
    }
    Vec2d contactPoint = primary.position + contactNormal * primary.radius;

    // Conserve total linear momentum: v_new = (m1 * v1 + m2 * v2) / (m1 + m2)
    if (!primary.isPinned && !secondary.isPinned) {
        primary.position = (primary.position * m1 + secondary.position * m2) / totalMass;
        primary.velocity = (primary.velocity * m1 + secondary.velocity * m2) / totalMass;
    } else if (secondary.isPinned) {
        primary.isPinned = true;
        primary.position = secondary.position;
        primary.velocity = { 0.0, 0.0 };
    }

    // Recalculate new radius assuming constant density: R_new = R_1 * cbrt(1 + m_2 / m_1)
    if (!primary.isBlackHole() && primary.bodyType != CelestialBodyType::NeutronStar) {
        double constantDensityR = primary.radius * std::cbrt(1.0 + (m2 / m1));
        double volumeCombinedR = std::cbrt(primary.radius * primary.radius * primary.radius +
                                           secondary.radius * secondary.radius * secondary.radius);
        primary.radius = 0.5 * (constantDensityR + volumeCombinedR);
    } else {
        primary.mass = totalMass;
        primary.recomputeRadiusFromMass();
    }
    primary.mass = totalMass;

    // Localized impact crater scar angle & magma heating (ported from SpaceSim2D)
    primary.lastImpactAngle = std::atan2(contactNormal.y, contactNormal.x);
    double massRatio = std::clamp(m2 / m1, 0.02, 1.0);
    double heatSpike = std::clamp(massRatio * 3.0 + (relSpeed / 180.0) * 0.45, 0.28, 1.0);
    primary.impactHeat = std::min(1.0, primary.impactHeat + heatSpike);

    double deltaTemp = std::clamp(850.0 + relSpeed * 18.0 * std::sqrt(massRatio), 450.0, 5200.0);
    primary.temperature = std::min(12000.0, primary.temperature + deltaTemp);

    if (!primary.isLuminousStar() && !primary.isBlackHole()) {
        primary.color = blendColorsWeighted(primary.color, m1, secondary.color, m2);
    }

    checkAstrophysicalTransitions(primary, shockwaves);

    if (selectedBodyId == static_cast<int>(secondary.id)) {
        selectedBodyId = static_cast<int>(primary.id);
    }
    if (cameraFollowId == static_cast<int>(secondary.id)) {
        cameraFollowId = static_cast<int>(primary.id);
    }

    bool isCompact = primary.isBlackHole() || primary.bodyType == CelestialBodyType::NeutronStar;
    spawnRealisticImpactPlumes(impactSparks, shockwaves, contactPoint, contactNormal,
                               primary.velocity, relVel, secondary.radius,
                               secondary.color, primary.color, isCompact);

    secondary.alive = false;
}

void CollisionSystem::bounceBodies(CelestialBody& a,
                                   CelestialBody& b,
                                   double restitution,
                                   std::vector<ShockwaveEvent>& shockwaves,
                                   std::vector<ImpactSpark>& impactSparks) {
    Vec2d delta = b.position - a.position;
    double dist = delta.length();
    double minDist = a.radius + b.radius;

    Vec2d normal = (dist > 1e-8) ? (delta / dist) : Vec2d(1.0, 0.0);
    Vec2d tangent = normal.perpendicular();
    double overlap = minDist - dist;

    double invM1 = a.isPinned ? 0.0 : (1.0 / std::max(1e-6, a.mass));
    double invM2 = b.isPinned ? 0.0 : (1.0 / std::max(1e-6, b.mass));
    double invMassSum = invM1 + invM2;

    if (invMassSum <= 0.0) return;

    // 1. Instantly resolve positional overlap proportionally to inverse mass ratios to prevent sticking artifacts
    if (overlap > 0.0) {
        double slop = 1.005;
        a.position -= normal * (overlap * slop * (invM1 / invMassSum));
        b.position += normal * (overlap * slop * (invM2 / invMassSum));
    }

    Vec2d relVel = b.velocity - a.velocity;
    double velAlongNormal = relVel.dot(normal);

    if (velAlongNormal > 0.0) return;

    // 2. Elastic Restitution Impulse: J = -(1 + e) * v_rel_n / (1/m1 + 1/m2)
    double e = std::clamp(restitution, 0.0, 1.0);
    double jNormal = -(1.0 + e) * velAlongNormal / invMassSum;
    Vec2d normalImpulse = normal * jNormal;

    // 3. Tangential friction impulse for realistic oblique impact spin transfer
    double velAlongTangent = relVel.dot(tangent);
    double frictionCoeff = 0.18;
    double jTangent = std::clamp(-velAlongTangent / invMassSum, -frictionCoeff * jNormal, frictionCoeff * jNormal);
    Vec2d totalImpulse = normalImpulse + tangent * jTangent;

    if (!a.isPinned) a.velocity -= totalImpulse * invM1;
    if (!b.isPinned) b.velocity += totalImpulse * invM2;

    a.angularVelocity = std::clamp(a.angularVelocity - (jTangent * invM1) * 0.025, -8.0, 8.0);
    b.angularVelocity = std::clamp(b.angularVelocity + (jTangent * invM2) * 0.025, -8.0, 8.0);

    a.lastImpactAngle = std::atan2(normal.y, normal.x);
    b.lastImpactAngle = std::atan2(-normal.y, -normal.x);
    double impactIntensity = std::clamp(std::abs(velAlongNormal) / 140.0, 0.18, 0.85);
    a.impactHeat = std::min(1.0, a.impactHeat + impactIntensity);
    b.impactHeat = std::min(1.0, b.impactHeat + impactIntensity);

    a.temperature = std::min(9000.0, a.temperature + std::abs(velAlongNormal) * 6.0);
    b.temperature = std::min(9000.0, b.temperature + std::abs(velAlongNormal) * 6.0);

    Vec2d contactPt = a.position + normal * a.radius;
    spawnImpactSparks(impactSparks, contactPt, (a.velocity + b.velocity) * 0.5,
                      std::abs(velAlongNormal) * 0.8 + 30.0, 65, ORANGE);

    if (shockwaves.size() < Config::MAX_SHOCKWAVES && std::abs(velAlongNormal) > 5.0) {
        ShockwaveEvent sw;
        sw.position = contactPt;
        sw.currentRadius = 2.0f;
        sw.maxRadius = static_cast<float>(std::min(a.radius, b.radius) * 3.0 + 14.0);
        sw.flashRadius = static_cast<float>(std::min(a.radius, b.radius) * 1.8 + 8.0);
        sw.age = 0.0f;
        sw.duration = 0.45f;
        sw.color = Color{ 255, 220, 140, 255 };
        sw.isSupernova = false;
        shockwaves.push_back(sw);
    }
}

void CollisionSystem::hybridCollision(CelestialBody& a,
                                      CelestialBody& b,
                                      double G,
                                      double fragThreshold,
                                      int currentTotalBodies,
                                      int maxDebrisCap,
                                      uint32_t& nextBodyId,
                                      std::vector<ShockwaveEvent>& shockwaves,
                                      std::vector<ImpactSpark>& impactSparks,
                                      int& selectedBodyId,
                                      int& cameraFollowId) {
    CelestialBody* primary = (a.mass >= b.mass || a.isPinned) ? &a : &b;
    CelestialBody* secondary = (primary == &a) ? &b : &a;

    double m1 = primary->mass;
    double m2 = secondary->mass;
    double rSum = std::max(1.0, primary->radius + secondary->radius);

    double vEsc = std::sqrt((2.0 * G * (m1 + m2)) / rSum);
    double vRel = (primary->velocity - secondary->velocity).length();

    if (vRel < fragThreshold * vEsc || m2 < 1.8 || currentTotalBodies >= maxDebrisCap) {
        mergeBodies(*primary, *secondary, shockwaves, impactSparks, selectedBodyId, cameraFollowId);
        return;
    }

    double coreAbsorbFraction = 0.35;
    double absorbedMass = m2 * coreAbsorbFraction;
    double shatteredMass = m2 * (1.0 - coreAbsorbFraction);

    int availableSlots = std::max(0, maxDebrisCap - currentTotalBodies);
    int numFragments = std::clamp(static_cast<int>(secondary->radius * 1.5), 6, 16);
    numFragments = std::min(numFragments, availableSlots);

    if (numFragments < 2) {
        mergeBodies(*primary, *secondary, shockwaves, impactSparks, selectedBodyId, cameraFollowId);
        return;
    }

    Vec2d totalMomentum = primary->velocity * m1 + secondary->velocity * m2;
    Vec2d contactNormal = (secondary->position - primary->position).normalized();
    if (contactNormal.lengthSq() < 1e-6) contactNormal = Vec2d(1.0, 0.0);
    Vec2d impactCenter = primary->position + contactNormal * primary->radius;
    Vec2d vCom = totalMomentum / (m1 + m2);

    double newPrimaryMass = m1 + absorbedMass;
    if (!primary->isBlackHole()) {
        double r1Cube = primary->radius * primary->radius * primary->radius;
        double rAbsCube = (secondary->radius * secondary->radius * secondary->radius) * coreAbsorbFraction;
        primary->radius = std::cbrt(r1Cube + rAbsCube);
    }
    primary->mass = newPrimaryMass;
    primary->lastImpactAngle = std::atan2(contactNormal.y, contactNormal.x);
    primary->impactHeat = 1.0;
    primary->temperature = std::min(12000.0, primary->temperature + 2400.0 + vRel * 15.0);
    checkAstrophysicalTransitions(*primary, shockwaves);

    if (!primary->isPinned) {
        primary->velocity = (primary->velocity * m1 + secondary->velocity * absorbedMass) / newPrimaryMass;
    }

    double fragMass = shatteredMass / static_cast<double>(numFragments);
    double fragRadius = std::max(1.8, secondary->radius * std::cbrt((1.0 - coreAbsorbFraction) / numFragments));
    double ringDist = primary->radius + fragRadius * 2.4 + 3.5;
    double ejectSpeed = std::clamp(vRel * 0.42, vEsc * 0.65, vEsc * 2.2);

    std::uniform_real_distribution<double> angleJitter(-0.15, 0.15);
    std::uniform_real_distribution<double> speedJitter(0.75, 1.25);

    Vec2d sumFragRelMomentum{ 0.0, 0.0 };
    size_t startIdx = m_spawnedFragments.size();

    for (int k = 0; k < numFragments; ++k) {
        double baseAngle = (2.0 * M_PI * k) / static_cast<double>(numFragments) + angleJitter(m_rng);
        Vec2d radialDir(std::cos(baseAngle), std::sin(baseAngle));
        double spd = ejectSpeed * speedJitter(m_rng);
        Vec2d relVel = radialDir * spd;

        CelestialBody frag;
        frag.id = nextBodyId++;
        frag.name = secondary->name + " Frag";
        frag.bodyType = CelestialBodyType::Asteroid;
        frag.textureSeed = frag.id * 7919u;
        frag.mass = fragMass;
        frag.radius = fragRadius;
        frag.density = secondary->density;
        frag.temperature = 2800.0; // Glowing hot molten ejecta
        frag.impactHeat = 1.0;
        frag.position = primary->position + radialDir * ringDist;
        frag.velocity = relVel;
        frag.color = secondary->color;
        frag.isPinned = false;
        frag.isStar = false;
        frag.alive = true;

        sumFragRelMomentum += relVel * fragMass;
        m_spawnedFragments.push_back(frag);
    }

    Vec2d meanRelVel = sumFragRelMomentum / shatteredMass;
    Vec2d baseFragVel = (!primary->isPinned)
        ? ((totalMomentum - primary->velocity * primary->mass) / shatteredMass)
        : vCom;

    for (size_t idx = startIdx; idx < m_spawnedFragments.size(); ++idx) {
        m_spawnedFragments[idx].velocity = baseFragVel + (m_spawnedFragments[idx].velocity - meanRelVel);
    }

    if (selectedBodyId == static_cast<int>(secondary->id)) {
        selectedBodyId = static_cast<int>(primary->id);
    }
    if (cameraFollowId == static_cast<int>(secondary->id)) {
        cameraFollowId = static_cast<int>(primary->id);
    }

    spawnRealisticImpactPlumes(impactSparks, shockwaves, impactCenter, contactNormal,
                               vCom, secondary->velocity - primary->velocity,
                               secondary->radius, secondary->color, primary->color, primary->isBlackHole());

    secondary->alive = false;
}

void CollisionSystem::processRocheLimitShredding(std::vector<CelestialBody>& bodies,
                                                 std::vector<ShockwaveEvent>& shockwaves,
                                                 std::vector<ImpactSpark>& impactSparks,
                                                 double dt,
                                                 double G,
                                                 int maxDebrisCap,
                                                 uint32_t& nextBodyId,
                                                 int& selectedBodyId,
                                                 int& cameraFollowId) {
    m_spawnedFragments.clear();
    const size_t n = bodies.size();
    int currentTotal = static_cast<int>(n);

    for (size_t i = 0; i < n; ++i) {
        CelestialBody& sec = bodies[i];
        if (!sec.alive || sec.isLuminousStar() || sec.isBlackHole() || sec.isPinned) continue;
        // Skip tiny already-shredded asteroid particles so they form a stable accretion ring
        if (sec.bodyType == CelestialBodyType::Asteroid && sec.mass < 0.8) continue;

        for (size_t j = 0; j < n; ++j) {
            if (i == j || !bodies[j].alive) continue;
            const CelestialBody& pri = bodies[j];

            // Only massive stars, black holes, or gas giants with >= 15x mass exert tidal shredding
            if (pri.mass < 600.0 || pri.mass < sec.mass * 15.0) continue;

            Vec2d delta = sec.position - pri.position;
            double dist = delta.length();
            if (dist <= pri.radius + sec.radius) continue; // Handled by direct collision

            // Classical fluid Roche limit: d_Roche = 2.44 * R_primary * (rho_primary / rho_secondary)^(1/3)
            // Since M / R^3 is proportional to bulk density rho, R_pri * (rho_pri / rho_sec)^(1/3) = R_sec * (M_pri / M_sec)^(1/3).
            // We clamp this to a realistic tidal zone around the primary so planets visibly shred close to Black Holes & stars!
            double massRatioCbrt = std::cbrt(pri.mass / std::max(0.1, sec.mass));
            double theoreticalRoche = 2.44 * sec.radius * massRatioCbrt;
            double maxRocheZone = pri.isBlackHole() ? (pri.radius * 7.5) : (pri.radius * 3.4);
            double rocheLimit = std::min(theoreticalRoche, maxRocheZone);

            if (dist < rocheLimit) {
                // Inside the Roche limit! Heat the body via tidal friction and strip mass into an accretion stream
                double penetration = std::clamp((rocheLimit - dist) / std::max(1.0, rocheLimit - pri.radius), 0.1, 1.0);
                sec.temperature = std::min(8500.0, sec.temperature + 1800.0 * penetration * dt);
                sec.rocheDamageTimer += dt * (1.2 + 2.5 * penetration);

                // Emit continuous tidal sparks
                if (GetRandomValue(0, 100) < 45) {
                    spawnImpactSparks(impactSparks, sec.position, sec.velocity, 35.0, 4, sec.color);
                }

                if (sec.rocheDamageTimer >= 0.12 && currentTotal + static_cast<int>(m_spawnedFragments.size()) < maxDebrisCap) {
                    sec.rocheDamageTimer = 0.0;

                    Vec2d radial = delta / dist;
                    Vec2d tangent = radial.perpendicular();
                    if ((sec.velocity - pri.velocity).dot(tangent) < 0.0) {
                        tangent = -tangent;
                    }

                    // If body is small enough or deeply inside Roche limit, shatter completely into an accretion stream
                    if (sec.mass <= 3.5 || penetration > 0.72) {
                        int streamCount = std::clamp(static_cast<int>(sec.radius * 2.2), 8, 18);
                        streamCount = std::min(streamCount, maxDebrisCap - (currentTotal + static_cast<int>(m_spawnedFragments.size())));

                        if (streamCount >= 2) {
                            double fragMass = sec.mass / streamCount;
                            double fragRad = std::max(1.6, sec.radius * std::cbrt(1.0 / streamCount));

                            for (int k = 0; k < streamCount; ++k) {
                                // Spread along tidal axis (-radial toward L1, +radial toward L2) and tangential shear
                                double sNorm = (static_cast<double>(k) / (streamCount - 1)) * 2.0 - 1.0;
                                Vec2d offset = radial * (sNorm * sec.radius * 1.6) + tangent * (sNorm * sec.radius * 0.8);
                                Vec2d pos = sec.position + offset;
                                double rFrag = std::max(pri.radius + 3.0, (pos - pri.position).length());
                                double vKep = std::sqrt((G * pri.mass) / rFrag);

                                CelestialBody piece;
                                piece.id = nextBodyId++;
                                piece.name = sec.name + " Tidal Debris";
                                piece.bodyType = CelestialBodyType::Asteroid;
                                piece.textureSeed = piece.id * 3571u;
                                piece.mass = fragMass;
                                piece.radius = fragRad;
                                piece.density = sec.density;
                                piece.temperature = std::max(2200.0, sec.temperature);
                                piece.position = pos;
                                piece.velocity = pri.velocity + tangent * vKep + radial * (sNorm * 8.0);
                                piece.color = sec.color;
                                piece.alive = true;
                                m_spawnedFragments.push_back(piece);
                            }

                            spawnImpactSparks(impactSparks, sec.position, sec.velocity, 65.0, 80, Color{ 255, 160, 60, 255 });
                            if (shockwaves.size() < Config::MAX_SHOCKWAVES) {
                                ShockwaveEvent sw;
                                sw.position = sec.position;
                                sw.currentRadius = static_cast<float>(sec.radius);
                                sw.maxRadius = static_cast<float>(sec.radius * 4.5 + 18.0);
                                sw.flashRadius = static_cast<float>(sec.radius * 2.5 + 10.0);
                                sw.age = 0.0f;
                                sw.duration = 0.55f;
                                sw.color = Color{ 255, 170, 70, 255 };
                                shockwaves.push_back(sw);
                            }

                            if (selectedBodyId == static_cast<int>(sec.id)) selectedBodyId = static_cast<int>(pri.id);
                            if (cameraFollowId == static_cast<int>(sec.id)) cameraFollowId = static_cast<int>(pri.id);
                            sec.alive = false;
                            break;
                        }
                    } else {
                        // Strip 2 glowing tidal fragments from the L1/L2 lobes while shrinking the core
                        double shedMass = sec.mass * 0.14;
                        sec.mass -= shedMass;
                        sec.recomputeRadiusFromMass();

                        for (int lobe = -1; lobe <= 1; lobe += 2) {
                            Vec2d lobePos = sec.position + radial * (lobe * (sec.radius + 3.2));
                            double rLobe = std::max(pri.radius + 3.0, (lobePos - pri.position).length());
                            double vKep = std::sqrt((G * pri.mass) / rLobe);

                            CelestialBody piece;
                            piece.id = nextBodyId++;
                            piece.name = sec.name + " Stream";
                            piece.bodyType = CelestialBodyType::Asteroid;
                            piece.textureSeed = piece.id * 4099u;
                            piece.mass = shedMass * 0.5;
                            piece.radius = 1.9;
                            piece.density = sec.density;
                            piece.temperature = 2600.0;
                            piece.position = lobePos;
                            piece.velocity = pri.velocity + tangent * vKep;
                            piece.color = sec.color;
                            piece.alive = true;
                            m_spawnedFragments.push_back(piece);
                        }
                    }
                }
            }
        }
    }

    bodies.erase(
        std::remove_if(bodies.begin(), bodies.end(),
            [](const CelestialBody& b) { return !b.alive; }),
        bodies.end());

    if (!m_spawnedFragments.empty()) {
        bodies.insert(bodies.end(), m_spawnedFragments.begin(), m_spawnedFragments.end());
    }
}

} // namespace sim
