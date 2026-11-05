#include "ui/UIManager.hpp"
#include "ui/ImGuiRaylibBackend.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>

namespace sim {

UIManager::UIManager() = default;

UIManager::~UIManager() = default;

void UIManager::initialize() {
    imgui_backend::setup(true);
    applySpaceTheme();
}

void UIManager::shutdown() {
    imgui_backend::shutdown();
}

void UIManager::applySpaceTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 10.0f;
    style.ChildRounding     = 7.0f;
    style.FrameRounding     = 5.0f;
    style.GrabRounding      = 5.0f;
    style.PopupRounding     = 7.0f;
    style.ScrollbarRounding = 7.0f;
    style.WindowBorderSize  = 1.5f;
    style.FrameBorderSize   = 1.0f;
    style.FramePadding      = ImVec2(8.0f, 6.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.WindowPadding     = ImVec2(10.0f, 10.0f);

    // Spaceship Bridge & Control Board Theme: Deep Tinted Carbon Glass & Cyber Cyan Neon Trims
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(9.0f / 255.0f, 13.0f / 255.0f, 24.0f / 255.0f, 235.0f / 255.0f);
    colors[ImGuiCol_ChildBg]              = ImVec4(11.0f / 255.0f, 16.0f / 255.0f, 28.0f / 255.0f, 220.0f / 255.0f);
    colors[ImGuiCol_PopupBg]              = ImVec4(12.0f / 255.0f, 18.0f / 255.0f, 32.0f / 255.0f, 245.0f / 255.0f);
    colors[ImGuiCol_Border]               = ImVec4(0.00f / 255.0f, 215.0f / 255.0f, 255.0f / 255.0f, 175.0f / 255.0f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(14.0f / 255.0f, 24.0f / 255.0f, 44.0f / 255.0f, 220.0f / 255.0f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(20.0f / 255.0f, 42.0f / 255.0f, 76.0f / 255.0f, 245.0f / 255.0f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(26.0f / 255.0f, 58.0f / 255.0f, 102.0f / 255.0f, 255.0f / 255.0f);
    colors[ImGuiCol_TitleBg]              = ImVec4(8.0f / 255.0f, 12.0f / 255.0f, 20.0f / 255.0f, 240.0f / 255.0f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(12.0f / 255.0f, 24.0f / 255.0f, 44.0f / 255.0f, 255.0f / 255.0f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.00f, 0.95f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.00f, 0.85f, 1.00f, 0.95f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.40f, 0.95f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(16.0f / 255.0f, 34.0f / 255.0f, 62.0f / 255.0f, 230.0f / 255.0f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(24.0f / 255.0f, 58.0f / 255.0f, 102.0f / 255.0f, 255.0f / 255.0f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.00f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(18.0f / 255.0f, 38.0f / 255.0f, 68.0f / 255.0f, 220.0f / 255.0f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(26.0f / 255.0f, 54.0f / 255.0f, 96.0f / 255.0f, 255.0f / 255.0f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(32.0f / 255.0f, 72.0f / 255.0f, 128.0f / 255.0f, 255.0f / 255.0f);
    colors[ImGuiCol_Separator]            = ImVec4(0.00f, 0.85f, 1.00f, 0.40f);
    colors[ImGuiCol_PlotLines]            = ImVec4(0.00f, 0.95f, 0.80f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]     = ImVec4(1.00f, 0.80f, 0.25f, 1.00f);
}

CelestialBody UIManager::createPrototypeFromCatalog(int catalogIdx, const Vec2d& worldPos) {
    CelestialBody proto = StellarCatalog::createFromCatalog(catalogIdx, worldPos, { 0.0, 0.0 });
    proto.id = 99999998u;
    return proto;
}

void UIManager::handleCanvasSpawnerAndSelection(PhysicsWorld& world,
                                                Camera2DController& camera,
                                                SpawnerPreviewState& spawnerPreview) {
    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureKeyboard) {
        if (IsKeyPressed(KEY_SPACE)) {
            world.paused = !world.paused;
        }
        if (IsKeyPressed(KEY_G)) {
            PresetManager::generateProceduralStarSystem(world, camera);
        }
        if (IsKeyPressed(KEY_F) && world.selectedBodyId != -1) {
            if (CelestialBody* b = world.findBodyById(world.selectedBodyId)) {
                world.cameraFollowId = static_cast<int>(b->id);
                world.trackBarycenter = false;
                camera.focusOnPosition(b->position);
            }
        }
        if (IsKeyPressed(KEY_O) && world.selectedBodyId != -1) {
            world.setCircularOrbitAroundNearest(world.selectedBodyId, false);
        }
        if (IsKeyPressed(KEY_M)) {
            world.spawnMeteorBarrage(world.selectedBodyId);
        }
        if (IsKeyPressed(KEY_B)) {
            world.trackBarycenter = !world.trackBarycenter;
        }
        if (IsKeyPressed(KEY_P)) {
            world.spacecraft.enabled = !world.spacecraft.enabled;
            if (world.spacecraft.enabled) {
                world.cameraFollowId = -2;
            } else if (world.cameraFollowId == -2) {
                world.cameraFollowId = -1;
            }
        }
        if (IsKeyPressed(KEY_L)) {
            if (world.spacecraft.enabled) {
                world.cameraFollowId = -2;
                world.trackBarycenter = false;
                camera.focusOnPosition(world.spacecraft.position);
            }
        }
        if (IsKeyPressed(KEY_DELETE) && world.selectedBodyId != -1) {
            world.deleteBody(world.selectedBodyId);
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            world.selectedBodyId = -1;
            spawnerPreview.active = false;
        }
    }

    if (io.WantCaptureMouse) {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            spawnerPreview.active = false;
        }
        return;
    }

    Vector2 mouseScreen = GetMousePosition();
    Vec2d mouseWorld = camera.screenToWorld(mouseScreen);

    // Right-Click while dragging snaps & spawns into a circular orbit around nearest attractor
    if (spawnerPreview.active && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (const CelestialBody* dom = world.findNearestStarOrHeavyBody(spawnerPreview.spawnWorldPos)) {
            Vec2d rVec = spawnerPreview.spawnWorldPos - dom->position;
            double r = std::max(1.0, rVec.length());
            Vec2d tang = rVec.normalized().perpendicular();
            double vCirc = std::sqrt((world.G * (dom->mass + spawnerPreview.prototypeBody.mass)) / r);
            spawnerPreview.prototypeBody.velocity = dom->velocity + tang * vCirc;
        }
        CelestialBody spawned = spawnerPreview.prototypeBody;
        spawned.id = 0;
        CelestialBody& added = world.addBody(spawned);
        world.selectedBodyId = static_cast<int>(added.id);
        ++m_spawnCounter;
        spawnerPreview.active = false;
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int clickedId = -1;
        double minPickDistSq = 1e30;

        for (const auto& b : world.bodies) {
            if (!b.alive) continue;
            double pickRad = std::max(b.radius * 1.15, 12.0 / camera.zoom);
            double dSq = (b.position - mouseWorld).lengthSq();
            if (dSq <= pickRad * pickRad && dSq < minPickDistSq) {
                minPickDistSq = dSq;
                clickedId = static_cast<int>(b.id);
            }
        }

        if (clickedId != -1) {
            world.selectedBodyId = clickedId;
            spawnerPreview.active = false;
        } else {
            spawnerPreview.active = true;
            spawnerPreview.spawnWorldPos = mouseWorld;
            spawnerPreview.launchVelocity = { 0.0, 0.0 };
            spawnerPreview.prototypeBody = createPrototypeFromCatalog(selectedCatalogIndex, mouseWorld);
        }
    }

    if (spawnerPreview.active && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.001f) {
            spawnerPreview.prototypeBody.mass = std::clamp(
                spawnerPreview.prototypeBody.mass * std::exp(static_cast<double>(wheel) * 0.25), 0.001, 150000.0);
            spawnerPreview.prototypeBody.recomputeRadiusFromMass();
        }

        Vec2d dragVec = spawnerPreview.spawnWorldPos - mouseWorld;
        Vec2d launchVel = dragVec * 0.65;

        spawnerPreview.circularSnapActive = false;
        if (IsKeyDown(KEY_LEFT_SHIFT) || dragVec.lengthSq() < (12.0 / (camera.zoom * camera.zoom))) {
            if (const CelestialBody* dom = world.findNearestStarOrHeavyBody(spawnerPreview.spawnWorldPos)) {
                Vec2d rVec = spawnerPreview.spawnWorldPos - dom->position;
                double r = rVec.length();
                if (r > dom->radius + 2.0) {
                    Vec2d tang = rVec.normalized().perpendicular();
                    if (dragVec.lengthSq() > 1.0 && dragVec.dot(tang) < 0.0) {
                        tang = -tang;
                    }
                    double vCirc = std::sqrt((world.G * (dom->mass + spawnerPreview.prototypeBody.mass)) / r);
                    launchVel = dom->velocity + tang * vCirc;
                    spawnerPreview.circularSnapActive = true;
                }
            }
        }

        spawnerPreview.launchVelocity = launchVel;
        spawnerPreview.prototypeBody.velocity = launchVel;

        spawnerPreview.trajectory = world.computePredictedTrajectory(
            spawnerPreview.prototypeBody,
            world.predictionSteps,
            world.predictionStepDt,
            world.referenceFrameBodyId);
    }

    if (spawnerPreview.active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        CelestialBody spawned = spawnerPreview.prototypeBody;
        spawned.id = 0;
        CelestialBody& added = world.addBody(spawned);
        world.selectedBodyId = static_cast<int>(added.id);
        ++m_spawnCounter;
        spawnerPreview.active = false;
    }
}

void UIManager::renderUI(PhysicsWorld& world,
                         Camera2DController& camera,
                         Renderer& renderer,
                         SpawnerPreviewState& spawnerPreview) {
    (void)spawnerPreview;

    // Record energy history for diagnostic oscilloscope
    m_ekHistory.push_back(static_cast<float>(world.diagnostics.kineticEnergy));
    m_epHistory.push_back(static_cast<float>(std::abs(world.diagnostics.potentialEnergy)));
    if (m_ekHistory.size() > 140) {
        m_ekHistory.pop_front();
        m_epHistory.pop_front();
    }

    imgui_backend::beginFrame();

    drawTopGameBar(world, camera, renderer);

    if (showEncyclopedia) {
        drawLeftEncyclopedia(world, camera, spawnerPreview);
    }

    if (showLab) {
        drawRightLabAndInspector(world, camera, renderer);
    }

    if (world.spacecraft.enabled) {
        drawSpacecraftFlightCard(world, camera);
    }

    if (showAdvancedSettings) {
        drawAdvancedSettingsDrawer(world, camera, renderer);
    }

    imgui_backend::endFrame();
}

void UIManager::drawTopGameBar(PhysicsWorld& world,
                               Camera2DController& camera,
                               Renderer& renderer) {
    const float screenW = static_cast<float>(GetScreenWidth());
    // Auto-size width up to 1040px so all controls fit on 1 row without any overflow
    const float barWidth = std::min(screenW - 24.0f, 1040.0f);

    ImGui::SetNextWindowPos(ImVec2((screenW - barWidth) * 0.5f, 6.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(barWidth, 46.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##TopGameBar", nullptr, flags)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

        // 1. Play / Pause Button with illuminated indicator
        if (world.paused) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.55f, 0.32f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.72f, 0.42f, 1.00f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.28f, 0.50f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.42f, 0.70f, 1.00f));
        }
        if (ImGui::Button(world.paused ? " > PLAY " : " || PAUSE ")) {
            world.paused = !world.paused;
        }
        ImGui::PopStyleColor(2);

        // 2. Warp Speed Pills: [0.2x] [1x] [5x] [25x] [100x]
        const double speeds[5] = { 0.25, 1.0, 5.0, 25.0, 100.0 };
        const char* speedLabels[5] = { "0.2x", "1x", "5x", "25x", "100x" };

        for (int i = 0; i < 5; ++i) {
            ImGui::SameLine();
            bool active = (!world.paused && std::abs(world.timeScale - speeds[i]) < 0.05);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.68f, 0.95f, 1.00f));
            }
            if (ImGui::Button(speedLabels[i])) {
                world.timeScale = speeds[i];
                world.paused = false;
            }
            if (active) {
                ImGui::PopStyleColor();
            }
        }

        // 3. Scenario Presets Dropdown
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::SetNextItemWidth(175.0f);
        int presetIdx = static_cast<int>(currentPreset);
        const char* presetNames[static_cast<int>(ScenarioPreset::COUNT)];
        for (int i = 0; i < static_cast<int>(ScenarioPreset::COUNT); ++i) {
            presetNames[i] = PresetManager::getPresetName(static_cast<ScenarioPreset>(i));
        }
        if (ImGui::Combo("##ScenarioDropdown", &presetIdx, presetNames, static_cast<int>(ScenarioPreset::COUNT))) {
            currentPreset = static_cast<ScenarioPreset>(presetIdx);
            PresetManager::loadPreset(currentPreset, world, camera);
        }

        // 4. Quick Toggle Pill Helper (compact, never overflows)
        auto drawTogglePill = [](const char* label, const char* tooltip, bool& state) {
            ImGui::SameLine();
            if (state) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.46f, 0.70f, 0.95f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.12f, 0.20f, 0.85f));
            }
            if (ImGui::Button(label)) {
                state = !state;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s: %s", tooltip, state ? "ON" : "OFF");
            }
            ImGui::PopStyleColor();
        };

        drawTogglePill("CATALOG", "Toggle Navigation Catalog", showEncyclopedia);
        drawTogglePill("SENSORS", "Toggle Astrophysics Sensors", showLab);
        drawTogglePill("GRID", "Toggle Spacetime Gravity Grid", renderer.spacetimeGrid.enabled);
        drawTogglePill("TRAILS", "Toggle Orbital Trails", world.showTrails);
        drawTogglePill("LENS", "Toggle Einstein Lensing & Bloom", renderer.showBloom);
        drawTogglePill("RADAR", "Toggle Tactical Radar", renderer.showRadar);

        // Cosmic Scale Mode Pill (Logarithmic Hybrid vs Vast Linear)
        ImGui::SameLine();
        if (Config::use_logarithmic_scale) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06f, 0.46f, 0.70f, 0.95f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.16f, 0.58f, 0.95f));
        }
        if (ImGui::Button(Config::use_logarithmic_scale ? "LOG" : "VAST")) {
            world.applyCosmicScaleMode(!Config::use_logarithmic_scale);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Cosmic Scale Mode: %s", Config::use_logarithmic_scale ? "Hybrid Logarithmic AU" : "Vast Linear");
        }
        ImGui::PopStyleColor();

        // Pilot Ship Pill
        ImGui::SameLine();
        bool shipActive = world.spacecraft.enabled;
        if (shipActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.65f, 0.58f, 0.95f));
        }
        if (ImGui::Button("PILOT")) {
            if (!world.spacecraft.enabled) {
                Vec2d spawnPos = camera.position;
                Vec2d spawnVel{ 0.0, 0.0 };
                if (const CelestialBody* dom = world.findNearestStarOrHeavyBody(spawnPos)) {
                    spawnPos = dom->position + Vec2d(dom->radius + 26.0, 0.0);
                    double vCirc = std::sqrt((world.G * dom->mass) / (dom->radius + 26.0));
                    spawnVel = dom->velocity + Vec2d(0.0, vCirc);
                }
                world.spacecraft.reset(spawnPos, spawnVel, M_PI * 0.5);
                world.cameraFollowId = -2;
            } else {
                world.spacecraft.enabled = false;
                if (world.cameraFollowId == -2) world.cameraFollowId = -1;
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Pilot Explorer-1 Spaceship [P]");
        }
        if (shipActive) {
            ImGui::PopStyleColor();
        }

        drawTogglePill("CONFIG", "Toggle Advanced Simulation Settings", showAdvancedSettings);

        ImGui::PopStyleVar(2);
    }
    ImGui::End();
}

void UIManager::drawLeftEncyclopedia(PhysicsWorld& world,
                                     Camera2DController& camera,
                                     SpawnerPreviewState& spawnerPreview) {
    (void)spawnerPreview;
    const float screenH = static_cast<float>(GetScreenHeight());
    const float panelW = 330.0f;
    const float panelH = screenH - 74.0f;

    ImGui::SetNextWindowPos(ImVec2(10.0f, 64.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##LeftEncyclopedia", nullptr, flags)) {
        // Cockpit Subsystem Header (Compact, Zero Overlap!)
        ImGui::TextColored(ImVec4(0.0f, 0.95f, 1.0f, 1.0f), "NAV COMPUTER");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65.0f);
        ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.4f, 1.0f), "[● ONLINE]");
        ImGui::Separator();

        // Dual Subsystem Switcher: [ CATALOG (35) ] vs [ RADAR TRACKER ]
        const float tabBtnW = (ImGui::GetContentRegionAvail().x - 6.0f) * 0.5f;
        if (leftConsoleTab == 0) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.60f, 0.88f, 1.0f));
        if (ImGui::Button("CATALOG (35)", ImVec2(tabBtnW, 25.0f))) leftConsoleTab = 0;
        if (leftConsoleTab == 0) ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 6.0f);

        char radarTabLabel[64];
        std::snprintf(radarTabLabel, sizeof(radarTabLabel), "RADAR (%zu)", world.bodies.size());
        if (leftConsoleTab == 1) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.60f, 0.88f, 1.0f));
        if (ImGui::Button(radarTabLabel, ImVec2(tabBtnW, 25.0f))) leftConsoleTab = 1;
        if (leftConsoleTab == 1) ImGui::PopStyleColor();

        ImGui::Spacing();

        if (leftConsoleTab == 0) {
            // 6 Category Tabs: 2 rows of 3 buttons with tight padding so nothing clips
            struct CatTab { CatalogCategory cat; const char* name; };
            const CatTab catTabs[6] = {
                { CatalogCategory::All,                  "All" },
                { CatalogCategory::MoonsAndPlanets,      "Planets" },
                { CatalogCategory::Asteroids,            "Asteroids" },
                { CatalogCategory::Stars,                "Stars" },
                { CatalogCategory::Giants,               "Giants" },
                { CatalogCategory::CompactAndBlackHoles, "Black Holes" }
            };

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 3.0f));
            const float filterBtnW = (ImGui::GetContentRegionAvail().x - 8.0f) / 3.0f;
            for (int i = 0; i < 6; ++i) {
                if (i > 0 && i % 3 != 0) ImGui::SameLine(0.0f, 4.0f);
                bool isActive = (activeCategory == catTabs[i].cat);
                if (isActive) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.65f, 0.90f, 1.00f));
                }
                if (ImGui::Button(catTabs[i].name, ImVec2(filterBtnW, 22.0f))) {
                    activeCategory = catTabs[i].cat;
                }
                if (isActive) {
                    ImGui::PopStyleColor();
                }
            }
            ImGui::PopStyleVar();

            ImGui::Spacing();

            // Smooth Scrollable Catalog List (Height dynamically respects remaining bottom space)
            const auto& catalog = StellarCatalog::getEntries();
            std::vector<int> filteredIndices;
            for (const auto& entry : catalog) {
                if (activeCategory == CatalogCategory::All || entry.category == activeCategory) {
                    filteredIndices.push_back(entry.index);
                }
            }

            // Reserve exactly 126px for the armed card + buttons at the bottom
            float catalogListH = ImGui::GetContentRegionAvail().y - 126.0f;
            if (catalogListH < 120.0f) catalogListH = 120.0f;

            ImGui::BeginChild("##CatalogCardList", ImVec2(-1.0f, catalogListH), true);
            ImDrawList* dl = ImGui::GetWindowDrawList();

            for (int idx : filteredIndices) {
                const auto& entry = catalog[idx];
                bool isSelected = (selectedCatalogIndex == entry.index);
                const float cardH = 50.0f;

                ImGui::PushID(entry.index);
                bool clicked = ImGui::InvisibleButton("##CardBtn", ImVec2(-1.0f, cardH));
                bool isHovered = ImGui::IsItemHovered();
                if (clicked) {
                    selectedCatalogIndex = entry.index;
                }
                ImGui::PopID();

                // Exact bounding box of the card (guarantees zero scrollbar/cursor desync!)
                ImVec2 pMin = ImGui::GetItemRectMin();
                ImVec2 pMax = ImGui::GetItemRectMax();

                // Card background & borders
                ImU32 bgCol = isSelected ? IM_COL32(18, 48, 84, 245)
                                         : (isHovered ? IM_COL32(22, 36, 60, 230) : IM_COL32(13, 17, 28, 215));
                ImU32 borderCol = isSelected ? IM_COL32(0, 225, 255, 255)
                                             : (isHovered ? IM_COL32(65, 150, 220, 180) : IM_COL32(34, 46, 72, 160));

                dl->AddRectFilled(pMin, pMax, bgCol, 6.0f);
                dl->AddRect(pMin, pMax, borderCol, 6.0f, 0, isSelected ? 2.0f : 1.0f);

                // Procedural Mini Celestial Preview
                ImVec2 previewCenter(pMin.x + 22.0f, pMin.y + 25.0f);
                ImU32 mainCol = IM_COL32(entry.color.r, entry.color.g, entry.color.b, entry.color.a);

                if (entry.bodyType == CelestialBodyType::BlackHole) {
                    dl->AddCircle(previewCenter, 13.0f, IM_COL32(255, 140, 40, 200), 24, 2.5f);
                    dl->AddCircle(previewCenter, 8.5f, IM_COL32(255, 230, 180, 255), 24, 1.5f);
                    dl->AddCircleFilled(previewCenter, 7.0f, IM_COL32(0, 0, 0, 255));
                } else if (entry.bodyType == CelestialBodyType::NeutronStar) {
                    dl->AddLine(ImVec2(previewCenter.x - 13, previewCenter.y - 13),
                                ImVec2(previewCenter.x + 13, previewCenter.y + 13), IM_COL32(100, 215, 255, 220), 2.0f);
                    dl->AddCircleFilled(previewCenter, 12.0f, IM_COL32(180, 90, 255, 110));
                    dl->AddCircleFilled(previewCenter, 5.0f, IM_COL32(240, 250, 255, 255));
                } else if (entry.bodyType == CelestialBodyType::YellowStar || entry.bodyType == CelestialBodyType::BlueHypergiant) {
                    dl->AddCircleFilled(previewCenter, 14.0f, IM_COL32(entry.color.r, entry.color.g, entry.color.b, 80));
                    dl->AddCircleFilled(previewCenter, 10.0f, IM_COL32(entry.color.r, entry.color.g, entry.color.b, 180));
                    dl->AddCircleFilled(previewCenter, 6.0f, IM_COL32(255, 255, 255, 255));
                } else if (entry.bodyType == CelestialBodyType::GasGiant) {
                    if (entry.hasRings) {
                        dl->AddCircle(previewCenter, 13.0f, IM_COL32(entry.ringColor.r, entry.ringColor.g, entry.ringColor.b, 180), 24, 2.0f);
                    }
                    dl->AddCircleFilled(previewCenter, 8.5f, mainCol);
                    dl->AddLine(ImVec2(previewCenter.x - 7, previewCenter.y - 2),
                                ImVec2(previewCenter.x + 7, previewCenter.y - 2), IM_COL32(140, 75, 45, 180), 1.8f);
                } else if (entry.bodyType == CelestialBodyType::RockyPlanet) {
                    dl->AddCircle(previewCenter, 8.5f, IM_COL32(100, 200, 255, 180), 20, 1.2f);
                    dl->AddCircleFilled(previewCenter, 7.5f, mainCol);
                    dl->AddCircleFilled(ImVec2(previewCenter.x - 2, previewCenter.y - 2), 2.8f, IM_COL32(60, 180, 90, 240));
                } else {
                    dl->AddCircleFilled(previewCenter, 6.0f, mainCol);
                    dl->AddCircleFilled(ImVec2(previewCenter.x - 1.5f, previewCenter.y - 1.0f), 1.8f, IM_COL32(110, 110, 120, 240));
                }

                // Title & Subtitle Labels (Nicely spaced, zero overlap!)
                dl->AddText(ImVec2(pMin.x + 44.0f, pMin.y + 5.0f),
                            isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(215, 230, 245, 255),
                            entry.name);
                dl->AddText(ImVec2(pMin.x + 44.0f, pMin.y + 21.0f),
                            IM_COL32(130, 180, 220, 255),
                            entry.subtitle);

                // Logarithmic Mass Progress Indicator (Cleanly positioned at bottom of card)
                float logM = std::clamp(static_cast<float>((std::log10(std::max(entry.massEarth, 0.00001)) + 5.0) / 21.0), 0.05f, 1.0f);
                float barW = (pMax.x - pMin.x - 54.0f) * logM;
                dl->AddRectFilled(ImVec2(pMin.x + 44.0f, pMin.y + 38.0f),
                                  ImVec2(pMin.x + 44.0f + barW, pMin.y + 41.0f),
                                  mainCol, 2.0f);
            }
            ImGui::EndChild();
        } else {
            // Orbit Radar Tracker List
            ImGui::TextColored(ImVec4(0.35f, 0.90f, 0.50f, 1.0f), "ACTIVE TRACKED BODIES (%zu)", world.bodies.size());
            float outlinerH = ImGui::GetContentRegionAvail().y - 126.0f;
            if (outlinerH < 120.0f) outlinerH = 120.0f;

            ImGui::BeginChild("##ActiveBodiesList", ImVec2(-1.0f, outlinerH), true);
            for (const auto& b : world.bodies) {
                if (!b.alive) continue;
                bool isBodySelected = (static_cast<int>(b.id) == world.selectedBodyId);

                ImGui::PushID(static_cast<int>(b.id));
                char rowLabel[128];
                double massEarth = b.mass / 10.0;
                std::snprintf(rowLabel, sizeof(rowLabel), "  %s (%.2f M_E)", b.name.c_str(), massEarth);
                if (ImGui::Selectable(rowLabel, isBodySelected)) {
                    world.selectedBodyId = static_cast<int>(b.id);
                    world.cameraFollowId = static_cast<int>(b.id);
                    world.trackBarycenter = false;
                    camera.focusOnPosition(b.position);
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Armed Payload / Spawner Brush Indicator (Fixed 126px guaranteed space, NEVER overflowed!)
        const auto& catalog = StellarCatalog::getEntries();
        int safeCatIdx = std::clamp(selectedCatalogIndex, 0, static_cast<int>(catalog.size()) - 1);
        const auto& equipped = catalog[safeCatIdx];

        ImGui::TextColored(ImVec4(0.0f, 0.92f, 1.0f, 1.0f), "[● ARMED]");
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.90f, 0.45f, 1.0f), "%s", equipped.name);
        ImGui::Text("Mass: %.2f M_E  •  Radius: %.0f km", equipped.massEarth, equipped.radiusEarth * 6371.0);
        ImGui::TextColored(ImVec4(0.60f, 0.85f, 1.0f, 0.85f), "Click-drag to launch. [Shift] locks orbit.");

        ImGui::Spacing();

        // Side-by-Side Tactical Deploy Buttons (Saves vertical space and fits on any screen!)
        const float halfBtnW = (ImGui::GetContentRegionAvail().x - 6.0f) * 0.5f;
        if (ImGui::Button("+ Asteroids", ImVec2(halfBtnW, 25.0f))) {
            world.spawnAsteroidBelt(world.selectedBodyId, 160);
        }
        ImGui::SameLine(0.0f, 6.0f);
        if (ImGui::Button("Procedural [G]", ImVec2(halfBtnW, 25.0f))) {
            PresetManager::generateProceduralStarSystem(world, camera);
        }
    }
    ImGui::End();
}

void UIManager::drawRightLabAndInspector(PhysicsWorld& world,
                                        Camera2DController& camera,
                                        Renderer& renderer) {
    (void)renderer;
    const float screenW = static_cast<float>(GetScreenWidth());
    const float screenH = static_cast<float>(GetScreenHeight());
    const float panelW = 330.0f;
    const float panelH = screenH - 74.0f;

    ImGui::SetNextWindowPos(ImVec2(screenW - panelW - 10.0f, 64.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##RightLabAndInspector", nullptr, flags)) {
        // Upper Console: Astrophysics Sensors (Compact, Zero Overlap!)
        ImGui::TextColored(ImVec4(0.0f, 0.95f, 1.0f, 1.0f), "ASTROPHYSICS SENSORS");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65.0f);
        ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.4f, 1.0f), "[● ONLINE]");
        ImGui::Separator();

        double ek = world.diagnostics.kineticEnergy;
        double ep = std::abs(world.diagnostics.potentialEnergy);
        double virial = (ep > 1e-4) ? ((2.0 * ek) / ep) : 0.0;

        double totalMassSim = 0.0;
        for (const auto& b : world.bodies) {
            if (b.alive) totalMassSim += b.mass;
        }
        double totalMassSun = totalMassSim / 7500.0;
        double totalMassEarth = totalMassSim / 10.0;

        // Clean Two-Line Energy & Mass Telemetry (Never clips off-screen!)
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.40f, 1.0f), "System Mass: %.2e M_Sun (%.2e M_E)", totalMassSun, totalMassEarth);
        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.55f, 1.0f), "Ek: +%.2e J", ek);
        ImGui::SameLine(150.0f);
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "|Ep|: %.2e J", ep);

        // Virial Ratio & Status Badge
        ImGui::Text("Virial Ratio : %.2f", virial);
        ImGui::SameLine(180.0f);
        if (std::abs(virial - 1.0) < 0.25) {
            ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.4f, 1.0f), "[EQUILIBRIUM]");
        } else if (virial < 0.75) {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "[COLLAPSE]");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "[UNBOUND]");
        }

        // Diagnostic Oscilloscope Trace
        ImGui::Spacing();
        ImVec2 oscPos = ImGui::GetCursorScreenPos();
        const float oscW = ImGui::GetContentRegionAvail().x;
        const float oscH = 46.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(oscPos, ImVec2(oscPos.x + oscW, oscPos.y + oscH), IM_COL32(7, 10, 18, 255), 5.0f);
        dl->AddRect(oscPos, ImVec2(oscPos.x + oscW, oscPos.y + oscH), IM_COL32(36, 52, 82, 255), 5.0f);
        dl->AddText(ImVec2(oscPos.x + 8.0f, oscPos.y + 3.0f), IM_COL32(140, 160, 180, 255), "OSCILLOSCOPE : Ek [Grn]  |  |Ep| [Blu]");

        size_t histCount = std::min(m_ekHistory.size(), m_epHistory.size());
        if (histCount > 2) {
            float maxVal = 100.0f;
            for (size_t i = 0; i < histCount; ++i) {
                maxVal = std::max({ maxVal, m_ekHistory[i], m_epHistory[i] });
            }
            for (size_t i = 1; i < histCount; ++i) {
                float x1 = oscPos.x + (static_cast<float>(i - 1) / 140.0f) * oscW;
                float x2 = oscPos.x + (static_cast<float>(i) / 140.0f) * oscW;

                float yEk1 = oscPos.y + oscH - 4.0f - (m_ekHistory[i - 1] / maxVal) * (oscH - 18.0f);
                float yEk2 = oscPos.y + oscH - 4.0f - (m_ekHistory[i] / maxVal) * (oscH - 18.0f);
                dl->AddLine(ImVec2(x1, yEk1), ImVec2(x2, yEk2), IM_COL32(80, 240, 150, 220), 1.5f);

                float yEp1 = oscPos.y + oscH - 4.0f - (m_epHistory[i - 1] / maxVal) * (oscH - 18.0f);
                float yEp2 = oscPos.y + oscH - 4.0f - (m_epHistory[i] / maxVal) * (oscH - 18.0f);
                dl->AddLine(ImVec2(x1, yEp1), ImVec2(x2, yEp2), IM_COL32(80, 175, 255, 220), 1.5f);
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(oscPos.x, oscPos.y + oscH + 6.0f));

        // Hertzsprung-Russell Diagram (T_eff vs Mass)
        ImVec2 hrPos = ImGui::GetCursorScreenPos();
        const float hrH = 50.0f;
        dl->AddRectFilled(hrPos, ImVec2(hrPos.x + oscW, hrPos.y + hrH), IM_COL32(8, 11, 20, 255), 5.0f);
        dl->AddRect(hrPos, ImVec2(hrPos.x + oscW, hrPos.y + hrH), IM_COL32(38, 54, 86, 255), 5.0f);
        dl->AddText(ImVec2(hrPos.x + 8.0f, hrPos.y + 3.0f), IM_COL32(0, 210, 255, 255), "H-R DIAGRAM (T_eff vs Mass)");

        for (const auto& b : world.bodies) {
            if (!b.alive || b.temperature < 800.0) continue;
            float normT = std::clamp(static_cast<float>((32000.0 - std::min(b.temperature, 32000.0)) / 30000.0), 0.05f, 0.95f);
            float logM = std::clamp(static_cast<float>((std::log10(std::max(b.mass, 0.1)) + 1.0) / 6.0), 0.08f, 0.92f);

            ImVec2 pt(hrPos.x + normT * oscW, hrPos.y + hrH - logM * (hrH - 16.0f));
            ImU32 starCol = IM_COL32(b.color.r, b.color.g, b.color.b, 255);
            dl->AddCircleFilled(pt, 4.5f, IM_COL32(b.color.r, b.color.g, b.color.b, 100));
            dl->AddCircleFilled(pt, 2.5f, starCol);

            if (static_cast<int>(b.id) == world.selectedBodyId) {
                dl->AddCircle(pt, 6.0f, IM_COL32(0, 255, 120, 255), 16, 1.8f);
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(hrPos.x, hrPos.y + hrH + 8.0f));
        ImGui::Separator();

        // Lower Console: Target Telemetry Scanner & Operations
        CelestialBody* sel = world.findBodyById(world.selectedBodyId);
        if (sel) {
            if (m_lastEditedBodyId != static_cast<int>(sel->id)) {
                std::snprintf(m_editNameBuffer, sizeof(m_editNameBuffer), "%s", sel->name.c_str());
                m_lastEditedBodyId = static_cast<int>(sel->id);
            }

            ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.4f, 1.0f), "TARGET SCANNER");
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 22.0f);
            if (ImGui::Button("X", ImVec2(22.0f, 20.0f))) {
                world.selectedBodyId = -1;
                ImGui::End();
                return;
            }

            ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "[ %s ]", sel->getTypeBadgeName());

            // Exact Real-World Astronomical Calculations
            double massEarth = sel->mass / 10.0;
            double massSun = massEarth / 332946.0;
            double radiusEarth = sel->radius / 6.4;
            double radiusKm = radiusEarth * 6371.0;
            double volEarth = radiusEarth * radiusEarth * radiusEarth;
            double gSurface = 9.80665 * (massEarth / std::max(radiusEarth * radiusEarth, 1e-6));
            double vEscape = 11.186 * std::sqrt(massEarth / std::max(radiusEarth, 1e-6));
            double rsKm = massEarth * 0.008870;

            ImGui::Text("Mass (Earth)      : %.3g M_E", massEarth);
            ImGui::Text("Mass (Solar)      : %.3g M_Sun", massSun);
            ImGui::Text("Exact Radius      : %.1f km (%.2f R_E)", radiusKm, radiusEarth);
            ImGui::Text("3D Volume         : %.3g x Earth", volEarth);
            ImGui::Text("Surface Gravity g : %.2f m/s²", gSurface);
            ImGui::Text("Escape Velocity ve: %.1f km/s", vEscape);
            ImGui::Text("Schwarzschild Rs  : %.3f km", rsKm);
            ImGui::Text("Surface Temp      : %.0f K", sel->temperature);

            ImGui::Spacing();
            ImGui::PushItemWidth(-90.0f);
            if (ImGui::InputText("Name", m_editNameBuffer, sizeof(m_editNameBuffer))) {
                sel->name = m_editNameBuffer;
            }

            float massF = static_cast<float>(sel->mass);
            if (ImGui::SliderFloat("Mass", &massF, 0.01f, 80000.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) {
                sel->mass = static_cast<double>(massF);
                sel->recomputeRadiusFromMass();
            }

            float radF = static_cast<float>(sel->radius);
            if (ImGui::SliderFloat("Radius", &radF, 1.5f, 85.0f, "%.1f")) {
                sel->radius = static_cast<double>(radF);
            }

            float tempF = static_cast<float>(sel->temperature);
            if (ImGui::SliderFloat("Temp (K)", &tempF, 30.0f, 35000.0f, "%.0f K", ImGuiSliderFlags_Logarithmic)) {
                sel->temperature = static_cast<double>(tempF);
            }
            ImGui::PopItemWidth();

            if (!sel->isLuminousStar() && !sel->isBlackHole()) {
                ImGui::Checkbox("Rings", &sel->hasRings);
                ImGui::SameLine();
            }
            ImGui::Checkbox("Anchor", &sel->isPinned);

            ImGui::Spacing();
            // Tactical Action Buttons in 2 Clean Columns
            const float halfW = (ImGui::GetContentRegionAvail().x - 6.0f) * 0.5f;
            bool isCamFollow = (world.cameraFollowId == static_cast<int>(sel->id));
            if (ImGui::Button(isCamFollow ? "Tracking" : "Focus (F)", ImVec2(halfW, 25.0f))) {
                world.cameraFollowId = static_cast<int>(sel->id);
                world.trackBarycenter = false;
                camera.focusOnPosition(sel->position);
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ImGui::Button("Auto-Orbit (O)", ImVec2(halfW, 25.0f))) {
                world.setCircularOrbitAroundNearest(static_cast<int>(sel->id), false);
            }

            if (ImGui::Button("Mass x2", ImVec2(halfW, 25.0f))) {
                sel->mass *= 2.0;
                sel->recomputeRadiusFromMass();
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ImGui::Button("Mass / 2", ImVec2(halfW, 25.0f))) {
                sel->mass = std::max(0.01, sel->mass * 0.5);
                sel->recomputeRadiusFromMass();
            }

            if (ImGui::Button("+ Heat (+2K)", ImVec2(halfW, 25.0f))) {
                sel->temperature = std::min(35000.0, sel->temperature + 2000.0);
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ImGui::Button("* Freeze (60K)", ImVec2(halfW, 25.0f))) {
                sel->temperature = 60.0;
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.22f, 0.22f, 1.00f));
            if (sel->isLuminousStar()) {
                if (ImGui::Button("DETONATE SUPERNOVA!", ImVec2(-1.0f, 28.0f))) {
                    world.triggerSupernova(static_cast<int>(sel->id));
                }
            } else {
                if (ImGui::Button("SHATTER WORLD", ImVec2(halfW, 28.0f))) {
                    world.splitSelectedBody(static_cast<int>(sel->id));
                }
                ImGui::SameLine(0.0f, 6.0f);
                if (ImGui::Button("DETONATE NOVA", ImVec2(halfW, 28.0f))) {
                    world.triggerSupernova(static_cast<int>(sel->id));
                }
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.85f, 1.0f), "No celestial target selected.");
            ImGui::TextWrapped("Click any celestial body in space or the Radar list to lock sensors.");
            ImGui::Spacing();
            ImGui::Text("Active Entities : %zu", world.bodies.size());
            ImGui::Text("Engine Rate     : %d FPS", GetFPS());
            ImGui::Text("Solver Step     : %.2f ms", world.diagnostics.physicsStepTimeMs);
            ImGui::Text("Energy Drift    : %+.4f %%", world.diagnostics.energyDriftPercent);
        }
    }
    ImGui::End();
}

void UIManager::drawSpacecraftFlightCard(PhysicsWorld& world, Camera2DController& camera) {
    if (!world.spacecraft.enabled) return;

    const float screenH = static_cast<float>(GetScreenHeight());
    const float cardW = 380.0f;
    const float cardH = 210.0f;

    // Placed on the lower-left flight quadrant so it never overlaps the center tactical radar
    ImGui::SetNextWindowPos(ImVec2(345.0f, screenH - cardH - 12.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##SpacecraftFlightCard", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.0f, 0.95f, 1.0f, 1.0f), "EXPLORER-1 FLIGHT DECK  [WASD Flight Controls]");

        ImGui::PushItemWidth(-120.0f);
        float throt = static_cast<float>(world.spacecraft.throttle);
        if (ImGui::SliderFloat("Throttle", &throt, 0.0f, 1.0f, "%.0f%%")) {
            world.spacecraft.throttle = static_cast<double>(throt);
        }
        ImGui::PopItemWidth();

        const OrbitalTelemetry& tel = world.spacecraft.telemetry;
        ImGui::Text("Attractor : %s  |  Speed : %.1f km/s", tel.dominantBodyName.c_str(), tel.relativeSpeed);
        if (tel.isBoundOrbit) {
            ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.4f, 1.0f), "Pe: %.0f  |  Ap: %.0f  |  e: %.3f (Bound Closed Orbit)", tel.periapsis, tel.apoapsis, tel.eccentricity);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.70f, 0.2f, 1.0f), "Pe: %.0f  |  e: %.3f (Escape Trajectory)", tel.periapsis, tel.eccentricity);
        }

        ImGui::Separator();
        const float halfW = (ImGui::GetContentRegionAvail().x - 6.0f) * 0.5f;

        bool progradeLocked = (world.spacecraft.sasMode == SASMode::Prograde);
        if (progradeLocked) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.68f, 0.85f, 1.0f));
        if (ImGui::Button("Lock Prograde", ImVec2(halfW, 26.0f))) {
            world.spacecraft.sasMode = progradeLocked ? SASMode::Off : SASMode::Prograde;
        }
        if (progradeLocked) ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 6.0f);

        bool retroLocked = (world.spacecraft.sasMode == SASMode::Retrograde);
        if (retroLocked) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.68f, 0.85f, 1.0f));
        if (ImGui::Button("Lock Retrograde", ImVec2(halfW, 26.0f))) {
            world.spacecraft.sasMode = retroLocked ? SASMode::Off : SASMode::Retrograde;
        }
        if (retroLocked) ImGui::PopStyleColor();

        if (ImGui::Button("Match Target Vel", ImVec2(halfW, 26.0f))) {
            world.spacecraft.matchTargetVelocity(world.bodies, world.selectedBodyId);
        }
        ImGui::SameLine(0.0f, 6.0f);
        if (ImGui::Button("Circularize Orbit", ImVec2(halfW, 26.0f))) {
            world.spacecraft.autoCircularizeOrbit(world.bodies, world.G, world.selectedBodyId);
            world.cameraFollowId = -2;
            camera.focusOnPosition(world.spacecraft.position);
        }
    }
    ImGui::End();
}

void UIManager::drawAdvancedSettingsDrawer(PhysicsWorld& world,
                                           Camera2DController& camera,
                                           Renderer& renderer) {
    ImGui::SetNextWindowPos(ImVec2(345.0f, 64.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 490.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Advanced Simulation & Engine Parameters", &showAdvancedSettings)) {
        ImGui::PushItemWidth(-160.0f);

        int solverIdx = static_cast<int>(world.solverType);
        const char* solverItems[] = { "Direct O(N^2) Symmetric", "Barnes-Hut Quadtree O(N log N)" };
        if (ImGui::Combo("Gravity Solver", &solverIdx, solverItems, 2)) {
            world.solverType = static_cast<GravitySolverType>(solverIdx);
        }

        int integIdx = static_cast<int>(world.integratorType);
        const char* integItems[] = { "Velocity Verlet (Symplectic)", "Runge-Kutta 4 (RK4)" };
        if (ImGui::Combo("Integrator", &integIdx, integItems, 2)) {
            world.integratorType = static_cast<IntegratorType>(integIdx);
        }

        int colIdx = static_cast<int>(world.collisionMode);
        const char* colItems[] = { "Inelastic (Merge)", "Elastic (Bounce)", "Hybrid (Accrete + Shatter)" };
        if (ImGui::Combo("Collision Mode", &colIdx, colItems, 3)) {
            world.collisionMode = static_cast<CollisionMode>(colIdx);
        }

        float gVal = static_cast<float>(world.G);
        if (ImGui::SliderFloat("Gravitational Const G", &gVal, 0.1f, 5000.0f, "%.1f", ImGuiSliderFlags_Logarithmic)) {
            world.G = static_cast<double>(gVal);
            world.computeEnergyAndMomentum(true);
        }

        ImGui::SliderFloat("2.5D Spacetime Warp", &renderer.spacetimeGrid.warpStrength, 0.2f, 3.0f, "%.2f");
        ImGui::SliderInt("Lookahead Steps", &world.predictionSteps, 100, 1500);

        ImGui::PopItemWidth();

        ImGui::Checkbox("Velocity Vectors", &renderer.showVelocityVectors);
        ImGui::SameLine();
        ImGui::Checkbox("Gravity Vectors", &renderer.showAccelerationVectors);

        ImGui::Checkbox("Quadtree Grid", &renderer.showQuadtreeDebug);
        ImGui::SameLine();
        ImGui::Checkbox("Body Labels", &renderer.showBodyLabels);

        bool logScale = Config::use_logarithmic_scale;
        if (ImGui::Checkbox("Logarithmic Cosmic Scale (AU Hybrid)", &logScale)) {
            world.applyCosmicScaleMode(logScale);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Barycenter [B]", &renderer.showBarycenter);

        if (ImGui::Button("Reset Camera (Home)", ImVec2(120.0f, 28.0f))) {
            world.cameraFollowId = -1;
            world.trackBarycenter = false;
            camera.resetView();
        }
        ImGui::SameLine();
        if (ImGui::Button("Meteor Barrage [M]", ImVec2(130.0f, 28.0f))) {
            world.spawnMeteorBarrage(world.selectedBodyId);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All", ImVec2(95.0f, 28.0f))) {
            world.clear();
        }

        ImGui::Separator();
        ImGui::Text("Performance : %d FPS  |  Physics Step: %.2f ms  |  Bodies: %zu",
                    GetFPS(), world.diagnostics.physicsStepTimeMs, world.bodies.size());
        ImGui::Text("Energy Conservation Drift (dE/E0): %+.4f %%", world.diagnostics.energyDriftPercent);

        if (!world.diagnostics.energyHistory.empty()) {
            float minE = *std::min_element(world.diagnostics.energyHistory.begin(), world.diagnostics.energyHistory.end());
            float maxE = *std::max_element(world.diagnostics.energyHistory.begin(), world.diagnostics.energyHistory.end());
            float span = std::max(1.0f, (maxE - minE) * 0.2f);
            ImGui::PlotLines("##EnergyGraph",
                             world.diagnostics.energyHistory.data(),
                             static_cast<int>(world.diagnostics.energyHistory.size()),
                             0,
                             "Mechanical Energy E(t)",
                             minE - span,
                             maxE + span,
                             ImVec2(-1.0f, 50.0f));
        }
    }
    ImGui::End();
}

} // namespace sim
