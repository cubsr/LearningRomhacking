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
#define COOP_PROTOCOL_VERSION 3

// --- Self-describing, single-erasure-tolerant packets --------------------
// The emulated link runs the two games one transfer out of step. The
// stream's cycle is nine words - our eight plus the link layer's own
// checksum - so a shifted receive window always swallows that foreign
// checksum word and drops exactly one of ours, in some rotation. Neither
// fixed positions nor plain rotation-recovery survive that.
//
// So each word carries its own address: bit 15 marks it as ours, bits
// 14-12 give its index, and bits 11-0 carry payload. Position in the
// window is irrelevant. Word 7 is the XOR parity of the other seven, so
// any single missing word is reconstructed. A checksum chunk then
// confirms the result, and packets repeat every frame, so a frame lost
// to an unlucky collision just retries.

#define COOP_WORD_BIT    0x8000
#define COOP_CHUNK_MASK  0x0FFF
#define COOP_CHUNKS      8      // 7 payload chunks + 1 parity

enum CoopPacketType
{
    COOP_PKT_HELLO = 1,
    COOP_PKT_PRESENCE,
    COOP_PKT_BATTLE_REQ,
    COOP_PKT_BATTLE_ACK,
    COOP_PKT_BATTLE_BUSY,
    COOP_PKT_BYE,
};

// chunk[0] = type | version, chunk[1..5] = fields, chunk[6] = checksum
static u16 CoopChunkChecksum(const u16 *chunk)
{
    u32 sum = 0;
    u32 i;

    for (i = 0; i < 6; i++)
        sum += chunk[i];
    return sum & COOP_CHUNK_MASK;
}

static void CoopBuildPacket(u16 *dst, u8 type, const u16 *fields)
{
    u16 chunk[COOP_CHUNKS];
    u16 parity = 0;
    u32 i;

    chunk[0] = (type & 0xF) | (COOP_PROTOCOL_VERSION << 4);
    for (i = 0; i < 5; i++)
        chunk[1 + i] = fields[i] & COOP_CHUNK_MASK;
    chunk[6] = CoopChunkChecksum(chunk);

    for (i = 0; i < COOP_CHUNKS - 1; i++)
        parity ^= chunk[i];
    chunk[7] = parity;

    for (i = 0; i < COOP_CHUNKS; i++)
        dst[i] = COOP_WORD_BIT | (i << 12) | (chunk[i] & COOP_CHUNK_MASK);
}

// Rebuild the chunks from a window that may be rotated and is missing at
// most one of our words. Returns FALSE if this isn't a valid co-op packet.
// Verify one candidate assignment, filling any single gap from parity.
static bool32 CoopVerifyChunks(u16 *chunk, u32 seen)
{
    u32 gaps = 0;
    u32 missingIdx = 0;
    u16 parity = 0;
    u32 i;

    for (i = 0; i < COOP_CHUNKS; i++)
    {
        if (!(seen & (1 << i)))
        {
            gaps++;
            missingIdx = i;
        }
    }
    if (gaps > 1)
        return FALSE;               // more than one gap: unrecoverable

    if (gaps == 1)
    {
        u16 restore = 0;

        for (i = 0; i < COOP_CHUNKS; i++)
        {
            if (i != missingIdx)
                restore ^= chunk[i];
        }
        chunk[missingIdx] = restore;
    }

    // Parity is circular once a word has been restored from it, so the
    // sum check (an independent function of the data) is what actually
    // proves the packet, along with the version field.
    for (i = 0; i < COOP_CHUNKS - 1; i++)
        parity ^= chunk[i];
    if (parity != chunk[7])
        return FALSE;
    if (chunk[6] != CoopChunkChecksum(chunk))
        return FALSE;
    if ((chunk[0] >> 4) != COOP_PROTOCOL_VERSION)
        return FALSE;
    return TRUE;
}

// Rebuild the chunks from a window that may be rotated and is missing at
// most one of our words, with at most one foreign word in its place.
// A foreign word can claim an index we already hold; when that happens
// both readings are tried and the packet is only accepted if exactly one
// of them verifies, so an impersonating word can never slip through.
static bool32 CoopParsePacket(const u16 *cmd, u16 *chunkOut)
{
    u16 cand[COOP_CHUNKS][2];
    u8 candCount[COOP_CHUNKS] = {0};
    u16 chunk[COOP_CHUNKS];
    u16 winner[COOP_CHUNKS];
    u32 seen = 0;
    u32 dupIdx = COOP_CHUNKS;
    u32 attempts, a, i;
    u32 verified = 0;

    for (i = 0; i < CMD_LENGTH; i++)
    {
        u32 idx;
        u16 value;

        if (!(cmd[i] & COOP_WORD_BIT))
            continue;
        idx = (cmd[i] >> 12) & 7;
        value = cmd[i] & COOP_CHUNK_MASK;
        if (candCount[idx] == 0)
        {
            cand[idx][0] = value;
            candCount[idx] = 1;
            seen |= 1 << idx;
        }
        else if (cand[idx][0] != value && candCount[idx] == 1)
        {
            cand[idx][1] = value;
            candCount[idx] = 2;
            dupIdx = idx;
        }
    }

    attempts = (dupIdx < COOP_CHUNKS) ? 2 : 1;
    for (a = 0; a < attempts; a++)
    {
        for (i = 0; i < COOP_CHUNKS; i++)
        {
            if (candCount[i] != 0)
                chunk[i] = cand[i][(i == dupIdx) ? a : 0];
        }
        if (CoopVerifyChunks(chunk, seen))
        {
            verified++;
            for (i = 0; i < COOP_CHUNKS; i++)
                winner[i] = chunk[i];
        }
    }

    if (verified != 1)
        return FALSE;               // none, or ambiguous: wait for the next frame

    for (i = 0; i < COOP_CHUNKS; i++)
        chunkOut[i] = winner[i];
    return TRUE;
}

static EWRAM_DATA struct
{
    u8 state;           // enum CoopSessionState
    u8 localId;
    u8 txTimer;
    u16 seq;
    u32 nonce;
    struct CoopPartnerStatus partner;

    // Repeated control packet (takes priority over the presence beat)
    u8 queuedType;
    u8 queuedRepeats;
    u16 queuedArg;
    u16 presence[5];

    // Co-op battle negotiation mailbox
    bool8 reqPending;       // partner asked us to join a battle
    u16 reqTrainerId;
    bool8 ackReceived;      // partner accepted our request
    bool8 busyReceived;     // partner declined our request
    bool8 inCoopBattle;
    bool8 helloPending;
    u8 recvLogCount;
    u16 hellosSent;
    u16 rxSeen;     // commands handed to us from the partner's slot
    u16 rxParsed;   // ...that decoded into a valid co-op packet
    u8 dumpCount;
    u16 lastSent[CMD_LENGTH];
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
                DebugPrintf("coop: waiting for hello (%d frames) sent=%d rxSeen=%d rxParsed=%d status=%08x",
                            tTimer, sCoop.hellosSent, sCoop.rxSeen, sCoop.rxParsed, gLinkStatus);
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

// Queue a control packet; it is repeated for a few frames so a single
// mangled transfer can't lose it.
void Coop_QueueCommand(u16 op, u16 a, u16 b)
{
    switch (op)
    {
    case LINKCMD_COOP_BATTLE_REQ:  sCoop.queuedType = COOP_PKT_BATTLE_REQ;  break;
    case LINKCMD_COOP_BATTLE_ACK:  sCoop.queuedType = COOP_PKT_BATTLE_ACK;  break;
    case LINKCMD_COOP_BATTLE_BUSY: sCoop.queuedType = COOP_PKT_BATTLE_BUSY; break;
    case LINKCMD_COOP_BYE:         sCoop.queuedType = COOP_PKT_BYE;         break;
    default: return;
    }
    sCoop.queuedArg = a;
    sCoop.queuedRepeats = 20;
}

// Called once per frame from LinkMain2 after link callbacks have had
// their chance to claim gSendCmd. The same packet is emitted on
// consecutive frames so a receiver whose framing straddles two commands
// still reads consistent content.
void CoopLink_BuildCmd(void)
{
    u16 payload[5] = {0};

    if (sCoop.state != COOP_STATE_ACTIVE && sCoop.state != COOP_STATE_LINKING)
        return;

    if (sCoop.partner.valid && sCoop.partner.framesSinceUpdate < 0xFFFF)
        sCoop.partner.framesSinceUpdate++;

    if (gSendCmd[0] != 0)
        return;

    sCoop.txTimer++;

    if (sCoop.helloPending)
    {
        u32 seed = gSaveBlock2Ptr->randomizationSeed;

        sCoop.hellosSent++;
        payload[0] = seed & COOP_CHUNK_MASK;
        payload[1] = (seed >> 12) & COOP_CHUNK_MASK;
        payload[2] = (seed >> 24) & 0xFF;
        payload[3] = gSaveBlock2Ptr->randomizationFlags | (gSaveBlock2Ptr->playerGender << 8);
        payload[4] = 0;
        CoopBuildPacket(gSendCmd, COOP_PKT_HELLO, payload);
        memcpy(sCoop.lastSent, gSendCmd, sizeof(sCoop.lastSent));
        return;
    }

    if (sCoop.state != COOP_STATE_ACTIVE)
        return;

    if (sCoop.queuedRepeats != 0)
    {
        sCoop.queuedRepeats--;
        payload[0] = sCoop.queuedArg;
        CoopBuildPacket(gSendCmd, sCoop.queuedType, payload);
        return;
    }

    // Refresh the presence snapshot a few times a second, but keep
    // sending the current one every frame.
    if ((sCoop.txTimer & 3) == 0)
    {
        sCoop.presence[0] = (u8)gSaveBlock1Ptr->location.mapGroup;
        sCoop.presence[1] = (u8)gSaveBlock1Ptr->location.mapNum;
        sCoop.presence[2] = gSaveBlock1Ptr->pos.x & COOP_CHUNK_MASK;
        sCoop.presence[3] = gSaveBlock1Ptr->pos.y & COOP_CHUNK_MASK;
        sCoop.presence[4] = (GetPlayerFacingDirection() << 8)
                          | (Coop_GetLocalActivity() << 5)
                          | (++sCoop.seq & 0x1F);
    }
    CoopBuildPacket(gSendCmd, COOP_PKT_PRESENCE, sCoop.presence);
}

void CoopLink_HandleRecvCmd(const u16 *cmd, u32 playerId)
{
    u16 chunk[COOP_CHUNKS];
    const u16 *payload = &chunk[1];

    if (sCoop.state == COOP_STATE_IDLE || sCoop.state == COOP_STATE_ERROR)
        return;

    // Ground truth: dump raw words for both our own echo and the
    // partner's slot, next to what we actually put on the wire.
    if (sCoop.dumpCount < 8)
    {
        sCoop.dumpCount++;
        DebugPrintf("coop: RX slot%d %04x %04x %04x %04x %04x %04x %04x %04x",
                    playerId, cmd[0], cmd[1], cmd[2], cmd[3],
                    cmd[4], cmd[5], cmd[6], cmd[7]);
        if (sCoop.dumpCount == 1)
            DebugPrintf("coop: TX      %04x %04x %04x %04x %04x %04x %04x %04x",
                        sCoop.lastSent[0], sCoop.lastSent[1], sCoop.lastSent[2],
                        sCoop.lastSent[3], sCoop.lastSent[4], sCoop.lastSent[5],
                        sCoop.lastSent[6], sCoop.lastSent[7]);
    }

    // Only slots below the player count are refreshed by the link layer;
    // the rest keep stale data forever, so ignore anything that isn't
    // the partner's slot.
    if (playerId != (u32)(sCoop.localId ^ 1))
        return;

    sCoop.rxSeen++;
    if (!CoopParsePacket(cmd, chunk))
        return;
    sCoop.rxParsed++;

    switch (chunk[0] & 0xF)
    {
    case COOP_PKT_HELLO:
        sCoop.partner.seed = payload[0] | ((u32)payload[1] << 12) | ((u32)payload[2] << 24);
        sCoop.partner.randoFlags = payload[3] & 0xFF;
        sCoop.partner.gender = (payload[3] >> 8) & 1;
        sCoop.partner.valid = TRUE;
        sCoop.partner.framesSinceUpdate = 0;
        break;
    case COOP_PKT_PRESENCE:
        sCoop.partner.valid = TRUE;
        sCoop.partner.mapGroup = payload[0];
        sCoop.partner.mapNum = payload[1];
        sCoop.partner.x = payload[2];
        sCoop.partner.y = payload[3];
        sCoop.partner.facing = payload[4] >> 8;
        sCoop.partner.activity = (payload[4] >> 5) & 0x7;
        sCoop.partner.lastSeq = payload[4] & 0x1F;
        sCoop.partner.framesSinceUpdate = 0;
        break;
    case COOP_PKT_BATTLE_REQ:
        if (!sCoop.reqPending)
            DebugPrintf("coop: battle request trainer=%d", payload[0]);
        sCoop.reqPending = TRUE;
        sCoop.reqTrainerId = payload[0];
        break;
    case COOP_PKT_BATTLE_ACK:
        sCoop.ackReceived = TRUE;
        break;
    case COOP_PKT_BATTLE_BUSY:
        sCoop.busyReceived = TRUE;
        break;
    case COOP_PKT_BYE:
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
