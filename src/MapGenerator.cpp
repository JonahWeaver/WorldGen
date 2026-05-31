#include "MapGenerator.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

struct Point { int x; int y; };

struct SphereDir { float x; float y; float z; };

static constexpr float PI = 3.14159265358979323846f;

uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)g << 8 | r;
}

SphereDir toSphereDirection(int x, int y, int size)
{
    float lon = 2.0f * PI * (static_cast<float>(x) / size);
    float lat = PI * (0.5f - static_cast<float>(y) / size);
    float cosLat = std::cos(lat);
    return { cosLat * std::cos(lon), std::sin(lat), cosLat * std::sin(lon) };
}

inline float chordDistanceSquared(const SphereDir& a, const SphereDir& b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static uint32_t hsvToRgba(float h, float s, float v)
{
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (h < 60.0f) { r = c; g = x; b = 0.0f; }
    else if (h < 120.0f) { r = x; g = c; b = 0.0f; }
    else if (h < 180.0f) { r = 0.0f; g = c; b = x; }
    else if (h < 240.0f) { r = 0.0f; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0.0f; b = c; }
    else { r = c; g = 0.0f; b = x; }

    return rgba(
        static_cast<uint8_t>(std::clamp<int>(static_cast<int>((r + m) * 255.0f), 0, 255)),
        static_cast<uint8_t>(std::clamp<int>(static_cast<int>((g + m) * 255.0f), 0, 255)),
        static_cast<uint8_t>(std::clamp<int>(static_cast<int>((b + m) * 255.0f), 0, 255)));
}

std::vector<uint32_t> buildPalette(int count)
{
    std::vector<uint32_t> palette;
    palette.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        float hue = 360.0f * i / static_cast<float>(count);
        float saturation = 0.65f + 0.25f * (static_cast<float>((i % 4) / 3));
        float value = 0.75f + 0.20f * (static_cast<float>(((i / 4) % 2)));
        palette.push_back(hsvToRgba(hue, saturation, value));
    }
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
    int plateCount = std::clamp(settings.plateCount, 2, 64);
    int thickness = std::clamp(settings.faultIntensity, 1, 8);

    std::mt19937 rng(settings.seed ? settings.seed : static_cast<unsigned int>(std::random_device{}()));
    std::uniform_int_distribution<int> posDist(0, size - 1);

    std::vector<SphereDir> seeds;
    seeds.reserve(plateCount);
    for (int i = 0; i < plateCount; ++i)
        seeds.push_back(toSphereDirection(posDist(rng), posDist(rng), size));

    std::vector<int> plateIndex(size * size);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            SphereDir cellDir = toSphereDirection(x, y, size);
            int best = 0;
            float bestDistance = chordDistanceSquared(cellDir, seeds[0]);
            for (int i = 1; i < plateCount; ++i)
            {
                float distance = chordDistanceSquared(cellDir, seeds[i]);
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

    std::vector<uint32_t> mercatorPixels(size * size);
    for (int y = 0; y < size; ++y)
    {
        float t = (static_cast<float>(y) + 0.5f) / size;
        float mercatorLat = std::atan(std::sinh(PI * (1.0f - 2.0f * t)));
        float sourceV = 0.5f - mercatorLat / PI;
        int sourceY = std::clamp(static_cast<int>(sourceV * size), 0, size - 1);

        for (int x = 0; x < size; ++x)
        {
            mercatorPixels[y * size + x] = pixels[sourceY * size + x];
        }
    }

    return MapResult{ size, std::move(pixels), std::move(mercatorPixels) };
}
