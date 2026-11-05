#pragma once

#include "physics/PhysicsWorld.hpp"
#include "rendering/Camera2DController.hpp"
#include <string>
#include <vector>

namespace sim {

enum class ScenarioPreset : int {
    ScaledSolarSystem = 0,
    BinaryStarSystem = 1,
    FigureEightChoreography = 2,
    BlackHoleAccretionDisk = 3,
    BarnesHutGalaxyCollision = 4,
    SpacecraftSlingshotMission = 5,
    TON618Quasar = 6,
    ProceduralSystem = 7,
    COUNT = 8
};

class PresetManager {
public:
    static const char* getPresetName(ScenarioPreset preset);
    static const char* getPresetDescription(ScenarioPreset preset);

    static void loadPreset(ScenarioPreset preset,
                           PhysicsWorld& world,
                           Camera2DController& camera);

    static void zeroBarycentricMomentum(std::vector<CelestialBody>& bodies);
    static void generateProceduralStarSystem(PhysicsWorld& world, Camera2DController& camera);

private:
    static void setupSolarSystem(PhysicsWorld& world, Camera2DController& camera);
    static void setupBinaryStarSystem(PhysicsWorld& world, Camera2DController& camera);
    static void setupFigureEight(PhysicsWorld& world, Camera2DController& camera);
    static void setupBlackHoleAccretionDisk(PhysicsWorld& world, Camera2DController& camera);
    static void setupGalaxyCollision(PhysicsWorld& world, Camera2DController& camera);
    static void setupSpacecraftMission(PhysicsWorld& world, Camera2DController& camera);
    static void setupTON618Quasar(PhysicsWorld& world, Camera2DController& camera);
};

} // namespace sim
