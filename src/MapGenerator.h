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
    float cohesionStrength  = 0.7f;
    float cohesionHalfLife  = 0.07f;
    float cohesionLerpRate  = 0.025f;
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
