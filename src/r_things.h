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
//		Rendering of moving objects, sprites.
//
//-----------------------------------------------------------------------------


#ifndef __R_THINGS__
#define __R_THINGS__


// A vissprite_t is a thing
//	that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.

struct vissprite_t
{
	short			x1, x2;
	fixed_t			cx;				// for line side calculation
	fixed_t			gx, gy, gz;		// origin in world coordinates
	angle_t			angle;
	fixed_t			gzb, gzt;		// global bottom / top for silhouette clipping
	fixed_t			startfrac;		// horizontal position of x1
	fixed_t			xscale, yscale;
	fixed_t			xiscale;		// negative if flipped
	fixed_t			depth;
	fixed_t			idepth;			// 1/z
	fixed_t			texturemid;
	DWORD			FillColor;
	sector_t		*heightsec;		// killough 3/27/98: height sector for underwater/fake ceiling
	sector_t		*sector;		// [RH] sector this sprite is in
	F3DFloor	*fakefloor;
	F3DFloor	*fakeceiling;
	fixed_t			floorclip;
	union
	{
		FTexture	  *pic;
		struct FVoxel *voxel;
	};
	BYTE			bIsVoxel:1;		// [RH] Use voxel instead of pic
	BYTE			bSplitSprite:1;	// [RH] Sprite was split by a drawseg
	BYTE			FakeFlatStat;	// [RH] which side of fake/floor ceiling sprite is on
	short 			renderflags;
	DWORD			Translation;	// [RH] for color translation
	visstyle_t		Style;
};

struct particle_t;

void R_DrawParticle (vissprite_t *);
void R_ProjectParticle (particle_t *, const sector_t *sector, int shade, int fakeside);

extern int MaxVisSprites;

extern vissprite_t		**vissprites, **firstvissprite;
extern vissprite_t		**vissprite_p;

// Constant arrays used for psprite clipping
//	and initializing clipping.
extern short			zeroarray[MAXWIDTH];
extern short			screenheightarray[MAXWIDTH];

// vars for R_DrawMaskedColumn
extern short*			mfloorclip;
extern short*			mceilingclip;
extern fixed_t			spryscale;
extern fixed_t			sprtopscreen;
extern bool				sprflipvert;

extern fixed_t			pspritexscale;
extern fixed_t			pspriteyscale;
extern fixed_t			pspritexiscale;


void R_DrawMaskedColumn (const BYTE *column, const FTexture::Span *spans);


void R_CacheSprite (spritedef_t *sprite);
void R_SortVisSprites (int (STACK_ARGS *compare)(const void *, const void *), size_t first);
void R_AddSprites (sector_t *sec, int lightlevel, int fakeside);
void R_AddPSprites ();
void R_DrawSprites ();
void R_ClearSprites ();
void R_DrawMasked ();
void R_DrawRemainingPlayerSprites ();

void R_CheckOffscreenBuffer(int width, int height, bool spansonly);

enum { DVF_OFFSCREEN = 1, DVF_SPANSONLY = 2 };

void R_DrawVoxel(fixed_t dasprx, fixed_t daspry, fixed_t dasprz, angle_t dasprang,
	fixed_t daxscale, fixed_t dayscale, FVoxel *voxobj,
	lighttable_t *colormap, short *daumost, short *dadmost, int minslabz, int maxslabz, int flags);

void R_ClipVisSprite (vissprite_t *vis, int xl, int xh);


#endif
