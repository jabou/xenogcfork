/**
 * XenoBoot v2 - memory card DOL loader for the XenoGC drivecode
 *
 * Originally XenoShell by cheqmate/adhs, continued by gc-forever.com members
 * (emu_kidid, ...).  XenoBoot memory card loader by vingt-2.
 *
 * v2 changes:
 *  - Official Nintendo memory cards power up LOCKED and return scrambled data
 *    until the DSP-assisted unlock handshake has been performed.  v1 skipped
 *    that step, so on official cards (e.g. DOL-020 "1019") both directory
 *    checksums failed and the loader reported "Could not find xeno.dol".
 *    v2 reads the card ID/status and performs the same unlock libogc does.
 *  - Uses a small built-in font instead of the IPL font and leaves the IPL's
 *    low-memory values alone (libogc rebuilds them anyway).  The whole payload
 *    has to share the ATmega8 flash with the drivecode, so size matters.
 *
 * After a successful unlock the card's flash ID and its checksum are stored in
 * SRAM exactly like libogc/the SDK do (__card_domount), because a mount of an
 * already unlocked card verifies them and fails otherwise.
 */

#include "main.h"
#include "cardmath.h"
#include "font5x7.h"

/*** externs from boot.s ***/
extern u32 GetMSR(void);
extern void SetMSR(u32);
extern void dcache_flush_icache_inv(void *, int);
extern u32 __bss_start[], __bss_end[];

#define NOINLINE __attribute__((noinline))

static void memset32(u32 *d, u32 v, u32 n)
{
	while (n--)
		*d++ = v;
}

static u32 mftb_lo(void)
{
	u32 v;
	asm volatile("mftb %0" : "=r"(v));
	return v;
}

/*** video (NTSC timing, as XenoBoot v1 used) ***/

static const u32 VI_Regs[32] = {
	0x0F060001, 0x476901AD, 0x02EA5140, 0x00030018,
	0x00020019, 0x410C410C, 0x40ED40ED, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x110701AE, 0x10010001, 0x00010001, 0x00010001,
	0x00000000, 0x00000000, 0x28500100, 0x1AE771F0,
	0x0DB4A574, 0x00C1188E, 0xC4C0CBE2, 0xFCECDECF,
	0x13130F08, 0x00080C0F, 0x00FF0000, 0x00000000,
	0x02800000, 0x000000FF, 0x00FF00FF, 0x00FF00FF
};

/*** text output: 5x7 font, each font pixel = 2x2 screen pixels ***/

#define TEXT_COLS 50
#define COL_WHITE 0xEB80EB80u
#define COL_BLACK 0x10801080u

static u32 cur_x, cur_y;

static void cls(void)
{
	memset32((u32 *)MEM_FB2, COL_BLACK, (640 * 480) / 2);
	cur_x = cur_y = 0;
}

static NOINLINE void putch(char c)
{
	const u8 *g;
	u32 *p;
	int col, row;

	if (c == '\n') {
		cur_x = 0;
		cur_y++;
		return;
	}
	if (cur_x >= TEXT_COLS) {
		cur_x = 0;
		cur_y++;
	}
	cur_x++;
	if (c == ' ')
		return;
	if (c >= 'a' && c <= 'z')
		c -= 0x20;
	if ((u8)c < FONT_FIRST || (u8)c > FONT_LAST)
		c = '?';

	g = font5x7 + ((u8)c - FONT_FIRST) * 5;
	p = (u32 *)(MEM_FB2 + YBORDEROFFSET) + cur_y * 16 * 320 + 16 + (cur_x - 1) * 6;
	for (row = 0; row < 7; row++, p += 640) {
		for (col = 0; col < 5; col++) {
			u32 v = (g[col] >> row) & 1 ? COL_WHITE : COL_BLACK;
			p[col] = v;
			p[col + 320] = v;
		}
	}
}

static NOINLINE void print(const char *s)
{
	while (*s)
		putch(*s++);
}

/* print a label followed by a hex value */
static NOINLINE void printv(const char *s, u32 v, int digits)
{
	print(s);
	while (digits--)
		putch("0123456789ABCDEF"[(v >> (digits * 4)) & 0xF]);
}

/*** EXI (memory card slot) access ***/

static volatile u32 *exi;	/* register base of the channel in use */
#define EXI_CSR  exi[0]
#define EXI_CR   exi[3]
#define EXI_DATA exi[4]

static void exi_select(u32 freq)
{
	EXI_CSR = (EXI_CSR & 0x405) | 0x80 | (freq << 4);	/* device 0 */
}

static void exi_deselect(void)
{
	EXI_CSR = EXI_CSR & 0x405;
}

/* immediate write of 1..4 bytes, MSB first */
static NOINLINE void exi_imm_write(u32 data, u32 len)
{
	EXI_DATA = data;
	EXI_CR = ((len - 1) << 4) | 0x05;
	while (EXI_CR & 1);
}

/* immediate read of 1..4 bytes, returned MSB first */
static NOINLINE u32 exi_imm_read(u32 len)
{
	EXI_CR = ((len - 1) << 4) | 0x01;
	while (EXI_CR & 1);
	return EXI_DATA;
}

/* start a write to the SRAM/RTC chip at byte offset loc (EXI channel 0, device 1, 8 MHz), libogc __sram_write */
static NOINLINE void sram_cmd(u32 loc)
{
	exi = (volatile u32 *)EXI_CHANNEL_BASE(0);
	EXI_CSR = (EXI_CSR & 0x405) | 0x100 | (EXI_SPEED8MHZ << 4);
	exi_imm_write(0xA0000100u + (loc << 6), 4);
}

/*** memory card commands (see libogc card.c) ***/

static u32 card_latency;	/* dummy bytes between read command and data */

static u32 mc_read_id(void)
{
	u32 id;
	exi_select(EXI_SPEED1MHZ);
	exi_imm_write(0x00000000, 2);
	id = exi_imm_read(4);
	exi_deselect();
	return id;
}

/* 0x83 = read status, 0x89 = clear status */
static NOINLINE u8 mc_status(u32 cmd)
{
	u32 v = 0;
	exi_select(EXI_SPEED16MHZ);
	exi_imm_write(cmd << 24, cmd == 0x83 ? 2 : 1);
	if (cmd == 0x83)
		v = exi_imm_read(1);
	exi_deselect();
	return v >> 24;
}

/* send a 5 byte read command, skip the latency bytes and read len bytes */
static NOINLINE void mc_cmd_read(u32 cmd0_3, u32 cmd4, u8 *dst, u32 len)
{
	u32 l;
	exi_select(EXI_SPEED16MHZ);
	exi_imm_write(cmd0_3, 4);
	exi_imm_write(cmd4 << 24, 1);
	for (l = 0; l < card_latency; l += 4)
		exi_imm_write(0, 4);
	while (len) {
		u32 n = len > 4 ? 4 : len, v = exi_imm_read(n), i;
		for (i = 0; i < n; i++)
			*dst++ = v >> (24 - 8 * i);
		len -= n;
	}
	exi_deselect();
}

static NOINLINE void mc_read_block(u32 block, u8 *dst)
{
	u32 off = block * MEMCARD_BLOCK_SIZE, end = off + MEMCARD_BLOCK_SIZE;
	for (; off < end; off += MEMCARD_PAGE_SIZE, dst += MEMCARD_PAGE_SIZE)
		mc_cmd_read(0x52000000 | (((off >> 17) & 0x7F) << 16) | (((off >> 9) & 0xFF) << 8) | ((off >> 7) & 3),
			    off & 0x7F, dst, MEMCARD_PAGE_SIZE);
}

/* the special reads used by the unlock handshake (libogc __card_readarrayunlock) */
static NOINLINE void mc_readarray_unlock(u32 addr, u8 *dst, u32 len, int flag)
{
	addr &= 0xFFFFF000u;
	if (!flag)
		mc_cmd_read(0x52000000 | (((addr >> 29) & 3) << 16) | (((addr >> 21) & 0xFF) << 8) | ((addr >> 19) & 3),
			    (addr >> 12) & 0x7F, dst, len);
	else
		mc_cmd_read(0x52000000 | ((addr >> 24) << 16) | (((addr >> 16) & 0xFF) << 8), 0, dst, len);
}

/*** DSP: runs Nintendo's card unlock microcode (libogc _cardunlockdata, GC version) ***/

static const u32 dsp_ucode[88] __attribute__((aligned(32))) = {
	0x00000000,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000021,0x02ff0021,
	0x13061203,0x12041305,0x009200ff,0x0088ffff,
	0x0089ffff,0x008affff,0x008bffff,0x8f0002bf,
	0x008816fc,0xdcd116fd,0x000016fb,0x000102bf,
	0x008e25ff,0x0380ff00,0x02940027,0x02bf008e,
	0x1fdf24ff,0x02400fff,0x00980400,0x009a0010,
	0x00990000,0x8e0002bf,0x009402bf,0x864402bf,
	0x008816fc,0xdcd116fd,0x000316fb,0x00018f00,
	0x02bf008e,0x0380cdd1,0x02940048,0x27ff0380,
	0x00010295,0x005a0380,0x00020295,0x8000029f,
	0x00480021,0x8e0002bf,0x008e25ff,0x02bf008e,
	0x25ff02bf,0x008e25ff,0x02bf008e,0x00c5ffff,
	0x03400fff,0x1c9f02bf,0x008e00c7,0xffff02bf,
	0x008e00c6,0xffff02bf,0x008e00c0,0xffff02bf,
	0x008e20ff,0x03400fff,0x1f5f02bf,0x008e21ff,
	0x02bf008e,0x23ff1205,0x1206029f,0x80b50021,
	0x27fc03c0,0x8000029d,0x008802df,0x27fe03c0,
	0x8000029c,0x008e02df,0x2ece2ccf,0x00f8ffcd,
	0x00f9ffc9,0x00faffcb,0x26c902c0,0x0004029d,
	0x009c02df,0x00000000,0x00000000,0x00000000,
	0x00000000,0x00000000,0x00000000,0x00000000
};

/* boot mails sent to the DSP ROM (libogc __dsp_boottask); entry 1 is patched at runtime */
static const u32 dsp_boot_mails[10] = {
	0x80F3A001, 0, 0x80F3C002, 0, 0x80F3A002, sizeof(dsp_ucode),
	0x80F3B002, 0, 0x80F3D001, 16
};

#define DSP_TIMEOUT 0x04000000u

static NOINLINE void dsp_send(u32 mail)
{
	u32 t = DSP_TIMEOUT;
	DSP_REG(0) = mail >> 16;
	DSP_REG(1) = mail & 0xFFFF;
	while ((DSP_REG(0) & 0x8000) && --t);
}

/* wait for a mail from the DSP; 0xFFFFFFFF on timeout */
static NOINLINE u32 dsp_recv(void)
{
	u32 t = DSP_TIMEOUT, hi;
	while (!(DSP_REG(2) & 0x8000))
		if (!--t) return 0xFFFFFFFFu;
	hi = DSP_REG(2);
	return (hi << 16) | (DSP_REG(3) & 0xFFFF);
}

/* work area handed to the ucode: [0..7] parameters, [8..15] input, [16..23] output */
static u32 unlock_buf[24] __attribute__((aligned(32)));
/* the card's 96 bit flash ID recovered during the unlock (libogc card->key), goes to SRAM */
static u32 flash_id[3];

/* returns the mail that did not match, or 0 on success */
static NOINLINE u32 dsp_run_unlock(void)
{
	int i;
	u32 m;

	DSP_REG(5) = (DSP_REG(5) & ~(DSPCR_AIINT | DSPCR_ARINT | DSPCR_DSPINT)) | DSPCR_DSPRESET;
	DSP_REG(5) = DSP_REG(5) & ~(DSPCR_HALT | DSPCR_AIINT | DSPCR_ARINT | DSPCR_DSPINT);

	if ((m = dsp_recv()) != 0x8071FEED)
		return m | 1;
	for (i = 0; i < 10; i++)
		dsp_send(i == 1 ? ((u32)dsp_ucode & 0x3FFFFFFF) : dsp_boot_mails[i]);
	if ((m = dsp_recv()) != 0xDCD10000)
		return m | 1;
	dsp_send(0xFF000000);
	dsp_send((u32)unlock_buf);
	if ((m = dsp_recv()) != 0xDCD10003)
		return m | 1;
	dsp_send(0xCDD10002);
	return 0;
}

/*** the unlock handshake itself (libogc __dounlock + __dsp_donecallback) ***/

static NOINLINE u32 dummylen(void)
{
	u32 l = (mftb_lo() & 0x1F) + 1;
	return l < 4 ? 4 : l;
}

static u32 cipher_step(u32 cipher, u32 n)
{
	return cm_fold_l(cm_exnor(cipher, n));
}

/* returns 0 on success, otherwise the offending DSP mail */
static NOINLINE u32 mc_unlock(void)
{
	u8 tmp[64] __attribute__((aligned(4)));
	u32 *w = (u32 *)tmp;
	u32 *work = unlock_buf, *cipher1 = unlock_buf + 8, *cipher2 = unlock_buf + 16;
	u32 cipher, len, key, m;
	u32 array_addr = 0x7FEC8000u | ((mftb_lo() & 7) << 12);

	len = dummylen();
	mc_readarray_unlock(array_addr, tmp, len, 0);
	cipher = cm_bitrev(cm_fold_r(cm_exnor_1st(array_addr, (len << 3) + 1)));

	len = dummylen();
	mc_readarray_unlock(0, tmp, len + 20, 1);
	/* libogc __dounlock: a,b,c (= the card's flash ID) and d,e (fed to the DSP) are w[0..4]
	   each XORed with the cipher, with a 32 step advance in between */
	for (key = 0; key < 5; key++) {
		u32 v = w[key] ^ cipher;
		if (key < 3)
			flash_id[key] = v;
		else
			cipher1[key - 3] = v;
		if (key < 4)
			cipher = cipher_step(cipher, 32);
	}
	cipher = cipher_step(cipher, len << 3);
	cipher = cipher_step(cipher, 33);

	cipher2[0] = 0;
	work[0] = (u32)cipher1;
	work[1] = 8;
	work[2] = 0;
	work[3] = (u32)cipher2;
	dcache_flush_icache_inv(unlock_buf, sizeof(unlock_buf));
	dcache_flush_icache_inv((void *)dsp_ucode, sizeof(dsp_ucode));

	if ((m = dsp_run_unlock()) != 0)
		return m;

	dcache_flush_icache_inv(cipher2, 32);
	key = cipher2[0];

	len = dummylen();
	mc_readarray_unlock((key ^ cipher) & ~0xFFFFu, tmp, len, 1);
	cipher = cipher_step(cipher, ((len + card_latency + 4) << 3) + 1);

	len = dummylen();
	mc_readarray_unlock(((key << 16) ^ cipher) & ~0xFFFFu, tmp, len, 1);
	return 0;
}

/* libogc and the SDK trust the flash ID stored in SRAM once a card reports "unlocked"
   (__card_domount checks it and fails the mount otherwise), so store it like they do.
   Leaves `exi` on channel 0. */
static NOINLINE void store_flash_id(int chn)
{
	u32 k, sum = 0;
	for (k = 0; k < 12; k++)
		sum += ((u8 *)flash_id)[k];
	sram_cmd(0x14 + 12 * chn);
	for (k = 0; k < 3; k++)
		exi_imm_write(flash_id[k], 4);
	exi_deselect();
	sram_cmd(0x3A + chn);
	exi_imm_write(~sum << 24, 1);
	exi_deselect();
}

/*** card file system ***/

/* verify a system block: cs points at the two checksum words, data at what they cover */
static NOINLINE int checksum_ok(const u16 *data, const u16 *cs)
{
	u16 cs1 = 0, cs2 = 0;
	u32 i;
	for (i = 0; i < (MEMCARD_BLOCK_SIZE - 4) / 2; i++) {
		cs1 += data[i];
		cs2 += data[i] ^ 0xFFFF;
	}
	if (cs1 == 0xFFFF) cs1 = 0;
	if (cs2 == 0xFFFF) cs2 = 0;
	return cs1 == cs[0] && cs2 == cs[1];
}

/* pick the newest valid copy of a system block pair (copy 2 follows copy 1) */
static NOINLINE void *pick_block(u8 *a, u32 cs_off, u32 counter_off)
{
	u8 *b = a + MEMCARD_BLOCK_SIZE;
	u32 data_off = cs_off ? 0 : 4;
	int ga = checksum_ok((u16 *)(a + data_off), (u16 *)(a + cs_off));
	int gb = checksum_ok((u16 *)(b + data_off), (u16 *)(b + cs_off));
	if (ga && gb)
		return *(u16 *)(a + counter_off) > *(u16 *)(b + counter_off) ? a : b;
	return ga ? a : gb ? b : 0;
}

/*** DOL loading ***/

struct dol_s {
	u32 sec_pos[18];
	u32 sec_address[18];
	u32 sec_size[18];
	u32 bss_address, bss_size, entry_point;
};

static NOINLINE int looks_like_dol(const void *p)
{
	const struct dol_s *d = p;
	return d->sec_pos[0] >= 0xE4 && d->sec_pos[0] < 0x100000 && (d->entry_point >> 24) == 0x80 &&
	       (d->sec_address[0] >> 24) == 0x80;
}

static void LoadDolAtAddress(void *dol)
{
	struct dol_s *d = dol;
	int i;
	u32 n;
	u8 *dst;
	void (*entrypoint)(void);

	for (i = 0; i < 18; ++i) {
		u8 *src = (u8 *)dol + d->sec_pos[i];
		n = d->sec_size[i];
		dst = (u8 *)d->sec_address[i];
		if (!n)
			continue;
		while (n--)
			*dst++ = *src++;
		dcache_flush_icache_inv((void *)d->sec_address[i], d->sec_size[i]);
	}
	n = d->bss_size;
	dst = (u8 *)d->bss_address;
	while (n--)
		*dst++ = 0;

	SetMSR((GetMSR() | 2) & ~0x8000);
	SetMSR(GetMSR() & ~0x8000);	/* EE off */
	SetMSR(GetMSR() | 0x2002);	/* FP, RI */

	entrypoint = (void (*)(void))d->entry_point;
	entrypoint();
}

/*** main flow ***/

static NOINLINE void boot_entry(DirectoryEntry *e, Fat *fat)
{
	u16 blk = e->firstBlockIndex, n = e->fileLength, c;
	u8 *dst = (u8 *)DOL_ADDRESS, *dol;

	printv("\nBLK ", n, 4);
	printv(" @ ", blk, 4);
	for (c = 0; c < n && blk >= 5 && blk - 5 < 0xFFB; c++) {
		mc_read_block(blk, dst + c * MEMCARD_BLOCK_SIZE);
		blk = fat->blockAllocTable[blk - 5];
	}
	if (c != n) {			/* chain ended before fileLength blocks: do not boot half a file */
		print("\nBAD FAT\n");
		return;
	}
	/* Swiss stores its own header (name, time, icon) in the first block */
	dol = looks_like_dol(dst) ? dst : dst + MEMCARD_BLOCK_SIZE;
	if (!looks_like_dol(dol)) {
		print("\nNOT A DOL\n");
		return;
	}
	printv("\nBOOT ", ((struct dol_s *)dol)->entry_point, 8);
	LoadDolAtAddress(dol);
}

static void try_slot(int chn)
{
	u32 id, i, st, files = 0;
	u8 *hdr = (u8 *)HDR_ADDRESS;
	MemCard *mc = (MemCard *)hdr;
	Directory *dir;
	Fat *fat;
	DirectoryEntry *found = 0;

	exi = (volatile u32 *)EXI_CHANNEL_BASE(chn);
	print(chn ? "\nB: " : "\nA: ");
	if (!(EXI_CSR & EXI_CSR_EXT)) {
		print("EMPTY");
		return;
	}

	id = mc_read_id();
	printv("ID=", id, 8);
	if ((id >> 16) || !(id & 0xFC)) {
		print(" NO CARD");
		return;
	}
	card_latency = 4u << ((id >> 8) & 7);

	mc_status(0x89);
	st = mc_status(0x83);
	printv(" ST=", st, 2);
	if (!(st & CARD_STATUS_UNLOCKED)) {
		if ((i = mc_unlock()) != 0) {
			printv("\nDSP ", i, 8);
			return;
		}
		st = mc_status(0x83);
		printv("\nUNLOCK ST=", st, 2);
		if (!(st & CARD_STATUS_UNLOCKED))
			return;
		store_flash_id(chn);
		exi = (volatile u32 *)EXI_CHANNEL_BASE(chn);
	}

	for (i = 0; i < 5; i++)
		mc_read_block(i, hdr + i * MEMCARD_BLOCK_SIZE);

	dir = pick_block((u8 *)&mc->directory1, 0x1FFC, 0x1FFA);
	fat = pick_block((u8 *)&mc->fat1, 0, 4);
	printv("\nDIR ", dir != 0, 1);
	printv(" FAT ", fat != 0, 1);
	if (!dir || !fat)
		return;

	for (i = 0; i < DIRECTORY_SIZE; i++) {
		DirectoryEntry *e = &dir->entries[i];
		int k;
		if (e->gamecode == 0xFFFFFFFFu)
			continue;
		files++;
		for (k = 0; k < 9 && e->filename[k] == XENO_DOL_NAME[k]; k++);
		if (k == 9)
			found = e;
	}
	printv(" FILES ", files, 2);
	if (found)
		boot_entry(found, fat);
}

int main(void)
{
	SetMSR(GetMSR() & ~0x8000);		/* no interrupts while we poll hardware */
	memset32(__bss_start, 0, __bss_end - __bss_start);
	REG32(0x80000C00) = 0x4C000064;		/* rfi: system call handler used by the cache flush */

	for (int i = 0; i < 32; i++)
		REG32(VI_BASE + i * 4) = VI_Regs[i];	/* VI_BASE2 follows VI_BASE directly */
	R_VIDEO_FRAMEBUFFER_1 = MEM_FB;
	R_VIDEO_FRAMEBUFFER_2 = MEM_FB2;
	cls();
	print("XENOBOOT V2");

	try_slot(1);
	try_slot(0);

	print("\n\nNO BOOTABLE ");
	print(XENO_DOL_NAME);
	while (1);
	return 0;
}
