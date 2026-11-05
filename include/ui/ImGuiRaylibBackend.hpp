#pragma once

// Self-contained Dear ImGui (v1.90.x) platform + renderer backend for raylib 5.0.
// Replaces the external rlImGui dependency so the project never breaks when
// upstream rlImGui moves to a newer ImGui texture API.

namespace sim::imgui_backend {

// Creates the ImGui context, uploads the font atlas to the GPU and configures IO.
void setup(bool darkTheme = true);

// Feeds raylib input (mouse, keyboard, text, focus, display size) into ImGui and calls ImGui::NewFrame().
void beginFrame();

// Calls ImGui::Render() and draws the resulting draw lists using rlgl.
void endFrame();

// Releases GPU resources and destroys the ImGui context.
void shutdown();

} // namespace sim::imgui_backend
