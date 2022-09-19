#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#if defined __i386__ || __x86_64__
#include <x86gprintrin.h>
#endif

extern inline int8_t ilogxid(int32_t i, int8_t base)
{
	assert(base > 1 && base < 32);
	#if __has_builtin(__builtin_clz) || defined(__GNUC__)
		return ((31 - __builtin_clz(i)) / (31 - __builtin_clz(base)));
	#else
		#error "__builtin_clz instruction not supported!"
	#endif
}

extern inline int8_t ilogxiq(int64_t i, int8_t base)
{
	assert(base > 1 && base < 64);
	#if __WORDSIZE == 64
		#if __has_builtin(__builtin_clzl) || defined(__GNUC__)
		return ((63 - __builtin_clzl(i)) / (63 - __builtin_clzl(base)));
		#else
			#error "__builtin_clzl instruction not supported!"
		#endif
	#else
		#if __has_builtin(__builtin_clzll) || defined(__GNUC__)
		return ((63 - __builtin_clzll(i)) / (63 - __builtin_clzll(base)));
		#else
			#error "__builtin_clzll instruction not supported!"
		#endif
	#endif
}

#ifdef __cplusplus
}
#endif
