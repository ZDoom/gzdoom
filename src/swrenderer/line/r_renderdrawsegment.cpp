//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
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
#include "g_levellocals.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "d_net.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/line/r_fogboundary.h"
#include "swrenderer/line/r_renderdrawsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/viewport/r_spritedrawer.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);

namespace swrenderer
{
	RenderDrawSegment::RenderDrawSegment(RenderThread *thread)
	{
		Thread = thread;
	}

	void RenderDrawSegment::Render(DrawSegment *ds, int x1, int x2, Fake3DTranslucent clip3DFloor)
	{
		auto viewport = Thread->Viewport.get();

		curline = ds->curline;
		m3DFloor = clip3DFloor;

		float alpha = (float)MIN(curline->linedef->alpha, 1.);
		bool additive = (curline->linedef->flags & ML_ADDTRANS) != 0;

		WallDrawerArgs walldrawerargs;
		walldrawerargs.SetStyle(true, additive, FLOAT2FIXED(alpha));

		SpriteDrawerArgs columndrawerargs;
		FDynamicColormap *patchstylecolormap = nullptr;
		bool visible = columndrawerargs.SetStyle(viewport, LegacyRenderStyles[additive ? STYLE_Add : STYLE_Translucent], alpha, 0, 0, patchstylecolormap);

		if (!visible && !ds->bFogBoundary && !ds->Has3DFloorWalls())
		{
			return;
		}

		if (Thread->MainThread)
			NetUpdate();

		frontsector = curline->frontsector;
		backsector = curline->backsector;

		// killough 4/13/98: get correct lightlevel for 2s normal textures
		sector_t tempsec;
		const sector_t *sec = Thread->OpaquePass->FakeFlat(frontsector, &tempsec, nullptr, nullptr, nullptr, 0, 0, 0, 0);

		FDynamicColormap *basecolormap = GetColorTable(sec->Colormap, sec->SpecialColors[sector_t::walltop]);	// [RH] Set basecolormap

		int wallshade = ds->shade;
		rw_lightstep = ds->lightstep;
		rw_light = ds->light + (x1 - ds->x1) * rw_lightstep;

		Clip3DFloors *clip3d = Thread->Clip3D.get();

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedLightLevel() < 0)
		{
			double clipTop = m3DFloor.clipTop ? m3DFloor.sclipTop : sec->ceilingplane.ZatPoint(Thread->Viewport->viewpoint.Pos);
			for (int i = frontsector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
			{
				if (clipTop <= frontsector->e->XFloor.lightlist[i].plane.Zat0())
				{
					lightlist_t *lit = &frontsector->e->XFloor.lightlist[i];
					basecolormap = GetColorTable(lit->extra_colormap, frontsector->SpecialColors[sector_t::walltop]);
					bool foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE)); // [RH] set foggy flag
					wallshade = LightVisibility::LightLevelToShade(curline->sidedef->GetLightLevel(ds->foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + LightVisibility::ActualExtraLight(ds->foggy, viewport), foggy);
					break;
				}
			}
		}

		// [RH] Draw fog partition
		bool renderwall = true;
		bool notrelevant = false;
		if (ds->bFogBoundary)
		{
			const short *mfloorclip = ds->sprbottomclip - ds->x1;
			const short *mceilingclip = ds->sprtopclip - ds->x1;

			RenderFogBoundary renderfog;
			renderfog.Render(Thread, x1, x2, mceilingclip, mfloorclip, wallshade, rw_light, rw_lightstep, basecolormap);

			if (ds->maskedtexturecol == nullptr)
				renderwall = false;
		}
		else if ((ds->Has3DFloorWalls() && !ds->Has3DFloorMidTexture()) || !visible)
		{
			renderwall = false;
		}

		if (renderwall)
			notrelevant = RenderWall(ds, x1, x2, walldrawerargs, columndrawerargs, visible, basecolormap, wallshade);

		if (ds->Has3DFloorFrontSectorWalls() || ds->Has3DFloorBackSectorWalls())
		{
			RenderFakeWallRange(ds, x1, x2, wallshade);
		}
		if (!notrelevant)
		{
			ds->sprclipped = true;
			fillshort(ds->sprtopclip - ds->x1 + x1, x2 - x1, viewheight);
		}
	}

	bool RenderDrawSegment::RenderWall(DrawSegment *ds, int x1, int x2, WallDrawerArgs &walldrawerargs, SpriteDrawerArgs &columndrawerargs, bool visible, FDynamicColormap *basecolormap, int wallshade)
	{
		auto renderstyle = DefaultRenderStyle();
		auto viewport = Thread->Viewport.get();
		Clip3DFloors *clip3d = Thread->Clip3D.get();

		FTexture *tex = TexMan(curline->sidedef->GetTexture(side_t::mid), true);
		if (i_compatflags & COMPATF_MASKEDMIDTEX)
		{
			tex = tex->GetRawTexture();
		}

		const short *mfloorclip = ds->sprbottomclip - ds->x1;
		const short *mceilingclip = ds->sprtopclip - ds->x1;

		float *MaskedSWall = ds->swall - ds->x1;
		float MaskedScaleY = ds->yscale;
		fixed_t *maskedtexturecol = ds->maskedtexturecol - ds->x1;
		double spryscale = ds->iscale + ds->iscalestep * (x1 - ds->x1);
		float rw_scalestep = ds->iscalestep;

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedLightLevel() >= 0)
		{
			walldrawerargs.SetLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap, 0, cameraLight->FixedLightLevelShade());
			columndrawerargs.SetLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap, 0, cameraLight->FixedLightLevelShade());
		}
		else if (cameraLight->FixedColormap() != nullptr)
		{
			walldrawerargs.SetLight(cameraLight->FixedColormap(), 0, 0);
			columndrawerargs.SetLight(cameraLight->FixedColormap(), 0, 0);
		}

		// find positioning
		double texheight = tex->GetScaledHeightDouble();
		double texheightscale = fabs(curline->sidedef->GetTextureYScale(side_t::mid));
		if (texheightscale != 1)
		{
			texheight = texheight / texheightscale;
		}

		double texturemid;
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			texturemid = MAX(frontsector->GetPlaneTexZ(sector_t::floor), backsector->GetPlaneTexZ(sector_t::floor)) + texheight;
		}
		else
		{
			texturemid = MIN(frontsector->GetPlaneTexZ(sector_t::ceiling), backsector->GetPlaneTexZ(sector_t::ceiling));
		}

		double rowoffset = curline->sidedef->GetTextureYOffset(side_t::mid);

		bool wrap = (curline->linedef->flags & ML_WRAP_MIDTEX) || (curline->sidedef->Flags & WALLF_WRAP_MIDTEX);
		if (!wrap)
		{ // Texture does not wrap vertically.
			double textop;

			bool sprflipvert = false;

			if (MaskedScaleY < 0)
			{
				MaskedScaleY = -MaskedScaleY;
				sprflipvert = true;
			}
			if (tex->bWorldPanning)
			{
				// rowoffset is added before the multiply so that the masked texture will
				// still be positioned in world units rather than texels.
				texturemid += rowoffset - Thread->Viewport->viewpoint.Pos.Z;
				textop = texturemid;
				texturemid *= MaskedScaleY;
			}
			else
			{
				// rowoffset is added outside the multiply so that it positions the texture
				// by texels instead of world units.
				textop = texturemid + rowoffset / MaskedScaleY - Thread->Viewport->viewpoint.Pos.Z;
				texturemid = (texturemid - Thread->Viewport->viewpoint.Pos.Z) * MaskedScaleY + rowoffset;
			}
			if (sprflipvert)
			{
				MaskedScaleY = -MaskedScaleY;
				texturemid -= tex->GetHeight() << FRACBITS;
			}

			// [RH] Don't bother drawing segs that are completely offscreen
			if (viewport->globaldclip * ds->sz1 < -textop && viewport->globaldclip * ds->sz2 < -textop)
			{ // Texture top is below the bottom of the screen
				return false;
			}

			if (viewport->globaluclip * ds->sz1 > texheight - textop && viewport->globaluclip * ds->sz2 > texheight - textop)
			{ // Texture bottom is above the top of the screen
				return false;
			}

			if (m3DFloor.clipBottom && textop < m3DFloor.sclipBottom - Thread->Viewport->viewpoint.Pos.Z)
			{
				return true;
			}
			if (m3DFloor.clipTop && textop - texheight > m3DFloor.sclipTop - Thread->Viewport->viewpoint.Pos.Z)
			{
				return true;
			}

			WallC.sz1 = ds->sz1;
			WallC.sz2 = ds->sz2;
			WallC.sx1 = ds->sx1;
			WallC.sx2 = ds->sx2;

			// Unclipped vanilla Doom range for the wall. Relies on ceiling/floor clip to clamp the wall in range.
			double ceilZ = textop;
			double floorZ = textop - texheight;

			// The 3D Floors implementation destroys the ceiling clip when doing its height passes..
			if (m3DFloor.clipTop || m3DFloor.clipBottom)
			{
				// Use the actual correct wall range for the midtexture.
				// This doesn't work for self-referenced sectors, which is why we only do it if we have 3D floors.

				double top, bot;
				GetMaskedWallTopBottom(ds, top, bot);
				top -= Thread->Viewport->viewpoint.Pos.Z;
				bot -= Thread->Viewport->viewpoint.Pos.Z;

				ceilZ = MIN(ceilZ, top);
				floorZ = MAX(floorZ, bot);
			}

			// Clip wall by the current 3D floor render range.
			if (m3DFloor.clipTop)
			{
				double clipZ = m3DFloor.sclipTop - Thread->Viewport->viewpoint.Pos.Z;
				ceilZ = MIN(ceilZ, clipZ);
			}
			if (m3DFloor.clipBottom)
			{
				double clipZ = m3DFloor.sclipBottom - Thread->Viewport->viewpoint.Pos.Z;
				floorZ = MAX(floorZ, clipZ);
			}

			wallupper.Project(Thread->Viewport.get(), ceilZ, &WallC);
			walllower.Project(Thread->Viewport.get(), floorZ, &WallC);

			for (int i = x1; i < x2; i++)
			{
				if (wallupper.ScreenY[i] < mceilingclip[i])
					wallupper.ScreenY[i] = mceilingclip[i];
			}
			for (int i = x1; i < x2; i++)
			{
				if (walllower.ScreenY[i] > mfloorclip[i])
					walllower.ScreenY[i] = mfloorclip[i];
			}

			if (clip3d->CurrentSkybox)
			{ // Midtex clipping doesn't work properly with skyboxes, since you're normally below the floor
			  // or above the ceiling, so the appropriate end won't be clipped automatically when adding
			  // this drawseg.
				if ((curline->linedef->flags & ML_CLIP_MIDTEX) ||
					(curline->sidedef->Flags & WALLF_CLIP_MIDTEX) ||
					(ib_compatflags & BCOMPATF_CLIPMIDTEX))
				{
					ClipMidtex(x1, x2);
				}
			}

			mfloorclip = walllower.ScreenY;
			mceilingclip = wallupper.ScreenY;

			// draw the columns one at a time
			if (visible)
			{
				Thread->PrepareTexture(tex, renderstyle);
				for (int x = x1; x < x2; ++x)
				{
					if (cameraLight->FixedColormap() == nullptr && cameraLight->FixedLightLevel() < 0)
					{
						columndrawerargs.SetLight(basecolormap, rw_light, wallshade);
					}

					fixed_t iscale = xs_Fix<16>::ToFix(MaskedSWall[x] * MaskedScaleY);
					double sprtopscreen;
					if (sprflipvert)
						sprtopscreen = viewport->CenterY + texturemid * spryscale;
					else
						sprtopscreen = viewport->CenterY - texturemid * spryscale;

					columndrawerargs.DrawMaskedColumn(Thread, x, iscale, tex, maskedtexturecol[x], spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, renderstyle);

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
				texturemid = (texturemid - Thread->Viewport->viewpoint.Pos.Z + rowoffset) * MaskedScaleY;
			}
			else
			{
				// rowoffset is added outside the multiply so that it positions the texture
				// by texels instead of world units.
				texturemid = (texturemid - Thread->Viewport->viewpoint.Pos.Z) * MaskedScaleY + rowoffset;
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
					(curline->sidedef->Flags & WALLF_CLIP_MIDTEX) ||
					(ib_compatflags & BCOMPATF_CLIPMIDTEX))
				{
					ClipMidtex(x1, x2);
				}
			}

			if (m3DFloor.clipTop)
			{
				wallupper.Project(Thread->Viewport.get(), m3DFloor.sclipTop - Thread->Viewport->viewpoint.Pos.Z, &WallC);
				for (int i = x1; i < x2; i++)
				{
					if (wallupper.ScreenY[i] < mceilingclip[i])
						wallupper.ScreenY[i] = mceilingclip[i];
				}
				mceilingclip = wallupper.ScreenY;
			}
			if (m3DFloor.clipBottom)
			{
				walllower.Project(Thread->Viewport.get(), m3DFloor.sclipBottom - Thread->Viewport->viewpoint.Pos.Z, &WallC);
				for (int i = x1; i < x2; i++)
				{
					if (walllower.ScreenY[i] > mfloorclip[i])
						walllower.ScreenY[i] = mfloorclip[i];
				}
				mfloorclip = walllower.ScreenY;
			}

			rw_offset = 0;
			rw_pic = tex;

			double top, bot;
			GetMaskedWallTopBottom(ds, top, bot);

			RenderWallPart renderWallpart(Thread);
			renderWallpart.Render(walldrawerargs, frontsector, curline, WallC, rw_pic, x1, x2, mceilingclip, mfloorclip, texturemid, MaskedSWall, maskedtexturecol, ds->yscale, top, bot, true, wallshade, rw_offset, rw_light, rw_lightstep, nullptr, ds->foggy, basecolormap);
		}

		return false;
	}

	// kg3D - render one fake wall
	void RenderDrawSegment::RenderFakeWall(DrawSegment *ds, int x1, int x2, F3DFloor *rover, int wallshade, FDynamicColormap *basecolormap, double clipTop, double clipBottom)
	{
		int i;
		double xscale;
		double yscale;

		fixed_t Alpha = Scale(rover->alpha, OPAQUE, 255);
		if (Alpha <= 0)
			return;

		WallDrawerArgs drawerargs;
		drawerargs.SetStyle(true, (rover->flags & FF_ADDITIVETRANS) != 0, Alpha);

		rw_lightstep = ds->lightstep;
		rw_light = ds->light + (x1 - ds->x1) * rw_lightstep;

		const short *mfloorclip = ds->sprbottomclip - ds->x1;
		const short *mceilingclip = ds->sprtopclip - ds->x1;

		//double spryscale = ds->iscale + ds->iscalestep * (x1 - ds->x1);
		float *MaskedSWall = ds->swall - ds->x1;

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
		double texturemid = (planez - Thread->Viewport->viewpoint.Pos.Z) * yscale;
		if (rw_pic->bWorldPanning)
		{
			// rowoffset is added before the multiply so that the masked texture will
			// still be positioned in world units rather than texels.

			texturemid = texturemid + rowoffset * yscale;
			rw_offset = xs_RoundToInt(rw_offset * xscale);
		}
		else
		{
			// rowoffset is added outside the multiply so that it positions the texture
			// by texels instead of world units.
			texturemid += rowoffset;
		}

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedLightLevel() >= 0)
			drawerargs.SetLight((r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap, 0, cameraLight->FixedLightLevelShade());
		else if (cameraLight->FixedColormap() != nullptr)
			drawerargs.SetLight(cameraLight->FixedColormap(), 0, 0);

		WallC.sz1 = ds->sz1;
		WallC.sz2 = ds->sz2;
		WallC.sx1 = ds->sx1;
		WallC.sx2 = ds->sx2;
		WallC.tleft.X = ds->cx;
		WallC.tleft.Y = ds->cy;
		WallC.tright.X = ds->cx + ds->cdx;
		WallC.tright.Y = ds->cy + ds->cdy;
		WallT = ds->tmapvals;

		Clip3DFloors *clip3d = Thread->Clip3D.get();
		wallupper.Project(Thread->Viewport.get(), clipTop - Thread->Viewport->viewpoint.Pos.Z, &WallC);
		walllower.Project(Thread->Viewport.get(), clipBottom - Thread->Viewport->viewpoint.Pos.Z, &WallC);

		for (i = x1; i < x2; i++)
		{
			if (wallupper.ScreenY[i] < mceilingclip[i])
				wallupper.ScreenY[i] = mceilingclip[i];
		}
		for (i = x1; i < x2; i++)
		{
			if (walllower.ScreenY[i] > mfloorclip[i])
				walllower.ScreenY[i] = mfloorclip[i];
		}

		ProjectedWallTexcoords walltexcoords;
		walltexcoords.ProjectPos(Thread->Viewport.get(), curline->sidedef->TexelLength*xscale, ds->sx1, ds->sx2, WallT);

		double top, bot;
		GetMaskedWallTopBottom(ds, top, bot);

		RenderWallPart renderWallpart(Thread);
		renderWallpart.Render(drawerargs, frontsector, curline, WallC, rw_pic, x1, x2, wallupper.ScreenY, walllower.ScreenY, texturemid, MaskedSWall, walltexcoords.UPos, yscale, top, bot, true, wallshade, rw_offset, rw_light, rw_lightstep, nullptr, ds->foggy, basecolormap);

		RenderDecal::RenderDecals(Thread, curline->sidedef, ds, wallshade, rw_light, rw_lightstep, curline, WallC, ds->foggy, basecolormap, wallupper.ScreenY, walllower.ScreenY, true);
	}

	// kg3D - walls of fake floors
	void RenderDrawSegment::RenderFakeWallRange(DrawSegment *ds, int x1, int x2, int wallshade)
	{
		FTexture *const DONT_DRAW = ((FTexture*)(intptr_t)-1);
		int i, j;
		F3DFloor *rover, *fover = nullptr;
		int passed, last;
		double floorHeight;
		double ceilingHeight;

		curline = ds->curline;

		frontsector = curline->frontsector;
		backsector = curline->backsector;

		if (backsector == nullptr)
		{
			return;
		}
		if (ds->Has3DFloorFrontSectorWalls() && !ds->Has3DFloorBackSectorWalls())
		{
			sector_t *sec = backsector;
			backsector = frontsector;
			frontsector = sec;
		}

		floorHeight = backsector->CenterFloor();
		ceilingHeight = backsector->CenterCeiling();

		Clip3DFloors *clip3d = Thread->Clip3D.get();

		double clipTop = m3DFloor.clipTop ? m3DFloor.sclipTop : ceilingHeight;
		double clipBottom = m3DFloor.clipBottom ? m3DFloor.sclipBottom : floorHeight;

		// maybe not visible
		if (clipBottom >= frontsector->CenterCeiling()) return;
		if (clipTop <= frontsector->CenterFloor()) return;

		if (m3DFloor.down2Up)
		{ // bottom to viewz
			last = 0;
			for (i = backsector->e->XFloor.ffloors.Size() - 1; i >= 0; i--)
			{
				rover = backsector->e->XFloor.ffloors[i];
				if (!(rover->flags & FF_EXISTS)) continue;

				// visible?
				passed = 0;
				if (!(rover->flags & FF_RENDERSIDES) || rover->top.plane->isSlope() || rover->bottom.plane->isSlope() ||
					rover->top.plane->Zat0() <= clipBottom ||
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
				if (rover->bottom.plane->Zat0() >= clipTop || passed)
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
						if (fover->top.plane->Zat0() <= clipBottom) continue; // no
						if (fover->bottom.plane->Zat0() >= clipTop)
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
						if (fover->top.plane->Zat0() <= clipBottom) continue; // no
						if (fover->bottom.plane->Zat0() >= clipTop)
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
				FDynamicColormap *basecolormap = nullptr;
				wallshade = ds->shade;
				CameraLight *cameraLight = CameraLight::Instance();
				if (cameraLight->FixedLightLevel() < 0)
				{
					if (ds->Has3DFloorFrontSectorWalls() && !ds->Has3DFloorBackSectorWalls())
					{
						for (j = backsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
						{
							if (clipTop <= backsector->e->XFloor.lightlist[j].plane.Zat0())
							{
								lightlist_t *lit = &backsector->e->XFloor.lightlist[j];
								basecolormap = GetColorTable(lit->extra_colormap, frontsector->SpecialColors[sector_t::walltop]);
								bool foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE)); // [RH] set foggy flag
								wallshade = LightVisibility::LightLevelToShade(curline->sidedef->GetLightLevel(ds->foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + LightVisibility::ActualExtraLight(ds->foggy, Thread->Viewport.get()), foggy);
								break;
							}
						}
					}
					else
					{
						for (j = frontsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
						{
							if (clipTop <= frontsector->e->XFloor.lightlist[j].plane.Zat0())
							{
								lightlist_t *lit = &frontsector->e->XFloor.lightlist[j];
								basecolormap = GetColorTable(lit->extra_colormap, frontsector->SpecialColors[sector_t::walltop]);
								bool foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE)); // [RH] set foggy flag
								wallshade = LightVisibility::LightLevelToShade(curline->sidedef->GetLightLevel(ds->foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + LightVisibility::ActualExtraLight(ds->foggy, Thread->Viewport.get()), foggy);
								break;
							}
						}
					}
				}
				if (basecolormap == nullptr) basecolormap = GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::walltop]);

				if (rw_pic != DONT_DRAW)
				{
					RenderFakeWall(ds, x1, x2, fover ? fover : rover, wallshade, basecolormap, clipTop, clipBottom);
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
					rover->bottom.plane->Zat0() >= clipTop ||
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
				if (rover->top.plane->Zat0() <= clipBottom || passed)
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
						if (fover->bottom.plane->Zat0() >= clipTop) continue; // no
						if (fover->top.plane->Zat0() <= clipBottom)
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
						if (fover->bottom.plane->Zat0() >= clipTop) continue; // no
						if (fover->top.plane->Zat0() <= clipBottom)
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
				FDynamicColormap *basecolormap = nullptr;
				wallshade = ds->shade;
				CameraLight *cameraLight = CameraLight::Instance();
				if (cameraLight->FixedLightLevel() < 0)
				{
					if (ds->Has3DFloorFrontSectorWalls() && !ds->Has3DFloorBackSectorWalls())
					{
						for (j = backsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
						{
							if (clipTop <= backsector->e->XFloor.lightlist[j].plane.Zat0())
							{
								lightlist_t *lit = &backsector->e->XFloor.lightlist[j];
								basecolormap = GetColorTable(lit->extra_colormap, frontsector->SpecialColors[sector_t::walltop]);
								bool foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE)); // [RH] set foggy flag
								wallshade = LightVisibility::LightLevelToShade(curline->sidedef->GetLightLevel(ds->foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + LightVisibility::ActualExtraLight(ds->foggy, Thread->Viewport.get()), foggy);
								break;
							}
						}
					}
					else
					{
						for (j = frontsector->e->XFloor.lightlist.Size() - 1; j >= 0; j--)
						{
							if (clipTop <= frontsector->e->XFloor.lightlist[j].plane.Zat0())
							{
								lightlist_t *lit = &frontsector->e->XFloor.lightlist[j];
								basecolormap = GetColorTable(lit->extra_colormap, frontsector->SpecialColors[sector_t::walltop]);
								bool foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE)); // [RH] set foggy flag
								wallshade = LightVisibility::LightLevelToShade(curline->sidedef->GetLightLevel(ds->foggy, *lit->p_lightlevel, lit->lightsource != nullptr) + LightVisibility::ActualExtraLight(ds->foggy, Thread->Viewport.get()), foggy);
								break;
							}
						}
					}
				}
				if (basecolormap == nullptr) basecolormap = GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::walltop]);

				if (rw_pic != DONT_DRAW)
				{
					RenderFakeWall(ds, x1, x2, fover ? fover : rover, wallshade, basecolormap, clipTop, clipBottom);
				}
				else
				{
					rw_pic = nullptr;
				}
				break;
			}
		}
	}

	// Clip a midtexture to the floor and ceiling of the sector in front of it.
	void RenderDrawSegment::ClipMidtex(int x1, int x2)
	{
		ProjectedWallLine most;

		RenderPortal *renderportal = Thread->Portal.get();

		most.Project(Thread->Viewport.get(), curline->frontsector->ceilingplane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
		for (int i = x1; i < x2; ++i)
		{
			if (wallupper.ScreenY[i] < most.ScreenY[i])
				wallupper.ScreenY[i] = most.ScreenY[i];
		}
		most.Project(Thread->Viewport.get(), curline->frontsector->floorplane, &WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
		for (int i = x1; i < x2; ++i)
		{
			if (walllower.ScreenY[i] > most.ScreenY[i])
				walllower.ScreenY[i] = most.ScreenY[i];
		}
	}

	void RenderDrawSegment::GetMaskedWallTopBottom(DrawSegment *ds, double &top, double &bot)
	{
		double frontcz1 = ds->curline->frontsector->ceilingplane.ZatPoint(ds->curline->v1);
		double frontfz1 = ds->curline->frontsector->floorplane.ZatPoint(ds->curline->v1);
		double frontcz2 = ds->curline->frontsector->ceilingplane.ZatPoint(ds->curline->v2);
		double frontfz2 = ds->curline->frontsector->floorplane.ZatPoint(ds->curline->v2);
		top = MAX(frontcz1, frontcz2);
		bot = MIN(frontfz1, frontfz2);

		if (m3DFloor.clipTop)
		{
			top = MIN(top, m3DFloor.sclipTop);
		}
		if (m3DFloor.clipBottom)
		{
			bot = MAX(bot, m3DFloor.sclipBottom);
		}
	}
}
