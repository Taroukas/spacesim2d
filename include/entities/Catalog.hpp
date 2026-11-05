#pragma once

#include "core/Config.hpp"
#include "physics/CelestialBody.hpp"
#include <vector>
#include <string>
#include <algorithm>

namespace sim {

enum class CatalogCategory : int {
    All = 0,
    MoonsAndPlanets = 1,
    Asteroids = 2,
    Stars = 3,
    Giants = 4,
    CompactAndBlackHoles = 5
};

struct CatalogEntry {
    int index;
    const char* name;
    const char* subtitle;
    CelestialBodyType bodyType;
    CatalogCategory category;
    double massEarth;
    double radiusEarth;
    float temperatureK;
    Color color;
    bool hasRings;
    Color ringColor;
    double simMass;
    double simRadius;
};

class StellarCatalog {
public:
    static const std::vector<CatalogEntry>& getEntries() {
        static const std::vector<CatalogEntry> catalog = {
            // Moons and Dwarf Planets
            { 0,  "Moon (Luna)",         "0.0123 M_E | 1,737 km",         CelestialBodyType::CrateredMoon,   CatalogCategory::MoonsAndPlanets, 0.0123,  0.2727, 220.0f,  Color{ 200, 205, 215, 255 }, false, WHITE, 0.15, 3.2 },
            { 1,  "Io (Jupiter I)",      "0.0150 M_E | Volcanic World",   CelestialBodyType::CrateredMoon,   CatalogCategory::MoonsAndPlanets, 0.0150,  0.2859, 130.0f,  Color{ 245, 220, 90, 255 },  false, WHITE, 0.18, 3.4 },
            { 2,  "Europa (Jupiter II)", "0.0080 M_E | Subsurface Ocean", CelestialBodyType::CrateredMoon,   CatalogCategory::MoonsAndPlanets, 0.0080,  0.2450, 102.0f,  Color{ 180, 215, 240, 255 }, false, WHITE, 0.11, 3.0 },
            { 3,  "Ganymede",            "0.0248 M_E | Largest Moon",     CelestialBodyType::CrateredMoon,   CatalogCategory::MoonsAndPlanets, 0.0248,  0.4135, 110.0f,  Color{ 165, 155, 145, 255 }, false, WHITE, 0.28, 4.0 },
            { 4,  "Titan (Saturn VI)",   "0.0225 M_E | N2 Atmosphere",    CelestialBodyType::RockyPlanet,    CatalogCategory::MoonsAndPlanets, 0.0225,  0.4043, 94.0f,   Color{ 235, 175, 75, 255 },  false, WHITE, 0.26, 3.9 },
            { 5,  "Pluto",               "0.0022 M_E | Dwarf Planet",     CelestialBodyType::CrateredMoon,   CatalogCategory::MoonsAndPlanets, 0.00218, 0.1865, 44.0f,   Color{ 210, 185, 165, 255 }, false, WHITE, 0.05, 2.4 },

            // Terrestrial & Habitable Worlds
            { 6,  "Mercury",             "0.0553 M_E | 2,439 km",         CelestialBodyType::CrateredMoon,   CatalogCategory::MoonsAndPlanets, 0.0553,  0.3829, 440.0f,  Color{ 165, 155, 145, 255 }, false, WHITE, 0.55, 3.8 },
            { 7,  "Mars",                "0.1074 M_E | 3,389 km",         CelestialBodyType::RockyPlanet,    CatalogCategory::MoonsAndPlanets, 0.1074,  0.5320, 210.0f,  Color{ 225, 105, 65, 255 },  false, WHITE, 1.07, 4.8 },
            { 8,  "Venus",               "0.8150 M_E | 6,051 km",         CelestialBodyType::RockyPlanet,    CatalogCategory::MoonsAndPlanets, 0.8150,  0.9499, 737.0f,  Color{ 240, 205, 130, 255 }, false, WHITE, 8.15, 6.0 },
            { 9,  "Earth (Terra)",       "1.0000 M_E | 6,371 km",         CelestialBodyType::RockyPlanet,    CatalogCategory::MoonsAndPlanets, 1.0000,  1.0000, 288.0f,  Color{ 65, 155, 240, 255 },  false, WHITE, 10.0, 6.4 },
            { 10, "Kepler-452b",         "5.00 M_E   | Super-Earth",      CelestialBodyType::RockyPlanet,    CatalogCategory::MoonsAndPlanets, 5.0000,  1.6300, 265.0f,  Color{ 75, 195, 165, 255 },  false, WHITE, 35.0, 8.8 },

            // Gas Giants & Ice Giants
            { 11, "Uranus",              "14.54 M_E  | Ice Giant",        CelestialBodyType::GasGiant,       CatalogCategory::MoonsAndPlanets, 14.536,  3.9810, 76.0f,   Color{ 125, 225, 235, 255 }, true,  Color{ 165, 225, 245, 160 }, 75.0,  11.5 },
            { 12, "Neptune",             "17.15 M_E  | 24,622 km",        CelestialBodyType::GasGiant,       CatalogCategory::MoonsAndPlanets, 17.147,  3.8650, 72.0f,   Color{ 55, 105, 245, 255 },  false, WHITE, 85.0,  11.2 },
            { 13, "Saturn",              "95.16 M_E  | Giant Rings",      CelestialBodyType::GasGiant,       CatalogCategory::MoonsAndPlanets, 95.159,  9.1400, 134.0f,  Color{ 235, 200, 135, 255 }, true,  Color{ 235, 215, 175, 190 }, 185.0, 14.8 },
            { 14, "Jupiter",             "317.83 M_E | 69,911 km",        CelestialBodyType::GasGiant,       CatalogCategory::MoonsAndPlanets, 317.83,  10.973, 165.0f,  Color{ 225, 160, 110, 255 }, false, WHITE, 320.0, 18.0 },

            // Main Sequence & Dwarf Stars
            { 15, "TRAPPIST-1",          "0.0898 M_Sun | Ultra-Cool Dwarf", CelestialBodyType::YellowStar, CatalogCategory::Stars, 29898.0, 13.0, 2566.0f, Color{ 235, 70, 55, 255 }, false, WHITE, 1200.0, 16.0 },
            { 16, "Proxima Centauri",    "0.1221 M_Sun | Red Dwarf",        CelestialBodyType::YellowStar, CatalogCategory::Stars, 40652.0, 16.8, 3042.0f, Color{ 245, 85, 60, 255 }, false, WHITE, 1650.0, 17.5 },
            { 17, "Barnard's Star",      "0.1440 M_Sun | Red Dwarf M4",     CelestialBodyType::YellowStar, CatalogCategory::Stars, 47944.0, 21.4, 3134.0f, Color{ 245, 95, 65, 255 }, false, WHITE, 1950.0, 18.5 },
            { 18, "Alpha Centauri B",    "0.9070 M_Sun | Orange Dwarf K1V", CelestialBodyType::YellowStar, CatalogCategory::Stars, 301982.0, 94.2, 5260.0f, Color{ 255, 195, 95, 255 }, false, WHITE, 6200.0, 23.0 },
            { 19, "Sun (Sol G2V)",       "332,946 M_E  | 109.2 R_Earth",    CelestialBodyType::YellowStar, CatalogCategory::Stars, 332946.0, 109.16, 5778.0f, Color{ 255, 225, 75, 255 }, false, WHITE, 7500.0, 25.0 },
            { 20, "Alpha Centauri A",    "1.100 M_Sun  | 1.22 R_Sun",       CelestialBodyType::YellowStar, CatalogCategory::Stars, 366240.0, 132.8, 5790.0f, Color{ 255, 235, 120, 255 }, false, WHITE, 8200.0, 26.0 },
            { 21, "Sirius A",            "2.063 M_Sun  | White Main Seq.",  CelestialBodyType::BlueHypergiant, CatalogCategory::Stars, 686867.0, 186.7, 9940.0f, Color{ 200, 230, 255, 255 }, false, WHITE, 12500.0, 29.0 },
            { 22, "Vega",                "2.135 M_Sun  | Blue-White Star",  CelestialBodyType::BlueHypergiant, CatalogCategory::Stars, 710839.0, 257.6, 9602.0f, Color{ 185, 220, 255, 255 }, false, WHITE, 13200.0, 30.0 },

            // Giants & Supergiants
            { 23, "Arcturus",            "1.08 M_Sun | 25.4 R_Sun (Giant)", CelestialBodyType::YellowStar, CatalogCategory::Giants, 359581.0, 2772.0, 4286.0f, Color{ 255, 140, 55, 255 }, false, WHITE, 9500.0, 34.0 },
            { 24, "Aldebaran",           "1.16 M_Sun | 44.1 R_Sun (Giant)", CelestialBodyType::YellowStar, CatalogCategory::Giants, 386217.0, 4817.0, 3910.0f, Color{ 255, 105, 45, 255 }, false, WHITE, 10500.0, 38.0 },
            { 25, "Rigel",               "21.0 M_Sun | 78.9 R_Sun (Blue)",  CelestialBodyType::BlueHypergiant, CatalogCategory::Giants, 6991866.0, 8612.0, 12100.0f, Color{ 135, 195, 255, 255 }, false, WHITE, 24000.0, 44.0 },
            { 26, "Betelgeuse",          "16.5 M_Sun | 764 R_Sun (Super)",  CelestialBodyType::YellowStar, CatalogCategory::Giants, 5493609.0, 83398.0, 3600.0f, Color{ 255, 75, 35, 255 }, false, WHITE, 21000.0, 52.0 },

            // Compact & Relativistic Objects
            { 27, "Sirius B",            "1.018 M_Sun | White Dwarf",       CelestialBodyType::NeutronStar, CatalogCategory::CompactAndBlackHoles, 338939.0, 0.916, 25200.0f, Color{ 175, 240, 255, 255 }, false, WHITE, 9000.0, 9.0 },
            { 28, "Crab Pulsar",         "1.40 M_Sun  | R = 11.5 km",       CelestialBodyType::NeutronStar, CatalogCategory::CompactAndBlackHoles, 466124.0, 0.0018, 1600000.0f, Color{ 195, 110, 255, 255 }, false, WHITE, 14000.0, 7.5 },
            { 29, "Cygnus X-1",          "21.2 M_Sun  | Stellar Black Hole", CelestialBodyType::BlackHole,  CatalogCategory::CompactAndBlackHoles, 7058455.0, 0.0098, 0.0f, BLACK, false, WHITE, 25000.0, 16.0 },
            { 30, "Sagittarius A*",      "4.15M M_Sun | Galactic Core SMBH", CelestialBodyType::BlackHole,  CatalogCategory::CompactAndBlackHoles, 1.38e12, 1885.0, 0.0f, BLACK, false, WHITE, 65000.0, 24.0 },
            { 31, "TON 618",             "66 Billion M_Sun | Quasar SMBH",  CelestialBodyType::BlackHole,  CatalogCategory::CompactAndBlackHoles, 2.20e16, 30500000.0, 0.0f, BLACK, false, WHITE, 120000.0, 35.0 },

            // Asteroids
            { 32, "Asteroid (Bennu)",    "1.5e-5 M_E | Near-Earth Obj",     CelestialBodyType::Asteroid,   CatalogCategory::Asteroids, 0.000015, 0.0120, 190.0f, Color{ 145, 140, 132, 255 }, false, WHITE, 0.0015, 2.2 },
            { 33, "Asteroid (Eros)",     "8.0e-5 M_E | S-Type Asteroid",    CelestialBodyType::Asteroid,   CatalogCategory::Asteroids, 0.000080, 0.0260, 200.0f, Color{ 172, 158, 142, 255 }, false, WHITE, 0.006,  2.8 },
            { 34, "Asteroid (Vesta)",    "4.0e-4 M_E | Protoplanet",        CelestialBodyType::Asteroid,   CatalogCategory::Asteroids, 0.000400, 0.0520, 210.0f, Color{ 185, 180, 172, 255 }, false, WHITE, 0.035,  3.6 }
        };
        return catalog;
    }

    static CelestialBody createFromCatalog(int index, const Vec2d& pos, const Vec2d& vel) {
        const auto& entries = getEntries();
        int safeIdx = std::clamp(index, 0, static_cast<int>(entries.size()) - 1);
        const CatalogEntry& e = entries[safeIdx];

        CelestialBody b;
        b.id = 0; // Assigned by PhysicsWorld::addBody
        b.name = e.name;
        b.bodyType = e.bodyType;
        b.position = pos;
        b.velocity = vel;
        b.acceleration = { 0.0, 0.0 };
        b.mass = e.simMass;
        b.radius = e.simRadius;
        b.temperature = e.temperatureK;
        b.color = e.color;
        b.hasRings = e.hasRings;
        b.ringColor = e.ringColor;
        b.isStar = (e.bodyType == CelestialBodyType::YellowStar ||
                    e.bodyType == CelestialBodyType::BlueHypergiant ||
                    e.bodyType == CelestialBodyType::NeutronStar);
        b.textureSeed = static_cast<uint32_t>(safeIdx * 7919 + 101);
        return b;
    }
};

} // namespace sim
