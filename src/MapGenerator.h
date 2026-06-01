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
    float angularVelocity = 0.25f;  // slow drift so shapes stay recognisable across snapshots
    bool fragmentation = false;     // off by default; keeps plate count stable across timeline
    int snapshotCount = 6;   // how many evenly-spaced snapshots to capture (including final)
};

// Render layers — each available as a globe texture and a Mercator texture.
enum class MapLayer
{
    TectonicPlates   = 0,  // solid plate colours, fractal edges, no boundary overlay
    BoundaryTypes    = 1,  // plate colours + coloured boundary lines (red/green/gold)
    CollisionEffects = 2,  // plate colours + halo terrain effects at boundaries
    Elevation        = 3,  // topographic heatmap (deep ocean -> coast -> highland -> peak)
    Count            = 4
};

// One point-in-time snapshot of the world.
struct MapSnapshot
{
    float simulationTime = 0.f;   // elapsed simulation seconds at this snapshot
    int   plateCount     = 0;     // number of plates at this moment

    // layerPixels[L] and layerMercator[L] for each MapLayer L
    std::vector<uint32_t> layerPixels   [static_cast<int>(MapLayer::Count)];
    std::vector<uint32_t> layerMercator [static_cast<int>(MapLayer::Count)];
};

struct MapResult
{
    int size = 0;

    // Ordered snapshots from t=0 (or first interval) through final time.
    // snapshots.back() is always the final map.
    std::vector<MapSnapshot> snapshots;

    // Convenience accessors into the final snapshot (back())
    const std::vector<uint32_t>& layerPixels  (int L) const { return snapshots.back().layerPixels  [L]; }
    const std::vector<uint32_t>& layerMercator(int L) const { return snapshots.back().layerMercator[L]; }

    int finalPlateCount = 0;
    int convergentCount = 0;
    int transformCount  = 0;
    int divergentCount  = 0;
};

MapResult generateMap(const GeneratorSettings& settings);
