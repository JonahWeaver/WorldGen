#pragma once

#include <cstdint>
#include <vector>

struct GeneratorSettings
{
    int mapSize = 256;
    int plateCount = 7;
    int faultIntensity = 2;
    unsigned int seed = 0;
};

struct MapResult
{
    int size = 0;
    std::vector<uint32_t> pixels;
};

MapResult generateMap(const GeneratorSettings& settings);
