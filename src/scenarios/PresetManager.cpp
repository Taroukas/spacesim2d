#include "scenarios/PresetManager.hpp"
#include <cmath>
#include <random>

namespace sim {

const char* PresetManager::getPresetName(ScenarioPreset preset) {
    switch (preset) {
        case ScenarioPreset::ScaledSolarSystem:        return "Solar System (Sun, Planets, Rings & Moon)";
        case ScenarioPreset::BinaryStarSystem:         return "Binary Stars (Hypergiant + Sun + Worlds)";
        case ScenarioPreset::FigureEightChoreography:  return "Figure-8 Three-Body Choreography (RK4)";
        case ScenarioPreset::BlackHoleAccretionDisk:   return "Black Hole Accretion Disk & Roche Shredding";
        case ScenarioPreset::BarnesHutGalaxyCollision: return "Galaxy Crash (1,200+ Stars & Black Holes)";
        case ScenarioPreset::SpacecraftSlingshotMission:return "Spacecraft Slingshot Mission (Explorer-1)";
        case ScenarioPreset::TON618Quasar:             return "TON 618 Quasar (Ultramassive Black Hole)";
        case ScenarioPreset::ProceduralSystem:         return "Procedural Star System Generator [G]";
        default:                                       return "Unknown Scenario";
    }
}

const char* PresetManager::getPresetDescription(ScenarioPreset preset) {
    switch (preset) {
        case ScenarioPreset::ScaledSolarSystem:
            return "Central Sun with Terran Earth + Moon, cratered Mercury/Mars, banded Jupiter, ringed Saturn, Uranus, and Neptune.";
        case ScenarioPreset::BinaryStarSystem:
            return "A Blue Hypergiant and a Yellow Star orbiting their common barycenter with inner S-type and outer P-type worlds.";
        case ScenarioPreset::FigureEightChoreography:
            return "Chenciner-Montgomery periodic 3-body solution where three stars trace a single figure-8 curve under RK4 integration.";
        case ScenarioPreset::BlackHoleAccretionDisk:
            return "Gargantua Supermassive Black Hole surrounded by a swirling debris disk and doomed moons passing inside the Roche Limit.";
        case ScenarioPreset::BarnesHutGalaxyCollision:
            return "Two spiral galaxies (1,202 bodies) on a tidal merger course solved at 60 FPS using the Barnes-Hut O(N log N) quadtree.";
        case ScenarioPreset::SpacecraftSlingshotMission:
            return "Pilot Explorer-1 from a parking orbit around Terra to execute a Hohmann transfer and gravity assist around Giant Jove.";
        case ScenarioPreset::TON618Quasar:
            return "TON 618 Ultramassive Black Hole with relativistic Einstein lensing, accretion disk, and orbiting supergiants (Rigel, Betelgeuse, Sirius B).";
        case ScenarioPreset::ProceduralSystem:
            return "Generates an infinite variety of procedural solar systems with rocky planets, gas giants, asteroid belts, and pilotable spacecraft.";
        default:
            return "";
    }
}

void PresetManager::zeroBarycentricMomentum(std::vector<CelestialBody>& bodies) {
    Vec2d totalMomentum{ 0.0, 0.0 };
    double totalMass = 0.0;

    for (const auto& b : bodies) {
        if (!b.alive || b.isPinned) continue;
        totalMomentum += b.velocity * b.mass;
        totalMass += b.mass;
    }

    if (totalMass > 0.0) {
        Vec2d comVel = totalMomentum / totalMass;
        for (auto& b : bodies) {
            if (!b.alive || b.isPinned) continue;
            b.velocity -= comVel;
        }
    }
}

void PresetManager::loadPreset(ScenarioPreset preset,
                               PhysicsWorld& world,
                               Camera2DController& camera) {
    world.clear();
    world.G = Config::DEFAULT_G;
    world.softening = Config::DEFAULT_SOFTENING;
    world.timeScale = 1.0;
    world.paused = false;

    switch (preset) {
        case ScenarioPreset::ScaledSolarSystem:
            setupSolarSystem(world, camera);
            break;
        case ScenarioPreset::BinaryStarSystem:
            setupBinaryStarSystem(world, camera);
            break;
        case ScenarioPreset::FigureEightChoreography:
            setupFigureEight(world, camera);
            break;
        case ScenarioPreset::BlackHoleAccretionDisk:
            setupBlackHoleAccretionDisk(world, camera);
            break;
        case ScenarioPreset::BarnesHutGalaxyCollision:
            setupGalaxyCollision(world, camera);
            break;
        case ScenarioPreset::SpacecraftSlingshotMission:
            setupSpacecraftMission(world, camera);
            break;
        case ScenarioPreset::TON618Quasar:
            setupTON618Quasar(world, camera);
            break;
        case ScenarioPreset::ProceduralSystem:
            generateProceduralStarSystem(world, camera);
            break;
        default:
            setupSolarSystem(world, camera);
            break;
    }

    world.barnesHutTree.computeAllAccelerations(
        world.bodies, world.solverType, world.G, world.softening, world.barnesHutTheta);
    world.computeEnergyAndMomentum(true);
}

void PresetManager::setupSolarSystem(PhysicsWorld& world, Camera2DController& camera) {
    world.solverType = GravitySolverType::DirectNBody;
    world.integratorType = IntegratorType::VelocityVerlet;
    world.collisionMode = CollisionMode::RealisticHybrid;
    world.showTrails = true;
    world.maxTrailLength = 750;

    const double sunMass = 10000.0;

    CelestialBody sun;
    sun.name = "Sun";
    sun.bodyType = CelestialBodyType::YellowStar;
    sun.mass = sunMass;
    sun.radius = 29.0;
    sun.temperature = 5778.0;
    sun.position = { 0.0, 0.0 };
    sun.velocity = { 0.0, 0.0 };
    sun.color = Color{ 255, 215, 70, 255 };
    sun.isStar = true;
    world.addBody(sun);

    struct PlanetSpec {
        const char* name;
        CelestialBodyType type;
        uint32_t texSeed;
        double au;
        double mass;
        double radius;
        double eccentricity;
        double tempK;
        bool rings;
        Color color;
    };

    // Astronomical semi-major axes in AU (scaled via Config::scaleOrbitalDistanceAU)
    const PlanetSpec specs[] = {
        { "Mercury", CelestialBodyType::CrateredMoon, 0u,  0.387,  1.2,  3.6, 0.05, 440.0, false, { 185, 180, 175, 255 } },
        { "Venus",   CelestialBodyType::GasGiant,     1u,  0.723,  4.5,  5.6, 0.01, 737.0, false, { 240, 200, 130, 255 } },
        { "Earth",   CelestialBodyType::RockyPlanet,  0u,  1.000, 12.0,  6.8, 0.01, 288.0, false, {  65, 160, 255, 255 } },
        { "Mars",    CelestialBodyType::CrateredMoon, 1u,  1.524,  2.8,  4.8, 0.04, 210.0, false, { 235, 105,  70, 255 } },
        { "Jupiter", CelestialBodyType::GasGiant,     0u,  5.203, 95.0, 15.5, 0.02, 165.0, false, { 230, 175, 125, 255 } },
        { "Saturn",  CelestialBodyType::GasGiant,     1u,  9.537, 55.0, 13.0, 0.03, 134.0, true,  { 240, 215, 155, 255 } },
        { "Uranus",  CelestialBodyType::GasGiant,     2u, 19.191, 22.0,  9.5, 0.02,  76.0, true,  { 140, 230, 245, 255 } },
        { "Neptune", CelestialBodyType::GasGiant,     3u, 30.069, 24.0,  9.2, 0.01,  72.0, false, {  75, 125, 250, 255 } }
    };

    int earthId = -1;
    Vec2d earthPos{ 0.0, 0.0 };
    Vec2d earthVel{ 0.0, 0.0 };
    double earthMass = 12.0;

    for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); ++i) {
        const auto& sp = specs[i];
        double orbDist = Config::scaleOrbitalDistanceAU(sp.au);
        double angle = static_cast<double>(i) * 0.85;
        Vec2d radial(std::cos(angle), std::sin(angle));
        Vec2d tangent = radial.perpendicular();

        double vCirc = std::sqrt((world.G * (sunMass + sp.mass)) / orbDist);
        double speed = vCirc * std::sqrt(1.0 + sp.eccentricity);

        CelestialBody p;
        p.name = sp.name;
        p.bodyType = sp.type;
        p.textureSeed = sp.texSeed;
        p.semiMajorAxisAU = sp.au;
        p.mass = sp.mass;
        p.radius = sp.radius;
        p.temperature = sp.tempK;
        p.position = radial * orbDist;
        p.velocity = tangent * speed;
        p.color = sp.color;
        p.isStar = false;
        p.hasRings = sp.rings;
        if (std::string(sp.name) == "Uranus") {
            p.ringColor = Color{ 165, 235, 255, 115 };
            p.ringInnerRadiusRatio = 1.35;
            p.ringOuterRadiusRatio = 1.85;
        }

        CelestialBody& added = world.addBody(p);
        if (std::string(sp.name) == "Earth") {
            earthId = static_cast<int>(added.id);
            earthPos = added.position;
            earthVel = added.velocity;
            earthMass = added.mass;
        }
    }

    if (earthId != -1) {
        double moonDist = 18.5;
        double moonMass = 0.15;
        double vMoon = std::sqrt((world.G * (earthMass + moonMass)) / moonDist);

        CelestialBody moon;
        moon.name = "Moon";
        moon.bodyType = CelestialBodyType::CrateredMoon;
        moon.textureSeed = 2u;
        moon.mass = moonMass;
        moon.radius = 2.4;
        moon.temperature = 220.0;
        moon.position = earthPos + Vec2d(moonDist, 0.0);
        moon.velocity = earthVel + Vec2d(0.0, vMoon);
        moon.color = Color{ 215, 220, 230, 255 };
        world.addBody(moon);
    }

    zeroBarycentricMomentum(world.bodies);
    camera.focusOnPosition({ 0.0, 0.0 }, Config::use_logarithmic_scale ? 0.36 : 0.14);
}

void PresetManager::setupBinaryStarSystem(PhysicsWorld& world, Camera2DController& camera) {
    world.solverType = GravitySolverType::DirectNBody;
    world.integratorType = IntegratorType::VelocityVerlet;
    world.collisionMode = CollisionMode::RealisticHybrid;
    world.showTrails = true;
    world.maxTrailLength = 800;

    const double m1 = 6000.0;
    const double m2 = 3800.0;
    const double totalStarMass = m1 + m2;
    const double separation = 230.0;
    const double eccentricity = 0.16;

    double r1 = separation * (m2 / totalStarMass);
    double r2 = separation * (m1 / totalStarMass);

    double vRel = std::sqrt((world.G * totalStarMass / separation) * ((1.0 + eccentricity) / (1.0 - eccentricity)));
    double v1 = vRel * (m2 / totalStarMass);
    double v2 = vRel * (m1 / totalStarMass);

    CelestialBody starA;
    starA.name = "Rigel A (Hypergiant)";
    starA.bodyType = CelestialBodyType::BlueHypergiant;
    starA.mass = m1;
    starA.radius = 23.0;
    starA.temperature = 12000.0;
    starA.position = { -r1, 0.0 };
    starA.velocity = { 0.0, -v1 };
    starA.color = Color{ 110, 200, 255, 255 };
    starA.isStar = true;
    world.addBody(starA);

    CelestialBody starB;
    starB.name = "Helios B";
    starB.bodyType = CelestialBodyType::YellowStar;
    starB.mass = m2;
    starB.radius = 18.5;
    starB.temperature = 5800.0;
    starB.position = { r2, 0.0 };
    starB.velocity = { 0.0, v2 };
    starB.color = Color{ 255, 210, 90, 255 };
    starB.isStar = true;
    world.addBody(starB);

    // Close S-type molten world orbiting Rigel A
    {
        double sDist = 52.0;
        double sVel = std::sqrt((world.G * m1) / sDist);
        CelestialBody sPlanet;
        sPlanet.name = "Mustafar (S-Type)";
        sPlanet.bodyType = CelestialBodyType::CrateredMoon;
        sPlanet.mass = 1.6;
        sPlanet.radius = 4.2;
        sPlanet.temperature = 2100.0;
        sPlanet.position = starA.position + Vec2d(0.0, -sDist);
        sPlanet.velocity = starA.velocity + Vec2d(sVel, 0.0);
        sPlanet.color = Color{ 255, 115, 75, 255 };
        world.addBody(sPlanet);
    }

    const double pRadii[3] = { 460.0, 660.0, 890.0 };
    const char* pNames[3] = { "Tatooine (Terran)", "Kepler-16b (Ringed Giant)", "Hyperion Ice World" };
    const CelestialBodyType pTypes[3] = {
        CelestialBodyType::RockyPlanet,
        CelestialBodyType::GasGiant,
        CelestialBodyType::RockyPlanet
    };

    for (int i = 0; i < 3; ++i) {
        double angle = i * 2.1;
        Vec2d dir(std::cos(angle), std::sin(angle));
        Vec2d tang = dir.perpendicular();
        double vCirc = std::sqrt((world.G * totalStarMass) / pRadii[i]);

        CelestialBody p;
        p.name = pNames[i];
        p.bodyType = pTypes[i];
        p.textureSeed = static_cast<uint32_t>(i + 1);
        p.mass = 10.0 + i * 18.0;
        p.radius = 6.2 + i * 2.4;
        p.hasRings = (i == 1);
        p.temperature = (i == 2) ? 140.0 : 290.0;
        p.position = dir * pRadii[i];
        p.velocity = tang * vCirc;
        p.color = (i == 1) ? Color{ 235, 195, 130, 255 } : Color{ 85, 190, 255, 255 };
        world.addBody(p);
    }

    zeroBarycentricMomentum(world.bodies);
    camera.focusOnPosition({ 0.0, 0.0 }, 0.65);
}

void PresetManager::setupFigureEight(PhysicsWorld& world, Camera2DController& camera) {
    world.solverType = GravitySolverType::DirectNBody;
    world.integratorType = IntegratorType::RungeKutta4;
    world.collisionMode = CollisionMode::ElasticBounce;
    world.G = 100.0;
    world.softening = 0.001;
    world.subSteps = 8;
    world.showTrails = true;
    world.maxTrailLength = 950;

    const double M = 1000.0;
    const double L = 250.0;
    const double V = std::sqrt((world.G * M) / L);

    Vec2d r1(-0.97000436 * L,  0.24308753 * L);
    Vec2d r2( 0.97000436 * L, -0.24308753 * L);
    Vec2d r3(0.0, 0.0);

    Vec2d v3(-0.93240737 * V, -0.86473146 * V);
    Vec2d v1 = -0.5 * v3;
    Vec2d v2 = -0.5 * v3;

    CelestialBody b1;
    b1.name = "Alpha Solaris";
    b1.bodyType = CelestialBodyType::YellowStar;
    b1.mass = M;
    b1.radius = 12.5;
    b1.position = r1;
    b1.velocity = v1;
    b1.color = Color{ 255, 195, 65, 255 };
    b1.isStar = true;
    world.addBody(b1);

    CelestialBody b2;
    b2.name = "Beta Rigel";
    b2.bodyType = CelestialBodyType::BlueHypergiant;
    b2.mass = M;
    b2.radius = 12.5;
    b2.position = r2;
    b2.velocity = v2;
    b2.color = Color{ 75, 215, 255, 255 };
    b2.isStar = true;
    world.addBody(b2);

    CelestialBody b3;
    b3.name = "Gamma Nova";
    b3.bodyType = CelestialBodyType::YellowStar;
    b3.mass = M;
    b3.radius = 12.5;
    b3.position = r3;
    b3.velocity = v3;
    b3.color = Color{ 255, 95, 185, 255 };
    b3.isStar = true;
    world.addBody(b3);

    camera.focusOnPosition({ 0.0, 0.0 }, 1.25);
}

void PresetManager::setupBlackHoleAccretionDisk(PhysicsWorld& world, Camera2DController& camera) {
    world.solverType = GravitySolverType::BarnesHut;
    world.integratorType = IntegratorType::VelocityVerlet;
    world.collisionMode = CollisionMode::RealisticHybrid;
    world.fragmentationThreshold = 2.8;
    world.softening = 2.5;
    world.showTrails = false;

    const double bhMass = 18000.0;
    CelestialBody gargantua;
    gargantua.name = "Gargantua (Black Hole)";
    gargantua.bodyType = CelestialBodyType::BlackHole;
    gargantua.mass = bhMass;
    gargantua.radius = 22.0;
    gargantua.position = { 0.0, 0.0 };
    gargantua.velocity = { 0.0, 0.0 };
    gargantua.color = Color{ 255, 165, 65, 255 };
    gargantua.isStar = false;
    world.addBody(gargantua);

    // Place two doomed moons on grazing elliptical orbits that dip inside the Black Hole's Roche Limit!
    for (int m = 0; m < 2; ++m) {
        double dist = 145.0 + m * 35.0;
        double angle = m * 3.14159;
        Vec2d radial(std::cos(angle), std::sin(angle));
        Vec2d tang = radial.perpendicular();
        double vEllip = std::sqrt((world.G * bhMass) / dist) * 0.78; // Dips inside Roche limit

        CelestialBody doomed;
        doomed.name = (m == 0) ? "Miller's World" : "Doomed Moon";
        doomed.bodyType = (m == 0) ? CelestialBodyType::RockyPlanet : CelestialBodyType::CrateredMoon;
        doomed.mass = 9.0;
        doomed.radius = 6.8;
        doomed.position = radial * dist;
        doomed.velocity = tang * vEllip;
        doomed.color = (m == 0) ? Color{ 80, 185, 255, 255 } : Color{ 225, 175, 130, 255 };
        world.addBody(doomed);
    }

    // Outer stable Gas Giant with rings
    {
        double gDist = 470.0;
        double vG = std::sqrt((world.G * bhMass) / gDist);
        CelestialBody mann;
        mann.name = "Edmunds Giant";
        mann.bodyType = CelestialBodyType::GasGiant;
        mann.textureSeed = 2u;
        mann.mass = 65.0;
        mann.radius = 13.0;
        mann.hasRings = true;
        mann.position = { 0.0, -gDist };
        mann.velocity = { vG, 0.0 };
        mann.color = Color{ 130, 220, 255, 255 };
        world.addBody(mann);
    }

    // Swirling accretion disk of 600 glowing planetesimals
    std::mt19937 rng(777u);
    std::uniform_real_distribution<double> angleDist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radialDist(95.0, 560.0);
    std::uniform_real_distribution<double> massDist(0.3, 1.6);

    for (int i = 0; i < 600; ++i) {
        double r = radialDist(rng);
        double theta = angleDist(rng);
        Vec2d radial(std::cos(theta), std::sin(theta));
        Vec2d tang = radial.perpendicular();

        double vCirc = std::sqrt((world.G * bhMass) / r);

        CelestialBody rock;
        rock.name = "Accretion Debris";
        rock.bodyType = CelestialBodyType::Asteroid;
        rock.mass = massDist(rng);
        rock.radius = std::max(1.8, 1.7 * std::cbrt(rock.mass));
        rock.temperature = (r < 190.0) ? 1800.0 : 320.0;
        rock.position = radial * r;
        rock.velocity = tang * vCirc;

        float normR = static_cast<float>((r - 95.0) / (560.0 - 95.0));
        rock.color = (normR < 0.35f)
                     ? Color{ 255, 165, 75, 255 }
                     : Color{ 140, 175, 245, 255 };
        world.addBody(rock);
    }

    zeroBarycentricMomentum(world.bodies);
    camera.focusOnPosition({ 0.0, 0.0 }, 0.75);
}

void PresetManager::setupGalaxyCollision(PhysicsWorld& world, Camera2DController& camera) {
    world.solverType = GravitySolverType::BarnesHut;
    world.integratorType = IntegratorType::VelocityVerlet;
    world.collisionMode = CollisionMode::InelasticMerge;
    world.barnesHutTheta = 0.55;
    world.softening = 5.0;
    world.showTrails = false;

    std::mt19937 rng(2026u);
    std::uniform_real_distribution<double> angleDist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> radiusNorm(0.0, 1.0);
    std::uniform_real_distribution<double> massDist(0.1, 1.0);

    auto spawnGalaxy = [&](const char* coreName, Vec2d corePos, Vec2d coreVel,
                           double coreMass, double diskMinR, double diskMaxR,
                           int starCount, bool clockwise, Color coreColor, Color armColorA, Color armColorB) {
        CelestialBody core;
        core.name = coreName;
        core.bodyType = CelestialBodyType::BlackHole;
        core.mass = coreMass;
        core.radius = 18.0;
        core.position = corePos;
        core.velocity = coreVel;
        core.color = coreColor;
        core.isStar = false;
        world.addBody(core);

        for (int i = 0; i < starCount; ++i) {
            double u = radiusNorm(rng);
            double r = diskMinR + (diskMaxR - diskMinR) * (0.25 * u + 0.75 * u * u);
            double theta = angleDist(rng);

            Vec2d radial(std::cos(theta), std::sin(theta));
            Vec2d tangent = clockwise ? -radial.perpendicular() : radial.perpendicular();

            double enclosedMass = coreMass + (starCount * 0.55) * ((r - diskMinR) / (diskMaxR - diskMinR));
            double vOrb = std::sqrt((world.G * enclosedMass) / r);

            CelestialBody star;
            star.name = "Galactic Star";
            star.bodyType = CelestialBodyType::Asteroid;
            star.mass = massDist(rng);
            star.radius = 2.0;
            star.position = corePos + radial * r;
            star.velocity = coreVel + tangent * vOrb;

            float t = static_cast<float>((r - diskMinR) / (diskMaxR - diskMinR));
            star.color = Color{
                static_cast<unsigned char>(armColorA.r + (armColorB.r - armColorA.r) * t),
                static_cast<unsigned char>(armColorA.g + (armColorB.g - armColorA.g) * t),
                static_cast<unsigned char>(armColorA.b + (armColorB.b - armColorA.b) * t),
                255
            };
            star.isStar = false;
            world.addBody(star);
        }
    };

    spawnGalaxy("Milky Way Core (SMBH)",
                Vec2d(-460.0, -140.0), Vec2d(22.0, 9.0),
                15000.0, 32.0, 310.0, 600, false,
                Color{ 255, 210, 110, 255 },
                Color{ 255, 215, 130, 255 },
                Color{ 85, 190, 255, 255 });

    spawnGalaxy("Andromeda Core (SMBH)",
                Vec2d(460.0, 140.0), Vec2d(-22.0, -9.0),
                15000.0, 32.0, 310.0, 600, false,
                Color{ 225, 140, 255, 255 },
                Color{ 255, 155, 110, 255 },
                Color{ 195, 120, 255, 255 });

    zeroBarycentricMomentum(world.bodies);
    camera.focusOnPosition({ 0.0, 0.0 }, 0.58);
}

void PresetManager::setupSpacecraftMission(PhysicsWorld& world, Camera2DController& camera) {
    world.solverType = GravitySolverType::DirectNBody;
    world.integratorType = IntegratorType::VelocityVerlet;
    world.collisionMode = CollisionMode::RealisticHybrid;
    world.showTrails = true;
    world.showTrajectoryPrediction = true;
    world.predictionSteps = 750;
    world.predictionStepDt = 0.05;

    const double starMass = 12000.0;
    CelestialBody star;
    star.name = "Sol Prime";
    star.bodyType = CelestialBodyType::YellowStar;
    star.mass = starMass;
    star.radius = 28.0;
    star.position = { 0.0, 0.0 };
    star.velocity = { 0.0, 0.0 };
    star.color = Color{ 255, 220, 85, 255 };
    star.isStar = true;
    world.addBody(star);

    double terraDist = 230.0;
    double terraMass = 85.0;
    double vTerra = std::sqrt((world.G * (starMass + terraMass)) / terraDist);

    CelestialBody terra;
    terra.name = "Terra";
    terra.bodyType = CelestialBodyType::RockyPlanet;
    terra.textureSeed = 0u;
    terra.mass = terraMass;
    terra.radius = 9.5;
    terra.position = { terraDist, 0.0 };
    terra.velocity = { 0.0, vTerra };
    terra.color = Color{ 65, 165, 255, 255 };
    CelestialBody& addedTerra = world.addBody(terra);

    double joveDist = 540.0;
    double joveMass = 320.0;
    double joveAngle = 1.15;
    Vec2d joveDir(std::cos(joveAngle), std::sin(joveAngle));
    Vec2d joveTang = joveDir.perpendicular();
    double vJove = std::sqrt((world.G * (starMass + joveMass)) / joveDist);

    CelestialBody jove;
    jove.name = "Giant Jove";
    jove.bodyType = CelestialBodyType::GasGiant;
    jove.textureSeed = 0u;
    jove.mass = joveMass;
    jove.radius = 18.0;
    jove.hasRings = true;
    jove.position = joveDir * joveDist;
    jove.velocity = joveTang * vJove;
    jove.color = Color{ 235, 165, 105, 255 };
    world.addBody(jove);

    zeroBarycentricMomentum(world.bodies);

    double parkAlt = 22.0;
    double vPark = std::sqrt((world.G * addedTerra.mass) / parkAlt);
    Vec2d shipPos = addedTerra.position + Vec2d(parkAlt, 0.0);
    Vec2d shipVel = addedTerra.velocity + Vec2d(0.0, vPark);

    world.spacecraft.reset(shipPos, shipVel, M_PI * 0.5);
    world.cameraFollowId = -2;
    camera.focusOnPosition(shipPos, 1.35);
}

void PresetManager::setupTON618Quasar(PhysicsWorld& world, Camera2DController& camera) {
    world.solverType = GravitySolverType::DirectNBody;
    world.integratorType = IntegratorType::VelocityVerlet;
    world.collisionMode = CollisionMode::RealisticHybrid;
    world.showTrails = true;
    world.maxTrailLength = 800;
    world.G = 120.0;

    // Central Ultramassive Black Hole: TON 618
    CelestialBody ton;
    ton.name = "TON 618 (Quasar)";
    ton.bodyType = CelestialBodyType::BlackHole;
    ton.mass = 85000.0;
    ton.radius = 28.0;
    ton.temperature = 0.0;
    ton.position = { 0.0, 0.0 };
    ton.velocity = { 0.0, 0.0 };
    ton.color = BLACK;
    ton.isStar = false;
    world.addBody(ton);

    // Orbiting Supergiants & Stellar Remnants
    struct QuasarSatellite {
        const char* name;
        CelestialBodyType type;
        double dist;
        double mass;
        double radius;
        double tempK;
        Color color;
    };

    const QuasarSatellite sats[] = {
        { "Sirius B (White Dwarf)",   CelestialBodyType::NeutronStar,    950.0, 1200.0,  7.5, 25200.0, Color{ 175, 240, 255, 255 } },
        { "Crab Pulsar",              CelestialBodyType::NeutronStar,   1650.0, 1600.0,  7.0, 1600000.0, Color{ 195, 110, 255, 255 } },
        { "Rigel (Blue Hypergiant)",  CelestialBodyType::BlueHypergiant,2600.0, 6500.0, 36.0, 12100.0, Color{ 135, 195, 255, 255 } },
        { "Betelgeuse (Supergiant)",  CelestialBodyType::YellowStar,    4200.0, 5200.0, 48.0,  3600.0, Color{ 255,  85,  40, 255 } }
    };

    for (size_t i = 0; i < sizeof(sats) / sizeof(sats[0]); ++i) {
        const auto& s = sats[i];
        double ang = static_cast<double>(i) * 1.570796;
        Vec2d pos(std::cos(ang) * s.dist, std::sin(ang) * s.dist);
        Vec2d tang = pos.normalized().perpendicular();
        double vCirc = std::sqrt((world.G * ton.mass) / s.dist);

        CelestialBody body;
        body.name = s.name;
        body.bodyType = s.type;
        body.mass = s.mass;
        body.radius = s.radius;
        body.temperature = s.tempK;
        body.position = pos;
        body.velocity = tang * vCirc;
        body.color = s.color;
        body.isStar = (s.type != CelestialBodyType::BlackHole);
        world.addBody(body);
    }

    // Dense relativistic swirling accretion disk
    std::mt19937 rng(42u);
    std::uniform_real_distribution<double> dist01(0.0, 1.0);
    for (int i = 0; i < 90; ++i) {
        double r = 65.0 + dist01(rng) * 420.0;
        double a = dist01(rng) * 2.0 * M_PI;
        Vec2d pos(std::cos(a) * r, std::sin(a) * r);
        Vec2d tang = pos.normalized().perpendicular();
        double v = std::sqrt((world.G * ton.mass) / r);

        CelestialBody debris;
        debris.name = "Accretion Clump #" + std::to_string(i + 1);
        debris.bodyType = CelestialBodyType::Asteroid;
        debris.mass = 0.05 + dist01(rng) * 0.15;
        debris.radius = 1.8 + dist01(rng) * 1.4;
        debris.position = pos;
        debris.velocity = tang * v;
        debris.temperature = 1200.0 + dist01(rng) * 2500.0;
        debris.color = Color{ 255, static_cast<unsigned char>(120 + dist01(rng) * 100), 40, 255 };
        world.addBody(debris);
    }

    zeroBarycentricMomentum(world.bodies);
    camera.resetView();
    camera.focusOnPosition({ 0.0, 0.0 });
    camera.zoom = 0.065;
}

void PresetManager::generateProceduralStarSystem(PhysicsWorld& world, Camera2DController& camera) {
    world.clear();
    world.G = Config::DEFAULT_G;
    world.softening = Config::DEFAULT_SOFTENING;
    world.timeScale = 1.0;
    world.paused = false;
    world.solverType = GravitySolverType::DirectNBody;
    world.integratorType = IntegratorType::VelocityVerlet;
    world.collisionMode = CollisionMode::RealisticHybrid;
    world.showTrails = true;
    world.maxTrailLength = 700;

    std::mt19937 rng(static_cast<uint32_t>(GetTime() * 1000.0) + 1337u);
    std::uniform_real_distribution<double> dist01(0.0, 1.0);
    std::uniform_real_distribution<double> angDist(0.0, 2.0 * M_PI);

    // Primary Star
    CelestialBody star;
    star.id = 0;
    star.position = { 0.0, 0.0 };
    star.velocity = { 0.0, 0.0 };

    double starChoice = dist01(rng);
    if (starChoice < 0.65) {
        star.name = "Sol-" + std::to_string(rng() % 900 + 100);
        star.bodyType = CelestialBodyType::YellowStar;
        star.mass = 8000.0 + dist01(rng) * 3000.0;
        star.radius = 26.0;
        star.temperature = 5800.0;
        star.color = Color{ 255, 220, 80, 255 };
        star.isStar = true;
    } else if (starChoice < 0.88) {
        star.name = "Rigel-" + std::to_string(rng() % 900 + 100);
        star.bodyType = CelestialBodyType::BlueHypergiant;
        star.mass = 16000.0 + dist01(rng) * 8000.0;
        star.radius = 35.0;
        star.temperature = 16000.0;
        star.color = Color{ 140, 210, 255, 255 };
        star.isStar = true;
    } else {
        star.name = "Gargantua Prime";
        star.bodyType = CelestialBodyType::BlackHole;
        star.mass = 35000.0;
        star.radius = 18.0;
        star.temperature = 0.0;
        star.color = BLACK;
        star.isStar = false;
    }
    world.addBody(star);

    int planetCount = 4 + (rng() % 3);
    double curDist = 160.0;

    Vec2d homePlanetPos{ 0.0, 0.0 };
    Vec2d homePlanetVel{ 0.0, 0.0 };
    double homePlanetRadius = 6.0;

    for (int p = 0; p < planetCount; ++p) {
        curDist += 110.0 + dist01(rng) * 180.0;
        double a = angDist(rng);
        Vec2d pos = Vec2d(std::cos(a), std::sin(a)) * curDist;
        Vec2d tang = pos.normalized().perpendicular();
        double vCirc = std::sqrt((world.G * star.mass) / curDist);
        Vec2d vel = tang * vCirc;

        CelestialBody planet;
        planet.id = 0;
        planet.position = pos;
        planet.velocity = vel;

        if (p < 2 || (p < 3 && dist01(rng) > 0.4)) {
            planet.bodyType = (dist01(rng) > 0.4) ? CelestialBodyType::RockyPlanet : CelestialBodyType::CrateredMoon;
            planet.mass = 4.0 + dist01(rng) * 12.0;
            planet.radius = 4.5 + dist01(rng) * 3.5;
            planet.temperature = std::clamp(5800.0 / std::sqrt(curDist * 0.12), 80.0, 850.0);
            planet.color = (planet.bodyType == CelestialBodyType::RockyPlanet)
                ? Color{ 65, static_cast<unsigned char>(140 + dist01(rng) * 60), 245, 255 }
                : Color{ 190, 185, 180, 255 };
            planet.name = "Terra-" + std::to_string(p + 1);
            if (homePlanetPos.lengthSq() < 1.0) {
                homePlanetPos = pos;
                homePlanetVel = vel;
                homePlanetRadius = planet.radius;
            }
        } else {
            planet.bodyType = CelestialBodyType::GasGiant;
            planet.mass = 45.0 + dist01(rng) * 120.0;
            planet.radius = 12.0 + dist01(rng) * 7.0;
            planet.temperature = std::clamp(3200.0 / std::sqrt(curDist * 0.15), 45.0, 250.0);
            planet.hasRings = (dist01(rng) > 0.35);
            planet.ringColor = Color{ 235, 210, 160, 180 };
            planet.color = Color{ static_cast<unsigned char>(180 + dist01(rng) * 65),
                                  static_cast<unsigned char>(130 + dist01(rng) * 60),
                                  static_cast<unsigned char>(90 + dist01(rng) * 40), 255 };
            planet.name = "Jove-" + std::to_string(p + 1);
        }

        world.addBody(planet);
    }

    // Asteroid Belt
    double beltInner = curDist * 0.55;
    double beltOuter = curDist * 0.75;
    int asteroidCount = 65;
    for (int i = 0; i < asteroidCount; ++i) {
        double r = beltInner + dist01(rng) * (beltOuter - beltInner);
        double a = angDist(rng);
        Vec2d pos = Vec2d(std::cos(a), std::sin(a)) * r;
        Vec2d tang = pos.normalized().perpendicular();
        double vCirc = std::sqrt((world.G * star.mass) / r) * (0.97 + dist01(rng) * 0.06);

        CelestialBody ast;
        ast.id = 0;
        ast.name = "Asteroid-" + std::to_string(i + 1);
        ast.bodyType = CelestialBodyType::Asteroid;
        ast.mass = 0.005 + dist01(rng) * 0.02;
        ast.radius = 1.6 + dist01(rng) * 1.4;
        ast.position = pos;
        ast.velocity = tang * vCirc;
        ast.color = Color{ 165, 160, 155, 255 };
        world.addBody(ast);
    }

    // Spacecraft in parking orbit
    if (homePlanetPos.lengthSq() > 1.0) {
        Vec2d shipPos = homePlanetPos + Vec2d(homePlanetRadius + 22.0, 0.0);
        double vOrb = std::sqrt((world.G * 10.0) / (homePlanetRadius + 22.0));
        Vec2d shipVel = homePlanetVel + Vec2d(0.0, vOrb);
        world.spacecraft.reset(shipPos, shipVel, M_PI * 0.5);
    }

    camera.resetView();
    camera.focusOnPosition({ 0.0, 0.0 });
    camera.zoom = 0.16;
}

} // namespace sim
