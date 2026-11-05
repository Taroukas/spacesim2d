#pragma once

#include "core/Config.hpp"
#include "core/Vec2d.hpp"
#include "physics/CelestialBody.hpp"
#include <cstdint>
#include <raylib.h>
#include <vector>

namespace sim {

class BarnesHutTree {
public:
    struct QuadNode {
        Vec2d center{ 0.0, 0.0 };
        double halfSize = 0.0;
        double totalMass = 0.0;
        Vec2d centerOfMass{ 0.0, 0.0 };

        int bodyIndex = -1; // -1 = empty or internal node, >=0 = leaf body index
        bool isLeaf = true;

        // Children indices in flat node pool: 0=NW, 1=NE, 2=SW, 3=SE (-1 if unallocated)
        int children[4] = { -1, -1, -1, -1 };
    };

    BarnesHutTree();

    // Builds the quadtree over all alive bodies with zero per-step heap churn
    void build(const std::vector<CelestialBody>& bodies);

    // Computes net acceleration on a point mass at pos (excluding ignoreBodyId)
    Vec2d computeAcceleration(const Vec2d& pos, uint32_t ignoreBodyId,
                              const std::vector<CelestialBody>& bodies,
                              double G, double softeningSq, double theta) const;

    // Populates world-space rectangles of active quadtree nodes for debug rendering
    void getDebugRectangles(std::vector<Rectangle>& outRects) const;

    // Solves all body accelerations using either Direct O(N^2) or Barnes-Hut O(N log N)
    void computeAllAccelerations(std::vector<CelestialBody>& bodies,
                                 GravitySolverType solverType,
                                 double G, double softening, double theta);

private:
    std::vector<QuadNode> m_nodes;
    int m_rootIndex = -1;

    int allocateNode(const Vec2d& center, double halfSize);
    int getQuadrant(const Vec2d& nodeCenter, const Vec2d& pos) const;
    Vec2d getChildCenter(const Vec2d& parentCenter, double childHalfSize, int quadrant) const;

    void insertBody(int nodeIdx, int bodyIdx, const std::vector<CelestialBody>& bodies, int depth);
    Vec2d computeForceRecursive(int nodeIdx, const Vec2d& pos, uint32_t ignoreBodyId,
                                const std::vector<CelestialBody>& bodies,
                                double G, double softeningSq, double theta) const;
};

} // namespace sim
