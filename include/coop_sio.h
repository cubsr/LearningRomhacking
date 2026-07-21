#ifndef GUARD_COOP_SIO_H
#define GUARD_COOP_SIO_H

#include "global.h"

// Minimal serial transport for co-op, owning the SIO hardware directly.
//
// The vanilla link stack multiplexes an eight-word command plus a stream
// checksum into every frame and hands back data unrelated to what was
// sent under emulation. The raw bus, by contrast, is perfectly reliable:
// the debug probe moved 5400 consecutive transfers at the same rate with
// zero bad payloads. So co-op drives the hardware itself, one polled
// transfer per frame, with no interrupts, queues or stream checksums.
//
// One 16-bit word crosses per frame in each direction. Every word is
// self-describing (see the packet format in coop_link.c), so word order,
// a frame of lag, or an occasional drop are all harmless: the receiver
// files each word by its own index and acts once a packet is complete.

void CoopSio_Start(void);
void CoopSio_Stop(void);
bool32 CoopSio_IsActive(void);
u32 CoopSio_GetLocalId(void);

// Packet to transmit, one word per frame, repeating until replaced.
void CoopSio_SetPacket(const u16 *words);

// Called every frame from the main loop.
void CoopSio_Update(void);

// Implemented by coop_link.c; receives one word per completed transfer.
void CoopLink_ReceiveWord(u16 word);

#endif // GUARD_COOP_SIO_H
