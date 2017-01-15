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

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "p_lnspec.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "g_levellocals.h"
#include "p_maputl.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/things/r_visiblespritelist.h"
#include "swrenderer/things/r_voxel.h"
#include "swrenderer/things/r_particle.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/things/r_wallsprite.h"
#include "swrenderer/things/r_playersprite.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_translucent_pass.h"
#include "swrenderer/scene/r_viewport.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/r_memory.h"

EXTERN_CVAR(Int, r_drawfuzz)
EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Bool, r_blendmethod)

CVAR(Bool, r_fullbrightignoresectorcolor, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

namespace swrenderer
{
	bool RenderTranslucentPass::DrewAVoxel;
	TArray<drawseg_t *> RenderTranslucentPass::portaldrawsegs;

	void RenderTranslucentPass::Deinit()
	{
		VisibleSpriteList::Deinit();
		SortedVisibleSpriteList::Deinit();
		RenderVoxel::Deinit();
	}

	void RenderTranslucentPass::Clear()
	{
		VisibleSpriteList::Clear();
		DrewAVoxel = false;
	}

	void RenderTranslucentPass::CollectPortals()
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

	bool RenderTranslucentPass::ClipSpriteColumnWithPortals(int x, vissprite_t* spr)
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
			if ((x >= seg->x1) && (x < seg->x2))
				return true;
		}

		return false;
	}

	void RenderTranslucentPass::DrawSprite(vissprite_t *spr)
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
		if (!spr->bIsVoxel && spr->pic == nullptr)
		{
			// kg3D - reject invisible parts
			if ((clip3d->fake3D & FAKE3D_CLIPBOTTOM) && spr->gpos.Z <= clip3d->sclipBottom) return;
			if ((clip3d->fake3D & FAKE3D_CLIPTOP) && spr->gpos.Z >= clip3d->sclipTop) return;
			RenderParticle::Render(spr);
			return;
		}

		x1 = spr->x1;
		x2 = spr->x2;

		// [RH] Quickly reject sprites with bad x ranges.
		if (x1 >= x2)
			return;

		// [RH] Sprites split behind a one-sided line can also be discarded.
		if (spr->sector == nullptr)
			return;

		// kg3D - reject invisible parts
		if ((clip3d->fake3D & FAKE3D_CLIPBOTTOM) && spr->gzt <= clip3d->sclipBottom) return;
		if ((clip3d->fake3D & FAKE3D_CLIPTOP) && spr->gzb >= clip3d->sclipTop) return;

		// kg3D - correct colors now
		if (!fixedcolormap && fixedlightlev < 0 && spr->sector->e && spr->sector->e->XFloor.lightlist.Size())
		{
			if (!(clip3d->fake3D & FAKE3D_CLIPTOP))
			{
				clip3d->sclipTop = spr->sector->ceilingplane.ZatPoint(ViewPos);
			}
			sector_t *sec = nullptr;
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
						mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(255, 255, 255), mybasecolormap->Desaturate);
						invertcolormap = false;
					}
					else
					{ // Fade to black
						mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(0, 0, 0), mybasecolormap->Desaturate);
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
				else if (!spr->foggy && (spr->renderflags & RF_FULLBRIGHT))
				{ // full bright
					spr->Style.BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : mybasecolormap;
					spr->Style.ColormapNum = 0;
				}
				else
				{ // diminished light
					int spriteshade = LIGHT2SHADE(sec->lightlevel + R_ActualExtraLight(spr->foggy));
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
						botclip = MAX<short>(0, h);
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
						botclip = MAX<short>(0, h);
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
					if (ViewPos.Z > floorz && floorz == clip3d->sclipBottom)
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
				if (spr->fakeceiling != nullptr)
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
				(!(ds->silhouette & SIL_BOTH) && ds->maskedtexturecol == nullptr &&
					!ds->bFogBoundary))
			{
				// does not cover sprite
				continue;
			}

			r1 = MAX<int>(ds->x1, x1);
			r2 = MIN<int>(ds->x2, x2);

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
					(ds->maskedtexturecol != nullptr || ds->bFogBoundary))
					R_RenderMaskedSegRange(ds, r1, r2);

				continue;
			}

			// clip this piece of the sprite
			// killough 3/27/98: optimized and made much shorter
			// [RH] Optimized further (at least for VC++;
			// other compilers should be at least as good as before)

			if (ds->silhouette & SIL_BOTTOM) //bottom sil
			{
				clip1 = clipbot + r1;
				clip2 = ds->sprbottomclip + r1 - ds->x1;
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
				clip2 = ds->sprtopclip + r1 - ds->x1;
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
				RenderSprite::Render(spr, clipbot, cliptop);
			}
			else
			{
				RenderWallSprite::Render(spr, clipbot, cliptop);
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
			RenderVoxel::Render(spr, minvoxely, maxvoxely, cliptop, clipbot);
		}
		spr->Style.BaseColormap = colormap;
		spr->Style.ColormapNum = colormapnum;
	}

	void RenderTranslucentPass::DrawMaskedSingle(bool renew)
	{
		RenderPortal *renderportal = RenderPortal::Instance();

		for (int i = SortedVisibleSpriteList::vsprcount; i > 0; i--)
		{
			if (SortedVisibleSpriteList::spritesorter[i - 1]->CurrentPortalUniq != renderportal->CurrentPortalUniq)
				continue; // probably another time
			DrawSprite(SortedVisibleSpriteList::spritesorter[i - 1]);
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
		for (drawseg_t *ds = ds_p; ds-- > firstdrawseg; )	// new -- killough
		{
			// [ZZ] the same as above
			if (ds->CurrentPortalUniq != renderportal->CurrentPortalUniq)
				continue;
			// kg3D - no fake segs
			if (ds->fake) continue;
			if (ds->maskedtexturecol != nullptr || ds->bFogBoundary)
			{
				R_RenderMaskedSegRange(ds, ds->x1, ds->x2);
			}
		}
	}

	void RenderTranslucentPass::Render()
	{
		CollectPortals();
		SortedVisibleSpriteList::Sort(DrewAVoxel ? SortedVisibleSpriteList::sv_compare2d : SortedVisibleSpriteList::sv_compare, VisibleSpriteList::firstvissprite - VisibleSpriteList::vissprites);

		Clip3DFloors *clip3d = Clip3DFloors::Instance();
		if (clip3d->height_top == nullptr)
		{ // kg3D - no visible 3D floors, normal rendering
			DrawMaskedSingle(false);
		}
		else
		{ // kg3D - correct sorting
			// ceilings
			for (HeightLevel *hl = clip3d->height_cur; hl != nullptr && hl->height >= ViewPos.Z; hl = hl->prev)
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
				DrawMaskedSingle(true);
				VisiblePlaneList::Instance()->RenderHeight(hl->height);
			}

			// floors
			clip3d->fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPTOP;
			clip3d->sclipTop = clip3d->height_top->height;
			DrawMaskedSingle(true);
			for (HeightLevel *hl = clip3d->height_top; hl != nullptr && hl->height < ViewPos.Z; hl = hl->next)
			{
				VisiblePlaneList::Instance()->RenderHeight(hl->height);
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
				DrawMaskedSingle(true);
			}
			clip3d->DeleteHeights();
			clip3d->fake3D = 0;
		}
		RenderPlayerSprite::RenderPlayerSprites();
	}
}
