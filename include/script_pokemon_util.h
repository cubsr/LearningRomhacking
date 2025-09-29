#ifndef GUARD_SCRIPT_POKEMON_UTIL_H
#define GUARD_SCRIPT_POKEMON_UTIL_H

u8 ScriptGiveMon(u16 species, u8 level, u16 item, u16 ball, u8 nature, u8 abilityNum, u8 gender,
                 u8 hpEv, u8 atkEv, u8 defEv, u8 speedEv, u8 spAtkEv, u8 spDefEv,
                 u8 hpIv, u8 atkIv, u8 defIv, u8 speedIv, u8 spAtkIv, u8 spDefIv,
                 u16 move1, u16 move2, u16 move3, u16 move4, bool8 isShiny, bool8 gmaxFactor, u8 teraType, u8 dmaxLevel);
u8 ScriptGiveEgg(u16 species);
void CreateScriptedWildMon(u16 species, u8 level, u16 item);
void CreateScriptedDoubleWildMon(u16 species, u8 level, u16 item, u16 species2, u8 level2, u16 item2);
void ScriptSetMonMoveSlot(u8 monIndex, u16 move, u8 slot);
void ReducePlayerPartyToSelectedMons(void);
void HealPlayerParty(void);
void Script_GetChosenMonOffensiveEVs(void);
void Script_GetChosenMonDefensiveEVs(void);
void Script_GetChosenMonOffensiveIVs(void);
void Script_GetChosenMonDefensiveIVs(void);

#endif // GUARD_SCRIPT_POKEMON_UTIL_H
