/*
 * main.h - XenoBoot memory card loader: types, hardware registers and the
 *          on-card data structures (see libogc card.c / YAGCD 10.4).
 */
#ifndef MAIN_H
#define MAIN_H

typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

#define REG32(a) (*(volatile u32 *)(a))
#define REG16(a) (*(volatile u16 *)(a))

/* memory layout used by the loader */
#define LOADER_ADDRESS 0x81500000u  /* where stub.c unpacks this program to (see link.lds) */
#define HDR_ADDRESS   0x80700000u   /* card system area: blocks 0-4 (5 x 8 KB)   */
#define DOL_ADDRESS   0x80800000u   /* xeno.dol is staged here before launching */
#define MEM_FB        0xC0F00000u   /* framebuffer (uncached)                    */
#define MEM_FB2       (MEM_FB + 0x500u)
#define YBORDEROFFSET (640u * 2u * 32u)
#define GC_INIT_BASE  0x80000020u
#define VI_BASE       0xCC002000u
#define VI_BASE2      0xCC002040u
#define R_VIDEO_FRAMEBUFFER_1 REG32(0xCC00201Cu)
#define R_VIDEO_FRAMEBUFFER_2 REG32(0xCC002024u)

/* EXI channel register base: channel n at 0xCC006800 + n*0x14 */
#define EXI_CHANNEL_BASE(n) (0xCC006800u + (n) * 0x14u)
#define EXI_SPEED1MHZ  0
#define EXI_SPEED8MHZ  3
#define EXI_SPEED16MHZ 4
#define EXI_CSR_EXT    0x1000u      /* device present */

/* DSP registers (u16), offsets from 0xCC005000 */
#define DSP_REG(n)     REG16(0xCC005000u + (n) * 2u)
#define DSPCR_RES      0x0001u
#define DSPCR_PIINT    0x0002u
#define DSPCR_HALT     0x0004u
#define DSPCR_AIINT    0x0008u
#define DSPCR_ARINT    0x0020u
#define DSPCR_DSPINT   0x0080u
#define DSPCR_DSPRESET 0x0800u

/* memory card */
#define MEMCARD_BLOCK_SIZE 0x2000u
#define MEMCARD_PAGE_SIZE  0x200u
#define CARD_STATUS_UNLOCKED 0x40u
#define DIRECTORY_SIZE 127
#define LAST_BLOCK 0xFFFFu
#define XENO_DOL_NAME "xeno.dol"

struct DirectoryEntry_t {
	u32 gamecode;
	u16 makercode;
	u8  unused;
	u8  bannerFormat;
	char filename[32];
	u32 timestamp;
	u32 imageDataOffset;
	u16 iconFormat;
	u16 iconSpeed;
	u8  filePermission;
	u8  copyCounter;
	u16 firstBlockIndex;
	u16 fileLength;      /* in blocks, including the header block written by Swiss */
	u16 unused2;
	u32 commentOffset;
} __attribute__((__packed__));
typedef struct DirectoryEntry_t DirectoryEntry;

struct Directory_t {
	DirectoryEntry entries[DIRECTORY_SIZE];
	u8  padding[0x3A];
	u16 updateCounter;
	u16 checksum1;
	u16 checksum2;
} __attribute__((__packed__));
typedef struct Directory_t Directory;

struct Fat_t {
	u16 checksum1;
	u16 checksum2;
	u16 updateCounter;
	u16 numFreeBlocks;
	u16 lastAllocatedBlock;
	u16 blockAllocTable[0xFFB];   /* entry i describes block i + 5 */
} __attribute__((__packed__));
typedef struct Fat_t Fat;

typedef struct {
	u8        header[MEMCARD_BLOCK_SIZE];
	Directory directory1;
	Directory directory2;
	Fat       fat1;
	Fat       fat2;
} MemCard;

#endif
