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

SphereDir normalize(const SphereDir& v)
{
    float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length <= 0.0f)
        return { 0.0f, 0.0f, 1.0f };
    return { v.x / length, v.y / length, v.z / length };
}

SphereDir cross(const SphereDir& a, const SphereDir& b)
{
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}

float dot(const SphereDir& a, const SphereDir& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

SphereDir rotateAroundAxis(const SphereDir& v, const SphereDir& axis, float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    SphereDir term1 = { v.x * c, v.y * c, v.z * c };
    SphereDir term2 = cross(axis, v);
    term2 = { term2.x * s, term2.y * s, term2.z * s };
    float dotAV = dot(axis, v);
    SphereDir term3 = { axis.x * dotAV * (1.0f - c),
                        axis.y * dotAV * (1.0f - c),
                        axis.z * dotAV * (1.0f - c) };
    return normalize({ term1.x + term2.x + term3.x,
                       term1.y + term2.y + term3.y,
                       term1.z + term2.z + term3.z });
}

SphereDir toSphereDirection(float x, float y, int size)
{
    float lon = 2.0f * PI * (x / static_cast<float>(size));
    float lat = PI * (0.5f - y / static_cast<float>(size));
    float cosLat = std::cos(lat);
    return { cosLat * std::cos(lon), std::sin(lat), cosLat * std::sin(lon) };
}

SphereDir randomUnitSphereDir(std::mt19937& rng)
{
    std::uniform_real_distribution<float> zDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> phiDist(0.0f, 2.0f * PI);
    float z = zDist(rng);
    float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    float phi = phiDist(rng);
    return { r * std::cos(phi), z, r * std::sin(phi) };
}

inline float chordDistanceSquared(const SphereDir& a, const SphereDir& b)
{
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

// ── Noise ────────────────────────────────────────────────────────────────────

float valueNoise3D(float x, float y, float z, unsigned int seed)
{
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    int iz = static_cast<int>(std::floor(z));
    float fx = x - ix, fy = y - iy, fz = z - iz;
    auto fade = [](float t){ return t*t*t*(t*(t*6.f-15.f)+10.f); };
    float ux = fade(fx), uy = fade(fy), uz = fade(fz);
    auto h = [&](int cx, int cy, int cz) -> float {
        uint32_t v = static_cast<uint32_t>(cx)*1664525u
                   ^ static_cast<uint32_t>(cy)*1013904223u
                   ^ static_cast<uint32_t>(cz)*374761393u
                   ^ seed*0x9E3779B9u;
        v = (v^(v>>13))*1274126177u; v ^= v>>16;
        return static_cast<float>(v&0x00FFFFFFu)/static_cast<float>(0x01000000u);
    };
    float v000=h(ix,iy,iz),   v100=h(ix+1,iy,iz),
          v010=h(ix,iy+1,iz), v110=h(ix+1,iy+1,iz),
          v001=h(ix,iy,iz+1), v101=h(ix+1,iy,iz+1),
          v011=h(ix,iy+1,iz+1),v111=h(ix+1,iy+1,iz+1);
    auto lerp=[](float a,float b,float t){return a+t*(b-a);};
    return lerp(lerp(lerp(v000,v100,ux),lerp(v010,v110,ux),uy),
                lerp(lerp(v001,v101,ux),lerp(v011,v111,ux),uy),uz);
}

SphereDir fbmDisplacement(const SphereDir& p, unsigned int seed,
                           float amplitude, int octaves)
{
    float scale=2.5f, amp=amplitude, freq=1.0f;
    float dx=0,dy=0,dz=0;
    for (int oct=0; oct<octaves; ++oct)
    {
        float nx=valueNoise3D(p.x*scale*freq,p.y*scale*freq,p.z*scale*freq,seed+oct*3+0)*2.f-1.f;
        float ny=valueNoise3D(p.x*scale*freq,p.y*scale*freq,p.z*scale*freq,seed+oct*3+1)*2.f-1.f;
        float nz=valueNoise3D(p.x*scale*freq,p.y*scale*freq,p.z*scale*freq,seed+oct*3+2)*2.f-1.f;
        dx+=nx*amp; dy+=ny*amp; dz+=nz*amp;
        amp*=0.5f; freq*=2.0f;
    }
    float proj=dx*p.x+dy*p.y+dz*p.z;
    return { dx-proj*p.x, dy-proj*p.y, dz-proj*p.z };
}

float pseudoNoise(int x, int y, unsigned int seed)
{
    uint32_t h = static_cast<uint32_t>(x)*374761393u
               + static_cast<uint32_t>(y)*668265263u
               + seed*0x9E3779B9u;
    h = (h^(h>>13))*1274126177u;
    return static_cast<float>(h&0x00FFFFFFu)/static_cast<float>(0x01000000u);
}

static uint32_t hsvToRgba(float h, float s, float v)
{
    h = std::fmod(h, 360.f); if (h<0) h+=360.f;
    float c=v*s, x=c*(1.f-std::fabs(std::fmod(h/60.f,2.f)-1.f)), m=v-c;
    float r=0,g=0,b=0;
    if      (h< 60) {r=c;g=x;}
    else if (h<120) {r=x;g=c;}
    else if (h<180) {g=c;b=x;}
    else if (h<240) {g=x;b=c;}
    else if (h<300) {r=x;b=c;}
    else            {r=c;b=x;}
    return rgba(static_cast<uint8_t>(std::clamp<int>(static_cast<int>((r+m)*255),0,255)),
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>((g+m)*255),0,255)),
                static_cast<uint8_t>(std::clamp<int>(static_cast<int>((b+m)*255),0,255)));
}

std::vector<uint32_t> buildPalette(int count)
{
    std::vector<uint32_t> p; p.reserve(count);
    for (int i=0;i<count;++i)
        p.push_back(hsvToRgba(360.f*i/static_cast<float>(count),0.70f,0.85f));
    return p;
}

struct VoronoiResult { int nearest; int second; float d1; float d2; };

VoronoiResult voronoiQuery(const SphereDir& cellDir,
                            const std::vector<SphereDir>& centers)
{
    int best=0, second=-1;
    float bestDist=chordDistanceSquared(cellDir,centers[0]), secondDist=1e9f;
    for (int i=1;i<static_cast<int>(centers.size());++i)
    {
        float d=chordDistanceSquared(cellDir,centers[i]);
        if (d<bestDist)      { second=best; secondDist=bestDist; best=i; bestDist=d; }
        else if (d<secondDist){ second=i;   secondDist=d; }
    }
    return {best,second,bestDist,secondDist};
}

// Apply Mercator projection to a square pixel buffer
std::vector<uint32_t> toMercator(const std::vector<uint32_t>& src, int size)
{
    std::vector<uint32_t> dst(size*size);
    for (int y=0;y<size;++y)
    {
        float t=(static_cast<float>(y)+0.5f)/size;
        float lat=std::atan(std::sinh(PI*(1.f-2.f*t)));
        int sy=std::clamp(static_cast<int>((0.5f-lat/PI)*size),0,size-1);
        for (int x=0;x<size;++x)
            dst[y*size+x]=src[sy*size+x];
    }
    return dst;
}

} // namespace

MapResult generateMap(const GeneratorSettings& settings)
{
    int size      = std::clamp(settings.mapSize,     64, 2048);
    int plateCount= std::clamp(settings.plateCount,   2,   64);
    int thickness = std::clamp(settings.faultIntensity,1,    8);

    struct Plate { SphereDir center, axis; float angularVelocity; int motionType; };

    std::mt19937 rng(settings.seed ? settings.seed
                                   : static_cast<unsigned int>(std::random_device{}()));
    std::uniform_int_distribution<int>   posDist(0, size-1);
    std::uniform_real_distribution<float> floatDist(0.f, 1.f);

    std::vector<Plate> plates;
    plates.reserve(plateCount);
    for (int i=0;i<plateCount;++i)
    {
        SphereDir center = toSphereDirection(posDist(rng), posDist(rng), size);
        SphereDir axis   = randomUnitSphereDir(rng);
        float speed = std::clamp(settings.angularVelocity*(0.4f+floatDist(rng)*0.8f),0.1f,4.f);
        plates.push_back({center,axis,speed,1});
    }

    // ── Simulation ────────────────────────────────────────────────────────────
    int steps = std::max(settings.simulationSteps, 1);
    float stepTime = settings.simulationTime / static_cast<float>(steps);
    float collisionThreshold = 0.22f;
    for (int step=0; step<steps; ++step)
    {
        for (auto& p : plates)
            p.center = rotateAroundAxis(p.center, p.axis, p.angularVelocity*stepTime);

        if (settings.fragmentation)
        {
            for (size_t i=0; i<plates.size()&&plates.size()<64; ++i)
            for (size_t j=i+1; j<plates.size()&&plates.size()<64; ++j)
            {
                float angle=std::acos(std::clamp(dot(plates[i].center,plates[j].center),-1.f,1.f));
                if (angle<collisionThreshold)
                {
                    SphereDir mid=normalize({plates[i].center.x+plates[j].center.x,
                                             plates[i].center.y+plates[j].center.y,
                                             plates[i].center.z+plates[j].center.z});
                    SphereDir ax=normalize(cross(mid,randomUnitSphereDir(rng)));
                    float sp=std::clamp((plates[i].angularVelocity+plates[j].angularVelocity)*0.5f,0.1f,4.f);
                    plates.push_back({mid,ax,sp,1});
                }
            }
        }
    }

    // ── Classify boundary types ───────────────────────────────────────────────
    std::vector<int> motionCategory(plates.size(), 1);
    int convergentCount=0, transformCount=0, divergentCount=0;

    for (size_t i=0; i<plates.size(); ++i)
    {
        int nearest=-1; float bestDist=1e9f;
        for (size_t j=0; j<plates.size(); ++j)
        {
            if (i==j) continue;
            float d=chordDistanceSquared(plates[i].center,plates[j].center);
            if (d<bestDist){bestDist=d; nearest=static_cast<int>(j);}
        }
        if (nearest<0){motionCategory[i]=1; transformCount++; continue;}

        SphereDir velA=cross(plates[i].axis,plates[i].center);
        SphereDir velB=cross(plates[nearest].axis,plates[nearest].center);
        velA={velA.x*plates[i].angularVelocity,velA.y*plates[i].angularVelocity,velA.z*plates[i].angularVelocity};
        velB={velB.x*plates[nearest].angularVelocity,velB.y*plates[nearest].angularVelocity,velB.z*plates[nearest].angularVelocity};
        SphereDir rel={velA.x-velB.x,velA.y-velB.y,velA.z-velB.z};
        SphereDir dir=normalize({plates[nearest].center.x-plates[i].center.x,
                                  plates[nearest].center.y-plates[i].center.y,
                                  plates[nearest].center.z-plates[i].center.z});
        float alignment=dot(rel,dir);
        if      (alignment<-0.05f){motionCategory[i]=0; convergentCount++;}
        else if (alignment> 0.05f){motionCategory[i]=2; divergentCount++;}
        else                      {motionCategory[i]=1; transformCount++;}
    }

    // ── Voronoi + fractal domain warp ─────────────────────────────────────────
    std::vector<SphereDir> plateCenters;
    plateCenters.reserve(plates.size());
    for (auto& p : plates) plateCenters.push_back(p.center);

    float roughness    = std::clamp(settings.boundaryRoughness, 0.f, 1.f);
    float fbmAmplitude = roughness * 0.55f;
    int   fbmOctaves   = 2 + static_cast<int>(roughness * 4.f);
    unsigned int noiseSeed = settings.seed ^ 0xDEADBEEFu;

    std::vector<int>   plateIndex (size*size, 0);
    std::vector<int>   plateIndex2(size*size, 0);
    std::vector<float> borderDist (size*size, 1.f);

    for (int y=0; y<size; ++y)
    for (int x=0; x<size; ++x)
    {
        SphereDir cellDir = toSphereDirection(static_cast<float>(x),
                                              static_cast<float>(y), size);
        if (fbmAmplitude > 0.f)
        {
            SphereDir disp = fbmDisplacement(cellDir, noiseSeed, fbmAmplitude, fbmOctaves);
            cellDir = normalize({cellDir.x+disp.x, cellDir.y+disp.y, cellDir.z+disp.z});
        }
        VoronoiResult vr = voronoiQuery(cellDir, plateCenters);
        plateIndex [y*size+x] = vr.nearest;
        plateIndex2[y*size+x] = vr.second>=0 ? vr.second : vr.nearest;
        float d1=std::sqrt(vr.d1);
        float d2=vr.second>=0 ? std::sqrt(vr.d2) : d1;
        borderDist[y*size+x] = (d2-d1)/(d1+d2+1e-6f);
    }

    auto isBoundary = [&](int idx){ return plateIndex[idx] != plateIndex2[idx]; };

    // Boundary type for a pixel (uses the more dramatic of the two meeting plates)
    auto boundaryType = [&](int idx) -> int {
        int ca=motionCategory[plateIndex[idx]];
        int cb=motionCategory[plateIndex2[idx]];
        if (ca==0||cb==0) return 0;
        if (ca==2||cb==2) return 2;
        return 1;
    };

    auto palette = buildPalette(static_cast<int>(plates.size()));

    // Helper: interior pixel colour (flat plate colour + subtle checkerboard)
    auto interiorColour = [&](int x, int y, int plate) -> uint32_t {
        uint32_t color = palette[plate];
        int elev = ((x+y)%6)-3;
        int r=std::clamp<int>((color&0xFF)        +elev*4,0,255);
        int g=std::clamp<int>(((color>>8)&0xFF)   +elev*4,0,255);
        int b=std::clamp<int>(((color>>16)&0xFF)  +elev*4,0,255);
        return rgba(static_cast<uint8_t>(r),static_cast<uint8_t>(g),static_cast<uint8_t>(b));
    };

    // ── Layer 0: Tectonic Plates ──────────────────────────────────────────────
    // Pure plate colours, fractal edges visible as colour transitions, no lines.
    std::vector<uint32_t> pixPlates(size*size);
    for (int y=0;y<size;++y)
    for (int x=0;x<size;++x)
    {
        int idx=y*size+x;
        pixPlates[idx] = interiorColour(x, y, plateIndex[idx]);
    }

    // ── Layer 1: Boundary Types ───────────────────────────────────────────────
    // Plate colours + coloured boundary lines (red/green/gold), thickened.
    std::vector<uint32_t> pixBoundary = pixPlates; // start from plate colours

    // First mark boundary pixels with their type colour
    for (int y=0;y<size;++y)
    for (int x=0;x<size;++x)
    {
        int idx=y*size+x;
        if (!isBoundary(idx)) continue;
        switch (boundaryType(idx))
        {
            case 0:  pixBoundary[idx]=rgba(200,70,70);   break; // convergent — red
            case 2:  pixBoundary[idx]=rgba(90,180,110);  break; // divergent  — green
            default: pixBoundary[idx]=rgba(210,170,45);  break; // transform  — gold
        }
    }
    // Thicken
    {
        std::vector<uint32_t> thick = pixBoundary;
        for (int y=0;y<size;++y)
        for (int x=0;x<size;++x)
        {
            int idx=y*size+x;
            if (!isBoundary(idx)) continue;
            uint32_t col=pixBoundary[idx];
            for (int dy=-thickness;dy<=thickness;++dy)
            for (int dx=-thickness;dx<=thickness;++dx)
            {
                int nx=x+dx, ny=y+dy;
                if (nx<0||nx>=size||ny<0||ny>=size) continue;
                thick[ny*size+nx]=col;
            }
        }
        pixBoundary=std::move(thick);
    }

    // ── Layer 2: Collision Effects ────────────────────────────────────────────
    // Plate colours + gradient halo terrain effects near boundaries.
    float thicknessScale = static_cast<float>(thickness)/3.f;
    float haloConvergent = 0.055f * thicknessScale;
    float haloDivergent  = 0.032f * thicknessScale;
    float haloTransform  = 0.018f * thicknessScale;
    float maxHalo        = haloConvergent;

    std::vector<uint32_t> pixCollision(size*size);
    for (int y=0;y<size;++y)
    for (int x=0;x<size;++x)
    {
        int idx   = y*size+x;
        int plate = plateIndex[idx];
        float ep  = borderDist[idx];

        // Only apply halo if close enough to a real boundary
        if (!isBoundary(idx) || ep >= maxHalo)
        {
            pixCollision[idx] = interiorColour(x, y, plate);
            continue;
        }

        int bcat = boundaryType(idx);

        if (bcat == 0 && ep < haloConvergent) // ── Convergent: mountain ridge
        {
            float t = ep / haloConvergent;
            uint32_t pc=palette[plate];
            int pr=pc&0xFF, pg=(pc>>8)&0xFF, pb=(pc>>16)&0xFF;
            float bump = pseudoNoise(x*3,y*3,noiseSeed+77u)*0.4f
                       + pseudoNoise(x*7,y*7,noiseSeed+13u)*0.2f;
            float tn = std::clamp(t+(bump-0.3f)*0.5f,0.f,1.f);
            int r,g,b;
            if      (tn<0.15f){float s=tn/0.15f;       r=int(18+s*(90-18));  g=int(12+s*(55-12));  b=int(12+s*(50-12));}
            else if (tn<0.55f){float s=(tn-0.15f)/0.4f; r=int(90+s*(140-90)); g=int(55+s*(110-55)); b=int(50+s*(95-50));}
            else               {float s=(tn-0.55f)/0.45f;r=int(140+s*(pr-140));g=int(110+s*(pg-110));b=int(95+s*(pb-95));}
            pixCollision[idx]=rgba(uint8_t(std::clamp(r,0,255)),uint8_t(std::clamp(g,0,255)),uint8_t(std::clamp(b,0,255)));
        }
        else if (bcat == 2 && ep < haloDivergent) // ── Divergent: rift valley
        {
            float t = ep / haloDivergent;
            uint32_t pc=palette[plate];
            int pr=pc&0xFF, pg=(pc>>8)&0xFF, pb=(pc>>16)&0xFF;
            float bump = pseudoNoise(x*5,y*5,noiseSeed+31u)*0.35f
                       + pseudoNoise(x*11,y*11,noiseSeed+99u)*0.15f;
            float tn = std::clamp(t+(bump-0.25f)*0.6f,0.f,1.f);
            int r,g,b;
            if      (tn<0.18f){float s=tn/0.18f;        r=int(10+s*(30-10));  g=int(20+s*(70-20));  b=int(25+s*(80-25));}
            else if (tn<0.55f){float s=(tn-0.18f)/0.37f; r=int(30+s*(60-30));  g=int(70+s*(160-70)); b=int(80+s*(130-80));}
            else               {float s=(tn-0.55f)/0.45f; r=int(60+s*(pr-60));  g=int(160+s*(pg-160));b=int(130+s*(pb-130));}
            pixCollision[idx]=rgba(uint8_t(std::clamp(r,0,255)),uint8_t(std::clamp(g,0,255)),uint8_t(std::clamp(b,0,255)));
        }
        else if (bcat == 1 && ep < haloTransform) // ── Transform: fault scar
        {
            float t = ep / haloTransform;
            uint32_t pc=palette[plate];
            int pr=pc&0xFF, pg=(pc>>8)&0xFF, pb=(pc>>16)&0xFF;
            float bump = pseudoNoise(x*9,y*9,noiseSeed+55u)*0.5f;
            float tn = std::clamp(t+(bump-0.25f)*0.7f,0.f,1.f);
            int r,g,b;
            if (tn<0.25f){float s=tn/0.25f;        r=int(200+s*(220-200));g=int(140+s*(175-140));b=int(20+s*(40-20));}
            else          {float s=(tn-0.25f)/0.75f; r=int(220+s*(pr-220)); g=int(175+s*(pg-175)); b=int(40+s*(pb-40));}
            pixCollision[idx]=rgba(uint8_t(std::clamp(r,0,255)),uint8_t(std::clamp(g,0,255)),uint8_t(std::clamp(b,0,255)));
        }
        else
        {
            pixCollision[idx] = interiorColour(x, y, plate);
        }
    }

    // ── Build result ──────────────────────────────────────────────────────────
    MapResult result;
    result.size            = size;
    result.finalPlateCount = static_cast<int>(plates.size());
    result.convergentCount = convergentCount;
    result.transformCount  = transformCount;
    result.divergentCount  = divergentCount;

    result.layerPixels  [0] = std::move(pixPlates);
    result.layerPixels  [1] = std::move(pixBoundary);
    result.layerPixels  [2] = std::move(pixCollision);

    for (int L=0; L<static_cast<int>(MapLayer::Count); ++L)
        result.layerMercator[L] = toMercator(result.layerPixels[L], size);

    return result;
}
