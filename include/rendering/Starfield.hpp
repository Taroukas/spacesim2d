#pragma once

#include "core/Vec2d.hpp"
#include "rendering/Camera2DController.hpp"
#include <cstdint>
#include <raylib.h>
#include <vector>

namespace sim {

class Starfield {
public:
    Starfield();
    ~Starfield();

    // Procedurally generates star layers, radial glow, and spherical planet textures in RAM
    void initialize(int starCountPerLayer = 320);
    void unload();

    void renderBackground(const Camera2DController& camera, int screenWidth, int screenHeight, double timeSeconds) const;

    Texture2D getGlowTexture() const { return m_glowTexture; }
    Texture2D getTerranTexture(uint32_t seed) const { return m_terranTextures[seed % 4]; }
    Texture2D getCloudTexture() const { return m_cloudTexture; }
    Texture2D getGasGiantTexture(uint32_t seed) const { return m_gasGiantTextures[seed % 4]; }
    Texture2D getCrateredTexture(uint32_t seed) const { return m_crateredTextures[seed % 4]; }
    Texture2D getLavaTexture() const { return m_lavaTexture; }

    // Seamless noise textures for the GLSL 3D Spherical Normal-Mapping & Lighting Shader
    Texture2D getRockyNoiseTexture() const { return m_texRockyNoise; }
    Texture2D getGasNoiseTexture() const { return m_texGasNoise; }
    Texture2D getStarPlasmaTexture() const { return m_texStarPlasma; }
    Texture2D getDeepNebulaTexture() const { return m_texDeepNebula; }

private:
    struct BackgroundStar {
        float normX = 0.0f;
        float normY = 0.0f;
        float size = 1.5f;
        float twinklePhase = 0.0f;
        float twinkleSpeed = 1.5f;
        int layer = 0;
        Color baseColor = WHITE;
    };

    std::vector<BackgroundStar> m_stars;
    Texture2D m_glowTexture{};
    Texture2D m_terranTextures[4]{};
    Texture2D m_cloudTexture{};
    Texture2D m_gasGiantTextures[4]{};
    Texture2D m_crateredTextures[4]{};
    Texture2D m_lavaTexture{};
    Texture2D m_texRockyNoise{};
    Texture2D m_texGasNoise{};
    Texture2D m_texStarPlasma{};
    Texture2D m_texDeepNebula{};
    bool m_initialized = false;

    void generateProceduralGlowTexture();
    void generateProceduralPlanetTextures();
    void generateShaderNoiseTextures();

    static float hash2D(int x, int y, uint32_t seed);
    static float valueNoise2D(float x, float y, uint32_t seed);
    static float fbm2D(float x, float y, uint32_t seed, int octaves = 4);
};

} // namespace sim
