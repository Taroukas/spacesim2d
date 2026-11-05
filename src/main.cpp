#include "core/Config.hpp"
#include "physics/PhysicsWorld.hpp"
#include "rendering/Camera2DController.hpp"
#include "rendering/Renderer.hpp"
#include "scenarios/PresetManager.hpp"
#include "ui/UIManager.hpp"
#include <imgui.h>
#include <raylib.h>

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(sim::Config::DEFAULT_SCREEN_WIDTH,
               sim::Config::DEFAULT_SCREEN_HEIGHT,
               "2D Space & Astrophysics Simulator - Universe Sandbox Edition");
    SetExitKey(KEY_NULL);
    SetTargetFPS(sim::Config::TARGET_FPS);

    sim::PhysicsWorld world;
    sim::Camera2DController camera(GetScreenWidth(), GetScreenHeight());
    sim::Renderer renderer;
    sim::UIManager ui;
    sim::SpawnerPreviewState spawnerPreview;

    renderer.initialize();
    ui.initialize();

    // Load default Scaled Solar System scenario at startup
    sim::PresetManager::loadPreset(sim::ScenarioPreset::ScaledSolarSystem, world, camera);

    while (!WindowShouldClose()) {
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();
        camera.onResize(screenW, screenH);

        const double frameDt = static_cast<double>(GetFrameTime());
        const double elapsedSeconds = GetTime();

        const ImGuiIO& io = ImGui::GetIO();
        const bool uiCapturesMouse = io.WantCaptureMouse;
        const bool uiCapturesKeyboard = io.WantCaptureKeyboard;

        // 1. Route input to Camera, Spawner Tool, and Physics World
        ui.handleCanvasSpawnerAndSelection(world, camera, spawnerPreview);
        world.update(frameDt, uiCapturesKeyboard);
        camera.update(frameDt, world, uiCapturesMouse, uiCapturesKeyboard);

        // 2. Render Simulation & UI
        BeginDrawing();

        renderer.render(world, camera, spawnerPreview, screenW, screenH, elapsedSeconds);
        ui.renderUI(world, camera, renderer, spawnerPreview);

        EndDrawing();
    }

    ui.shutdown();
    renderer.unload();
    CloseWindow();

    return 0;
}
