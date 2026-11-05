#include "rendering/Renderer.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sim {

// GLSL 330 3D Spherical Normal-Mapping, Lambertian/Specular Terminator & Rayleigh Atmospheric Scattering Shader (ported from SpaceSim2D)
static const char* BODY_SPHERE_SHADER = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float uTime;
uniform int uBodyMode;
uniform vec2 uLightDir;
uniform vec4 uMergeGeom;
uniform vec4 uMergeColorAndOsc;

float fbm(vec2 uv) {
    float v = 0.0;
    float a = 0.5;
    mat2 rot = mat2(0.80, -0.60, 0.60, 0.80);
    for (int i = 0; i < 4; ++i) {
        v += a * texture(texture0, uv).r;
        uv = rot * uv * 2.03 + vec2(0.17, 0.31);
        a *= 0.5;
    }
    return v;
}

void main() {
    vec2 p = fragTexCoord * 2.0 - 1.0;
    float r = length(p);
    if (r > 1.0) discard;

    float quadEdgeFade = smoothstep(1.0, 0.72, r);

    float rawLightLen = length(uLightDir);
    float impactHeat = clamp((rawLightLen - 1.0) / 3.0, 0.0, 1.0);
    vec2 lightDir2D = (rawLightLen > 0.001) ? (uLightDir / rawLightLen) : vec2(-0.707, -0.707);

    float r1 = max(uMergeGeom.x, 0.10);
    float rotAngle = uMergeGeom.y;
    float tempK = uMergeGeom.z;
    float impactAngle = uMergeGeom.w;
    float seed = uMergeColorAndOsc.a;

    // MODE 4: Irregular 3D Normal-Mapped Rocky Asteroids & Molten Fragments
    if (uBodyMode == 4) {
        float ca = cos(rotAngle);
        float sa = sin(rotAngle);
        vec2 rp = vec2(ca * p.x + sa * p.y, -sa * p.x + ca * p.y);

        float theta = atan(rp.y, rp.x);
        float rimNoise = texture(texture0, vec2(theta / 6.28318 + 0.5, seed * 0.13)).r;

        float maxR = 0.62
                   + 0.11 * cos(theta * 2.0 + seed)
                   + 0.08 * sin(theta * 3.0 - seed * 1.7)
                   + 0.05 * cos(theta * 5.0 + seed * 2.3)
                   + (rimNoise - 0.5) * 0.12;

        float dist = length(rp);
        if (dist > maxR) discard;

        float normR = dist / max(maxR, 0.01);
        float z = sqrt(max(0.02, 1.0 - normR * normR));

        vec2 rockUV = rp * 0.85 + vec2(0.5, 0.5) + vec2(seed * 0.31, seed * 0.17);
        float hC = fbm(rockUV);
        float hX = fbm(rockUV + vec2(0.02, 0.0));
        float hY = fbm(rockUV + vec2(0.0, 0.02));

        vec2 c1 = rp - vec2(0.16 * cos(seed), 0.12 * sin(seed));
        vec2 c2 = rp - vec2(-0.20 * sin(seed * 1.3), 0.15 * cos(seed * 1.3));
        float dC1 = length(c1) / 0.21;
        float dC2 = length(c2) / 0.15;

        float craterBowl = 0.0;
        vec2 craterGrad = vec2(0.0);
        if (dC1 < 1.0) { craterBowl += (1.0 - dC1 * dC1) * 0.38; craterGrad += (c1 / 0.21) * 1.2; }
        if (dC2 < 1.0) { craterBowl += (1.0 - dC2 * dC2) * 0.30; craterGrad += (c2 / 0.15) * 1.0; }

        vec2 localGrad = vec2((hC - hX) * 3.2, (hC - hY) * 3.2) + craterGrad;
        vec2 worldGrad = vec2(ca * localGrad.x - sa * localGrad.y, sa * localGrad.x + ca * localGrad.y);
        vec3 N = normalize(vec3(p.x / maxR + worldGrad.x, p.y / maxR + worldGrad.y, z * 0.85));

        vec3 lightVec = normalize(vec3(lightDir2D.x, lightDir2D.y, 0.42));
        float diff = max(dot(N, lightVec), 0.05);

        vec3 rockCol = fragColor.rgb * (0.58 + 0.65 * hC) * (1.0 - craterBowl);
        vec3 litRock = rockCol * diff + fragColor.rgb * pow(max(dot(N, normalize(lightVec + vec3(0,0,1))), 0.0), 14.0) * 0.22;

        if (impactHeat > 0.01 || tempK > 900.0) {
            float heat = max(impactHeat, clamp((tempK - 900.0) / 2200.0, 0.0, 1.0));
            float lavaMask = smoothstep(0.42, 0.72, hC) * heat;
            vec3 moltenCol = mix(vec3(1.0, 0.22, 0.02), vec3(1.0, 0.92, 0.55), lavaMask);
            litRock = mix(litRock, moltenCol * 1.45, clamp(heat * 0.85, 0.0, 1.0));
        }

        finalColor = vec4(litRock, 1.0);
        return;
    }

    // MODE 3: Relativistic Black Hole (Gargantua Accretion Disk & Photon Ring)
    if (uBodyMode == 3) {
        float rShadow = r1 * 0.38;
        float tiltA = -0.24;
        float ct = cos(tiltA);
        float st = sin(tiltA);
        vec2 dp = vec2(ct * p.x + st * p.y, -st * p.x + ct * p.y);

        float frontBarY = dp.y + 0.035 * r1;
        float rEquat = length(vec2(dp.x, frontBarY / 0.24));
        float frontDiskMask = smoothstep(rShadow * 1.05, rShadow * 1.38, rEquat)
                            * smoothstep(0.96, rShadow * 1.45, rEquat)
                            * exp(-abs(frontBarY / max(r1 * 0.16, 0.01)) * 2.2);

        if (r < rShadow) {
            if (frontDiskMask > 0.08) {
                float eqNoise = fbm(vec2(dp.x * 2.2 - uTime * 0.85, frontBarY * 6.0));
                float eqDoppler = 0.65 + 0.55 * clamp(-dp.x / max(r1, 0.05), -0.8, 1.0);
                vec3 eqCol = mix(vec3(0.95, 0.25, 0.03), vec3(1.0, 0.94, 0.75), eqNoise) * eqDoppler;
                finalColor = vec4(eqCol * frontDiskMask * 1.45, 1.0);
                return;
            }
            finalColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        float angle = atan(p.y, p.x);
        float radialNorm = clamp((r - rShadow) / max(0.98 - rShadow, 0.01), 0.0, 1.0);
        float keplerPhase = uTime * (1.8 / pow(max(r, 0.12), 0.85));
        float spiral = sin(angle * 4.0 - keplerPhase + 11.0 / (r + 0.08)) * 0.5 + 0.5;
        float plasma = fbm(vec2(r * 2.2 - uTime * 0.25, angle / 6.28318 + uTime * 0.10));

        float photonRing1 = exp(-abs(r - rShadow * 1.05) * 68.0) * 2.2;
        float photonRing2 = exp(-abs(r - rShadow * 1.22) * 28.0) * 1.15;
        float diskEnvelope = smoothstep(0.98, rShadow * 1.18, r) * smoothstep(rShadow * 0.98, rShadow * 1.16, r);
        float filaments = diskEnvelope * (0.35 + 0.65 * plasma * spiral);

        float dopplerShift = cos(angle - 2.35);
        float dopplerBoost = 0.75 + 0.48 * dopplerShift;

        vec3 tempCol = mix(vec3(1.0, 0.92, 0.68), vec3(1.0, 0.45, 0.08), smoothstep(0.0, 0.48, radialNorm));
        tempCol = mix(tempCol, vec3(0.75, 0.12, 0.02), smoothstep(0.42, 0.95, radialNorm));
        vec3 ringCol = mix(vec3(1.0, 0.65, 0.22), vec3(1.0, 0.98, 0.90), clamp(photonRing1 * 0.6, 0.0, 1.0));
        vec3 col = (tempCol * (filaments + frontDiskMask * 0.55) + ringCol * (photonRing1 + photonRing2 * 0.65)) * dopplerBoost;
        float alpha = clamp((filaments + photonRing1 + photonRing2 * 0.7 + frontDiskMask * 0.6) * quadEdgeFade, 0.0, 1.0);

        finalColor = vec4(col * (1.35 + impactHeat * 0.65), alpha);
        return;
    }

    float d = r - r1;
    vec2 impactDir = vec2(cos(impactAngle), sin(impactAngle));
    float alignImpact = max(0.0, dot(normalize(p + vec2(0.0001)), impactDir));

    // Outer Stellar Corona (Mode 2) or Atmospheric Rayleigh Scattering Rim Halo (Modes 0 & 1)
    if (d > 0.0) {
        vec3 haloCol = fragColor.rgb;
        float heatHalo = impactHeat * pow(alignImpact, 1.6) * exp(-d * 18.0) * 1.15;
        vec3 impactAtmoGlow = vec3(1.0, 0.55, 0.15) * heatHalo;

        if (uBodyMode == 2) {
            float angle = atan(p.y, p.x);
            float promNoise = fbm(vec2(angle * 1.8 + seed, uTime * 0.18));
            float loops = pow(max(0.0, sin(angle * 7.0 + promNoise * 5.0 - uTime * 0.6)), 4.0) * exp(-d / (r1 * 0.28)) * 0.85;
            float corona = (exp(-d / max(r1 * 0.22, 0.02)) * 0.78 + loops) * quadEdgeFade;
            vec3 col = mix(haloCol, vec3(1.0, 0.92, 0.70), loops) * 1.30 + impactAtmoGlow * quadEdgeFade;
            finalColor = vec4(col, clamp(corona + heatHalo * 0.55 * quadEdgeFade, 0.0, 1.0));
            return;
        } else {
            float sunFacing = max(0.10, dot(normalize(p + vec2(0.0001)), lightDir2D) * 0.65 + 0.35);
            float atmo = exp(-d / max(r1 * 0.105, 0.01)) * 0.72 * sunFacing * quadEdgeFade;
            // Cyan/blue Rayleigh scattering for Terran worlds (mode 0); warm amber/gold for Gas Giants (mode 1)
            vec3 rayleighTarget = (uBodyMode == 0) ? vec3(0.22, 0.75, 1.0) : vec3(1.0, 0.78, 0.38);
            vec3 rayleighCol = mix(haloCol, rayleighTarget, (uBodyMode == 0) ? 0.65 : 0.45);
            finalColor = vec4((rayleighCol * 1.25 + impactAtmoGlow) * quadEdgeFade, clamp(atmo + heatHalo * 0.65 * quadEdgeFade, 0.0, 1.0));
            return;
        }
    }

    // Procedural 3D Spherical Normal Vector: n = (x, y, sqrt(R^2 - x^2 - y^2)) / R
    vec2 sp = clamp(p / r1, vec2(-0.9999), vec2(0.9999));
    float normDist = clamp(length(sp), 0.0, 1.0);
    float z = sqrt(max(0.001, 1.0 - normDist * normDist));
    vec3 baseCol = fragColor.rgb;

    // Localized impact crater & magma fracture veins
    vec2 impactCenter = impactDir * 0.68;
    float distToImpact = length(sp - impactCenter);
    float craterMask = 0.0;
    vec3 magmaEmission = vec3(0.0);

    if (impactHeat > 0.005) {
        float organicNoise = fbm(sp * 1.6 + vec2(seed * 0.17, 0.43));
        float perturbedDist = distToImpact + (organicNoise - 0.5) * 0.22;
        float corePool = smoothstep(0.40 * impactHeat + 0.08, 0.0, perturbedDist);
        float waveRadius = (1.0 - impactHeat) * 1.30;
        float seismicRing = exp(-abs(perturbedDist - waveRadius) * 9.5) * impactHeat * 0.85;
        float angleToImpact = atan(sp.y - impactCenter.y, sp.x - impactCenter.x);
        float radialVeins = pow(0.5 + 0.5 * sin(angleToImpact * 7.0 + organicNoise * 6.0), 3.0)
                          * smoothstep(0.80 * impactHeat + 0.20, 0.05, perturbedDist) * impactHeat;

        craterMask = clamp(corePool + seismicRing + radialVeins * 0.70, 0.0, 1.0);
        magmaEmission = mix(vec3(1.0, 0.22, 0.02), vec3(1.0, 0.75, 0.20), clamp(seismicRing + radialVeins, 0.0, 1.0));
        magmaEmission = mix(magmaEmission, vec3(1.0, 0.98, 0.88), clamp(corePool * impactHeat * 1.25, 0.0, 1.0));
    }

    // MODE 2: Stars (3D Convective Granulation, Sunspots & Limb Darkening)
    if (uBodyMode == 2) {
        vec2 rotOffset = vec2(rotAngle * 0.08 + uTime * 0.02, seed * 0.37);
        float g1 = fbm(sp * 1.4 + rotOffset + vec2(z * 0.25));
        float g2 = fbm(sp * 3.2 - rotOffset * 0.7 - vec2(z * 0.20));
        float cells = clamp(g1 * 0.62 + g2 * 0.38, 0.0, 1.0);

        float spotNoise = fbm(sp * 1.15 + vec2(seed * 0.53, uTime * 0.01));
        float sunspot = smoothstep(0.70, 0.78, spotNoise);
        float faculae = smoothstep(0.62, 0.70, spotNoise) * (1.0 - sunspot);

        float limb = pow(z, 0.45);
        vec3 photosphere = mix(baseCol * 0.55, vec3(1.0, 0.98, 0.86), pow(cells, 1.2) * limb);
        photosphere = mix(photosphere, baseCol * 0.18, sunspot * 0.82);
        photosphere += vec3(1.0, 0.95, 0.80) * faculae * 0.35;

        vec3 rimGlow = baseCol * pow(1.0 - z, 2.4) * 0.95;
        vec3 starFinal = photosphere + rimGlow + magmaEmission * craterMask * 1.05;
        finalColor = vec4(starFinal, 1.0);
        return;
    }

    // 3D Spherical Coordinates & Normal Vector N, Star Light Vector L, View Vector V, Half Vector H
    float clampedY = clamp(sp.y, -0.9999, 0.9999);
    float lon = atan(sp.x, max(z, 0.01)) / 3.14159 + rotAngle * 0.15 + seed * 0.71;
    float lat = asin(clampedY) / 3.14159 + 0.5;
    vec2 sphereUV = vec2(lon, lat);

    vec3 N = normalize(vec3(sp.x, sp.y, max(z, 0.04)));
    vec3 L = normalize(vec3(lightDir2D.x, lightDir2D.y, 0.38));
    vec3 V = vec3(0.0, 0.0, 1.0);
    vec3 H = normalize(L + V);

    // MODE 1: Gas Giants (Turbulent Zonal Bands + Great Storm Vortex + Warm Amber/Gold Rayleigh Rim)
    if (uBodyMode == 1) {
        float warp = fbm(vec2(lon * 0.9 + uTime * 0.015, lat * 2.2 + seed));
        float jetStreams = sin(lat * 34.0 + warp * 6.5) * 0.5 + 0.5;
        float fineBands  = cos(lat * 78.0 - warp * 9.0 + lon * 2.0) * 0.5 + 0.5;

        vec2 stormCenter = vec2(fract(lon * 0.5 + 0.25) - 0.5, lat - 0.36);
        stormCenter.x *= 1.8;
        float stormDist = length(stormCenter) / 0.14;
        float stormMask = smoothstep(1.1, 0.15, stormDist);
        float swirl = sin(stormDist * 12.0 - atan(stormCenter.y, stormCenter.x) * 2.0 - uTime * 0.8) * 0.5 + 0.5;

        vec3 zoneCol = baseCol * 1.28 + vec3(0.08, 0.05, 0.02);
        vec3 beltCol = baseCol * 0.54;
        vec3 gasCol = mix(beltCol, zoneCol, jetStreams * 0.75 + fineBands * 0.25);

        vec3 vortexCol = mix(baseCol * 1.35 + vec3(0.25, 0.05, -0.05), vec3(1.0, 0.88, 0.75), swirl * 0.5);
        gasCol = mix(gasCol, vortexCol, stormMask * 0.85);

        // Smooth terminator crescent into pitch-black night hemisphere
        float rawDot = dot(N, L);
        float diff = max(rawDot, 0.0) * smoothstep(-0.08, 0.15, rawDot) + 0.025;
        float fresnel = pow(1.0 - z, 2.8) * 0.75;
        vec3 amberRim = mix(baseCol, vec3(1.0, 0.78, 0.38), 0.55);
        vec3 litGas = gasCol * diff + amberRim * fresnel * max(rawDot + 0.28, 0.08);

        if (craterMask > 0.005) {
            litGas = mix(litGas, magmaEmission * 1.35, clamp(craterMask * 1.05, 0.0, 1.0));
        }
        finalColor = vec4(litGas, 1.0);
        return;
    }

    // MODE 0: Terran Rocky Worlds & Cratered Moons (3D Bump Normal Mapping, Specular Oceans, Polar Caps, Clouds & Night Cities)
    float hMap  = fbm(sphereUV * 1.35);
    float hMapX = fbm((sphereUV + vec2(0.012, 0.0)) * 1.35);
    float hMapY = fbm((sphereUV + vec2(0.0, 0.012)) * 1.35);
    float moisture = fbm(sphereUV * 2.4 + vec2(3.1, 1.7));

    bool hasLiquidOcean = (baseCol.b > baseCol.r * 1.08) && (tempK >= 180.0) && (tempK <= 520.0);
    float seaLevel = hasLiquidOcean ? 0.50 : -1.0;
    float isLand = smoothstep(seaLevel - 0.02, seaLevel + 0.025, hMap);

    vec2 bumpGrad = vec2(hMap - hMapX, hMap - hMapY) * (hasLiquidOcean ? (isLand * 4.5) : 5.5);
    vec3 bumpedN = normalize(vec3(N.x + bumpGrad.x, N.y + bumpGrad.y, N.z));

    float rawNDotL = dot(N, L);
    float terminator = smoothstep(-0.10, 0.16, rawNDotL);
    float diff = max(dot(bumpedN, L), 0.0) * terminator + 0.02;
    float dayFactor = smoothstep(-0.08, 0.18, rawNDotL);
    float nightFactor = 1.0 - smoothstep(-0.20, 0.05, rawNDotL);

    vec3 surfaceCol;
    float specStrength = 0.08;

    if (hasLiquidOcean) {
        float oceanDepth = clamp((seaLevel - hMap) / max(seaLevel, 0.01), 0.0, 1.0);
        vec3 deepSea = baseCol * 0.42;
        vec3 shallowCoast = mix(baseCol * 0.95, vec3(0.15, 0.82, 0.88), 0.45);
        vec3 oceanCol = mix(shallowCoast, deepSea, pow(oceanDepth, 0.65));

        float elev = clamp((hMap - seaLevel) / (1.0 - seaLevel), 0.0, 1.0);
        vec3 lushGreen = vec3(0.16, 0.52, 0.20);
        vec3 aridSand  = vec3(0.76, 0.64, 0.42);
        vec3 rockPeak  = vec3(0.52, 0.48, 0.45);
        vec3 snowPeak  = vec3(0.95, 0.97, 1.00);

        vec3 biome = mix(aridSand, lushGreen, smoothstep(0.35, 0.65, moisture));
        biome = mix(biome, rockPeak, smoothstep(0.45, 0.78, elev));
        biome = mix(biome, snowPeak, smoothstep(0.75, 0.92, elev));

        surfaceCol = mix(oceanCol, biome, isLand);
        specStrength = mix(0.95, 0.08, isLand);
    } else {
        float craters = fbm(sphereUV * 4.2 + vec2(seed));
        surfaceCol = mix(baseCol * 0.48, baseCol * 1.28, smoothstep(0.28, 0.76, hMap * 0.7 + craters * 0.3));
    }

    float poleLat = abs(sp.y);
    float iceThreshold = clamp(1.05 - (310.0 - clamp(tempK, 80.0, 400.0)) * 0.0028, 0.0, 0.96);
    float iceMask = smoothstep(iceThreshold - 0.08, iceThreshold + 0.06, poleLat + (hMap - 0.5) * 0.18);
    if (tempK < 420.0) {
        surfaceCol = mix(surfaceCol, vec3(0.90, 0.95, 1.0), iceMask * 0.88);
        specStrength = max(specStrength, iceMask * 0.55);
    }

    float thermalLava = clamp((tempK - 650.0) / 900.0, 0.0, 1.0);
    vec3 lavaRivers = vec3(0.0);
    if (thermalLava > 0.01) {
        float vein = smoothstep(0.56, 0.42, hMap) * thermalLava;
        lavaRivers = mix(vec3(1.0, 0.20, 0.01), vec3(1.0, 0.85, 0.30), vein) * vein * 1.4;
    }

    float cloudCover = 0.0;
    if (hasLiquidOcean || tempK > 240.0) {
        vec2 cloudUV = vec2(lon - uTime * 0.03, lat * 1.6);
        float cNoise = fbm(cloudUV * 1.8);
        cloudCover = smoothstep(0.54, 0.78, cNoise) * (hasLiquidOcean ? 0.75 : 0.32);
    }

    float spec = pow(max(dot(N, H), 0.0), 28.0) * specStrength * (1.0 - cloudCover * 0.85) * dayFactor;
    float fresnel = pow(1.0 - z, 2.8) * 0.82;
    vec3 atmoTint = hasLiquidOcean ? vec3(0.22, 0.72, 1.0) : (baseCol * 0.85);

    vec3 litSurface = surfaceCol * (1.0 - cloudCover * 0.35) * diff
                    + vec3(1.0, 0.96, 0.88) * spec
                    + vec3(0.96, 0.98, 1.0) * cloudCover * max(rawNDotL, 0.0) * terminator
                    + atmoTint * fresnel * max(rawNDotL + 0.30, 0.08);

    if (hasLiquidOcean && isLand > 0.5 && iceMask < 0.3) {
        float cityGrid = fbm(sphereUV * 9.5);
        float coastalPref = smoothstep(0.75, 0.50, hMap);
        float cities = smoothstep(0.60, 0.74, cityGrid) * coastalPref * nightFactor * (1.0 - cloudCover * 0.7);
        litSurface += vec3(1.0, 0.78, 0.35) * cities * 1.35;
    }

    litSurface += lavaRivers;

    if (craterMask > 0.005) {
        litSurface = mix(litSurface, magmaEmission * 1.35, clamp(craterMask * 1.05, 0.0, 1.0));
    }

    finalColor = vec4(litSurface, 1.0);
}
)";

// GLSL 330 Full-Screen Einstein Relativistic Gravitational Lensing & Optical Bloom Shader (ported from SpaceSim2D)
static const char* POST_LENSING_BLOOM_SHADER = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform int lensCount;
uniform vec2 lensPos[8];
uniform float lensRadius[8];
uniform float aspectRatio;
uniform int bloomEnabled;

void main() {
    vec2 uv = fragTexCoord;
    vec2 totalOffset = vec2(0.0);
    float photonRingGlow = 0.0;
    bool insideHorizon = false;

    for (int i = 0; i < lensCount; i++) {
        vec2 delta = uv - lensPos[i];
        delta.x *= aspectRatio;
        float dist = length(delta);
        float r_s = lensRadius[i];

        if (dist < r_s * 0.74) {
            insideHorizon = true;
        } else {
            float einsteinR = r_s * 1.72;
            float deflection = (einsteinR * einsteinR) / (dist + 0.0012);
            deflection = min(deflection, dist * 0.90);

            vec2 dir = normalize(delta);
            dir.x /= aspectRatio;
            totalOffset -= dir * deflection;

            float ringDist = abs(dist - r_s * 0.92);
            photonRingGlow += exp(-ringDist / max(r_s * 0.16, 0.0008)) * 1.35;
        }
    }

    if (insideHorizon) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 warpedUV = clamp(uv + totalOffset, 0.001, 0.999);
    vec3 baseCol = texture(texture0, warpedUV).rgb;

    if (bloomEnabled == 1) {
        vec2 s1 = vec2(0.0028 / aspectRatio, 0.0028);
        vec2 s2 = vec2(0.0056 / aspectRatio, 0.0056);

        vec3 b1 = (
            texture(texture0, clamp(warpedUV + vec2( s1.x,  0.0), 0.001, 0.999)).rgb +
            texture(texture0, clamp(warpedUV + vec2(-s1.x,  0.0), 0.001, 0.999)).rgb +
            texture(texture0, clamp(warpedUV + vec2( 0.0,  s1.y), 0.001, 0.999)).rgb +
            texture(texture0, clamp(warpedUV + vec2( 0.0, -s1.y), 0.001, 0.999)).rgb
        ) * 0.25;

        vec3 b2 = (
            texture(texture0, clamp(warpedUV + vec2( s2.x,  s2.y), 0.001, 0.999)).rgb +
            texture(texture0, clamp(warpedUV + vec2(-s2.x,  s2.y), 0.001, 0.999)).rgb +
            texture(texture0, clamp(warpedUV + vec2( s2.x, -s2.y), 0.001, 0.999)).rgb +
            texture(texture0, clamp(warpedUV + vec2(-s2.x, -s2.y), 0.001, 0.999)).rgb
        ) * 0.25;

        vec3 bloom = b1 * 0.65 + b2 * 0.35;
        float lum = dot(bloom, vec3(0.299, 0.587, 0.114));
        baseCol += bloom * smoothstep(0.28, 0.92, lum) * 0.72;
    }

    vec3 ringColor = vec3(1.0, 0.78, 0.42) * photonRingGlow;
    finalColor = vec4(baseCol + ringColor, 1.0);
}
)";

void Renderer::drawTaperedNeedleSpike(Vector2 center, float angle, float halfLen, float maxHalfThick, Color col, float alpha) {
    if (alpha <= 0.005f || halfLen <= 0.5f) return;
    Vector2 dir{ std::cos(angle), std::sin(angle) };
    Vector2 perp{ -dir.y, dir.x };

    const float lenScales[2]   = { 1.0f,  0.45f };
    const float thickScales[2] = { 0.45f, 1.00f };
    const float alphaScales[2] = { 0.32f, 0.68f };

    for (int i = 0; i < 2; ++i) {
        float L = halfLen * lenScales[i];
        float W = maxHalfThick * thickScales[i];
        unsigned char a = static_cast<unsigned char>(std::clamp(alpha * alphaScales[i] * 255.0f, 0.0f, 255.0f));
        Color c = (i == 1) ? Color{ 255, 255, 255, a } : Color{ col.r, col.g, col.b, a };

        Vector2 tip1{ center.x + dir.x * L, center.y + dir.y * L };
        Vector2 tip2{ center.x - dir.x * L, center.y - dir.y * L };
        Vector2 side1{ center.x + perp.x * W, center.y + perp.y * W };
        Vector2 side2{ center.x - perp.x * W, center.y - perp.y * W };

        DrawTriangle(tip1, side1, side2, c);
        DrawTriangle(tip1, side2, side1, c);
        DrawTriangle(tip2, side2, side1, c);
        DrawTriangle(tip2, side1, side2, c);
    }
}

void Renderer::drawPlanetaryRings3D(Vector2 center, float drawR, Color col, Vec2d toLight, bool frontHalf) {
    // 3D tilted planetary rings with Cassini division & planetary shadow casting (ported from SpaceSim2D)
    float tilt = -0.32f;
    float ct = std::cos(tilt);
    float st = std::sin(tilt);
    float ySquash = 0.36f;

    Vector2 shadowDir{ static_cast<float>(-toLight.x), static_cast<float>(-toLight.y) };

    struct RingBand { float rIn; float rOut; Color c; float alpha; };
    const RingBand bands[4] = {
        { 1.26f, 1.44f, col,       0.32f },
        { 1.45f, 1.88f, RAYWHITE,  0.72f },
        { 1.96f, 2.34f, col,       0.58f },
        { 2.38f, 2.45f, LIGHTGRAY, 0.28f }
    };

    int segStart = frontHalf ? 0 : 24;
    int segEnd   = frontHalf ? 24 : 48;

    for (const auto& band : bands) {
        float r1 = drawR * band.rIn;
        float r2 = drawR * band.rOut;
        Color baseC = ColorAlphaBlend(band.c, col, WHITE);

        for (int i = segStart; i < segEnd; ++i) {
            float a0 = (static_cast<float>(i) / 48.0f) * 2.0f * 3.14159265f;
            float a1 = (static_cast<float>(i + 1) / 48.0f) * 2.0f * 3.14159265f;
            float aMid = 0.5f * (a0 + a1);

            auto ringPt = [&](float r, float a) -> Vector2 {
                float lx = std::cos(a) * r;
                float ly = std::sin(a) * r * ySquash;
                return Vector2{
                    center.x + (lx * ct - ly * st),
                    center.y + (lx * st + ly * ct)
                };
            };

            Vector2 p1 = ringPt(r1, a0);
            Vector2 p2 = ringPt(r2, a0);
            Vector2 p3 = ringPt(r2, a1);
            Vector2 p4 = ringPt(r1, a1);

            Vector2 midScreen = ringPt(0.5f * (r1 + r2), aMid);
            Vector2 rel{ midScreen.x - center.x, midScreen.y - center.y };
            float projShadow = rel.x * shadowDir.x + rel.y * shadowDir.y;
            float perpDist = std::abs(rel.x * (-shadowDir.y) + rel.y * shadowDir.x);

            float shadowFactor = 1.0f;
            if (projShadow > 0.0f && perpDist < drawR * 0.96f) {
                shadowFactor = 0.14f + 0.86f * std::clamp((perpDist - drawR * 0.72f) / (drawR * 0.24f), 0.0f, 1.0f);
            }

            Color segCol{
                static_cast<unsigned char>(baseC.r * shadowFactor),
                static_cast<unsigned char>(baseC.g * shadowFactor),
                static_cast<unsigned char>(baseC.b * shadowFactor),
                static_cast<unsigned char>(255.0f * band.alpha)
            };

            DrawTriangle(p1, p2, p3, segCol);
            DrawTriangle(p3, p2, p1, segCol);
            DrawTriangle(p1, p3, p4, segCol);
            DrawTriangle(p4, p3, p1, segCol);
        }
    }
}

Renderer::Renderer() = default;

Renderer::~Renderer() {
    unload();
}

void Renderer::initialize() {
    m_starfield.initialize(320);

    m_bodyShader = LoadShaderFromMemory(nullptr, BODY_SPHERE_SHADER);
    if (m_bodyShader.id != 0 && m_bodyShader.locs != nullptr) {
        m_locBodyTime = GetShaderLocation(m_bodyShader, "uTime");
        m_locBodyMode = GetShaderLocation(m_bodyShader, "uBodyMode");
        m_locLightDir = GetShaderLocation(m_bodyShader, "uLightDir");
        m_locMergeGeom = GetShaderLocation(m_bodyShader, "uMergeGeom");
        m_locMergeColorAndOsc = GetShaderLocation(m_bodyShader, "uMergeColorAndOsc");
    }

    m_postShader = LoadShaderFromMemory(nullptr, POST_LENSING_BLOOM_SHADER);
    if (m_postShader.id != 0 && m_postShader.locs != nullptr) {
        m_locLensCount = GetShaderLocation(m_postShader, "lensCount");
        m_locLensPos = GetShaderLocation(m_postShader, "lensPos");
        m_locLensRadius = GetShaderLocation(m_postShader, "lensRadius");
        m_locAspectRatio = GetShaderLocation(m_postShader, "aspectRatio");
        m_locBloomEnabled = GetShaderLocation(m_postShader, "bloomEnabled");
    }
}

void Renderer::unload() {
    if (m_bodyShader.id != 0) {
        UnloadShader(m_bodyShader);
        m_bodyShader.id = 0;
    }
    if (m_postShader.id != 0) {
        UnloadShader(m_postShader);
        m_postShader.id = 0;
    }
    if (m_sceneTarget.id != 0) {
        UnloadRenderTexture(m_sceneTarget);
        m_sceneTarget.id = 0;
    }
    m_starfield.unload();
}

void Renderer::checkTargetResize(int screenWidth, int screenHeight) {
    if (screenWidth <= 0 || screenHeight <= 0) return;
    if (m_sceneTarget.id == 0 || screenWidth != m_targetWidth || screenHeight != m_targetHeight) {
        if (m_sceneTarget.id != 0) {
            UnloadRenderTexture(m_sceneTarget);
        }
        m_targetWidth = screenWidth;
        m_targetHeight = screenHeight;
        m_sceneTarget = LoadRenderTexture(m_targetWidth, m_targetHeight);
    }
}

void Renderer::addCameraShake(float intensity) {
    m_cameraShake = std::min(24.0f, m_cameraShake + intensity);
}

void Renderer::updateLensingShaderUniforms(const PhysicsWorld& world, const Camera2DController& camera, int screenWidth, int screenHeight) {
    if (m_postShader.id == 0 || m_postShader.locs == nullptr) return;

    int count = 0;
    Vector2 positions[8] = {};
    float radii[8] = {};

    for (const auto& b : world.bodies) {
        if (!b.alive) continue;
        if (b.isBlackHole() || b.bodyType == CelestialBodyType::NeutronStar) {
            Vector2 screenPos = camera.worldToScreen(b.position);

            // Normalized screen coordinates (Y inverted for Raylib render textures)
            positions[count] = {
                screenPos.x / static_cast<float>(screenWidth),
                1.0f - (screenPos.y / static_cast<float>(screenHeight))
            };

            float effectiveR = b.isBlackHole() ? static_cast<float>(b.radius * 1.35) : 12.0f;
            radii[count] = (effectiveR * static_cast<float>(camera.zoom)) / static_cast<float>(screenHeight);

            count++;
            if (count >= 8) break;
        }
    }

    float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
    int bloomInt = showBloom ? 1 : 0;
    if (m_locLensCount != -1) SetShaderValue(m_postShader, m_locLensCount, &count, SHADER_UNIFORM_INT);
    if (m_locAspectRatio != -1) SetShaderValue(m_postShader, m_locAspectRatio, &aspect, SHADER_UNIFORM_FLOAT);
    if (m_locBloomEnabled != -1) SetShaderValue(m_postShader, m_locBloomEnabled, &bloomInt, SHADER_UNIFORM_INT);
    if (count > 0) {
        if (m_locLensPos != -1) SetShaderValueV(m_postShader, m_locLensPos, positions, SHADER_UNIFORM_VEC2, count);
        if (m_locLensRadius != -1) SetShaderValueV(m_postShader, m_locLensRadius, radii, SHADER_UNIFORM_FLOAT, count);
    }
}

void Renderer::drawTacticalRadar(const PhysicsWorld& world,
                                 const Camera2DController& camera,
                                 int screenWidth,
                                 int screenHeight) const {
    Vector2 radarCenter{ static_cast<float>(screenWidth) * 0.5f, static_cast<float>(screenHeight) - 80.0f };
    float radarR = 48.0f;

    // Dark sci-fi glass circle + glowing cyan ring
    DrawCircleV(radarCenter, radarR + 2.0f, Color{ 8, 12, 22, 235 });
    DrawRing(radarCenter, radarR, radarR + 2.0f, 0.0f, 360.0f, 48, Color{ 0, 195, 255, 160 });
    DrawRing(radarCenter, radarR * 0.5f, radarR * 0.5f + 1.0f, 0.0f, 360.0f, 36, Color{ 0, 195, 255, 60 });
    DrawLineV(Vector2{ radarCenter.x - radarR, radarCenter.y }, Vector2{ radarCenter.x + radarR, radarCenter.y }, Color{ 0, 195, 255, 55 });
    DrawLineV(Vector2{ radarCenter.x, radarCenter.y - radarR }, Vector2{ radarCenter.x, radarCenter.y + radarR }, Color{ 0, 195, 255, 55 });

    float radarScale = radarR / 4500.0f;
    for (const auto& b : world.bodies) {
        if (!b.alive || b.bodyType == CelestialBodyType::Asteroid) continue;
        Vec2d relPos = b.position - camera.position;
        Vector2 blipRel{
            static_cast<float>(relPos.x * radarScale),
            static_cast<float>(relPos.y * radarScale)
        };
        float dist = Vector2Length(blipRel);
        if (dist > radarR - 3.0f) {
            blipRel = Vector2Scale(Vector2Normalize(blipRel), radarR - 3.0f);
        }
        float blipR = (b.isBlackHole() || b.isLuminousStar()) ? 3.5f : 2.2f;
        Color blipCol = b.isBlackHole() ? ORANGE : b.color;
        DrawCircleV(Vector2Add(radarCenter, blipRel), blipR, blipCol);
    }

    if (world.spacecraft.enabled) {
        Vec2d shipRelPos = world.spacecraft.position - camera.position;
        Vector2 shipRel{
            static_cast<float>(shipRelPos.x * radarScale),
            static_cast<float>(shipRelPos.y * radarScale)
        };
        if (Vector2Length(shipRel) < radarR - 2.0f) {
            DrawCircleV(Vector2Add(radarCenter, shipRel), 2.5f, GREEN);
        }
    }

    const char* hintText = "[G] Procedural   |   [F] Focus   |   [O] Auto-Orbit   |   [M] Meteor Barrage   |   [B] Barycenter   |   [P] Pilot Ship   |   [Space] Pause";
    int textW = MeasureText(hintText, 11);
    float pillW = static_cast<float>(textW + 24);
    float pillH = 18.0f;
    float pillX = radarCenter.x - pillW * 0.5f;
    float pillY = static_cast<float>(screenHeight) - 22.0f;

    DrawRectangleRounded(Rectangle{ pillX, pillY, pillW, pillH }, 0.45f, 6, Color{ 8, 12, 22, 230 });
    DrawRectangleRoundedLines(Rectangle{ pillX, pillY, pillW, pillH }, 0.45f, 6, 1.0f, Color{ 0, 195, 255, 140 });
    DrawText(hintText, static_cast<int>(radarCenter.x - textW * 0.5f), static_cast<int>(pillY + 3.0f), 11, Color{ 0, 225, 255, 240 });
}

void Renderer::render(const PhysicsWorld& world,
                      const Camera2DController& camera,
                      const SpawnerPreviewState& spawnerPreview,
                      int screenWidth,
                      int screenHeight,
                      double elapsedSeconds) {
    checkTargetResize(screenWidth, screenHeight);

    if (m_bodyShader.id != 0 && m_locBodyTime >= 0) {
        float tSec = static_cast<float>(elapsedSeconds);
        SetShaderValue(m_bodyShader, m_locBodyTime, &tSec, SHADER_UNIFORM_FLOAT);
    }

    // Camera shake damping
    m_cameraShake = std::max(0.0f, m_cameraShake - static_cast<float>(GetFrameTime()) * 26.0f);

    BeginTextureMode(m_sceneTarget);

    // 1. Deep space background & 3-layer parallax starfield + deep-space nebula veils
    m_starfield.renderBackground(camera, screenWidth, screenHeight, elapsedSeconds);

    // 2. 2.5D Deformable glowing spacetime gravity grid
    spacetimeGrid.render(camera, world.bodies, world.G, screenWidth, screenHeight);

    // 3. Expanding colorful Supernova Nebula particle clouds
    renderNebulaClouds(world, camera);

    // 4. Barnes-Hut Quadtree debug spatial grid
    if (showQuadtreeDebug) {
        renderQuadtreeDebug(world, camera);
    }

    // 5. Fading orbital history trails
    if (world.showTrails) {
        renderOrbitalTrails(world, camera);
    }

    // 6. Future trajectory prediction lines for selected body or active spacecraft
    if (world.showTrajectoryPrediction) {
        if (const CelestialBody* selected = world.findBodyById(world.selectedBodyId)) {
            PredictedTrajectory pred = world.computePredictedTrajectory(
                *selected, world.predictionSteps, world.predictionStepDt, world.referenceFrameBodyId);
            renderPredictedTrajectory(pred, world, camera, Config::PREDICTION_COLOR);
        }

        if (world.spacecraft.enabled) {
            CelestialBody shipProbe;
            shipProbe.id = 99999999u;
            shipProbe.name = "Explorer-1";
            shipProbe.position = world.spacecraft.position;
            shipProbe.velocity = world.spacecraft.velocity;
            shipProbe.mass = world.spacecraft.getTotalMass();
            shipProbe.radius = world.spacecraft.radius;

            PredictedTrajectory shipPred = world.computePredictedTrajectory(
                shipProbe, world.predictionSteps, world.predictionStepDt, world.referenceFrameBodyId);
            renderPredictedTrajectory(shipPred, world, camera, Color{ 85, 225, 255, 225 });
        }
    }

    // 7. Procedural 3D-Shaded Celestial Bodies (Black Holes, Stars, Banded Gas Giants, Terran Worlds, Cratered Moons)
    renderCelestialBodies(world, camera, screenWidth, screenHeight, elapsedSeconds);

    // 8. Sleek Sci-Fi Explorer Spacecraft & Twin-Nacelle Plasma Plumes
    if (world.spacecraft.enabled) {
        renderSpacecraft(world.spacecraft, camera, elapsedSeconds);
    }

    // 9. Live velocity & net gravitational acceleration vector arrows
    if (showVelocityVectors || showAccelerationVectors) {
        renderVectorOverlays(world, camera);
    }

    // 10. Impact Sparks, Flashes & Expanding Shockwave Rings
    renderImpactSparksAndShockwaves(world, camera);

    // 11. Slingshot body spawner preview & dynamic lookahead curve
    if (spawnerPreview.active) {
        renderSpawnerPreview(spawnerPreview, world, camera, elapsedSeconds);
    }

    EndTextureMode();

    // 12. Full-Screen Post-Processing Pass: Einstein Relativistic Gravitational Lensing & Bloom
    updateLensingShaderUniforms(world, camera, screenWidth, screenHeight);

    Rectangle srcRect{ 0.0f, 0.0f, static_cast<float>(screenWidth), -static_cast<float>(screenHeight) };
    if (m_postShader.id != 0 && m_postShader.locs != nullptr) {
        BeginShaderMode(m_postShader);
        DrawTextureRec(m_sceneTarget.texture, srcRect, Vector2{ 0.0f, 0.0f }, WHITE);
        EndShaderMode();
    } else {
        DrawTextureRec(m_sceneTarget.texture, srcRect, Vector2{ 0.0f, 0.0f }, WHITE);
    }

    // 13. System Barycenter Crosshair Indicator
    if ((showBarycenter || world.trackBarycenter) && !world.bodies.empty()) {
        Vector2 bScreen = camera.worldToScreen(world.diagnostics.barycenter);
        Color bCol{ 255, 215, 70, 210 };
        DrawCircleLines(static_cast<int>(bScreen.x), static_cast<int>(bScreen.y), 8.0f, bCol);
        DrawLineEx(Vector2{ bScreen.x - 13.0f, bScreen.y }, Vector2{ bScreen.x + 13.0f, bScreen.y }, 1.5f, bCol);
        DrawLineEx(Vector2{ bScreen.x, bScreen.y - 13.0f }, Vector2{ bScreen.x, bScreen.y + 13.0f }, 1.5f, bCol);
        DrawText("Barycenter", static_cast<int>(bScreen.x + 11.0f), static_cast<int>(bScreen.y - 16.0f), 11, bCol);
    }

    // 14. Tactical Circular Radar & Shortcut Banner
    if (showRadar) {
        drawTacticalRadar(world, camera, screenWidth, screenHeight);
    }
}

void Renderer::renderNebulaClouds(const PhysicsWorld& world, const Camera2DController& camera) const {
    if (world.nebulaParticles.empty()) return;
    Texture2D glowTex = m_starfield.getGlowTexture();
    if (glowTex.id == 0) return;

    BeginBlendMode(BLEND_ADDITIVE);
    Rectangle src{ 0.0f, 0.0f, static_cast<float>(glowTex.width), static_cast<float>(glowTex.height) };

    for (const auto& np : world.nebulaParticles) {
        float t = std::clamp(np.age / np.maxLife, 0.0f, 1.0f);
        float worldRad = np.startRadius + (np.endRadius - np.startRadius) * std::sqrt(t);
        float screenDiam = std::max(12.0f, static_cast<float>(worldRad * 2.0 * camera.zoom));

        Vector2 sp = camera.worldToScreen(np.position);
        float fade = std::sin(t * 3.14159265f);
        Color c = np.color;
        c.a = static_cast<unsigned char>(std::clamp(static_cast<float>(np.color.a) * fade, 0.0f, 255.0f));

        Rectangle dst{ sp.x - screenDiam * 0.5f, sp.y - screenDiam * 0.5f, screenDiam, screenDiam };
        DrawTexturePro(glowTex, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f, c);
    }
    EndBlendMode();
}

void Renderer::renderQuadtreeDebug(const PhysicsWorld& world, const Camera2DController& camera) const {
    BarnesHutTree debugTree;
    if (world.solverType == GravitySolverType::BarnesHut) {
        world.barnesHutTree.getDebugRectangles(m_quadtreeRects);
    } else {
        debugTree.build(world.bodies);
        debugTree.getDebugRectangles(m_quadtreeRects);
    }

    for (const auto& r : m_quadtreeRects) {
        Vector2 topLeft = camera.worldToScreen(Vec2d(r.x, r.y));
        float w = static_cast<float>(r.width * camera.zoom);
        float h = static_cast<float>(r.height * camera.zoom);
        DrawRectangleLinesEx(Rectangle{ topLeft.x, topLeft.y, w, h }, 1.0f, Config::QUADTREE_DEBUG_COLOR);
    }
}

void Renderer::renderOrbitalTrails(const PhysicsWorld& world, const Camera2DController& camera) const {
    const CelestialBody* refBody = world.findBodyById(world.referenceFrameBodyId);
    Vec2d refPos = refBody ? refBody->position : Vec2d(0.0, 0.0);

    for (const auto& b : world.bodies) {
        if (!b.alive) continue;

        const std::deque<Vec2d>& activeTrail = (refBody != nullptr) ? b.relativeTrail : b.trail;
        const size_t count = activeTrail.size();
        if (count < 2) continue;

        for (size_t i = 1; i < count; ++i) {
            float progress = static_cast<float>(i) / static_cast<float>(count);
            unsigned char alpha = static_cast<unsigned char>(progress * 195.0f);

            Vec2d p0 = activeTrail[i - 1];
            Vec2d p1 = activeTrail[i];
            if (refBody != nullptr) {
                p0 += refPos;
                p1 += refPos;
            }

            Vector2 s0 = camera.worldToScreen(p0);
            Vector2 s1 = camera.worldToScreen(p1);

            Color c = b.color;
            c.a = alpha;
            DrawLineEx(s0, s1, 1.5f, c);
        }
    }

    if (world.spacecraft.enabled && world.spacecraft.trail.size() >= 2) {
        const auto& sTrail = world.spacecraft.trail;
        const size_t count = sTrail.size();
        for (size_t i = 1; i < count; ++i) {
            float progress = static_cast<float>(i) / static_cast<float>(count);
            unsigned char alpha = static_cast<unsigned char>(progress * 215.0f);
            Vector2 s0 = camera.worldToScreen(sTrail[i - 1]);
            Vector2 s1 = camera.worldToScreen(sTrail[i]);
            DrawLineEx(s0, s1, 1.6f, Color{ 75, 225, 255, alpha });
        }
    }
}

void Renderer::renderPredictedTrajectory(const PredictedTrajectory& traj,
                                         const PhysicsWorld& world,
                                         const Camera2DController& camera,
                                         Color curveColor) const {
    (void)world;
    if (traj.points.size() < 2) return;

    const size_t n = traj.points.size();
    for (size_t i = 1; i < n; ++i) {
        if ((i % 2) == 0) continue;

        float fade = 1.0f - 0.65f * (static_cast<float>(i) / static_cast<float>(n));
        Color c = curveColor;
        c.a = static_cast<unsigned char>(static_cast<float>(curveColor.a) * fade);

        Vector2 s0 = camera.worldToScreen(traj.points[i - 1]);
        Vector2 s1 = camera.worldToScreen(traj.points[i]);
        DrawLineEx(s0, s1, 2.0f, c);
    }

    if (traj.hasPeriapsis) {
        Vector2 peScreen = camera.worldToScreen(traj.periapsisPoint);
        DrawCircleLines(static_cast<int>(peScreen.x), static_cast<int>(peScreen.y), 6.0f, Color{ 80, 255, 190, 240 });
        DrawText("Pe", static_cast<int>(peScreen.x) + 8, static_cast<int>(peScreen.y) - 6, 12, Color{ 80, 255, 190, 250 });
    }

    if (traj.hasApoapsis) {
        Vector2 apScreen = camera.worldToScreen(traj.apoapsisPoint);
        DrawCircleLines(static_cast<int>(apScreen.x), static_cast<int>(apScreen.y), 6.0f, Color{ 255, 210, 90, 240 });
        DrawText("Ap", static_cast<int>(apScreen.x) + 8, static_cast<int>(apScreen.y) - 6, 12, Color{ 255, 210, 90, 250 });
    }

    if (traj.hasImpact) {
        Vector2 hit = camera.worldToScreen(traj.impactPoint);
        float sz = 8.0f;
        DrawLineEx(Vector2{ hit.x - sz, hit.y - sz }, Vector2{ hit.x + sz, hit.y + sz }, 2.6f, Config::IMPACT_MARKER_COLOR);
        DrawLineEx(Vector2{ hit.x - sz, hit.y + sz }, Vector2{ hit.x + sz, hit.y - sz }, 2.6f, Config::IMPACT_MARKER_COLOR);
        DrawCircleLines(static_cast<int>(hit.x), static_cast<int>(hit.y), 11.0f, Config::IMPACT_MARKER_COLOR);
        DrawText("IMPACT", static_cast<int>(hit.x) + 14, static_cast<int>(hit.y) - 6, 12, Config::IMPACT_MARKER_COLOR);
    }
}

void Renderer::renderBlackHole(const CelestialBody& b,
                               Vector2 screenPos,
                               float screenRadius,
                               double elapsedSeconds) const {
    // 1. Full 3D Relativistic Accretion Disk & Photon Ring Shader (Mode 3 from SpaceSim2D)
    if (m_bodyShader.id != 0) {
        int bodyMode = 3;
        Vector2 lDir{ 1.0f, 0.0f };
        float mergeGeom[4] = {
            0.38f,
            static_cast<float>(b.rotationAngle),
            static_cast<float>(b.temperature),
            static_cast<float>(b.lastImpactAngle)
        };
        float mergeColOsc[4] = {
            1.0f, 0.72f, 0.35f,
            static_cast<float>(b.id * 1.73f + 1.0f)
        };
        SetShaderValue(m_bodyShader, m_locBodyMode, &bodyMode, SHADER_UNIFORM_INT);
        SetShaderValue(m_bodyShader, m_locLightDir, &lDir, SHADER_UNIFORM_VEC2);
        SetShaderValue(m_bodyShader, m_locMergeGeom, mergeGeom, SHADER_UNIFORM_VEC4);
        SetShaderValue(m_bodyShader, m_locMergeColorAndOsc, mergeColOsc, SHADER_UNIFORM_VEC4);

        float quadRadius = std::max(24.0f, screenRadius * 3.6f);
        Texture2D gasTex = m_starfield.getGasNoiseTexture();
        Rectangle src{ 0.0f, 0.0f, static_cast<float>(gasTex.width), static_cast<float>(gasTex.height) };
        Rectangle dst{ screenPos.x, screenPos.y, quadRadius * 2.0f, quadRadius * 2.0f };

        BeginShaderMode(m_bodyShader);
        DrawTexturePro(gasTex, src, dst, Vector2{ quadRadius, quadRadius }, 0.0f, WHITE);
        EndShaderMode();
        return;
    }

    Texture2D glowTex = m_starfield.getGlowTexture();
    Rectangle src{ 0.0f, 0.0f, static_cast<float>(glowTex.width), static_cast<float>(glowTex.height) };

    BeginBlendMode(BLEND_ADDITIVE);

    // 1. Gravitational lensing outer violet/cyan halo
    if (glowTex.id != 0) {
        float haloDiam = std::max(36.0f, screenRadius * 9.5f);
        Rectangle dst{ screenPos.x - haloDiam * 0.5f, screenPos.y - haloDiam * 0.5f, haloDiam, haloDiam };
        DrawTexturePro(glowTex, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f, Color{ 130, 80, 255, 95 });
    }

    // 2. Fast-spinning Doppler-shifted Relativistic Accretion Disk
    float diskInner = screenRadius * 1.45f;
    float diskOuter = screenRadius * 3.85f;
    float spinDeg = static_cast<float>(elapsedSeconds * 140.0 + b.id * 45.0);

    for (int band = 0; band < 5; ++band) {
        float frac = static_cast<float>(band) / 4.0f;
        float rInner = diskInner + (diskOuter - diskInner) * frac * 0.82f;
        float rOuter = rInner + (diskOuter - diskInner) * 0.28f;

        unsigned char alpha = static_cast<unsigned char>((1.0f - frac * 0.65f) * 145.0f);
        DrawRing(screenPos, rInner, rOuter, spinDeg + band * 22.0f, spinDeg + 195.0f + band * 22.0f, 32,
                 Color{ 255, 165, 55, alpha });
        DrawRing(screenPos, rInner, rOuter, spinDeg + 180.0f + band * 22.0f, spinDeg + 360.0f + band * 22.0f, 32,
                 Color{ 170, 95, 255, static_cast<unsigned char>(alpha * 0.85f) });
    }

    DrawRing(screenPos, screenRadius * 1.06f, screenRadius * 1.32f, 0.0f, 360.0f, 48, Color{ 255, 235, 190, 245 });
    DrawRing(screenPos, screenRadius * 1.15f, screenRadius * 1.55f, 0.0f, 360.0f, 48, Color{ 255, 140, 45, 180 });

    EndBlendMode();

    DrawCircleV(screenPos, screenRadius * 1.08f, Color{ 15, 8, 25, 255 });
    DrawCircleV(screenPos, screenRadius, BLACK);
}

void Renderer::renderStarBody(const CelestialBody& b,
                              Vector2 screenPos,
                              float screenRadius,
                              double elapsedSeconds) const {
    Texture2D glowTex = m_starfield.getGlowTexture();
    Texture2D plasmaTex = m_starfield.getStarPlasmaTexture();
    Texture2D lavaTex = m_starfield.getLavaTexture();

    float pulse = 0.94f + 0.06f * std::sin(static_cast<float>(elapsedSeconds * 3.2 + b.id));

    BeginBlendMode(BLEND_ADDITIVE);

    // 1. Neutron Pulsar twin relativistic magnetic beams
    if (b.bodyType == CelestialBodyType::NeutronStar) {
        float beamLen = std::max(45.0f, screenRadius * 11.0f);
        float angle = static_cast<float>(b.rotationAngle);
        Vector2 dir{ std::cos(angle), std::sin(angle) };
        Vector2 perp{ -dir.y, dir.x };

        for (int side = -1; side <= 1; side += 2) {
            Vector2 tip{ screenPos.x + dir.x * beamLen * side, screenPos.y + dir.y * beamLen * side };
            Vector2 b1{ tip.x + perp.x * (screenRadius * 2.2f), tip.y + perp.y * (screenRadius * 2.2f) };
            Vector2 b2{ tip.x - perp.x * (screenRadius * 2.2f), tip.y - perp.y * (screenRadius * 2.2f) };
            DrawTriangle(screenPos, b1, b2, Color{ 90, 225, 255, 95 });
            DrawTriangle(screenPos, b2, b1, Color{ 90, 225, 255, 95 });
            DrawLineEx(screenPos, tip, 2.2f, Color{ 210, 250, 255, 195 });
        }
    }

    // 2. Multi-layered boiling additive corona glow
    if (glowTex.id != 0) {
        Rectangle src{ 0.0f, 0.0f, static_cast<float>(glowTex.width), static_cast<float>(glowTex.height) };
        float boost = (b.bodyType == CelestialBodyType::BlueHypergiant) ? 1.35f : 1.0f;
        float scales[3] = {
            6.8f * starGlowIntensity * pulse * boost,
            3.5f * starGlowIntensity * boost,
            1.85f * starGlowIntensity
        };
        unsigned char alphas[3] = { 95, 175, 245 };

        for (int layer = 0; layer < 3; ++layer) {
            float drawDiam = std::max(18.0f, screenRadius * 2.0f * scales[layer]);
            Rectangle dst{ screenPos.x - drawDiam * 0.5f, screenPos.y - drawDiam * 0.5f, drawDiam, drawDiam };
            Color tint = b.color;
            tint.a = alphas[layer];
            DrawTexturePro(glowTex, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f, tint);
        }
    }

    // 3. Tapered Starburst Diffraction Spikes (Additive Blending)
    {
        float spikeLen = std::max(45.0f, screenRadius * 4.6f * starGlowIntensity * pulse);
        float spikeHalfW = std::max(2.0f, screenRadius * 0.22f);
        drawTaperedNeedleSpike(screenPos, 0.0f,       spikeLen, spikeHalfW, b.color, 0.72f);
        drawTaperedNeedleSpike(screenPos, 1.5707963f, spikeLen, spikeHalfW, b.color, 0.72f);

        float diagLen = spikeLen * 0.52f;
        drawTaperedNeedleSpike(screenPos,  0.785398f, diagLen, spikeHalfW * 0.65f, b.color, 0.45f);
        drawTaperedNeedleSpike(screenPos, -0.785398f, diagLen, spikeHalfW * 0.65f, b.color, 0.45f);
    }

    // 4. Animated Solar Prominences & Flares on the stellar rim
    if (screenRadius >= 6.0f && b.bodyType != CelestialBodyType::NeutronStar) {
        int flareCount = 6;
        for (int f = 0; f < flareCount; ++f) {
            float baseAng = static_cast<float>(b.rotationAngle * 25.0 + f * 60.0f +
                                               12.0 * std::sin(elapsedSeconds * 1.7 + f * 2.1));
            float flareWave = 0.5f + 0.5f * std::sin(static_cast<float>(elapsedSeconds * 2.6 + f * 1.9 + b.id));
            float flareHeight = screenRadius * (0.18f + 0.32f * flareWave);

            float radAng = baseAng * (3.14159265f / 180.0f);
            Vector2 loopCenter{
                screenPos.x + std::cos(radAng) * (screenRadius * 0.92f),
                screenPos.y + std::sin(radAng) * (screenRadius * 0.92f)
            };
            Color flareCol = b.color;
            flareCol.a = static_cast<unsigned char>(135.0f * flareWave);
            DrawRing(loopCenter, flareHeight * 0.55f, flareHeight, baseAng - 70.0f, baseAng + 70.0f, 16, flareCol);
        }
    }

    EndBlendMode();

    // 5. 3D-Shaded Photosphere Disc with Convective Granulation, Sunspots & Limb Darkening
    if (m_bodyShader.id != 0 && plasmaTex.id != 0) {
        int bodyMode = 2;
        Vector2 lDir{ 1.0f, 0.0f };
        float mergeGeom[4] = {
            0.58f,
            static_cast<float>(b.rotationAngle),
            static_cast<float>(b.temperature),
            static_cast<float>(b.lastImpactAngle)
        };
        float mergeColOsc[4] = {
            b.color.r / 255.0f,
            b.color.g / 255.0f,
            b.color.b / 255.0f,
            static_cast<float>(b.textureSeed % 1024u)
        };

        SetShaderValue(m_bodyShader, m_locBodyMode, &bodyMode, SHADER_UNIFORM_INT);
        SetShaderValue(m_bodyShader, m_locLightDir, &lDir, SHADER_UNIFORM_VEC2);
        SetShaderValue(m_bodyShader, m_locMergeGeom, mergeGeom, SHADER_UNIFORM_VEC4);
        SetShaderValue(m_bodyShader, m_locMergeColorAndOsc, mergeColOsc, SHADER_UNIFORM_VEC4);

        float quadRadius = screenRadius / 0.58f;
        Rectangle src{ 0.0f, 0.0f, static_cast<float>(plasmaTex.width), static_cast<float>(plasmaTex.height) };
        Rectangle dst{ screenPos.x, screenPos.y, quadRadius * 2.0f, quadRadius * 2.0f };

        BeginShaderMode(m_bodyShader);
        DrawTexturePro(plasmaTex, src, dst, Vector2{ quadRadius, quadRadius }, 0.0f, b.color);
        EndShaderMode();
    } else {
        DrawCircleV(screenPos, screenRadius, b.color);
        if (lavaTex.id != 0 && screenRadius >= 4.0f) {
            BeginBlendMode(BLEND_ADDITIVE);
            Rectangle src{ 0.0f, 0.0f, static_cast<float>(lavaTex.width), static_cast<float>(lavaTex.height) };
            float d = screenRadius * 2.0f;
            Rectangle dst{ screenPos.x, screenPos.y, d, d };
            DrawTexturePro(lavaTex, src, dst, Vector2{ screenRadius, screenRadius },
                           static_cast<float>(b.rotationAngle * 57.2958), Color{ 255, 240, 195, 165 });
            EndBlendMode();
        }
        DrawCircleV(screenPos, screenRadius * 0.68f, Color{ 255, 252, 240, 235 });
    }
}

void Renderer::renderPlanetBody(const CelestialBody& b,
                                Vector2 screenPos,
                                float screenRadius,
                                bool hasLightSource,
                                Vec2d lightDir,
                                double elapsedSeconds) const {
    Texture2D glowTex = m_starfield.getGlowTexture();

    // 1. Atmospheric Rayleigh Scattering Rim Halo (Additive Blending)
    if (glowTex.id != 0 && (b.hasAtmosphere() || b.temperature > 1100.0 || b.impactHeat > 0.05)) {
        BeginBlendMode(BLEND_ADDITIVE);
        float haloDiam = screenRadius * 2.95f;
        Rectangle src{ 0.0f, 0.0f, static_cast<float>(glowTex.width), static_cast<float>(glowTex.height) };
        Rectangle dst{ screenPos.x - haloDiam * 0.5f, screenPos.y - haloDiam * 0.5f, haloDiam, haloDiam };

        Color atmoCol;
        if (b.temperature > 1200.0 || b.impactHeat > 0.35) {
            atmoCol = Color{ 255, 110, 30, 125 };
        } else if (b.bodyType == CelestialBodyType::RockyPlanet) {
            // Glowing cyan/blue Rayleigh atmosphere for Terran worlds
            atmoCol = Color{ 65, 185, 255, 88 };
        } else if (b.bodyType == CelestialBodyType::GasGiant) {
            // Warm amber/gold atmospheric scattering for Gas Giants
            atmoCol = Color{ 255, 190, 95, 78 };
        } else {
            atmoCol = Color{ b.color.r, b.color.g, b.color.b, 60 };
        }
        DrawTexturePro(glowTex, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f, atmoCol);
        EndBlendMode();
    }

    // 2. Back half of 3D Tilted Planetary Rings (behind planet sphere, with planetary shadow casting)
    if (b.hasRings && screenRadius >= 4.0f) {
        drawPlanetaryRings3D(screenPos, screenRadius, b.ringColor, lightDir, false);
    }

    // 3. 3D Spherical Normal-Mapped Planet / Moon / Asteroid Shader Pass
    Texture2D noiseTex{};
    int bodyMode = 0;
    if (b.bodyType == CelestialBodyType::GasGiant) {
        bodyMode = 1;
        noiseTex = m_starfield.getGasNoiseTexture();
    } else if (b.bodyType == CelestialBodyType::Asteroid) {
        bodyMode = 4;
        noiseTex = m_starfield.getRockyNoiseTexture();
    } else {
        bodyMode = 0;
        noiseTex = m_starfield.getRockyNoiseTexture();
    }

    if (m_bodyShader.id != 0 && noiseTex.id != 0) {
        float heatBoost = 1.0f + static_cast<float>(std::clamp(b.impactHeat, 0.0, 2.0)) * 3.0f;
        Vector2 lDir = hasLightSource
            ? Vector2{ static_cast<float>(lightDir.x) * heatBoost, static_cast<float>(lightDir.y) * heatBoost }
            : Vector2{ 0.8f * heatBoost, -0.6f * heatBoost };

        float mergeGeom[4] = {
            0.58f,
            static_cast<float>(b.rotationAngle),
            static_cast<float>(b.temperature),
            static_cast<float>(b.lastImpactAngle)
        };
        float mergeColOsc[4] = {
            b.color.r / 255.0f,
            b.color.g / 255.0f,
            b.color.b / 255.0f,
            static_cast<float>(b.textureSeed % 1024u)
        };

        SetShaderValue(m_bodyShader, m_locBodyMode, &bodyMode, SHADER_UNIFORM_INT);
        SetShaderValue(m_bodyShader, m_locLightDir, &lDir, SHADER_UNIFORM_VEC2);
        SetShaderValue(m_bodyShader, m_locMergeGeom, mergeGeom, SHADER_UNIFORM_VEC4);
        SetShaderValue(m_bodyShader, m_locMergeColorAndOsc, mergeColOsc, SHADER_UNIFORM_VEC4);

        float quadRadius = screenRadius / 0.58f;
        Rectangle src{ 0.0f, 0.0f, static_cast<float>(noiseTex.width), static_cast<float>(noiseTex.height) };
        Rectangle dst{ screenPos.x, screenPos.y, quadRadius * 2.0f, quadRadius * 2.0f };

        BeginShaderMode(m_bodyShader);
        DrawTexturePro(noiseTex, src, dst, Vector2{ quadRadius, quadRadius }, 0.0f, b.color);
        EndShaderMode();
    } else {
        // CPU Fallback if OpenGL 3.3 shader is unavailable
        Texture2D surfTex{};
        Color surfTint = WHITE;
        if (b.bodyType == CelestialBodyType::RockyPlanet) {
            surfTex = m_starfield.getTerranTexture(b.textureSeed);
        } else if (b.bodyType == CelestialBodyType::GasGiant) {
            surfTex = m_starfield.getGasGiantTexture(b.textureSeed);
            surfTint = Color{
                static_cast<unsigned char>(140 + b.color.r * 0.45f),
                static_cast<unsigned char>(140 + b.color.g * 0.45f),
                static_cast<unsigned char>(140 + b.color.b * 0.45f),
                255
            };
        } else {
            surfTex = m_starfield.getCrateredTexture(b.textureSeed);
            surfTint = b.color;
        }

        float rotDeg = static_cast<float>(b.rotationAngle * 57.2957795);
        float diam = screenRadius * 2.0f;

        if (surfTex.id != 0) {
            Rectangle src{ 0.0f, 0.0f, static_cast<float>(surfTex.width), static_cast<float>(surfTex.height) };
            Rectangle dst{ screenPos.x, screenPos.y, diam, diam };
            DrawTexturePro(surfTex, src, dst, Vector2{ screenRadius, screenRadius }, rotDeg, surfTint);
        } else {
            DrawCircleV(screenPos, screenRadius, b.color);
        }

        if (b.bodyType == CelestialBodyType::RockyPlanet && b.temperature < 750.0 && b.temperature > 160.0) {
            Texture2D cloudTex = m_starfield.getCloudTexture();
            if (cloudTex.id != 0) {
                Rectangle src{ 0.0f, 0.0f, static_cast<float>(cloudTex.width), static_cast<float>(cloudTex.height) };
                Rectangle dst{ screenPos.x, screenPos.y, diam, diam };
                float cloudRot = rotDeg * 1.45f + static_cast<float>(elapsedSeconds * 6.0);
                DrawTexturePro(cloudTex, src, dst, Vector2{ screenRadius, screenRadius }, cloudRot, WHITE);
            }
        }
    }

    // 4. Sunlit Rayleigh Atmospheric Scattering Rim Arc on the Star-Facing Limb
    if (hasLightSource && b.hasAtmosphere() && screenRadius >= 4.5f) {
        BeginBlendMode(BLEND_ADDITIVE);
        float lightAngleDeg = static_cast<float>(std::atan2(lightDir.y, lightDir.x) * 57.2957795);
        Color rimCol = (b.bodyType == CelestialBodyType::RockyPlanet)
                       ? Color{ 85, 205, 255, 165 }
                       : Color{ 255, 205, 115, 135 };
        DrawRing(screenPos, screenRadius * 0.94f, screenRadius * 1.07f,
                 lightAngleDeg - 86.0f, lightAngleDeg + 86.0f, 32, rimCol);
        EndBlendMode();
    }

    // 5. Localized Post-Impact Molten Scar Halo (fades via radiative cooling)
    if (b.impactHeat > 0.02 && glowTex.id != 0) {
        BeginBlendMode(BLEND_ADDITIVE);
        float heatAlpha = static_cast<float>(std::clamp(b.impactHeat, 0.0, 1.0));
        Vector2 scarCenter{
            screenPos.x + std::cos(static_cast<float>(b.lastImpactAngle)) * (screenRadius * 0.55f),
            screenPos.y + std::sin(static_cast<float>(b.lastImpactAngle)) * (screenRadius * 0.55f)
        };
        float scarDiam = screenRadius * 1.65f;
        Rectangle src{ 0.0f, 0.0f, static_cast<float>(glowTex.width), static_cast<float>(glowTex.height) };
        Rectangle dst{ scarCenter.x - scarDiam * 0.5f, scarCenter.y - scarDiam * 0.5f, scarDiam, scarDiam };
        DrawTexturePro(glowTex, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f,
                       Color{ 255, 130, 35, static_cast<unsigned char>(heatAlpha * 185.0f) });
        EndBlendMode();
    }

    // 6. Front half of 3D Tilted Planetary Rings (drawn in front of the planet sphere)
    if (b.hasRings && screenRadius >= 4.0f) {
        drawPlanetaryRings3D(screenPos, screenRadius, b.ringColor, lightDir, true);
    }
}

void Renderer::renderCelestialBodies(const PhysicsWorld& world,
                                     const Camera2DController& camera,
                                     int screenWidth,
                                     int screenHeight,
                                     double elapsedSeconds) const {
    // Collect luminous stars for directional planetary lighting
    struct LightSource {
        Vec2d pos;
        double luminosity;
    };
    std::vector<LightSource> stars;
    stars.reserve(16);
    for (const auto& b : world.bodies) {
        if (b.alive && b.isLuminousStar()) {
            stars.push_back({ b.position, b.mass });
        }
    }

    for (const auto& b : world.bodies) {
        if (!b.alive) continue;

        Vector2 screenPos = camera.worldToScreen(b.position);
        // Mandate: Minimum Screen-Space Radius Clamping (3px moons/asteroids, 6px planets, 16px stars/black holes)
        float screenRadius = std::max(b.getMinScreenRadius(), static_cast<float>(b.radius * camera.zoom));

        float cullMargin = std::max(90.0f, screenRadius * 9.5f);
        if (screenPos.x + cullMargin < 0 || screenPos.x - cullMargin > screenWidth ||
            screenPos.y + cullMargin < 0 || screenPos.y - cullMargin > screenHeight) {
            continue;
        }

        if (b.isBlackHole()) {
            renderBlackHole(b, screenPos, screenRadius, elapsedSeconds);
        } else if (b.isLuminousStar()) {
            renderStarBody(b, screenPos, screenRadius, elapsedSeconds);
        } else {
            // Find nearest/brightest illuminating star for 3D spherical lighting & terminator crescent
            bool hasLight = false;
            Vec2d bestLightDir{ 1.0, 0.0 };
            double bestFlux = -1.0;

            for (const auto& st : stars) {
                Vec2d toStar = st.pos - b.position;
                double dSq = std::max(1.0, toStar.lengthSq());
                double flux = st.luminosity / dSq;
                if (flux > bestFlux) {
                    bestFlux = flux;
                    bestLightDir = toStar / std::sqrt(dSq);
                    hasLight = true;
                }
            }

            renderPlanetBody(b, screenPos, screenRadius, hasLight, bestLightDir, elapsedSeconds);
        }

        // Selection Reticle
        if (static_cast<int>(b.id) == world.selectedBodyId) {
            float selRad = (b.hasRings ? screenRadius * static_cast<float>(b.ringOuterRadiusRatio) : screenRadius) + 7.0f;
            DrawCircleLines(static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), selRad, Color{ 0, 240, 255, 245 });
            DrawCircleLines(static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), selRad + 2.5f, Color{ 0, 240, 255, 115 });
        }

        // Crisp body label (skip tiny asteroids to keep viewport clean)
        if (showBodyLabels &&
            b.bodyType != CelestialBodyType::Asteroid &&
            (b.isLuminousStar() || b.isBlackHole() || b.mass >= 1.5 || static_cast<int>(b.id) == world.selectedBodyId)) {
            float labelOffset = (b.hasRings ? screenRadius * static_cast<float>(b.ringOuterRadiusRatio) : screenRadius) + 7.0f;
            DrawText(b.name.c_str(),
                     static_cast<int>(screenPos.x + labelOffset),
                     static_cast<int>(screenPos.y - 6.0f),
                     12,
                     Color{ 220, 238, 255, 215 });
        }
    }
}

void Renderer::renderSpacecraft(const Spacecraft& ship,
                                const Camera2DController& camera,
                                double elapsedSeconds) const {
    Texture2D glowTex = m_starfield.getGlowTexture();

    // 1. Render plasma exhaust particles and white RCS puffs
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& p : ship.exhaustParticles) {
        Vector2 sp = camera.worldToScreen(p.position);
        float lifeRatio = 1.0f - std::clamp(p.age / p.maxLife, 0.0f, 1.0f);
        Color c = p.color;
        c.a = static_cast<unsigned char>(lifeRatio * 235.0f);
        float rad = std::clamp(static_cast<float>(p.size * camera.zoom * lifeRatio), 1.4f, 5.5f);
        DrawCircleV(sp, rad, c);
    }

    Vector2 shipScreen = camera.worldToScreen(ship.position);
    // Mandate: Nimble 12-16 pixel scale on screen so the ship never looks oversized
    float sz = std::clamp(static_cast<float>(ship.radius * camera.zoom * 2.4), 11.0f, 16.5f);

    Vec2d fwd = ship.getForwardHeading();
    Vec2d right = fwd.perpendicular();

    auto toScreenPt = [&](double fwdMul, double rightMul) -> Vector2 {
        return Vector2{
            shipScreen.x + static_cast<float>((fwd.x * fwdMul + right.x * rightMul) * sz),
            shipScreen.y + static_cast<float>((fwd.y * fwdMul + right.y * rightMul) * sz)
        };
    };

    // 2. Flickering Multi-Layered Plasma Exhaust Plumes from Dual Rear Nacelles
    if (ship.firingMain && ship.throttle > 0.01) {
        float flicker = 0.85f + 0.30f * std::sin(static_cast<float>(elapsedSeconds * 55.0));
        float plumeLen = (1.1f + 1.4f * static_cast<float>(ship.throttle)) * flicker;

        for (int side = -1; side <= 1; side += 2) {
            double nacY = side * 0.34;
            Vector2 nozCenter = toScreenPt(-0.82, nacY);
            Vector2 nozLeft   = toScreenPt(-0.80, nacY - 0.14);
            Vector2 nozRight  = toScreenPt(-0.80, nacY + 0.14);
            Vector2 outerTip  = toScreenPt(-0.82 - plumeLen, nacY);
            Vector2 innerTip  = toScreenPt(-0.82 - plumeLen * 0.62f, nacY);

            // Outer cyan/violet plasma cone
            DrawTriangle(nozLeft, nozRight, outerTip, Color{ 45, 155, 255, 195 });
            DrawTriangle(nozRight, nozLeft, outerTip, Color{ 45, 155, 255, 195 });

            // Blazing white-blue inner plasma core
            DrawTriangle(nozLeft, nozRight, innerTip, Color{ 230, 250, 255, 245 });
            DrawTriangle(nozRight, nozLeft, innerTip, Color{ 230, 250, 255, 245 });

            if (glowTex.id != 0) {
                float gDiam = sz * 2.1f;
                Rectangle src{ 0.0f, 0.0f, static_cast<float>(glowTex.width), static_cast<float>(glowTex.height) };
                Rectangle dst{ nozCenter.x - gDiam * 0.5f, nozCenter.y - gDiam * 0.5f, gDiam, gDiam };
                DrawTexturePro(glowTex, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f, Color{ 70, 190, 255, 185 });
            }
        }
    }

    // 3. White RCS Gas Jets from Nose / Wingtips when Rotating or Retro-Braking
    if (ship.firingRetro) {
        for (int side = -1; side <= 1; side += 2) {
            Vector2 noseNoz = toScreenPt(0.75, side * 0.22);
            Vector2 jetTip  = toScreenPt(1.45, side * 0.32);
            DrawLineEx(noseNoz, jetTip, 2.2f, Color{ 215, 245, 255, 225 });
        }
    }
    if (ship.rotatingCCW) {
        DrawLineEx(toScreenPt(0.55, 0.30), toScreenPt(0.55, 0.95), 2.0f, Color{ 225, 248, 255, 225 });
        DrawLineEx(toScreenPt(-0.55, -0.70), toScreenPt(-0.55, -1.25), 2.0f, Color{ 225, 248, 255, 225 });
    }
    if (ship.rotatingCW) {
        DrawLineEx(toScreenPt(0.55, -0.30), toScreenPt(0.55, -0.95), 2.0f, Color{ 225, 248, 255, 225 });
        DrawLineEx(toScreenPt(-0.55, 0.70), toScreenPt(-0.55, 1.25), 2.0f, Color{ 225, 248, 255, 225 });
    }

    EndBlendMode();

    // 4. Multi-Part Sci-Fi Explorer Spacecraft Hull
    Vector2 nose        = toScreenPt( 1.15,  0.00);
    Vector2 shoulderL   = toScreenPt( 0.25, -0.32);
    Vector2 shoulderR   = toScreenPt( 0.25,  0.32);
    Vector2 wingtipL    = toScreenPt(-0.55, -0.92);
    Vector2 wingtipR    = toScreenPt(-0.55,  0.92);
    Vector2 wingTrailL  = toScreenPt(-0.78, -0.52);
    Vector2 wingTrailR  = toScreenPt(-0.78,  0.52);
    Vector2 tailCenter  = toScreenPt(-0.68,  0.00);

    // Swept delta wings (dark titanium base)
    DrawTriangle(shoulderL, wingtipL, wingTrailL, Color{ 58, 74, 98, 255 });
    DrawTriangle(wingTrailL, wingtipL, shoulderL, Color{ 58, 74, 98, 255 });
    DrawTriangle(shoulderR, wingTrailR, wingtipR, Color{ 58, 74, 98, 255 });
    DrawTriangle(wingtipR, wingTrailR, shoulderR, Color{ 58, 74, 98, 255 });

    // Dual rear engine nacelles
    for (int side = -1; side <= 1; side += 2) {
        double ny = side * 0.34;
        Vector2 nFront = toScreenPt(-0.15, ny);
        Vector2 nBackL = toScreenPt(-0.84, ny - 0.14);
        Vector2 nBackR = toScreenPt(-0.84, ny + 0.14);
        DrawTriangle(nFront, nBackL, nBackR, Color{ 38, 46, 62, 255 });
        DrawTriangle(nBackR, nBackL, nFront, Color{ 38, 46, 62, 255 });
    }

    // Central fuselage spine (bright metallic silver-blue)
    DrawTriangle(nose, shoulderL, tailCenter, Color{ 210, 225, 245, 255 });
    DrawTriangle(tailCenter, shoulderL, nose, Color{ 210, 225, 245, 255 });
    DrawTriangle(nose, tailCenter, shoulderR, Color{ 175, 195, 222, 255 });
    DrawTriangle(shoulderR, tailCenter, nose, Color{ 175, 195, 222, 255 });

    // Glowing cyan cockpit canopy glass
    Vector2 canFront = toScreenPt( 0.68,  0.00);
    Vector2 canLeft  = toScreenPt( 0.05, -0.17);
    Vector2 canRight = toScreenPt( 0.05,  0.17);
    Vector2 canBack  = toScreenPt(-0.18,  0.00);
    DrawTriangle(canFront, canLeft, canBack, Color{ 0, 225, 255, 245 });
    DrawTriangle(canBack, canLeft, canFront, Color{ 0, 225, 255, 245 });
    DrawTriangle(canFront, canBack, canRight, Color{ 0, 180, 235, 245 });
    DrawTriangle(canRight, canBack, canFront, Color{ 0, 180, 235, 245 });

    // Crisp hull rim highlights & navigation lights
    DrawLineEx(nose, wingtipL, 1.3f, Color{ 110, 210, 255, 215 });
    DrawLineEx(nose, wingtipR, 1.3f, Color{ 110, 210, 255, 215 });
    DrawCircleV(wingtipL, 1.8f, Color{ 255, 60, 60, 240 });  // Port red strobe
    DrawCircleV(wingtipR, 1.8f, Color{ 60, 255, 110, 240 }); // Starboard green strobe

    // 5. Subtle Targeting Diamond when zoomed out so you never lose track of the ship
    if (camera.zoom < 0.95) {
        float dRad = 18.0f;
        Vector2 top{ shipScreen.x, shipScreen.y - dRad };
        Vector2 bot{ shipScreen.x, shipScreen.y + dRad };
        Vector2 lft{ shipScreen.x - dRad, shipScreen.y };
        Vector2 rgt{ shipScreen.x + dRad, shipScreen.y };
        Color diamondCol{ 0, 235, 255, 185 };
        DrawLineEx(top, rgt, 1.4f, diamondCol);
        DrawLineEx(rgt, bot, 1.4f, diamondCol);
        DrawLineEx(bot, lft, 1.4f, diamondCol);
        DrawLineEx(lft, top, 1.4f, diamondCol);
        DrawText("EXPLORER-1", static_cast<int>(shipScreen.x + dRad + 5.0f), static_cast<int>(shipScreen.y - 6.0f), 11, diamondCol);
    }
}

void Renderer::renderVectorOverlays(const PhysicsWorld& world, const Camera2DController& camera) const {
    for (const auto& b : world.bodies) {
        if (!b.alive) continue;
        if (b.mass < 1.5 && world.bodies.size() > 150) continue;

        Vector2 start = camera.worldToScreen(b.position);

        if (showVelocityVectors) {
            double vMag = b.velocity.length();
            if (vMag > 1e-3) {
                Vec2d dir = b.velocity / vMag;
                float arrowLen = static_cast<float>(std::clamp(16.0 * std::log1p(vMag), 10.0, 95.0));
                Vector2 end{
                    start.x + static_cast<float>(dir.x) * arrowLen,
                    start.y + static_cast<float>(dir.y) * arrowLen
                };
                drawArrowScreen(start, end, 1.7f, Config::VELOCITY_VEC_COLOR);
            }
        }

        if (showAccelerationVectors) {
            double aMag = b.acceleration.length();
            if (aMag > 1e-3) {
                Vec2d dir = b.acceleration / aMag;
                float arrowLen = static_cast<float>(std::clamp(14.0 * std::log1p(aMag), 8.0, 85.0));
                Vector2 end{
                    start.x + static_cast<float>(dir.x) * arrowLen,
                    start.y + static_cast<float>(dir.y) * arrowLen
                };
                drawArrowScreen(start, end, 1.7f, Config::ACCEL_VEC_COLOR);
            }
        }
    }
}

void Renderer::renderImpactSparksAndShockwaves(const PhysicsWorld& world,
                                               const Camera2DController& camera) const {
    Texture2D glowTex = m_starfield.getGlowTexture();

    BeginBlendMode(BLEND_ADDITIVE);

    // 1. Glowing hot debris sparks with velocity streaks
    for (const auto& sp : world.impactSparks) {
        float lifeRatio = 1.0f - std::clamp(sp.age / sp.maxLife, 0.0f, 1.0f);
        Vector2 head = camera.worldToScreen(sp.position);
        Vector2 tail = camera.worldToScreen(sp.position - sp.velocity * 0.028);

        Color c = sp.color;
        c.a = static_cast<unsigned char>(lifeRatio * 245.0f);
        DrawLineEx(tail, head, std::max(1.2f, sp.size * lifeRatio), c);
        DrawCircleV(head, std::max(1.2f, sp.size * 0.75f * lifeRatio), Color{ 255, 250, 220, c.a });
    }

    // 2. Impact Flashes & Expanding Shockwave Rings
    for (const auto& sw : world.shockwaves) {
        Vector2 center = camera.worldToScreen(sw.position);
        float screenRad = static_cast<float>(sw.currentRadius * camera.zoom);
        float progress = std::clamp(sw.age / sw.duration, 0.0f, 1.0f);

        // Intense initial impact flash during the first 35% of the shockwave lifetime
        if (progress < 0.35f && glowTex.id != 0) {
            float flashFade = 1.0f - (progress / 0.35f);
            float flashDiam = std::max(28.0f, static_cast<float>(sw.flashRadius * 2.5 * camera.zoom));
            Rectangle src{ 0.0f, 0.0f, static_cast<float>(glowTex.width), static_cast<float>(glowTex.height) };
            Rectangle dst{ center.x - flashDiam * 0.5f, center.y - flashDiam * 0.5f, flashDiam, flashDiam };
            DrawTexturePro(glowTex, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f,
                           Color{ 255, 245, 215, static_cast<unsigned char>(flashFade * 245.0f) });

            float spikeLen = std::max(36.0f, flashDiam * 1.4f) * flashFade;
            drawTaperedNeedleSpike(center, 0.0f,       spikeLen, 3.5f, sw.color, flashFade * 0.85f);
            drawTaperedNeedleSpike(center, 1.5707963f, spikeLen, 3.5f, sw.color, flashFade * 0.85f);
        }

        Color c = sw.color;
        c.a = static_cast<unsigned char>((1.0f - progress) * 240.0f);
        float ringThickness = sw.isSupernova ? 6.5f : 3.5f;
        DrawRing(center, std::max(1.0f, screenRad - ringThickness), screenRad + ringThickness, 0.0f, 360.0f, 48, c);
    }

    EndBlendMode();
}

void Renderer::renderSpawnerPreview(const SpawnerPreviewState& preview,
                                    const PhysicsWorld& world,
                                    const Camera2DController& camera,
                                    double elapsedSeconds) const {
    renderPredictedTrajectory(preview.trajectory, world, camera, Color{ 110, 255, 165, 240 });

    Vector2 spawnScreen = camera.worldToScreen(preview.spawnWorldPos);
    float screenRad = std::max(4.0f, static_cast<float>(preview.prototypeBody.radius * camera.zoom));

    if (preview.prototypeBody.isBlackHole()) {
        renderBlackHole(preview.prototypeBody, spawnScreen, screenRad, elapsedSeconds);
    } else if (preview.prototypeBody.isLuminousStar()) {
        renderStarBody(preview.prototypeBody, spawnScreen, screenRad, elapsedSeconds);
    } else {
        renderPlanetBody(preview.prototypeBody, spawnScreen, screenRad, true, Vec2d(1.0, -0.4).normalized(), elapsedSeconds);
    }

    DrawCircleLines(static_cast<int>(spawnScreen.x), static_cast<int>(spawnScreen.y), screenRad + 3.0f, Color{ 0, 240, 255, 220 });

    Vector2 mouseScreen = GetMousePosition();
    drawDashedLineScreen(spawnScreen, mouseScreen, 6.0f, 1.6f, Color{ 255, 220, 100, 200 });

    Vec2d velEndWorld = preview.spawnWorldPos + preview.launchVelocity * 0.6;
    Vector2 velEndScreen = camera.worldToScreen(velEndWorld);
    drawArrowScreen(spawnScreen, velEndScreen, 2.2f, Config::VELOCITY_VEC_COLOR);

    char infoBuf[128];
    std::snprintf(infoBuf, sizeof(infoBuf), "%s | M=%.1f | v=%.1f%s",
                  preview.prototypeBody.name.c_str(),
                  preview.prototypeBody.mass,
                  preview.launchVelocity.length(),
                  preview.circularSnapActive ? " [AUTO-ORBIT LOCK]" : "");
    DrawText(infoBuf, static_cast<int>(spawnScreen.x + screenRad + 10.0f), static_cast<int>(spawnScreen.y - 16.0f), 13, RAYWHITE);
}

void Renderer::drawArrowScreen(Vector2 start, Vector2 end, float thickness, Color color) {
    DrawLineEx(start, end, thickness, color);

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 4.0f) return;

    float ux = dx / len;
    float uy = dy / len;
    float headLen = std::min(9.0f, len * 0.4f);
    float headWidth = headLen * 0.55f;

    Vector2 left{
        end.x - ux * headLen - uy * headWidth,
        end.y - uy * headLen + ux * headWidth
    };
    Vector2 right{
        end.x - ux * headLen + uy * headWidth,
        end.y - uy * headLen - ux * headWidth
    };

    DrawLineEx(end, left, thickness, color);
    DrawLineEx(end, right, thickness, color);
}

void Renderer::drawDashedLineScreen(Vector2 start, Vector2 end, float dashLen, float thickness, Color color) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float totalLen = std::sqrt(dx * dx + dy * dy);
    if (totalLen < 1.0f || dashLen <= 1.0f) return;

    float ux = dx / totalLen;
    float uy = dy / totalLen;
    int segments = static_cast<int>(totalLen / (dashLen * 2.0f));

    for (int i = 0; i <= segments; ++i) {
        float d0 = static_cast<float>(i) * dashLen * 2.0f;
        float d1 = std::min(totalLen, d0 + dashLen);
        Vector2 p0{ start.x + ux * d0, start.y + uy * d0 };
        Vector2 p1{ start.x + ux * d1, start.y + uy * d1 };
        DrawLineEx(p0, p1, thickness, color);
    }
}

} // namespace sim
