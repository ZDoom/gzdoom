/** 
* @file  SFMT.c
* @brief SIMD oriented Fast Mersenne Twister(SFMT)
*
* @author Mutsuo Saito (Hiroshima University)
* @author Makoto Matsumoto (Hiroshima University)
*
* Copyright (C) 2006,2007 Mutsuo Saito, Makoto Matsumoto and Hiroshima
* University. All rights reserved.
*
* The new BSD License is applied to this software, see LICENSE.txt
*/
#include <string.h>
#include <assert.h>
#include "m_random.h"
#include "SFMT-params.h"

#if defined(__BIG_ENDIAN__) && !defined(__amd64) && !defined(BIG_ENDIAN64)
#define BIG_ENDIAN64 1
#endif
#if defined(HAVE_ALTIVEC) && !defined(BIG_ENDIAN64)
#define BIG_ENDIAN64 1
#endif
#if defined(ONLY64) && !defined(BIG_ENDIAN64)
#if defined(__GNUC__)
#error "-DONLY64 must be specified with -DBIG_ENDIAN64"
#endif
#undef ONLY64
#endif

/** a parity check vector which certificate the period of 2^{MEXP} */
static const DWORD parity[4] = { PARITY1, PARITY2, PARITY3, PARITY4 };

/*----------------
STATIC FUNCTIONS
----------------*/
inline static int idxof(int i);
inline static void rshift128(w128_t *out,  w128_t const *in, int shift);
inline static void lshift128(w128_t *out,  w128_t const *in, int shift);
inline static DWORD func1(DWORD x);
inline static DWORD func2(DWORD x);
#if defined(BIG_ENDIAN64) && !defined(ONLY64)
inline static void swap(w128_t *array, int size);
#endif

// These SIMD versions WILL NOT work as-is. I'm not even sure SSE2 is
// safe to provide as a runtime option without significant changes to
// how the state is stored, since the VC++ docs warn that:
//    Using variables of type __m128i will cause the compiler to generate
//    the SSE2 movdqa instruction. This instruction does not cause a fault
//    on Pentium III processors but will result in silent failure, with
//    possible side effects caused by whatever instructions movdqa
//    translates into on Pentium III processors.

#if defined(HAVE_ALTIVEC)
#include "SFMT-alti.h"
#elif defined(HAVE_SSE2)
#include "SFMT-sse2.h"
#endif

/**
* This function simulate a 64-bit index of LITTLE ENDIAN 
* in BIG ENDIAN machine.
*/
#ifdef ONLY64
inline static int idxof(int i) {
	return i ^ 1;
}
#else
inline static int idxof(int i) {
	return i;
}
#endif
/**
* This function simulates SIMD 128-bit right shift by the standard C.
* The 128-bit integer given in in is shifted by (shift * 8) bits.
* This function simulates the LITTLE ENDIAN SIMD.
* @param out the output of this function
* @param in the 128-bit data to be shifted
* @param shift the shift value
*/
#ifdef ONLY64
inline static void rshift128(w128_t *out, w128_t const *in, int shift) {
	QWORD th, tl, oh, ol;

	th = ((QWORD)in->u[2] << 32) | ((QWORD)in->u[3]);
	tl = ((QWORD)in->u[0] << 32) | ((QWORD)in->u[1]);

	oh = th >> (shift * 8);
	ol = tl >> (shift * 8);
	ol |= th << (64 - shift * 8);
	out->u[0] = (DWORD)(ol >> 32);
	out->u[1] = (DWORD)ol;
	out->u[2] = (DWORD)(oh >> 32);
	out->u[3] = (DWORD)oh;
}
#else
inline static void rshift128(w128_t *out, w128_t const *in, int shift) {
	QWORD th, tl, oh, ol;

	th = ((QWORD)in->u[3] << 32) | ((QWORD)in->u[2]);
	tl = ((QWORD)in->u[1] << 32) | ((QWORD)in->u[0]);

	oh = th >> (shift * 8);
	ol = tl >> (shift * 8);
	ol |= th << (64 - shift * 8);
	out->u[1] = (DWORD)(ol >> 32);
	out->u[0] = (DWORD)ol;
	out->u[3] = (DWORD)(oh >> 32);
	out->u[2] = (DWORD)oh;
}
#endif
/**
* This function simulates SIMD 128-bit left shift by the standard C.
* The 128-bit integer given in in is shifted by (shift * 8) bits.
* This function simulates the LITTLE ENDIAN SIMD.
* @param out the output of this function
* @param in the 128-bit data to be shifted
* @param shift the shift value
*/
#ifdef ONLY64
inline static void lshift128(w128_t *out, w128_t const *in, int shift) {
	QWORD th, tl, oh, ol;

	th = ((QWORD)in->u[2] << 32) | ((QWORD)in->u[3]);
	tl = ((QWORD)in->u[0] << 32) | ((QWORD)in->u[1]);

	oh = th << (shift * 8);
	ol = tl << (shift * 8);
	oh |= tl >> (64 - shift * 8);
	out->u[0] = (DWORD)(ol >> 32);
	out->u[1] = (DWORD)ol;
	out->u[2] = (DWORD)(oh >> 32);
	out->u[3] = (DWORD)oh;
}
#else
inline static void lshift128(w128_t *out, w128_t const *in, int shift) {
	QWORD th, tl, oh, ol;

	th = ((QWORD)in->u[3] << 32) | ((QWORD)in->u[2]);
	tl = ((QWORD)in->u[1] << 32) | ((QWORD)in->u[0]);

	oh = th << (shift * 8);
	ol = tl << (shift * 8);
	oh |= tl >> (64 - shift * 8);
	out->u[1] = (DWORD)(ol >> 32);
	out->u[0] = (DWORD)ol;
	out->u[3] = (DWORD)(oh >> 32);
	out->u[2] = (DWORD)oh;
}
#endif

/**
* This function represents the recursion formula.
* @param r output
* @param a a 128-bit part of the internal state array
* @param b a 128-bit part of the internal state array
* @param c a 128-bit part of the internal state array
* @param d a 128-bit part of the internal state array
*/
#if (!defined(HAVE_ALTIVEC)) && (!defined(HAVE_SSE2))
#ifdef ONLY64
inline static void do_recursion(w128_t *r, w128_t *a, w128_t *b, w128_t *c,
								w128_t *d) {
									w128_t x;
									w128_t y;

									lshift128(&x, a, SL2);
									rshift128(&y, c, SR2);
									r->u[0] = a->u[0] ^ x.u[0] ^ ((b->u[0] >> SR1) & MSK2) ^ y.u[0] 
									^ (d->u[0] << SL1);
									r->u[1] = a->u[1] ^ x.u[1] ^ ((b->u[1] >> SR1) & MSK1) ^ y.u[1] 
									^ (d->u[1] << SL1);
									r->u[2] = a->u[2] ^ x.u[2] ^ ((b->u[2] >> SR1) & MSK4) ^ y.u[2] 
									^ (d->u[2] << SL1);
									r->u[3] = a->u[3] ^ x.u[3] ^ ((b->u[3] >> SR1) & MSK3) ^ y.u[3] 
									^ (d->u[3] << SL1);
}
#else
inline static void do_recursion(w128_t *r, w128_t *a, w128_t *b, w128_t *c,
								w128_t *d) {
									w128_t x;
									w128_t y;

									lshift128(&x, a, SL2);
									rshift128(&y, c, SR2);
									r->u[0] = a->u[0] ^ x.u[0] ^ ((b->u[0] >> SR1) & MSK1) ^ y.u[0] 
									^ (d->u[0] << SL1);
									r->u[1] = a->u[1] ^ x.u[1] ^ ((b->u[1] >> SR1) & MSK2) ^ y.u[1] 
									^ (d->u[1] << SL1);
									r->u[2] = a->u[2] ^ x.u[2] ^ ((b->u[2] >> SR1) & MSK3) ^ y.u[2] 
									^ (d->u[2] << SL1);
									r->u[3] = a->u[3] ^ x.u[3] ^ ((b->u[3] >> SR1) & MSK4) ^ y.u[3] 
									^ (d->u[3] << SL1);
}
#endif
#endif

#if (!defined(HAVE_ALTIVEC)) && (!defined(HAVE_SSE2))
/**
* This function fills the internal state array with pseudorandom
* integers.
*/
void FRandom::GenRandAll()
{
	int i;
	w128_t *r1, *r2;

	r1 = &sfmt.w128[SFMT::N - 2];
	r2 = &sfmt.w128[SFMT::N - 1];
	for (i = 0; i < SFMT::N - POS1; i++) {
		do_recursion(&sfmt.w128[i], &sfmt.w128[i], &sfmt.w128[i + POS1], r1, r2);
		r1 = r2;
		r2 = &sfmt.w128[i];
	}
	for (; i < SFMT::N; i++) {
		do_recursion(&sfmt.w128[i], &sfmt.w128[i], &sfmt.w128[i + POS1 - SFMT::N], r1, r2);
		r1 = r2;
		r2 = &sfmt.w128[i];
	}
}

/**
* This function fills the user-specified array with pseudorandom
* integers.
*
* @param array an 128-bit array to be filled by pseudorandom numbers.  
* @param size number of 128-bit pseudorandom numbers to be generated.
*/
void FRandom::GenRandArray(w128_t *array, int size)
{
	int i, j;
	w128_t *r1, *r2;

	r1 = &sfmt.w128[SFMT::N - 2];
	r2 = &sfmt.w128[SFMT::N - 1];
	for (i = 0; i < SFMT::N - POS1; i++) {
		do_recursion(&array[i], &sfmt.w128[i], &sfmt.w128[i + POS1], r1, r2);
		r1 = r2;
		r2 = &array[i];
	}
	for (; i < SFMT::N; i++) {
		do_recursion(&array[i], &sfmt.w128[i], &array[i + POS1 - SFMT::N], r1, r2);
		r1 = r2;
		r2 = &array[i];
	}
	for (; i < size - SFMT::N; i++) {
		do_recursion(&array[i], &array[i - SFMT::N], &array[i + POS1 - SFMT::N], r1, r2);
		r1 = r2;
		r2 = &array[i];
	}
	for (j = 0; j < 2 * SFMT::N - size; j++) {
		sfmt.w128[j] = array[j + size - SFMT::N];
	}
	for (; i < size; i++, j++) {
		do_recursion(&array[i], &array[i - SFMT::N], &array[i + POS1 - SFMT::N], r1, r2);
		r1 = r2;
		r2 = &array[i];
		sfmt.w128[j] = array[i];
	}
}
#endif

#if defined(BIG_ENDIAN64) && !defined(ONLY64) && !defined(HAVE_ALTIVEC)
inline static void swap(w128_t *array, int size) {
	int i;
	DWORD x, y;

	for (i = 0; i < size; i++) {
		x = array[i].u[0];
		y = array[i].u[2];
		array[i].u[0] = array[i].u[1];
		array[i].u[2] = array[i].u[3];
		array[i].u[1] = x;
		array[i].u[3] = y;
	}
}
#endif
/**
* This function represents a function used in the initialization
* by init_by_array
* @param x 32-bit integer
* @return 32-bit integer
*/
static DWORD func1(DWORD x)
{
	return (x ^ (x >> 27)) * (DWORD)1664525UL;
}

/**
* This function represents a function used in the initialization
* by init_by_array
* @param x 32-bit integer
* @return 32-bit integer
*/
static DWORD func2(DWORD x)
{
	return (x ^ (x >> 27)) * (DWORD)1566083941UL;
}

/**
* This function certificate the period of 2^{MEXP}
*/
void FRandom::PeriodCertification()
{
	int inner = 0;
	int i, j;
	DWORD work;

	for (i = 0; i < 4; i++)
		inner ^= sfmt.u[idxof(i)] & parity[i];
	for (i = 16; i > 0; i >>= 1)
		inner ^= inner >> i;
	inner &= 1;
	/* check OK */
	if (inner == 1) {
		return;
	}
	/* check NG, and modification */
	for (i = 0; i < 4; i++) {
		work = 1;
		for (j = 0; j < 32; j++) {
			if ((work & parity[i]) != 0) {
				sfmt.u[idxof(i)] ^= work;
				return;
			}
			work = work << 1;
		}
	}
}

/*----------------
PUBLIC FUNCTIONS
----------------*/
/**
* This function returns the minimum size of array used for \b
* fill_array32() function.
* @return minimum size of array used for FillArray32() function.
*/
int FRandom::GetMinArraySize32()
{
	return SFMT::N32;
}

/**
* This function returns the minimum size of array used for \b
* fill_array64() function.
* @return minimum size of array used for FillArray64() function.
*/
int FRandom::GetMinArraySize64()
{
	return SFMT::N64;
}

#ifndef ONLY64
/**
* This function generates and returns 32-bit pseudorandom number.
* init_gen_rand or init_by_array must be called before this function.
* @return 32-bit pseudorandom number
*/
unsigned int FRandom::GenRand32()
{
	DWORD r;

	assert(initialized);
	if (idx >= SFMT::N32)
	{
		GenRandAll();
		idx = 0;
	}
	r = sfmt.u[idx++];
	return r;
}
#endif
/**
* This function generates and returns 64-bit pseudorandom number.
* init_gen_rand or init_by_array must be called before this function.
* The function gen_rand64 should not be called after gen_rand32,
* unless an initialization is again executed. 
* @return 64-bit pseudorandom number
*/
QWORD FRandom::GenRand64()
{
#if defined(BIG_ENDIAN64) && !defined(ONLY64)
	DWORD r1, r2;
#else
	QWORD r;
#endif

	assert(initialized);
	assert(idx % 2 == 0);

	if (idx >= SFMT::N32)
	{
		GenRandAll();
		idx = 0;
	}
#if defined(BIG_ENDIAN64) && !defined(ONLY64)
	r1 = sfmt.u[idx];
	r2 = sfmt.u[idx + 1];
	idx += 2;
	return ((QWORD)r2 << 32) | r1;
#else
	r = sfmt.u64[idx / 2];
	idx += 2;
	return r;
#endif
}

#ifndef ONLY64
/**
* This function generates pseudorandom 32-bit integers in the
* specified array[] by one call. The number of pseudorandom integers
* is specified by the argument size, which must be at least 624 and a
* multiple of four.  The generation by this function is much faster
* than the following gen_rand function.
*
* For initialization, init_gen_rand or init_by_array must be called
* before the first call of this function. This function can not be
* used after calling gen_rand function, without initialization.
*
* @param array an array where pseudorandom 32-bit integers are filled
* by this function.  The pointer to the array must be \b "aligned"
* (namely, must be a multiple of 16) in the SIMD version, since it
* refers to the address of a 128-bit integer.  In the standard C
* version, the pointer is arbitrary.
*
* @param size the number of 32-bit pseudorandom integers to be
* generated.  size must be a multiple of 4, and greater than or equal
* to (MEXP / 128 + 1) * 4.
*
* @note \b memalign or \b posix_memalign is available to get aligned
* memory. Mac OSX doesn't have these functions, but \b malloc of OSX
* returns the pointer to the aligned memory block.
*/
void FRandom::FillArray32(DWORD *array, int size)
{
	assert(initialized);
	assert(idx == SFMT::N32);
	assert(size % 4 == 0);
	assert(size >= SFMT::N32);

	GenRandArray((w128_t *)array, size / 4);
	idx = SFMT::N32;
}
#endif

/**
* This function generates pseudorandom 64-bit integers in the
* specified array[] by one call. The number of pseudorandom integers
* is specified by the argument size, which must be at least 312 and a
* multiple of two.  The generation by this function is much faster
* than the following gen_rand function.
*
* For initialization, init_gen_rand or init_by_array must be called
* before the first call of this function. This function can not be
* used after calling gen_rand function, without initialization.
*
* @param array an array where pseudorandom 64-bit integers are filled
* by this function.  The pointer to the array must be "aligned"
* (namely, must be a multiple of 16) in the SIMD version, since it
* refers to the address of a 128-bit integer.  In the standard C
* version, the pointer is arbitrary.
*
* @param size the number of 64-bit pseudorandom integers to be
* generated.  size must be a multiple of 2, and greater than or equal
* to (MEXP / 128 + 1) * 2
*
* @note \b memalign or \b posix_memalign is available to get aligned
* memory. Mac OSX doesn't have these functions, but \b malloc of OSX
* returns the pointer to the aligned memory block.
*/
void FRandom::FillArray64(QWORD *array, int size)
{
	assert(initialized);
	assert(idx == SFMT::N32);
	assert(size % 2 == 0);
	assert(size >= SFMT::N64);

	GenRandArray((w128_t *)array, size / 2);
	idx = SFMT::N32;

#if defined(BIG_ENDIAN64) && !defined(ONLY64)
	swap((w128_t *)array, size / 2);
#endif
}

/**
* This function initializes the internal state array with a 32-bit
* integer seed.
*
* @param seed a 32-bit integer used as the seed.
*/
void FRandom::InitGenRand(DWORD seed)
{
	int i;

	sfmt.u[idxof(0)] = seed;
	for (i = 1; i < SFMT::N32; i++)
	{
		sfmt.u[idxof(i)] = 1812433253UL * (sfmt.u[idxof(i - 1)] 
		^ (sfmt.u[idxof(i - 1)] >> 30))
			+ i;
	}
	idx = SFMT::N32;
	PeriodCertification();
#ifndef NDEBUG
	initialized = 1;
#endif
}

/**
* This function initializes the internal state array,
* with an array of 32-bit integers used as the seeds
* @param init_key the array of 32-bit integers, used as a seed.
* @param key_length the length of init_key.
*/
void FRandom::InitByArray(DWORD *init_key, int key_length)
{
	int i, j, count;
	DWORD r;
	int lag;
	int mid;
	int size = SFMT::N * 4;

	if (size >= 623) {
		lag = 11;
	} else if (size >= 68) {
		lag = 7;
	} else if (size >= 39) {
		lag = 5;
	} else {
		lag = 3;
	}
	mid = (size - lag) / 2;

	memset(&sfmt, 0x8b, sizeof(sfmt));
	if (key_length + 1 > SFMT::N32) {
		count = key_length + 1;
	} else {
		count = SFMT::N32;
	}
	r = func1(sfmt.u[idxof(0)] ^ sfmt.u[idxof(mid)] ^ sfmt.u[idxof(SFMT::N32 - 1)]);
	sfmt.u[idxof(mid)] += r;
	r += key_length;
	sfmt.u[idxof(mid + lag)] += r;
	sfmt.u[idxof(0)] = r;

	count--;
	for (i = 1, j = 0; (j < count) && (j < key_length); j++)
	{
		r = func1(sfmt.u[idxof(i)] ^ sfmt.u[idxof((i + mid) % SFMT::N32)] ^ sfmt.u[idxof((i + SFMT::N32 - 1) % SFMT::N32)]);
		sfmt.u[idxof((i + mid) % SFMT::N32)] += r;
		r += init_key[j] + i;
		sfmt.u[idxof((i + mid + lag) % SFMT::N32)] += r;
		sfmt.u[idxof(i)] = r;
		i = (i + 1) % SFMT::N32;
	}
	for (; j < count; j++)
	{
		r = func1(sfmt.u[idxof(i)] ^ sfmt.u[idxof((i + mid) % SFMT::N32)] ^ sfmt.u[idxof((i + SFMT::N32 - 1) % SFMT::N32)]);
		sfmt.u[idxof((i + mid) % SFMT::N32)] += r;
		r += i;
		sfmt.u[idxof((i + mid + lag) % SFMT::N32)] += r;
		sfmt.u[idxof(i)] = r;
		i = (i + 1) % SFMT::N32;
	}
	for (j = 0; j < SFMT::N32; j++)
	{
		r = func2(sfmt.u[idxof(i)] + sfmt.u[idxof((i + mid) % SFMT::N32)] + sfmt.u[idxof((i + SFMT::N32 - 1) % SFMT::N32)]);
		sfmt.u[idxof((i + mid) % SFMT::N32)] ^= r;
		r -= i;
		sfmt.u[idxof((i + mid + lag) % SFMT::N32)] ^= r;
		sfmt.u[idxof(i)] = r;
		i = (i + 1) % SFMT::N32;
	}

	idx = SFMT::N32;
	PeriodCertification();
#ifndef NDEBUG
	initialized = 1;
#endif
}
