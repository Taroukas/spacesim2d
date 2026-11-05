#pragma once

#include "entities/Catalog.hpp"
#include "physics/PhysicsWorld.hpp"
#include "rendering/Camera2DController.hpp"
#include "rendering/Renderer.hpp"
#include "scenarios/PresetManager.hpp"
#include <deque>
#include <string>

namespace sim {

class UIManager {
public:
    UIManager();
    ~UIManager();

    void initialize();
    void shutdown();

    void handleCanvasSpawnerAndSelection(PhysicsWorld& world,
                                         Camera2DController& camera,
                                         SpawnerPreviewState& spawnerPreview);

    void renderUI(PhysicsWorld& world,
                  Camera2DController& camera,
                  Renderer& renderer,
                  SpawnerPreviewState& spawnerPreview);

    ScenarioPreset currentPreset = ScenarioPreset::ScaledSolarSystem;
    int selectedCatalogIndex = 9; // Default Terra (Earth)
    CatalogCategory activeCategory = CatalogCategory::All;
    int leftConsoleTab = 0;       // 0: Catalog, 1: Live Radar Tracker
    bool showEncyclopedia = true;
    bool showLab = true;
    bool showAdvancedSettings = false;

private:
    char m_editNameBuffer[64] = "";
    int m_lastEditedBodyId = -1;
    uint32_t m_spawnCounter = 1;

    std::deque<float> m_ekHistory;
    std::deque<float> m_epHistory;

    CelestialBody createPrototypeFromCatalog(int catalogIdx, const Vec2d& worldPos);

    void applySpaceTheme();
    void drawTopGameBar(PhysicsWorld& world, Camera2DController& camera, Renderer& renderer);
    void drawLeftEncyclopedia(PhysicsWorld& world, Camera2DController& camera, SpawnerPreviewState& spawnerPreview);
    void drawRightLabAndInspector(PhysicsWorld& world, Camera2DController& camera, Renderer& renderer);
    void drawSpacecraftFlightCard(PhysicsWorld& world, Camera2DController& camera);
    void drawAdvancedSettingsDrawer(PhysicsWorld& world, Camera2DController& camera, Renderer& renderer);
};

} // namespace sim
