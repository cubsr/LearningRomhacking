#include "global.h"
#include "randomization.h"
#include "event_data.h"
#include "item.h"
#include "move.h"
#include "pokemon.h"
#include "constants/abilities.h"
#include "constants/item.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/species.h"

#if RANDOMIZATION_ENABLED == TRUE

// Choices made on the pre-game randomizer menu, held until the new save
// is created (InitializeRandomization runs during NewGameInitData).
static bool8 sPendingChoicesValid;
static u8 sPendingFlags;
static u8 sPendingSimilarStats;

void Randomizer_SetPendingChoices(u8 flagsMask, u8 similarStats)
{
    sPendingChoicesValid = TRUE;
    sPendingFlags = flagsMask;
    sPendingSimilarStats = similarStats;
}

void InitializeRandomization(u32 trainerId)
{
    gSaveBlock2Ptr->randomizationSeed = trainerId;
    gSaveBlock2Ptr->randomizationFlags = 0;
    gSaveBlock2Ptr->randomizationSimilarStats = RANDOMIZATION_DEFAULT_SIMILAR_STATS;

    if (sPendingChoicesValid)
    {
        gSaveBlock2Ptr->randomizationFlags = sPendingFlags;
        gSaveBlock2Ptr->randomizationSimilarStats = sPendingSimilarStats;
        sPendingChoicesValid = FALSE;
    }
}

void SetRandomizationFlags(u8 flagsMask, u8 similarStats)
{
    gSaveBlock2Ptr->randomizationFlags = flagsMask;
    gSaveBlock2Ptr->randomizationSimilarStats = similarStats;
}

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
        case RANDOMIZATION_TRAINERS:
            return (gSaveBlock2Ptr->randomizationFlags & RANDOMIZATION_FLAG_TRAINERS) != 0;
        case RANDOMIZATION_ABILITIES:
            return (gSaveBlock2Ptr->randomizationFlags & RANDOMIZATION_FLAG_ABILITIES) != 0;
        case RANDOMIZATION_MOVES:
            return (gSaveBlock2Ptr->randomizationFlags & RANDOMIZATION_FLAG_MOVES) != 0;
        case RANDOMIZATION_ITEMS:
            return (gSaveBlock2Ptr->randomizationFlags & RANDOMIZATION_FLAG_ITEMS) != 0;
        default:
            return FALSE;
    }
}

// 32-bit integer finalizer (splitmix-style). All randomization derives from
// hashing (seed, keys) so results are stable for a save and reproducible on
// another game that knows the seed (needed for co-op half-teams).
u32 RandomizerHash(u32 x)
{
    x ^= x >> 16;
    x *= 0x7FEB352D;
    x ^= x >> 15;
    x *= 0x846CA68B;
    x ^= x >> 16;
    return x;
}

static u32 RandomizerKey(u32 seed, u32 a, u32 b, u32 c)
{
    u32 h = RandomizerHash(seed ^ 0x9E3779B9);
    h = RandomizerHash(h ^ a);
    h = RandomizerHash(h ^ b);
    h = RandomizerHash(h ^ c);
    return h;
}

static u16 GetSpeciesBST(u16 species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];
    return info->baseHP + info->baseAttack + info->baseDefense
         + info->baseSpeed + info->baseSpAttack + info->baseSpDefense;
}

static bool32 IsRandomSpeciesAllowed(u16 species)
{
    const struct SpeciesInfo *info;

    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return FALSE;
    if (!IsSpeciesEnabled(species))
        return FALSE;
    info = &gSpeciesInfo[species];
    // Battle-only or scripted forms would break as permanent overworld mons.
    if (info->isTotem || info->isMegaEvolution || info->isPrimalReversion
     || info->isUltraBurst || info->isGigantamax || info->isTeraForm)
        return FALSE;
    return TRUE;
}

// Pick a species from a hash, optionally re-rolling toward one whose base
// stat total is close to the original's. The window widens per attempt so
// this always terminates with something reasonable.
static u16 PickRandomSpecies(u32 hash, u16 originalSpecies, bool32 similarStats)
{
    u32 attempt;
    u16 fallback = SPECIES_NONE;
    s32 originalBst = GetSpeciesBST(originalSpecies);

    for (attempt = 0; attempt < 24; attempt++)
    {
        u16 candidate = (hash % (NUM_SPECIES - 1)) + 1;
        if (IsRandomSpeciesAllowed(candidate))
        {
            if (fallback == SPECIES_NONE)
                fallback = candidate;
            if (!similarStats || originalBst == 0)
                return candidate;
            if (abs(GetSpeciesBST(candidate) - originalBst) <= 100 + (s32)attempt * 15)
                return candidate;
        }
        hash = RandomizerHash(hash + attempt + 1);
    }
    return fallback != SPECIES_NONE ? fallback : originalSpecies;
}

// Seed-explicit core, used directly for co-op partner-seeded generation.
u16 GetRandomizedSpeciesWithSeed(u32 seed, u16 originalSpecies, u32 key1, u32 key2)
{
    u32 hash = RandomizerKey(seed, originalSpecies, key1, key2);
    return PickRandomSpecies(hash, originalSpecies, gSaveBlock2Ptr->randomizationSimilarStats);
}

u16 GetRandomizedWildSpecies(u16 originalSpecies, u32 locationId, u8 encounterType)
{
    if (!IsRandomizationEnabled(RANDOMIZATION_WILD))
        return originalSpecies;
    return GetRandomizedSpeciesWithSeed(gSaveBlock2Ptr->randomizationSeed,
                                        originalSpecies, locationId, encounterType);
}

u16 GetRandomizedStaticSpecies(u16 originalSpecies, u32 mapId, u32 objectId)
{
    if (!IsRandomizationEnabled(RANDOMIZATION_STATIC))
        return originalSpecies;
    return GetRandomizedSpeciesWithSeed(gSaveBlock2Ptr->randomizationSeed,
                                        originalSpecies, mapId, objectId | 0x10000);
}

u16 GetRandomizedTradeSpecies(u16 originalSpecies, u32 tradeId)
{
    if (!IsRandomizationEnabled(RANDOMIZATION_TRADES))
        return originalSpecies;
    return GetRandomizedSpeciesWithSeed(gSaveBlock2Ptr->randomizationSeed,
                                        originalSpecies, tradeId, 0x20000);
}

u16 GetRandomizedTrainerMonSpecies(u32 seed, u16 originalSpecies, u16 trainerId, u8 slot)
{
    return GetRandomizedSpeciesWithSeed(seed, originalSpecies,
                                        (u32)trainerId | 0x30000, slot);
}

// While nonzero, ability/move remaps use this seed instead of the save's.
// Co-op battles set it (to the host's seed) on both games so the linked
// battle engines resolve identical abilities/moves and stay in sync.
static u32 sSeedOverride;

void SetRandomizationSeedOverride(u32 seed)
{
    sSeedOverride = seed;
}

static u32 GetActiveSeed(void)
{
    return sSeedOverride != 0 ? sSeedOverride : gSaveBlock2Ptr->randomizationSeed;
}

u16 GetRandomizedAbility(u16 ability, u16 species, u8 abilityNum)
{
    u32 hash;

    if (ability == ABILITY_NONE || !IsRandomizationEnabled(RANDOMIZATION_ABILITIES))
        return ability;
    hash = RandomizerKey(GetActiveSeed(), species, abilityNum | 0x40000, ability);
    return (hash % (ABILITIES_COUNT - 1)) + 1;
}

// Learnable-move pools bucketed by type, built once on first use.
// EWRAM_DATA is load-bearing: plain statics go to IWRAM, and ~2KB there
// starves the stack (the smol decompressor runs its inner loop from
// on-stack buffers), overflowing into IWRAM globals.
static EWRAM_DATA u16 sMovesByType[MOVES_COUNT] = {0};
static EWRAM_DATA u16 sTypeStart[NUMBER_OF_MON_TYPES + 1] = {0};
static EWRAM_DATA bool8 sMovePoolsInitialized = FALSE;

static bool32 IsRandomMoveAllowed(u16 move)
{
    return move != MOVE_NONE && move != MOVE_STRUGGLE && move < MOVES_COUNT;
}

static void InitMovePoolsIfNeeded(void)
{
    u32 move, type;
    u16 counts[NUMBER_OF_MON_TYPES] = {0};
    u16 fill[NUMBER_OF_MON_TYPES];

    if (sMovePoolsInitialized)
        return;

    for (move = 1; move < MOVES_COUNT; move++)
    {
        if (IsRandomMoveAllowed(move))
            counts[GetMoveType(move)]++;
    }
    sTypeStart[0] = 0;
    for (type = 0; type < NUMBER_OF_MON_TYPES; type++)
    {
        sTypeStart[type + 1] = sTypeStart[type] + counts[type];
        fill[type] = sTypeStart[type];
    }
    for (move = 1; move < MOVES_COUNT; move++)
    {
        if (IsRandomMoveAllowed(move))
            sMovesByType[fill[GetMoveType(move)]++] = move;
    }
    sMovePoolsInitialized = TRUE;
}

// Remap a level-up move. ~70% of results share a type with the species.
u16 GetRandomizedMove(u16 move, u16 species)
{
    u32 hash, pick;

    if (!IsRandomMoveAllowed(move) || !IsRandomizationEnabled(RANDOMIZATION_MOVES))
        return move;

    InitMovePoolsIfNeeded();
    hash = RandomizerKey(GetActiveSeed(), species, move | 0x50000, 0);
    pick = RandomizerHash(hash);

    if (hash % 100 < 70)
    {
        u8 type1 = gSpeciesInfo[species].types[0];
        u8 type2 = gSpeciesInfo[species].types[1];
        u32 count1 = sTypeStart[type1 + 1] - sTypeStart[type1];
        u32 count2 = (type2 != type1) ? sTypeStart[type2 + 1] - sTypeStart[type2] : 0;
        u32 total = count1 + count2;

        if (total != 0)
        {
            u32 idx = pick % total;
            if (idx < count1)
                return sMovesByType[sTypeStart[type1] + idx];
            else
                return sMovesByType[sTypeStart[type2] + idx - count1];
        }
    }

    move = (pick % (MOVES_COUNT - 1)) + 1;
    if (!IsRandomMoveAllowed(move))
        move = MOVE_POUND;
    return move;
}

// Remap a picked-up item. Progression-relevant items (key items, TMs/HMs)
// are never replaced, and never appear as replacements.
u16 GetRandomizedItem(u16 item, u32 key1, u32 key2)
{
    u32 hash, attempt;
    enum Pocket pocket;

    if (item == ITEM_NONE || item >= ITEMS_COUNT || !IsRandomizationEnabled(RANDOMIZATION_ITEMS))
        return item;
    pocket = GetItemPocket(item);
    if (pocket == POCKET_KEY_ITEMS || pocket == POCKET_TM_HM)
        return item;

    hash = RandomizerKey(gSaveBlock2Ptr->randomizationSeed, item | 0x60000, key1, key2);
    for (attempt = 0; attempt < 24; attempt++)
    {
        u16 candidate = (hash % (ITEMS_COUNT - 1)) + 1;
        pocket = GetItemPocket(candidate);
        if (pocket != POCKET_KEY_ITEMS && pocket != POCKET_TM_HM && candidate != ITEM_LEVEL_UP)
            return candidate;
        hash = RandomizerHash(hash + attempt + 1);
    }
    return item;
}

#endif // RANDOMIZATION_ENABLED
