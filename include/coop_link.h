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
void CoopLink_FrameUpdate(void); // main loop, once per frame
bool32 Coop_OnLinkError(void);                         // TrySetLinkErrorBuffer; TRUE = suppress error screen

// Battle negotiation / co-op battle support
void Coop_QueueCommand(u16 op, u16 a, u16 b);
bool32 Coop_TakeIncomingBattleReq(u16 *trainerId);
u32 Coop_GetBattleReqAnswer(void); // 0 pending, 1 accepted, 2 declined
void Coop_ClearBattleNegotiation(void);
void Coop_SetInCoopBattle(bool32 inBattle);
bool32 Coop_InCoopBattle(void);
u32 Coop_GetTrainerSlotSeed(u32 slot, u32 monsCount);
u32 Coop_GetHostSeed(void);
void Coop_StartReestablish(void);
void Coop_SessionFailed(void);
bool32 Coop_ToleratesChecksumErrors(void);
void Coop_NoteChecksumError(void);

// coop_battle.c
bool32 Coop_TryStartCoopTrainerBattle(void); // from BattleSetup_StartTrainerBattle
void Coop_CheckIncomingBattleReq(void);      // from CoopOverworld_Update
void Coop_OnTrainerBattleEnd(void);          // from CB2_EndTrainerBattle / guest return

// coop_overworld.c: partner avatar, called from CB1_Overworld
void CoopOverworld_Update(void);

// overworld.c: avatar rendering primitives (coords are saveblock map coords)
bool32 CoopAvatar_IsActive(void);
void CoopAvatar_Spawn(s16 x, s16 y, u8 gender);
void CoopAvatar_Despawn(void);
u32 CoopAvatar_Update(s16 x, s16 y, u8 facingDir);

#endif // GUARD_COOP_LINK_H
