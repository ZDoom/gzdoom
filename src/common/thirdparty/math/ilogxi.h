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

#ifndef __LZCNT__
#include "lzcnt.h"
#endif

	inline uint8_t ilogxid(int32_t i, uint32_t base)
	{
		assert(base > 1 && base < 32);
			return ((__bsrd(INT32_MAX) - __lzcnt32(i)) / (__bsrd(UINT32_MAX) - __lzcnt32(base)));
	}

	inline uint8_t ilogxiq(int64_t i, uint64_t base)
	{
		assert(base > 1 && base < 64);
			return ((__bsrq(INT64_MAX) - __lzcnt64(i)) / (__bsrq(UINT64_MAX) - __lzcnt64(base)));
	}

#ifdef __cplusplus
}
#endif
