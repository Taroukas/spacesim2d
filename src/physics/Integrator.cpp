#include "physics/Integrator.hpp"
#include <cmath>

namespace sim {

void Integrator::step(std::vector<CelestialBody>& bodies,
                      Spacecraft& spacecraft,
                      BarnesHutTree& tree,
                      IntegratorType integratorType,
                      GravitySolverType solverType,
                      double dt,
                      double G,
                      double softening,
                      double theta) {
    if (integratorType == IntegratorType::VelocityVerlet) {
        stepVelocityVerlet(bodies, spacecraft, tree, solverType, dt, G, softening, theta);
    } else {
        stepRK4(bodies, spacecraft, tree, solverType, dt, G, softening, theta);
    }
}

Vec2d Integrator::computeSpacecraftGravity(const Vec2d& shipPos,
                                           const std::vector<CelestialBody>& bodies,
                                           const BarnesHutTree& tree,
                                           GravitySolverType solverType,
                                           double G,
                                           double softening,
                                           double theta) const {
    const double epsSq = softening * softening;
    if (solverType == GravitySolverType::BarnesHut) {
        return tree.computeAcceleration(shipPos, 0, bodies, G, epsSq, theta);
    }

    Vec2d acc{ 0.0, 0.0 };
    for (const auto& b : bodies) {
        if (!b.alive) continue;
        Vec2d delta = b.position - shipPos;
        double distSq = delta.lengthSq() + epsSq;
        double invDist = 1.0 / std::sqrt(distSq);
        double invDist3 = invDist * invDist * invDist;
        acc += delta * (G * b.mass * invDist3);
    }
    return acc;
}

void Integrator::stepVelocityVerlet(std::vector<CelestialBody>& bodies,
                                    Spacecraft& spacecraft,
                                    BarnesHutTree& tree,
                                    GravitySolverType solverType,
                                    double dt,
                                    double G,
                                    double softening,
                                    double theta) {
    const double halfDtSq = 0.5 * dt * dt;
    const double halfDt = 0.5 * dt;

    // Step 1: Update positions r(t + dt) = r(t) + v(t)*dt + 0.5*a(t)*dt^2
    for (auto& b : bodies) {
        if (!b.alive || b.isPinned) continue;
        b.prevAcceleration = b.acceleration;
        b.position += b.velocity * dt + b.acceleration * halfDtSq;
    }

    Vec2d thrustAcc{ 0.0, 0.0 };
    if (spacecraft.enabled) {
        thrustAcc = spacecraft.computeThrustAcceleration(dt, bodies);
        spacecraft.prevAcceleration = spacecraft.acceleration;
        spacecraft.position += spacecraft.velocity * dt + spacecraft.acceleration * halfDtSq;
    }

    // Step 2: Recompute gravitational accelerations a(t + dt) at new positions
    tree.computeAllAccelerations(bodies, solverType, G, softening, theta);

    if (spacecraft.enabled) {
        Vec2d gravAcc = computeSpacecraftGravity(spacecraft.position, bodies, tree, solverType, G, softening, theta);
        spacecraft.acceleration = gravAcc + thrustAcc;
    }

    // Step 3: Update velocities v(t + dt) = v(t) + 0.5*(a(t) + a(t + dt))*dt
    for (auto& b : bodies) {
        if (!b.alive || b.isPinned) continue;
        b.velocity += (b.prevAcceleration + b.acceleration) * halfDt;
    }

    if (spacecraft.enabled) {
        spacecraft.velocity += (spacecraft.prevAcceleration + spacecraft.acceleration) * halfDt;
    }
}

void Integrator::stepRK4(std::vector<CelestialBody>& bodies,
                         Spacecraft& spacecraft,
                         BarnesHutTree& tree,
                         GravitySolverType solverType,
                         double dt,
                         double G,
                         double softening,
                         double theta) {
    const size_t n = bodies.size();

    m_initialPos.resize(n);
    m_initialVel.resize(n);
    m_k1r.resize(n); m_k1v.resize(n);
    m_k2r.resize(n); m_k2v.resize(n);
    m_k3r.resize(n); m_k3v.resize(n);
    m_k4r.resize(n); m_k4v.resize(n);

    for (size_t i = 0; i < n; ++i) {
        m_initialPos[i] = bodies[i].position;
        m_initialVel[i] = bodies[i].velocity;
    }

    Vec2d shipInitPos = spacecraft.position;
    Vec2d shipInitVel = spacecraft.velocity;
    Vec2d shipThrustAcc{ 0.0, 0.0 };
    Vec2d sk1r, sk1v, sk2r, sk2v, sk3r, sk3v, sk4r, sk4v;

    if (spacecraft.enabled) {
        shipThrustAcc = spacecraft.computeThrustAcceleration(dt, bodies);
    }

    // --- Stage k1 ---
    tree.computeAllAccelerations(bodies, solverType, G, softening, theta);
    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive || bodies[i].isPinned) continue;
        m_k1r[i] = m_initialVel[i];
        m_k1v[i] = bodies[i].acceleration;
    }
    if (spacecraft.enabled) {
        sk1r = shipInitVel;
        sk1v = computeSpacecraftGravity(shipInitPos, bodies, tree, solverType, G, softening, theta) + shipThrustAcc;
    }

    // --- Stage k2 (t + 0.5 * dt) ---
    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive || bodies[i].isPinned) continue;
        bodies[i].position = m_initialPos[i] + m_k1r[i] * (0.5 * dt);
        bodies[i].velocity = m_initialVel[i] + m_k1v[i] * (0.5 * dt);
    }
    if (spacecraft.enabled) {
        spacecraft.position = shipInitPos + sk1r * (0.5 * dt);
        spacecraft.velocity = shipInitVel + sk1v * (0.5 * dt);
    }

    tree.computeAllAccelerations(bodies, solverType, G, softening, theta);
    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive || bodies[i].isPinned) continue;
        m_k2r[i] = bodies[i].velocity;
        m_k2v[i] = bodies[i].acceleration;
    }
    if (spacecraft.enabled) {
        sk2r = spacecraft.velocity;
        sk2v = computeSpacecraftGravity(spacecraft.position, bodies, tree, solverType, G, softening, theta) + shipThrustAcc;
    }

    // --- Stage k3 (t + 0.5 * dt) ---
    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive || bodies[i].isPinned) continue;
        bodies[i].position = m_initialPos[i] + m_k2r[i] * (0.5 * dt);
        bodies[i].velocity = m_initialVel[i] + m_k2v[i] * (0.5 * dt);
    }
    if (spacecraft.enabled) {
        spacecraft.position = shipInitPos + sk2r * (0.5 * dt);
        spacecraft.velocity = shipInitVel + sk2v * (0.5 * dt);
    }

    tree.computeAllAccelerations(bodies, solverType, G, softening, theta);
    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive || bodies[i].isPinned) continue;
        m_k3r[i] = bodies[i].velocity;
        m_k3v[i] = bodies[i].acceleration;
    }
    if (spacecraft.enabled) {
        sk3r = spacecraft.velocity;
        sk3v = computeSpacecraftGravity(spacecraft.position, bodies, tree, solverType, G, softening, theta) + shipThrustAcc;
    }

    // --- Stage k4 (t + dt) ---
    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive || bodies[i].isPinned) continue;
        bodies[i].position = m_initialPos[i] + m_k3r[i] * dt;
        bodies[i].velocity = m_initialVel[i] + m_k3v[i] * dt;
    }
    if (spacecraft.enabled) {
        spacecraft.position = shipInitPos + sk3r * dt;
        spacecraft.velocity = shipInitVel + sk3v * dt;
    }

    tree.computeAllAccelerations(bodies, solverType, G, softening, theta);
    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive || bodies[i].isPinned) continue;
        m_k4r[i] = bodies[i].velocity;
        m_k4v[i] = bodies[i].acceleration;
    }
    if (spacecraft.enabled) {
        sk4r = spacecraft.velocity;
        sk4v = computeSpacecraftGravity(spacecraft.position, bodies, tree, solverType, G, softening, theta) + shipThrustAcc;
    }

    // --- Combine RK4 weights ---
    const double dtOver6 = dt / 6.0;
    for (size_t i = 0; i < n; ++i) {
        if (!bodies[i].alive || bodies[i].isPinned) {
            bodies[i].position = m_initialPos[i];
            bodies[i].velocity = { 0.0, 0.0 };
            continue;
        }
        bodies[i].position = m_initialPos[i] + (m_k1r[i] + 2.0 * m_k2r[i] + 2.0 * m_k3r[i] + m_k4r[i]) * dtOver6;
        bodies[i].velocity = m_initialVel[i] + (m_k1v[i] + 2.0 * m_k2v[i] + 2.0 * m_k3v[i] + m_k4v[i]) * dtOver6;
    }

    if (spacecraft.enabled) {
        spacecraft.position = shipInitPos + (sk1r + 2.0 * sk2r + 2.0 * sk3r + sk4r) * dtOver6;
        spacecraft.velocity = shipInitVel + (sk1v + 2.0 * sk2v + 2.0 * sk3v + sk4v) * dtOver6;
        spacecraft.acceleration = sk4v;
    }
}

} // namespace sim
