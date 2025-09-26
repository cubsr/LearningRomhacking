#include "global.h"
#include "battle.h"
#include "event_data.h"
#include "caps.h"
#include "pokemon.h"
#include "string_util.h"
#include "menu_helpers.h"
#include "strings.h"


u32 GetCurrentLevelCap(void)
{
    static const u32 sLevelCapFlagMap[][2] =
    {
        // Custom level cap milestones based on story progression
        {TRAINER_FLAGS_START + TRAINER_GRUNT_PETALBURG_WOODS, 12},  // Route 104 Aqua Grunt
        {TRAINER_FLAGS_START + TRAINER_GRUNT_MUSEUM_1, 17},         // Museum Aqua Grunts
        {FLAG_DEFEATED_DEWFORD_GYM, 21},                            // Leader Brawly
        {FLAG_DEFEATED_RUSTBORO_GYM, 25},                           // Leader Roxanne
        {TRAINER_FLAGS_START + TRAINER_LEAH, 32},                   // Route 117 Chelle
        {FLAG_DEFEATED_MAUVILLE_GYM, 35},                           // Leader Wattson
        {FLAG_DEFEATED_RIVAL_ROUTE103, 38},                         // Cycling Road Rival
        {FLAG_DEFEATED_PETALBURG_GYM, 42},                          // Leader Norman
        {TRAINER_FLAGS_START + TRAINER_FELIX, 48},                  // Fallarbor Town Vito
        {TRAINER_FLAGS_START + TRAINER_ARCHIE, 54},                 // Mt. Chimney Maxie
        {FLAG_DEFEATED_LAVARIDGE_GYM, 57},                          // Leader Flannery
        {TRAINER_FLAGS_START + TRAINER_SHELLY_WEATHER_INSTITUTE, 65}, // Weather Institute Shelly
        {FLAG_DEFEATED_RIVAL_ROUTE_104, 66},                        // Route 119 Rival
        {FLAG_DEFEATED_FORTREE_GYM, 69},                            // Leader Winona
        {FLAG_DEFEATED_RIVAL_RUSTBORO, 73},                         // Lilycove City Rival
        {TRAINER_FLAGS_START + TRAINER_GRUNT_MT_PYRE_1, 76},        // Mt. Pyre Archie
        {TRAINER_FLAGS_START + TRAINER_GRUNT_AQUA_HIDEOUT_1, 79},   // Magma Hideout Maxie
        {TRAINER_FLAGS_START + TRAINER_MATT, 81},                   // Aqua Hideout Matt
        {FLAG_DEFEATED_MOSSDEEP_GYM, 85},                           // Leaders Tate & Liza
        {TRAINER_FLAGS_START + TRAINER_SHELLY_SEAFLOOR_CAVERN, 89}, // Seafloor Cavern Archie
        {FLAG_DEFEATED_SOOTOPOLIS_GYM, 91},                         // Leader Juan
        {TRAINER_FLAGS_START + TRAINER_VIOLET, 95},                 // Victory Road Vito
        {FLAG_IS_CHAMPION, 100},                                    // After Victory Road Vito
    };

    u32 i;

    if (B_LEVEL_CAP_TYPE == LEVEL_CAP_FLAG_LIST)
    {
        for (i = 0; i < ARRAY_COUNT(sLevelCapFlagMap); i++)
        {
            if (!FlagGet(sLevelCapFlagMap[i][0]))
                return sLevelCapFlagMap[i][1];
        }
    }
    else if (B_LEVEL_CAP_TYPE == LEVEL_CAP_VARIABLE)
    {
        return VarGet(B_LEVEL_CAP_VARIABLE);
    }

    return MAX_LEVEL;
}

u32 GetSoftLevelCapExpValue(u32 level, u32 expValue)
{
    static const u32 sExpScalingDown[5] = { 4, 8, 16, 32, 64 };
    static const u32 sExpScalingUp[5]   = { 16, 8, 4, 2, 1 };

    u32 levelDifference;
    u32 currentLevelCap = GetCurrentLevelCap();

    if (B_EXP_CAP_TYPE == EXP_CAP_NONE)
        return expValue;

    if (level < currentLevelCap)
    {
        if (B_LEVEL_CAP_EXP_UP)
        {
            levelDifference = currentLevelCap - level;
            if (levelDifference > ARRAY_COUNT(sExpScalingUp) - 1)
                return expValue + (expValue / sExpScalingUp[ARRAY_COUNT(sExpScalingUp) - 1]);
            else
                return expValue + (expValue / sExpScalingUp[levelDifference]);
        }
        else
        {
            return expValue;
        }
    }
    else if (B_EXP_CAP_TYPE == EXP_CAP_HARD)
    {
        return 0;
    }
    else if (B_EXP_CAP_TYPE == EXP_CAP_SOFT)
    {
        levelDifference = level - currentLevelCap;
        if (levelDifference > ARRAY_COUNT(sExpScalingDown) - 1)
            return expValue / sExpScalingDown[ARRAY_COUNT(sExpScalingDown) - 1];
        else
            return expValue / sExpScalingDown[levelDifference];
    }
    else
    {
       return expValue;
    }
}

u32 GetCurrentEVCap(void)
{
    static const u16 sEvCapFlagMap[][2] = {
        // Define EV caps for each milestone
        {FLAG_BADGE01_GET, MAX_TOTAL_EVS *  1 / 17},
        {FLAG_BADGE02_GET, MAX_TOTAL_EVS *  3 / 17},
        {FLAG_BADGE03_GET, MAX_TOTAL_EVS *  5 / 17},
        {FLAG_BADGE04_GET, MAX_TOTAL_EVS *  7 / 17},
        {FLAG_BADGE05_GET, MAX_TOTAL_EVS *  9 / 17},
        {FLAG_BADGE06_GET, MAX_TOTAL_EVS * 11 / 17},
        {FLAG_BADGE07_GET, MAX_TOTAL_EVS * 13 / 17},
        {FLAG_BADGE08_GET, MAX_TOTAL_EVS * 15 / 17},
        {FLAG_IS_CHAMPION, MAX_TOTAL_EVS},
    };

    if (B_EV_CAP_TYPE == EV_CAP_FLAG_LIST)
    {
        for (u32 evCap = 0; evCap < ARRAY_COUNT(sEvCapFlagMap); evCap++)
        {
            if (!FlagGet(sEvCapFlagMap[evCap][0]))
                return sEvCapFlagMap[evCap][1];
        }
    }
    else if (B_EV_CAP_TYPE == EV_CAP_VARIABLE)
    {
        return VarGet(B_EV_CAP_VARIABLE);
    }
    else if (B_EV_CAP_TYPE == EV_CAP_NO_GAIN)
    {
        return 0;
    }

    return MAX_TOTAL_EVS;
}

// Check if level cap has increased and display message
void CheckAndDisplayLevelCapIncrease(void)
{
    static u32 sLastLevelCap = 0;
    u32 currentLevelCap = GetCurrentLevelCap();
    
    if (currentLevelCap > sLastLevelCap && sLastLevelCap > 0)
    {
        // Level cap has increased - prepare message for display
        ConvertIntToDecimalStringN(gStringVar1, currentLevelCap, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringExpandPlaceholders(gStringVar4, gText_LevelCapIncreased);
        // The message is now ready in gStringVar4 for any system that wants to display it
    }
    
    sLastLevelCap = currentLevelCap;
}
