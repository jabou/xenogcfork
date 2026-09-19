/*
 * lzdec.h - decoder for the tiny LZSS format produced by tools/lzss.py
 *
 * Stream: control byte followed by 8 items (MSB first).
 *   bit 1 -> one literal byte
 *   bit 0 -> two bytes B1 B2: copy (B2 & 15) + 3 bytes from
 *            dst - (((B1 << 4) | (B2 >> 4)) + 1)
 * Copies may overlap (run-length style).  Shared by the GameCube stub and
 * the host round-trip test, so keep it plain C.
 */
#ifndef LZDEC_H
#define LZDEC_H

static void lz_decode(const unsigned char *src, unsigned char *dst, unsigned int outlen)
{
	unsigned char *end = dst + outlen;
	unsigned int ctrl = 0, bits = 0;

	while (dst < end) {
		if (!bits) {
			ctrl = *src++;
			bits = 8;
		}
		if (ctrl & 0x80) {
			*dst++ = *src++;
		} else {
			unsigned int b1 = *src++, b2 = *src++;
			unsigned int len = (b2 & 15) + 3;
			const unsigned char *p = dst - (((b1 << 4) | (b2 >> 4)) + 1);
			while (len--)
				*dst++ = *p++;
		}
		ctrl <<= 1;
		bits--;
	}
}

#endif
