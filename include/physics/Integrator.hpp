#pragma once

#include "core/Config.hpp"
#include "core/Vec2d.hpp"
#include "physics/BarnesHutTree.hpp"
#include "physics/CelestialBody.hpp"
#include "physics/Spacecraft.hpp"
#include <vector>

namespace sim {

class Integrator {
public:
    Integrator() = default;

    void step(std::vector<CelestialBody>& bodies,
              Spacecraft& spacecraft,
              BarnesHutTree& tree,
              IntegratorType integratorType,
              GravitySolverType solverType,
              double dt,
              double G,
              double softening,
              double theta);

private:
    // Pre-allocated state scratch buffers for RK4 to avoid heap allocations per frame
    std::vector<Vec2d> m_initialPos;
    std::vector<Vec2d> m_initialVel;
    std::vector<Vec2d> m_k1r, m_k1v;
    std::vector<Vec2d> m_k2r, m_k2v;
    std::vector<Vec2d> m_k3r, m_k3v;
    std::vector<Vec2d> m_k4r, m_k4v;

    void stepVelocityVerlet(std::vector<CelestialBody>& bodies,
                            Spacecraft& spacecraft,
                            BarnesHutTree& tree,
                            GravitySolverType solverType,
                            double dt,
                            double G,
                            double softening,
                            double theta);

    void stepRK4(std::vector<CelestialBody>& bodies,
                 Spacecraft& spacecraft,
                 BarnesHutTree& tree,
                 GravitySolverType solverType,
                 double dt,
                 double G,
                 double softening,
                 double theta);

    Vec2d computeSpacecraftGravity(const Vec2d& shipPos,
                                   const std::vector<CelestialBody>& bodies,
                                   const BarnesHutTree& tree,
                                   GravitySolverType solverType,
                                   double G,
                                   double softening,
                                   double theta) const;
};

} // namespace sim
