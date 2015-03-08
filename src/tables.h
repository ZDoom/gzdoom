// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		Lookup tables.
//		Do not try to look them up :-).
//		In the order of appearance: 
//
//		int finetangent[4096]	- Tangens LUT.
//		 Should work with BAM fairly well (12 of 16bit,
//		effectively, by shifting).
//
//		int finesine[10240] 			- Sine lookup.
//		 Guess what, serves as cosine, too.
//		 Remarkable thing is, how to use BAMs with this? 
//
//		int tantoangle[2049]	- ArcTan LUT,
//		  maps tan(angle) to angle fast. Gotta search.	
//	  
//-----------------------------------------------------------------------------


#ifndef __TABLES_H__
#define __TABLES_H__

#include <stdlib.h>
#include <math.h>
#include "basictypes.h"

#ifndef PI
#define PI				3.14159265358979323846		// matches value in gcc v2 math.h
#endif


#define FINEANGLEBITS	13
#define FINEANGLES		8192
#define FINEMASK		(FINEANGLES-1)

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT		19

#define BOBTOFINESHIFT			(FINEANGLEBITS - 6)

// Effective size is 10240.
extern	fixed_t 		finesine[5*FINEANGLES/4];

// Re-use data, is just PI/2 phase shift.
// [RH] Instead of using a pointer, use some inline code
// (encapsulated in a struct so that we can still use array accesses).
struct cosine_inline
{
	fixed_t operator[] (unsigned int x) const
	{
		return finesine[x+FINEANGLES/4];
	}
};
extern cosine_inline finecosine;

// Effective size is 4096.
extern fixed_t			finetangent[FINEANGLES/2];

// Binary Angle Measument, BAM.
#define ANG45			(0x20000000)
#define ANG90			(0x40000000)
#define ANG180			(0x80000000)
#define ANG270			(0xc0000000)

#define ANGLE_45		(0x20000000)
#define ANGLE_90		(0x40000000)
#define ANGLE_180		(0x80000000)
#define ANGLE_270		(0xc0000000)
#define ANGLE_MAX		(0xffffffff)
#define ANGLE_1			(ANGLE_45/45)
#define ANGLE_60		(ANGLE_180/3)


#define SLOPERANGE		2048
#define SLOPEBITS		11
#define DBITS			(FRACBITS-SLOPEBITS)

typedef uint32			angle_t;

// Previously seen all over the place, code like this:  abs(ang1 - ang2)
// Clang warns (and is absolutely correct) that technically, this
// could be optimized away and do nothing:
//   warning: taking the absolute value of unsigned type 'unsigned int' has no effect
//   note: remove the call to 'abs' since unsigned values cannot be negative
inline angle_t absangle(angle_t a)
{
	return (angle_t)abs((int32)a);
}

// Effective size is 2049;
// The +1 size is to handle the case when x==y
//	without additional checking.
extern angle_t			tantoangle[SLOPERANGE+1];

inline double bam2rad(angle_t ang)
{
	return double(ang >> 1) * (PI / ANGLE_90);
}
inline angle_t rad2bam(double ang)
{
	return angle_t(ang * (double(1<<30) / PI)) << 1;
}

#endif // __TABLES_H__
