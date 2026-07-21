#include "global.h"
#include "coop_sio.h"
#include "link.h"

#define COOP_SIO_WORDS 8

static EWRAM_DATA struct
{
    bool8 active;
    u8 txIndex;
    bool8 hasPacket;
    u16 packet[COOP_SIO_WORDS];
    u16 lastRecv;
} sSio = {0};

void CoopSio_Start(void)
{
    // Take the serial hardware away from the vanilla link stack.
    CloseLink();

    REG_RCNT = 0;
    REG_SIOCNT = SIO_MULTI_MODE | SIO_115200_BPS; // deliberately no IRQ
    REG_SIOMLT_SEND = 0;

    memset(&sSio, 0, sizeof(sSio));
    sSio.active = TRUE;
}

void CoopSio_Stop(void)
{
    if (!sSio.active)
        return;
    sSio.active = FALSE;
    REG_SIOMLT_SEND = 0;
}

bool32 CoopSio_IsActive(void)
{
    return sSio.active;
}

u32 CoopSio_GetLocalId(void)
{
    return SIO_MULTI_CNT->id;
}

void CoopSio_SetPacket(const u16 *words)
{
    u32 i;

    for (i = 0; i < COOP_SIO_WORDS; i++)
        sSio.packet[i] = words[i];
    sSio.hasPacket = TRUE;
}

void CoopSio_Update(void)
{
    u16 recv[4];
    u32 id;
    u16 partnerWord;

    if (!sSio.active)
        return;

    id = SIO_MULTI_CNT->id & 1;

    // Harvest the previous transfer before arming the next one: the
    // hardware blanks these registers while a transfer is in flight.
    *(u64 *)recv = REG_SIOMLT_RECV;
    partnerWord = recv[id ^ 1];
    if (partnerWord != 0xFFFF && partnerWord != 0 && partnerWord != sSio.lastRecv)
    {
        sSio.lastRecv = partnerWord;
        CoopLink_ReceiveWord(partnerWord);
    }

    // Stage this frame's word. Cycling one word per frame means the
    // partner's staging can lag by a frame without consequence.
    if (sSio.hasPacket)
    {
        REG_SIOMLT_SEND = sSio.packet[sSio.txIndex];
        if (++sSio.txIndex >= COOP_SIO_WORDS)
            sSio.txIndex = 0;
    }

    // Whoever holds ID 0 is the clock source.
    if (id == 0)
    {
        u16 siocnt = REG_SIOCNT;

        if (!(siocnt & SIO_MULTI_BUSY))
            REG_SIOCNT = siocnt | SIO_START;
    }
}
