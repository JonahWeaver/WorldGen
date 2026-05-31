#include "MapGenerator.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

struct Point { int x; int y; };

uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)g << 8 | r;
}

std::vector<uint32_t> buildPalette(int count)
{
    static const std::vector<uint32_t> baseColors = {
        rgba(63, 125, 187), rgba(169, 96, 47), rgba(118, 183, 84),
        rgba(167, 80, 159), rgba(214, 170, 71), rgba(94, 125, 76),
        rgba(187, 108, 57), rgba(110, 150, 170), rgba(143, 99, 124),
        rgba(156, 169, 99)
    };

    std::vector<uint32_t> palette;
    palette.reserve(count);
    for (int i = 0; i < count; ++i)
        palette.push_back(baseColors[i % static_cast<int>(baseColors.size())]);
    return palette;
}

int squaredDistanceWrappedX(const Point& a, const Point& b, int width)
{
    int dx = std::abs(a.x - b.x);
    dx = std::min(dx, width - dx);
    int dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool isBoundary(int x, int y, int size, const std::vector<int>& plateIndex)
{
    int center = plateIndex[y * size + x];
    static const int offsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (auto& offset : offsets)
    {
        int nx = x + offset[0];
        int ny = y + offset[1];
        if (ny < 0 || ny >= size)
            continue;
        if (nx < 0)
            nx = size - 1;
        else if (nx >= size)
            nx = 0;

        if (plateIndex[ny * size + nx] != center)
            return true;
    }
    return false;
}

} // namespace

MapResult generateMap(const GeneratorSettings& settings)
{
    int size = std::clamp(settings.mapSize, 64, 512);
    int plateCount = std::clamp(settings.plateCount, 2, 24);
    int thickness = std::clamp(settings.faultIntensity, 1, 8);

    std::mt19937 rng(settings.seed ? settings.seed : static_cast<unsigned int>(std::random_device{}()));
    std::uniform_int_distribution<int> posDist(0, size - 1);

    std::vector<Point> seeds;
    seeds.reserve(plateCount);
    for (int i = 0; i < plateCount; ++i)
        seeds.push_back({ posDist(rng), posDist(rng) });

    std::vector<int> plateIndex(size * size);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            Point cell { x, y };
            int best = 0;
            int bestDistance = squaredDistanceWrappedX(cell, seeds[0], size);
            for (int i = 1; i < plateCount; ++i)
            {
                int distance = squaredDistanceWrappedX(cell, seeds[i], size);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = i;
                }
            }
            plateIndex[y * size + x] = best;
        }
    }

    auto palette = buildPalette(plateCount);
    std::vector<uint32_t> pixels(size * size);
    const uint32_t faultColor = rgba(20, 20, 20);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            int index = y * size + x;
            int plate = plateIndex[index];
            if (isBoundary(x, y, size, plateIndex))
            {
                pixels[index] = faultColor;
                continue;
            }

            uint32_t color = palette[plate];
            int elevation = ((x + y) % 6) - 3;
            int r = std::clamp<int>((color & 0xFF) + elevation * 4, 0, 255);
            int g = std::clamp<int>(((color >> 8) & 0xFF) + elevation * 4, 0, 255);
            int b = std::clamp<int>(((color >> 16) & 0xFF) + elevation * 4, 0, 255);
            pixels[index] = rgba(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
        }
    }

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            if (!isBoundary(x, y, size, plateIndex))
                continue;

            for (int dy = -thickness; dy <= thickness; ++dy)
            {
                for (int dx = -thickness; dx <= thickness; ++dx)
                {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < size && ny >= 0 && ny < size)
                        pixels[ny * size + nx] = faultColor;
                }
            }
        }
    }

    return MapResult{ size, std::move(pixels) };
}
