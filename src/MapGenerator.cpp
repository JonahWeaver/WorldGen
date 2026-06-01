#include "MapGenerator.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

struct SphereDir { float x; float y; float z; };

static constexpr float PI = 3.14159265358979323846f;

uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)g << 8 | r;
}

SphereDir normalize(const SphereDir& v)
{
    float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len <= 0.f) return {0,0,1};
    return {v.x/len, v.y/len, v.z/len};
}

SphereDir cross(const SphereDir& a, const SphereDir& b)
{
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}

float dot(const SphereDir& a, const SphereDir& b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

SphereDir rotateAroundAxis(const SphereDir& v, const SphereDir& axis, float angle)
{
    float c=std::cos(angle), s=std::sin(angle);
    SphereDir t1={v.x*c,v.y*c,v.z*c};
    SphereDir t2=cross(axis,v); t2={t2.x*s,t2.y*s,t2.z*s};
    float d=dot(axis,v);
    SphereDir t3={axis.x*d*(1-c),axis.y*d*(1-c),axis.z*d*(1-c)};
    return normalize({t1.x+t2.x+t3.x, t1.y+t2.y+t3.y, t1.z+t2.z+t3.z});
}

SphereDir toSphereDirection(float x, float y, int size)
{
    float lon=2.f*PI*(x/size), lat=PI*(0.5f-y/size);
    float cl=std::cos(lat);
    return {cl*std::cos(lon), std::sin(lat), cl*std::sin(lon)};
}

SphereDir randomUnitSphereDir(std::mt19937& rng)
{
    std::uniform_real_distribution<float> zd(-1,1), pd(0,2*PI);
    float z=zd(rng), r=std::sqrt(std::max(0.f,1-z*z)), p=pd(rng);
    return {r*std::cos(p),z,r*std::sin(p)};
}

inline float chordSq(const SphereDir& a, const SphereDir& b)
{
    float dx=a.x-b.x,dy=a.y-b.y,dz=a.z-b.z;
    return dx*dx+dy*dy+dz*dz;
}

// ── Noise ─────────────────────────────────────────────────────────────────────

float valueNoise3D(float x, float y, float z, unsigned int seed)
{
    int ix=int(std::floor(x)),iy=int(std::floor(y)),iz=int(std::floor(z));
    float fx=x-ix,fy=y-iy,fz=z-iz;
    auto fade=[](float t){return t*t*t*(t*(t*6-15)+10);};
    float ux=fade(fx),uy=fade(fy),uz=fade(fz);
    auto h=[&](int cx,int cy,int cz)->float{
        uint32_t v=uint32_t(cx)*1664525u^uint32_t(cy)*1013904223u
                  ^uint32_t(cz)*374761393u^seed*0x9E3779B9u;
        v=(v^(v>>13))*1274126177u; v^=v>>16;
        return float(v&0xFFFFFFu)/float(0x1000000u);
    };
    float v000=h(ix,iy,iz),v100=h(ix+1,iy,iz),v010=h(ix,iy+1,iz),v110=h(ix+1,iy+1,iz),
          v001=h(ix,iy,iz+1),v101=h(ix+1,iy,iz+1),v011=h(ix,iy+1,iz+1),v111=h(ix+1,iy+1,iz+1);
    auto lerp=[](float a,float b,float t){return a+t*(b-a);};
    return lerp(lerp(lerp(v000,v100,ux),lerp(v010,v110,ux),uy),
                lerp(lerp(v001,v101,ux),lerp(v011,v111,ux),uy),uz);
}

SphereDir fbmDisplacement(const SphereDir& p, unsigned int seed, float amp, int oct)
{
    float scale=2.5f,freq=1,dx=0,dy=0,dz=0,a=amp;
    for(int o=0;o<oct;++o){
        float nx=valueNoise3D(p.x*scale*freq,p.y*scale*freq,p.z*scale*freq,seed+o*3+0)*2-1;
        float ny=valueNoise3D(p.x*scale*freq,p.y*scale*freq,p.z*scale*freq,seed+o*3+1)*2-1;
        float nz=valueNoise3D(p.x*scale*freq,p.y*scale*freq,p.z*scale*freq,seed+o*3+2)*2-1;
        dx+=nx*a; dy+=ny*a; dz+=nz*a; a*=0.5f; freq*=2;
    }
    float proj=dx*p.x+dy*p.y+dz*p.z;
    return {dx-proj*p.x, dy-proj*p.y, dz-proj*p.z};
}

float pseudoNoise(int x, int y, unsigned int seed)
{
    uint32_t h=uint32_t(x)*374761393u+uint32_t(y)*668265263u+seed*0x9E3779B9u;
    h=(h^(h>>13))*1274126177u;
    return float(h&0xFFFFFFu)/float(0x1000000u);
}

static uint32_t hsvToRgba(float h,float s,float v)
{
    h=std::fmod(h,360.f); if(h<0)h+=360;
    float c=v*s,x=c*(1-std::fabs(std::fmod(h/60,2)-1)),m=v-c,r=0,g=0,b=0;
    if(h<60){r=c;g=x;}else if(h<120){r=x;g=c;}else if(h<180){g=c;b=x;}
    else if(h<240){g=x;b=c;}else if(h<300){r=x;b=c;}else{r=c;b=x;}
    return rgba(uint8_t(std::clamp<int>(int((r+m)*255),0,255)),
                uint8_t(std::clamp<int>(int((g+m)*255),0,255)),
                uint8_t(std::clamp<int>(int((b+m)*255),0,255)));
}

std::vector<uint32_t> buildPalette(int n)
{
    std::vector<uint32_t> p; p.reserve(n);
    for(int i=0;i<n;++i) p.push_back(hsvToRgba(360.f*i/n,0.70f,0.85f));
    return p;
}

struct VoronoiResult{int nearest,second; float d1,d2;};

VoronoiResult voronoiQuery(const SphereDir& c, const std::vector<SphereDir>& centers)
{
    int best=0,second=-1;
    float bd=chordSq(c,centers[0]),sd=1e9f;
    for(int i=1;i<int(centers.size());++i){
        float d=chordSq(c,centers[i]);
        if(d<bd){second=best;sd=bd;best=i;bd=d;}
        else if(d<sd){second=i;sd=d;}
    }
    return {best,second,bd,sd};
}

std::vector<uint32_t> toMercator(const std::vector<uint32_t>& src, int size)
{
    std::vector<uint32_t> dst(size*size);
    for(int y=0;y<size;++y){
        float t=(float(y)+0.5f)/size;
        float lat=std::atan(std::sinh(PI*(1-2*t)));
        int sy=std::clamp(int((0.5f-lat/PI)*size),0,size-1);
        for(int x=0;x<size;++x) dst[y*size+x]=src[sy*size+x];
    }
    return dst;
}

// ── Plate data passed into the renderer ───────────────────────────────────────

struct PlateState
{
    SphereDir center, axis;
    float angularVelocity;
    int   motionCategory;   // 0=convergent 1=transform 2=divergent
};

// ── Core renderer: given a set of plates, produce all 4 layer pixel buffers ──

MapSnapshot renderSnapshot(
    const std::vector<PlateState>& plates,
    float elapsedTime,
    int size,
    int thickness,
    float roughness,
    unsigned int seed,
    int paletteSize)   // fixed palette size so colours are stable across all snapshots
{
    MapSnapshot snap;
    snap.simulationTime = elapsedTime;
    snap.plateCount     = int(plates.size());

    // ── Voronoi + fractal domain warp ─────────────────────────────────────────
    float fbmAmp  = roughness * 0.55f;
    int   fbmOct  = 2 + int(roughness * 4.f);
    unsigned int noiseSeed = seed ^ 0xDEADBEEFu;

    std::vector<SphereDir> centers;
    centers.reserve(plates.size());
    for(auto& p:plates) centers.push_back(p.center);

    std::vector<int>   pi1(size*size,0), pi2(size*size,0);
    std::vector<float> bd (size*size,1.f);

    for(int y=0;y<size;++y)
    for(int x=0;x<size;++x)
    {
        SphereDir cd=toSphereDirection(float(x),float(y),size);
        if(fbmAmp>0){
            SphereDir d=fbmDisplacement(cd,noiseSeed,fbmAmp,fbmOct);
            cd=normalize({cd.x+d.x,cd.y+d.y,cd.z+d.z});
        }
        VoronoiResult vr=voronoiQuery(cd,centers);
        pi1[y*size+x]=vr.nearest;
        pi2[y*size+x]=vr.second>=0?vr.second:vr.nearest;
        float d1=std::sqrt(vr.d1);
        float d2=vr.second>=0?std::sqrt(vr.d2):d1;
        bd[y*size+x]=(d2-d1)/(d1+d2+1e-6f);
    }

    auto isBnd=[&](int i){return pi1[i]!=pi2[i];};
    auto btype=[&](int i)->int{
        int ca=plates[pi1[i]].motionCategory;
        int cb=plates[pi2[i]].motionCategory;
        if(ca==0||cb==0)return 0;
        if(ca==2||cb==2)return 2;
        return 1;
    };

    // Use the fixed palette size so plate N always maps to the same colour
    // regardless of how many plates exist at this snapshot.
    auto palette=buildPalette(paletteSize);

    auto intCol=[&](int x,int y,int pl)->uint32_t{
        uint32_t c=palette[pl];
        int e=((x+y)%6)-3;
        int r=std::clamp<int>((c&0xFF)+e*4,0,255);
        int g=std::clamp<int>(((c>>8)&0xFF)+e*4,0,255);
        int b=std::clamp<int>(((c>>16)&0xFF)+e*4,0,255);
        return rgba(uint8_t(r),uint8_t(g),uint8_t(b));
    };

    // ── Layer 0: Tectonic Plates ──────────────────────────────────────────────
    std::vector<uint32_t> pix0(size*size);
    for(int y=0;y<size;++y)
    for(int x=0;x<size;++x)
        pix0[y*size+x]=intCol(x,y,pi1[y*size+x]);

    // ── Layer 1: Boundary Types ───────────────────────────────────────────────
    std::vector<uint32_t> pix1=pix0;
    for(int y=0;y<size;++y)
    for(int x=0;x<size;++x){
        int i=y*size+x;
        if(!isBnd(i))continue;
        switch(btype(i)){
            case 0: pix1[i]=rgba(200,70,70);  break;
            case 2: pix1[i]=rgba(90,180,110); break;
            default:pix1[i]=rgba(210,170,45); break;
        }
    }
    {
        std::vector<uint32_t> thick=pix1;
        for(int y=0;y<size;++y)
        for(int x=0;x<size;++x){
            int i=y*size+x; if(!isBnd(i))continue;
            uint32_t col=pix1[i];
            for(int dy=-thickness;dy<=thickness;++dy)
            for(int dx=-thickness;dx<=thickness;++dx){
                int nx=x+dx,ny=y+dy;
                if(nx<0||nx>=size||ny<0||ny>=size)continue;
                thick[ny*size+nx]=col;
            }
        }
        pix1=std::move(thick);
    }

    // ── Layer 2: Collision Effects ────────────────────────────────────────────
    float ts=float(thickness)/3.f;
    float hC=0.055f*ts, hD=0.032f*ts, hT=0.018f*ts, hMax=hC;

    std::vector<uint32_t> pix2(size*size);
    for(int y=0;y<size;++y)
    for(int x=0;x<size;++x){
        int i=y*size+x; int pl=pi1[i]; float ep=bd[i];
        if(!isBnd(i)||ep>=hMax){pix2[i]=intCol(x,y,pl);continue;}
        int bc=btype(i);
        if(bc==0&&ep<hC){
            float t=ep/hC;
            uint32_t pc=palette[pl];
            int pr=pc&0xFF,pg=(pc>>8)&0xFF,pb=(pc>>16)&0xFF;
            float bump=pseudoNoise(x*3,y*3,noiseSeed+77u)*0.4f+pseudoNoise(x*7,y*7,noiseSeed+13u)*0.2f;
            float tn=std::clamp(t+(bump-0.3f)*0.5f,0.f,1.f);
            int r,g,b;
            if(tn<0.15f){float s=tn/0.15f;r=int(18+s*72);g=int(12+s*43);b=int(12+s*38);}
            else if(tn<0.55f){float s=(tn-0.15f)/0.4f;r=int(90+s*50);g=int(55+s*55);b=int(50+s*45);}
            else{float s=(tn-0.55f)/0.45f;r=int(140+s*(pr-140));g=int(110+s*(pg-110));b=int(95+s*(pb-95));}
            pix2[i]=rgba(uint8_t(std::clamp(r,0,255)),uint8_t(std::clamp(g,0,255)),uint8_t(std::clamp(b,0,255)));
        }else if(bc==2&&ep<hD){
            float t=ep/hD;
            uint32_t pc=palette[pl];
            int pr=pc&0xFF,pg=(pc>>8)&0xFF,pb=(pc>>16)&0xFF;
            float bump=pseudoNoise(x*5,y*5,noiseSeed+31u)*0.35f+pseudoNoise(x*11,y*11,noiseSeed+99u)*0.15f;
            float tn=std::clamp(t+(bump-0.25f)*0.6f,0.f,1.f);
            int r,g,b;
            if(tn<0.18f){float s=tn/0.18f;r=int(10+s*20);g=int(20+s*50);b=int(25+s*55);}
            else if(tn<0.55f){float s=(tn-0.18f)/0.37f;r=int(30+s*30);g=int(70+s*90);b=int(80+s*50);}
            else{float s=(tn-0.55f)/0.45f;r=int(60+s*(pr-60));g=int(160+s*(pg-160));b=int(130+s*(pb-130));}
            pix2[i]=rgba(uint8_t(std::clamp(r,0,255)),uint8_t(std::clamp(g,0,255)),uint8_t(std::clamp(b,0,255)));
        }else if(bc==1&&ep<hT){
            float t=ep/hT;
            uint32_t pc=palette[pl];
            int pr=pc&0xFF,pg=(pc>>8)&0xFF,pb=(pc>>16)&0xFF;
            float bump=pseudoNoise(x*9,y*9,noiseSeed+55u)*0.5f;
            float tn=std::clamp(t+(bump-0.25f)*0.7f,0.f,1.f);
            int r,g,b;
            if(tn<0.25f){float s=tn/0.25f;r=int(200+s*20);g=int(140+s*35);b=int(20+s*20);}
            else{float s=(tn-0.25f)/0.75f;r=int(220+s*(pr-220));g=int(175+s*(pg-175));b=int(40+s*(pb-40));}
            pix2[i]=rgba(uint8_t(std::clamp(r,0,255)),uint8_t(std::clamp(g,0,255)),uint8_t(std::clamp(b,0,255)));
        }else{
            pix2[i]=intCol(x,y,pl);
        }
    }

    // ── Layer 3: Elevation Heatmap ────────────────────────────────────────────
    std::vector<float> baseElev(plates.size());
    {
        std::mt19937 er(seed^0xC0FFEE42u);
        std::uniform_real_distribution<float> ud(0,1);
        for(size_t i=0;i<plates.size();++i){
            float r=ud(er);
            baseElev[i]=(r<0.4f)?(0.05f+r/0.4f*0.5f):(-0.85f+(r-0.4f)/0.6f*0.75f);
        }
    }

    float eHC=0.10f*ts, eHD=0.07f*ts, eHT=0.03f*ts;
    unsigned int enSeed=seed^0xFACEB00Cu;

    auto elevCol=[](float e)->uint32_t{
        e=std::clamp(e,-1.f,1.f);
        struct Stop{float e;uint8_t r,g,b;};
        static const Stop s[]={
            {-1.00f,  5, 10, 40},{-0.50f, 10, 40,100},{-0.10f, 20, 80,140},
            { 0.00f, 60,130,100},{ 0.10f, 80,150, 70},{ 0.35f,120,140, 60},
            { 0.55f,140,110, 50},{ 0.75f,110, 85, 60},{ 0.90f,180,175,175},
            { 1.00f,240,240,255}
        };
        constexpr int N=10;
        int lo=0;
        for(int i=1;i<N;++i){if(s[i].e>=e){lo=i-1;break;}lo=N-2;}
        lo=std::clamp(lo,0,N-2);
        float t=std::clamp((e-s[lo].e)/(s[lo+1].e-s[lo].e+1e-9f),0.f,1.f);
        auto l8=[](uint8_t a,uint8_t b,float t)->uint8_t{
            return uint8_t(std::clamp<int>(int(a+t*(b-a)),0,255));};
        return rgba(l8(s[lo].r,s[lo+1].r,t),l8(s[lo].g,s[lo+1].g,t),l8(s[lo].b,s[lo+1].b,t));
    };

    std::vector<uint32_t> pix3(size*size);
    for(int y=0;y<size;++y)
    for(int x=0;x<size;++x){
        int i=y*size+x; int pl=pi1[i]; float ep=bd[i];
        float elev=baseElev[pl];
        if(isBnd(i)){
            int bc=btype(i);
            if(bc==0&&ep<eHC){float t=1-ep/eHC; elev+=t*t*0.70f;}
            else if(bc==2&&ep<eHD){float t=1-ep/eHD; elev-=t*t*0.55f;}
            else if(bc==1&&ep<eHT){float t=1-ep/eHT; elev-=t*0.15f;}
        }
        SphereDir cd=toSphereDirection(float(x),float(y),size);
        float coarse=(valueNoise3D(cd.x*3,cd.y*3,cd.z*3,enSeed)*2-1)
                    +(valueNoise3D(cd.x*6,cd.y*6,cd.z*6,enSeed+1u)-0.5f);
        float fine  =(valueNoise3D(cd.x*14,cd.y*14,cd.z*14,enSeed+2u)*2-1)*0.5f
                    +(valueNoise3D(cd.x*28,cd.y*28,cd.z*28,enSeed+3u)*2-1)*0.25f;
        elev+=coarse*0.18f+fine*0.08f;
        pix3[i]=elevCol(elev);
    }

    // ── Layer 4: Climate (wind + moisture + erosion) ──────────────────────────
    //
    // Step 1 — Elevation field (reuse pix3 data via a float buffer)
    //   We already have pix3 as RGBA; rebuild a float elevation grid here.
    //
    // Step 2 — Wind vectors from latitude (Hadley/Ferrel/Polar cells + Coriolis)
    //   Each pixel gets (wx, wy) in equirectangular map space.
    //
    // Step 3 — Moisture transport
    //   Start with ocean pixels fully moist. March along wind direction,
    //   depositing moisture when air rises (orographic lift) and drying out
    //   on the leeward side (rain shadow).
    //
    // Step 4 — Erosion
    //   Erode the elevation proportional to moisture × local slope.
    //   This softens mountain peaks and fills valleys in wet regions.
    //
    // Step 5 — Render climate layer
    //   Colour by moisture: desert (sandy) → savanna → temperate → rainforest (deep green)
    //   Overlay wind arrows every N pixels.

    // ── 4a. Float elevation grid ──────────────────────────────────────────────
    std::vector<float> elevGrid(size*size);
    for(int y=0;y<size;++y)
    for(int x=0;x<size;++x){
        int i=y*size+x; int pl=pi1[i]; float ep=bd[i];
        float elev=baseElev[pl];
        if(isBnd(i)){
            int bc=btype(i);
            if(bc==0&&ep<eHC){float t=1-ep/eHC; elev+=t*t*0.70f;}
            else if(bc==2&&ep<eHD){float t=1-ep/eHD; elev-=t*t*0.55f;}
            else if(bc==1&&ep<eHT){float t=1-ep/eHT; elev-=t*0.15f;}
        }
        SphereDir cd=toSphereDirection(float(x),float(y),size);
        float coarse=(valueNoise3D(cd.x*3,cd.y*3,cd.z*3,enSeed)*2-1)
                    +(valueNoise3D(cd.x*6,cd.y*6,cd.z*6,enSeed+1u)-0.5f);
        float fine  =(valueNoise3D(cd.x*14,cd.y*14,cd.z*14,enSeed+2u)*2-1)*0.5f
                    +(valueNoise3D(cd.x*28,cd.y*28,cd.z*28,enSeed+3u)*2-1)*0.25f;
        elevGrid[i]=std::clamp(elev+coarse*0.18f+fine*0.08f,-1.f,1.f);
    }

    // ── 4b. Wind vectors ──────────────────────────────────────────────────────
    // Latitude in [-PI/2, PI/2]; y=0 is north pole in equirectangular.
    // wx = east component, wy = south component (positive = southward in image).
    // Hadley cell  (|lat|<30°): easterly trade winds  → wx<0, wy toward equator
    // Ferrel cell  (30-60°):    westerlies             → wx>0, wy toward poles
    // Polar cell   (>60°):      polar easterlies       → wx<0, wy toward equator
    // Coriolis deflects N-hemi rightward, S-hemi leftward.
    unsigned int windSeed = seed ^ 0xBADF00Du;
    std::vector<float> wx(size*size), wy(size*size);
    for(int y=0;y<size;++y){
        // latitude: top of image = +PI/2 (north pole)
        float lat = PI*(0.5f - float(y)/float(size));  // [-PI/2, PI/2]
        float absLat = std::fabs(lat);
        float latDeg = absLat * 180.f / PI;
        float sign   = (lat >= 0.f) ? 1.f : -1.f;  // +1 north, -1 south

        // Base zonal (east-west) and meridional (north-south) components
        float baseWx, baseWy;
        if(latDeg < 30.f){
            // Trade winds: blow westward (wx<0) and toward equator
            float t = latDeg / 30.f;
            baseWx = -1.0f;                      // easterly
            baseWy =  sign * t * 0.3f;           // slight equatorward drift
        } else if(latDeg < 60.f){
            // Westerlies: blow eastward (wx>0) and poleward
            float t = (latDeg-30.f)/30.f;
            baseWx =  1.0f;                      // westerly
            baseWy = -sign * t * 0.3f;           // slight poleward drift
        } else {
            // Polar easterlies: blow westward and equatorward
            float t = (latDeg-60.f)/30.f;
            baseWx = -0.6f;
            baseWy =  sign * t * 0.2f;
        }

        for(int x=0;x<size;++x){
            int i=y*size+x;
            // Add low-frequency weather noise (large-scale pressure systems)
            float nx = valueNoise3D(float(x)*0.03f, float(y)*0.03f, 0.f, windSeed+0u)*2-1;
            float ny = valueNoise3D(float(x)*0.03f, float(y)*0.03f, 0.f, windSeed+1u)*2-1;
            wx[i] = baseWx + nx*0.4f;
            wy[i] = baseWy + ny*0.4f;
            // Normalise to unit length
            float len=std::sqrt(wx[i]*wx[i]+wy[i]*wy[i]);
            if(len>0){wx[i]/=len; wy[i]/=len;}
        }
    }

    // ── 4c. Moisture transport ────────────────────────────────────────────────
    // Full two-way advection: winds carry BOTH moist and dry air.
    //
    // Key insight: we use a weighted blend (not max) so that dry air arriving
    // from a continental interior or polar region can DISPLACE moist air and
    // actually dry out a region — just like the Atacama/Namib deserts form
    // when dry trade winds blow off hot continents toward the coast.
    //
    // Initialisation: ocean=1.0 (saturated), land seeded with a small base
    // moisture proportional to proximity to ocean (so the starting state is
    // physically reasonable before advection begins).
    //
    // Each pass: every land pixel blends its current moisture with the
    // moisture arriving from 1 pixel upwind. The blend weight controls how
    // quickly the air mass is replaced (advection strength).
    //
    // Orographic lift: upslope air loses moisture (rain); downslope air
    // descends and warms (föhn effect) — it arrives drier than it left.
    //
    // Lateral diffusion every 8 passes breaks up linear streaks.

    // ── Seed moisture: ocean=1, land=small base value ─────────────────────────
    std::vector<float> moisture(size*size, 0.f);
    for(int i=0;i<size*size;++i)
        moisture[i] = (elevGrid[i] < 0.f) ? 1.f : 0.05f;

    // ── Advection ─────────────────────────────────────────────────────────────
    // advectStrength: how much of the arriving air mass replaces the current
    // pixel's moisture each pass. 0.08 gives good inland penetration while
    // still producing smooth gradients without bleeding artefacts.
    const float advectStrength = 0.08f;
    const int   windPasses     = size;   // enough passes to cross the full map

    for(int pass=0;pass<windPasses;++pass){
        std::vector<float> newMoisture = moisture;

        for(int y=0;y<size;++y)
        for(int x=0;x<size;++x){
            int i=y*size+x;
            // Ocean is always fully saturated — it is the boundary condition.
            if(elevGrid[i]<0.f){ newMoisture[i]=1.f; continue; }

            // Sample moisture 1 pixel upwind (bilinear for smooth gradients)
            float wxi=wx[i], wyi=wy[i];
            float srcX = float(x) - wxi;
            float srcY = float(y) - wyi;

            int x0=int(std::floor(srcX)), y0=int(std::floor(srcY));
            int x1=x0+1, y1=y0+1;
            float fx=srcX-x0, fy=srcY-y0;
            auto wrapX=[&](int v){return ((v%size)+size)%size;};
            auto clampY=[&](int v){return std::clamp(v,0,size-1);};

            float m00=moisture[clampY(y0)*size+wrapX(x0)];
            float m10=moisture[clampY(y0)*size+wrapX(x1)];
            float m01=moisture[clampY(y1)*size+wrapX(x0)];
            float m11=moisture[clampY(y1)*size+wrapX(x1)];
            float srcMoist=(m00*(1-fx)+m10*fx)*(1-fy)+(m01*(1-fx)+m11*fx)*fy;

            // Bilinear elevation at source pixel
            float e00=elevGrid[clampY(y0)*size+wrapX(x0)];
            float e10=elevGrid[clampY(y0)*size+wrapX(x1)];
            float e01=elevGrid[clampY(y1)*size+wrapX(x0)];
            float e11=elevGrid[clampY(y1)*size+wrapX(x1)];
            float elevSrc=(e00*(1-fx)+e10*fx)*(1-fy)+(e01*(1-fx)+e11*fx)*fy;

            float elevDiff = elevGrid[i] - elevSrc;

            // Orographic lift (upslope): air cools, moisture condenses → rain
            // Föhn effect (downslope): air warms and dries out further
            float moistureChange;
            if(elevDiff > 0.f){
                // Upslope: lose moisture proportional to ascent
                float rainFraction = std::clamp(elevDiff * 1.2f, 0.f, 0.40f);
                moistureChange = srcMoist * (1.f - rainFraction);
            } else {
                // Downslope (föhn): descending air warms and becomes drier
                // than it would be from simple advection — amplify the drying
                float dryBoost = std::clamp(-elevDiff * 0.6f, 0.f, 0.20f);
                moistureChange = srcMoist * (1.f - dryBoost);
            }

            // Gentle background evaporation loss over land
            moistureChange *= 0.988f;

            // TWO-WAY BLEND: arriving air mass partially replaces current moisture.
            // This is the key change — dry air CAN displace moist air.
            // advectStrength controls how quickly the air mass turns over.
            newMoisture[i] = moisture[i] * (1.f - advectStrength)
                           + moistureChange * advectStrength;
            newMoisture[i] = std::clamp(newMoisture[i], 0.f, 1.f);
        }

        moisture = std::move(newMoisture);

        // Lateral diffusion every 4 passes: spreads moisture sideways,
        // breaks up linear streaks, simulates turbulent mixing.
        // More frequent diffusion = smoother, more uniform gradients.
        if((pass+1)%4==0){
            std::vector<float> sm = moisture;
            for(int y=1;y<size-1;++y)
            for(int x=0;x<size;++x){
                int i=y*size+x;
                if(elevGrid[i]<0.f) continue;
                int xL=((x-1)+size)%size, xR=(x+1)%size;
                // Stronger diffusion kernel: 40% self, 15% each cardinal neighbour
                sm[i] = moisture[i]*0.40f
                       + moisture[y*size+xL]*0.15f
                       + moisture[y*size+xR]*0.15f
                       + moisture[(y-1)*size+x]*0.15f
                       + moisture[(y+1)*size+x]*0.15f;
            }
            moisture=std::move(sm);
            // Re-enforce ocean boundary after diffusion
            for(int i=0;i<size*size;++i)
                if(elevGrid[i]<0.f) moisture[i]=1.f;
        }
    }

    // ── 4c-ii. River-fed moisture (post-advection) ────────────────────────────
    // After wind advection converges, rivers act as LOCAL moisture sources that
    // irrigate land on ALL sides regardless of wind direction.
    // We do this AFTER the main advection loop so it doesn't interfere with the
    // atmospheric moisture transport — it's a separate ground-level effect.
    //
    // Algorithm: compute a rough flow proxy from the elevation gradient
    // (steepest-descent accumulation), then diffuse moisture outward from
    // high-flow cells isotropically (no wind direction involved).
    //
    // We use a simple iterative blur seeded by flow-proxy values so that
    // pixels near rivers/streams get a moisture boost on all sides.
    {
        // Quick flow proxy: for each land pixel, estimate local drainage by
        // counting how many of its 8 neighbours are higher (it receives their runoff).
        // This is a cheap approximation — the real D8 flow is computed later in
        // the Hydrology layer, but we need something here for the climate layer.
        static const int fdx8[8]={1,1,0,-1,-1,-1,0,1};
        static const int fdy8[8]={0,1,1, 1, 0,-1,-1,-1};
        std::vector<float> flowProxy(size*size, 0.f);
        for(int y=1;y<size-1;++y)
        for(int x=0;x<size;++x){
            int i=y*size+x;
            if(elevGrid[i]<0.f) continue;
            float contrib=0.f;
            for(int d=0;d<8;++d){
                int nx2=((x+fdx8[d])%size+size)%size;
                int ny2=std::clamp(y+fdy8[d],0,size-1);
                int ni=ny2*size+nx2;
                if(elevGrid[ni]>elevGrid[i]) contrib+=1.f;
            }
            flowProxy[i]=contrib/8.f;
        }

        // Diffuse river moisture outward for several passes.
        // Each pass spreads moisture from high-flow cells to their neighbours.
        // The spread is isotropic (equal in all directions) — no wind bias.
        const int riverDiffPasses = std::max(size/8, 4);
        const float riverDiffDecay = 0.82f;  // moisture retained per step

        std::vector<float> riverMoist(size*size, 0.f);
        // Seed: ocean pixels and high-flow land pixels start moist
        for(int i=0;i<size*size;++i){
            if(elevGrid[i]<0.f) riverMoist[i]=1.f;
            else riverMoist[i]=flowProxy[i]*moisture[i];
        }

        for(int pass=0;pass<riverDiffPasses;++pass){
            std::vector<float> nm=riverMoist;
            for(int y=1;y<size-1;++y)
            for(int x=0;x<size;++x){
                int i=y*size+x;
                if(elevGrid[i]<0.f) continue;
                int xL=((x-1)+size)%size, xR=(x+1)%size;
                // Take max of self and decayed neighbours (isotropic spread)
                float best=riverMoist[i];
                best=std::max(best,riverMoist[y*size+xL]*riverDiffDecay);
                best=std::max(best,riverMoist[y*size+xR]*riverDiffDecay);
                best=std::max(best,riverMoist[(y-1)*size+x]*riverDiffDecay);
                best=std::max(best,riverMoist[(y+1)*size+x]*riverDiffDecay);
                nm[i]=best;
            }
            riverMoist=std::move(nm);
            // Re-enforce ocean boundary
            for(int i=0;i<size*size;++i)
                if(elevGrid[i]<0.f) riverMoist[i]=1.f;
        }

        // Blend river-fed moisture into the atmospheric moisture.
        // riverMoist contributes up to 45% of the final moisture value,
        // so river valleys are noticeably greener than surrounding terrain.
        for(int i=0;i<size*size;++i){
            if(elevGrid[i]<0.f) continue;
            moisture[i]=std::clamp(
                moisture[i]*0.60f + riverMoist[i]*0.45f, 0.f, 1.f);
        }
    }

    // ── 4d. Erosion ───────────────────────────────────────────────────────────
    // Erode elevation proportional to moisture and local slope magnitude.
    // High moisture + steep slope = strong erosion (rivers carve valleys).
    // Apply to a copy so we don't feedback into the same pass.
    std::vector<float> erodedElev = elevGrid;
    for(int y=1;y<size-1;++y)
    for(int x=1;x<size-1;++x){
        int i=y*size+x;
        if(elevGrid[i]<0.f) continue; // don't erode ocean floor

        // Local slope magnitude (central differences)
        float dzdx=(elevGrid[i+1]-elevGrid[i-1])*0.5f;
        float dzdy=(elevGrid[(y+1)*size+x]-elevGrid[(y-1)*size+x])*0.5f;
        float slope=std::sqrt(dzdx*dzdx+dzdy*dzdy);

        float erosion = moisture[i] * slope * 0.35f;
        erodedElev[i] = std::clamp(elevGrid[i]-erosion, -1.f, 1.f);
    }

    // ── 4e. Temperature field ─────────────────────────────────────────────────
    // temp in [0,1]: 1=hot (equator), 0=freezing (poles / high altitude)
    // Base: cosine of latitude (equator=1, poles=0)
    // Altitude lapse: subtract ~0.65°C per 100m; we map elev [0,1] → ~0.5 temp drop
    std::vector<float> tempGrid(size*size);
    for(int y=0;y<size;++y){
        float lat=PI*(0.5f-float(y)/float(size));   // [-PI/2, PI/2]
        float basetemp=std::cos(lat);                // 1 at equator, 0 at poles
        for(int x=0;x<size;++x){
            int i=y*size+x;
            float elev=erodedElev[i];
            // Altitude lapse: land above sea level cools; ocean is moderated
            float altCool = (elev > 0.f) ? elev * 0.55f : 0.f;
            tempGrid[i]=std::clamp(basetemp - altCool, 0.f, 1.f);
        }
    }

    // ── 4f. Biome colour from temperature × moisture (Whittaker diagram) ──────
    // Returns RGBA for a land pixel given temp [0,1] and moisture [0,1].
    // Biome matrix (approximate):
    //
    //          Cold(0-0.25)      Cool(0.25-0.5)    Warm(0.5-0.75)    Hot(0.75-1)
    // Dry      Ice/tundra        Cold steppe        Hot steppe        Hot desert
    // Moderate Tundra shrub      Boreal/taiga       Dry woodland      Savanna
    // Wet      Boreal forest     Temp. forest       Subtrop. forest   Tropical RF
    //
    // We implement this as a smooth 2-D blend between corner biome colours.

    auto biomeCol=[](float temp, float moist)->uint32_t{
        // Corner biome colours (r,g,b):
        // cold-dry, cold-wet, hot-dry, hot-wet
        // We use a 2×2 bilinear blend across the temp/moisture space.

        // Clamp inputs
        temp  = std::clamp(temp,  0.f, 1.f);
        moist = std::clamp(moist, 0.f, 1.f);

        // Define a 4×4 grid of biome colours indexed by [temp_band][moist_band]
        // temp bands:  0=freezing(<0.15), 1=cold(0.15-0.4), 2=warm(0.4-0.7), 3=hot(>0.7)
        // moist bands: 0=arid(<0.15), 1=dry(0.15-0.4), 2=moist(0.4-0.7), 3=wet(>0.7)
        // Softened palette: adjacent biomes share similar mid-tones so the
        // bilinear blend produces smooth, natural-looking transitions rather
        // than harsh colour jumps.
        struct C{uint8_t r,g,b;};
        static const C grid[4][4]={
            // moist:  arid              dry               moist             wet
            /* cold */ {{215,225,235},   {185,200,205},    {165,185,175},    {145,170,160}},  // ice/tundra
            /* cool */ {{195,180,140},   {155,165,110},    {100,145, 90},    { 70,130, 80}},  // steppe/boreal
            /* warm */ {{205,185,105},   {175,170, 85},    {110,150, 75},    { 60,120, 65}},  // desert/savanna/forest
            /* hot  */ {{215,185, 95},   {185,165, 80},    {120,145, 65},    { 45,105, 55}},  // hot desert/tropical
        };

        // Map temp and moist to continuous grid coordinates
        // temp: 0→0, 0.15→0.5, 0.4→1.5, 0.7→2.5, 1→3
        auto tempToGrid=[](float t)->float{
            if(t<0.15f) return t/0.15f*0.5f;
            if(t<0.40f) return 0.5f+(t-0.15f)/0.25f;
            if(t<0.70f) return 1.5f+(t-0.40f)/0.30f;
            return 2.5f+(t-0.70f)/0.30f*0.5f;
        };
        // Shifted moisture thresholds: "arid" band is narrower (only below 0.10),
        // so moderate moisture (0.10-0.35) maps to dry grassland/steppe rather
        // than desert. This reduces the amount of desert on typical maps.
        auto moistToGrid=[](float m)->float{
            if(m<0.10f) return m/0.10f*0.5f;           // arid: 0.0-0.10 → grid 0.0-0.5
            if(m<0.35f) return 0.5f+(m-0.10f)/0.25f;   // dry:  0.10-0.35 → grid 0.5-1.5
            if(m<0.65f) return 1.5f+(m-0.35f)/0.30f;   // moist:0.35-0.65 → grid 1.5-2.5
            return 2.5f+(m-0.65f)/0.35f*0.5f;           // wet:  0.65-1.0  → grid 2.5-3.0
        };

        float gx=std::clamp(tempToGrid(temp),  0.f, 3.f);
        float gy=std::clamp(moistToGrid(moist), 0.f, 3.f);

        int tx=std::min(int(gx),2), ty=std::min(int(gy),2);
        float fx=gx-tx, fy=gy-ty;

        // Bilinear interpolation over the 2×2 cell
        auto lerp8=[](uint8_t a,uint8_t b,float t)->float{return a+t*(b-a);};
        auto blendC=[&](int ti,int mi)->C{return grid[ti][mi];};

        float r=lerp8(lerp8(blendC(tx,ty).r,  blendC(tx+1,ty).r,  fx),
                      lerp8(blendC(tx,ty+1).r, blendC(tx+1,ty+1).r,fx), fy);
        float g=lerp8(lerp8(blendC(tx,ty).g,  blendC(tx+1,ty).g,  fx),
                      lerp8(blendC(tx,ty+1).g, blendC(tx+1,ty+1).g,fx), fy);
        float b=lerp8(lerp8(blendC(tx,ty).b,  blendC(tx+1,ty).b,  fx),
                      lerp8(blendC(tx,ty+1).b, blendC(tx+1,ty+1).b,fx), fy);

        return rgba(uint8_t(std::clamp<int>(int(r),0,255)),
                    uint8_t(std::clamp<int>(int(g),0,255)),
                    uint8_t(std::clamp<int>(int(b),0,255)));
    };

    std::vector<uint32_t> pix4(size*size);
    for(int y=0;y<size;++y)
    for(int x=0;x<size;++x){
        int i=y*size+x;
        float e=erodedElev[i];
        if(e<0.f){
            pix4[i]=elevCol(e);  // ocean: use elevation blue palette
        } else {
            pix4[i]=biomeCol(tempGrid[i], moisture[i]);
        }
    }

    // Wind arrow overlay — draw small arrows every arrowStep pixels
    // Arrow: a short line from the pixel in the wind direction, plus a tiny head
    {
        int arrowStep = std::max(size/24, 6);
        for(int ay=arrowStep/2; ay<size; ay+=arrowStep)
        for(int ax=arrowStep/2; ax<size; ax+=arrowStep){
            int ai=ay*size+ax;
            float wxi=wx[ai], wyi=wy[ai];
            int len=arrowStep/2;

            // Shaft
            for(int s2=0;s2<=len;++s2){
                int px=ax+int(wxi*s2+0.5f);
                int py=ay+int(wyi*s2+0.5f);
                if(px<0||px>=size||py<0||py>=size) continue;
                pix4[py*size+px]=rgba(255,255,255,200);
            }
            // Arrowhead (two short lines at ~45° to shaft)
            int tx=ax+int(wxi*len+0.5f);
            int ty=ay+int(wyi*len+0.5f);
            // Perpendicular
            float px2=-wyi, py2=wxi;
            for(int s2=1;s2<=len/2;++s2){
                auto plot=[&](int bx,int by){
                    if(bx>=0&&bx<size&&by>=0&&by<size)
                        pix4[by*size+bx]=rgba(255,255,255,200);
                };
                plot(tx+int((-wxi+px2)*s2*0.5f+0.5f), ty+int((-wyi+py2)*s2*0.5f+0.5f));
                plot(tx+int((-wxi-px2)*s2*0.5f+0.5f), ty+int((-wyi-py2)*s2*0.5f+0.5f));
            }
        }
    }

    // Also update pix3 with eroded elevation so the Elevation layer reflects erosion
    for(int i=0;i<size*size;++i)
        pix3[i]=elevCol(erodedElev[i]);

    // ── Layer 5: Hydrology (rivers, lakes, drainage basins) ───────────────────
    //
    // Algorithm:
    //  1. D8 flow direction: each land pixel drains to its lowest of 8 neighbours.
    //     Ocean pixels are sinks. Flat/local-minimum pixels drain to themselves
    //     (potential lake cells).
    //  2. Flow accumulation: topological sort upstream→downstream, count how many
    //     pixels drain through each cell. High accumulation = major river.
    //  3. Rainfall input: each land pixel contributes moisture[i] to the flow.
    //     Arid pixels contribute little, so rivers dry up in deserts.
    //  4. Lake detection: cells that drain to a local minimum (no lower neighbour)
    //     are marked as lakes. We flood-fill from each minimum up to the spill
    //     elevation to determine lake extent.
    //  5. River erosion: high-flow cells have their elevation lowered slightly,
    //     carving valleys.
    //  6. Render: elevation heatmap base + blue rivers (width ∝ log(flow)) +
    //     lake fill + endorheic basin tint.

    // ── 5a. Working elevation (use erodedElev, further carved by rivers) ──────
    std::vector<float> hydroElev = erodedElev;

    // ── 5b. D8 flow direction ─────────────────────────────────────────────────
    // dir[i] = index of the pixel this cell drains to, or i if it's a sink/lake.
    const int dx8[8]={1,1,0,-1,-1,-1, 0, 1};
    const int dy8[8]={0,1,1, 1, 0,-1,-1,-1};

    std::vector<int> flowDir(size*size);
    for(int y=0;y<size;++y)
    for(int x=0;x<size;++x){
        int i=y*size+x;
        // Ocean pixels are sinks — they drain to themselves
        if(hydroElev[i]<0.f){ flowDir[i]=i; continue; }

        float minE=hydroElev[i];
        int   best=i;  // default: drain to self (local minimum)
        for(int d=0;d<8;++d){
            int nx=x+dx8[d], ny=y+dy8[d];
            // Wrap longitude, clamp latitude
            nx=((nx%size)+size)%size;
            ny=std::clamp(ny,0,size-1);
            int ni=ny*size+nx;
            if(hydroElev[ni]<minE){ minE=hydroElev[ni]; best=ni; }
        }
        flowDir[i]=best;
    }

    // ── 5c. Flow accumulation (topological sort) ──────────────────────────────
    // Count in-degree (how many pixels drain INTO each pixel)
    std::vector<int> inDeg(size*size,0);
    for(int i=0;i<size*size;++i)
        if(flowDir[i]!=i) inDeg[flowDir[i]]++;

    // Topological sort: process sources first (in-degree 0), propagate downstream
    std::vector<int> order;
    order.reserve(size*size);
    std::vector<int> queue;
    queue.reserve(size*size/4);
    for(int i=0;i<size*size;++i)
        if(inDeg[i]==0) queue.push_back(i);

    while(!queue.empty()){
        int cur=queue.back(); queue.pop_back();
        order.push_back(cur);
        int dn=flowDir[cur];
        if(dn!=cur){
            inDeg[dn]--;
            if(inDeg[dn]==0) queue.push_back(dn);
        }
    }

    // Accumulate rainfall-weighted flow
    // Each land pixel contributes its moisture as "rainfall"
    std::vector<float> flow(size*size,0.f);
    for(int i=0;i<size*size;++i)
        if(hydroElev[i]>=0.f) flow[i]=moisture[i];  // rainfall input

    for(int idx=0;idx<int(order.size());++idx){
        int i=order[idx];
        int dn=flowDir[i];
        if(dn!=i){
            // Arid drying: flow evaporates proportional to aridity
            float aridity=std::max(0.f,1.f-moisture[i]*3.f);
            float evap=std::clamp(aridity*0.15f,0.f,0.9f);
            flow[dn]+=flow[i]*(1.f-evap);
        }
    }

    // Normalise flow to [0,1] using log scale (rivers span many orders of magnitude)
    float maxFlow=0.f;
    for(int i=0;i<size*size;++i) maxFlow=std::max(maxFlow,flow[i]);
    std::vector<float> flowNorm(size*size,0.f);
    if(maxFlow>0.f){
        float logMax=std::log(maxFlow+1.f);
        for(int i=0;i<size*size;++i)
            flowNorm[i]=std::log(flow[i]+1.f)/logMax;
    }

    // ── 5d. Lake detection ────────────────────────────────────────────────────
    // A cell is a lake seed if it drains to itself AND is a land cell.
    // We flood-fill from each seed up to a small spill height to get lake extent.
    std::vector<bool> isLake(size*size,false);
    {
        float spillHeight=0.04f;  // lakes fill up to this height above the minimum
        for(int i=0;i<size*size;++i){
            if(flowDir[i]==i && hydroElev[i]>=0.f){
                // Flood fill: mark all connected land cells within spillHeight
                float lakeLevel=hydroElev[i]+spillHeight;
                std::vector<int> stack; stack.push_back(i);
                std::vector<bool> visited(size*size,false);
                visited[i]=true;
                while(!stack.empty()){
                    int cur=stack.back(); stack.pop_back();
                    isLake[cur]=true;
                    int cy=cur/size, cx=cur%size;
                    for(int d=0;d<4;++d){  // 4-connected for lakes
                        int nx2=cx+dx8[d*2], ny2=cy+dy8[d*2];
                        nx2=((nx2%size)+size)%size;
                        ny2=std::clamp(ny2,0,size-1);
                        int ni=ny2*size+nx2;
                        if(!visited[ni] && hydroElev[ni]>=0.f
                           && hydroElev[ni]<=lakeLevel){
                            visited[ni]=true;
                            stack.push_back(ni);
                        }
                    }
                }
            }
        }
    }

    // ── 5e. River erosion ─────────────────────────────────────────────────────
    // High-flow cells carve their elevation down slightly
    for(int i=0;i<size*size;++i){
        if(hydroElev[i]<0.f || isLake[i]) continue;
        float carve=flowNorm[i]*flowNorm[i]*0.12f;
        hydroElev[i]=std::clamp(hydroElev[i]-carve,-1.f,1.f);
    }

    // ── 5f. Render hydrology layer ────────────────────────────────────────────
    // Base: elevation heatmap of the carved terrain
    // Rivers: blue overlay, opacity proportional to log(flow)
    // Lakes: flat blue-grey fill
    // Endorheic basins (rivers that dry up): shown with a warm sandy tint

    // River threshold: only draw pixels above this normalised flow value.
    // Lower = more rivers visible (including small tributaries and streams).
    const float riverThreshold = 0.18f;

    std::vector<uint32_t> pix5(size*size);
    for(int y=0;y<size;++y)
    for(int x=0;x<size;++x){
        int i=y*size+x;
        float e=hydroElev[i];

        if(e<0.f){
            // Ocean
            pix5[i]=elevCol(e);
        } else if(isLake[i]){
            // Lake: calm blue-grey
            pix5[i]=rgba(80,130,170);
        } else if(flowNorm[i]>riverThreshold){
            // River: blend from terrain colour toward deep blue based on flow
            float t=std::clamp((flowNorm[i]-riverThreshold)/(1.f-riverThreshold),0.f,1.f);
            // Narrow rivers: lighter blue; wide rivers: deep blue
            uint8_t rr=uint8_t(std::clamp<int>(int(60-t*40),0,255));
            uint8_t rg=uint8_t(std::clamp<int>(int(120+t*20),0,255));
            uint8_t rb=uint8_t(std::clamp<int>(int(180+t*50),0,255));
            pix5[i]=rgba(rr,rg,rb);
        } else {
            // Land: use biome colour from climate layer, tinted by flow
            // (slight green tint near rivers = riparian vegetation)
            uint32_t base=biomeCol(tempGrid[i],moisture[i]);
            if(flowNorm[i]>0.15f){
                float t=(flowNorm[i]-0.15f)/0.20f;
                uint8_t br=uint8_t(base&0xFF);
                uint8_t bg=uint8_t((base>>8)&0xFF);
                uint8_t bb=uint8_t((base>>16)&0xFF);
                // Riparian: slightly greener and darker
                br=uint8_t(std::clamp<int>(int(br*(1-t*0.15f)),0,255));
                bg=uint8_t(std::clamp<int>(int(bg*(1+t*0.08f)),0,255));
                bb=uint8_t(std::clamp<int>(int(bb*(1-t*0.10f)),0,255));
                pix5[i]=rgba(br,bg,bb);
            } else {
                pix5[i]=base;
            }
        }
    }

    // ── Pack into snapshot ────────────────────────────────────────────────────
    snap.layerPixels[0]=std::move(pix0);
    snap.layerPixels[1]=std::move(pix1);
    snap.layerPixels[2]=std::move(pix2);
    snap.layerPixels[3]=std::move(pix3);
    snap.layerPixels[4]=std::move(pix4);
    snap.layerPixels[5]=std::move(pix5);
    for(int L=0;L<int(MapLayer::Count);++L)
        snap.layerMercator[L]=toMercator(snap.layerPixels[L],size);

    return snap;
}

} // namespace

MapResult generateMap(const GeneratorSettings& settings)
{
    int size       = std::clamp(settings.mapSize,       64, 2048);
    int plateCount = std::clamp(settings.plateCount,     2,   64);
    int thickness  = std::clamp(settings.faultIntensity, 1,    8);
    int snapCount  = std::clamp(settings.snapshotCount,  1,   20);
    float roughness= std::clamp(settings.boundaryRoughness,0.f,1.f);

    struct Plate { SphereDir center, axis; float angularVelocity; };

    std::mt19937 rng(settings.seed ? settings.seed
                                   : static_cast<unsigned int>(std::random_device{}()));
    std::uniform_int_distribution<int>   posDist(0,size-1);
    std::uniform_real_distribution<float> floatDist(0,1);

    std::vector<Plate> plates;
    plates.reserve(plateCount);
    for(int i=0;i<plateCount;++i){
        SphereDir c=toSphereDirection(posDist(rng),posDist(rng),size);
        SphereDir a=randomUnitSphereDir(rng);
        float sp=std::clamp(settings.angularVelocity*(0.4f+floatDist(rng)*0.8f),0.1f,4.f);
        plates.push_back({c,a,sp});
    }

    // ── Simulation with snapshot capture ─────────────────────────────────────
    int   totalSteps = std::max(settings.simulationSteps, 1);
    float totalTime  = settings.simulationTime;
    float stepTime   = totalTime / float(totalSteps);
    float collisionThreshold = 0.22f;

    // Determine which steps to snapshot at.
    // We always include the final step. Distribute the rest evenly.
    // snapTimes[k] = elapsed time after which we take snapshot k.
    std::vector<float> snapTimes;
    snapTimes.reserve(snapCount);
    for(int k=1;k<=snapCount;++k)
        snapTimes.push_back(totalTime * float(k) / float(snapCount));

    MapResult result;
    result.size = size;
    result.snapshots.reserve(snapCount);

    float elapsed = 0.f;
    int   nextSnap = 0;  // index into snapTimes

    // Helper: classify motion categories for current plate set
    auto classifyPlates = [&]() -> std::vector<int>
    {
        std::vector<int> cat(plates.size(), 1);
        for(size_t i=0;i<plates.size();++i){
            int nearest=-1; float bd=1e9f;
            for(size_t j=0;j<plates.size();++j){
                if(i==j)continue;
                float d=chordSq(plates[i].center,plates[j].center);
                if(d<bd){bd=d;nearest=int(j);}
            }
            if(nearest<0)continue;
            SphereDir vA=cross(plates[i].axis,plates[i].center);
            SphereDir vB=cross(plates[nearest].axis,plates[nearest].center);
            vA={vA.x*plates[i].angularVelocity,vA.y*plates[i].angularVelocity,vA.z*plates[i].angularVelocity};
            vB={vB.x*plates[nearest].angularVelocity,vB.y*plates[nearest].angularVelocity,vB.z*plates[nearest].angularVelocity};
            SphereDir rel={vA.x-vB.x,vA.y-vB.y,vA.z-vB.z};
            SphereDir dir=normalize({plates[nearest].center.x-plates[i].center.x,
                                     plates[nearest].center.y-plates[i].center.y,
                                     plates[nearest].center.z-plates[i].center.z});
            float al=dot(rel,dir);
            if(al<-0.05f)cat[i]=0;
            else if(al>0.05f)cat[i]=2;
        }
        return cat;
    };

    // The palette size is fixed to the maximum possible plate count so that
    // plate N always maps to the same colour across every snapshot.
    // If fragmentation is on, plates can grow up to 64; otherwise it stays at plateCount.
    int paletteSize = settings.fragmentation ? 64 : plateCount;

    for(int step=0; step<totalSteps; ++step)
    {
        // Advance plates
        for(auto& p:plates)
            p.center=rotateAroundAxis(p.center,p.axis,p.angularVelocity*stepTime);
        elapsed+=stepTime;

        // Fragmentation
        if(settings.fragmentation){
            for(size_t i=0;i<plates.size()&&plates.size()<64;++i)
            for(size_t j=i+1;j<plates.size()&&plates.size()<64;++j){
                float angle=std::acos(std::clamp(dot(plates[i].center,plates[j].center),-1.f,1.f));
                if(angle<collisionThreshold){
                    SphereDir mid=normalize({plates[i].center.x+plates[j].center.x,
                                             plates[i].center.y+plates[j].center.y,
                                             plates[i].center.z+plates[j].center.z});
                    SphereDir ax=normalize(cross(mid,randomUnitSphereDir(rng)));
                    float sp=std::clamp((plates[i].angularVelocity+plates[j].angularVelocity)*0.5f,0.1f,4.f);
                    plates.push_back({mid,ax,sp});
                }
            }
        }

        // Check if we've passed the next snapshot time
        while(nextSnap < snapCount && elapsed >= snapTimes[nextSnap] - 1e-5f)
        {
            auto cats = classifyPlates();
            std::vector<PlateState> ps;
            ps.reserve(plates.size());
            for(size_t i=0;i<plates.size();++i)
                ps.push_back({plates[i].center, plates[i].axis,
                               plates[i].angularVelocity, cats[i]});

            result.snapshots.push_back(
                renderSnapshot(ps, elapsed, size, thickness, roughness, settings.seed, paletteSize));
            ++nextSnap;
        }
    }

    // Ensure we always have at least one snapshot (the final state)
    if(result.snapshots.empty())
    {
        auto cats=classifyPlates();
        std::vector<PlateState> ps;
        ps.reserve(plates.size());
        for(size_t i=0;i<plates.size();++i)
            ps.push_back({plates[i].center,plates[i].axis,plates[i].angularVelocity,cats[i]});
        result.snapshots.push_back(
            renderSnapshot(ps, elapsed, size, thickness, roughness, settings.seed, paletteSize));
    }

    // Fill result stats from the final snapshot
    {
        auto cats=classifyPlates();
        int conv=0,trans=0,div=0;
        for(int c:cats){if(c==0)conv++;else if(c==2)div++;else trans++;}
        result.finalPlateCount = int(plates.size());
        result.convergentCount = conv;
        result.transformCount  = trans;
        result.divergentCount  = div;
    }

    return result;
}
