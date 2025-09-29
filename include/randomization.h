#ifndef GUARD_RANDOMIZATION_H
#define GUARD_RANDOMIZATION_H

#include "global.h"

// Randomization types
typedef enum {
    RANDOMIZATION_WILD,
    RANDOMIZATION_STATIC,
    RANDOMIZATION_TRADES
} RandomizationType;

#if RANDOMIZATION_ENABLED == TRUE

// Initialize randomization system with trainer ID as seed
void InitializeRandomization(u32 trainerId);

// Set randomization flags
void SetRandomizationFlags(u8 wild, u8 staticEncounters, u8 trades, u8 similarStats);

// Check if a randomization type is enabled
bool8 IsRandomizationEnabled(RandomizationType type);

// Get a deterministic random number for a specific location/context
u32 GetRandomizationSeed(u32 baseSeed, u32 locationId, u32 contextId);

// Get randomized species for wild encounters
u16 GetRandomizedWildSpecies(u16 originalSpecies, u32 locationId, u8 encounterType);

// Get randomized species for static encounters
u16 GetRandomizedStaticSpecies(u16 originalSpecies, u32 mapId, u32 objectId);

// Get randomized species for trades
u16 GetRandomizedTradeSpecies(u16 originalSpecies, u32 tradeId);

#endif // RANDOMIZATION_ENABLED

#endif // GUARD_RANDOMIZATION_H
