#include "global.h"
#include "coop_link.h"
#include "battle.h"
#include "battle_setup.h"
#include "event_data.h"
#include "field_player_avatar.h"
#include "field_weather.h"
#include "link.h"
#include "load_save.h"
#include "main.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "randomization.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "task.h"
#include "constants/battle_setup.h"
#include "constants/field_weather.h"

// Track C3: co-op trainer battles. When either player starts a normal
// trainer battle, the partner is invited over the presence channel; on
// accept, both games close the roaming link and enter the same link
// multi battle (2 humans vs the host-run AI trainer), each bringing
// their first healthy Pokémon. On decline/timeout the initiator just
// fights solo.

static void Task_CoopBattleInitiate(u8 taskId);
static void Task_CoopBattleRespond(u8 taskId);
static void Task_CoopPostBattle(u8 taskId);
static void CB2_ReturnFromCoopBattleGuest(void);

static EWRAM_DATA u16 sRespondTrainerId = 0;

static u32 FirstHealthyPartySlot(void)
{
    u32 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];

        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE
         && !GetMonData(mon, MON_DATA_IS_EGG)
         && GetMonData(mon, MON_DATA_HP) != 0)
            return i;
    }
    return 0;
}

// Shared by both sides once the battle is agreed: shrink the party to
// one mon and flip the battle into a co-op link multi.
static void PrepareCoopBattle(void)
{
    u32 i;

    SavePlayerParty();
    gSelectedOrderFromParty[0] = FirstHealthyPartySlot() + 1;
    for (i = 1; i < ARRAY_COUNT(gSelectedOrderFromParty); i++)
        gSelectedOrderFromParty[i] = 0;
    ReducePlayerPartyToSelectedMons();

    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE
                     | BATTLE_TYPE_LINK | BATTLE_TYPE_MULTI | BATTLE_TYPE_COOP;
    SetRandomizationSeedOverride(Coop_GetHostSeed());
    Coop_SetInCoopBattle(TRUE);
}

bool32 Coop_TryStartCoopTrainerBattle(void)
{
    const struct CoopPartnerStatus *p;

    if (!CoopSession_IsActive())
        return FALSE;
    // Only plain one-trainer battles; scripted partners, double
    // approaches, tutorial battles etc. stay solo.
    if (gBattleTypeFlags & ~(BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLE))
        return FALSE;
    p = Coop_GetPartnerStatus();
    if (!p->valid || p->activity != COOP_ACTIVITY_ROAMING || p->framesSinceUpdate > 120)
        return FALSE;

    Coop_ClearBattleNegotiation();
    Coop_QueueCommand(LINKCMD_COOP_BATTLE_REQ, TRAINER_BATTLE_PARAM.opponentA, 0);
    CreateTask(Task_CoopBattleInitiate, 1);
    DebugPrintf("coop: inviting partner to trainer battle %d", TRAINER_BATTLE_PARAM.opponentA);
    return TRUE;
}

#define tState data[0]
#define tTimer data[1]

static void Task_CoopBattleInitiate(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (tState)
    {
    case 0: // wait for the partner's answer
        switch (Coop_GetBattleReqAnswer())
        {
        case 1:
            DebugPrintf("coop: partner accepted");
            FadeScreen(FADE_TO_BLACK, 0);
            tState = 1;
            break;
        case 2:
            DebugPrintf("coop: partner busy, fighting solo");
            BattleSetup_LaunchTrainerBattle();
            DestroyTask(taskId);
            break;
        default:
            if (++tTimer > 300)
            {
                DebugPrintf("coop: invite timed out, fighting solo");
                BattleSetup_LaunchTrainerBattle();
                DestroyTask(taskId);
            }
            break;
        }
        break;
    case 1:
        if (!gPaletteFade.active)
        {
            gLinkType = LINKTYPE_BATTLE;
            ClearLinkCallback_2();
            SetCloseLinkCallback();
            tState = 2;
        }
        break;
    case 2:
        if (!gReceivedRemoteLinkPlayers)
        {
            PrepareCoopBattle();
            CleanupOverworldWindowsAndTilemaps();
            SetMainCallback2(CB2_InitBattle);
            // gMain.savedCallback is already CB2_EndTrainerBattle
            DestroyTask(taskId);
        }
        break;
    }
}

// Called each field frame (via CoopOverworld_Update) on the partner's side.
void Coop_CheckIncomingBattleReq(void)
{
    u16 trainerId;
    u8 taskId;

    if (!Coop_TakeIncomingBattleReq(&trainerId))
        return;

    if (ArePlayerFieldControlsLocked() == TRUE || gPaletteFade.active)
    {
        Coop_QueueCommand(LINKCMD_COOP_BATTLE_BUSY, 0, 0);
        DebugPrintf("coop: declined battle invite (busy)");
        return;
    }

    DebugPrintf("coop: accepted battle invite (trainer %d)", trainerId);
    sRespondTrainerId = trainerId;
    LockPlayerFieldControls();
    Coop_QueueCommand(LINKCMD_COOP_BATTLE_ACK, 0, 0);
    taskId = CreateTask(Task_CoopBattleRespond, 1);
    gTasks[taskId].tState = 0;
}

static void Task_CoopBattleRespond(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (tState)
    {
    case 0: // give the ACK a moment to go out on the wire
        if (++tTimer > 8)
        {
            FadeScreen(FADE_TO_BLACK, 0);
            tState = 1;
        }
        break;
    case 1:
        if (!gPaletteFade.active)
        {
            gLinkType = LINKTYPE_BATTLE;
            ClearLinkCallback_2();
            SetCloseLinkCallback();
            tState = 2;
        }
        break;
    case 2:
        if (!gReceivedRemoteLinkPlayers)
        {
            PrepareCoopBattle();
            TRAINER_BATTLE_PARAM.opponentA = sRespondTrainerId;
            TRAINER_BATTLE_PARAM.opponentB = 0xFFFF;
            CleanupOverworldWindowsAndTilemaps();
            gMain.savedCallback = CB2_ReturnFromCoopBattleGuest;
            SetMainCallback2(CB2_InitBattle);
            DestroyTask(taskId);
        }
        break;
    }
}

#undef tState
#undef tTimer

// Common epilogue: restore the full party and hand the link back to the
// roaming session.
void Coop_OnTrainerBattleEnd(void)
{
    if (!Coop_InCoopBattle())
        return;

    {
        // Keep the battled mon's EXP/HP/status changes: it lives at
        // slot 0 of the reduced party, headed back to its real slot.
        u32 slot = gSelectedOrderFromParty[0] - 1;
        struct Pokemon battled = gPlayerParty[0];

        LoadPlayerParty();
        if (slot < PARTY_SIZE)
            gPlayerParty[slot] = battled;
    }
    SetRandomizationSeedOverride(0);
    Coop_SetInCoopBattle(FALSE);
    Coop_ClearBattleNegotiation();
    if (FindTaskIdByFunc(Task_CoopPostBattle) == TASK_NONE)
        CreateTask(Task_CoopPostBattle, 80);
}

static void Task_CoopPostBattle(u8 taskId)
{
    // Wait until the field is back, then reopen the roaming link.
    if (gMain.callback2 == CB2_Overworld)
    {
        Coop_StartReestablish();
        DestroyTask(taskId);
    }
}

static void CB2_ReturnFromCoopBattleGuest(void)
{
    gBattleTypeFlags &= ~BATTLE_TYPE_LINK_IN_BATTLE;
    Coop_OnTrainerBattleEnd();
    if (gBattleOutcome == B_OUTCOME_WON)
        SetTrainerFlag(sRespondTrainerId);
    Overworld_ResetMapMusic();
    UnlockPlayerFieldControls();
    SetMainCallback2(CB2_ReturnToField);
}
