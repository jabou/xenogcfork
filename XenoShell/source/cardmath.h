/*
 * cardmath.h - official Nintendo memory card unlock arithmetic.
 *
 * Compact re-implementation of the cipher helpers found in libogc's card.c
 * (exnor_1st, exnor, bitrev and the repeated "fold" blocks of __dounlock /
 * __dsp_donecallback).  Plain C with no GC specifics so it can also be compiled
 * on a host and compared against libogc's original code.
 */
#ifndef CARDMATH_H
#define CARDMATH_H

typedef unsigned int cm_u32;

/* LFSR stepping with right shifts: seeds the cipher from the array address. */
static cm_u32 cm_exnor_1st(cm_u32 a, cm_u32 n)
{
	while (n--) {
		cm_u32 r3 = ~((a >> 23) ^ ((a >> 15) ^ (a ^ (a >> 7))));
		a = (a >> 1) | ((r3 << 30) & 0x40000000u);
	}
	return a;
}

/* LFSR stepping with left shifts: used for every later cipher update. */
static cm_u32 cm_exnor(cm_u32 a, cm_u32 n)
{
	while (n--) {
		cm_u32 r3 = ~((a << 23) ^ ((a << 15) ^ (a ^ (a << 7))));
		a = (a << 1) | ((r3 >> 30) & 2u);
	}
	return a;
}

/* Final feedback bit after cm_exnor_1st (libogc: r1 = val | (r3 << 31)). */
static cm_u32 cm_fold_r(cm_u32 v)
{
	cm_u32 r3 = ~((v >> 23) ^ ((v >> 15) ^ (v ^ (v >> 7))));
	return v | (r3 << 31);
}

/* Final feedback bit after cm_exnor (libogc: r1 = val | (r3 >> 31)). */
static cm_u32 cm_fold_l(cm_u32 v)
{
	cm_u32 r3 = ~((v << 23) ^ ((v << 15) ^ (v ^ (v << 7))));
	return v | (r3 >> 31);
}

/* Plain 32-bit bit reversal (libogc's bitrev is exactly this). */
static cm_u32 cm_bitrev(cm_u32 v)
{
	cm_u32 r = 0;
	int i;
	for (i = 0; i < 32; i++) {
		r = (r << 1) | (v & 1u);
		v >>= 1;
	}
	return r;
}

#endif
