
#include <stdio.h>
#include <string.h>
typedef unsigned short u16; typedef unsigned char u8; typedef unsigned int u32; typedef int bool32;
#define TRUE 1
#define FALSE 0
#define CMD_LENGTH 8
#define COOP_WORD_BIT 0x8000
#define COOP_CHUNK_MASK 0x0FFF
#define COOP_CHUNKS 8
#define COOP_PROTOCOL_VERSION 3
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


int main(void){
 u16 fieldsets[4][5]={{0x123,0x456,0x7ff,0x089,0xabc},{0,0,0,0,0},{0xfff,0xfff,0xfff,0xfff,0xfff},{0x001,0x800,0x555,0xaaa,0x00f}};
 int pass=0,fail=0,decoded=0;
 for(int f=0;f<4;f++){
  u16 *fields=fieldsets[f];
  for(int type=1;type<=6;type++){
   u16 pkt[8],out[8];
   CoopBuildPacket(pkt,type,fields);
   if(!(CoopParsePacket(pkt,out)&&out[1]==fields[0]&&out[5]==fields[4]&&(out[0]&0xF)==type)){fail++;printf("clean f%d t%d FAIL\n",f,type);}else pass++;
   u16 foreigns[4]={0x5A5A,0x0000,0x8000|(3<<12)|0x777,0x8000|(6<<12)|0x111};
   for(int fo=0;fo<4;fo++){
    for(u32 off=0;off<9;off++){
     u16 stream[9],win[8];
     memcpy(stream,pkt,16); stream[8]=foreigns[fo];
     for(u32 j=0;j<8;j++)win[j]=stream[(off+j)%9];
     if(CoopParsePacket(win,out)){
      if(out[1]==fields[0]&&out[2]==fields[1]&&out[3]==fields[2]&&out[4]==fields[3]&&out[5]==fields[4]&&(out[0]&0xF)==type){pass++;decoded++;}
      else{fail++;printf("SILENT CORRUPTION f%d t%d fo%d off%u\n",f,type,fo,off);}
     } else pass++;
    }
   }
  }
 }
 printf("pass=%d fail=%d decodedUnderCorruption=%d\n",pass,fail,decoded);
 return fail!=0;
}
