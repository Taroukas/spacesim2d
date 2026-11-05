#include "rendering/Starfield.hpp"
#include "core/Config.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace sim {

Starfield::Starfield() = default;

Starfield::~Starfield() {
    unload();
}

float Starfield::hash2D(int x, int y, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
}

float Starfield::valueNoise2D(float x, float y, uint32_t seed) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(ix);
    float fy = y - static_cast<float>(iy);

    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fy * fy * (3.0f - 2.0f * fy);

    float n00 = hash2D(ix,     iy,     seed);
    float n10 = hash2D(ix + 1, iy,     seed);
    float n01 = hash2D(ix,     iy + 1, seed);
    float n11 = hash2D(ix + 1, iy + 1, seed);

    float nx0 = n00 + u * (n10 - n00);
    float nx1 = n01 + u * (n11 - n01);
    return nx0 + v * (nx1 - nx0);
}

float Starfield::fbm2D(float x, float y, uint32_t seed, int octaves) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float maxAmp = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        value += amplitude * valueNoise2D(x * frequency, y * frequency, seed + static_cast<uint32_t>(i * 101));
        maxAmp += amplitude;
        frequency *= 2.05f;
        amplitude *= 0.5f;
    }
    return value / std::max(1e-5f, maxAmp);
}

void Starfield::initialize(int starCountPerLayer) {
    if (m_initialized) return;

    generateProceduralGlowTexture();
    generateProceduralPlanetTextures();
    generateShaderNoiseTextures();

    std::mt19937 rng(42u);
    std::uniform_real_distribution<float> posDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> phaseDist(0.0f, 6.283185f);
    std::uniform_real_distribution<float> speedDist(0.8f, 3.2f);
    std::uniform_int_distribution<int> colorDist(0, 4);

    const Color spectralColors[5] = {
        { 165, 205, 255, 255 }, // O/B Blue-white
        { 240, 245, 255, 255 }, // A White
        { 255, 244, 214, 255 }, // G Yellow-white
        { 255, 205, 150, 255 }, // K Orange
        { 255, 175, 140, 255 }  // M Red dwarf
    };

    m_stars.clear();
    m_stars.reserve(static_cast<size_t>(starCountPerLayer * 3));

    for (int layer = 0; layer < 3; ++layer) {
        for (int i = 0; i < starCountPerLayer; ++i) {
            BackgroundStar s;
            s.normX = posDist(rng);
            s.normY = posDist(rng);
            s.layer = layer;
            s.size = (layer == 0) ? 1.0f : ((layer == 1) ? 1.5f : 2.2f);
            s.twinklePhase = phaseDist(rng);
            s.twinkleSpeed = speedDist(rng);
            s.baseColor = spectralColors[colorDist(rng)];
            m_stars.push_back(s);
        }
    }

    m_initialized = true;
}

void Starfield::unload() {
    if (!m_initialized) return;

    if (m_glowTexture.id != 0) UnloadTexture(m_glowTexture);
    if (m_cloudTexture.id != 0) UnloadTexture(m_cloudTexture);
    if (m_lavaTexture.id != 0) UnloadTexture(m_lavaTexture);
    if (m_texRockyNoise.id != 0) UnloadTexture(m_texRockyNoise);
    if (m_texGasNoise.id != 0) UnloadTexture(m_texGasNoise);
    if (m_texStarPlasma.id != 0) UnloadTexture(m_texStarPlasma);
    if (m_texDeepNebula.id != 0) UnloadTexture(m_texDeepNebula);

    for (int i = 0; i < 4; ++i) {
        if (m_terranTextures[i].id != 0) UnloadTexture(m_terranTextures[i]);
        if (m_gasGiantTextures[i].id != 0) UnloadTexture(m_gasGiantTextures[i]);
        if (m_crateredTextures[i].id != 0) UnloadTexture(m_crateredTextures[i]);
    }

    m_initialized = false;
}

void Starfield::generateProceduralGlowTexture() {
    constexpr int texSize = 128;
    Image img = GenImageColor(texSize, texSize, BLANK);

    const float center = (texSize - 1) * 0.5f;
    const float invMaxRadius = 1.0f / center;

    for (int y = 0; y < texSize; ++y) {
        for (int x = 0; x < texSize; ++x) {
            float dx = (static_cast<float>(x) - center) * invMaxRadius;
            float dy = (static_cast<float>(y) - center) * invMaxRadius;
            float r = std::sqrt(dx * dx + dy * dy);

            if (r >= 1.0f) {
                ImageDrawPixel(&img, x, y, Color{ 255, 255, 255, 0 });
            } else {
                float falloff = std::exp(-r * r * 4.2f) * (1.0f - r * r);
                unsigned char alpha = static_cast<unsigned char>(std::clamp(falloff * 255.0f, 0.0f, 255.0f));
                ImageDrawPixel(&img, x, y, Color{ 255, 255, 255, alpha });
            }
        }
    }

    m_glowTexture = LoadTextureFromImage(img);
    SetTextureFilter(m_glowTexture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
}

void Starfield::generateProceduralPlanetTextures() {
    constexpr int texSize = 128;
    const float center = (texSize - 1) * 0.5f;
    const float invR = 1.0f / center;

    // 1. Terran Planet Variants (Continents, Oceans, Polar Caps, Spherical Shading)
    for (int v = 0; v < 4; ++v) {
        uint32_t seed = 1000u + static_cast<uint32_t>(v * 337);
        Image img = GenImageColor(texSize, texSize, BLANK);

        for (int y = 0; y < texSize; ++y) {
            for (int x = 0; x < texSize; ++x) {
                float nx = (static_cast<float>(x) - center) * invR;
                float ny = (static_cast<float>(y) - center) * invR;
                float rSq = nx * nx + ny * ny;
                if (rSq > 1.0f) continue;

                float r = std::sqrt(rSq);
                float nz = std::sqrt(std::max(0.0f, 1.0f - rSq));

                // Spherical projection coordinates so continents wrap around curvature naturally
                float sx = nx / (nz + 0.35f);
                float sy = ny / (nz + 0.35f);

                float height = fbm2D(sx * 2.6f + 5.0f, sy * 2.6f + 5.0f, seed, 5);
                float moisture = fbm2D(sx * 3.5f + 12.0f, sy * 3.5f + 12.0f, seed + 55u, 3);
                float latitude = std::abs(ny) + 0.12f * (height - 0.5f);

                Color col;
                if (latitude > 0.82f) {
                    // Polar ice caps
                    col = Color{ 235, 245, 255, 255 };
                } else if (height < 0.45f) {
                    // Deep ocean to shallow coastal shelf
                    float depthT = std::clamp(height / 0.45f, 0.0f, 1.0f);
                    col = Color{
                        static_cast<unsigned char>(12 + depthT * 32),
                        static_cast<unsigned char>(50 + depthT * 95),
                        static_cast<unsigned char>(135 + depthT * 95),
                        255
                    };
                } else if (height < 0.48f) {
                    // Coastal sand / beach
                    col = Color{ 215, 198, 138, 255 };
                } else if (height < 0.72f) {
                    // Lush forest green vs savanna tan depending on moisture
                    float m = std::clamp(moisture, 0.0f, 1.0f);
                    col = Color{
                        static_cast<unsigned char>(155 - m * 105),
                        static_cast<unsigned char>(145 + m * 35),
                        static_cast<unsigned char>(70 - m * 15),
                        255
                    };
                } else if (height < 0.84f) {
                    // Rocky mountain highlands
                    col = Color{ 130, 110, 92, 255 };
                } else {
                    // Snow-capped mountain peaks
                    col = Color{ 240, 245, 250, 255 };
                }

                // 3D spherical limb darkening + smooth anti-aliased edge
                float limb = 0.42f + 0.58f * std::pow(nz, 0.65f);
                float edgeAlpha = std::clamp((1.0f - r) * 32.0f, 0.0f, 1.0f);

                col.r = static_cast<unsigned char>(std::clamp(col.r * limb, 0.0f, 255.0f));
                col.g = static_cast<unsigned char>(std::clamp(col.g * limb, 0.0f, 255.0f));
                col.b = static_cast<unsigned char>(std::clamp(col.b * limb, 0.0f, 255.0f));
                col.a = static_cast<unsigned char>(edgeAlpha * 255.0f);

                ImageDrawPixel(&img, x, y, col);
            }
        }

        m_terranTextures[v] = LoadTextureFromImage(img);
        SetTextureFilter(m_terranTextures[v], TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
    }

    // 2. Rotating Cloud Layer Texture
    {
        Image img = GenImageColor(texSize, texSize, BLANK);
        for (int y = 0; y < texSize; ++y) {
            for (int x = 0; x < texSize; ++x) {
                float nx = (static_cast<float>(x) - center) * invR;
                float ny = (static_cast<float>(y) - center) * invR;
                float rSq = nx * nx + ny * ny;
                if (rSq > 1.0f) continue;

                float r = std::sqrt(rSq);
                float nz = std::sqrt(std::max(0.0f, 1.0f - rSq));
                float sx = nx / (nz + 0.4f);
                float sy = ny / (nz + 0.4f);

                float swirl = fbm2D(sx * 3.2f, sy * 3.2f, 8888u, 4);
                float clouds = fbm2D(sx * 4.0f + swirl * 1.4f, sy * 4.0f - swirl * 1.2f, 9999u, 5);
                float density = std::clamp((clouds - 0.48f) * 2.8f, 0.0f, 1.0f);
                float edgeAlpha = std::clamp((1.0f - r) * 24.0f, 0.0f, 1.0f);

                unsigned char a = static_cast<unsigned char>(density * edgeAlpha * 185.0f);
                ImageDrawPixel(&img, x, y, Color{ 250, 253, 255, a });
            }
        }
        m_cloudTexture = LoadTextureFromImage(img);
        SetTextureFilter(m_cloudTexture, TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
    }

    // 3. Gas Giant Variants (Horizontal Turbulence Bands + Great Storm Spot)
    for (int v = 0; v < 4; ++v) {
        uint32_t seed = 3000u + static_cast<uint32_t>(v * 521);
        Image img = GenImageColor(texSize, texSize, BLANK);

        for (int y = 0; y < texSize; ++y) {
            for (int x = 0; x < texSize; ++x) {
                float nx = (static_cast<float>(x) - center) * invR;
                float ny = (static_cast<float>(y) - center) * invR;
                float rSq = nx * nx + ny * ny;
                if (rSq > 1.0f) continue;

                float r = std::sqrt(rSq);
                float nz = std::sqrt(std::max(0.0f, 1.0f - rSq));
                float sx = nx / (nz + 0.45f);
                float sy = ny / (nz + 0.45f);

                float turb = fbm2D(sx * 3.0f, sy * 6.0f, seed, 4) * 0.16f;
                float bandCoord = (ny + turb) * 9.5f;
                float bandWave = 0.5f + 0.5f * std::sin(bandCoord * 3.14159f);
                float fineBand = fbm2D(sx * 2.0f, bandCoord * 1.8f, seed + 17u, 3);

                float mixT = std::clamp(0.6f * bandWave + 0.4f * fineBand, 0.0f, 1.0f);

                // Great Storm Spot oval in southern hemisphere
                float stormDx = (nx - 0.22f) * 2.6f;
                float stormDy = (ny - 0.28f) * 4.2f;
                float stormDist = std::sqrt(stormDx * stormDx + stormDy * stormDy);
                float stormMask = std::clamp(1.0f - stormDist, 0.0f, 1.0f);

                Color c1{ 235, 195, 145, 255 };
                Color c2{ 185, 105,  60, 255 };
                Color stormCol{ 235,  85,  50, 255 };

                if (v == 1) { // Saturn gold/cream
                    c1 = { 245, 225, 170, 255 };
                    c2 = { 195, 165, 105, 255 };
                    stormCol = { 255, 240, 200, 255 };
                } else if (v == 2) { // Ice giant cyan
                    c1 = { 155, 235, 250, 255 };
                    c2 = {  75, 165, 215, 255 };
                    stormCol = { 220, 250, 255, 255 };
                } else if (v == 3) { // Deep Neptune azure
                    c1 = { 105, 155, 255, 255 };
                    c2 = {  35,  70, 185, 255 };
                    stormCol = { 190, 220, 255, 255 };
                }

                float rCol = c1.r + (c2.r - c1.r) * mixT;
                float gCol = c1.g + (c2.g - c1.g) * mixT;
                float bCol = c1.b + (c2.b - c1.b) * mixT;

                if (stormMask > 0.0f) {
                    float s = stormMask * stormMask;
                    rCol = rCol * (1.0f - s) + stormCol.r * s;
                    gCol = gCol * (1.0f - s) + stormCol.g * s;
                    bCol = bCol * (1.0f - s) + stormCol.b * s;
                }

                float limb = 0.38f + 0.62f * std::pow(nz, 0.7f);
                float edgeAlpha = std::clamp((1.0f - r) * 32.0f, 0.0f, 1.0f);

                Color out{
                    static_cast<unsigned char>(std::clamp(rCol * limb, 0.0f, 255.0f)),
                    static_cast<unsigned char>(std::clamp(gCol * limb, 0.0f, 255.0f)),
                    static_cast<unsigned char>(std::clamp(bCol * limb, 0.0f, 255.0f)),
                    static_cast<unsigned char>(edgeAlpha * 255.0f)
                };
                ImageDrawPixel(&img, x, y, out);
            }
        }

        m_gasGiantTextures[v] = LoadTextureFromImage(img);
        SetTextureFilter(m_gasGiantTextures[v], TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
    }

    // 4. Cratered Moon / Mercury Variants
    struct Crater {
        float cx, cy, rad;
    };

    for (int v = 0; v < 4; ++v) {
        uint32_t seed = 5000u + static_cast<uint32_t>(v * 613);
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> posD(-0.72f, 0.72f);
        std::uniform_real_distribution<float> radD(0.08f, 0.28f);

        std::vector<Crater> craters(18);
        for (auto& c : craters) {
            c.cx = posD(rng);
            c.cy = posD(rng);
            c.rad = radD(rng);
        }

        Image img = GenImageColor(texSize, texSize, BLANK);
        for (int y = 0; y < texSize; ++y) {
            for (int x = 0; x < texSize; ++x) {
                float nx = (static_cast<float>(x) - center) * invR;
                float ny = (static_cast<float>(y) - center) * invR;
                float rSq = nx * nx + ny * ny;
                if (rSq > 1.0f) continue;

                float r = std::sqrt(rSq);
                float nz = std::sqrt(std::max(0.0f, 1.0f - rSq));

                float regolith = fbm2D(nx * 4.5f + 3.0f, ny * 4.5f + 3.0f, seed, 4);
                float mare = fbm2D(nx * 1.8f + 7.0f, ny * 1.8f + 7.0f, seed + 99u, 3);
                float shade = 0.55f + 0.30f * regolith - (mare < 0.45f ? 0.14f : 0.0f);

                for (const auto& c : craters) {
                    float dx = nx - c.cx;
                    float dy = ny - c.cy;
                    float d = std::sqrt(dx * dx + dy * dy) / c.rad;
                    if (d < 1.18f) {
                        if (d < 0.82f) {
                            // Darkened crater floor + directional inner wall shadow
                            shade -= 0.14f * (1.0f - d * d) + 0.08f * (dx / c.rad);
                        } else {
                            // Raised bright crater rim
                            float rim = 1.0f - std::abs(d - 0.96f) / 0.22f;
                            shade += 0.20f * std::max(0.0f, rim);
                        }
                    }
                }

                float limb = 0.36f + 0.64f * std::pow(nz, 0.65f);
                float finalVal = std::clamp(shade * limb * 255.0f, 20.0f, 248.0f);
                float edgeAlpha = std::clamp((1.0f - r) * 32.0f, 0.0f, 1.0f);

                unsigned char g = static_cast<unsigned char>(finalVal);
                ImageDrawPixel(&img, x, y, Color{ g, g, g, static_cast<unsigned char>(edgeAlpha * 255.0f) });
            }
        }

        m_crateredTextures[v] = LoadTextureFromImage(img);
        SetTextureFilter(m_crateredTextures[v], TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
    }

    // 5. Molten Lava Crust Overlay Texture (Ridged Magma Cracks)
    {
        Image img = GenImageColor(texSize, texSize, BLANK);
        for (int y = 0; y < texSize; ++y) {
            for (int x = 0; x < texSize; ++x) {
                float nx = (static_cast<float>(x) - center) * invR;
                float ny = (static_cast<float>(y) - center) * invR;
                float rSq = nx * nx + ny * ny;
                if (rSq > 1.0f) continue;

                float r = std::sqrt(rSq);
                float nz = std::sqrt(std::max(0.0f, 1.0f - rSq));
                float sx = nx / (nz + 0.38f);
                float sy = ny / (nz + 0.38f);

                float n1 = valueNoise2D(sx * 5.5f + 2.0f, sy * 5.5f + 2.0f, 7777u);
                float n2 = valueNoise2D(sx * 11.0f + 5.0f, sy * 11.0f + 5.0f, 7789u);
                // Sharp ridge lines where noise crosses 0.5
                float ridge = 1.0f - std::abs(n1 - 0.5f) * 5.0f;
                float fineRidge = 1.0f - std::abs(n2 - 0.5f) * 4.5f;
                float magma = std::clamp(std::max(ridge, fineRidge * 0.75f), 0.0f, 1.0f);
                magma = magma * magma;

                float edgeAlpha = std::clamp((1.0f - r) * 28.0f, 0.0f, 1.0f);
                unsigned char rC = static_cast<unsigned char>(std::clamp(magma * 255.0f, 0.0f, 255.0f));
                unsigned char gC = static_cast<unsigned char>(std::clamp(magma * magma * 215.0f, 0.0f, 255.0f));
                unsigned char bC = static_cast<unsigned char>(std::clamp(magma * magma * magma * 90.0f, 0.0f, 255.0f));
                unsigned char aC = static_cast<unsigned char>(magma * edgeAlpha * 245.0f);

                ImageDrawPixel(&img, x, y, Color{ rC, gC, bC, aC });
            }
        }
        m_lavaTexture = LoadTextureFromImage(img);
        SetTextureFilter(m_lavaTexture, TEXTURE_FILTER_BILINEAR);
        UnloadImage(img);
    }
}

void Starfield::generateShaderNoiseTextures() {
    // Seamless procedural noise textures used by the GLSL 3D Spherical Planet & Star Shader (ported from SpaceSim2D)
    constexpr int noiseSize = 256;

    // 1. Rocky continent & crater FBM noise texture
    {
        Image imgRocky = GenImageColor(noiseSize, noiseSize, BLACK);
        for (int y = 0; y < noiseSize; ++y) {
            for (int x = 0; x < noiseSize; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(noiseSize);
                float v = static_cast<float>(y) / static_cast<float>(noiseSize);
                // Tileable 2D FBM approximation
                float n = fbm2D(u * 6.0f + 1.3f, v * 6.0f + 2.7f, 1337u, 4);
                unsigned char val = static_cast<unsigned char>(std::clamp(n * 255.0f, 0.0f, 255.0f));
                ImageDrawPixel(&imgRocky, x, y, Color{ val, val, val, 255 });
            }
        }
        m_texRockyNoise = LoadTextureFromImage(imgRocky);
        SetTextureWrap(m_texRockyNoise, TEXTURE_WRAP_REPEAT);
        SetTextureFilter(m_texRockyNoise, TEXTURE_FILTER_BILINEAR);
        UnloadImage(imgRocky);
    }

    // 2. Gas Giant zonal turbulence texture
    {
        Image imgGas = GenImageColor(noiseSize, noiseSize, BLACK);
        for (int y = 0; y < noiseSize; ++y) {
            for (int x = 0; x < noiseSize; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(noiseSize);
                float v = static_cast<float>(y) / static_cast<float>(noiseSize);
                float n = fbm2D(u * 4.5f + 7.1f, v * 4.5f + 3.9f, 4217u, 4);
                unsigned char val = static_cast<unsigned char>(std::clamp(n * 255.0f, 0.0f, 255.0f));
                ImageDrawPixel(&imgGas, x, y, Color{ val, val, val, 255 });
            }
        }
        m_texGasNoise = LoadTextureFromImage(imgGas);
        SetTextureWrap(m_texGasNoise, TEXTURE_WRAP_REPEAT);
        SetTextureFilter(m_texGasNoise, TEXTURE_FILTER_BILINEAR);
        UnloadImage(imgGas);
    }

    // 3. Stellar plasma cellular convection granulation
    {
        Image imgStar = GenImageCellular(noiseSize, noiseSize, 24);
        m_texStarPlasma = LoadTextureFromImage(imgStar);
        SetTextureWrap(m_texStarPlasma, TEXTURE_WRAP_REPEAT);
        SetTextureFilter(m_texStarPlasma, TEXTURE_FILTER_BILINEAR);
        UnloadImage(imgStar);
    }

    // 4. Deep-Space Parallax Nebula Veil
    {
        Image imgNeb = GenImageColor(noiseSize, noiseSize, BLACK);
        for (int y = 0; y < noiseSize; ++y) {
            for (int x = 0; x < noiseSize; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(noiseSize);
                float v = static_cast<float>(y) / static_cast<float>(noiseSize);
                float n = fbm2D(u * 3.2f + 11.0f, v * 3.2f + 19.0f, 9001u, 4);
                float wisp = std::pow(std::clamp((n - 0.32f) * 1.45f, 0.0f, 1.0f), 1.6f);
                unsigned char val = static_cast<unsigned char>(std::clamp(wisp * 255.0f, 0.0f, 255.0f));
                ImageDrawPixel(&imgNeb, x, y, Color{ val, val, val, 255 });
            }
        }
        m_texDeepNebula = LoadTextureFromImage(imgNeb);
        SetTextureWrap(m_texDeepNebula, TEXTURE_WRAP_MIRROR_REPEAT);
        SetTextureFilter(m_texDeepNebula, TEXTURE_FILTER_BILINEAR);
        UnloadImage(imgNeb);
    }
}

void Starfield::renderBackground(const Camera2DController& camera,
                                 int screenWidth,
                                 int screenHeight,
                                 double timeSeconds) const {
    // True pitch-black deep-space abyss: RGBA(2, 4, 8, 255)
    ClearBackground(Config::BG_DEEP_SPACE);

    if (!m_initialized || screenWidth <= 0 || screenHeight <= 0) return;

    const float w = static_cast<float>(screenWidth);
    const float h = static_cast<float>(screenHeight);

    // Subtle deep-space parallax nebula veils (ported from SpaceSim2D)
    if (m_texDeepNebula.id != 0) {
        float panX = static_cast<float>(std::fmod(camera.position.x * 0.012, 512.0));
        float panY = static_cast<float>(std::fmod(camera.position.y * 0.012, 512.0));
        Rectangle src1{ panX, panY, 256.0f, 256.0f };
        Rectangle src2{ -panX * 0.7f + 128.0f, panY * 0.7f + 128.0f, 256.0f, 256.0f };
        Rectangle dst{ 0.0f, 0.0f, w, h };
        DrawTexturePro(m_texDeepNebula, src1, dst, Vector2{ 0.0f, 0.0f }, 0.0f, Color{ 8, 22, 52, 55 });
        DrawTexturePro(m_texDeepNebula, src2, dst, Vector2{ 0.0f, 0.0f }, 0.0f, Color{ 38, 12, 54, 38 });
    }

    const double parallaxFactors[3] = { 0.015, 0.042, 0.095 };
    const float baseAlphas[3] = { 100.0f, 165.0f, 235.0f };

    for (const auto& s : m_stars) {
        double pFactor = parallaxFactors[s.layer];

        double offsetX = std::fmod(-camera.position.x * pFactor, static_cast<double>(w));
        double offsetY = std::fmod(-camera.position.y * pFactor, static_cast<double>(h));

        float sx = std::fmod(s.normX * w + static_cast<float>(offsetX) + w * 2.0f, w);
        float sy = std::fmod(s.normY * h + static_cast<float>(offsetY) + h * 2.0f, h);

        float twinkle = 0.68f + 0.32f * std::sin(static_cast<float>(timeSeconds) * s.twinkleSpeed + s.twinklePhase);
        unsigned char alpha = static_cast<unsigned char>(std::clamp(baseAlphas[s.layer] * twinkle, 15.0f, 255.0f));

        Color c = s.baseColor;
        c.a = alpha;

        if (s.size <= 1.2f) {
            DrawPixelV(Vector2{ sx, sy }, c);
        } else {
            DrawCircleV(Vector2{ sx, sy }, s.size * 0.65f, c);
        }
    }
}

} // namespace sim
