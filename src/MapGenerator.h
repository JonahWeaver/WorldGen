#pragma once

#include <cstdint>
#include <vector>

struct GeneratorSettings
{
    int mapSize = 256;
    int plateCount = 7;
    int faultIntensity = 2;
    unsigned int seed = 0;
    float simulationTime = 2.0f;
    int simulationSteps = 8;
    float boundaryRoughness = 0.6f;
    float angularVelocity = 0.25f;
    bool fragmentation = false;
    int snapshotCount = 6;

    // Civilization simulation settings
    int   civSteps       = 200;   // number of civilization simulation ticks
    float growthRate     = 0.08f; // base population growth rate per tick
    float spreadThresh   = 0.35f; // min habitability for a cell to be settled
    int   civSnapshots   = 6;     // how many civ timeline snapshots to capture
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
    Civilization     = 6,  // habitability + settlements + culture + trade
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

// Per-cell civilization data (computed once from the final geology snapshot).
struct CivCell
{
    float habitability  = 0.f;  // [0,1] overall suitability for human settlement
    float fertility     = 0.f;  // [0,1] agricultural potential
    float traversability= 0.f;  // [0,1] ease of movement (roads, rivers, flat land)
    float population    = 0.f;  // current population (arbitrary units)
    float culture       = 0.f;  // cultural strength [0,1]
    float cohesion      = 1.f;  // political cohesion to owning country [0,1]
    int   cultureId     = -1;   // which culture group this cell belongs to (-1=none)
    int   countryId     = -1;   // which country/polity owns this cell (-1=none)
    bool  settled       = false;
};

// One point-in-time snapshot of the civilization simulation.
struct CivSnapshot
{
    int   tick           = 0;
    float totalPop       = 0.f;
    int   settledCells   = 0;
    int   cultureGroups  = 0;
    std::vector<uint32_t> pixels;    // globe equirectangular
    std::vector<uint32_t> mercator;  // Mercator projection
};

struct MapResult
{
    int size = 0;

    // Geology snapshots
    std::vector<MapSnapshot> snapshots;

    const std::vector<uint32_t>& layerPixels  (int L) const { return snapshots.back().layerPixels  [L]; }
    const std::vector<uint32_t>& layerMercator(int L) const { return snapshots.back().layerMercator[L]; }

    int finalPlateCount = 0;
    int convergentCount = 0;
    int transformCount  = 0;
    int divergentCount  = 0;

    // Civilization simulation (computed from final geology snapshot)
    std::vector<CivCell>     civCells;    // per-cell data (size×size)
    std::vector<CivSnapshot> civSnaps;    // timeline snapshots
};

MapResult generateMap(const GeneratorSettings& settings);
