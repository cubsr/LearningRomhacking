#include "global.h"
#include "coop_link.h"
#include "overworld.h"

// Track C1: renders the co-op partner in the local overworld. Runs once
// per field frame from CB1_Overworld. The partner's game free-runs; we
// just chase its last reported tile with the link-player avatar.

#define PARTNER_STALE_FRAMES 300 // ~5s without a presence packet
#define SNAP_TILE_DISTANCE   6   // farther than this: teleport, don't walk

void CoopOverworld_Update(void)
{
    const struct CoopPartnerStatus *p;

    if (!CoopSession_IsActive())
    {
        CoopAvatar_Despawn();
        return;
    }

    Coop_CheckIncomingBattleReq();

    p = Coop_GetPartnerStatus();
    if (!p->valid
     || p->framesSinceUpdate > PARTNER_STALE_FRAMES
     || p->mapGroup != (u8)gSaveBlock1Ptr->location.mapGroup
     || p->mapNum != (u8)gSaveBlock1Ptr->location.mapNum)
    {
        CoopAvatar_Despawn();
        return;
    }

    if (!CoopAvatar_IsActive())
    {
        CoopAvatar_Spawn(p->x, p->y, p->gender);
        return;
    }

    if (CoopAvatar_Update(p->x, p->y, p->facing) > SNAP_TILE_DISTANCE)
    {
        CoopAvatar_Despawn();
        CoopAvatar_Spawn(p->x, p->y, p->gender);
    }
}
