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
    float angularVelocity = 0.7f;
    bool fragmentation = true;
};

// Three render layers, each available as a globe texture and a Mercator texture.
enum class MapLayer
{
    TectonicPlates  = 0,  // solid plate colours, fractal edges, no boundary overlay
    BoundaryTypes   = 1,  // plate colours + coloured boundary lines (red/green/gold)
    CollisionEffects = 2, // plate colours + halo terrain effects at boundaries
    Count           = 3
};

struct MapResult
{
    int size = 0;

    // [0] = TectonicPlates, [1] = BoundaryTypes, [2] = CollisionEffects
    std::vector<uint32_t> layerPixels   [static_cast<int>(MapLayer::Count)];
    std::vector<uint32_t> layerMercator [static_cast<int>(MapLayer::Count)];

    int finalPlateCount = 0;
    int convergentCount = 0;
    int transformCount  = 0;
    int divergentCount  = 0;
};

MapResult generateMap(const GeneratorSettings& settings);
