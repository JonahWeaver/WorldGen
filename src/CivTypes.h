#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

// ─────────────────────────────────────────────────────────────────────────────
// CULTURAL ATTRIBUTES
// Each country has a vector of cultural attributes that drift over time.
// ─────────────────────────────────────────────────────────────────────────────

enum class CulturalAttr : int
{
    Militarism   = 0,  // tendency to wage war / expand by force
    Mercantilism = 1,  // trade focus, wealth accumulation
    Piety        = 2,  // religious cohesion, resistance to foreign culture
    Scholarship  = 3,  // science generation bonus
    Isolationism = 4,  // resistance to cultural spread in/out
    Count        = 5
};

static constexpr int CULT_ATTR_COUNT = static_cast<int>(CulturalAttr::Count);

// ─────────────────────────────────────────────────────────────────────────────
// RESOURCES
// ─────────────────────────────────────────────────────────────────────────────

enum class ResourceType : int
{
    Iron      = 0,
    Wood      = 1,
    Niter     = 2,
    ManaStone = 3,
    Gold      = 4,
    Silver    = 5,
    Copper    = 6,
    Count     = 7
};

static constexpr int RESOURCE_COUNT = static_cast<int>(ResourceType::Count);

static const char* resourceName(ResourceType r)
{
    switch(r){
        case ResourceType::Iron:      return "Iron";
        case ResourceType::Wood:      return "Wood";
        case ResourceType::Niter:     return "Niter";
        case ResourceType::ManaStone: return "Mana Stone";
        case ResourceType::Gold:      return "Gold";
        case ResourceType::Silver:    return "Silver";
        case ResourceType::Copper:    return "Copper";
        default:                      return "Unknown";
    }
}

// Per-cell resource deposit (0 = none, >0 = deposit size)
struct ResourceDeposit
{
    ResourceType type  = ResourceType::Iron;
    float        amount = 0.f;  // [0,1] relative richness
};

// ─────────────────────────────────────────────────────────────────────────────
// RESEARCH TREE  (Civ5-style)
// ─────────────────────────────────────────────────────────────────────────────

enum class TechId : int
{
    // Tier 0 — starting techs
    Agriculture   = 0,
    Mining        = 1,
    Sailing       = 2,

    // Tier 1
    IronWorking   = 3,   // requires Mining
    Writing       = 4,   // requires Agriculture
    Masonry       = 5,   // requires Mining
    Navigation    = 6,   // requires Sailing

    // Tier 2
    Mathematics   = 7,   // requires Writing
    Metallurgy    = 8,   // requires IronWorking
    Engineering   = 9,   // requires Masonry + Mathematics
    Cartography   = 10,  // requires Navigation + Writing

    // Tier 3
    Gunpowder     = 11,  // requires Metallurgy + Niter resource
    Printing      = 12,  // requires Mathematics
    Arcane        = 13,  // requires ManaStone resource + Mathematics
    Astronomy     = 14,  // requires Cartography + Mathematics

    Count         = 15
};

static constexpr int TECH_COUNT = static_cast<int>(TechId::Count);

static const char* techName(TechId t)
{
    switch(t){
        case TechId::Agriculture: return "Agriculture";
        case TechId::Mining:      return "Mining";
        case TechId::Sailing:     return "Sailing";
        case TechId::IronWorking: return "Iron Working";
        case TechId::Writing:     return "Writing";
        case TechId::Masonry:     return "Masonry";
        case TechId::Navigation:  return "Navigation";
        case TechId::Mathematics: return "Mathematics";
        case TechId::Metallurgy:  return "Metallurgy";
        case TechId::Engineering: return "Engineering";
        case TechId::Cartography: return "Cartography";
        case TechId::Gunpowder:   return "Gunpowder";
        case TechId::Printing:    return "Printing";
        case TechId::Arcane:      return "Arcane Arts";
        case TechId::Astronomy:   return "Astronomy";
        default:                  return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// EVENT LOG
// ─────────────────────────────────────────────────────────────────────────────

enum class EventType : int
{
    Founded      = 0,  // country founded / settlement established
    Conquest     = 1,  // country conquered a region
    Secession    = 2,  // region broke away
    Merger       = 3,  // two countries merged
    TechUnlocked = 4,  // technology researched
    CityFounded  = 5,  // city reached city-level population
    Collapse     = 6,  // country lost all territory
};

struct CivEvent
{
    int       tick       = 0;
    EventType type       = EventType::Founded;
    int       countryId  = -1;   // primary actor
    int       countryId2 = -1;   // secondary actor (conquest target, merger partner)
    int       techId     = -1;   // for TechUnlocked events
    int       cellX      = -1;   // map location (if relevant)
    int       cellY      = -1;
    std::string note;            // human-readable description
};

// ─────────────────────────────────────────────────────────────────────────────
// PER-CELL CIVILIZATION DATA
// ─────────────────────────────────────────────────────────────────────────────

struct CivCell
{
    // Geography scores (derived from geology, fixed after init)
    float habitability   = 0.f;
    float fertility      = 0.f;
    float traversability = 0.f;  // land traversability [0,1]
    float seaAccess      = 0.f;  // proximity to ocean [0,1] — enables sea trade/movement
    bool  isOcean        = false;

    // Dynamic simulation state
    float population     = 0.f;
    float culture        = 0.f;  // cultural strength [0,1]
    float cohesion       = 1.f;  // political cohesion [0,1]
    int   cultureId      = -1;
    int   countryId      = -1;
    bool  settled        = false;

    // Resources present in this cell (sparse — most cells have none)
    // Index matches ResourceType enum
    float resources[RESOURCE_COUNT] = {};

    // Effective traversability including sea routes (computed each tick)
    float effectiveTraversability = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
// PER-COUNTRY STATE  (live, updated each tick)
// ─────────────────────────────────────────────────────────────────────────────

struct CountryState
{
    int   countryId  = -1;
    int   cultureId  = -1;
    bool  alive      = false;

    // Cultural attributes [0,1] each
    std::array<float, CULT_ATTR_COUNT> culturalAttrs = {};

    // Resource stockpiles
    std::array<float, RESOURCE_COUNT> stockpile = {};

    // Research
    std::array<bool,  TECH_COUNT>     techs      = {};  // which techs are unlocked
    float scienceAccum = 0.f;   // accumulated science points
    int   currentResearch = -1; // TechId being researched (-1 = none)

    // Per-tick stats
    float income      = 0.f;   // gold income this tick
    float scienceRate = 0.f;   // science generated this tick
    float tradeVolume = 0.f;   // total trade with neighbours this tick
    float militaryStr = 0.f;   // military strength (pop × militarism × tech bonus)

    // Cohesion bonus from technology (Navigation, Engineering, etc.)
    float techCohesionBonus = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
// AGGREGATED PER-COUNTRY INFO  (for UI display, computed at snapshot time)
// ─────────────────────────────────────────────────────────────────────────────

struct CivInfo
{
    int   countryId    = -1;
    int   cultureId    = -1;
    float totalPop     = 0.f;
    int   cellCount    = 0;
    float avgFertility = 0.f;
    float avgCohesion  = 0.f;
    float avgCulture   = 0.f;
    float avgTraversability = 0.f;
    int   cityCount    = 0;
    int   townCount    = 0;
    int   villageCount = 0;
    uint32_t colour    = 0;

    // Extended stats
    std::array<float, RESOURCE_COUNT>  totalResources = {};
    std::array<float, CULT_ATTR_COUNT> culturalAttrs  = {};
    std::array<bool,  TECH_COUNT>      techs          = {};
    float scienceRate  = 0.f;
    float tradeVolume  = 0.f;
    float income       = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
// CIVILIZATION SNAPSHOT  (one point in time)
// ─────────────────────────────────────────────────────────────────────────────

struct CivSnapshot
{
    int   tick          = 0;
    float totalPop      = 0.f;
    int   settledCells  = 0;
    int   cultureGroups = 0;
    int   countryCount  = 0;

    std::vector<uint32_t> pixels;    // globe equirectangular
    std::vector<uint32_t> mercator;  // Mercator projection

    // Events that occurred up to this tick
    std::vector<CivEvent> events;
};
