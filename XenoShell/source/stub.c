/*
 * stub.c - first stage of the XenoBoot payload.
 *
 * The drivecode copies the whole payload to 0x81700000 and jumps to it.  The
 * ATmega8 flash is tight, so the real loader (main.c) is stored LZSS
 * compressed right behind this stub.  We unpack it to LOADER_ADDRESS, flush
 * the caches and jump to its entry point (_start = LOADER_ADDRESS).
 */
#include "lzdec.h"

#define LOADER_ADDRESS 0x81500000u

extern const unsigned char payload_lz[], payload_lz_end[];
extern const unsigned int payload_len;

void stub_main(void)
{
	unsigned char *dst = (unsigned char *)LOADER_ADDRESS;
	unsigned int a, len = payload_len;

	lz_decode(payload_lz, dst, len);

	for (a = LOADER_ADDRESS & ~31u; a < LOADER_ADDRESS + len; a += 32) {
		asm volatile("dcbf 0,%0" :: "r"(a) : "memory");
		asm volatile("icbi 0,%0" :: "r"(a) : "memory");
	}
	asm volatile("sync; isync" ::: "memory");

	((void (*)(void))LOADER_ADDRESS)();
}
