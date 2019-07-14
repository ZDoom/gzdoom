#ifndef __BASICTYPES_H
#define __BASICTYPES_H

#include <stdint.h>

typedef uint32_t				BITFIELD;
typedef int						INTBOOL;

#if !defined(GUID_DEFINED)
#define GUID_DEFINED
typedef struct _GUID
{
    uint32_t Data1;
    uint16_t Data2;
	uint16_t Data3;
    uint8_t	Data4[8];
} GUID;
#endif

//
// fixed point, 32bit as 16.16.
//
#define FRACBITS						16
#define FRACUNIT						(1<<FRACBITS)

typedef int32_t							fixed_t;

#define FIXED_MAX						(signed)(0x7fffffff)
#define FIXED_MIN						(signed)(0x80000000)

// the last remnants of tables.h
#define ANGLE_90		(0x40000000)
#define ANGLE_180		(0x80000000)
#define ANGLE_270		(0xc0000000)
#define ANGLE_MAX		(0xffffffff)

typedef uint32_t			angle_t;


#ifdef __GNUC__
#define GCCPRINTF(stri,firstargi)		__attribute__((format(printf,stri,firstargi)))
#define GCCFORMAT(stri)					__attribute__((format(printf,stri,0)))
#define GCCNOWARN						__attribute__((unused))
#else
#define GCCPRINTF(a,b)
#define GCCFORMAT(a)
#define GCCNOWARN
#endif


#endif
