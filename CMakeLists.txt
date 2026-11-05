cmake_minimum_required(VERSION 3.15...3.30)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(SpaceSimulator VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure _USE_MATH_DEFINES is set on MSVC/Windows and warnings are clean
add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)

include(FetchContent)

# 1. Fetch Raylib 5.0 (strictly pinned release tag)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# 2. Fetch Dear ImGui v1.90.4 (strictly pinned release tag)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# Compile Dear ImGui core sources into a static library
add_library(imgui_core STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui_core PUBLIC ${imgui_SOURCE_DIR})
target_compile_features(imgui_core PUBLIC cxx_std_20)

# 3. SpaceSimulator Executable
set(SIMULATOR_SOURCES
    src/main.cpp
    src/physics/BarnesHutTree.cpp
    src/physics/Integrator.cpp
    src/physics/CollisionSystem.cpp
    src/physics/PhysicsWorld.cpp
    src/rendering/Camera2DController.cpp
    src/rendering/SpacetimeGrid.cpp
    src/rendering/Starfield.cpp
    src/rendering/Renderer.cpp
    src/scenarios/PresetManager.cpp
    src/ui/UIManager.cpp
    src/ui/ImGuiRaylibBackend.cpp
)

add_executable(SpaceSimulator ${SIMULATOR_SOURCES})

target_include_directories(SpaceSimulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SpaceSimulator PRIVATE
    raylib
    imgui_core
)

if (MSVC)
    target_compile_options(SpaceSimulator PRIVATE /W3 /fp:precise $<$<NOT:$<CONFIG:Debug>>:/O2>)
else()
    target_compile_options(SpaceSimulator PRIVATE -Wall -Wextra $<$<NOT:$<CONFIG:Debug>>:-O3>)
endif()
