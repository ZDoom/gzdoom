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
//		int finesine[10240] 			- Sine lookup.
//		 Remarkable thing is, how to use BAMs with this? 
//
//	  
//-----------------------------------------------------------------------------


#ifndef __TABLES_H__
#define __TABLES_H__

#include <stdlib.h>
#include <math.h>
#include "basictypes.h"

#define FINEANGLES		8192
#define FINEMASK		(FINEANGLES-1)

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT		19

// Effective size is 10240.
extern	fixed_t 		finesine[5*FINEANGLES/4];

// Binary Angle Measument, BAM.
#define ANGLE_90		(0x40000000)
#define ANGLE_180		(0x80000000)
#define ANGLE_270		(0xc0000000)
#define ANGLE_MAX		(0xffffffff)


typedef uint32			angle_t;

#endif // __TABLES_H__
