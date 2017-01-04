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

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"
#include "i_system.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "d_net.h"
#include "swrenderer/r_main.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/scene/r_things.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_bsp.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/line/r_fogboundary.h"
#include "swrenderer/segments/r_drawsegment.h"

namespace swrenderer
{
	drawseg_t *firstdrawseg;
	drawseg_t *ds_p;
	drawseg_t *drawsegs;

	size_t FirstInterestingDrawseg;
	TArray<size_t> InterestingDrawsegs;

	namespace
	{
		size_t MaxDrawSegs;

		sector_t *frontsector;
		sector_t *backsector;

		seg_t *curline;

		FWallCoords WallC;
		FWallTmapVals WallT;

		float rw_light;
		float rw_lightstep;
		fixed_t rw_offset;
		FTexture *rw_pic;
	}

	void R_FreeDrawSegs()
	{
		if (drawsegs != nullptr)
		{
			M_Free(drawsegs);
			drawsegs = nullptr;
		}
	}

	void R_ClearDrawSegs()
	{
		if (drawsegs == nullptr)
		{
			MaxDrawSegs = 256; // [RH] Default. Increased as needed.
			firstdrawseg = drawsegs = (drawseg_t *)M_Malloc (MaxDrawSegs * sizeof(drawseg_t));
		}
		FirstInterestingDrawseg = 0;
		InterestingDrawsegs.Clear ();
		ds_p = drawsegs;
	}

	drawseg_t *R_AddDrawSegment()
	{
		if (ds_p == &drawsegs[MaxDrawSegs])
		{ // [RH] Grab some more drawsegs
			size_t newdrawsegs = MaxDrawSegs ? MaxDrawSegs * 2 : 32;
			ptrdiff_t firstofs = firstdrawseg - drawsegs;
			drawsegs = (drawseg_t *)M_Realloc(drawsegs, newdrawsegs * sizeof(drawseg_t));
			firstdrawseg = drawsegs + firstofs;
			ds_p = drawsegs + MaxDrawSegs;
			MaxDrawSegs = newdrawsegs;
			DPrintf(DMSG_NOTIFY, "MaxDrawSegs increased to %zu\n", MaxDrawSegs);
		}

		return ds_p++;
	}

	// Clip a midtexture to the floor and ceiling of the sector in front of it.
	void ClipMidtex(int x1, int x2)
	{
		short most[MAXWIDTH];

		R_CreateWallSegmentYSloped(most, curline->frontsector->ceilingplane, &WallC, curline, MirrorFlags & RF_XFLIP);
		for (int i = x1; i < x2; ++i)
		{
			if (wallupper[i] < most[i])
				wallupper[i] = most[i];
		}
		R_CreateWallSegmentYSloped(most, curline->frontsector->floorplane, &WallC, curline, MirrorFlags & RF_XFLIP);
		for (int i = x1; i < x2; ++i)
		{
			if (walllower[i] > most[i])
				walllower[i] = most[i];
		}
	}

	void R_RenderMaskedSegRange(drawseg_t *ds, int x1, int x2)
	{
		float *MaskedSWall = nullptr, MaskedScaleY = 0, rw_scalestep = 0;
		fixed_t *maskedtexturecol = nullptr;
	
		FTexture	*tex;
		int			i;
		sector_t	tempsec;		// killough 4/13/98
		double		texheight, texheightscale;
		bool		notrelevant = false;
		double		rowoffset;
		bool		wrap = false;

		const sector_t *sec;

		sprflipvert = false;

		curline = ds->curline;

		bool visible = R_SetPatchStyle(LegacyRenderStyles[curline->linedef->flags & ML_ADDTRANS ? STYLE_Add : STYLE_Translucent],
			(float)MIN(curline->linedef->alpha, 1.), 0, 0);

		if (!visible && !ds->bFogBoundary && !ds->bFakeBoundary)
		{
			return;
		}

		NetUpdate();

		frontsector = curline->frontsector;
		backsector = curline->backsector;

		tex = TexMan(curline->sidedef->GetTexture(side_t::mid), true);
		if (i_compatflags & COMPATF_MASKEDMIDTEX)
		{
			tex = tex->GetRawTexture();
		}

		// killough 4/13/98: get correct lightlevel for 2s normal textures
		sec = RenderBSP::Instance()->FakeFlat(frontsector, &tempsec, nullptr, nullptr, nullptr, 0, 0, 0, 0);

		basecolormap = sec->ColorMap;	// [RH] Set basecolormap

		int wallshade = ds->shade;
		rw_lightstep = ds->lightstep;
		rw_light = ds->light + (x1 - ds->x1) * rw_lightstep;

		Clip3DFloors *clip3d = Clip3DFloors::Instance();

		if (fixedlightlev < 0)
		{
			if (!(clip3d->fake3D & FAKE3D_CLIPTOP))
			{
				clip3d->sclipTop = sec->ceilingplane.ZatPoint(ViewPos);
			}
			for (i = frontsector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
			{
				if (clip3d->sclipTop <= frontsector->e->XFloor.lightlist[i].plane.Zat0())
				{
					lightlist_t *lit = &frontsector->e->XFloor.lightlist[i];
					basecolormap = lit->extra_colormap;
					wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + r_actualextralight);
					break;
				}
			}
		}

		mfloorclip = openings + ds->sprbottomclip - ds->x1;
		mceilingclip = openings + ds->sprtopclip - ds->x1;


		// [RH] Draw fog partition
		if (ds->bFogBoundary)
		{
			R_DrawFogBoundary(x1, x2, mceilingclip, mfloorclip, wallshade, rw_light, rw_lightstep);
			if (ds->maskedtexturecol == -1)
			{
				goto clearfog;
			}
		}
		if ((ds->bFakeBoundary && !(ds->bFakeBoundary & 4)) || !visible)
		{
			goto clearfog;
		}

		MaskedSWall = (float *)(openings + ds->swall) - ds->x1;
		MaskedScaleY = ds->yscale;
		maskedtexturecol = (fixed_t *)(openings + ds->maskedtexturecol) - ds->x1;
		spryscale = ds->iscale + ds->iscalestep * (x1 - ds->x1);
		rw_scalestep = ds->iscalestep;

		if (fixedlightlev >= 0)
			R_SetColorMapLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
		else if (fixedcolormap != nullptr)
			R_SetColorMapLight(fixedcolormap, 0, 0);

		// find positioning
		texheight = tex->GetScaledHeightDouble();
		texheightscale = fabs(curline->sidedef->GetTextureYScale(side_t::mid));
		if (texheightscale != 1)
		{
			texheight = texheight / texheightscale;
		}
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			dc_texturemid = MAX(frontsector->GetPlaneTexZ(sector_t::floor), backsector->GetPlaneTexZ(sector_t::floor)) + texheight;
		}
		else
		{
			dc_texturemid = MIN(frontsector->GetPlaneTexZ(sector_t::ceiling), backsector->GetPlaneTexZ(sector_t::ceiling));
		}

		rowoffset = curline->sidedef->GetTextureYOffset(side_t::mid);

		wrap = (curline->linedef->flags & ML_WRAP_MIDTEX) || (curline->sidedef->Flags & WALLF_WRAP_MIDTEX);
		if (!wrap)
		{ // Texture does not wrap vertically.
			double textop;

			if (MaskedScaleY < 0)
			{
				MaskedScaleY = -MaskedScaleY;
				sprflipvert = true;
			}
			if (tex->bWorldPanning)
			{
				// rowoffset is added before the multiply so that the masked texture will
				// still be positioned in world units rather than texels.
				dc_texturemid += rowoffset - ViewPos.Z;
				textop = dc_texturemid;
				dc_texturemid *= MaskedScaleY;
			}
			else
			{
				// rowoffset is added outside the multiply so that it positions the texture
				// by texels instead of world units.
				textop = dc_texturemid + rowoffset / MaskedScaleY - ViewPos.Z;
				dc_texturemid = (dc_texturemid - ViewPos.Z) * MaskedScaleY + rowoffset;
			}
			if (sprflipvert)
			{
				MaskedScaleY = -MaskedScaleY;
				dc_texturemid -= tex->GetHeight() << FRACBITS;
			}

			// [RH] Don't bother drawing segs that are completely offscreen
			if (globaldclip * ds->sz1 < -textop && globaldclip * ds->sz2 < -textop)
			{ // Texture top is below the bottom of the screen
				goto clearfog;
			}

			if (globaluclip * ds->sz1 > texheight - textop && globaluclip * ds->sz2 > texheight - textop)
			{ // Texture bottom is above the top of the screen
				goto clearfog;
			}

			if ((clip3d->fake3D & FAKE3D_CLIPBOTTOM) && textop < clip3d->sclipBottom - ViewPos.Z)
			{
				notrelevant = true;
				goto clearfog;
			}
			if ((clip3d->fake3D & FAKE3D_CLIPTOP) && textop - texheight > clip3d->sclipTop - ViewPos.Z)
			{
				notrelevant = true;
				goto clearfog;
			}

			WallC.sz1 = ds->sz1;
			WallC.sz2 = ds->sz2;
			WallC.sx1 = ds->sx1;
			WallC.sx2 = ds->sx2;

			if (clip3d->fake3D & FAKE3D_CLIPTOP)
			{
				R_CreateWallSegmentY(wallupper, textop < clip3d->sclipTop - ViewPos.Z ? textop : clip3d->sclipTop - ViewPos.Z, &WallC);
			}
			else
			{
				R_CreateWallSegmentY(wallupper, textop, &WallC);
			}
			if (clip3d->fake3D & FAKE3D_CLIPBOTTOM)
			{
				R_CreateWallSegmentY(walllower, textop - texheight > clip3d->sclipBottom - ViewPos.Z ? textop - texheight : clip3d->sclipBottom - ViewPos.Z, &WallC);
			}
			else
			{
				R_CreateWallSegmentY(walllower, textop - texheight, &WallC);
			}

			for (i = x1; i < x2; i++)
			{
				if (wallupper[i] < mceilingclip[i])
					wallupper[i] = mceilingclip[i];
			}
			for (i = x1; i < x2; i++)
			{
				if (walllower[i] > mfloorclip[i])
					walllower[i] = mfloorclip[i];
			}

			if (clip3d->CurrentSkybox)
			{ // Midtex clipping doesn't work properly with skyboxes, since you're normally below the floor
			  // or above the ceiling, so the appropriate end won't be clipped automatically when adding
			  // this drawseg.
				if ((curline->linedef->flags & ML_CLIP_MIDTEX) ||
					(curline->sidedef->Flags & WALLF_CLIP_MIDTEX))
				{
					ClipMidtex(x1, x2);
				}
			}

			mfloorclip = walllower;
			mceilingclip = wallupper;

			// draw the columns one at a time
			if (visible)
			{
				using namespace drawerargs;
				for (dc_x = x1; dc_x < x2; ++dc_x)
				{
					if (fixedcolormap == nullptr && fixedlightlev < 0)
					{
						R_SetColorMapLight(basecolormap, rw_light, wallshade);
					}

					dc_iscale = xs_Fix<16>::ToFix(MaskedSWall[dc_x] * MaskedScaleY);
					if (sprflipvert)
						sprtopscreen = CenterY + dc_texturemid * spryscale;
					else
						sprtopscreen = CenterY - dc_texturemid * spryscale;

					R_DrawMaskedColumn(tex, maskedtexturecol[dc_x]);

					rw_light += rw_lightstep;
					spryscale += rw_scalestep;
				}
			}
		}
		else
		{ // Texture does wrap vertically.
			if (tex->bWorldPanning)
			{
				// rowoffset is added before the multiply so that the masked texture will
				// still be positioned in world units rather than texels.
				dc_texturemid = (dc_texturemid - ViewPos.Z + rowoffset) * MaskedScaleY;
			}
			else
			{
				// rowoffset is added outside the multiply so that it positions the texture
				// by texels instead of world units.
				dc_texturemid = (dc_texturemid - ViewPos.Z) * MaskedScaleY + rowoffset;
			}

			WallC.sz1 = ds->sz1;
			WallC.sz2 = ds->sz2;
			WallC.sx1 = ds->sx1;
			WallC.sx2 = ds->sx2;

			if (clip3d->CurrentSkybox)
			{ // Midtex clipping doesn't work properly with skyboxes, since you're normally below the floor
			  // or above the ceiling, so the appropriate end won't be clipped automatically when adding
			  // this drawseg.
				if ((curline->linedef->flags & ML_CLIP_MIDTEX) ||
					(curline->sidedef->Flags & WALLF_CLIP_MIDTEX))
				{
					ClipMidtex(x1, x2);
				}
			}

			if (clip3d->fake3D & FAKE3D_CLIPTOP)
			{
				R_CreateWallSegmentY(wallupper, clip3d->sclipTop - ViewPos.Z, &WallC);
				for (i = x1; i < x2; i++)
				{
					if (wallupper[i] < mceilingclip[i])
						wallupper[i] = mceilingclip[i];
				}
				mceilingclip = wallupper;
			}
			if (clip3d->fake3D & FAKE3D_CLIPBOTTOM)
			{
				R_CreateWallSegmentY(walllower, clip3d->sclipBottom - ViewPos.Z, &WallC);
				for (i = x1; i < x2; i++)
				{
					if (walllower[i] > mfloorclip[i])
						walllower[i] = mfloorclip[i];
				}
				mfloorclip = walllower;
			}

			rw_offset = 0;
			rw_pic = tex;
			R_DrawDrawSeg(frontsector, curline, WallC, rw_pic, ds, x1, x2, mceilingclip, mfloorclip, MaskedSWall, maskedtexturecol, ds->yscale, wallshade, rw_offset, rw_light, rw_lightstep);
		}

	clearfog:
		R_FinishSetPatchStyle();
		if (ds->bFakeBoundary & 3)
		{
			R_RenderFakeWallRange(ds, x1, x2, wallshade);
		}
		if (!notrelevant)
		{
			if (clip3d->fake3D & FAKE3D_REFRESHCLIP)
			{
				if (!wrap)
				{
					assert(ds->bkup >= 0);
					memcpy(openings + ds->sprtopclip, openings + ds->bkup, (ds->x2 - ds->x1) * 2);
				}
			}
			else
			{
				fillshort(openings + ds->sprtopclip - ds->x1 + x1, x2 - x1, viewheight);
			}
		}
		return;
	}

	// kg3D - render one fake wall
	void R_RenderFakeWall(drawseg_t *ds, int x1, int x2, F3DFloor *rover, int wallshade)
	{
		int i;
		double xscale;
		double yscale;

		fixed_t Alpha = Scale(rover->alpha, OPAQUE, 255);
		bool visible = R_SetPatchStyle(LegacyRenderStyles[rover->flags & FF_ADDITIVETRANS ? STYLE_Add : STYLE_Translucent],
			Alpha, 0, 0);

		if (!visible) {
			R_FinishSetPatchStyle();
			return;
		}

		rw_lightstep = ds->lightstep;
		rw_light = ds->light + (x1 - ds->x1) * rw_lightstep;

		mfloorclip = openings + ds->sprbottomclip - ds->x1;
		mceilingclip = openings + ds->sprtopclip - ds->x1;

		spryscale = ds->iscale + ds->iscalestep * (x1 - ds->x1);
		float *MaskedSWall = (float *)(openings + ds->swall) - ds->x1;

		// find positioning
		side_t *scaledside;
		side_t::ETexpart scaledpart;
		if (rover->flags & FF_UPPERTEXTURE)
		{
			scaledside = curline->sidedef;
			scaledpart = side_t::top;
		}
		else if (rover->flags & FF_LOWERTEXTURE)
		{
			scaledside = curline->sidedef;
			scaledpart = side_t::bottom;
		}
		else
		{
			scaledside = rover->master->sidedef[0];
			scaledpart = side_t::mid;
		}
		xscale = rw_pic->Scale.X * scaledside->GetTextureXScale(scaledpart);
		yscale = rw_pic->Scale.Y * scaledside->GetTextureYScale(scaledpart);

		double rowoffset = curline->sidedef->GetTextureYOffset(side_t::mid) + rover->master->sidedef[0]->GetTextureYOffset(side_t::mid);
		double planez = rover->model->GetPlaneTexZ(sector_t::ceiling);
		rw_offset = FLOAT2FIXED(curline->sidedef->GetTextureXOffset(side_t::mid) + rover->master->sidedef[0]->GetTextureXOffset(side_t::mid));
		if (rowoffset < 0)
		{
			rowoffset += rw_pic->GetHeight();
		}
		dc_texturemid = (planez - ViewPos.Z) * yscale;
		if (rw_pic->bWorldPanning)
		{
			// rowoffset is added before the multiply so that the masked texture will
			// still be positioned in world units rather than texels.

			dc_texturemid = dc_texturemid + rowoffset * yscale;
			rw_offset = xs_RoundToInt(rw_offset * xscale);
		}
		else
		{
			// rowoffset is added outside the multiply so that it positions the texture
			// by texels instead of world units.
			dc_texturemid += rowoffset;
		}

		if (fixedlightlev >= 0)
			R_SetColorMapLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
		else if (fixedcolormap != nullptr)
			R_SetColorMapLight(fixedcolormap, 0, 0);

		WallC.sz1 = ds->sz1;
		WallC.sz2 = ds->sz2;
		WallC.sx1 = ds->sx1;
		WallC.sx2 = ds->sx2;
		WallC.tleft.X = ds->cx;
		WallC.tleft.Y = ds->cy;
		WallC.tright.X = ds->cx + ds->cdx;
		WallC.tright.Y = ds->cy + ds->cdy;
		WallT = ds->tmapvals;

		Clip3DFloors *clip3d = Clip3DFloors::Instance();
		R_CreateWallSegmentY(wallupper, clip3d->sclipTop - ViewPos.Z, &WallC);
		R_CreateWallSegmentY(walllower, clip3d->sclipBottom - ViewPos.Z, &WallC);

		for (i = x1; i < x2; i++)
		{
			if (wallupper[i] < mceilingclip[i])
				wallupper[i] = mceilingclip[i];
		}
		for (i = x1; i < x2; i++)
		{
			if (walllower[i] > mfloorclip[i])
				walllower[i] = mfloorclip[i];
		}

		PrepLWall(lwall, curline->sidedef->TexelLength*xscale, ds->sx1, ds->sx2, WallT);
		R_DrawDrawSeg(frontsector, curline, WallC, rw_pic, ds, x1, x2, wallupper, walllower, MaskedSWall, lwall, yscale, wallshade, rw_offset, rw_light, rw_lightstep);
		R_FinishSetPatchStyle();
	}

	// kg3D - walls of fake floors
	void R_RenderFakeWallRange(drawseg_t *ds, int x1, int x2, int wallshade)
	{
		FTexture *const DONT_DRAW = ((FTexture*)(intptr_t)-1);
		int i, j;
		F3DFloor *rover, *fover = nullptr;
		int passed, last;
		double floorHeight;
		double ceilingHeight;

		sprflipvert = false;
		curline = ds->curline;

		frontsector = curline->frontsector;
		backsector = curline->backsector;

		if (backsector == nullptr)
		{
			return;
		}
		if ((ds->bFakeBoundary & 3) == 2)
		{
			sector_t *sec = backsector;
			backsector = frontsector;
			frontsector = sec;
		}

		floorHeight = backsector->CenterFloor();
		ceilingHeight = backsector->CenterCeiling();

		Clip3DFloors *clip3d = Clip3DFloors::Instance();

		// maybe fix clipheights
		if (!(clip3d->fake3D & FAKE3D_CLIPBOTTOM)) clip3d->sclipBottom = floorHeight;
		if (!(clip3d->fake3D & FAKE3D_CLIPTOP)) clip3d->sclipTop = ceilingHeight;

		// maybe not visible
		if (clip3d->sclipBottom >= frontsector->CenterCeiling()) return;
		if (clip3d->sclipTop <= frontsector->CenterFloor()) return;

		if (clip3d->fake3D & FAKE3D_DOWN2UP)
		{ // bottom to viewz
			last = 0;
			for (i = backsector->e->XFloor.ffloors.Size() - 1; i >= 0; i--)
			{
				rover = backsector->e->XFloor.ffloors[i];
				if (!(rover->flags & FF_EXISTS)) continue;

				// visible?
				passed = 0;
				if (!(rover->flags & FF_RENDERSIDES) || rover->top.plane->isSlope() || rover->bottom.plane->isSlope() ||
					rover->top.plane->Zat0() <= clip3d->sclipBottom ||
					rover->bottom.plane->Zat0() >= ceilingHeight ||
					rover->top.plane->Zat0() <= floorHeight)
				{
					if (!i)
					{
						passed = 1;
					}
					else
					{
						continue;
					}
				}

				rw_pic = nullptr;
				if (rover->bottom.plane->Zat0() >= clip3d->sclipTop || passed)
				{
					if (last)
					{
						break;
					}
					// maybe wall from inside rendering?
					fover = nullptr;
					for (j = frontsector->e->XFloor.ffloors.Size() - 1; j >= 0; j--)
					{
						fover = frontsector->e->XFloor.ffloors[j];
						if (fover->model == rover->model)
						{ // never
							fover = nullptr;
							break;
						}
						if (!(fover->flags & FF_EXISTS)) continue;
						if (!(fover->flags & FF_RENDERSIDES)) continue;
						// no sloped walls, it's bugged
						if (fover->top.plane->isSlope() || fover->bottom.plane->isSlope()) continue;

						// visible?
						if (fover->top.plane->Zat0() <= clip3d->sclipBottom) continue; // no
						if (fover->bottom.plane->Zat0() >= clip3d->sclipTop)
						{ // no, last possible
							fover = nullptr;
							break;
						}
						// it is, render inside?
						if (!(fover->flags & (FF_BOTHPLANES | FF_INVERTPLANES)))
						{ // no
							fover = nullptr;
						}
						break;
					}
					// nothing
					if (!fover || j == -1)
					{
						break;
					}
					// correct texture
					if (fover->flags & rover->flags & FF_SWIMMABLE)
					{	// don't ever draw (but treat as something has been found)
						rw_pic = DONT_DRAW;
					}
					else if (fover->flags & FF_UPPERTEXTURE)
					{
						rw_pic = TexMan(curline->sidedef->GetTexture(side_t::top), true);
					}
					else if (fover->flags & FF_LOWERTEXTURE)
					{
						rw_pic = TexMan(curline->sidedef->GetTexture(side_t::bottom), true);
					}
					else
					{
						rw_pic = TexMan(fover->master->sidedef[0]->GetTexture(side_t::mid), true);
					}
				}
				else if (frontsector->e->XFloor.ffloors.Size())
				{
					// maybe not visible?
					fover = nullptr;
					for (j = frontsector->e->XFloor.ffloors.Size() - 1; j >= 0; j--)
					{
						fover = frontsector->e->XFloor.ffloors[j];
						if (fover->model == rover->model) // never
						{
							break;
						}
						if (!(fover->flags & FF_EXISTS)) continue;
						if (!(fover->flags & FF_RENDERSIDES)) continue;
						// no sloped walls, it's bugged
						if (fover->top.plane->isSlope() || fover->bottom.plane->isSlope()) continue;

						// visible?
						if (fover->top.plane->Zat0() <= clip3d->sclipBottom) continue; // no
						if (fover->bottom.plane->Zat0() >= clip3d->sclipTop)
						{ // visible, last possible
							fover = nullptr;
							break;
						}
						if ((fover->flags & FF_SOLID) == (rover->flags & FF_SOLID) &&
							!(!(fover->flags & FF_SOLID) && (fover->alpha == 255 || rover->alpha == 255))
							)
						{
							break;
						}
						if (fover->flags & rover->flags & FF_SWIMMABLE)
						{ // don't ever draw (but treat as something has been found)
							rw_pic = DONT_DRAW;
						}
						fover = nullptr; // visible
						break;
					}
					if (fover && j != -1)
					{
						fover = nullptr;
						last = 1;
						continue; // not visible
					}
				}
				if (!rw_pic)
				{
					fover = nullptr;
					if (rover->flags & FF_UPPERTEXTURE)
					{
						rw_pic = TexMan(curline->sidedef->GetTexture(side_t::top), true);
					}
					else if (rover->flags & FF_LOWERTEXTURE)
					{
						rw_pic = TexMan(curline->sidedef->GetTexture(side_t::bottom), true);
					}
					else
					{
						rw_pic = TexMan(rover->master->sidedef[0]->GetTexture(side_t::mid), true);
					}
				}
				// correct colors now
				basecolormap = frontsector->ColorMap;
				wallshade = ds->shade;
				if (fixedlightlev < 0)
				{
					if ((ds->bFakeBoundary & 3) == 2)
					{
						for (j = backsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
						{
							if (clip3d->sclipTop <= backsector->e->XFloor.lightlist[j].plane.Zat0())
							{
								lightlist_t *lit = &backsector->e->XFloor.lightlist[j];
								basecolormap = lit->extra_colormap;
								wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + r_actualextralight);
								break;
							}
						}
					}
					else
					{
						for (j = frontsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
						{
							if (clip3d->sclipTop <= frontsector->e->XFloor.lightlist[j].plane.Zat0())
							{
								lightlist_t *lit = &frontsector->e->XFloor.lightlist[j];
								basecolormap = lit->extra_colormap;
								wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + r_actualextralight);
								break;
							}
						}
					}
				}
				if (rw_pic != DONT_DRAW)
				{
					R_RenderFakeWall(ds, x1, x2, fover ? fover : rover, wallshade);
				}
				else rw_pic = nullptr;
				break;
			}
		}
		else
		{ // top to viewz
			for (i = 0; i < (int)backsector->e->XFloor.ffloors.Size(); i++)
			{
				rover = backsector->e->XFloor.ffloors[i];
				if (!(rover->flags & FF_EXISTS)) continue;

				// visible?
				passed = 0;
				if (!(rover->flags & FF_RENDERSIDES) ||
					rover->top.plane->isSlope() || rover->bottom.plane->isSlope() ||
					rover->bottom.plane->Zat0() >= clip3d->sclipTop ||
					rover->top.plane->Zat0() <= floorHeight ||
					rover->bottom.plane->Zat0() >= ceilingHeight)
				{
					if ((unsigned)i == backsector->e->XFloor.ffloors.Size() - 1)
					{
						passed = 1;
					}
					else
					{
						continue;
					}
				}
				rw_pic = nullptr;
				if (rover->top.plane->Zat0() <= clip3d->sclipBottom || passed)
				{ // maybe wall from inside rendering?
					fover = nullptr;
					for (j = 0; j < (int)frontsector->e->XFloor.ffloors.Size(); j++)
					{
						fover = frontsector->e->XFloor.ffloors[j];
						if (fover->model == rover->model)
						{ // never
							fover = nullptr;
							break;
						}
						if (!(fover->flags & FF_EXISTS)) continue;
						if (!(fover->flags & FF_RENDERSIDES)) continue;
						// no sloped walls, it's bugged
						if (fover->top.plane->isSlope() || fover->bottom.plane->isSlope()) continue;

						// visible?
						if (fover->bottom.plane->Zat0() >= clip3d->sclipTop) continue; // no
						if (fover->top.plane->Zat0() <= clip3d->sclipBottom)
						{ // no, last possible
							fover = nullptr;
							break;
						}
						// it is, render inside?
						if (!(fover->flags & (FF_BOTHPLANES | FF_INVERTPLANES)))
						{ // no
							fover = nullptr;
						}
						break;
					}
					// nothing
					if (!fover || (unsigned)j == frontsector->e->XFloor.ffloors.Size())
					{
						break;
					}
					// correct texture
					if (fover->flags & rover->flags & FF_SWIMMABLE)
					{
						rw_pic = DONT_DRAW;	// don't ever draw (but treat as something has been found)
					}
					else if (fover->flags & FF_UPPERTEXTURE)
					{
						rw_pic = TexMan(curline->sidedef->GetTexture(side_t::top), true);
					}
					else if (fover->flags & FF_LOWERTEXTURE)
					{
						rw_pic = TexMan(curline->sidedef->GetTexture(side_t::bottom), true);
					}
					else
					{
						rw_pic = TexMan(fover->master->sidedef[0]->GetTexture(side_t::mid), true);
					}
				}
				else if (frontsector->e->XFloor.ffloors.Size())
				{ // maybe not visible?
					fover = nullptr;
					for (j = 0; j < (int)frontsector->e->XFloor.ffloors.Size(); j++)
					{
						fover = frontsector->e->XFloor.ffloors[j];
						if (fover->model == rover->model)
						{ // never
							break;
						}
						if (!(fover->flags & FF_EXISTS)) continue;
						if (!(fover->flags & FF_RENDERSIDES)) continue;
						// no sloped walls, its bugged
						if (fover->top.plane->isSlope() || fover->bottom.plane->isSlope()) continue;

						// visible?
						if (fover->bottom.plane->Zat0() >= clip3d->sclipTop) continue; // no
						if (fover->top.plane->Zat0() <= clip3d->sclipBottom)
						{ // visible, last possible
							fover = nullptr;
							break;
						}
						if ((fover->flags & FF_SOLID) == (rover->flags & FF_SOLID) &&
							!(!(rover->flags & FF_SOLID) && (fover->alpha == 255 || rover->alpha == 255))
							)
						{
							break;
						}
						if (fover->flags & rover->flags & FF_SWIMMABLE)
						{ // don't ever draw (but treat as something has been found)
							rw_pic = DONT_DRAW;
						}
						fover = nullptr; // visible
						break;
					}
					if (fover && (unsigned)j != frontsector->e->XFloor.ffloors.Size())
					{ // not visible
						break;
					}
				}
				if (rw_pic == nullptr)
				{
					fover = nullptr;
					if (rover->flags & FF_UPPERTEXTURE)
					{
						rw_pic = TexMan(curline->sidedef->GetTexture(side_t::top), true);
					}
					else if (rover->flags & FF_LOWERTEXTURE)
					{
						rw_pic = TexMan(curline->sidedef->GetTexture(side_t::bottom), true);
					}
					else
					{
						rw_pic = TexMan(rover->master->sidedef[0]->GetTexture(side_t::mid), true);
					}
				}
				// correct colors now
				basecolormap = frontsector->ColorMap;
				wallshade = ds->shade;
				if (fixedlightlev < 0)
				{
					if ((ds->bFakeBoundary & 3) == 2)
					{
						for (j = backsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
						{
							if (clip3d->sclipTop <= backsector->e->XFloor.lightlist[j].plane.Zat0())
							{
								lightlist_t *lit = &backsector->e->XFloor.lightlist[j];
								basecolormap = lit->extra_colormap;
								wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + r_actualextralight);
								break;
							}
						}
					}
					else
					{
						for (j = frontsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
						{
							if (clip3d->sclipTop <= frontsector->e->XFloor.lightlist[j].plane.Zat0())
							{
								lightlist_t *lit = &frontsector->e->XFloor.lightlist[j];
								basecolormap = lit->extra_colormap;
								wallshade = LIGHT2SHADE(curline->sidedef->GetLightLevel(foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + r_actualextralight);
								break;
							}
						}
					}
				}

				if (rw_pic != DONT_DRAW)
				{
					R_RenderFakeWall(ds, x1, x2, fover ? fover : rover, wallshade);
				}
				else
				{
					rw_pic = nullptr;
				}
				break;
			}
		}
		return;
	}
}
