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
// $Log:$
//
// DESCRIPTION:
//		Refresh of things, i.e. objects represented by sprites.
//
// This file contains some code from the Build Engine.
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "r_local.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "v_video.h"
#include "sc_man.h"
#include "s_sound.h"
#include "sbar.h"
#include "gi.h"
#include "r_sky.h"
#include "cmdlib.h"
#include "g_level.h"
#include "d_net.h"
#include "colormatcher.h"
#include "d_netinf.h"
#include "p_effect.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_3dfloors.h"
#include "v_palette.h"
#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "r_data/voxels.h"
#include "p_local.h"

// [RH] A c-buffer. Used for keeping track of offscreen voxel spans.

struct FCoverageBuffer
{
	struct Span
	{
		Span *NextSpan;
		short Start, Stop;
	};

	FCoverageBuffer(int size);
	~FCoverageBuffer();

	void Clear();
	void InsertSpan(int listnum, int start, int stop);
	Span *AllocSpan();

	FMemArena SpanArena;
	Span **Spans;	// [0..NumLists-1] span lists
	Span *FreeSpans;
	unsigned int NumLists;
};

extern fixed_t globaluclip, globaldclip;


#define MINZ			(2048*4)
#define BASEYCENTER 	(100)

EXTERN_CVAR (Bool, st_scale)
EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Int, r_drawfuzz)

//
// Sprite rotation 0 is facing the viewer,
//	rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//	which increases counter clockwise (protractor).
//
fixed_t 		pspritexscale;
fixed_t			pspriteyscale;
fixed_t 		pspritexiscale;
fixed_t			sky1scale;			// [RH] Sky 1 scale factor
fixed_t			sky2scale;			// [RH] Sky 2 scale factor

vissprite_t		*VisPSprites[NUMPSPRITES];
int				VisPSpritesX1[NUMPSPRITES];
FDynamicColormap *VisPSpritesBaseColormap[NUMPSPRITES];

static int		spriteshade;

// constant arrays
//	used for psprite clipping and initializing clipping
short			zeroarray[MAXWIDTH];
short			screenheightarray[MAXWIDTH];

EXTERN_CVAR (Bool, r_drawplayersprites)
EXTERN_CVAR (Bool, r_drawvoxels)

//
// INITIALIZATION FUNCTIONS
//

int OffscreenBufferWidth, OffscreenBufferHeight;
BYTE *OffscreenColorBuffer;
FCoverageBuffer *OffscreenCoverageBuffer;

//
// GAME FUNCTIONS
//
int				MaxVisSprites;
vissprite_t 	**vissprites;
vissprite_t		**firstvissprite;
vissprite_t		**vissprite_p;
vissprite_t		**lastvissprite;
int 			newvissprite;
bool			DrewAVoxel;

static vissprite_t **spritesorter;
static int spritesortersize = 0;
static int vsprcount;


void R_DeinitSprites()
{
	// Free vissprites
	for (int i = 0; i < MaxVisSprites; ++i)
	{
		delete vissprites[i];
	}
	free (vissprites);
	vissprites = NULL;
	vissprite_p = lastvissprite = NULL;
	MaxVisSprites = 0;

	// Free vissprites sorter
	if (spritesorter != NULL)
	{
		delete[] spritesorter;
		spritesortersize = 0;
		spritesorter = NULL;
	}

	// Free offscreen buffer
	if (OffscreenColorBuffer != NULL)
	{
		delete[] OffscreenColorBuffer;
		OffscreenColorBuffer = NULL;
	}
	if (OffscreenCoverageBuffer != NULL)
	{
		delete OffscreenCoverageBuffer;
		OffscreenCoverageBuffer = NULL;
	}
	OffscreenBufferHeight = OffscreenBufferWidth = 0;
}

//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites (void)
{
	vissprite_p = firstvissprite;
	DrewAVoxel = false;
}


//
// R_NewVisSprite
//
vissprite_t *R_NewVisSprite (void)
{
	if (vissprite_p == lastvissprite)
	{
		ptrdiff_t firstvisspritenum = firstvissprite - vissprites;
		ptrdiff_t prevvisspritenum = vissprite_p - vissprites;

		MaxVisSprites = MaxVisSprites ? MaxVisSprites * 2 : 128;
		vissprites = (vissprite_t **)M_Realloc (vissprites, MaxVisSprites * sizeof(vissprite_t));
		lastvissprite = &vissprites[MaxVisSprites];
		firstvissprite = &vissprites[firstvisspritenum];
		vissprite_p = &vissprites[prevvisspritenum];
		DPrintf ("MaxVisSprites increased to %d\n", MaxVisSprites);

		// Allocate sprites from the new pile
		for (vissprite_t **p = vissprite_p; p < lastvissprite; ++p)
		{
			*p = new vissprite_t;
		}
	}

	vissprite_p++;
	return *(vissprite_p-1);
}

//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//	in posts/runs of opaque pixels.
//
short*			mfloorclip;
short*			mceilingclip;

fixed_t 		spryscale;
fixed_t 		sprtopscreen;

bool			sprflipvert;

void R_DrawMaskedColumn (const BYTE *column, const FTexture::Span *span)
{
	while (span->Length != 0)
	{
		const int length = span->Length;
		const int top = span->TopOffset;

		// calculate unclipped screen coordinates for post
		dc_yl = (sprtopscreen + spryscale * top) >> FRACBITS;
		dc_yh = (sprtopscreen + spryscale * (top + length) - FRACUNIT) >> FRACBITS;

		if (sprflipvert)
		{
			swapvalues (dc_yl, dc_yh);
		}

		if (dc_yh >= mfloorclip[dc_x])
		{
			dc_yh = mfloorclip[dc_x] - 1;
		}
		if (dc_yl < mceilingclip[dc_x])
		{
			dc_yl = mceilingclip[dc_x];
		}

		if (dc_yl <= dc_yh)
		{
			if (sprflipvert)
			{
				dc_texturefrac = (dc_yl*dc_iscale) - (top << FRACBITS)
					- FixedMul (centeryfrac, dc_iscale) - dc_texturemid;
				const fixed_t maxfrac = length << FRACBITS;
				while (dc_texturefrac >= maxfrac)
				{
					if (++dc_yl > dc_yh)
						goto nextpost;
					dc_texturefrac += dc_iscale;
				}
				fixed_t endfrac = dc_texturefrac + (dc_yh-dc_yl)*dc_iscale;
				while (endfrac < 0)
				{
					if (--dc_yh < dc_yl)
						goto nextpost;
					endfrac -= dc_iscale;
				}
			}
			else
			{
				dc_texturefrac = dc_texturemid - (top << FRACBITS)
					+ (dc_yl*dc_iscale) - FixedMul (centeryfrac-FRACUNIT, dc_iscale);
				while (dc_texturefrac < 0)
				{
					if (++dc_yl > dc_yh)
						goto nextpost;
					dc_texturefrac += dc_iscale;
				}
				fixed_t endfrac = dc_texturefrac + (dc_yh-dc_yl)*dc_iscale;
				const fixed_t maxfrac = length << FRACBITS;
				if (dc_yh < mfloorclip[dc_x]-1 && endfrac < maxfrac - dc_iscale)
				{
					dc_yh++;
				}
				else while (endfrac >= maxfrac)
				{
					if (--dc_yh < dc_yl)
						goto nextpost;
					endfrac -= dc_iscale;
				}
			}
			dc_source = column + top;
			dc_dest = ylookup[dc_yl] + dc_x + dc_destorg;
			dc_count = dc_yh - dc_yl + 1;
			colfunc ();
		}
nextpost:
		span++;
	}
}

//
// R_DrawVisSprite
//	mfloorclip and mceilingclip should also be set.
//
void R_DrawVisSprite (vissprite_t *vis)
{
	const BYTE *pixels;
	const FTexture::Span *spans;
	fixed_t 		frac;
	FTexture		*tex;
	int				x2, stop4;
	fixed_t			xiscale;
	ESPSResult		mode;

	dc_colormap = vis->Style.colormap;

	mode = R_SetPatchStyle (vis->Style.RenderStyle, vis->Style.alpha, vis->Translation, vis->FillColor);

	if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Shaded])
	{ // For shaded sprites, R_SetPatchStyle sets a dc_colormap to an alpha table, but
	  // it is the brightest one. We need to get back to the proper light level for
	  // this sprite.
		dc_colormap += vis->ColormapNum << COLORMAPSHIFT;
	}

	if (mode != DontDraw)
	{
		if (mode == DoDraw0)
		{
			// One column at a time
			stop4 = vis->x1;
		}
		else	 // DoDraw1
		{
			// Up to four columns at a time
			stop4 = (vis->x2 + 1) & ~3;
		}

		tex = vis->pic;
		spryscale = vis->yscale;
		sprflipvert = false;
		dc_iscale = 0xffffffffu / (unsigned)vis->yscale;
		dc_texturemid = vis->texturemid;
		frac = vis->startfrac;
		xiscale = vis->xiscale;

		sprtopscreen = centeryfrac - FixedMul (dc_texturemid, spryscale);

		dc_x = vis->x1;
		x2 = vis->x2 + 1;

		if (dc_x < x2)
		{
			while ((dc_x < stop4) && (dc_x & 3))
			{
				pixels = tex->GetColumn (frac >> FRACBITS, &spans);
				R_DrawMaskedColumn (pixels, spans);
				dc_x++;
				frac += xiscale;
			}

			while (dc_x < stop4)
			{
				rt_initcols();
				for (int zz = 4; zz; --zz)
				{
					pixels = tex->GetColumn (frac >> FRACBITS, &spans);
					R_DrawMaskedColumnHoriz (pixels, spans);
					dc_x++;
					frac += xiscale;
				}
				rt_draw4cols (dc_x - 4);
			}

			while (dc_x < x2)
			{
				pixels = tex->GetColumn (frac >> FRACBITS, &spans);
				R_DrawMaskedColumn (pixels, spans);
				dc_x++;
				frac += xiscale;
			}
		}
	}

	R_FinishSetPatchStyle ();

	NetUpdate ();
}

void R_DrawVisVoxel(vissprite_t *spr, int minslabz, int maxslabz, short *cliptop, short *clipbot)
{
	ESPSResult mode;
	int flags = 0;

	// Do setup for blending.
	dc_colormap = spr->Style.colormap;
	mode = R_SetPatchStyle(spr->Style.RenderStyle, spr->Style.alpha, spr->Translation, spr->FillColor);

	if (mode == DontDraw)
	{
		return;
	}
	if (colfunc == fuzzcolfunc || colfunc == R_FillColumnP)
	{
		flags = DVF_OFFSCREEN | DVF_SPANSONLY;
	}
	else if (colfunc != basecolfunc)
	{
		flags = DVF_OFFSCREEN;
	}
	if (flags != 0)
	{
		R_CheckOffscreenBuffer(RenderTarget->GetWidth(), RenderTarget->GetHeight(), !!(flags & DVF_SPANSONLY));
	}
	if (spr->bInMirror)
	{
		flags |= DVF_MIRRORED;
	}

	// Render the voxel, either directly to the screen or offscreen.
	R_DrawVoxel(spr->vx, spr->vy, spr->vz, spr->vang, spr->gx, spr->gy, spr->gz, spr->angle,
		spr->xscale, spr->yscale, spr->voxel, spr->Style.colormap, cliptop, clipbot,
		minslabz, maxslabz, flags);

	// Blend the voxel, if that's what we need to do.
	if ((flags & ~DVF_MIRRORED) != 0)
	{
		for (int x = 0; x < viewwidth; ++x)
		{
			if (!(flags & DVF_SPANSONLY) && (x & 3) == 0)
			{
				rt_initcols(OffscreenColorBuffer + x * OffscreenBufferHeight);
			}
			for (FCoverageBuffer::Span *span = OffscreenCoverageBuffer->Spans[x]; span != NULL; span = span->NextSpan)
			{
				if (flags & DVF_SPANSONLY)
				{
					dc_x = x;
					dc_yl = span->Start;
					dc_yh = span->Stop - 1;
					dc_count = span->Stop - span->Start;
					dc_dest = ylookup[span->Start] + x + dc_destorg;
					colfunc();
				}
				else
				{
					unsigned int **tspan = &dc_ctspan[x & 3];
					(*tspan)[0] = span->Start;
					(*tspan)[1] = span->Stop - 1;
					*tspan += 2;
				}
			}
			if (!(flags & DVF_SPANSONLY) && (x & 3) == 3)
			{
				rt_draw4cols(x - 3);
			}
		}
	}

	R_FinishSetPatchStyle();
	NetUpdate();
}

//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//
void R_ProjectSprite (AActor *thing, int fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling)
{
	fixed_t				fx, fy, fz;
	fixed_t 			tr_x;
	fixed_t 			tr_y;
	
	fixed_t				gzt;				// killough 3/27/98
	fixed_t				gzb;				// [RH] use bottom of sprite, not actor
	fixed_t 			tx, tx2;
	fixed_t 			tz;

	fixed_t 			xscale = FRACUNIT, yscale = FRACUNIT;
	
	int 				x1;
	int 				x2;

	FTextureID			picnum;
	FTexture			*tex;
	FVoxelDef			*voxel;
	
	WORD 				flip;
	
	vissprite_t*		vis;
	
	fixed_t 			iscale;

	sector_t*			heightsec;			// killough 3/27/98

	// Don't waste time projecting sprites that are definitely not visible.
	if (thing == NULL ||
		(thing->renderflags & RF_INVISIBLE) ||
		!thing->RenderStyle.IsVisible(thing->alpha) ||
		!thing->IsVisibleToPlayer())
	{
		return;
	}

	// [RH] Interpolate the sprite's position to make it look smooth
	fx = thing->PrevX + FixedMul (r_TicFrac, thing->x - thing->PrevX);
	fy = thing->PrevY + FixedMul (r_TicFrac, thing->y - thing->PrevY);
	fz = thing->PrevZ + FixedMul (r_TicFrac, thing->z - thing->PrevZ) + thing->GetBobOffset(r_TicFrac);

	// transform the origin point
	tr_x = fx - viewx;
	tr_y = fy - viewy;

	tz = DMulScale20 (tr_x, viewtancos, tr_y, viewtansin);

	tex = NULL;
	voxel = NULL;

	int spritenum = thing->sprite;
	fixed_t spritescaleX = thing->scaleX;
	fixed_t spritescaleY = thing->scaleY;
	if (thing->player != NULL)
	{
		P_CheckPlayerSprite(thing, spritenum, spritescaleX, spritescaleY);
	}

	if (thing->picnum.isValid())
	{
		picnum = thing->picnum;

		tex = TexMan(picnum);
		if (tex->UseType == FTexture::TEX_Null)
		{
			return;
		}
		flip = 0;

		if (tex->Rotations != 0xFFFF)
		{
			// choose a different rotation based on player view
			spriteframe_t *sprframe = &SpriteFrames[tex->Rotations];
			angle_t ang = R_PointToAngle (fx, fy);
			angle_t rot;
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9) >> 28;
			}
			else
			{
				rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9-(angle_t)(ANGLE_180/16)) >> 28;
			}
			picnum = sprframe->Texture[rot];
			flip = sprframe->Flip & (1 << rot);
			tex = TexMan[picnum];	// Do not animate the rotation
		}
	}
	else
	{
		// decide which texture to use for the sprite
#ifdef RANGECHECK
		if (spritenum >= (signed)sprites.Size () || spritenum < 0)
		{
			DPrintf ("R_ProjectSprite: invalid sprite number %u\n", spritenum);
			return;
		}
#endif
		spritedef_t *sprdef = &sprites[spritenum];
		if (thing->frame >= sprdef->numframes)
		{
			// If there are no frames at all for this sprite, don't draw it.
			return;
		}
		else
		{
			//picnum = SpriteFrames[sprdef->spriteframes + thing->frame].Texture[0];
			// choose a different rotation based on player view
			spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + thing->frame];
			angle_t ang = R_PointToAngle (fx, fy);
			angle_t rot;
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9) >> 28;
			}
			else
			{
				rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9-(angle_t)(ANGLE_180/16)) >> 28;
			}
			picnum = sprframe->Texture[rot];
			flip = sprframe->Flip & (1 << rot);
			tex = TexMan[picnum];	// Do not animate the rotation
			if (r_drawvoxels)
			{
				voxel = sprframe->Voxel;
			}
		}
	}
	if (spritescaleX < 0)
	{
		spritescaleX = -spritescaleX;
		flip = !flip;
	}
	if (voxel == NULL && (tex == NULL || tex->UseType == FTexture::TEX_Null))
	{
		return;
	}

	// thing is behind view plane?
	if (voxel == NULL && tz < MINZ)
		return;

	tx = DMulScale16 (tr_x, viewsin, -tr_y, viewcos);

	// [RH] Flip for mirrors
	if (MirrorFlags & RF_XFLIP)
	{
		tx = -tx;
	}
	tx2 = tx >> 4;

	// too far off the side?
	// if it's a voxel, it can be further off the side
	if ((voxel == NULL && (abs(tx) >> 6) > abs(tz)) ||
		(voxel != NULL && (abs(tx) >> 7) > abs(tz)))
	{
		return;
	}

	if (voxel == NULL)
	{
		// [RH] Added scaling
		int scaled_to = tex->GetScaledTopOffset();
		int scaled_bo = scaled_to - tex->GetScaledHeight();
		gzt = fz + spritescaleY * scaled_to;
		gzb = fz + spritescaleY * scaled_bo;
	}
	else
	{
		xscale = FixedMul(spritescaleX, voxel->Scale);
		yscale = FixedMul(spritescaleY, voxel->Scale);
		gzt = fz + MulScale8(yscale, voxel->Voxel->Mips[0].PivotZ) - thing->floorclip;
		gzb = fz + MulScale8(yscale, voxel->Voxel->Mips[0].PivotZ - (voxel->Voxel->Mips[0].SizeZ << 8));
		if (gzt <= gzb)
			return;
	}

	// killough 3/27/98: exclude things totally separated
	// from the viewer, by either water or fake ceilings
	// killough 4/11/98: improve sprite clipping for underwater/fake ceilings

	heightsec = thing->Sector->GetHeightSec();

	if (heightsec != NULL)	// only clip things which are in special sectors
	{
		if (fakeside == FAKED_AboveCeiling)
		{
			if (gzt < heightsec->ceilingplane.ZatPoint (fx, fy))
				return;
		}
		else if (fakeside == FAKED_BelowFloor)
		{
			if (gzb >= heightsec->floorplane.ZatPoint (fx, fy))
				return;
		}
		else
		{
			if (gzt < heightsec->floorplane.ZatPoint (fx, fy))
				return;
			if (!(heightsec->MoreFlags & SECF_FAKEFLOORONLY) && gzb >= heightsec->ceilingplane.ZatPoint (fx, fy))
				return;
		}
	}

	if (voxel == NULL)
	{
		xscale = DivScale12 (centerxfrac, tz);

		// [RH] Reject sprites that are off the top or bottom of the screen
		if (MulScale12 (globaluclip, tz) > viewz - gzb ||
			MulScale12 (globaldclip, tz) < viewz - gzt)
		{
			return;
		}

		// [RH] Flip for mirrors and renderflags
		if ((MirrorFlags ^ thing->renderflags) & RF_XFLIP)
		{
			flip = !flip;
		}

		// calculate edges of the shape
		const fixed_t thingxscalemul = DivScale16(spritescaleX, tex->xScale);

		tx -= (flip ? (tex->GetWidth() - tex->LeftOffset - 1) : tex->LeftOffset) * thingxscalemul;
		x1 = centerx + MulScale32 (tx, xscale);

		// off the right side?
		if (x1 > WindowRight)
			return;

		tx += tex->GetWidth() * thingxscalemul;
		x2 = centerx + MulScale32 (tx, xscale);

		// off the left side or too small?
		if ((x2 < WindowLeft || x2 <= x1))
			return;

		xscale = FixedDiv(FixedMul(spritescaleX, xscale), tex->xScale);
		iscale = (tex->GetWidth() << FRACBITS) / (x2 - x1);
		x2--;

		fixed_t yscale = SafeDivScale16(spritescaleY, tex->yScale);

		// store information in a vissprite
		vis = R_NewVisSprite();

		vis->xscale = xscale;
		vis->yscale = Scale(InvZtoScale, yscale, tz << 4);
		vis->idepth = (unsigned)DivScale32(1, tz) >> 1;	// tz is 20.12, so idepth ought to be 12.20, but signed math makes it 13.19
		vis->floorclip = FixedDiv (thing->floorclip, yscale);
		vis->texturemid = (tex->TopOffset << FRACBITS) - FixedDiv (viewz - fz + thing->floorclip, yscale);
		vis->x1 = x1 < WindowLeft ? WindowLeft : x1;
		vis->x2 = x2 > WindowRight ? WindowRight : x2;
		vis->angle = thing->angle;

		if (flip)
		{
			vis->startfrac = (tex->GetWidth() << FRACBITS) - 1;
			vis->xiscale = -iscale;
		}
		else
		{
			vis->startfrac = 0;
			vis->xiscale = iscale;
		}

		if (vis->x1 > x1)
			vis->startfrac += vis->xiscale * (vis->x1 - x1);
	}
	else
	{
		vis = R_NewVisSprite();

		vis->xscale = xscale;
		vis->yscale = yscale;
		vis->x1 = WindowLeft;
		vis->x2 = WindowRight;
		vis->idepth = (unsigned)DivScale32(1, MAX(tz, MINZ)) >> 1;
		vis->floorclip = thing->floorclip;

		fz -= thing->floorclip;

		vis->angle = thing->angle + voxel->AngleOffset;

		int voxelspin = (thing->flags & MF_DROPPED) ? voxel->DroppedSpin : voxel->PlacedSpin;
		if (voxelspin != 0)
		{
			double ang = double(I_FPSTime()) * voxelspin / 1000;
			vis->angle -= angle_t(ang * (4294967296.f / 360));
		}

		vis->vx = viewx;
		vis->vy = viewy;
		vis->vz = viewz;
		vis->vang = viewangle;
	}

	// killough 3/27/98: save sector for special clipping later
	vis->heightsec = heightsec;
	vis->sector = thing->Sector;

	vis->cx = tx2;
	vis->depth = tz;
	vis->gx = fx;
	vis->gy = fy;
	vis->gz = fz;
	vis->gzb = gzb;		// [RH] use gzb, not thing->z
	vis->gzt = gzt;		// killough 3/27/98
	vis->deltax = fx - viewx;
	vis->deltay = fy - viewy;
	vis->renderflags = thing->renderflags;
	if(thing->flags5 & MF5_BRIGHT) vis->renderflags |= RF_FULLBRIGHT; // kg3D
	vis->Style.RenderStyle = thing->RenderStyle;
	vis->FillColor = thing->fillcolor;
	vis->Translation = thing->Translation;		// [RH] thing translation table
	vis->FakeFlatStat = fakeside;
	vis->Style.alpha = thing->alpha;
	vis->fakefloor = fakefloor;
	vis->fakeceiling = fakeceiling;
	vis->ColormapNum = 0;
	vis->bInMirror = MirrorFlags & RF_XFLIP;

	if (voxel != NULL)
	{
		vis->voxel = voxel->Voxel;
		vis->bIsVoxel = true;
		DrewAVoxel = true;
	}
	else
	{
		vis->pic = tex;
		vis->bIsVoxel = false;
	}

	// The software renderer cannot invert the source without inverting the overlay
	// too. That means if the source is inverted, we need to do the reverse of what
	// the invert overlay flag says to do.
	INTBOOL invertcolormap = (vis->Style.RenderStyle.Flags & STYLEF_InvertOverlay);

	if (vis->Style.RenderStyle.Flags & STYLEF_InvertSource)
	{
		invertcolormap = !invertcolormap;
	}

	FDynamicColormap *mybasecolormap = basecolormap;

	// Sprites that are added to the scene must fade to black.
	if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Add] && mybasecolormap->Fade != 0)
	{
		mybasecolormap = GetSpecialLights(mybasecolormap->Color, 0, mybasecolormap->Desaturate);
	}

	if (vis->Style.RenderStyle.Flags & STYLEF_FadeToBlack)
	{
		if (invertcolormap)
		{ // Fade to white
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(255,255,255), mybasecolormap->Desaturate);
			invertcolormap = false;
		}
		else
		{ // Fade to black
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(0,0,0), mybasecolormap->Desaturate);
		}
	}

	// get light level
	if (fixedcolormap != NULL)
	{ // fixed map
		vis->Style.colormap = fixedcolormap;
	}
	else
	{
		if (invertcolormap)
		{
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
		}
		if (fixedlightlev >= 0)
		{
			vis->Style.colormap = mybasecolormap->Maps + fixedlightlev;
		}
		else if (!foggy && ((thing->renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT)))
		{ // full bright
			vis->Style.colormap = mybasecolormap->Maps;
		}
		else
		{ // diminished light
			vis->ColormapNum = GETPALOOKUP(
				(fixed_t)DivScale12 (r_SpriteVisibility, MAX(tz, MINZ)), spriteshade);
			vis->Style.colormap = mybasecolormap->Maps + (vis->ColormapNum << COLORMAPSHIFT);
		}
	}
}


//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
// [RH] Save which side of heightsec sprite is on here.
void R_AddSprites (sector_t *sec, int lightlevel, int fakeside)
{
	AActor *thing;
	F3DFloor *rover;
	F3DFloor *fakeceiling = NULL;
	F3DFloor *fakefloor = NULL;

	// BSP is traversed by subsector.
	// A sector might have been split into several
	//	subsectors during BSP building.
	// Thus we check whether it was already added.
	if (sec->thinglist == NULL || sec->validcount == validcount)
		return;

	// Well, now it will be done.
	sec->validcount = validcount;

	spriteshade = LIGHT2SHADE(lightlevel + r_actualextralight);

	// Handle all things in sector.
	for (thing = sec->thinglist; thing; thing = thing->snext)
	{
		// find fake level
		for(int i = 0; i < (int)frontsector->e->XFloor.ffloors.Size(); i++) {
			rover = frontsector->e->XFloor.ffloors[i];
			if(!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES)) continue;
			if(!(rover->flags & FF_SOLID) || rover->alpha != 255) continue;
			if(!fakefloor)
			{
				if(!(rover->top.plane->a) && !(rover->top.plane->b))
				{
					if(rover->top.plane->Zat0() <= thing->z) fakefloor = rover;
				}
			}
			if(!(rover->bottom.plane->a) && !(rover->bottom.plane->b))
			{
				if(rover->bottom.plane->Zat0() >= thing->z + thing->height) fakeceiling = rover;
			}
		}	
		R_ProjectSprite (thing, fakeside, fakefloor, fakeceiling);
		fakeceiling = NULL;
		fakefloor = NULL;
	}
}


//
// R_DrawPSprite
//
void R_DrawPSprite (pspdef_t* psp, int pspnum, AActor *owner, fixed_t sx, fixed_t sy)
{
	fixed_t 			tx;
	int 				x1;
	int 				x2;
	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	FTextureID			picnum;
	WORD				flip;
	FTexture*			tex;
	vissprite_t*		vis;
	static vissprite_t	avis[NUMPSPRITES];
	bool noaccel;

	assert(pspnum >= 0 && pspnum < NUMPSPRITES);

	// decide which patch to use
	if ( (unsigned)psp->sprite >= (unsigned)sprites.Size ())
	{
		DPrintf ("R_DrawPSprite: invalid sprite number %i\n", psp->sprite);
		return;
	}
	sprdef = &sprites[psp->sprite];
	if (psp->frame >= sprdef->numframes)
	{
		DPrintf ("R_DrawPSprite: invalid sprite frame %i : %i\n", psp->sprite, psp->frame);
		return;
	}
	sprframe = &SpriteFrames[sprdef->spriteframes + psp->frame];

	picnum = sprframe->Texture[0];
	flip = sprframe->Flip & 1;
	tex = TexMan(picnum);

	if (tex->UseType == FTexture::TEX_Null)
		return;

	// calculate edges of the shape
	tx = sx-((320/2)<<FRACBITS);

	tx -= tex->GetScaledLeftOffset() << FRACBITS;
	x1 = (centerxfrac + FixedMul (tx, pspritexscale)) >>FRACBITS;
	VisPSpritesX1[pspnum] = x1;

	// off the right side
	if (x1 > viewwidth)
		return; 

	tx += tex->GetScaledWidth() << FRACBITS;
	x2 = ((centerxfrac + FixedMul (tx, pspritexscale)) >>FRACBITS) - 1;

	// off the left side
	if (x2 < 0)
		return;
	
	// store information in a vissprite
	vis = &avis[pspnum];
	vis->renderflags = owner->renderflags;
	vis->floorclip = 0;


	vis->texturemid = MulScale16((BASEYCENTER<<FRACBITS) - sy, tex->yScale) + (tex->TopOffset << FRACBITS);


	if (camera->player && (RenderTarget != screen ||
		viewheight == RenderTarget->GetHeight() ||
		(RenderTarget->GetWidth() > 320 && !st_scale)))
	{	// Adjust PSprite for fullscreen views
		AWeapon *weapon = NULL;
		if (camera->player != NULL)
		{
			weapon = camera->player->ReadyWeapon;
		}
		if (pspnum <= ps_flash && weapon != NULL && weapon->YAdjust != 0)
		{
			if (RenderTarget != screen || viewheight == RenderTarget->GetHeight())
			{
				vis->texturemid -= weapon->YAdjust;
			}
			else
			{
				vis->texturemid -= FixedMul (StatusBar->GetDisplacement (),
					weapon->YAdjust);
			}
		}
	}
	if (pspnum <= ps_flash)
	{ // Move the weapon down for 1280x1024.
		vis->texturemid -= BaseRatioSizes[WidescreenRatio][2];
	}
	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
	vis->xscale = DivScale16(pspritexscale, tex->xScale);
	vis->yscale = DivScale16(pspriteyscale, tex->yScale);
	vis->Translation = 0;		// [RH] Use default colors
	vis->pic = tex;
	vis->ColormapNum = 0;

	if (flip)
	{
		vis->xiscale = -MulScale16(pspritexiscale, tex->xScale);
		vis->startfrac = (tex->GetWidth() << FRACBITS) - 1;
	}
	else
	{
		vis->xiscale = MulScale16(pspritexiscale, tex->xScale);
		vis->startfrac = 0;
	}

	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);

	noaccel = false;
	if (pspnum <= ps_flash)
	{
		vis->Style.alpha = owner->alpha;
		vis->Style.RenderStyle = owner->RenderStyle;

		// The software renderer cannot invert the source without inverting the overlay
		// too. That means if the source is inverted, we need to do the reverse of what
		// the invert overlay flag says to do.
		INTBOOL invertcolormap = (vis->Style.RenderStyle.Flags & STYLEF_InvertOverlay);

		if (vis->Style.RenderStyle.Flags & STYLEF_InvertSource)
		{
			invertcolormap = !invertcolormap;
		}

		FDynamicColormap *mybasecolormap = basecolormap;

		if (vis->Style.RenderStyle.Flags & STYLEF_FadeToBlack)
		{
			if (invertcolormap)
			{ // Fade to white
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(255,255,255), mybasecolormap->Desaturate);
				invertcolormap = false;
			}
			else
			{ // Fade to black
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(0,0,0), mybasecolormap->Desaturate);
			}
		}

		if (realfixedcolormap != NULL)
		{ // fixed color
			vis->Style.colormap = realfixedcolormap->Colormap;
		}
		else
		{
			if (invertcolormap)
			{
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
			}
			if (fixedlightlev >= 0)
			{
				vis->Style.colormap = mybasecolormap->Maps + fixedlightlev;
			}
			else if (!foggy && psp->state->GetFullbright())
			{ // full bright
				vis->Style.colormap = mybasecolormap->Maps;	// [RH] use basecolormap
			}
			else
			{ // local light
				vis->Style.colormap = mybasecolormap->Maps + (GETPALOOKUP (0, spriteshade) << COLORMAPSHIFT);
			}
		}
		if (camera->Inventory != NULL)
		{
			lighttable_t *oldcolormap = vis->Style.colormap;
			camera->Inventory->AlterWeaponSprite (&vis->Style);
			if (vis->Style.colormap != oldcolormap)
			{
				// The colormap has changed. Is it one we can easily identify?
				// If not, then don't bother trying to identify it for
				// hardware accelerated drawing.
				if (vis->Style.colormap < SpecialColormaps[0].Colormap || 
					vis->Style.colormap > SpecialColormaps.Last().Colormap)
				{
					noaccel = true;
				}
				// Has the basecolormap changed? If so, we can't hardware accelerate it,
				// since we don't know what it is anymore.
				else if (vis->Style.colormap < mybasecolormap->Maps ||
					vis->Style.colormap >= mybasecolormap->Maps + NUMCOLORMAPS*256)
				{
					noaccel = true;
				}
			}
		}
		// If we're drawing with a special colormap, but shaders for them are disabled, do
		// not accelerate.
		if (!r_shadercolormaps && (vis->Style.colormap >= SpecialColormaps[0].Colormap &&
			vis->Style.colormap <= SpecialColormaps.Last().Colormap))
		{
			noaccel = true;
		}
		// If the main colormap has fixed lights, and this sprite is being drawn with that
		// colormap, disable acceleration so that the lights can remain fixed.
		if (!noaccel && realfixedcolormap == NULL &&
			NormalLightHasFixedLights && mybasecolormap == &NormalLight &&
			vis->pic->UseBasePalette())
		{
			noaccel = true;
		}
		VisPSpritesBaseColormap[pspnum] = mybasecolormap;
	}
	else
	{
		VisPSpritesBaseColormap[pspnum] = basecolormap;
		vis->Style.colormap = basecolormap->Maps;
		vis->Style.RenderStyle = STYLE_Normal;
	}

	// Check for hardware-assisted 2D. If it's available, and this sprite is not
	// fuzzy, don't draw it until after the switch to 2D mode.
	if (!noaccel && RenderTarget == screen && (DFrameBuffer *)screen->Accel2D)
	{
		FRenderStyle style = vis->Style.RenderStyle;
		style.CheckFuzz();
		if (style.BlendOp != STYLEOP_Fuzz)
		{
			VisPSprites[pspnum] = vis;
			return;
		}
	}
	R_DrawVisSprite (vis);
}



//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void R_DrawPlayerSprites ()
{
	int 		i;
	int 		lightnum;
	pspdef_t*	psp;
	sector_t*	sec = NULL;
	static sector_t tempsec;
	int			floorlight, ceilinglight;
	F3DFloor *rover;

	if (!r_drawplayersprites ||
		!camera->player ||
		(players[consoleplayer].cheats & CF_CHASECAM))
		return;

	if(fixedlightlev < 0 && viewsector->e && viewsector->e->XFloor.lightlist.Size()) {
		for(i = viewsector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
			if(viewz <= viewsector->e->XFloor.lightlist[i].plane.Zat0()) {
				rover = viewsector->e->XFloor.lightlist[i].caster;
				if(rover) {
					if(rover->flags & FF_DOUBLESHADOW && viewz <= rover->bottom.plane->Zat0())
						break;
					sec = rover->model;
					if(rover->flags & FF_FADEWALLS)
						basecolormap = sec->ColorMap;
					else
						basecolormap = viewsector->e->XFloor.lightlist[i].extra_colormap;
				}
				break;
			}
		if(!sec) {
			sec = viewsector;
			basecolormap = sec->ColorMap;
		}
		floorlight = ceilinglight = sec->lightlevel;
	} else {
		// This used to use camera->Sector but due to interpolation that can be incorrect
		// when the interpolated viewpoint is in a different sector than the camera.
		sec = R_FakeFlat (viewsector, &tempsec, &floorlight,
			&ceilinglight, false);

		// [RH] set basecolormap
		basecolormap = sec->ColorMap;
	}

	// [RH] set foggy flag
	foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE));
	r_actualextralight = foggy ? 0 : extralight << 4;

	// get light level
	lightnum = ((floorlight + ceilinglight) >> 1) + r_actualextralight;
	spriteshade = LIGHT2SHADE(lightnum) - 24*FRACUNIT;

	// clip to screen bounds
	mfloorclip = screenheightarray;
	mceilingclip = zeroarray;

	if (camera->player != NULL)
	{
		fixed_t centerhack = centeryfrac;
		fixed_t ofsx, ofsy;

		centery = viewheight >> 1;
		centeryfrac = centery << FRACBITS;

		P_BobWeapon (camera->player, &camera->player->psprites[ps_weapon], &ofsx, &ofsy);

		// add all active psprites
		for (i = 0, psp = camera->player->psprites;
			 i < NUMPSPRITES;
			 i++, psp++)
		{
			// [RH] Don't draw the targeter's crosshair if the player already has a crosshair set.
			if (psp->state && (i != ps_targetcenter || CrosshairImage == NULL))
			{
				R_DrawPSprite (psp, i, camera, psp->sx + ofsx, psp->sy + ofsy);
			}
			// [RH] Don't bob the targeter.
			if (i == ps_flash)
			{
				ofsx = ofsy = 0;
			}
		}

		centeryfrac = centerhack;
		centery = centerhack >> FRACBITS;
	}
}

//==========================================================================
//
// R_DrawRemainingPlayerSprites
//
// Called from D_Display to draw sprites that were not drawn by
// R_DrawPlayerSprites().
//
//==========================================================================

void R_DrawRemainingPlayerSprites()
{
	for (int i = 0; i < NUMPSPRITES; ++i)
	{
		vissprite_t *vis;
		
		vis = VisPSprites[i];
		VisPSprites[i] = NULL;

		if (vis != NULL)
		{
			FDynamicColormap *colormap = VisPSpritesBaseColormap[i];
			bool flip = vis->xiscale < 0;
			FSpecialColormap *special = NULL;
			PalEntry overlay = 0;
			FColormapStyle colormapstyle;
			bool usecolormapstyle = false;

			if (vis->Style.colormap >= SpecialColormaps[0].Colormap && 
				vis->Style.colormap < SpecialColormaps[SpecialColormaps.Size()].Colormap)
			{
				// Yuck! There needs to be a better way to store colormaps in the vissprite... :(
				ptrdiff_t specialmap = (vis->Style.colormap - SpecialColormaps[0].Colormap) / sizeof(FSpecialColormap);
				special = &SpecialColormaps[specialmap];
			}
			else if (colormap->Color == PalEntry(255,255,255) &&
				colormap->Desaturate == 0)
			{
				overlay = colormap->Fade;
				overlay.a = BYTE(((vis->Style.colormap - colormap->Maps) >> 8) * 255 / NUMCOLORMAPS);
			}
			else
			{
				usecolormapstyle = true;
				colormapstyle.Color = colormap->Color;
				colormapstyle.Fade = colormap->Fade;
				colormapstyle.Desaturate = colormap->Desaturate;
				colormapstyle.FadeLevel = ((vis->Style.colormap - colormap->Maps) >> 8) / float(NUMCOLORMAPS);
			}
			screen->DrawTexture(vis->pic,
				viewwindowx + VisPSpritesX1[i],
				viewwindowy + viewheight/2 - (vis->texturemid / 65536.0) * (vis->yscale / 65536.0) - 0.5,
				DTA_DestWidthF, FIXED2FLOAT(vis->pic->GetWidth() * vis->xscale),
				DTA_DestHeightF, FIXED2FLOAT(vis->pic->GetHeight() * vis->yscale),
				DTA_Translation, TranslationToTable(vis->Translation),
				DTA_FlipX, flip,
				DTA_TopOffset, 0,
				DTA_LeftOffset, 0,
				DTA_ClipLeft, viewwindowx,
				DTA_ClipTop, viewwindowy,
				DTA_ClipRight, viewwindowx + viewwidth,
				DTA_ClipBottom, viewwindowy + viewheight,
				DTA_Alpha, vis->Style.alpha,
				DTA_RenderStyle, vis->Style.RenderStyle,
				DTA_FillColor, vis->FillColor,
				DTA_SpecialColormap, special,
				DTA_ColorOverlay, overlay.d,
				DTA_ColormapStyle, usecolormapstyle ? &colormapstyle : NULL,
				TAG_DONE);
		}
	}
}




//
// R_SortVisSprites
//
// [RH] The old code for this function used a bubble sort, which was far less
//		than optimal with large numbers of sprites. I changed it to use the
//		stdlib qsort() function instead, and now it is a *lot* faster; the
//		more vissprites that need to be sorted, the better the performance
//		gain compared to the old function.
//
// Sort vissprites by depth, far to near

// This is the standard version, which does a simple test based on depth.
static bool sv_compare(vissprite_t *a, vissprite_t *b)
{
	return a->idepth > b->idepth;
}

// This is an alternate version, for when one or more voxel is in view.
// It does a 2D distance test based on whichever one is furthest from
// the viewpoint.
static bool sv_compare2d(vissprite_t *a, vissprite_t *b)
{
	return TVector2<double>(a->deltax, a->deltay).LengthSquared() <
		   TVector2<double>(b->deltax, b->deltay).LengthSquared();
}

#if 0
static drawseg_t **drawsegsorter;
static int drawsegsortersize = 0;

// Sort vissprites by leftmost column, left to right
static int STACK_ARGS sv_comparex (const void *arg1, const void *arg2)
{
	return (*(vissprite_t **)arg2)->x1 - (*(vissprite_t **)arg1)->x1;
}

// Sort drawsegs by rightmost column, left to right
static int STACK_ARGS sd_comparex (const void *arg1, const void *arg2)
{
	return (*(drawseg_t **)arg2)->x2 - (*(drawseg_t **)arg1)->x2;
}

CVAR (Bool, r_splitsprites, true, CVAR_ARCHIVE)

// Split up vissprites that intersect drawsegs
void R_SplitVisSprites ()
{
	size_t start, stop;
	size_t numdrawsegs = ds_p - firstdrawseg;
	size_t numsprites;
	size_t spr, dseg, dseg2;

	if (!r_splitsprites)
		return;

	if (numdrawsegs == 0 || vissprite_p - firstvissprite == 0)
		return;

	// Sort drawsegs from left to right
	if (numdrawsegs > drawsegsortersize)
	{
		if (drawsegsorter != NULL)
			delete[] drawsegsorter;
		drawsegsortersize = numdrawsegs * 2;
		drawsegsorter = new drawseg_t *[drawsegsortersize];
	}
	for (dseg = dseg2 = 0; dseg < numdrawsegs; ++dseg)
	{
		// Drawsegs that don't clip any sprites don't need to be considered.
		if (firstdrawseg[dseg].silhouette)
		{
			drawsegsorter[dseg2++] = &firstdrawseg[dseg];
		}
	}
	numdrawsegs = dseg2;
	if (numdrawsegs == 0)
	{
		return;
	}
	qsort (drawsegsorter, numdrawsegs, sizeof(drawseg_t *), sd_comparex);

	// Now sort vissprites from left to right, and walk them simultaneously
	// with the drawsegs, splitting any that intersect.
	start = firstvissprite - vissprites;

	int p = 0;
	do
	{
		p++;
		R_SortVisSprites (sv_comparex, start);
		stop = vissprite_p - vissprites;
		numsprites = stop - start;

		spr = dseg = 0;
		do
		{
			vissprite_t *vis = spritesorter[spr], *vis2;

			// Skip drawsegs until we get to one that doesn't end before the sprite
			// begins.
			while (dseg < numdrawsegs && drawsegsorter[dseg]->x2 <= vis->x1)
			{
				dseg++;
			}
			// Now split the sprite against any drawsegs it intersects
			for (dseg2 = dseg; dseg2 < numdrawsegs; dseg2++)
			{
				drawseg_t *ds = drawsegsorter[dseg2];

				if (ds->x1 > vis->x2 || ds->x2 < vis->x1)
					continue;

				if ((vis->idepth < ds->siz1) != (vis->idepth < ds->siz2))
				{ // The drawseg is crossed; find the x where the intersection occurs
					int cross = Scale (vis->idepth - ds->siz1, ds->sx2 - ds->sx1, ds->siz2 - ds->siz1) + ds->sx1 + 1;

/*					if (cross < ds->x1 || cross > ds->x2)
					{ // The original seg is crossed, but the drawseg is not
						continue;
					}
*/					if (cross <= vis->x1 || cross >= vis->x2)
					{ // Don't create 0-sized sprites
						continue;
					}

					vis->bSplitSprite = true;

					// Create a new vissprite for the right part of the sprite
					vis2 = R_NewVisSprite ();
					*vis2 = *vis;
					vis2->startfrac += vis2->xiscale * (cross - vis2->x1);
					vis->x2 = cross-1;
					vis2->x1 = cross;
					//vis2->alpha /= 2;
					//vis2->RenderStyle = STYLE_Add;

					if (vis->idepth < ds->siz1)
					{ // Left is in back, right is in front
						vis->sector  = ds->curline->backsector;
						vis2->sector = ds->curline->frontsector;
					}
					else
					{ // Right is in front, left is in back
						vis->sector  = ds->curline->frontsector;
						vis2->sector = ds->curline->backsector;
					}
				}
			}
		}
		while (dseg < numdrawsegs && ++spr < numsprites);

		// Repeat for any new sprites that were added.
	}
	while (start = stop, stop != vissprite_p - vissprites);
}
#endif

#ifdef __GNUC__
static void swap(vissprite_t *&a, vissprite_t *&b)
{
	vissprite_t *t = a;
	a = b;
	b = t;
}
#endif

void R_SortVisSprites (bool (*compare)(vissprite_t *, vissprite_t *), size_t first)
{
	int i;
	vissprite_t **spr;

	vsprcount = int(vissprite_p - &vissprites[first]);

	if (vsprcount == 0)
		return;

	if (spritesortersize < MaxVisSprites)
	{
		if (spritesorter != NULL)
			delete[] spritesorter;
		spritesorter = new vissprite_t *[MaxVisSprites];
		spritesortersize = MaxVisSprites;
	}

	if (!(i_compatflags & COMPATF_SPRITESORT))
	{
		for (i = 0, spr = firstvissprite; i < vsprcount; i++, spr++)
		{
			spritesorter[i] = *spr;
		}
	}
	else
	{
		// If the compatibility option is on sprites of equal distance need to
		// be sorted in inverse order. This is most easily achieved by
		// filling the sort array backwards before the sort.
		for (i = 0, spr = firstvissprite + vsprcount-1; i < vsprcount; i++, spr--)
		{
			spritesorter[i] = *spr;
		}
	}

	std::stable_sort(&spritesorter[0], &spritesorter[vsprcount], compare);
}


//
// R_DrawSprite
//
void R_DrawSprite (vissprite_t *spr)
{
	static short clipbot[MAXWIDTH];
	static short cliptop[MAXWIDTH];
	drawseg_t *ds;
	int i;
	int x1, x2;
	int r1, r2;
	short topclip, botclip;
	short *clip1, *clip2;
	lighttable_t *colormap = spr->Style.colormap;
	F3DFloor *rover;
	FDynamicColormap *mybasecolormap;

	// [RH] Check for particles
	if (!spr->bIsVoxel && spr->pic == NULL)
	{
		// kg3D - reject invisible parts
		if ((fake3D & FAKE3D_CLIPBOTTOM) && spr->gz <= sclipBottom) return;
		if ((fake3D & FAKE3D_CLIPTOP)    && spr->gz >= sclipTop) return;
		R_DrawParticle (spr);
		return;
	}

	x1 = spr->x1;
	x2 = spr->x2;

	// [RH] Quickly reject sprites with bad x ranges.
	if (x1 > x2)
		return;

	// [RH] Sprites split behind a one-sided line can also be discarded.
	if (spr->sector == NULL)
		return;

	// kg3D - reject invisible parts
	if ((fake3D & FAKE3D_CLIPBOTTOM) && spr->gzt <= sclipBottom) return;
	if ((fake3D & FAKE3D_CLIPTOP)    && spr->gzb >= sclipTop) return;

	// kg3D - correct colors now
	if (!fixedcolormap && fixedlightlev < 0 && spr->sector->e && spr->sector->e->XFloor.lightlist.Size()) 
	{
		if (!(fake3D & FAKE3D_CLIPTOP))
		{
			sclipTop = spr->sector->ceilingplane.ZatPoint(viewx, viewy);
		}
		sector_t *sec = NULL;
		for (i = spr->sector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
		{
			if (sclipTop <= spr->sector->e->XFloor.lightlist[i].plane.Zat0()) 
			{
				rover = spr->sector->e->XFloor.lightlist[i].caster;
				if (rover) 
				{
					if (rover->flags & FF_DOUBLESHADOW && sclipTop <= rover->bottom.plane->Zat0())
					{
						break;
					}
					sec = rover->model;
					if (rover->flags & FF_FADEWALLS)
					{
						mybasecolormap = sec->ColorMap;
					}
					else
					{
						mybasecolormap = spr->sector->e->XFloor.lightlist[i].extra_colormap;
					}
				}
				break;
			}
		}
		// found new values, recalculate
		if (sec) 
		{
			INTBOOL invertcolormap = (spr->Style.RenderStyle.Flags & STYLEF_InvertOverlay);

			if (spr->Style.RenderStyle.Flags & STYLEF_InvertSource)
			{
				invertcolormap = !invertcolormap;
			}

			// Sprites that are added to the scene must fade to black.
			if (spr->Style.RenderStyle == LegacyRenderStyles[STYLE_Add] && mybasecolormap->Fade != 0)
			{
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, 0, mybasecolormap->Desaturate);
			}

			if (spr->Style.RenderStyle.Flags & STYLEF_FadeToBlack)
			{
				if (invertcolormap)
				{ // Fade to white
					mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(255,255,255), mybasecolormap->Desaturate);
					invertcolormap = false;
				}
				else
				{ // Fade to black
					mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(0,0,0), mybasecolormap->Desaturate);
				}
			}

			// get light level
			if (invertcolormap)
			{
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
			}
			if (fixedlightlev >= 0)
			{
				spr->Style.colormap = mybasecolormap->Maps + fixedlightlev;
			}
			else if (!foggy && (spr->renderflags & RF_FULLBRIGHT))
			{ // full bright
				spr->Style.colormap = mybasecolormap->Maps;
			}
			else
			{ // diminished light
				spriteshade = LIGHT2SHADE(sec->lightlevel + r_actualextralight);
				spr->Style.colormap = mybasecolormap->Maps + (GETPALOOKUP (
					(fixed_t)DivScale12 (r_SpriteVisibility, spr->depth), spriteshade) << COLORMAPSHIFT);
			}
		}
	}

	// [RH] Initialize the clipping arrays to their largest possible range
	// instead of using a special "not clipped" value. This eliminates
	// visual anomalies when looking down and should be faster, too.
	topclip = 0;
	botclip = viewheight;

	// killough 3/27/98:
	// Clip the sprite against deep water and/or fake ceilings.
	// [RH] rewrote this to be based on which part of the sector is really visible

	fixed_t scale = MulScale19 (InvZtoScale, spr->idepth);
	fixed_t hzb = FIXED_MIN, hzt = FIXED_MAX;

	if (spr->bIsVoxel && spr->floorclip != 0)
	{
		hzb = spr->gzb;
	}

	if (spr->heightsec && !(spr->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{ // only things in specially marked sectors
		if (spr->FakeFlatStat != FAKED_AboveCeiling)
		{
			fixed_t hz = spr->heightsec->floorplane.ZatPoint (spr->gx, spr->gy);
			fixed_t h = (centeryfrac - FixedMul (hz-viewz, scale)) >> FRACBITS;

			if (spr->FakeFlatStat == FAKED_BelowFloor)
			{ // seen below floor: clip top
				if (!spr->bIsVoxel && h > topclip)
				{
					topclip = MIN<short> (h, viewheight);
				}
				hzt = MIN(hzt, hz);
			}
			else
			{ // seen in the middle: clip bottom
				if (!spr->bIsVoxel && h < botclip)
				{
					botclip = MAX<short> (0, h);
				}
				hzb = MAX(hzb, hz);
			}
		}
		if (spr->FakeFlatStat != FAKED_BelowFloor && !(spr->heightsec->MoreFlags & SECF_FAKEFLOORONLY))
		{
			fixed_t hz = spr->heightsec->ceilingplane.ZatPoint (spr->gx, spr->gy);
			fixed_t h = (centeryfrac - FixedMul (hz-viewz, scale)) >> FRACBITS;

			if (spr->FakeFlatStat == FAKED_AboveCeiling)
			{ // seen above ceiling: clip bottom
				if (!spr->bIsVoxel && h < botclip)
				{
					botclip = MAX<short> (0, h);
				}
				hzb = MAX(hzb, hz);
			}
			else
			{ // seen in the middle: clip top
				if (!spr->bIsVoxel && h > topclip)
				{
					topclip = MIN<short> (h, viewheight);
				}
				hzt = MIN(hzt, hz);
			}
		}
	}
	// killough 3/27/98: end special clipping for deep water / fake ceilings
	else if (!spr->bIsVoxel && spr->floorclip)
	{ // [RH] Move floorclip stuff from R_DrawVisSprite to here
		int clip = ((centeryfrac - FixedMul (spr->texturemid -
			(spr->pic->GetHeight() << FRACBITS) +
			spr->floorclip, spr->yscale)) >> FRACBITS);
		if (clip < botclip)
		{
			botclip = MAX<short> (0, clip);
		}
	}

	if (fake3D & FAKE3D_CLIPBOTTOM)
	{
		if (!spr->bIsVoxel)
		{
			fixed_t h = sclipBottom;
			if (spr->fakefloor)
			{
				fixed_t floorz = spr->fakefloor->top.plane->Zat0();
				if (viewz > floorz && floorz == sclipBottom )
				{
					h = spr->fakefloor->bottom.plane->Zat0();
				}
			}
			h = (centeryfrac - FixedMul(h-viewz, scale)) >> FRACBITS;
			if (h < botclip)
			{
				botclip = MAX<short>(0, h);
			}
		}
		hzb = MAX(hzb, sclipBottom);
	}
	if (fake3D & FAKE3D_CLIPTOP)
	{
		if (!spr->bIsVoxel)
		{
			fixed_t h = sclipTop;

			if (spr->fakeceiling != NULL)
			{
				fixed_t ceilingz = spr->fakeceiling->bottom.plane->Zat0();
				if (viewz < ceilingz && ceilingz == sclipTop)
				{
					h = spr->fakeceiling->top.plane->Zat0();
				}
			}
			h = (centeryfrac - FixedMul (h-viewz, scale)) >> FRACBITS;
			if (h > topclip)
			{
				topclip = MIN<short>(h, viewheight);
			}
		}
		hzt = MIN(hzt, sclipTop);
	}

#if 0
	// [RH] Sprites that were split by a drawseg should also be clipped
	// by the sector's floor and ceiling. (Not sure how/if to handle this
	// with fake floors, since those already do clipping.)
	if (spr->bSplitSprite &&
		(spr->heightsec == NULL || (spr->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)))
	{
		fixed_t h = spr->sector->floorplane.ZatPoint (spr->gx, spr->gy);
		h = (centeryfrac - FixedMul (h-viewz, scale)) >> FRACBITS;
		if (h < botclip)
		{
			botclip = MAX<short> (0, h);
		}
		h = spr->sector->ceilingplane.ZatPoint (spr->gx, spr->gy);
		h = (centeryfrac - FixedMul (h-viewz, scale)) >> FRACBITS;
		if (h > topclip)
		{
			topclip = MIN<short> (h, viewheight);
		}
	}
#endif

	if (topclip >= botclip)
	{
		spr->Style.colormap = colormap;
		return;
	}

	i = x2 - x1 + 1;
	clip1 = clipbot + x1;
	clip2 = cliptop + x1;
	do
	{
		*clip1++ = botclip;
		*clip2++ = topclip;
	} while (--i);

	// Scan drawsegs from end to start for obscuring segs.
	// The first drawseg that is closer than the sprite is the clip seg.

	// Modified by Lee Killough:
	// (pointer check was originally nonportable
	// and buggy, by going past LEFT end of array):

	//		for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

	for (ds = ds_p; ds-- > firstdrawseg; )  // new -- killough
	{
		// kg3D - no clipping on fake segs
		if(ds->fake) continue;
		// determine if the drawseg obscures the sprite
		if (ds->x1 > x2 || ds->x2 < x1 ||
			(!(ds->silhouette & SIL_BOTH) && ds->maskedtexturecol == -1 &&
			 !ds->bFogBoundary) )
		{
			// does not cover sprite
			continue;
		}

		r1 = MAX<int> (ds->x1, x1);
		r2 = MIN<int> (ds->x2, x2);

		fixed_t neardepth, fardepth;
		if (ds->sz1 < ds->sz2)
		{
			neardepth = ds->sz1, fardepth = ds->sz2;
		}
		else
		{
			neardepth = ds->sz2, fardepth = ds->sz1;
		}
		if (neardepth > spr->depth || (fardepth > spr->depth &&
			// Check if sprite is in front of draw seg:
			DMulScale32(spr->gy - ds->curline->v1->y, ds->curline->v2->x - ds->curline->v1->x,
						ds->curline->v1->x - spr->gx, ds->curline->v2->y - ds->curline->v1->y) <= 0))
		{
			// seg is behind sprite, so draw the mid texture if it has one
			if (ds->maskedtexturecol != -1 || ds->bFogBoundary)
				R_RenderMaskedSegRange (ds, r1, r2);
			continue;
		}

		// clip this piece of the sprite
		// killough 3/27/98: optimized and made much shorter
		// [RH] Optimized further (at least for VC++;
		// other compilers should be at least as good as before)

		if (ds->silhouette & SIL_BOTTOM) //bottom sil
		{
			clip1 = clipbot + r1;
			clip2 = openings + ds->sprbottomclip + r1 - ds->x1;
			i = r2 - r1 + 1;
			do
			{
				if (*clip1 > *clip2)
					*clip1 = *clip2;
				clip1++;
				clip2++;
			} while (--i);
		}

		if (ds->silhouette & SIL_TOP)   // top sil
		{
			clip1 = cliptop + r1;
			clip2 = openings + ds->sprtopclip + r1 - ds->x1;
			i = r2 - r1 + 1;
			do
			{
				if (*clip1 < *clip2)
					*clip1 = *clip2;
				clip1++;
				clip2++;
			} while (--i);
		}
	}

	// all clipping has been performed, so draw the sprite

	if (!spr->bIsVoxel)
	{
		mfloorclip = clipbot;
		mceilingclip = cliptop;
		R_DrawVisSprite (spr);
	}
	else
	{
		// If it is completely clipped away, don't bother drawing it.
		if (cliptop[x2] >= clipbot[x2])
		{
			for (i = x1; i < x2; ++i)
			{
				if (cliptop[i] < clipbot[i])
				{
					break;
				}
			}
			if (i == x2)
			{
				spr->Style.colormap = colormap;
				return;
			}
		}
		// Add everything outside the left and right edges to the clipping array
		// for R_DrawVisVoxel().
		if (x1 > 0)
		{
			clearbufshort(cliptop, x1, viewheight);
		}
		if (x2 < viewwidth - 1)
		{
			clearbufshort(cliptop + x2 + 1, viewwidth - x2 - 1, viewheight);
		}
		int minvoxely = spr->gzt <= hzt ? 0 : (spr->gzt - hzt) / spr->yscale;
		int maxvoxely = spr->gzb > hzb ? INT_MAX : (spr->gzt - hzb) / spr->yscale;
		R_DrawVisVoxel(spr, minvoxely, maxvoxely, cliptop, clipbot);
	}
	spr->Style.colormap = colormap;
}

// kg3D:
// R_DrawMasked contains sorting
// original renamed to R_DrawMaskedSingle

void R_DrawMaskedSingle (bool renew)
{
	drawseg_t *ds;
	int i;

#if 0
	R_SplitVisSprites ();
#endif

	for (i = vsprcount; i > 0; i--)
	{
		R_DrawSprite (spritesorter[i-1]);
	}

	// render any remaining masked mid textures

	// Modified by Lee Killough:
	// (pointer check was originally nonportable
	// and buggy, by going past LEFT end of array):

	//		for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

	if (renew)
	{
		fake3D |= FAKE3D_REFRESHCLIP;
	}
	for (ds = ds_p; ds-- > firstdrawseg; )	// new -- killough
	{
		// kg3D - no fake segs
		if (ds->fake) continue;
		if (ds->maskedtexturecol != -1 || ds->bFogBoundary)
		{
			R_RenderMaskedSegRange (ds, ds->x1, ds->x2);
		}
	}
}

void R_DrawHeightPlanes(fixed_t height); // kg3D - fake planes

void R_DrawMasked (void)
{
	R_SortVisSprites (DrewAVoxel ? sv_compare2d : sv_compare, firstvissprite - vissprites);

	if (height_top == NULL)
	{ // kg3D - no visible 3D floors, normal rendering
		R_DrawMaskedSingle(false);
	}
	else
	{ // kg3D - correct sorting
		HeightLevel *hl;

		// ceilings
		for (hl = height_cur; hl != NULL && hl->height >= viewz; hl = hl->prev)
		{
			if (hl->next)
			{
				fake3D = FAKE3D_CLIPBOTTOM | FAKE3D_CLIPTOP;
				sclipTop = hl->next->height;
			}
			else
			{
				fake3D = FAKE3D_CLIPBOTTOM;
			}
			sclipBottom = hl->height;
			R_DrawMaskedSingle(true);
			R_DrawHeightPlanes(hl->height);
		}

		// floors
		fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPTOP;
		sclipTop = height_top->height;
		R_DrawMaskedSingle(true);
		hl = height_top;
		for (hl = height_top; hl != NULL && hl->height < viewz; hl = hl->next)
		{
			R_DrawHeightPlanes(hl->height);
			if (hl->next)
			{
				fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPTOP | FAKE3D_CLIPBOTTOM;
				sclipTop = hl->next->height;
			}
			else
			{
				fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPBOTTOM;
			}
			sclipBottom = hl->height;
			R_DrawMaskedSingle(true);
		}
		R_3D_DeleteHeights();
		fake3D = 0;
	}
	R_DrawPlayerSprites ();
}


void R_ProjectParticle (particle_t *particle, const sector_t *sector, int shade, int fakeside)
{
	fixed_t 			tr_x;
	fixed_t 			tr_y;
	fixed_t 			tx, ty;
	fixed_t 			tz, tiz;
	fixed_t 			xscale, yscale;
	int 				x1, x2, y1, y2;
	vissprite_t*		vis;
	sector_t*			heightsec = NULL;
	BYTE*				map;

	// transform the origin point
	tr_x = particle->x - viewx;
	tr_y = particle->y - viewy;

	tz = DMulScale20 (tr_x, viewtancos, tr_y, viewtansin);

	// particle is behind view plane?
	if (tz < MINZ)
		return;

	tx = DMulScale20 (tr_x, viewsin, -tr_y, viewcos);

	// Flip for mirrors
	if (MirrorFlags & RF_XFLIP)
	{
		tx = viewwidth - tx - 1;
	}

	// too far off the side?
	if (tz <= abs (tx))
		return;

	tiz = 268435456 / tz;
	xscale = centerx * tiz;

	// calculate edges of the shape
	int psize = particle->size << (12-3);

	x1 = MAX<int> (WindowLeft, (centerxfrac + MulScale12 (tx-psize, xscale)) >> FRACBITS);
	x2 = MIN<int> (WindowRight, (centerxfrac + MulScale12 (tx+psize, xscale)) >> FRACBITS);

	if (x1 >= x2)
		return;

	yscale = MulScale16 (yaspectmul, xscale);
	ty = particle->z - viewz;
	psize <<= 4;
	y1 = (centeryfrac - FixedMul (ty+psize, yscale)) >> FRACBITS;
	y2 = (centeryfrac - FixedMul (ty-psize, yscale)) >> FRACBITS;

	// Clip the particle now. Because it's a point and projected as its subsector is
	// entered, we don't need to clip it to drawsegs like a normal sprite.

	// Clip particles behind walls.
	if (y1 <  ceilingclip[x1])		y1 = ceilingclip[x1];
	if (y1 <  ceilingclip[x2-1])	y1 = ceilingclip[x2-1];
	if (y2 >= floorclip[x1])		y2 = floorclip[x1] - 1;
	if (y2 >= floorclip[x2-1])		y2 = floorclip[x2-1] - 1;

	if (y1 > y2)
		return;

	// Clip particles above the ceiling or below the floor.
	heightsec = sector->GetHeightSec();

	const secplane_t *topplane;
	const secplane_t *botplane;
	FTextureID toppic;
	FTextureID botpic;

	if (heightsec)	// only clip things which are in special sectors
	{
		if (fakeside == FAKED_AboveCeiling)
		{
			topplane = &sector->ceilingplane;
			botplane = &heightsec->ceilingplane;
			toppic = sector->GetTexture(sector_t::ceiling);
			botpic = heightsec->GetTexture(sector_t::ceiling);
			map = heightsec->ColorMap->Maps;
		}
		else if (fakeside == FAKED_BelowFloor)
		{
			topplane = &heightsec->floorplane;
			botplane = &sector->floorplane;
			toppic = heightsec->GetTexture(sector_t::floor);
			botpic = sector->GetTexture(sector_t::floor);
			map = heightsec->ColorMap->Maps;
		}
		else
		{
			topplane = &heightsec->ceilingplane;
			botplane = &heightsec->floorplane;
			toppic = heightsec->GetTexture(sector_t::ceiling);
			botpic = heightsec->GetTexture(sector_t::floor);
			map = sector->ColorMap->Maps;
		}
	}
	else
	{
		topplane = &sector->ceilingplane;
		botplane = &sector->floorplane;
		toppic = sector->GetTexture(sector_t::ceiling);
		botpic = sector->GetTexture(sector_t::floor);
		map = sector->ColorMap->Maps;
	}

	if (botpic != skyflatnum && particle->z < botplane->ZatPoint (particle->x, particle->y))
		return;
	if (toppic != skyflatnum && particle->z >= topplane->ZatPoint (particle->x, particle->y))
		return;

	// store information in a vissprite
	vis = R_NewVisSprite ();
	vis->heightsec = heightsec;
	vis->xscale = xscale;
//	vis->yscale = FixedMul (xscale, InvZtoScale);
	vis->yscale = xscale;
	vis->depth = tz;
	vis->idepth = (DWORD)DivScale32 (1, tz) >> 1;
	vis->cx = tx;
	vis->gx = particle->x;
	vis->gy = particle->y;
	vis->gz = particle->z; // kg3D
	vis->gzb = y1;
	vis->gzt = y2;
	vis->x1 = x1;
	vis->x2 = x2;
	vis->Translation = 0;
	vis->startfrac = 255 & (particle->color >>24);
	vis->pic = NULL;
	vis->bIsVoxel = false;
	vis->renderflags = particle->trans;
	vis->FakeFlatStat = fakeside;
	vis->floorclip = 0;
	vis->ColormapNum = 0;

	if (fixedlightlev >= 0)
	{
		vis->Style.colormap = map + fixedlightlev;
	}
	else if (fixedcolormap)
	{
		vis->Style.colormap = fixedcolormap;
	}
	else if(particle->bright) {
		vis->Style.colormap = map;
	}
	else
	{
		// Using MulScale15 instead of 16 makes particles slightly more visible
		// than regular sprites.
		vis->ColormapNum = GETPALOOKUP(MulScale15 (tiz, r_SpriteVisibility), shade);
		vis->Style.colormap = map + (vis->ColormapNum << COLORMAPSHIFT);
	}
}

static void R_DrawMaskedSegsBehindParticle (const vissprite_t *vis)
{
	const int x1 = vis->x1;
	const int x2 = vis->x2;

	// Draw any masked textures behind this particle so that when the
	// particle is drawn, it will be in front of them.
	for (unsigned int p = InterestingDrawsegs.Size(); p-- > FirstInterestingDrawseg; )
	{
		drawseg_t *ds = &drawsegs[InterestingDrawsegs[p]];
		// kg3D - no fake segs
		if(ds->fake) continue;
		if (ds->x1 >= x2 || ds->x2 < x1)
		{
			continue;
		}
		if (Scale (ds->siz2 - ds->siz1, (x2 + x1)/2 - ds->sx1, ds->sx2 - ds->sx1) + ds->siz1 < vis->idepth)
		{
			R_RenderMaskedSegRange (ds, MAX<int> (ds->x1, x1), MIN<int> (ds->x2, x2-1));
		}
	}
}

void R_DrawParticle (vissprite_t *vis)
{
	DWORD *bg2rgb;
	int spacing;
	BYTE *dest;
	DWORD fg;
	BYTE color = vis->Style.colormap[vis->startfrac];
	int yl = vis->gzb;
	int ycount = vis->gzt - yl + 1;
	int x1 = vis->x1;
	int countbase = vis->x2 - x1 + 1;

	R_DrawMaskedSegsBehindParticle (vis);

	// vis->renderflags holds translucency level (0-255)
	{
		fixed_t fglevel, bglevel;
		DWORD *fg2rgb;

		fglevel = ((vis->renderflags + 1) << 8) & ~0x3ff;
		bglevel = FRACUNIT-fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
		fg = fg2rgb[color];
	}

	spacing = RenderTarget->GetPitch() - countbase;
	dest = ylookup[yl] + x1 + dc_destorg;

	do
	{
		int count = countbase;
		do
		{
			DWORD bg = bg2rgb[*dest];
			bg = (fg+bg) | 0x1f07c1f;
			*dest++ = RGB32k[0][0][bg & (bg>>15)];
		} while (--count);
		dest += spacing;
	} while (--ycount);
}

extern fixed_t baseyaspectmul;

void R_DrawVoxel(fixed_t globalposx, fixed_t globalposy, fixed_t globalposz, angle_t viewang,
	fixed_t dasprx, fixed_t daspry, fixed_t dasprz, angle_t dasprang,
	fixed_t daxscale, fixed_t dayscale, FVoxel *voxobj,
	lighttable_t *colormap, short *daumost, short *dadmost, int minslabz, int maxslabz, int flags)
{
	int i, j, k, x, y, syoff, ggxstart, ggystart, nxoff;
	fixed_t cosang, sinang, sprcosang, sprsinang;
	int backx, backy, gxinc, gyinc;
	int daxscalerecip, dayscalerecip, cnt, gxstart, gystart, dazscale;
	int lx, rx, nx, ny, x1=0, y1=0, x2=0, y2=0, yinc=0;
	int yoff, xs=0, ys=0, xe, ye, xi=0, yi=0, cbackx, cbacky, dagxinc, dagyinc;
	kvxslab_t *voxptr, *voxend;
	FVoxelMipLevel *mip;
	int z1a[64], z2a[64], yplc[64];

	const int nytooclose = centerxwide * 2100, nytoofar = 32768*32768 - 1048576;
	const int xdimenscale = Scale(centerxwide, yaspectmul, 160);
	const double centerxwide_f = centerxwide;
	const double centerxwidebig_f = centerxwide_f * 65536*65536*8;

	// Convert to Build's coordinate system.
	globalposx =  globalposx >> 12;
	globalposy = -globalposy >> 12;
	globalposz = -globalposz >> 8;

	dasprx =  dasprx >> 12;
	daspry = -daspry >> 12;
	dasprz = -dasprz >> 8;

	// Shift the scales from 16 bits of fractional precision to 6.
	// Also do some magic voodoo scaling to make them the right size.
	daxscale = daxscale / (0xC000 >> 6);
	dayscale = dayscale / (0xC000 >> 6);

	cosang = finecosine[viewang >> ANGLETOFINESHIFT] >> 2;
	sinang = -finesine[viewang >> ANGLETOFINESHIFT] >> 2;
	sprcosang = finecosine[dasprang >> ANGLETOFINESHIFT] >> 2;
	sprsinang = -finesine[dasprang >> ANGLETOFINESHIFT] >> 2;

	R_SetupDrawSlab(colormap);

	// Select mip level
	i = abs(DMulScale6(dasprx - globalposx, cosang, daspry - globalposy, sinang));
	i = DivScale6(i, MIN(daxscale, dayscale));
	j = FocalLengthX >> 3;
	for (k = 0; i >= j && k < voxobj->NumMips; ++k)
	{
		i >>= 1;
	}
	if (k >= voxobj->NumMips) k = voxobj->NumMips - 1;

	mip = &voxobj->Mips[k];		if (mip->SlabData == NULL) return;

	minslabz >>= k;
	maxslabz >>= k;

	daxscale <<= (k+8); dayscale <<= (k+8);
	dazscale = FixedDiv(dayscale, baseyaspectmul);
	daxscale = FixedDiv(daxscale, yaspectmul);
	daxscale = Scale(daxscale, xdimenscale, centerxwide << 9);
	dayscale = Scale(dayscale, FixedMul(xdimenscale, viewingrangerecip), centerxwide << 9);

	daxscalerecip = (1<<30) / daxscale;
	dayscalerecip = (1<<30) / dayscale;

	x = FixedMul(globalposx - dasprx, daxscalerecip);
	y = FixedMul(globalposy - daspry, daxscalerecip);
	backx = (DMulScale10(x, sprcosang, y,  sprsinang) + mip->PivotX) >> 8;
	backy = (DMulScale10(y, sprcosang, x, -sprsinang) + mip->PivotY) >> 8;
	cbackx = clamp(backx, 0, mip->SizeX - 1);
	cbacky = clamp(backy, 0, mip->SizeY - 1);

	sprcosang = MulScale14(daxscale, sprcosang);
	sprsinang = MulScale14(daxscale, sprsinang);

	x = (dasprx - globalposx) - DMulScale18(mip->PivotX, sprcosang, mip->PivotY, -sprsinang);
	y = (daspry - globalposy) - DMulScale18(mip->PivotY, sprcosang, mip->PivotX,  sprsinang);

	cosang = FixedMul(cosang, dayscalerecip);
	sinang = FixedMul(sinang, dayscalerecip);

	gxstart = y*cosang - x*sinang;
	gystart = x*cosang + y*sinang;
	gxinc = DMulScale10(sprsinang, cosang, sprcosang, -sinang);
	gyinc = DMulScale10(sprcosang, cosang, sprsinang,  sinang);
	if ((abs(globalposz - dasprz) >> 10) >= abs(dazscale)) return;

	x = 0; y = 0; j = MAX(mip->SizeX, mip->SizeY);
	fixed_t *ggxinc = (fixed_t *)alloca((j + 1) * sizeof(fixed_t) * 2);
	fixed_t *ggyinc = ggxinc + (j + 1);
	for (i = 0; i <= j; i++)
	{
		ggxinc[i] = x; x += gxinc;
		ggyinc[i] = y; y += gyinc;
	}

	syoff = DivScale21(globalposz - dasprz, FixedMul(dazscale, 0xE800)) + (mip->PivotZ << 7);
	yoff = (abs(gxinc) + abs(gyinc)) >> 1;

	for (cnt = 0; cnt < 8; cnt++)
	{
		switch (cnt)
		{
			case 0: xs = 0;				ys = 0;				xi =  1; yi =  1; break;
			case 1: xs = mip->SizeX-1;	ys = 0;				xi = -1; yi =  1; break;
			case 2: xs = 0;				ys = mip->SizeY-1;	xi =  1; yi = -1; break;
			case 3: xs = mip->SizeX-1;	ys = mip->SizeY-1;	xi = -1; yi = -1; break;
			case 4: xs = 0;				ys = cbacky;		xi =  1; yi =  2; break;
			case 5: xs = mip->SizeX-1;	ys = cbacky;		xi = -1; yi =  2; break;
			case 6: xs = cbackx;		ys = 0;				xi =  2; yi =  1; break;
			case 7: xs = cbackx;		ys = mip->SizeY-1;	xi =  2; yi = -1; break;
		}
		xe = cbackx; ye = cbacky;
		if (cnt < 4)
		{
			if ((xi < 0) && (xe >= xs)) continue;
			if ((xi > 0) && (xe <= xs)) continue;
			if ((yi < 0) && (ye >= ys)) continue;
			if ((yi > 0) && (ye <= ys)) continue;
		}
		else
		{
			if ((xi < 0) && (xe > xs)) continue;
			if ((xi > 0) && (xe < xs)) continue;
			if ((yi < 0) && (ye > ys)) continue;
			if ((yi > 0) && (ye < ys)) continue;
			xe += xi; ye += yi;
		}

		i = ksgn(ys-backy)+ksgn(xs-backx)*3+4;
		switch(i)
		{
			case 6: case 7: x1 = 0;				y1 = 0;				break;
			case 8: case 5: x1 = gxinc;			y1 = gyinc;			break;
			case 0: case 3: x1 = gyinc;			y1 = -gxinc;		break;
			case 2: case 1: x1 = gxinc+gyinc;	y1 = gyinc-gxinc;	break;
		}
		switch(i)
		{
			case 2: case 5: x2 = 0;				y2 = 0;				break;
			case 0: case 1: x2 = gxinc;			y2 = gyinc;			break;
			case 8: case 7: x2 = gyinc;			y2 = -gxinc;		break;
			case 6: case 3: x2 = gxinc+gyinc;	y2 = gyinc-gxinc;	break;
		}
		BYTE oand = (1 << int(xs<backx)) + (1 << (int(ys<backy)+2));
		BYTE oand16 = oand + 16;
		BYTE oand32 = oand + 32;

		if (yi > 0) { dagxinc =  gxinc; dagyinc =  FixedMul(gyinc, viewingrangerecip); }
			   else { dagxinc = -gxinc; dagyinc = -FixedMul(gyinc, viewingrangerecip); }

			/* Fix for non 90 degree viewing ranges */
		nxoff = FixedMul(x2 - x1, viewingrangerecip);
		x1 = FixedMul(x1, viewingrangerecip);

		ggxstart = gxstart + ggyinc[ys];
		ggystart = gystart - ggxinc[ys];

		for (x = xs; x != xe; x += xi)
		{
			BYTE *slabxoffs = &mip->SlabData[mip->OffsetX[x]];
			short *xyoffs = &mip->OffsetXY[x * (mip->SizeY + 1)];

			nx = FixedMul(ggxstart + ggxinc[x], viewingrangerecip) + x1;
			ny = ggystart + ggyinc[x];
			for (y = ys; y != ye; y += yi, nx += dagyinc, ny -= dagxinc)
			{
				if ((ny <= nytooclose) || (ny >= nytoofar)) continue;
				voxptr = (kvxslab_t *)(slabxoffs + xyoffs[y]);
				voxend = (kvxslab_t *)(slabxoffs + xyoffs[y+1]);
				if (voxptr >= voxend) continue;

				lx = xs_RoundToInt(nx * centerxwide_f / (ny + y1)) + centerx;
				if (lx < 0) lx = 0;
				rx = xs_RoundToInt((nx + nxoff) * centerxwide_f / (ny + y2)) + centerx;
				if (rx > viewwidth) rx = viewwidth;
				if (rx <= lx) continue;

				if (flags & DVF_MIRRORED)
				{
					int t = viewwidth - lx;
					lx = viewwidth - rx;
					rx = t;
				}

				fixed_t l1 = xs_RoundToInt(centerxwidebig_f / (ny - yoff));
				fixed_t l2 = xs_RoundToInt(centerxwidebig_f / (ny + yoff));
				for (; voxptr < voxend; voxptr = (kvxslab_t *)((BYTE *)voxptr + voxptr->zleng + 3))
				{
					const BYTE *col = voxptr->col;
					int zleng = voxptr->zleng;
					int ztop = voxptr->ztop;
					fixed_t z1, z2;

					if (ztop < minslabz)
					{
						int diff = minslabz - ztop;
						ztop = minslabz;
						col += diff;
						zleng -= diff;
					}
					if (ztop + zleng > maxslabz)
					{
						int diff = ztop + zleng - maxslabz;
						zleng -= diff;
					}
					if (zleng <= 0) continue;

					j = (ztop << 15) - syoff;
					if (j < 0)
					{
						k = j + (zleng << 15);
						if (k < 0)
						{
							if ((voxptr->backfacecull & oand32) == 0) continue;
							z2 = MulScale32(l2, k) + centery;					/* Below slab */
						}
						else
						{
							if ((voxptr->backfacecull & oand) == 0) continue;	/* Middle of slab */
							z2 = MulScale32(l1, k) + centery;
						}
						z1 = MulScale32(l1, j) + centery;
					}
					else
					{
						if ((voxptr->backfacecull & oand16) == 0) continue;
						z1 = MulScale32(l2, j) + centery;						/* Above slab */
						z2 = MulScale32(l1, j + (zleng << 15)) + centery;
					}

					if (z2 <= z1) continue;

					if (zleng == 1)
					{
						yinc = 0;
					}
					else
					{
						if (z2-z1 >= 1024) yinc = FixedDiv(zleng, z2 - z1);
						else yinc = (((1 << 24) - 1) / (z2 - z1)) * zleng >> 8;
					}
					// [RH] Clip each column separately, not just by the first one.
					for (int stripwidth = MIN<int>(countof(z1a), rx - lx), lxt = lx;
						lxt < rx;
						(lxt += countof(z1a)), stripwidth = MIN<int>(countof(z1a), rx - lxt))
					{
						// Calculate top and bottom pixels locations
						for (int xxx = 0; xxx < stripwidth; ++xxx)
						{
							if (zleng == 1)
							{
								yplc[xxx] = 0;
								z1a[xxx] = MAX<int>(z1, daumost[lxt + xxx]);
							}
							else
							{
								if (z1 < daumost[lxt + xxx])
								{
									yplc[xxx] = yinc * (daumost[lxt + xxx] - z1);
									z1a[xxx] = daumost[lxt + xxx];
								}
								else
								{
									yplc[xxx] = 0;
									z1a[xxx] = z1;
								}
							}
							z2a[xxx] = MIN<int>(z2, dadmost[lxt + xxx]);
						}
						// Find top and bottom pixels that match and draw them as one strip
						for (int xxl = 0, xxr; xxl < stripwidth; )
						{
							if (z1a[xxl] >= z2a[xxl])
							{ // No column here
								xxl++;
								continue;
							}
							int z1 = z1a[xxl];
							int z2 = z2a[xxl];
							// How many columns share the same extents?
							for (xxr = xxl + 1; xxr < stripwidth; ++xxr)
							{
								if (z1a[xxr] != z1 || z2a[xxr] != z2)
									break;
							}

							if (!(flags & DVF_OFFSCREEN))
							{
								// Draw directly to the screen.
								R_DrawSlab(xxr - xxl, yplc[xxl], z2 - z1, yinc, col, ylookup[z1] + lxt + xxl + dc_destorg);
							}
							else
							{
								// Record the area covered and possibly draw to an offscreen buffer.
								dc_yl = z1;
								dc_yh = z2 - 1;
								dc_count = z2 - z1;
								dc_iscale = yinc;
								for (int x = xxl; x < xxr; ++x)
								{
									OffscreenCoverageBuffer->InsertSpan(lxt + x, z1, z2);
									if (!(flags & DVF_SPANSONLY))
									{
										dc_x = lxt + x;
										rt_initcols(OffscreenColorBuffer + (dc_x & ~3) * OffscreenBufferHeight);
										dc_source = col;
										dc_texturefrac = yplc[xxl];
										hcolfunc_pre();
									}
								}
							}
							xxl = xxr;
						}
					}
				}
			}
		}
	}
}

//==========================================================================
//
// FCoverageBuffer Constructor
//
//==========================================================================

FCoverageBuffer::FCoverageBuffer(int lists)
	: Spans(NULL), FreeSpans(NULL)
{
	NumLists = lists;
	Spans = new Span *[lists];
	memset(Spans, 0, sizeof(Span*)*lists);
}

//==========================================================================
//
// FCoverageBuffer Destructor
//
//==========================================================================

FCoverageBuffer::~FCoverageBuffer()
{
	if (Spans != NULL)
	{
		delete[] Spans;
	}
}

//==========================================================================
//
// FCoverageBuffer :: Clear
//
//==========================================================================

void FCoverageBuffer::Clear()
{
	SpanArena.FreeAll();
	memset(Spans, 0, sizeof(Span*)*NumLists);
	FreeSpans = NULL;
}

//==========================================================================
//
// FCoverageBuffer :: InsertSpan
//
// start is inclusive.
// stop is exclusive.
//
//==========================================================================

void FCoverageBuffer::InsertSpan(int listnum, int start, int stop)
{
	assert(unsigned(listnum) < NumLists);
	assert(start < stop);

	Span **span_p = &Spans[listnum];
	Span *span;

	if (*span_p == NULL || (*span_p)->Start > stop)
	{ // This list is empty or the first entry is after this one, so we can just insert the span.
		goto addspan;
	}

	// Insert the new span in order, merging with existing ones.
	while (*span_p != NULL)
	{
		if ((*span_p)->Stop < start)							// =====		(existing span)
		{ // Span ends before this one starts.					//		  ++++	(new span)
			span_p = &(*span_p)->NextSpan;
			continue;
		}

		// Does the new span overlap or abut the existing one?
		if ((*span_p)->Start <= start)
		{
			if ((*span_p)->Stop >= stop)						// =============
			{ // The existing span completely covers this one.	//     +++++
				return;
			}
extend:		// Extend the existing span with the new one.		// ======
			span = *span_p;										//     +++++++
			span->Stop = stop;									// (or)  +++++

			// Free up any spans we just covered up.
			span_p = &(*span_p)->NextSpan;
			while (*span_p != NULL && (*span_p)->Start <= stop && (*span_p)->Stop <= stop)
			{
				Span *span = *span_p;							// ======  ======
				*span_p = span->NextSpan;						//     +++++++++++++
				span->NextSpan = FreeSpans;
				FreeSpans = span;
			}
			if (*span_p != NULL && (*span_p)->Start <= stop)	// =======         ========
			{ // Our new span connects two existing spans.		//     ++++++++++++++
			  // They should all be collapsed into a single span.
				span->Stop = (*span_p)->Stop;
				span = *span_p;
				*span_p = span->NextSpan;
				span->NextSpan = FreeSpans;
				FreeSpans = span;
			}
			goto check;
		}
		else if ((*span_p)->Start <= stop)						//        =====
		{ // The new span extends the existing span from		//    ++++
		  // the beginning.										// (or) ++++
			(*span_p)->Start = start;
			if ((*span_p)->Stop < stop)
			{ // The new span also extends the existing span	//     ======
			  // at the bottom									// ++++++++++++++
				goto extend;
			}
			goto check;
		}
		else													//         ======
		{ // No overlap, so insert a new span.					// +++++
			goto addspan;
		}
	}
	// Append a new span to the end of the list.
addspan:
	span = AllocSpan();
	span->NextSpan = *span_p;
	span->Start = start;
	span->Stop = stop;
	*span_p = span;
check:
#ifdef _DEBUG
	// Validate the span list: Spans must be in order, and there must be
	// at least one pixel between spans.
	for (span = Spans[listnum]; span != NULL; span = span->NextSpan)
	{
		assert(span->Start < span->Stop);
		if (span->NextSpan != NULL)
		{
			assert(span->Stop < span->NextSpan->Start);
		}
	}
#endif
	;
}

//==========================================================================
//
// FCoverageBuffer :: AllocSpan
//
//==========================================================================

FCoverageBuffer::Span *FCoverageBuffer::AllocSpan()
{
	Span *span;

	if (FreeSpans != NULL)
	{
		span = FreeSpans;
		FreeSpans = span->NextSpan;
	}
	else
	{
		span = (Span *)SpanArena.Alloc(sizeof(Span));
	}
	return span;
}

//==========================================================================
//
// R_CheckOffscreenBuffer
//
// Allocates the offscreen coverage buffer and optionally the offscreen
// color buffer. If they already exist but are the wrong size, they will
// be reallocated.
//
//==========================================================================

void R_CheckOffscreenBuffer(int width, int height, bool spansonly)
{
	if (OffscreenCoverageBuffer == NULL)
	{
		assert(OffscreenColorBuffer == NULL && "The color buffer cannot exist without the coverage buffer");
		OffscreenCoverageBuffer = new FCoverageBuffer(width);
	}
	else if (OffscreenCoverageBuffer->NumLists != (unsigned)width)
	{
		delete OffscreenCoverageBuffer;
		OffscreenCoverageBuffer = new FCoverageBuffer(width);
		if (OffscreenColorBuffer != NULL)
		{
			delete[] OffscreenColorBuffer;
			OffscreenColorBuffer = NULL;
		}
	}
	else
	{
		OffscreenCoverageBuffer->Clear();
	}

	if (!spansonly)
	{
		if (OffscreenColorBuffer == NULL)
		{
			OffscreenColorBuffer = new BYTE[width * height];
		}
		else if (OffscreenBufferWidth != width || OffscreenBufferHeight != height)
		{
			delete[] OffscreenColorBuffer;
			OffscreenColorBuffer = new BYTE[width * height];
		}
	}
	OffscreenBufferWidth = width;
	OffscreenBufferHeight = height;
}
