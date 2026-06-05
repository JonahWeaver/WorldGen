#pragma once

#include <cstdint>
#include <vector>
#include "CivTypes.h"

struct GeneratorSettings
{
    int mapSize = 512;
    int plateCount = 64;
    int faultIntensity = 2;
    unsigned int seed = 0;
    float simulationTime = 120.0f;
    int simulationSteps = 32;
    float boundaryRoughness = 0.75f;
    float angularVelocity = 0.25f;
    bool fragmentation = true;
    int snapshotCount = 1;

    // Civilization simulation settings
    int   civSteps          = 2000;
    float growthRate        = 0.008f;
    float spreadThresh      = 0.43f;
    int   civSnapshots      = 20;

    // Cohesion settings
    float cohesionStrength  = 0.87f;
    float cohesionHalfLife  = 0.09f;
    float cohesionLerpRate  = 0.04f;

    // Great People settings
    // Military great people: accumulate points from pop × militarism × resource bonus
    float gpMilFrequency    = 1.0f;   // multiplier on military GP spawn rate (0=never, 2=double)
    float gpMilPowerMin     = 0.15f;  // minimum power roll for military great people [0,1]
    float gpMilPowerMax     = 0.60f;  // maximum power roll for military great people [0,1]

    // Arcane great people: accumulate points from ManaStone stockpile × Arcane tech
    float gpArcaneFrequency = 1.0f;   // multiplier on arcane GP spawn rate (rarer than military)
    float gpArcanePowerMin  = 0.30f;  // minimum power roll for arcane great people [0,1]
    float gpArcanePowerMax  = 0.90f;  // maximum power roll for arcane great people [0,1]

    // Harbinger: extremely rare legendary arcane figure
    float gpHarbingerChance = 0.05f;  // probability [0,1] that an arcane GP is a Harbinger

    // Diplomacy / buyout settings
    // isolationismBuyoutBlock: Isolationism value at or above which buyout is impossible.
    //   Default 0.5 (low threshold — most isolationist civs resist buyout).
    //   Set higher (e.g. 2.5) to allow buying even very isolationist civs.
    float isolationismBuyoutBlock  = 0.5f;

    // culturalSimMaxDiscount: maximum Gold discount from cultural similarity [0,1].
    //   Default 0.5 (50% off for identical cultures).
    //   Set to 0 to disable cultural discounts entirely.
    float culturalSimMaxDiscount   = 0.5f;

    // diplomacyTechDiscount: Gold discount multiplier when buyer has Diplomacy tech [0,1].
    //   Default 0.30 (30% off).
    float diplomacyTechDiscount    = 0.30f;

    // buyoutCohesionCap: target's average cohesion must be below this to accept buyout.
    //   Default 0.65 (only fragile/unstable countries can be bought).
    //   Set higher to allow buying stable countries (at great cost).
    float buyoutCohesionCap        = 0.65f;

    // isolationismPriceScale: multiplier on how much Isolationism inflates the price.
    //   price *= (1 + Isolationism * isolationismPriceScale)
    //   Default 1.5 — each point of Isolationism adds 150% to the base price.
    float isolationismPriceScale   = 1.5f;
};

// Render layers
enum class MapLayer
{
    TectonicPlates   = 0,
    BoundaryTypes    = 1,
    CollisionEffects = 2,
    Elevation        = 3,
    Climate          = 4,
    Hydrology        = 5,
    Civilization     = 6,
    Count            = 7
};

// One point-in-time snapshot of the world (geology phase).
struct MapSnapshot
{
    float simulationTime = 0.f;
    int   plateCount     = 0;

    std::vector<uint32_t> layerPixels   [static_cast<int>(MapLayer::Count)];
    std::vector<uint32_t> layerMercator [static_cast<int>(MapLayer::Count)];
};

struct MapResult
{
    int size = 0;

    // Geology snapshots
    std::vector<MapSnapshot> snapshots;

    const std::vector<uint32_t>& finalLayerPixels  (int L) const { return snapshots.back().layerPixels  [L]; }
    const std::vector<uint32_t>& finalLayerMercator(int L) const { return snapshots.back().layerMercator[L]; }

    int finalPlateCount = 0;
    int convergentCount = 0;
    int transformCount  = 0;
    int divergentCount  = 0;

    // Civilization simulation (from CivTypes.h)
    std::vector<CivCell>        civCells;       // per-cell data (size×size)
    std::vector<CivSnapshot>    civSnaps;        // timeline snapshots
    std::vector<CivInfo>        civInfos;        // aggregated per-country data
    std::vector<CountryState>   countryStates;   // live per-country state (final tick)
    std::vector<CivEvent>       eventLog;        // all events across all ticks
    std::vector<uint32_t>       resourcePixels;  // resource overlay (equirectangular)
    std::vector<uint32_t>       resourceMercator;
};

MapResult generateMap(const GeneratorSettings& settings);
