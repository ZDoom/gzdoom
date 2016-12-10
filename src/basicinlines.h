// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file is based on pragmas.h from Ken Silverman's original Build
// source code release but is meant for use with any compiler and does not
// rely on any inline assembly.
//

#if _MSC_VER
#pragma once
#endif

#if defined(__GNUC__) && !defined(__forceinline)
#define __forceinline __inline__ __attribute__((always_inline))
#endif

static __forceinline SDWORD Scale (SDWORD a, SDWORD b, SDWORD c)
{
	return (SDWORD)(((SQWORD)a*b)/c);
}

static __forceinline SDWORD MulScale14 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 14); }
static __forceinline SDWORD MulScale16 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 16); }
static __forceinline SDWORD MulScale30 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 30); }
static __forceinline SDWORD MulScale32 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 32); }

static __forceinline DWORD UMulScale16 (DWORD a, DWORD b) { return (DWORD)(((QWORD)a * b) >> 16); }

static __forceinline SDWORD DMulScale3 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 3); }
static __forceinline SDWORD DMulScale6 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 6); }
static __forceinline SDWORD DMulScale10 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 10); }
static __forceinline SDWORD DMulScale18 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 18); }
static __forceinline SDWORD DMulScale32 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 32); }

static inline SDWORD DivScale6 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 6) / b); }
static inline SDWORD DivScale16 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 16) / b); }
static inline SDWORD DivScale21 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 21) / b); }
static inline SDWORD DivScale30 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 30) / b); }

