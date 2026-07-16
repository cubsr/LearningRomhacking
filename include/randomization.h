#ifndef GUARD_RANDOMIZATION_H
#define GUARD_RANDOMIZATION_H

#include "global.h"

// Randomization flag bits (stored in gSaveBlock2Ptr->randomizationFlags)
#define RANDOMIZATION_FLAG_WILD      (1 << 0)
#define RANDOMIZATION_FLAG_STATIC    (1 << 1)
#define RANDOMIZATION_FLAG_TRADES    (1 << 2)
#define RANDOMIZATION_FLAG_TRAINERS  (1 << 3)
#define RANDOMIZATION_FLAG_ABILITIES (1 << 4)
#define RANDOMIZATION_FLAG_MOVES     (1 << 5)
#define RANDOMIZATION_FLAG_ITEMS     (1 << 6)

// Randomization types
typedef enum {
    RANDOMIZATION_WILD,
    RANDOMIZATION_STATIC,
    RANDOMIZATION_TRADES,
    RANDOMIZATION_TRAINERS,
    RANDOMIZATION_ABILITIES,
    RANDOMIZATION_MOVES,
    RANDOMIZATION_ITEMS
} RandomizationType;

#if RANDOMIZATION_ENABLED == TRUE

// Initialize randomization system with trainer ID as seed
void InitializeRandomization(u32 trainerId);

// Set randomization flags (mask of RANDOMIZATION_FLAG_*)
void SetRandomizationFlags(u8 flagsMask, u8 similarStats);

// Check if a randomization type is enabled
bool8 IsRandomizationEnabled(RandomizationType type);

// Deterministic 32-bit hash used by all randomization
u32 RandomizerHash(u32 x);

// Seed-explicit species remap (co-op uses this with the partner's seed)
u16 GetRandomizedSpeciesWithSeed(u32 seed, u16 originalSpecies, u32 key1, u32 key2);

// Get randomized species for wild encounters
u16 GetRandomizedWildSpecies(u16 originalSpecies, u32 locationId, u8 encounterType);

// Get randomized species for static encounters
u16 GetRandomizedStaticSpecies(u16 originalSpecies, u32 mapId, u32 objectId);

// Get randomized species for trades
u16 GetRandomizedTradeSpecies(u16 originalSpecies, u32 tradeId);

// Get randomized species for a trainer's party slot (seed-explicit for co-op)
u16 GetRandomizedTrainerMonSpecies(u32 seed, u16 originalSpecies, u16 trainerId, u8 slot);

// Ability/move/item remaps (no-ops when their toggles are off)
u16 GetRandomizedAbility(u16 ability, u16 species, u8 abilityNum);
u16 GetRandomizedMove(u16 move, u16 species);
u16 GetRandomizedItem(u16 item, u32 key1, u32 key2);

// Nonzero = use this seed for ability/move remaps (co-op battle sync); 0 = own save seed
void SetRandomizationSeedOverride(u32 seed);

#endif // RANDOMIZATION_ENABLED

#endif // GUARD_RANDOMIZATION_H
