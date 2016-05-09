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

#include "r_bsp.h"

// A vissprite_t is a thing
//	that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.

struct vissprite_t
{
	struct posang
	{
		FVector3 vpos;			// view origin
		FAngle vang;			// view angle
	};

	short			x1, x2;
	FVector3		gpos;			// origin in world coordinates
	union
	{
		struct
		{
			float	gzb, gzt;		// global bottom / top for silhouette clipping
		};
		struct
		{
			int		y1, y2;			// top / bottom of particle on screen
		};
	};
	DAngle			Angle;
	fixed_t			xscale;
	float			yscale;
	float			depth;
	float			idepth;			// 1/z
	float			deltax, deltay;
	DWORD			FillColor;
	double			floorclip;
	union
	{
		FTexture *pic;
		struct FVoxel *voxel;
	};
	union
	{
		// Used by face sprites
		struct
		{
			double	texturemid;
			fixed_t	startfrac;		// horizontal position of x1
			fixed_t	xiscale;		// negative if flipped
		};
		// Used by wall sprites
		FWallCoords wallc;
		// Used by voxels
		posang pa;
	};
	sector_t		*heightsec;		// killough 3/27/98: height sector for underwater/fake ceiling
	sector_t		*sector;		// [RH] sector this sprite is in
	F3DFloor		*fakefloor;
	F3DFloor		*fakeceiling;
	BYTE			bIsVoxel:1;		// [RH] Use voxel instead of pic
	BYTE			bWallSprite:1;	// [RH] This is a wall sprite
	BYTE			bSplitSprite:1;	// [RH] Sprite was split by a drawseg
	BYTE			bInMirror:1;	// [RH] Sprite is "inside" a mirror
	BYTE			FakeFlatStat;	// [RH] which side of fake/floor ceiling sprite is on
	BYTE			ColormapNum;	// Which colormap is rendered (needed for shaded drawer)
	short 			renderflags;
	DWORD			Translation;	// [RH] for color translation
	visstyle_t		Style;
	int				CurrentPortalUniq; // [ZZ] to identify the portal that this thing is in. used for clipping.

	vissprite_t() {}
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
extern double			spryscale;
extern double			sprtopscreen;
extern bool				sprflipvert;

extern double			pspritexscale;
extern double			pspritexiscale;
extern double			pspriteyscale;

extern FTexture			*WallSpriteTile;


void R_DrawMaskedColumn (const BYTE *column, const FTexture::Span *spans);
void R_WallSpriteColumn (void (*drawfunc)(const BYTE *column, const FTexture::Span *spans));

void R_CacheSprite (spritedef_t *sprite);
void R_SortVisSprites (int (*compare)(const void *, const void *), size_t first);
void R_AddSprites (sector_t *sec, int lightlevel, int fakeside);
void R_DrawSprites ();
void R_ClearSprites ();
void R_DrawMasked ();
void R_DrawRemainingPlayerSprites ();

void R_CheckOffscreenBuffer(int width, int height, bool spansonly);

enum { DVF_OFFSCREEN = 1, DVF_SPANSONLY = 2, DVF_MIRRORED = 4 };

void R_DrawVoxel(const FVector3 &viewpos, FAngle viewangle,
	const FVector3 &sprpos, DAngle dasprang,
	fixed_t daxscale, fixed_t dayscale, struct FVoxel *voxobj,
	lighttable_t *colormap, short *daumost, short *dadmost, int minslabz, int maxslabz, int flags);

void R_ClipVisSprite (vissprite_t *vis, int xl, int xh);


#endif
