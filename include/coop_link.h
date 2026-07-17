#ifndef GUARD_COOP_LINK_H
#define GUARD_COOP_LINK_H

#include "global.h"

// Logical 2-player co-op session over the wired link (mGBA multiplayer
// windows). The session is independent of the physical link: battles may
// close and reopen the hardware link without ending the session.

enum CoopSessionState
{
    COOP_STATE_IDLE,
    COOP_STATE_LINKING,   // linkup task running
    COOP_STATE_ACTIVE,    // heartbeats flowing
    COOP_STATE_ERROR,     // link died; session over
};

// What the local/remote player is currently doing (sent in presence packets)
enum CoopActivity
{
    COOP_ACTIVITY_ROAMING,
    COOP_ACTIVITY_BATTLE,
    COOP_ACTIVITY_MENU,
};

struct CoopPartnerStatus
{
    bool8 valid;
    u8 activity;
    u8 facing;
    u8 mapGroup;
    u8 mapNum;
    u8 gender;
    s16 x;
    s16 y;
    u16 lastSeq;
    u16 framesSinceUpdate;
    u32 seed;       // partner's randomizationSeed (for half-and-half teams)
    u8 randoFlags;
};

void Coop_StartSession(void);
void Coop_EndSession(void);
bool32 CoopSession_IsActive(void);
u8 Coop_GetSessionState(void);
bool32 Coop_IsHost(void);
const struct CoopPartnerStatus *Coop_GetPartnerStatus(void);

// Hooks called from link.c
void CoopLink_BuildCmd(void);                          // LinkMain2, once per frame
void CoopLink_HandleRecvCmd(const u16 *cmd, u32 playerId); // ProcessRecvCmds
bool32 Coop_OnLinkError(void);                         // TrySetLinkErrorBuffer; TRUE = suppress error screen

#endif // GUARD_COOP_LINK_H
