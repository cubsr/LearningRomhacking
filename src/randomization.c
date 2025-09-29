#include "global.h"
#include "randomization.h"
#include "random.h"
#include "pokemon.h"
#include "constants/species.h"

// Randomization flag definitions
#define RANDOMIZATION_FLAG_WILD     (1 << 0)
#define RANDOMIZATION_FLAG_STATIC   (1 << 1)
#define RANDOMIZATION_FLAG_TRADES   (1 << 2)

#if RANDOMIZATION_ENABLED == TRUE

// Initialize randomization system with trainer ID as seed
void InitializeRandomization(u32 trainerId)
{
    gSaveBlock2Ptr->randomizationSeed = trainerId;
    gSaveBlock2Ptr->randomizationFlags = 0;
    gSaveBlock2Ptr->randomizationSimilarStats = RANDOMIZATION_DEFAULT_SIMILAR_STATS;
}

// Set randomization flags
void SetRandomizationFlags(u8 wild, u8 staticEncounters, u8 trades, u8 similarStats)
{
    gSaveBlock2Ptr->randomizationFlags = 0;
    if (wild)
        gSaveBlock2Ptr->randomizationFlags |= RANDOMIZATION_FLAG_WILD;
    if (staticEncounters)
        gSaveBlock2Ptr->randomizationFlags |= RANDOMIZATION_FLAG_STATIC;
    if (trades)
        gSaveBlock2Ptr->randomizationFlags |= RANDOMIZATION_FLAG_TRADES;
    
    gSaveBlock2Ptr->randomizationSimilarStats = similarStats;
}

// Check if a randomization type is enabled
bool8 IsRandomizationEnabled(RandomizationType type)
{
    switch (type)
    {
        case RANDOMIZATION_WILD:
            return (gSaveBlock2Ptr->randomizationFlags & RANDOMIZATION_FLAG_WILD) != 0;
        case RANDOMIZATION_STATIC:
            return (gSaveBlock2Ptr->randomizationFlags & RANDOMIZATION_FLAG_STATIC) != 0;
        case RANDOMIZATION_TRADES:
            return (gSaveBlock2Ptr->randomizationFlags & RANDOMIZATION_FLAG_TRADES) != 0;
        default:
            return FALSE;
    }
}

// Get a deterministic random number for a specific location/context
u32 GetRandomizationSeed(u32 baseSeed, u32 locationId, u32 contextId)
{
    return baseSeed ^ (locationId << 8) ^ (contextId << 16);
}

// Get randomized species for wild encounters
u16 GetRandomizedWildSpecies(u16 originalSpecies, u32 locationId, u8 encounterType)
{
    if (!IsRandomizationEnabled(RANDOMIZATION_WILD))
        return originalSpecies;
    
    u32 seed = GetRandomizationSeed(gSaveBlock2Ptr->randomizationSeed, locationId, encounterType);
    u32 randomValue = seed ^ (seed >> 11) ^ (seed >> 22);
    
    // Get a random species (excluding invalid species)
    u16 newSpecies = (randomValue % (NUM_SPECIES - 1)) + 1;
    
    // If similar stats is enabled, try to find a species with similar base stats
    if (gSaveBlock2Ptr->randomizationSimilarStats)
    {
        u16 originalBaseStats = GetMonData(&gPlayerParty[0], MON_DATA_BASE_STATS_TOTAL, NULL);
        u16 newBaseStats = GetMonData(&gPlayerParty[0], MON_DATA_BASE_STATS_TOTAL, NULL);
        
        // If the new species has very different stats, try again with a different random value
        if (abs(originalBaseStats - newBaseStats) > 101)
        {
            randomValue = randomValue ^ 0x12345678;
            newSpecies = (randomValue % (NUM_SPECIES - 1)) + 1;
        }
    }
    
    return newSpecies;
}

// Get randomized species for static encounters
u16 GetRandomizedStaticSpecies(u16 originalSpecies, u32 mapId, u32 objectId)
{
    if (!IsRandomizationEnabled(RANDOMIZATION_STATIC))
        return originalSpecies;
    
    u32 seed = GetRandomizationSeed(gSaveBlock2Ptr->randomizationSeed, mapId, objectId);
    u32 randomValue = seed ^ (seed >> 11) ^ (seed >> 22);
    
    // Get a random species (excluding invalid species)
    u16 newSpecies = (randomValue % (NUM_SPECIES - 1)) + 1;
    
    // If similar stats is enabled, try to find a species with similar base stats
    if (gSaveBlock2Ptr->randomizationSimilarStats)
    {
        u16 originalBaseStats = GetMonData(&gPlayerParty[0], MON_DATA_BASE_STATS_TOTAL, NULL);
        u16 newBaseStats = GetMonData(&gPlayerParty[0], MON_DATA_BASE_STATS_TOTAL, NULL);
        
        // If the new species has very different stats, try again with a different random value
        if (abs(originalBaseStats - newBaseStats) > 100)
        {
            randomValue = randomValue ^ 0x12345678;
            newSpecies = (randomValue % (NUM_SPECIES - 1)) + 1;
        }
    }
    
    return newSpecies;
}

// Get randomized species for trades
u16 GetRandomizedTradeSpecies(u16 originalSpecies, u32 tradeId)
{
    if (!IsRandomizationEnabled(RANDOMIZATION_TRADES))
        return originalSpecies;
    
    u32 seed = GetRandomizationSeed(gSaveBlock2Ptr->randomizationSeed, tradeId, 0);
    u32 randomValue = seed ^ (seed >> 11) ^ (seed >> 22);
    
    // Get a random species (excluding invalid species)
    u16 newSpecies = (randomValue % (NUM_SPECIES - 1)) + 1;
    
    // If similar stats is enabled, try to find a species with similar base stats
    if (gSaveBlock2Ptr->randomizationSimilarStats)
    {
        u16 originalBaseStats = GetMonData(&gPlayerParty[0], MON_DATA_BASE_STATS_TOTAL, NULL);
        u16 newBaseStats = GetMonData(&gPlayerParty[0], MON_DATA_BASE_STATS_TOTAL, NULL);
        
        // If the new species has very different stats, try again with a different random value
        if (abs(originalBaseStats - newBaseStats) > 100)
        {
            randomValue = randomValue ^ 0x12345678;
            newSpecies = (randomValue % (NUM_SPECIES - 1)) + 1;
        }
    }
    
    return newSpecies;
}

#endif // RANDOMIZATION_ENABLED
