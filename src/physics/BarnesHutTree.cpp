#include "physics/BarnesHutTree.hpp"
#include <algorithm>
#include <cmath>

namespace sim {

BarnesHutTree::BarnesHutTree() {
    m_nodes.reserve(8192);
}

int BarnesHutTree::allocateNode(const Vec2d& center, double halfSize) {
    QuadNode node;
    node.center = center;
    node.halfSize = halfSize;
    node.totalMass = 0.0;
    node.centerOfMass = { 0.0, 0.0 };
    node.bodyIndex = -1;
    node.isLeaf = true;
    node.children[0] = -1;
    node.children[1] = -1;
    node.children[2] = -1;
    node.children[3] = -1;

    m_nodes.push_back(node);
    return static_cast<int>(m_nodes.size()) - 1;
}

int BarnesHutTree::getQuadrant(const Vec2d& nodeCenter, const Vec2d& pos) const {
    // 0: NW (-x, -y), 1: NE (+x, -y), 2: SW (-x, +y), 3: SE (+x, +y)
    int quad = 0;
    if (pos.x >= nodeCenter.x) quad |= 1;
    if (pos.y >= nodeCenter.y) quad |= 2;
    return quad;
}

Vec2d BarnesHutTree::getChildCenter(const Vec2d& parentCenter, double childHalfSize, int quadrant) const {
    double offsetX = (quadrant & 1) ? childHalfSize : -childHalfSize;
    double offsetY = (quadrant & 2) ? childHalfSize : -childHalfSize;
    return Vec2d(parentCenter.x + offsetX, parentCenter.y + offsetY);
}

void BarnesHutTree::build(const std::vector<CelestialBody>& bodies) {
    m_nodes.clear();
    m_rootIndex = -1;

    if (bodies.empty()) {
        return;
    }

    double minX = 1e30, maxX = -1e30;
    double minY = 1e30, maxY = -1e30;
    bool hasAlive = false;

    for (const auto& b : bodies) {
        if (!b.alive) continue;
        hasAlive = true;
        minX = std::min(minX, b.position.x);
        maxX = std::max(maxX, b.position.x);
        minY = std::min(minY, b.position.y);
        maxY = std::max(maxY, b.position.y);
    }

    if (!hasAlive) {
        return;
    }

    Vec2d center(0.5 * (minX + maxX), 0.5 * (minY + maxY));
    double span = std::max(maxX - minX, maxY - minY);
    double halfSize = std::max(10.0, 0.55 * span + 10.0);

    m_rootIndex = allocateNode(center, halfSize);

    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].alive && bodies[i].mass > 0.0) {
            insertBody(m_rootIndex, static_cast<int>(i), bodies, 0);
        }
    }
}

void BarnesHutTree::insertBody(int nodeIdx, int bodyIdx, const std::vector<CelestialBody>& bodies, int depth) {
    const CelestialBody& newBody = bodies[bodyIdx];

    // If node is an empty leaf, store the body directly
    if (m_nodes[nodeIdx].isLeaf && m_nodes[nodeIdx].bodyIndex == -1) {
        m_nodes[nodeIdx].bodyIndex = bodyIdx;
        m_nodes[nodeIdx].totalMass = newBody.mass;
        m_nodes[nodeIdx].centerOfMass = newBody.position;
        return;
    }

    // If we reached maximum depth (e.g., two bodies at nearly identical coordinates),
    // aggregate into center of mass without subdividing further to prevent stack overflow
    if (depth >= Config::BARNES_HUT_MAX_DEPTH) {
        double combinedMass = m_nodes[nodeIdx].totalMass + newBody.mass;
        if (combinedMass > 0.0) {
            m_nodes[nodeIdx].centerOfMass =
                (m_nodes[nodeIdx].centerOfMass * m_nodes[nodeIdx].totalMass + newBody.position * newBody.mass) / combinedMass;
            m_nodes[nodeIdx].totalMass = combinedMass;
        }
        return;
    }

    // If this node was a leaf occupied by an existing body, push that existing body down first
    if (m_nodes[nodeIdx].isLeaf && m_nodes[nodeIdx].bodyIndex != -1) {
        int existingBodyIdx = m_nodes[nodeIdx].bodyIndex;
        m_nodes[nodeIdx].bodyIndex = -1;
        m_nodes[nodeIdx].isLeaf = false;

        Vec2d nodeCenter = m_nodes[nodeIdx].center;
        double childHalf = m_nodes[nodeIdx].halfSize * 0.5;
        int existingQuad = getQuadrant(nodeCenter, bodies[existingBodyIdx].position);

        int childIdx = m_nodes[nodeIdx].children[existingQuad];
        if (childIdx == -1) {
            Vec2d childCenter = getChildCenter(nodeCenter, childHalf, existingQuad);
            childIdx = allocateNode(childCenter, childHalf);
            m_nodes[nodeIdx].children[existingQuad] = childIdx;
        }
        insertBody(childIdx, existingBodyIdx, bodies, depth + 1);
    }

    // Update running center of mass and total mass of this internal node
    double newTotalMass = m_nodes[nodeIdx].totalMass + newBody.mass;
    if (newTotalMass > 0.0) {
        m_nodes[nodeIdx].centerOfMass =
            (m_nodes[nodeIdx].centerOfMass * m_nodes[nodeIdx].totalMass + newBody.position * newBody.mass) / newTotalMass;
        m_nodes[nodeIdx].totalMass = newTotalMass;
    }

    // Insert the new body into its corresponding quadrant child
    Vec2d nodeCenter = m_nodes[nodeIdx].center;
    double childHalf = m_nodes[nodeIdx].halfSize * 0.5;
    int newQuad = getQuadrant(nodeCenter, newBody.position);

    int childIdx = m_nodes[nodeIdx].children[newQuad];
    if (childIdx == -1) {
        Vec2d childCenter = getChildCenter(nodeCenter, childHalf, newQuad);
        childIdx = allocateNode(childCenter, childHalf);
        m_nodes[nodeIdx].children[newQuad] = childIdx;
    }
    insertBody(childIdx, bodyIdx, bodies, depth + 1);
}

Vec2d BarnesHutTree::computeAcceleration(const Vec2d& pos, uint32_t ignoreBodyId,
                                         const std::vector<CelestialBody>& bodies,
                                         double G, double softeningSq, double theta) const {
    if (m_rootIndex == -1 || m_nodes.empty()) {
        return Vec2d(0.0, 0.0);
    }
    return computeForceRecursive(m_rootIndex, pos, ignoreBodyId, bodies, G, softeningSq, theta);
}

Vec2d BarnesHutTree::computeForceRecursive(int nodeIdx, const Vec2d& pos, uint32_t ignoreBodyId,
                                           const std::vector<CelestialBody>& bodies,
                                           double G, double softeningSq, double theta) const {
    const QuadNode& node = m_nodes[nodeIdx];
    if (node.totalMass <= 0.0) {
        return Vec2d(0.0, 0.0);
    }

    Vec2d delta = node.centerOfMass - pos;
    double distSq = delta.lengthSq();

    if (node.isLeaf) {
        if (node.bodyIndex != -1 && bodies[node.bodyIndex].id == ignoreBodyId) {
            return Vec2d(0.0, 0.0);
        }
        double invDist = 1.0 / std::sqrt(distSq + softeningSq);
        double invDist3 = invDist * invDist * invDist;
        return delta * (G * node.totalMass * invDist3);
    }

    double nodeWidth = node.halfSize * 2.0;
    // Check Barnes-Hut opening angle criterion: (s / d) < theta  <=>  s^2 < theta^2 * d^2
    if (nodeWidth * nodeWidth < (theta * theta) * distSq) {
        double invDist = 1.0 / std::sqrt(distSq + softeningSq);
        double invDist3 = invDist * invDist * invDist;
        return delta * (G * node.totalMass * invDist3);
    }

    Vec2d netAcc{ 0.0, 0.0 };
    for (int i = 0; i < 4; ++i) {
        if (node.children[i] != -1) {
            netAcc += computeForceRecursive(node.children[i], pos, ignoreBodyId, bodies, G, softeningSq, theta);
        }
    }
    return netAcc;
}

void BarnesHutTree::getDebugRectangles(std::vector<Rectangle>& outRects) const {
    outRects.clear();
    outRects.reserve(m_nodes.size());

    for (const auto& node : m_nodes) {
        if (node.totalMass <= 0.0) continue;
        Rectangle r;
        r.x = static_cast<float>(node.center.x - node.halfSize);
        r.y = static_cast<float>(node.center.y - node.halfSize);
        r.width = static_cast<float>(node.halfSize * 2.0);
        r.height = static_cast<float>(node.halfSize * 2.0);
        outRects.push_back(r);
    }
}

void BarnesHutTree::computeAllAccelerations(std::vector<CelestialBody>& bodies,
                                            GravitySolverType solverType,
                                            double G, double softening, double theta) {
    const size_t n = bodies.size();
    const double epsSq = softening * softening;

    for (size_t i = 0; i < n; ++i) {
        bodies[i].acceleration = { 0.0, 0.0 };
    }

    if (solverType == GravitySolverType::DirectNBody) {
        // Pairwise Newtonian symmetry O(N^2) solver
        for (size_t i = 0; i < n; ++i) {
            if (!bodies[i].alive) continue;
            Vec2d posI = bodies[i].position;
            double massI = bodies[i].mass;
            Vec2d accI = bodies[i].acceleration;

            for (size_t j = i + 1; j < n; ++j) {
                if (!bodies[j].alive) continue;

                Vec2d delta = bodies[j].position - posI;
                double distSq = delta.lengthSq() + epsSq;
                double invDist = 1.0 / std::sqrt(distSq);
                double invDist3 = invDist * invDist * invDist;
                Vec2d commonTerm = delta * (G * invDist3);

                accI += commonTerm * bodies[j].mass;
                bodies[j].acceleration -= commonTerm * massI;
            }
            bodies[i].acceleration = accI;
        }
    } else {
        // Barnes-Hut Quadtree O(N log N) solver
        build(bodies);
        for (size_t i = 0; i < n; ++i) {
            if (!bodies[i].alive || bodies[i].isPinned) continue;
            bodies[i].acceleration = computeAcceleration(
                bodies[i].position, bodies[i].id, bodies, G, epsSq, theta);
        }
    }

    for (size_t i = 0; i < n; ++i) {
        if (bodies[i].isPinned) {
            bodies[i].acceleration = { 0.0, 0.0 };
            bodies[i].velocity = { 0.0, 0.0 };
        }
    }
}

} // namespace sim
