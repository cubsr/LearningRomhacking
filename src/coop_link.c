#include "global.h"
#include "coop_link.h"
#include "event_data.h"
#include "field_player_avatar.h"
#include "link.h"
#include "main.h"
#include "overworld.h"
#include "random.h"
#include "randomization.h"
#include "task.h"
#include "constants/songs.h"

// 2-player co-op session (Track C0). The linkup clones the cable club
// task chain but runs in the field with no message boxes: the master
// auto-confirms once both games are connected. After the player-data
// exchange, both sides swap a hello block (nonce + randomizer seed).

#define COOP_HELLO_MAGIC 0x434F4F50 // "COOP"

struct CoopHello
{
    u32 magic;
    u32 nonce;
    u32 seed;
    u8 randoFlags;
    u8 gender;
    u8 padding[2];
};

static EWRAM_DATA struct
{
    u8 state;           // enum CoopSessionState
    u8 localId;
    u8 txTimer;
    u16 seq;
    u32 nonce;
    struct CoopPartnerStatus partner;
} sCoop = {0};

static void Task_CoopLinkup(u8 taskId);

bool32 CoopSession_IsActive(void)
{
    return sCoop.state == COOP_STATE_ACTIVE;
}

u8 Coop_GetSessionState(void)
{
    return sCoop.state;
}

bool32 Coop_IsHost(void)
{
    return sCoop.localId == 0;
}

const struct CoopPartnerStatus *Coop_GetPartnerStatus(void)
{
    return &sCoop.partner;
}

void Coop_StartSession(void)
{
    u8 oldTask;

    if (sCoop.state == COOP_STATE_ACTIVE)
        return;

    // Restart cleanly if a previous attempt is wedged.
    oldTask = FindTaskIdByFunc(Task_CoopLinkup);
    if (oldTask != TASK_NONE)
    {
        DestroyTask(oldTask);
        CloseLink();
    }

    memset(&sCoop, 0, sizeof(sCoop));
    sCoop.state = COOP_STATE_LINKING;
    CreateTask(Task_CoopLinkup, 80);
    DebugPrintf("coop: linkup started");
}

void Coop_EndSession(void)
{
    u8 taskId = FindTaskIdByFunc(Task_CoopLinkup);

    if (taskId != TASK_NONE)
        DestroyTask(taskId);
    if (sCoop.state == COOP_STATE_ACTIVE || sCoop.state == COOP_STATE_LINKING)
        SetCloseLinkCallback();
    sCoop.state = COOP_STATE_IDLE;
    sCoop.partner.valid = FALSE;
    DebugPrintf("coop: session ended");
}

#define tState  data[0]
#define tTimer  data[1]

static void CoopLinkupFailed(u8 taskId, const char *why)
{
    DebugPrintf("coop: linkup failed (%s)", why);
    gLinkType = 0;
    CloseLink();
    sCoop.state = COOP_STATE_ERROR;
    DestroyTask(taskId);
}

static void Task_CoopLinkup(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (tState)
    {
    case 0:
        gLinkType = LINKTYPE_COOP;
        OpenLinkTimed();
        ResetLinkPlayerCount();
        ResetLinkPlayers();
        tTimer = 0;
        tState = 1;
        break;
    case 1:
        if (++tTimer > 10)
            tState = 2;
        break;
    case 2: // wait for the second game
        if (JOY_NEW(B_BUTTON) && IsLinkConnectionEstablished() == FALSE)
        {
            CoopLinkupFailed(taskId, "canceled");
            return;
        }
        if (++tTimer > 60 * 30)
        {
            CoopLinkupFailed(taskId, "no partner");
            return;
        }
        if (GetLinkPlayerCount_2() >= 2)
        {
            SetSuppressLinkErrorMessage(TRUE);
            tTimer = 0;
            tState = 3;
        }
        break;
    case 3: // settle, then the master advances the link state for everyone
        if (HasLinkErrorOccurred() == TRUE)
        {
            CoopLinkupFailed(taskId, "link error");
            return;
        }
        if (++tTimer < 30)
            break;
        SaveLinkPlayers(GetLinkPlayerCount_2());
        if (IsLinkMaster() == TRUE)
            CheckShouldAdvanceLinkState();
        tTimer = 0;
        tState = 4;
        break;
    case 4: // wait until both games have each other's player data.
            // The vanilla exchange validator also compares link types,
            // which is timing-sensitive; the hello block's magic below is
            // our real compatibility check, so don't use it.
    {
        struct CoopHello *hello;

        if (HasLinkErrorOccurred() == TRUE)
        {
            CoopLinkupFailed(taskId, "link error");
            return;
        }
        if (gReceivedRemoteLinkPlayers != TRUE)
        {
            if (++tTimer > 900)
                CoopLinkupFailed(taskId, "player data timeout");
            return;
        }
        DebugPrintf("coop: players exchanged (linkTypes %04x/%04x)",
                    gLinkPlayers[0].linkType, gLinkPlayers[1].linkType);
        gFieldLinkPlayerCount = GetLinkPlayerCount_2();
        gLocalLinkPlayerId = GetMultiplayerId();
        SaveLinkPlayers(gFieldLinkPlayerCount);
        sCoop.localId = gLocalLinkPlayerId;
        sCoop.nonce = ((u32)Random() << 16) | Random();

        hello = (struct CoopHello *)gBlockSendBuffer;
        hello->magic = COOP_HELLO_MAGIC;
        hello->nonce = sCoop.nonce;
        hello->seed = gSaveBlock2Ptr->randomizationSeed;
        hello->randoFlags = gSaveBlock2Ptr->randomizationFlags;
        hello->gender = gSaveBlock2Ptr->playerGender;
        if (IsLinkMaster() == TRUE)
            SendBlockRequest(BLOCK_REQ_SIZE_100);
        tTimer = 0;
        tState = 5;
        break;
    }
    case 5: // wait for both hello blocks
    {
        const struct CoopHello *hello;
        u8 partnerId;

        if (HasLinkErrorOccurred() == TRUE)
        {
            CoopLinkupFailed(taskId, "link error");
            return;
        }
        if ((GetBlockReceivedStatus() & 3) != 3)
        {
            if (++tTimer > 600)
                CoopLinkupFailed(taskId, "hello timeout");
            return;
        }
        partnerId = sCoop.localId ^ 1;
        hello = (const struct CoopHello *)gBlockRecvBuffer[partnerId];
        if (hello->magic != COOP_HELLO_MAGIC)
        {
            CoopLinkupFailed(taskId, "bad hello");
            return;
        }
        sCoop.partner.seed = hello->seed;
        sCoop.partner.randoFlags = hello->randoFlags;
        sCoop.partner.gender = hello->gender;
        ResetBlockReceivedFlags();
        sCoop.state = COOP_STATE_ACTIVE;
        DebugPrintf("coop: ACTIVE id=%d host=%d partnerSeed=%08x",
                    sCoop.localId, Coop_IsHost(), sCoop.partner.seed);
        DestroyTask(taskId);
        break;
    }
    }
}

#undef tState
#undef tTimer

static u8 Coop_GetLocalActivity(void)
{
    if (gMain.inBattle)
        return COOP_ACTIVITY_BATTLE;
    return COOP_ACTIVITY_ROAMING;
}

// Called once per frame from LinkMain2 after link callbacks have had
// their chance to claim gSendCmd.
void CoopLink_BuildCmd(void)
{
    if (sCoop.state != COOP_STATE_ACTIVE)
        return;

    if (sCoop.partner.valid && sCoop.partner.framesSinceUpdate < 0xFFFF)
        sCoop.partner.framesSinceUpdate++;

    if (gSendCmd[0] != 0)
        return;
    if ((++sCoop.txTimer & 3) != 0)
        return;

    gSendCmd[0] = LINKCMD_COOP_PRESENCE;
    gSendCmd[1] = ((u8)gSaveBlock1Ptr->location.mapGroup << 8) | (u8)gSaveBlock1Ptr->location.mapNum;
    gSendCmd[2] = gSaveBlock1Ptr->pos.x;
    gSendCmd[3] = gSaveBlock1Ptr->pos.y;
    gSendCmd[4] = (GetPlayerFacingDirection() << 8) | Coop_GetLocalActivity();
    gSendCmd[5] = ++sCoop.seq;
}

void CoopLink_HandleRecvCmd(const u16 *cmd, u32 playerId)
{
    if (sCoop.state != COOP_STATE_ACTIVE || playerId == sCoop.localId)
        return;

    switch (cmd[0])
    {
    case LINKCMD_COOP_PRESENCE:
        sCoop.partner.valid = TRUE;
        sCoop.partner.mapGroup = cmd[1] >> 8;
        sCoop.partner.mapNum = cmd[1] & 0xFF;
        sCoop.partner.x = cmd[2];
        sCoop.partner.y = cmd[3];
        sCoop.partner.facing = cmd[4] >> 8;
        sCoop.partner.activity = cmd[4] & 0xFF;
        sCoop.partner.lastSeq = cmd[5];
        sCoop.partner.framesSinceUpdate = 0;
        break;
    case LINKCMD_COOP_BYE:
        DebugPrintf("coop: partner left");
        sCoop.partner.valid = FALSE;
        sCoop.state = COOP_STATE_ERROR;
        break;
    }
}

// Returns TRUE if the error belongs to a co-op session; the caller then
// skips the CB2_LinkError screen and just closes the link.
bool32 Coop_OnLinkError(void)
{
    if (sCoop.state != COOP_STATE_ACTIVE && sCoop.state != COOP_STATE_LINKING)
        return FALSE;

    DebugPrintf("coop: link error, session over (status=%08x)", gLinkStatus);
    sCoop.state = COOP_STATE_ERROR;
    sCoop.partner.valid = FALSE;
    return TRUE;
}
