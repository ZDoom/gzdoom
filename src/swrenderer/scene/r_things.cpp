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
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "p_lnspec.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_things.h"
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
#include "r_3dfloors.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/drawers/r_draw_pal.h"
#include "v_palette.h"
#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "r_data/voxels.h"
#include "p_local.h"
#include "p_maputl.h"
#include "swrenderer/things/r_voxel.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "r_portal.h"
#include "swrenderer/things/r_particle.h"
#include "swrenderer/things/r_playersprite.h"
#include "swrenderer/things/r_wallsprite.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/r_memory.h"

EXTERN_CVAR(Int, r_drawfuzz)
EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Bool, r_blendmethod)

CVAR(Bool, r_fullbrightignoresectorcolor, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, r_splitsprites, true, CVAR_ARCHIVE)

namespace swrenderer
{
	using namespace drawerargs;

//
// Sprite rotation 0 is facing the viewer,
//	rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//	which increases counter clockwise (protractor).
//
double 			pspritexscale;
double	 		pspritexiscale;
double			pspriteyscale;
fixed_t			sky1scale;			// [RH] Sky 1 scale factor
fixed_t			sky2scale;			// [RH] Sky 2 scale factor

int		spriteshade;

FTexture		*WallSpriteTile;

//
// INITIALIZATION FUNCTIONS
//


//

// GAME FUNCTIONS
//
bool			DrewAVoxel;

static vissprite_t **spritesorter;
static int spritesortersize = 0;
static int vsprcount;




void R_DeinitSprites()
{
	R_DeinitVisSprites();
	R_DeinitRenderVoxel();

	// Free vissprites sorter
	if (spritesorter != NULL)
	{
		delete[] spritesorter;
		spritesortersize = 0;
		spritesorter = NULL;
	}
}

//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites (void)
{
	R_ClearVisSprites();
	DrewAVoxel = false;
}

static TArray<drawseg_t *> portaldrawsegs;

static inline void R_CollectPortals()
{
	// This function collects all drawsegs that may be of interest to R_ClipSpriteColumnWithPortals 
	// Having that function over the entire list of drawsegs can break down performance quite drastically.
	// This is doing the costly stuff only once so that R_ClipSpriteColumnWithPortals can 
	// a) exit early if no relevant info is found and
	// b) skip most of the collected drawsegs which have no portal attached.
	portaldrawsegs.Clear();
	for (drawseg_t* seg = ds_p; seg-- > firstdrawseg; ) // copied code from killough below
	{
		// I don't know what makes this happen (some old top-down portal code or possibly skybox code? something adds null lines...)
		// crashes at the first frame of the first map of Action2.wad
		if (!seg->curline) continue;

		line_t* line = seg->curline->linedef;
		// ignore minisegs from GL nodes.
		if (!line) continue;

		// check if this line will clip sprites to itself
		if (!line->isVisualPortal() && line->special != Line_Mirror)
			continue;

		// don't clip sprites with portal's back side (it's transparent)
		if (seg->curline->sidedef != line->sidedef[0])
			continue;

		portaldrawsegs.Push(seg);
	}
}

bool R_ClipSpriteColumnWithPortals(vissprite_t* spr)
{
	RenderPortal *renderportal = RenderPortal::Instance();
	
	// [ZZ] 10.01.2016: don't clip sprites from the root of a skybox.
	if (renderportal->CurrentPortalInSkybox)
		return false;

	for (drawseg_t *seg : portaldrawsegs)
	{
		// ignore segs from other portals
		if (seg->CurrentPortalUniq != renderportal->CurrentPortalUniq)
			continue;

		// (all checks that are already done in R_CollectPortals have been removed for performance reasons.)

		// don't clip if the sprite is in front of the portal
		if (!P_PointOnLineSidePrecise(spr->gpos.X, spr->gpos.Y, seg->curline->linedef))
			continue;

		// now if current column is covered by this drawseg, we clip it away
		if ((dc_x >= seg->x1) && (dc_x < seg->x2))
			return true;
	}

	return false;
}

//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//
void R_ProjectSprite (AActor *thing, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector)
{
	double 			tr_x;
	double 			tr_y;
	
	double				gzt;				// killough 3/27/98
	double				gzb;				// [RH] use bottom of sprite, not actor
	double	 			tx;// , tx2;
	double 				tz;

	double 				xscale = 1, yscale = 1;
	
	int 				x1;
	int 				x2;

	FTextureID			picnum;
	FTexture			*tex;
	FVoxelDef			*voxel;
	
	vissprite_t*		vis;
	
	fixed_t 			iscale;

	sector_t*			heightsec;			// killough 3/27/98

	// Don't waste time projecting sprites that are definitely not visible.
	if (thing == NULL ||
		(thing->renderflags & RF_INVISIBLE) ||
		!thing->RenderStyle.IsVisible(thing->Alpha) ||
		!thing->IsVisibleToPlayer() ||
		!thing->IsInsideVisibleAngles())
	{
		return;
	}
	
	RenderPortal *renderportal = RenderPortal::Instance();

	// [ZZ] Or less definitely not visible (hue)
	// [ZZ] 10.01.2016: don't try to clip stuff inside a skybox against the current portal.
	if (!renderportal->CurrentPortalInSkybox && renderportal->CurrentPortal && !!P_PointOnLineSidePrecise(thing->Pos(), renderportal->CurrentPortal->dst))
		return;

	// [RH] Interpolate the sprite's position to make it look smooth
	DVector3 pos = thing->InterpolatedPosition(r_TicFracF);
	pos.Z += thing->GetBobOffset(r_TicFracF);

	tex = NULL;
	voxel = NULL;

	int spritenum = thing->sprite;
	DVector2 spriteScale = thing->Scale;
	int renderflags = thing->renderflags;
	if (spriteScale.Y < 0)
	{
		spriteScale.Y = -spriteScale.Y;
		renderflags ^= RF_YFLIP;
	}
	if (thing->player != NULL)
	{
		P_CheckPlayerSprite(thing, spritenum, spriteScale);
	}

	if (thing->picnum.isValid())
	{
		picnum = thing->picnum;

		tex = TexMan(picnum);
		if (tex->UseType == FTexture::TEX_Null)
		{
			return;
		}

		if (tex->Rotations != 0xFFFF)
		{
			// choose a different rotation based on player view
			spriteframe_t *sprframe = &SpriteFrames[tex->Rotations];
			DAngle ang = (pos - ViewPos).Angle();
			angle_t rot;
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				if (thing->flags7 & MF7_SPRITEANGLE)
					rot = (thing->SpriteAngle + 45.0 / 2 * 9).BAMs() >> 28;
				else
					rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + 45.0 / 2 * 9).BAMs() >> 28;
			}
			else
			{
				if (thing->flags7 & MF7_SPRITEANGLE)
					rot = (thing->SpriteAngle + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
				else
					rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
			}
			picnum = sprframe->Texture[rot];
			if (sprframe->Flip & (1 << rot))
			{
				renderflags ^= RF_XFLIP;
			}
			tex = TexMan[picnum];	// Do not animate the rotation
		}
	}
	else
	{
		// decide which texture to use for the sprite
		if ((unsigned)spritenum >= sprites.Size ())
		{
			DPrintf (DMSG_ERROR, "R_ProjectSprite: invalid sprite number %u\n", spritenum);
			return;
		}
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
			DAngle ang = (pos - ViewPos).Angle();
			angle_t rot;
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				if (thing->flags7 & MF7_SPRITEANGLE)
					rot = (thing->SpriteAngle + 45.0 / 2 * 9).BAMs() >> 28;
				else
					rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + 45.0 / 2 * 9).BAMs() >> 28;
			}
			else
			{
				if (thing->flags7 & MF7_SPRITEANGLE)
					rot = (thing->SpriteAngle + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
				else
					rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
			}
			picnum = sprframe->Texture[rot];
			if (sprframe->Flip & (1 << rot))
			{
				renderflags ^= RF_XFLIP;
			}
			tex = TexMan[picnum];	// Do not animate the rotation
			if (r_drawvoxels)
			{
				voxel = sprframe->Voxel;
			}
		}
	}
	if (spriteScale.X < 0)
	{
		spriteScale.X = -spriteScale.X;
		renderflags ^= RF_XFLIP;
	}
	if (voxel == NULL && (tex == NULL || tex->UseType == FTexture::TEX_Null))
	{
		return;
	}

	if ((renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
	{
		R_ProjectWallSprite(thing, pos, picnum, spriteScale, renderflags);
		return;
	}

	// transform the origin point
	tr_x = pos.X - ViewPos.X;
	tr_y = pos.Y - ViewPos.Y;

	tz = tr_x * ViewTanCos + tr_y * ViewTanSin;

	// thing is behind view plane?
	if (voxel == NULL && tz < MINZ)
		return;

	tx = tr_x * ViewSin - tr_y * ViewCos;

	// [RH] Flip for mirrors
	if (renderportal->MirrorFlags & RF_XFLIP)
	{
		tx = -tx;
	}
	//tx2 = tx >> 4;

	// too far off the side?
	// if it's a voxel, it can be further off the side
	if ((voxel == NULL && (fabs(tx / 64) > fabs(tz))) ||
		(voxel != NULL && (fabs(tx / 128) > fabs(tz))))
	{
		return;
	}

	if (voxel == NULL)
	{
		// [RH] Added scaling
		int scaled_to = tex->GetScaledTopOffset();
		int scaled_bo = scaled_to - tex->GetScaledHeight();
		gzt = pos.Z + spriteScale.Y * scaled_to;
		gzb = pos.Z + spriteScale.Y * scaled_bo;
	}
	else
	{
		xscale = spriteScale.X * voxel->Scale;
		yscale = spriteScale.Y * voxel->Scale;
		double piv = voxel->Voxel->Mips[0].Pivot.Z;
		gzt = pos.Z + yscale * piv - thing->Floorclip;
		gzb = pos.Z + yscale * (piv - voxel->Voxel->Mips[0].SizeZ);
		if (gzt <= gzb)
			return;
	}

	// killough 3/27/98: exclude things totally separated
	// from the viewer, by either water or fake ceilings
	// killough 4/11/98: improve sprite clipping for underwater/fake ceilings

	heightsec = thing->Sector->GetHeightSec();

	if (heightsec != NULL)	// only clip things which are in special sectors
	{
		if (fakeside == WaterFakeSide::AboveCeiling)
		{
			if (gzt < heightsec->ceilingplane.ZatPoint(pos))
				return;
		}
		else if (fakeside == WaterFakeSide::BelowFloor)
		{
			if (gzb >= heightsec->floorplane.ZatPoint(pos))
				return;
		}
		else
		{
			if (gzt < heightsec->floorplane.ZatPoint(pos))
				return;
			if (!(heightsec->MoreFlags & SECF_FAKEFLOORONLY) && gzb >= heightsec->ceilingplane.ZatPoint(pos))
				return;
		}
	}

	if (voxel == NULL)
	{
		xscale = CenterX / tz;

		// [RH] Reject sprites that are off the top or bottom of the screen
		if (globaluclip * tz > ViewPos.Z - gzb || globaldclip * tz < ViewPos.Z - gzt)
		{
			return;
		}

		// [RH] Flip for mirrors
		renderflags ^= renderportal->MirrorFlags & RF_XFLIP;

		// calculate edges of the shape
		const double thingxscalemul = spriteScale.X / tex->Scale.X;

		tx -= ((renderflags & RF_XFLIP) ? (tex->GetWidth() - tex->LeftOffset - 1) : tex->LeftOffset) * thingxscalemul;
		double dtx1 = tx * xscale;
		x1 = centerx + xs_RoundToInt(dtx1);

		// off the right side?
		if (x1 >= renderportal->WindowRight)
			return;

		tx += tex->GetWidth() * thingxscalemul;
		x2 = centerx + xs_RoundToInt(tx * xscale);

		// off the left side or too small?
		if ((x2 < renderportal->WindowLeft || x2 <= x1))
			return;

		xscale = spriteScale.X * xscale / tex->Scale.X;
		iscale = (fixed_t)(FRACUNIT / xscale); // Round towards zero to avoid wrapping in edge cases

		double yscale = spriteScale.Y / tex->Scale.Y;

		// store information in a vissprite
		vis = R_NewVisSprite();

		vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		vis->xscale = FLOAT2FIXED(xscale);
		vis->yscale = float(InvZtoScale * yscale / tz);
		vis->idepth = float(1 / tz);
		vis->floorclip = thing->Floorclip / yscale;
		vis->texturemid = tex->TopOffset - (ViewPos.Z - pos.Z + thing->Floorclip) / yscale;
		vis->x1 = x1 < renderportal->WindowLeft ? renderportal->WindowLeft : x1;
		vis->x2 = x2 > renderportal->WindowRight ? renderportal->WindowRight : x2;
		vis->Angle = thing->Angles.Yaw;

		if (renderflags & RF_XFLIP)
		{
			vis->startfrac = (tex->GetWidth() << FRACBITS) - 1;
			vis->xiscale = -iscale;
		}
		else
		{
			vis->startfrac = 0;
			vis->xiscale = iscale;
		}

		vis->startfrac += (fixed_t)(vis->xiscale * (vis->x1 - centerx + 0.5 - dtx1));
	}
	else
	{
		vis = R_NewVisSprite();

		vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		vis->xscale = FLOAT2FIXED(xscale);
		vis->yscale = (float)yscale;
		vis->x1 = renderportal->WindowLeft;
		vis->x2 = renderportal->WindowRight;
		vis->idepth = 1 / MINZ;
		vis->floorclip = thing->Floorclip;

		pos.Z -= thing->Floorclip;

		vis->Angle = thing->Angles.Yaw + voxel->AngleOffset;

		int voxelspin = (thing->flags & MF_DROPPED) ? voxel->DroppedSpin : voxel->PlacedSpin;
		if (voxelspin != 0)
		{
			DAngle ang = double(I_FPSTime()) * voxelspin / 1000;
			vis->Angle -= ang;
		}

		vis->pa.vpos = { (float)ViewPos.X, (float)ViewPos.Y, (float)ViewPos.Z };
		vis->pa.vang = FAngle((float)ViewAngle.Degrees);
	}

	// killough 3/27/98: save sector for special clipping later
	vis->heightsec = heightsec;
	vis->sector = thing->Sector;

	vis->depth = (float)tz;
	vis->gpos = { (float)pos.X, (float)pos.Y, (float)pos.Z };
	vis->gzb = (float)gzb;		// [RH] use gzb, not thing->z
	vis->gzt = (float)gzt;		// killough 3/27/98
	vis->deltax = float(pos.X - ViewPos.X);
	vis->deltay = float(pos.Y - ViewPos.Y);
	vis->renderflags = renderflags;
	if(thing->flags5 & MF5_BRIGHT)
		vis->renderflags |= RF_FULLBRIGHT; // kg3D
	vis->Style.RenderStyle = thing->RenderStyle;
	vis->FillColor = thing->fillcolor;
	vis->Translation = thing->Translation;		// [RH] thing translation table
	vis->FakeFlatStat = fakeside;
	vis->Style.Alpha = float(thing->Alpha);
	vis->fakefloor = fakefloor;
	vis->fakeceiling = fakeceiling;
	vis->Style.ColormapNum = 0;
	vis->bInMirror = renderportal->MirrorFlags & RF_XFLIP;
	vis->bSplitSprite = false;

	if (voxel != NULL)
	{
		vis->voxel = voxel->Voxel;
		vis->bIsVoxel = true;
		vis->bWallSprite = false;
		DrewAVoxel = true;
	}
	else
	{
		vis->pic = tex;
		vis->bIsVoxel = false;
		vis->bWallSprite = false;
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
	if (current_sector->sectornum != thing->Sector->sectornum)	// compare sectornums to account for R_FakeFlat copies.
	{
		// Todo: The actor is from a different sector so we have to retrieve the proper basecolormap for that sector.
	}

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
		vis->Style.BaseColormap = fixedcolormap;
		vis->Style.ColormapNum = 0;
	}
	else
	{
		if (invertcolormap)
		{
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
		}
		if (fixedlightlev >= 0)
		{
			vis->Style.BaseColormap = mybasecolormap;
			vis->Style.ColormapNum = fixedlightlev >> COLORMAPSHIFT;
		}
		else if (!foggy && ((renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT)))
		{ // full bright
			vis->Style.BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : mybasecolormap;
			vis->Style.ColormapNum = 0;
		}
		else
		{ // diminished light
			vis->Style.ColormapNum = GETPALOOKUP(
				r_SpriteVisibility / MAX(tz, MINZ), spriteshade);
			vis->Style.BaseColormap = mybasecolormap;
		}
	}
}

//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
// [RH] Save which side of heightsec sprite is on here.
void R_AddSprites (sector_t *sec, int lightlevel, WaterFakeSide fakeside)
{
	F3DFloor *fakeceiling = NULL;
	F3DFloor *fakefloor = NULL;

	// BSP is traversed by subsector.
	// A sector might have been split into several
	//	subsectors during BSP building.
	// Thus we check whether it was already added.
	if (sec->touching_renderthings == nullptr || sec->validcount == validcount)
		return;

	// Well, now it will be done.
	sec->validcount = validcount;

	spriteshade = LIGHT2SHADE(lightlevel + r_actualextralight);

	// Handle all things in sector.
	for(auto p = sec->touching_renderthings; p != nullptr; p = p->m_snext)
	{
		auto thing = p->m_thing;
		if (thing->validcount == validcount) continue;
		thing->validcount = validcount;
		
		FIntCVar *cvar = thing->GetClass()->distancecheck;
		if (cvar != NULL && *cvar >= 0)
		{
			double dist = (thing->Pos() - ViewPos).LengthSquared();
			double check = (double)**cvar;
			if (dist >= check * check)
			{
				continue;
			}
		}

		// find fake level
		for(auto rover : thing->Sector->e->XFloor.ffloors) 
		{
			if(!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES)) continue;
			if(!(rover->flags & FF_SOLID) || rover->alpha != 255) continue;
			if(!fakefloor)
			{
				if(!rover->top.plane->isSlope())
				{
					if(rover->top.plane->ZatPoint(0., 0.) <= thing->Z()) fakefloor = rover;
				}
			}
			if(!rover->bottom.plane->isSlope())
			{
				if(rover->bottom.plane->ZatPoint(0., 0.) >= thing->Top()) fakeceiling = rover;
			}
		}	
		R_ProjectSprite (thing, fakeside, fakefloor, fakeceiling, sec);
		fakeceiling = NULL;
		fakefloor = NULL;
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
	return DVector2(a->deltax, a->deltay).LengthSquared() <
		   DVector2(b->deltax, b->deltay).LengthSquared();
}

#if 0
static drawseg_t **drawsegsorter;
static int drawsegsortersize = 0;

// Sort vissprites by leftmost column, left to right
static int sv_comparex (const void *arg1, const void *arg2)
{
	return (*(vissprite_t **)arg2)->x1 - (*(vissprite_t **)arg1)->x1;
}

// Sort drawsegs by rightmost column, left to right
static int sd_comparex (const void *arg1, const void *arg2)
{
	return (*(drawseg_t **)arg2)->x2 - (*(drawseg_t **)arg1)->x2;
}

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
	FSWColormap *colormap = spr->Style.BaseColormap;
	int colormapnum = spr->Style.ColormapNum;
	F3DFloor *rover;
	FDynamicColormap *mybasecolormap;

	Clip3DFloors *clip3d = Clip3DFloors::Instance();

	// [RH] Check for particles
	if (!spr->bIsVoxel && spr->pic == NULL)
	{
		// kg3D - reject invisible parts
		if ((clip3d->fake3D & FAKE3D_CLIPBOTTOM) && spr->gpos.Z <= clip3d->sclipBottom) return;
		if ((clip3d->fake3D & FAKE3D_CLIPTOP)    && spr->gpos.Z >= clip3d->sclipTop) return;
		R_DrawParticle (spr);
		return;
	}

	x1 = spr->x1;
	x2 = spr->x2;

	// [RH] Quickly reject sprites with bad x ranges.
	if (x1 >= x2)
		return;

	// [RH] Sprites split behind a one-sided line can also be discarded.
	if (spr->sector == NULL)
		return;

	// kg3D - reject invisible parts
	if ((clip3d->fake3D & FAKE3D_CLIPBOTTOM) && spr->gzt <= clip3d->sclipBottom) return;
	if ((clip3d->fake3D & FAKE3D_CLIPTOP)    && spr->gzb >= clip3d->sclipTop) return;

	// kg3D - correct colors now
	if (!fixedcolormap && fixedlightlev < 0 && spr->sector->e && spr->sector->e->XFloor.lightlist.Size()) 
	{
		if (!(clip3d->fake3D & FAKE3D_CLIPTOP))
		{
			clip3d->sclipTop = spr->sector->ceilingplane.ZatPoint(ViewPos);
		}
		sector_t *sec = NULL;
		for (i = spr->sector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
		{
			if (clip3d->sclipTop <= spr->sector->e->XFloor.lightlist[i].plane.Zat0())
			{
				rover = spr->sector->e->XFloor.lightlist[i].caster;
				if (rover) 
				{
					if (rover->flags & FF_DOUBLESHADOW && clip3d->sclipTop <= rover->bottom.plane->Zat0())
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
				spr->Style.BaseColormap = mybasecolormap;
				spr->Style.ColormapNum = fixedlightlev >> COLORMAPSHIFT;
			}
			else if (!foggy && (spr->renderflags & RF_FULLBRIGHT))
			{ // full bright
				spr->Style.BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : mybasecolormap;
				spr->Style.ColormapNum = 0;
			}
			else
			{ // diminished light
				spriteshade = LIGHT2SHADE(sec->lightlevel + r_actualextralight);
				spr->Style.BaseColormap = mybasecolormap;
				spr->Style.ColormapNum = GETPALOOKUP(r_SpriteVisibility / MAX(MINZ, (double)spr->depth), spriteshade);
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

	double scale = InvZtoScale * spr->idepth;
	double hzb = DBL_MIN, hzt = DBL_MAX;

	if (spr->bIsVoxel && spr->floorclip != 0)
	{
		hzb = spr->gzb;
	}

	if (spr->heightsec && !(spr->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{ // only things in specially marked sectors
		if (spr->FakeFlatStat != WaterFakeSide::AboveCeiling)
		{
			double hz = spr->heightsec->floorplane.ZatPoint(spr->gpos);
			int h = xs_RoundToInt(CenterY - (hz - ViewPos.Z) * scale);

			if (spr->FakeFlatStat == WaterFakeSide::BelowFloor)
			{ // seen below floor: clip top
				if (!spr->bIsVoxel && h > topclip)
				{
					topclip = short(MIN(h, viewheight));
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
		if (spr->FakeFlatStat != WaterFakeSide::BelowFloor && !(spr->heightsec->MoreFlags & SECF_FAKEFLOORONLY))
		{
			double hz = spr->heightsec->ceilingplane.ZatPoint(spr->gpos);
			int h = xs_RoundToInt(CenterY - (hz - ViewPos.Z) * scale);

			if (spr->FakeFlatStat == WaterFakeSide::AboveCeiling)
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
					topclip = MIN(h, viewheight);
				}
				hzt = MIN(hzt, hz);
			}
		}
	}
	// killough 3/27/98: end special clipping for deep water / fake ceilings
	else if (!spr->bIsVoxel && spr->floorclip)
	{ // [RH] Move floorclip stuff from R_DrawVisSprite to here
		//int clip = ((FLOAT2FIXED(CenterY) - FixedMul (spr->texturemid - (spr->pic->GetHeight() << FRACBITS) + spr->floorclip, spr->yscale)) >> FRACBITS);
		int clip = xs_RoundToInt(CenterY - (spr->texturemid - spr->pic->GetHeight() + spr->floorclip) * spr->yscale);
		if (clip < botclip)
		{
			botclip = MAX<short>(0, clip);
		}
	}

	if (clip3d->fake3D & FAKE3D_CLIPBOTTOM)
	{
		if (!spr->bIsVoxel)
		{
			double hz = clip3d->sclipBottom;
			if (spr->fakefloor)
			{
				double floorz = spr->fakefloor->top.plane->Zat0();
				if (ViewPos.Z > floorz && floorz == clip3d->sclipBottom )
				{
					hz = spr->fakefloor->bottom.plane->Zat0();
				}
			}
			int h = xs_RoundToInt(CenterY - (hz - ViewPos.Z) * scale);
			if (h < botclip)
			{
				botclip = MAX<short>(0, h);
			}
		}
		hzb = MAX(hzb, clip3d->sclipBottom);
	}
	if (clip3d->fake3D & FAKE3D_CLIPTOP)
	{
		if (!spr->bIsVoxel)
		{
			double hz = clip3d->sclipTop;
			if (spr->fakeceiling != NULL)
			{
				double ceilingZ = spr->fakeceiling->bottom.plane->Zat0();
				if (ViewPos.Z < ceilingZ && ceilingZ == clip3d->sclipTop)
				{
					hz = spr->fakeceiling->top.plane->Zat0();
				}
			}
			int h = xs_RoundToInt(CenterY - (hz - ViewPos.Z) * scale);
			if (h > topclip)
			{
				topclip = short(MIN(h, viewheight));
			}
		}
		hzt = MIN(hzt, clip3d->sclipTop);
	}

	if (topclip >= botclip)
	{
		spr->Style.BaseColormap = colormap;
		spr->Style.ColormapNum = colormapnum;
		return;
	}

	i = x2 - x1;
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
		// [ZZ] portal handling here
		//if (ds->CurrentPortalUniq != spr->CurrentPortalUniq)
		//	continue;
		// [ZZ] WARNING: uncommenting the two above lines, totally breaks sprite clipping

		// kg3D - no clipping on fake segs
		if (ds->fake) continue;
		// determine if the drawseg obscures the sprite
		if (ds->x1 >= x2 || ds->x2 <= x1 ||
			(!(ds->silhouette & SIL_BOTH) && ds->maskedtexturecol == -1 &&
			 !ds->bFogBoundary) )
		{
			// does not cover sprite
			continue;
		}

		r1 = MAX<int> (ds->x1, x1);
		r2 = MIN<int> (ds->x2, x2);

		float neardepth, fardepth;
		if (!spr->bWallSprite)
		{
			if (ds->sz1 < ds->sz2)
			{
				neardepth = ds->sz1, fardepth = ds->sz2;
			}
			else
			{
				neardepth = ds->sz2, fardepth = ds->sz1;
			}
		}
		
		
		// Check if sprite is in front of draw seg:
		if ((!spr->bWallSprite && neardepth > spr->depth) || ((spr->bWallSprite || fardepth > spr->depth) &&
			(spr->gpos.Y - ds->curline->v1->fY()) * (ds->curline->v2->fX() - ds->curline->v1->fX()) -
			(spr->gpos.X - ds->curline->v1->fX()) * (ds->curline->v2->fY() - ds->curline->v1->fY()) <= 0))
		{
			RenderPortal *renderportal = RenderPortal::Instance();
			
			// seg is behind sprite, so draw the mid texture if it has one
			if (ds->CurrentPortalUniq == renderportal->CurrentPortalUniq && // [ZZ] instead, portal uniq check is made here
				(ds->maskedtexturecol != -1 || ds->bFogBoundary))
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
			i = r2 - r1;
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
			i = r2 - r1;
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
		if (!spr->bWallSprite)
		{
			R_DrawVisSprite(spr, clipbot, cliptop);
		}
		else
		{
			R_DrawWallSprite(spr, clipbot, cliptop);
		}
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
				spr->Style.BaseColormap = colormap;
				spr->Style.ColormapNum = colormapnum;
				return;
			}
		}
		// Add everything outside the left and right edges to the clipping array
		// for R_DrawVisVoxel().
		if (x1 > 0)
		{
			fillshort(cliptop, x1, viewheight);
		}
		if (x2 < viewwidth - 1)
		{
			fillshort(cliptop + x2, viewwidth - x2, viewheight);
		}
		int minvoxely = spr->gzt <= hzt ? 0 : xs_RoundToInt((spr->gzt - hzt) / spr->yscale);
		int maxvoxely = spr->gzb > hzb ? INT_MAX : xs_RoundToInt((spr->gzt - hzb) / spr->yscale);
		R_DrawVisVoxel(spr, minvoxely, maxvoxely, cliptop, clipbot);
	}
	spr->Style.BaseColormap = colormap;
	spr->Style.ColormapNum = colormapnum;
}

// kg3D:
// R_DrawMasked contains sorting
// original renamed to R_DrawMaskedSingle

void R_DrawMaskedSingle (bool renew)
{
	drawseg_t *ds;
	int i;

	RenderPortal *renderportal = RenderPortal::Instance();

	for (i = vsprcount; i > 0; i--)
	{
		if (spritesorter[i-1]->CurrentPortalUniq != renderportal->CurrentPortalUniq)
			continue; // probably another time
		R_DrawSprite (spritesorter[i-1]);
	}

	// render any remaining masked mid textures

	// Modified by Lee Killough:
	// (pointer check was originally nonportable
	// and buggy, by going past LEFT end of array):

	//		for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

	if (renew)
	{
		Clip3DFloors::Instance()->fake3D |= FAKE3D_REFRESHCLIP;
	}
	for (ds = ds_p; ds-- > firstdrawseg; )	// new -- killough
	{
		// [ZZ] the same as above
		if (ds->CurrentPortalUniq != renderportal->CurrentPortalUniq)
			continue;
		// kg3D - no fake segs
		if (ds->fake) continue;
		if (ds->maskedtexturecol != -1 || ds->bFogBoundary)
		{
			R_RenderMaskedSegRange (ds, ds->x1, ds->x2);
		}
	}
}

void R_DrawHeightPlanes(double height); // kg3D - fake planes

void R_DrawMasked (void)
{
	R_CollectPortals();
	R_SortVisSprites (DrewAVoxel ? sv_compare2d : sv_compare, firstvissprite - vissprites);

	Clip3DFloors *clip3d = Clip3DFloors::Instance();
	if (clip3d->height_top == NULL)
	{ // kg3D - no visible 3D floors, normal rendering
		R_DrawMaskedSingle(false);
	}
	else
	{ // kg3D - correct sorting
		HeightLevel *hl;

		// ceilings
		for (hl = clip3d->height_cur; hl != NULL && hl->height >= ViewPos.Z; hl = hl->prev)
		{
			if (hl->next)
			{
				clip3d->fake3D = FAKE3D_CLIPBOTTOM | FAKE3D_CLIPTOP;
				clip3d->sclipTop = hl->next->height;
			}
			else
			{
				clip3d->fake3D = FAKE3D_CLIPBOTTOM;
			}
			clip3d->sclipBottom = hl->height;
			R_DrawMaskedSingle(true);
			R_DrawHeightPlanes(hl->height);
		}

		// floors
		clip3d->fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPTOP;
		clip3d->sclipTop = clip3d->height_top->height;
		R_DrawMaskedSingle(true);
		hl = clip3d->height_top;
		for (hl = clip3d->height_top; hl != NULL && hl->height < ViewPos.Z; hl = hl->next)
		{
			R_DrawHeightPlanes(hl->height);
			if (hl->next)
			{
				clip3d->fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPTOP | FAKE3D_CLIPBOTTOM;
				clip3d->sclipTop = hl->next->height;
			}
			else
			{
				clip3d->fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPBOTTOM;
			}
			clip3d->sclipBottom = hl->height;
			R_DrawMaskedSingle(true);
		}
		clip3d->DeleteHeights();
		clip3d->fake3D = 0;
	}
	R_DrawPlayerSprites();
}

extern double BaseYaspectMul;;

inline int sgn(int v)
{
	return v < 0 ? -1 : v > 0 ? 1 : 0;
}

}
