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

// Bump when the packet layout changes so mismatched builds don't pair.
#define COOP_PROTOCOL_VERSION 1

static EWRAM_DATA struct
{
    u8 state;           // enum CoopSessionState
    u8 localId;
    u8 txTimer;
    u16 seq;
    u32 nonce;
    struct CoopPartnerStatus partner;

    // One-shot outgoing command (takes priority over the presence beat)
    u16 queuedCmd[3];
    bool8 cmdQueued;

    // Co-op battle negotiation mailbox
    bool8 reqPending;       // partner asked us to join a battle
    u16 reqTrainerId;
    bool8 ackReceived;      // partner accepted our request
    bool8 busyReceived;     // partner declined our request
    bool8 inCoopBattle;
    bool8 helloPending;
    u8 recvLogCount;
    u16 hellosSent;
    u32 checksumErrors;
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

#define tRetries data[2]
#define COOP_LINKUP_MAX_RETRIES 8
#define COOP_LINKUP_RETRY_STATE 90

static void Task_CoopLinkup(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    // Transient SIO errors are common when the two games open their
    // links far apart in time: back off and reopen instead of failing.
    if (tState >= 1 && tState < COOP_LINKUP_RETRY_STATE && HasLinkErrorOccurred() == TRUE)
    {
        if (++tRetries > COOP_LINKUP_MAX_RETRIES)
        {
            CoopLinkupFailed(taskId, "too many link errors");
            return;
        }
        DebugPrintf("coop: linkup retry %d", tRetries);
        CloseLink();
        tTimer = 0;
        tState = COOP_LINKUP_RETRY_STATE;
    }

    switch (tState)
    {
    case COOP_LINKUP_RETRY_STATE: // cooldown, then reopen
        if (++tTimer > 60)
            tState = 0;
        break;
    case 0:
        gLinkType = LINKTYPE_COOP;
        OpenLinkTimed();
        ResetLinkPlayerCount();
        ResetLinkPlayers();
        // Drop the vanilla player-data exchange callback: co-op does its
        // own identification, and leaving it armed would start a block
        // transfer we neither need nor can rely on. It also keeps
        // gReceivedRemoteLinkPlayers clear, so field code doesn't mistake
        // the session for a cable club room.
        gLinkCallback = NULL;
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
        if (++tTimer < 30)
            break;
        DebugPrintf("coop: advancing link state, players=%d master=%d",
                    GetLinkPlayerCount_2(), (gLinkStatus & LINK_STAT_MASTER) != 0);
        if (gLinkStatus & LINK_STAT_MASTER)
            CheckShouldAdvanceLinkState();
        tTimer = 0;
        tState = 4;
        break;
    case 4: // wait for the hardware connection, then identify ourselves
            // from the link status directly. We deliberately do NOT use
            // the vanilla player-data exchange: it rides the multi-frame
            // block protocol, which the emulator's serial drift corrupts.
        if (!(gLinkStatus & LINK_STAT_CONN_ESTABLISHED))
        {
            if (++tTimer > 900)
                CoopLinkupFailed(taskId, "no connection");
            return;
        }
        sCoop.localId = gLinkStatus & LINK_STAT_LOCAL_ID;
        sCoop.nonce = ((u32)Random() << 16) | Random();
        sCoop.helloPending = TRUE;
        DebugPrintf("coop: connected as id=%d host=%d, saying hello",
                    sCoop.localId, Coop_IsHost());
        tTimer = 0;
        tState = 5;
        break;
    case 5: // trade hello packets on the raw command channel. These are
            // single self-contained frames repeated until one lands, so a
            // corrupted transfer costs nothing but a few frames.
        if (!sCoop.partner.valid)
        {
            if ((++tTimer % 300) == 0)
                DebugPrintf("coop: waiting for hello (%d frames), sent=%d recvQ=%d recvSeen=%d status=%08x",
                            tTimer, sCoop.hellosSent, GetLinkRecvQueueLength(),
                            sCoop.recvLogCount, gLinkStatus);
            if (tTimer > 1800)
                CoopLinkupFailed(taskId, "hello timeout");
            return;
        }
        sCoop.helloPending = FALSE;
        sCoop.state = COOP_STATE_ACTIVE;
        DebugPrintf("coop: ACTIVE id=%d host=%d partnerSeed=%08x",
                    sCoop.localId, Coop_IsHost(), sCoop.partner.seed);
        DestroyTask(taskId);
        break;
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

void Coop_QueueCommand(u16 op, u16 a, u16 b)
{
    sCoop.queuedCmd[0] = op;
    sCoop.queuedCmd[1] = a;
    sCoop.queuedCmd[2] = b;
    sCoop.cmdQueued = TRUE;
}

// Called once per frame from LinkMain2 after link callbacks have had
// their chance to claim gSendCmd.
void CoopLink_BuildCmd(void)
{
    if (sCoop.state != COOP_STATE_ACTIVE && sCoop.state != COOP_STATE_LINKING)
        return;

    if (sCoop.partner.valid && sCoop.partner.framesSinceUpdate < 0xFFFF)
        sCoop.partner.framesSinceUpdate++;

    if (gSendCmd[0] != 0)
        return;

    // Repeat the hello until the partner's arrives; the reply doubles as
    // the acknowledgement.
    if (sCoop.helloPending)
    {
        if ((++sCoop.txTimer & 7) != 0)
            return;
        sCoop.hellosSent++;
        gSendCmd[0] = LINKCMD_COOP_HELLO;
        gSendCmd[1] = COOP_PROTOCOL_VERSION;
        gSendCmd[2] = gSaveBlock2Ptr->randomizationSeed;
        gSendCmd[3] = gSaveBlock2Ptr->randomizationSeed >> 16;
        gSendCmd[4] = gSaveBlock2Ptr->randomizationFlags | (gSaveBlock2Ptr->playerGender << 8);
        gSendCmd[5] = sCoop.nonce;
        return;
    }

    if (sCoop.state != COOP_STATE_ACTIVE)
        return;

    if (sCoop.cmdQueued)
    {
        gSendCmd[0] = sCoop.queuedCmd[0];
        gSendCmd[1] = sCoop.queuedCmd[1];
        gSendCmd[2] = sCoop.queuedCmd[2];
        sCoop.cmdQueued = FALSE;
        return;
    }

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
    if (sCoop.state == COOP_STATE_IDLE || sCoop.state == COOP_STATE_ERROR)
        return;
    if (playerId == sCoop.localId)
        return;

    switch (cmd[0])
    {
    case LINKCMD_COOP_HELLO:
        if (cmd[1] != COOP_PROTOCOL_VERSION)
        {
            DebugPrintf("coop: partner protocol %d != %d, ignoring",
                        cmd[1], COOP_PROTOCOL_VERSION);
            return;
        }
        sCoop.partner.seed = cmd[2] | ((u32)cmd[3] << 16);
        sCoop.partner.randoFlags = cmd[4] & 0xFF;
        sCoop.partner.gender = cmd[4] >> 8;
        sCoop.partner.valid = TRUE;
        sCoop.partner.framesSinceUpdate = 0;
        break;
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
    case LINKCMD_COOP_BATTLE_REQ:
        DebugPrintf("coop: battle request trainer=%d", cmd[1]);
        sCoop.reqPending = TRUE;
        sCoop.reqTrainerId = cmd[1];
        break;
    case LINKCMD_COOP_BATTLE_ACK:
        sCoop.ackReceived = TRUE;
        break;
    case LINKCMD_COOP_BATTLE_BUSY:
        sCoop.busyReceived = TRUE;
        break;
    case LINKCMD_COOP_BYE:
        DebugPrintf("coop: partner left");
        sCoop.partner.valid = FALSE;
        sCoop.state = COOP_STATE_ERROR;
        break;
    }
}

// --- Co-op battle support -----------------------------------------------

bool32 Coop_TakeIncomingBattleReq(u16 *trainerId)
{
    if (!sCoop.reqPending)
        return FALSE;
    sCoop.reqPending = FALSE;
    *trainerId = sCoop.reqTrainerId;
    return TRUE;
}

// 0 = no answer yet, 1 = accepted, 2 = declined
u32 Coop_GetBattleReqAnswer(void)
{
    if (sCoop.ackReceived)
        return 1;
    if (sCoop.busyReceived)
        return 2;
    return 0;
}

void Coop_ClearBattleNegotiation(void)
{
    sCoop.reqPending = FALSE;
    sCoop.ackReceived = FALSE;
    sCoop.busyReceived = FALSE;
}

void Coop_SetInCoopBattle(bool32 inBattle)
{
    sCoop.inCoopBattle = inBattle;
}

bool32 Coop_InCoopBattle(void)
{
    return sCoop.inCoopBattle;
}

// Seed used to randomize the enemy trainer's party slot. In a co-op
// battle the first half of the team comes from the host's world and the
// rest from the guest's, and both games agree on the halves.
u32 Coop_GetTrainerSlotSeed(u32 slot, u32 monsCount)
{
    u32 hostSeed, guestSeed;

    if (!sCoop.inCoopBattle)
        return gSaveBlock2Ptr->randomizationSeed;

    if (Coop_IsHost())
    {
        hostSeed = gSaveBlock2Ptr->randomizationSeed;
        guestSeed = sCoop.partner.seed;
    }
    else
    {
        hostSeed = sCoop.partner.seed;
        guestSeed = gSaveBlock2Ptr->randomizationSeed;
    }
    return (slot < (monsCount + 1) / 2) ? hostSeed : guestSeed;
}

u32 Coop_GetHostSeed(void)
{
    return Coop_IsHost() ? gSaveBlock2Ptr->randomizationSeed : sCoop.partner.seed;
}

// Physical link comes back after a battle closed it; the logical session
// carried on the whole time.
static void Task_CoopReestablish(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (data[0] >= 1 && data[0] <= 3 && HasLinkErrorOccurred() == TRUE)
    {
        if (++data[2] > 8)
        {
            DebugPrintf("coop: reestablish giving up");
            Coop_SessionFailed();
            CloseLink();
            DestroyTask(taskId);
            return;
        }
        DebugPrintf("coop: reestablish retry %d", data[2]);
        CloseLink();
        data[1] = 0;
        data[0] = 4;
    }

    switch (data[0])
    {
    case 4: // cooldown before reopening
        if (++data[1] > 60)
            data[0] = 0;
        break;
    case 0:
        gLinkType = LINKTYPE_COOP;
        OpenLink();
        ResetLinkPlayers();
        data[1] = 0;
        data[0] = 1;
        break;
    case 1:
        if (++data[1] > 10)
            data[0] = 2;
        break;
    case 2:
        if (GetLinkPlayerCount_2() >= 2)
        {
            if (IsLinkMaster() == TRUE)
                CheckShouldAdvanceLinkState();
            data[1] = 0;
            data[0] = 3;
        }
        else if (++data[1] > 1800)
        {
            DebugPrintf("coop: reestablish timed out");
            Coop_SessionFailed();
            CloseLink();
            DestroyTask(taskId);
        }
        break;
    case 3:
        if (gReceivedRemoteLinkPlayers == TRUE)
        {
            DebugPrintf("coop: link reestablished");
            DestroyTask(taskId);
        }
        else if (++data[1] > 1800)
        {
            DebugPrintf("coop: reestablish timed out");
            Coop_SessionFailed();
            CloseLink();
            DestroyTask(taskId);
        }
        break;
    }
}

void Coop_StartReestablish(void)
{
    if (sCoop.state != COOP_STATE_ACTIVE)
        return;
    if (FindTaskIdByFunc(Task_CoopReestablish) == TASK_NONE)
        CreateTask(Task_CoopReestablish, 80);
}

// Give up on the session entirely (retries exhausted).
void Coop_SessionFailed(void)
{
    sCoop.state = COOP_STATE_ERROR;
    sCoop.partner.valid = FALSE;
}

// Checksum errors are survivable outside of battles, where the vanilla
// battle protocol needs an exact stream.
bool32 Coop_ToleratesChecksumErrors(void)
{
    if (sCoop.inCoopBattle)
        return FALSE;
    return sCoop.state == COOP_STATE_LINKING || sCoop.state == COOP_STATE_ACTIVE;
}

// Wire diagnostics: what, if anything, is actually arriving. Logs the
// first few opcodes seen per slot so we can tell "nothing crosses" from
// "data crosses but is corrupted".
void CoopLink_NoteRecvOpcode(u32 playerId, u16 opcode)
{
    if (sCoop.state != COOP_STATE_LINKING)
        return;
    if (sCoop.recvLogCount >= 12)
        return;
    sCoop.recvLogCount++;
    DebugPrintf("coop: wire recv slot=%d opcode=%04x (mine=%d)",
                playerId, opcode, playerId == sCoop.localId);
}

void Coop_NoteChecksumError(void)
{
    sCoop.checksumErrors++;
    // Log sparsely; these can fire every frame while drifted.
    if (sCoop.checksumErrors == 1 || (sCoop.checksumErrors % 300) == 0)
        DebugPrintf("coop: %d checksum errors (tolerated), state=%d players=%d",
                    sCoop.checksumErrors, sCoop.state, GetLinkPlayerCount_2());
}

// Returns TRUE if the error belongs to a co-op session; the caller then
// skips the CB2_LinkError screen and just closes the link. Transient
// SIO errors are common when one game opens its link long before the
// other, so both linkup and live sessions retry instead of dying.
bool32 Coop_OnLinkError(void)
{
    switch (sCoop.state)
    {
    case COOP_STATE_LINKING:
        // The linkup task notices via HasLinkErrorOccurred and retries.
        DebugPrintf("coop: link error during linkup (status=%08x) hw=%d qfull=%d lagM=%d badId=%d lagS=%d players=%d",
                    gLinkStatus,
                    (gLinkStatus & LINK_STAT_ERROR_HARDWARE) != 0,
                    (gLinkStatus & LINK_STAT_ERROR_QUEUE_FULL) != 0,
                    (gLinkStatus & LINK_STAT_ERROR_LAG_MASTER) != 0,
                    (gLinkStatus & LINK_STAT_ERROR_INVALID_ID) != 0,
                    (gLinkStatus & LINK_STAT_ERROR_LAG_SLAVE) != 0,
                    GetLinkPlayerCount_2());
        return TRUE;
    case COOP_STATE_ACTIVE:
        DebugPrintf("coop: err bits hw=%d cksum=%d qfull=%d lagM=%d badId=%d lagS=%d players=%d",
                    (gLinkStatus & LINK_STAT_ERROR_HARDWARE) != 0,
                    (gLinkStatus & LINK_STAT_ERROR_CHECKSUM) != 0,
                    (gLinkStatus & LINK_STAT_ERROR_QUEUE_FULL) != 0,
                    (gLinkStatus & LINK_STAT_ERROR_LAG_MASTER) != 0,
                    (gLinkStatus & LINK_STAT_ERROR_INVALID_ID) != 0,
                    (gLinkStatus & LINK_STAT_ERROR_LAG_SLAVE) != 0,
                    GetLinkPlayerCount_2());
        if (sCoop.inCoopBattle)
        {
            // Mid-battle errors are fatal; the battle engine owns the link.
            DebugPrintf("coop: link error in battle (status=%08x)", gLinkStatus);
            Coop_SessionFailed();
            return TRUE;
        }
        DebugPrintf("coop: link error (status=%08x), reconnecting", gLinkStatus);
        sCoop.partner.framesSinceUpdate = 0x1000; // avatar goes stale immediately
        Coop_StartReestablish();
        return TRUE;
    default:
        return FALSE;
    }
}
