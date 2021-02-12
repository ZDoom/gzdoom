/** 
 * @file SFMT.h 
 *
 * @brief SIMD oriented Fast Mersenne Twister(SFMT) pseudorandom
 * number generator
 *
 * @author Mutsuo Saito (Hiroshima University)
 * @author Makoto Matsumoto (Hiroshima University)
 *
 * Copyright (C) 2006, 2007 Mutsuo Saito, Makoto Matsumoto and Hiroshima
 * University. All rights reserved.
 *
 * The new BSD License is applied to this software.
 * see LICENSE.txt
 *
 * @note We assume that your system has inttypes.h.  If your system
 * doesn't have inttypes.h, you have to typedef uint32_t and uint64_t,
 * and you have to define PRIu64 and PRIx64 in this file as follows:
 * @verbatim
 typedef unsigned int uint32_t
 typedef unsigned long long uint64_t  
 #define PRIu64 "llu"
 #define PRIx64 "llx"
@endverbatim
 * uint32_t must be exactly 32-bit unsigned integer type (no more, no
 * less), and uint64_t must be exactly 64-bit unsigned integer type.
 * PRIu64 and PRIx64 are used for printf function to print 64-bit
 * unsigned int and 64-bit unsigned int in hexadecimal format.
 */

#include <inttypes.h>

#ifndef SFMT_H
#define SFMT_H

#ifndef PRIu64
  #if defined(_MSC_VER) || defined(__BORLANDC__)
    #define PRIu64 "I64u"
    #define PRIx64 "I64x"
  #else
    #define PRIu64 "llu"
    #define PRIx64 "llx"
  #endif
#endif

#if defined(__GNUC__)
#define ALWAYSINLINE __attribute__((always_inline))
#else
#define ALWAYSINLINE
#endif

#if defined(_MSC_VER)
  #if _MSC_VER >= 1200
    #define PRE_ALWAYS __forceinline
  #else
    #define PRE_ALWAYS inline
  #endif
#else
  #define PRE_ALWAYS inline
#endif

/*------------------------------------------------------
  128-bit SIMD data type for Altivec, SSE2 or standard C
  ------------------------------------------------------*/
#if defined(HAVE_ALTIVEC)
  #if !defined(__APPLE__)
    #include <altivec.h>
  #endif
/** 128-bit data structure */
union w128_t {
    vector unsigned int s;
    uint32_t u[4];
	uint64_t u64[2];
};

#elif defined(HAVE_SSE2)
  #include <emmintrin.h>

/** 128-bit data structure */
union w128_t {
    __m128i si;
    uint32_t u[4];
	uint64_t u64[2];
};

#else

/** 128-bit data structure */
union w128_t {
    uint32_t u[4];
	uint64_t u64[2];
};

#endif

/*-----------------
  BASIC DEFINITIONS
  -----------------*/

/** Mersenne Exponent. The period of the sequence 
 *  is a multiple of 2^MEXP-1. */
#if !defined(MEXP)
  // [RH] The default MEXP for SFMT is 19937, but since that consumes
  // quite a lot of space for state, and we're using lots of different
  // RNGs, default to something smaller.
  #define MEXP 607
#endif

namespace SFMT
{
/** SFMT generator has an internal state array of 128-bit integers,
 * and N is its size. */
enum { N = MEXP / 128 + 1 };
/** N32 is the size of internal state array when regarded as an array
 * of 32-bit integers.*/
enum { N32 = N * 4 };
/** N64 is the size of internal state array when regarded as an array
 * of 64-bit integers.*/
enum { N64 = N * 2 };
};

#endif
