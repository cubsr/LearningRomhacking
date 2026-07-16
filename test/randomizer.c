#include "global.h"
#include "item.h"
#include "malloc.h"
#include "move.h"
#include "pokemon.h"
#include "randomization.h"
#include "script_pokemon_util.h"
#include "test/test.h"
#include "constants/abilities.h"
#include "constants/item.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/species.h"

static void EnableAllRandomization(void)
{
    gSaveBlock2Ptr->randomizationSeed = 0xC0FFEE42;
    SetRandomizationFlags(RANDOMIZATION_FLAG_WILD | RANDOMIZATION_FLAG_STATIC | RANDOMIZATION_FLAG_TRADES
                        | RANDOMIZATION_FLAG_TRAINERS | RANDOMIZATION_FLAG_ABILITIES | RANDOMIZATION_FLAG_MOVES
                        | RANDOMIZATION_FLAG_ITEMS, TRUE);
}

TEST("Randomizer: species remaps are valid, enabled, and deterministic")
{
    u32 species;
    EnableAllRandomization();
    for (species = 1; species < NUM_SPECIES; species += 13)
    {
        u16 out = GetRandomizedWildSpecies(species, 21, 1);
        EXPECT_NE(out, SPECIES_NONE);
        EXPECT_LT(out, NUM_SPECIES);
        EXPECT(IsSpeciesEnabled(out));
        EXPECT_EQ(out, GetRandomizedWildSpecies(species, 21, 1));
    }
}

TEST("Randomizer: move remaps are valid for every move")
{
    u32 move;
    EnableAllRandomization();
    for (move = 1; move < MOVES_COUNT; move++)
    {
        u16 out = GetRandomizedMove(move, SPECIES_TREECKO);
        EXPECT_NE(out, MOVE_NONE);
        EXPECT_LT(out, MOVES_COUNT);
        if (move != MOVE_STRUGGLE) // Struggle isn't learnable; it passes through unchanged
            EXPECT_NE(out, MOVE_STRUGGLE);
    }
}

TEST("Randomizer: move type buckets stay in range")
{
    u32 move;
    for (move = 1; move < MOVES_COUNT; move++)
        EXPECT_LT(GetMoveType(move), NUMBER_OF_MON_TYPES);
}

TEST("Randomizer: ability remaps are valid")
{
    u32 species;
    EnableAllRandomization();
    for (species = 1; species < NUM_SPECIES; species += 17)
    {
        u16 out = GetAbilityBySpecies(species, 0);
        EXPECT_LT(out, ABILITIES_COUNT);
    }
}

TEST("Randomizer: item remaps never produce key items or TMs")
{
    u32 item;
    EnableAllRandomization();
    for (item = 1; item < ITEMS_COUNT; item += 3)
    {
        u16 out = GetRandomizedItem(item, 5, 9);
        enum Pocket inPocket = GetItemPocket(item);
        EXPECT_NE(out, ITEM_NONE);
        EXPECT_LT(out, ITEMS_COUNT);
        if (inPocket == POCKET_KEY_ITEMS || inPocket == POCKET_TM_HM)
            EXPECT_EQ(out, item);
        else
        {
            enum Pocket outPocket = GetItemPocket(out);
            EXPECT_NE(outPocket, POCKET_KEY_ITEMS);
            EXPECT_NE(outPocket, POCKET_TM_HM);
        }
    }
}

TEST("Randomizer: starter give flow survives with all randomization on")
{
    u32 i;
    EnableAllRandomization();
    ZeroPlayerPartyMons();
    ScriptGiveMon(SPECIES_TREECKO, 5, ITEM_NONE);
    EXPECT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_SPECIES) != SPECIES_NONE, TRUE);
    for (i = 0; i < MAX_MON_MOVES; i++)
        EXPECT_LT(GetMonData(&gPlayerParty[0], MON_DATA_MOVE1 + i), MOVES_COUNT);
}

TEST("Randomizer: scripted wild mon creation survives with all randomization on")
{
    EnableAllRandomization();
    CreateScriptedWildMon(SPECIES_ZIGZAGOON, 2, ITEM_NONE);
    EXPECT_EQ(GetMonData(&gParties[B_TRAINER_OPPONENT_A][0], MON_DATA_SPECIES) != SPECIES_NONE, TRUE);
}
