#pragma once

#include <stdint.h>
#include <limits.h>

uint32_t lzcnt32_generic(uint32_t x);
uint32_t lzcnt64_generic(uint64_t x);

inline int32_t bsrd_generic(int32_t x)
{
	if(x == 0)
		return -1;
	else
		return (31 - lzcnt32_generic(x));
}

inline int64_t bsrq_generic(int64_t x)
{
	if(x == 0)
		return -1;
	else
		return (63 - lzcnt64_generic(x));
}

#undef __lzcnt32
#define __lzcnt32 lzcnt32_generic

#undef __lzcnt64
#define __lzcnt64 lzcnt64_generic

#undef __bsrd
#define __bsrd bsrd_generic

#undef __bsrq
#define __bsrq bsrq_generic

