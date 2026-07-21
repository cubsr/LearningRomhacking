#ifndef GUARD_START_MENU_HUD_H
#define GUARD_START_MENU_HUD_H

// Icon-based start menu overlay: a column of action icons down the right edge,
// the party as mon icons along the bottom, and a Pokedex counter. Drawn over the
// live field while the start menu is open; see src/start_menu_hud.c.

void StartMenuHud_Show(const u8 *actions, u32 numActions, u32 cursorPos);
void StartMenuHud_Hide(void);
bool32 StartMenuHud_IsActive(void);

// Returns TRUE if the d-pad moved the cursor this frame.
bool32 StartMenuHud_HandleDpadInput(void);
bool32 StartMenuHud_IsOnParty(void);
u32 StartMenuHud_GetPartyPos(void);
u32 StartMenuHud_GetCursorPos(void);

#endif // GUARD_START_MENU_HUD_H
