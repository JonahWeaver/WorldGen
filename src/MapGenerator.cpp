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
    unsigned int seed)
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

    auto palette=buildPalette(int(plates.size()));

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

    // ── Pack into snapshot ────────────────────────────────────────────────────
    snap.layerPixels[0]=std::move(pix0);
    snap.layerPixels[1]=std::move(pix1);
    snap.layerPixels[2]=std::move(pix2);
    snap.layerPixels[3]=std::move(pix3);
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
            // Build PlateState list with current motion categories
            auto cats = classifyPlates();
            std::vector<PlateState> ps;
            ps.reserve(plates.size());
            for(size_t i=0;i<plates.size();++i)
                ps.push_back({plates[i].center, plates[i].axis,
                               plates[i].angularVelocity, cats[i]});

            result.snapshots.push_back(
                renderSnapshot(ps, elapsed, size, thickness, roughness, settings.seed));
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
        result.snapshots.push_back(renderSnapshot(ps,elapsed,size,thickness,roughness,settings.seed));
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
