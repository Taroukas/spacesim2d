#include "physics/PhysicsWorld.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

namespace sim {

PhysicsWorld::PhysicsWorld() {
    bodies.reserve(2048);
    shockwaves.reserve(Config::MAX_SHOCKWAVES);
    impactSparks.reserve(Config::MAX_IMPACT_SPARKS);
    nebulaParticles.reserve(Config::MAX_NEBULA_PARTICLES);
    diagnostics.energyHistory.resize(Config::ENERGY_HISTORY_SIZE, 0.0f);
}

void PhysicsWorld::clear() {
    bodies.clear();
    shockwaves.clear();
    impactSparks.clear();
    nebulaParticles.clear();
    spacecraft.enabled = false;
    spacecraft.trail.clear();
    spacecraft.exhaustParticles.clear();
    selectedBodyId = -1;
    cameraFollowId = -1;
    referenceFrameBodyId = -1;
    diagnostics.baselineCaptured = false;
    std::fill(diagnostics.energyHistory.begin(), diagnostics.energyHistory.end(), 0.0f);
}

uint32_t PhysicsWorld::allocateBodyId() {
    return m_nextBodyId++;
}

CelestialBody& PhysicsWorld::addBody(const CelestialBody& prototype) {
    CelestialBody b = prototype;
    if (b.id == 0) {
        b.id = allocateBodyId();
    } else if (b.id >= m_nextBodyId) {
        m_nextBodyId = b.id + 1;
    }
    if (b.textureSeed == 101u) {
        b.textureSeed = b.id * 2654435761u;
    }
    b.alive = true;
    bodies.push_back(b);
    return bodies.back();
}

CelestialBody* PhysicsWorld::findBodyById(int id) {
    if (id < 0) return nullptr;
    for (auto& b : bodies) {
        if (b.alive && static_cast<int>(b.id) == id) {
            return &b;
        }
    }
    return nullptr;
}

const CelestialBody* PhysicsWorld::findBodyById(int id) const {
    if (id < 0) return nullptr;
    for (const auto& b : bodies) {
        if (b.alive && static_cast<int>(b.id) == id) {
            return &b;
        }
    }
    return nullptr;
}

const CelestialBody* PhysicsWorld::findNearestStarOrHeavyBody(const Vec2d& pos, int excludeId) const {
    const CelestialBody* best = nullptr;
    double bestScore = -1.0;

    for (const auto& b : bodies) {
        if (!b.alive || static_cast<int>(b.id) == excludeId) continue;
        double distSq = std::max(16.0, (b.position - pos).lengthSq());
        double score = b.mass / distSq;
        if (b.isLuminousStar() || b.isBlackHole()) score *= 6.0;
        if (score > bestScore) {
            bestScore = score;
            best = &b;
        }
    }
    return best;
}

void PhysicsWorld::update(double frameDt, bool uiCapturesKeyboard) {
    auto startTime = std::chrono::high_resolution_clock::now();

    double clampedFrameDt = std::min(frameDt, 0.05);
    spacecraft.handleInput(clampedFrameDt, uiCapturesKeyboard);

    if (!uiCapturesKeyboard && IsKeyPressed(KEY_M)) {
        spawnMeteorBarrage(selectedBodyId);
    }

    // Update visual shockwaves, impact sparks, and nebula clouds
    for (auto& sw : shockwaves) {
        sw.age += static_cast<float>(clampedFrameDt);
        float t = std::clamp(sw.age / sw.duration, 0.0f, 1.0f);
        float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        sw.currentRadius = sw.maxRadius * eased;
    }
    shockwaves.erase(
        std::remove_if(shockwaves.begin(), shockwaves.end(),
            [](const ShockwaveEvent& s) { return s.age >= s.duration; }),
        shockwaves.end());

    for (auto& sp : impactSparks) {
        sp.position += sp.velocity * clampedFrameDt;
        sp.age += static_cast<float>(clampedFrameDt);
    }
    impactSparks.erase(
        std::remove_if(impactSparks.begin(), impactSparks.end(),
            [](const ImpactSpark& s) { return s.age >= s.maxLife; }),
        impactSparks.end());

    for (auto& nb : nebulaParticles) {
        nb.position += nb.velocity * clampedFrameDt;
        nb.velocity *= std::pow(0.82, clampedFrameDt); // Interstellar drag
        nb.age += static_cast<float>(clampedFrameDt);
    }
    nebulaParticles.erase(
        std::remove_if(nebulaParticles.begin(), nebulaParticles.end(),
            [](const NebulaParticle& n) { return n.age >= n.maxLife; }),
        nebulaParticles.end());

    if (!paused) {
        double totalSimTime = clampedFrameDt * timeScale;

        int activeSubSteps = subSteps;
        if (autoSubStepping) {
            int autoCount = static_cast<int>(std::ceil(timeScale / 4.0));
            activeSubSteps = std::clamp(std::max(subSteps, autoCount), 1, Config::MAX_SUBSTEPS);
        }

        double subDt = totalSimTime / static_cast<double>(activeSubSteps);

        // Automatically switch to Barnes-Hut O(N log N) if debris/asteroid count grows large
        GravitySolverType effectiveSolver = (bodies.size() > 320) ? GravitySolverType::BarnesHut : solverType;

        for (int step = 0; step < activeSubSteps; ++step) {
            m_integrator.step(bodies, spacecraft, barnesHutTree,
                              integratorType, effectiveSolver, subDt,
                              G, softening, barnesHutTheta);

            m_collisionSystem.resolveCollisions(bodies, spacecraft, shockwaves, impactSparks,
                                                collisionMode, G, restitution,
                                                fragmentationThreshold, maxDebrisCap,
                                                m_nextBodyId, selectedBodyId, cameraFollowId);
        }

        // Check Roche Limit tidal shredding once per frame
        m_collisionSystem.processRocheLimitShredding(
            bodies, shockwaves, impactSparks, clampedFrameDt * std::min(timeScale, 8.0),
            G, maxDebrisCap, m_nextBodyId, selectedBodyId, cameraFollowId);

        // Rotate celestial bodies & cool down hot molten post-impact surfaces
        for (auto& b : bodies) {
            if (!b.alive) continue;
            b.rotationAngle = std::remainder(b.rotationAngle + b.angularVelocity * clampedFrameDt, 2.0 * M_PI);

            if (b.impactHeat > 0.0) {
                double coolRate = (b.bodyType == CelestialBodyType::Asteroid) ? 0.18 : 0.11;
                b.impactHeat = std::max(0.0, b.impactHeat - clampedFrameDt * coolRate);
            }

            if (!b.isLuminousStar() && !b.isBlackHole()) {
                double eqTemp = 260.0;
                // Radiative cooling of molten lava crust back toward equilibrium
                if (b.temperature > eqTemp) {
                    double excess = b.temperature - eqTemp;
                    b.temperature -= excess * std::min(0.95, 0.24 * clampedFrameDt);
                } else if (b.temperature < eqTemp) {
                    b.temperature += (eqTemp - b.temperature) * std::min(0.95, 0.05 * clampedFrameDt);
                }
            }
        }

        // Record orbital trails once per render frame
        Vec2d refPos{ 0.0, 0.0 };
        bool hasRef = false;
        if (const CelestialBody* refBody = findBodyById(referenceFrameBodyId)) {
            refPos = refBody->position;
            hasRef = true;
        }

        size_t effectiveTrailCap = showTrails ? maxTrailLength : 0;
        for (auto& b : bodies) {
            if (!b.alive) continue;
            // Skip trail recording for tiny asteroids when count is large to preserve 60 FPS
            if (b.bodyType == CelestialBodyType::Asteroid && bodies.size() > 120) continue;
            b.recordTrailPoint(refPos, hasRef, effectiveTrailCap);
        }

        spacecraft.updateParticlesAndTrail(clampedFrameDt, effectiveTrailCap);
    }

    if (spacecraft.enabled) {
        spacecraft.computeTelemetry(bodies, G);
    }

    m_diagnosticsTimer += clampedFrameDt;
    if (m_diagnosticsTimer >= 0.08 || !diagnostics.baselineCaptured) {
        m_diagnosticsTimer = 0.0;
        computeEnergyAndMomentum(!diagnostics.baselineCaptured);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    diagnostics.physicsStepTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
}

void PhysicsWorld::stepSingleFrame() {
    bool wasPaused = paused;
    paused = false;
    update(Config::BASE_DT, false);
    paused = wasPaused;
}

void PhysicsWorld::computeEnergyAndMomentum(bool resetBaseline) {
    double kinetic = 0.0;
    double potential = 0.0;
    Vec2d totalMomentum{ 0.0, 0.0 };
    Vec2d weightedPos{ 0.0, 0.0 };
    double totalMass = 0.0;
    double epsSq = softening * softening;

    const size_t n = bodies.size();
    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive) continue;
        double m = bodies[i].mass;
        double vSq = bodies[i].velocity.lengthSq();
        kinetic += 0.5 * m * vSq;
        totalMomentum += bodies[i].velocity * m;
        weightedPos += bodies[i].position * m;
        totalMass += m;

        if (n <= 500) {
            for (size_t j = i + 1; j < n; ++j) {
                if (!bodies[j].alive) continue;
                double distSq = (bodies[j].position - bodies[i].position).lengthSq() + epsSq;
                potential -= (G * m * bodies[j].mass) / std::sqrt(distSq);
            }
        }
    }

    if (n > 500) {
        for (size_t i = 0; i < n; ++i) {
            if (!bodies[i].alive) continue;
            if (bodies[i].mass > 50.0) {
                for (size_t j = 0; j < n; ++j) {
                    if (i == j || !bodies[j].alive) continue;
                    double distSq = (bodies[j].position - bodies[i].position).lengthSq() + epsSq;
                    potential -= 0.5 * (G * bodies[i].mass * bodies[j].mass) / std::sqrt(distSq);
                }
            }
        }
    }

    diagnostics.kineticEnergy = kinetic;
    diagnostics.potentialEnergy = potential;
    diagnostics.totalEnergy = kinetic + potential;
    diagnostics.totalMomentumMag = totalMomentum.length();

    if (totalMass > 0.0) {
        diagnostics.barycenter = weightedPos / totalMass;
        diagnostics.barycentricVelocity = totalMomentum / totalMass;
    }

    if (resetBaseline || !diagnostics.baselineCaptured) {
        diagnostics.initialTotalEnergy = diagnostics.totalEnergy;
        diagnostics.baselineCaptured = true;
    }

    if (std::abs(diagnostics.initialTotalEnergy) > 1e-6) {
        diagnostics.energyDriftPercent =
            ((diagnostics.totalEnergy - diagnostics.initialTotalEnergy) / std::abs(diagnostics.initialTotalEnergy)) * 100.0;
    } else {
        diagnostics.energyDriftPercent = 0.0;
    }

    if (!diagnostics.energyHistory.empty()) {
        std::rotate(diagnostics.energyHistory.begin(),
                    diagnostics.energyHistory.begin() + 1,
                    diagnostics.energyHistory.end());
        diagnostics.energyHistory.back() = static_cast<float>(diagnostics.totalEnergy);
    }
}

PredictedTrajectory PhysicsWorld::computePredictedTrajectory(const CelestialBody& probe,
                                                             int steps,
                                                             double stepDt,
                                                             int referenceBodyId) const {
    PredictedTrajectory result;
    result.points.reserve(steps + 1);

    struct SimProxy {
        uint32_t id;
        std::string name;
        Vec2d pos;
        Vec2d vel;
        Vec2d acc;
        double mass;
        double radius;
        bool isPinned;
    };

    std::vector<SimProxy> attractors;
    attractors.reserve(32);

    int refIndex = -1;
    int domIndex = -1;
    double maxPull = -1.0;

    for (const auto& b : bodies) {
        if (!b.alive || b.id == probe.id) continue;
        if (b.isLuminousStar() || b.isBlackHole() || b.mass >= Config::PREDICTOR_ATTRACTOR_MIN_MASS ||
            static_cast<int>(b.id) == referenceBodyId) {
            int idx = static_cast<int>(attractors.size());
            if (static_cast<int>(b.id) == referenceBodyId) {
                refIndex = idx;
            }
            double distSq = std::max(1.0, (b.position - probe.position).lengthSq());
            double pull = (G * b.mass) / distSq;
            if (pull > maxPull) {
                maxPull = pull;
                domIndex = idx;
            }

            attractors.push_back(SimProxy{
                b.id, b.name, b.position, b.velocity, { 0.0, 0.0 }, b.mass, b.radius, b.isPinned
            });
        }
    }

    Vec2d probePos = probe.position;
    Vec2d probeVel = probe.velocity;
    Vec2d probeAcc{ 0.0, 0.0 };

    Vec2d initialRefPos = (refIndex >= 0) ? attractors[refIndex].pos : Vec2d(0.0, 0.0);
    result.points.push_back(probePos);

    const double epsSq = softening * softening;

    auto computeAccelerations = [&]() {
        const size_t m = attractors.size();
        for (size_t i = 0; i < m; ++i) {
            attractors[i].acc = { 0.0, 0.0 };
        }
        for (size_t i = 0; i < m; ++i) {
            for (size_t j = i + 1; j < m; ++j) {
                Vec2d delta = attractors[j].pos - attractors[i].pos;
                double dSq = delta.lengthSq() + epsSq;
                double invD = 1.0 / std::sqrt(dSq);
                double invD3 = invD * invD * invD;
                Vec2d f = delta * (G * invD3);
                if (!attractors[i].isPinned) attractors[i].acc += f * attractors[j].mass;
                if (!attractors[j].isPinned) attractors[j].acc -= f * attractors[i].mass;
            }
        }

        probeAcc = { 0.0, 0.0 };
        for (size_t i = 0; i < m; ++i) {
            Vec2d delta = attractors[i].pos - probePos;
            double dSq = delta.lengthSq() + epsSq;
            double invD = 1.0 / std::sqrt(dSq);
            double invD3 = invD * invD * invD;
            probeAcc += delta * (G * attractors[i].mass * invD3);
            if (!attractors[i].isPinned && probe.mass > 1.0) {
                attractors[i].acc -= delta * (G * probe.mass * invD3);
            }
        }
    };

    computeAccelerations();

    double prevDist = -1.0;
    double prevPrevDist = -1.0;
    Vec2d prevPlotPoint = probePos;

    const double halfDtSq = 0.5 * stepDt * stepDt;
    const double halfDt = 0.5 * stepDt;

    std::vector<Vec2d> oldAcc(attractors.size());

    for (int step = 0; step < steps; ++step) {
        for (auto& a : attractors) {
            if (!a.isPinned) {
                a.pos += a.vel * stepDt + a.acc * halfDtSq;
            }
        }
        probePos += probeVel * stepDt + probeAcc * halfDtSq;

        for (size_t i = 0; i < attractors.size(); ++i) oldAcc[i] = attractors[i].acc;
        Vec2d oldProbeAcc = probeAcc;

        computeAccelerations();

        for (size_t i = 0; i < attractors.size(); ++i) {
            if (!attractors[i].isPinned) {
                attractors[i].vel += (oldAcc[i] + attractors[i].acc) * halfDt;
            }
        }
        probeVel += (oldProbeAcc + probeAcc) * halfDt;

        Vec2d plotPoint = probePos;
        if (refIndex >= 0) {
            plotPoint = (probePos - attractors[refIndex].pos) + initialRefPos;
        }
        result.points.push_back(plotPoint);

        if (domIndex >= 0) {
            double currDist = (probePos - attractors[domIndex].pos).length();
            if (prevDist > 0.0 && prevPrevDist > 0.0) {
                if (!result.hasPeriapsis && prevDist < prevPrevDist && prevDist < currDist) {
                    result.hasPeriapsis = true;
                    result.periapsisPoint = prevPlotPoint;
                    result.periapsisDist = prevDist;
                }
                if (!result.hasApoapsis && prevDist > prevPrevDist && prevDist > currDist) {
                    result.hasApoapsis = true;
                    result.apoapsisPoint = prevPlotPoint;
                    result.apoapsisDist = prevDist;
                }
            }
            prevPrevDist = prevDist;
            prevDist = currDist;
            prevPlotPoint = plotPoint;
        }

        for (const auto& a : attractors) {
            double hitRadius = a.radius + probe.radius;
            if ((probePos - a.pos).lengthSq() <= hitRadius * hitRadius) {
                result.hasImpact = true;
                result.impactPoint = plotPoint;
                result.impactBodyName = a.name;
                return result;
            }
        }
    }

    return result;
}

void PhysicsWorld::setCircularOrbitAroundNearest(int bodyId, bool clockwise) {
    CelestialBody* target = findBodyById(bodyId);
    if (!target) return;

    const CelestialBody* attractor = findNearestStarOrHeavyBody(target->position, bodyId);
    if (!attractor) return;

    Vec2d rVec = target->position - attractor->position;
    double r = rVec.length();
    if (r < 1.0) return;

    Vec2d tangent = rVec.normalized().perpendicular();
    if (clockwise) tangent = -tangent;

    double vCirc = std::sqrt((G * (attractor->mass + target->mass)) / r);
    target->velocity = attractor->velocity + tangent * vCirc;
    target->clearTrails();
}

void PhysicsWorld::splitSelectedBody(int bodyId) {
    CelestialBody* target = findBodyById(bodyId);
    if (!target) return;

    m_collisionSystem.spawnImpactSparks(impactSparks, target->position, target->velocity, 95.0, 140, target->color);

    if (shockwaves.size() < Config::MAX_SHOCKWAVES) {
        ShockwaveEvent sw;
        sw.position = target->position;
        sw.currentRadius = static_cast<float>(target->radius);
        sw.maxRadius = static_cast<float>(target->radius * 6.5 + 45.0);
        sw.flashRadius = static_cast<float>(target->radius * 3.8 + 20.0);
        sw.age = 0.0f;
        sw.duration = 0.85f;
        sw.color = Color{ 255, 160, 50, 255 };
        shockwaves.push_back(sw);
    }

    if (target->mass < 2.0) {
        deleteBody(bodyId);
        return;
    }

    CelestialBody copy = *target;
    target->alive = false;

    int count = 10;
    double fragMass = copy.mass / count;
    double fragRadius = std::max(2.0, copy.radius * std::cbrt(1.0 / count));
    double ringDist = fragRadius * 2.6 + 3.0;
    double ejectSpeed = std::sqrt((G * copy.mass) / std::max(2.0, copy.radius)) * 0.72;

    for (int i = 0; i < count; ++i) {
        double angle = (2.0 * M_PI * i) / count;
        Vec2d dir(std::cos(angle), std::sin(angle));

        CelestialBody frag;
        frag.id = allocateBodyId();
        frag.name = copy.name + " Shard";
        frag.bodyType = CelestialBodyType::Asteroid;
        frag.textureSeed = frag.id * 4999u;
        frag.mass = fragMass;
        frag.radius = fragRadius;
        frag.density = copy.density;
        frag.temperature = 3200.0;
        frag.position = copy.position + dir * ringDist;
        frag.velocity = copy.velocity + dir * ejectSpeed;
        frag.color = copy.color;
        frag.isPinned = false;
        frag.isStar = false;
        frag.alive = true;
        bodies.push_back(frag);
    }

    bodies.erase(
        std::remove_if(bodies.begin(), bodies.end(),
            [](const CelestialBody& b) { return !b.alive; }),
        bodies.end());

    selectedBodyId = -1;
}

void PhysicsWorld::triggerSupernova(int bodyId) {
    CelestialBody* target = findBodyById(bodyId);

    // If no valid bodyId was specified, detonate the selected body or the most massive star in the system
    if (!target) {
        target = findBodyById(selectedBodyId);
    }
    if (!target) {
        double maxM = -1.0;
        for (auto& b : bodies) {
            if (b.alive && (b.isLuminousStar() || b.mass > maxM)) {
                if (b.isLuminousStar() && b.mass > maxM) {
                    maxM = b.mass;
                    target = &b;
                } else if (!target && b.mass > maxM) {
                    maxM = b.mass;
                    target = &b;
                }
            }
        }
    }
    if (!target) return;

    Vec2d novaPos = target->position;
    Vec2d novaVel = target->velocity;
    double origMass = target->mass;
    double origRadius = target->radius;

    // 1. Spawn triple-layered Supernova Shockwaves & blinding Flash
    const Color ringColors[3] = {
        { 255, 250, 220, 255 },
        { 80,  220, 255, 255 },
        { 255,  90, 210, 255 }
    };
    const float durations[3] = { 1.4f, 2.2f, 3.1f };
    const float maxRadii[3] = { 950.0f, 1550.0f, 2100.0f };

    for (int i = 0; i < 3; ++i) {
        if (shockwaves.size() < Config::MAX_SHOCKWAVES) {
            ShockwaveEvent sw;
            sw.position = novaPos;
            sw.currentRadius = static_cast<float>(origRadius);
            sw.maxRadius = maxRadii[i];
            sw.flashRadius = static_cast<float>(origRadius * 16.0 + 180.0);
            sw.age = 0.0f;
            sw.duration = durations[i];
            sw.color = ringColors[i];
            sw.isSupernova = true;
            shockwaves.push_back(sw);
        }
    }

    // 2. Spawn expanding colorful Supernova Nebula particle clouds
    std::mt19937 rng(static_cast<unsigned int>(GetTime() * 1000.0) + target->id);
    std::uniform_real_distribution<double> angleDist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> speedDist(18.0, 240.0);
    std::uniform_real_distribution<float> lifeDist(5.0f, 11.0f);
    std::uniform_real_distribution<float> radDist(45.0f, 165.0f);
    std::uniform_int_distribution<int> colPick(0, 4);

    const Color nebulaPalette[5] = {
        {  45, 215, 255, 160 }, // Electric Cyan
        { 255,  70, 190, 150 }, // Supernova Magenta
        { 155,  85, 255, 150 }, // Deep Violet
        { 255, 185,  55, 155 }, // Stellar Gold
        { 255,  95,  60, 150 }  // Crimson Hydrogen-Alpha
    };

    for (int i = 0; i < 160; ++i) {
        if (nebulaParticles.size() >= Config::MAX_NEBULA_PARTICLES) break;
        double a = angleDist(rng);
        double spd = speedDist(rng);
        Vec2d dir(std::cos(a), std::sin(a));

        NebulaParticle np;
        np.position = novaPos + dir * (origRadius * 0.5);
        np.velocity = novaVel + dir * spd;
        np.age = 0.0f;
        np.maxLife = lifeDist(rng);
        np.startRadius = static_cast<float>(origRadius * 0.9 + 12.0);
        np.endRadius = radDist(rng);
        np.color = nebulaPalette[colPick(rng)];
        nebulaParticles.push_back(np);
    }

    // 3. Spawn 250+ high-velocity glowing stellar ejecta sparks
    m_collisionSystem.spawnImpactSparks(impactSparks, novaPos, novaVel, 290.0, 260, Color{ 110, 230, 255, 255 });

    // 4. Blast nearby planets outward, strip rings, and superheat their surfaces into molten lava!
    for (auto& b : bodies) {
        if (!b.alive || b.id == target->id || b.isPinned) continue;
        Vec2d delta = b.position - novaPos;
        double dist = std::max(15.0, delta.length());
        Vec2d dir = delta / dist;

        double blastImpulse = std::clamp((45000.0 / (dist + 40.0)), 8.0, 420.0);
        b.velocity += dir * blastImpulse;
        b.temperature = std::min(12000.0, b.temperature + (850000.0 / (dist + 60.0)));
        if (dist < 650.0) {
            b.hasRings = false; // Strip planetary rings in blast wave
        }
    }

    // 5. Transform core into a spinning Neutron Pulsar or Black Hole remnant!
    if (origMass >= 18000.0) {
        target->name = target->name + " Black Hole";
        target->bodyType = CelestialBodyType::BlackHole;
        target->isStar = false;
        target->mass = origMass * 0.45;
        target->radius = 14.0;
        target->color = Color{ 255, 160, 70, 255 };
        target->angularVelocity = 3.5;
        target->temperature = 10000.0;
    } else {
        target->name = target->name + " Pulsar";
        target->bodyType = CelestialBodyType::NeutronStar;
        target->isStar = true;
        target->mass = std::max(800.0, origMass * 0.28);
        target->radius = 6.5;
        target->color = Color{ 130, 235, 255, 255 };
        target->angularVelocity = 6.0;
        target->temperature = 15000.0;
    }
    target->clearTrails();
}

void PhysicsWorld::spawnAsteroidBelt(int centerBodyId, int count) {
    const CelestialBody* center = findBodyById(centerBodyId);
    if (!center) {
        center = findBodyById(selectedBodyId);
    }
    if (!center) {
        center = findNearestStarOrHeavyBody({ 0.0, 0.0 });
    }
    if (!center) return;

    Vec2d cPos = center->position;
    Vec2d cVel = center->velocity;
    double cMass = std::max(50.0, center->mass);
    double cRad = center->radius;

    int available = std::max(0, maxDebrisCap - static_cast<int>(bodies.size()));
    int spawnCount = std::min(count, available);
    if (spawnCount <= 0) return;

    std::mt19937 rng(static_cast<unsigned int>(GetTime() * 1000.0) + 901u);
    std::uniform_real_distribution<double> angleDist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radiusBand(cRad * 3.2 + 45.0, cRad * 6.8 + 150.0);
    std::uniform_real_distribution<double> massDist(0.15, 0.85);
    std::uniform_real_distribution<double> eccJitter(0.97, 1.03);

    for (int i = 0; i < spawnCount; ++i) {
        double r = radiusBand(rng);
        double a = angleDist(rng);
        Vec2d radial(std::cos(a), std::sin(a));
        Vec2d tangent = radial.perpendicular();
        double vCirc = std::sqrt((G * cMass) / r) * eccJitter(rng);

        CelestialBody rock;
        rock.id = allocateBodyId();
        rock.name = "Asteroid";
        rock.bodyType = CelestialBodyType::Asteroid;
        rock.textureSeed = rock.id * 6151u;
        rock.mass = massDist(rng);
        rock.radius = std::clamp(1.6 + 0.8 * rock.mass, 1.7, 2.8);
        rock.density = 1.5;
        rock.temperature = 220.0;
        rock.position = cPos + radial * r;
        rock.velocity = cVel + tangent * vCirc;
        rock.color = (i % 3 == 0) ? Color{ 195, 165, 135, 255 } : Color{ 155, 165, 180, 255 };
        rock.isPinned = false;
        rock.isStar = false;
        rock.alive = true;
        bodies.push_back(rock);
    }
}

void PhysicsWorld::spawnMeteorBarrage(int targetBodyId) {
    const CelestialBody* target = findBodyById(targetBodyId);
    if (!target) {
        for (const auto& b : bodies) {
            if (b.alive && b.bodyType != CelestialBodyType::Asteroid) {
                if (!target || (!b.isLuminousStar() && b.mass > target->mass)) {
                    target = &b;
                }
            }
        }
    }
    if (!target || static_cast<int>(bodies.size()) >= maxDebrisCap) return;

    Vec2d targetPos = target->position;
    Vec2d targetVel = target->velocity;
    double targetR = std::max(8.0, target->radius);

    std::mt19937 rng(static_cast<unsigned int>(GetTime() * 1000.0) + 777u);
    std::uniform_real_distribution<double> baseAngDist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> spreadDist(-0.28, 0.28);
    std::uniform_real_distribution<double> distJitter(45.0, 165.0);
    std::uniform_real_distribution<double> spdDist(160.0, 290.0);
    std::uniform_real_distribution<double> velJitter(-14.0, 14.0);
    std::uniform_real_distribution<double> massDist(0.35, 1.45);

    double baseAngle = baseAngDist(rng);
    int barrageCount = std::min(14, maxDebrisCap - static_cast<int>(bodies.size()));

    for (int k = 0; k < barrageCount; ++k) {
        double a = baseAngle + spreadDist(rng);
        double dist = targetR * 3.4 + distJitter(rng);
        Vec2d spawnPos = targetPos + Vec2d(std::cos(a) * dist, std::sin(a) * dist);
        Vec2d toTarget = (targetPos - spawnPos).normalized();
        double spd = spdDist(rng);

        CelestialBody meteor;
        meteor.id = allocateBodyId();
        meteor.name = "Meteorite-" + std::to_string(meteor.id);
        meteor.bodyType = CelestialBodyType::Asteroid;
        meteor.textureSeed = meteor.id * 8191u;
        meteor.mass = massDist(rng);
        meteor.radius = std::clamp(2.0 + 0.65 * meteor.mass, 2.1, 3.6);
        meteor.density = 1.6;
        meteor.temperature = 1950.0;
        meteor.impactHeat = 0.85;
        meteor.position = spawnPos;
        meteor.velocity = targetVel + toTarget * spd + Vec2d(velJitter(rng), velJitter(rng));
        meteor.color = Color{ 235, 145, 85, 255 };
        meteor.alive = true;
        bodies.push_back(meteor);
    }
}

void PhysicsWorld::applyCosmicScaleMode(bool logarithmicScale) {
    Config::use_logarithmic_scale = logarithmicScale;

    // Find primary star or supermassive black hole to rescale planetary orbits around
    const CelestialBody* primary = nullptr;
    for (const auto& b : bodies) {
        if (b.alive && (b.isLuminousStar() || b.isBlackHole())) {
            if (!primary || b.mass > primary->mass) {
                primary = &b;
            }
        }
    }
    if (!primary) return;

    Vec2d centerPos = primary->position;
    Vec2d centerVel = primary->velocity;
    double primaryMass = primary->mass;
    int primaryId = static_cast<int>(primary->id);

    for (auto& b : bodies) {
        if (!b.alive || static_cast<int>(b.id) == primaryId || b.isPinned) continue;
        Vec2d rel = b.position - centerPos;
        double currDist = rel.length();
        if (currDist < primary->radius + 5.0) continue;

        double targetDist = currDist;
        if (b.semiMajorAxisAU > 0.01) {
            targetDist = Config::scaleOrbitalDistanceAU(b.semiMajorAxisAU);
        } else {
            // Map arbitrary distance between compressed logarithmic and vast linear cosmic scale
            double approxAU = logarithmicScale ? ((currDist - 45.0) / 420.0)
                                               : std::pow(std::max(0.1, (currDist - 65.0) / 280.0), 1.32);
            approxAU = std::clamp(approxAU, 0.15, 45.0);
            targetDist = Config::scaleOrbitalDistanceAU(approxAU);
        }

        Vec2d radial = rel / currDist;
        Vec2d tangent = radial.perpendicular();
        if ((b.velocity - centerVel).dot(tangent) < 0.0) {
            tangent = -tangent;
        }

        b.position = centerPos + radial * targetDist;
        double vCirc = std::sqrt((G * (primaryMass + b.mass)) / targetDist);
        b.velocity = centerVel + tangent * vCirc;
        b.clearTrails();
    }

    computeEnergyAndMomentum(true);
}

void PhysicsWorld::deleteBody(int bodyId) {
    for (auto& b : bodies) {
        if (static_cast<int>(b.id) == bodyId) {
            b.alive = false;
        }
    }
    bodies.erase(
        std::remove_if(bodies.begin(), bodies.end(),
            [](const CelestialBody& b) { return !b.alive; }),
        bodies.end());

    if (selectedBodyId == bodyId) selectedBodyId = -1;
    if (cameraFollowId == bodyId) cameraFollowId = -1;
    if (referenceFrameBodyId == bodyId) referenceFrameBodyId = -1;
}

void PhysicsWorld::clearAllTrails() {
    for (auto& b : bodies) {
        b.clearTrails();
    }
    spacecraft.trail.clear();
}

} // namespace sim
