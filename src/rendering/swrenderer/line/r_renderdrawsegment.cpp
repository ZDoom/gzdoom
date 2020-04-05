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

		frontsector = curline->frontsector;
		backsector = curline->backsector;

		if (ds->HasFogBoundary())
			RenderFog(ds, x1, x2);

		if (ds->HasTranslucentMidTexture())
			RenderWall(ds, x1, x2);

		if (ds->Has3DFloorWalls())
			Render3DFloorWallRange(ds, x1, x2);

		if (ds->HasFogBoundary() || ds->HasTranslucentMidTexture() || ds->Has3DFloorWalls())
			ds->drawsegclip.SetRangeDrawn(x1, x2);
	}

	void RenderDrawSegment::RenderWall(DrawSegment *ds, int x1, int x2)
	{
		auto renderstyle = DefaultRenderStyle();
		auto viewport = Thread->Viewport.get();
		Clip3DFloors *clip3d = Thread->Clip3D.get();

		FTexture *ttex = TexMan.GetPalettedTexture(curline->sidedef->GetTexture(side_t::mid), true);
		if (curline->GetLevel()->i_compatflags & COMPATF_MASKEDMIDTEX)
		{
			ttex = ttex->GetRawTexture();
		}
		FSoftwareTexture *tex = ttex->GetSoftwareTexture();

		const short *mfloorclip = ds->drawsegclip.sprbottomclip;
		const short *mceilingclip = ds->drawsegclip.sprtopclip;

		bool wrap = (curline->linedef->flags & ML_WRAP_MIDTEX) || (curline->sidedef->Flags & WALLF_WRAP_MIDTEX);
		if (!wrap)
		{ // Texture does not wrap vertically.

			double ceilZ, floorZ;
			GetNoWrapMidTextureZ(ds, tex, ceilZ, floorZ);

			// Texture top is below the bottom of the screen
			if (viewport->globaldclip * ds->WallC.sz1 < -ceilZ && viewport->globaldclip * ds->WallC.sz2 < -ceilZ) return;

			// Texture bottom is above the top of the screen
			if (viewport->globaluclip * ds->WallC.sz1 > -floorZ && viewport->globaluclip * ds->WallC.sz2 > -floorZ) return;

			if (m3DFloor.clipBottom && ceilZ < m3DFloor.sclipBottom - Thread->Viewport->viewpoint.Pos.Z) return;
			if (m3DFloor.clipTop && floorZ > m3DFloor.sclipTop - Thread->Viewport->viewpoint.Pos.Z) return;

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

			wallupper.Project(Thread->Viewport.get(), ceilZ, &ds->WallC);
			walllower.Project(Thread->Viewport.get(), floorZ, &ds->WallC);

			wallupper.ClipTop(x1, x2, ds->drawsegclip);
			walllower.ClipBottom(x1, x2, ds->drawsegclip);

			if (clip3d->CurrentSkybox)
			{ // Midtex clipping doesn't work properly with skyboxes, since you're normally below the floor
			  // or above the ceiling, so the appropriate end won't be clipped automatically when adding
			  // this drawseg.
				if ((curline->linedef->flags & ML_CLIP_MIDTEX) ||
					(curline->sidedef->Flags & WALLF_CLIP_MIDTEX) ||
					(curline->GetLevel()->ib_compatflags & BCOMPATF_CLIPMIDTEX))
				{
					ClipMidtex(ds, x1, x2);
				}
			}

			mfloorclip = walllower.ScreenY;
			mceilingclip = wallupper.ScreenY;
		}
		else
		{ // Texture does wrap vertically.

			if (clip3d->CurrentSkybox)
			{ // Midtex clipping doesn't work properly with skyboxes, since you're normally below the floor
			  // or above the ceiling, so the appropriate end won't be clipped automatically when adding
			  // this drawseg.
				if ((curline->linedef->flags & ML_CLIP_MIDTEX) ||
					(curline->sidedef->Flags & WALLF_CLIP_MIDTEX) ||
					(curline->GetLevel()->ib_compatflags & BCOMPATF_CLIPMIDTEX))
				{
					ClipMidtex(ds, x1, x2);
				}
			}

			if (m3DFloor.clipTop)
			{
				wallupper.Project(Thread->Viewport.get(), m3DFloor.sclipTop - Thread->Viewport->viewpoint.Pos.Z, &ds->WallC);
				wallupper.ClipTop(x1, x2, ds->drawsegclip);
				mceilingclip = wallupper.ScreenY;
			}
			if (m3DFloor.clipBottom)
			{
				walllower.Project(Thread->Viewport.get(), m3DFloor.sclipBottom - Thread->Viewport->viewpoint.Pos.Z, &ds->WallC);
				walllower.ClipBottom(x1, x2, ds->drawsegclip);
				mfloorclip = walllower.ScreenY;
			}
		}

		sector_t tempsec;
		const sector_t* lightsector = Thread->OpaquePass->FakeFlat(frontsector, &tempsec, nullptr, nullptr, nullptr, 0, 0, 0, 0);

		fixed_t alpha = FLOAT2FIXED((float)MIN(curline->linedef->alpha, 1.));
		bool additive = (curline->linedef->flags & ML_ADDTRANS) != 0;

		RenderWallPart renderWallpart(Thread);
		renderWallpart.Render(lightsector, curline, ds->WallC, tex, x1, x2, mceilingclip, mfloorclip, ds->texcoords, true, additive, alpha);
	}

	void RenderDrawSegment::Render3DFloorWall(DrawSegment *ds, int x1, int x2, F3DFloor *rover, double clipTop, double clipBottom, FSoftwareTexture *rw_pic)
	{
		fixed_t Alpha = Scale(rover->alpha, OPAQUE, 255);
		if (Alpha <= 0)
			return;

		sector_t* lightsector = (ds->Has3DFloorFrontSectorWalls() && !ds->Has3DFloorBackSectorWalls()) ? backsector : frontsector;

		wallupper.Project(Thread->Viewport.get(), clipTop - Thread->Viewport->viewpoint.Pos.Z, &ds->WallC);
		walllower.Project(Thread->Viewport.get(), clipBottom - Thread->Viewport->viewpoint.Pos.Z, &ds->WallC);

		wallupper.ClipTop(x1, x2, ds->drawsegclip);
		walllower.ClipBottom(x1, x2, ds->drawsegclip);

		ProjectedWallTexcoords walltexcoords;
		walltexcoords.Project3DFloor(Thread->Viewport.get(), rover, curline, ds->WallC, rw_pic);

		RenderWallPart renderWallpart(Thread);
		renderWallpart.Render(lightsector, curline, ds->WallC, rw_pic, x1, x2, wallupper.ScreenY, walllower.ScreenY, walltexcoords, true, (rover->flags & FF_ADDITIVETRANS) != 0, Alpha);

		RenderDecal::RenderDecals(Thread, ds, curline, lightsector, wallupper.ScreenY, walllower.ScreenY, true);
	}

	void RenderDrawSegment::Render3DFloorWallRange(DrawSegment *ds, int x1, int x2)
	{
		int i, j;
		F3DFloor *rover, *fover = nullptr;
		int passed, last;
		double floorHeight;
		double ceilingHeight;

		curline = ds->curline;

		frontsector = curline->frontsector;
		backsector = curline->backsector;

		if (!backsector)
			return;

		if (ds->Has3DFloorFrontSectorWalls() && !ds->Has3DFloorBackSectorWalls())
			std::swap(frontsector, backsector);

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

				FSoftwareTexture *rw_pic = nullptr;
				bool swimmable_found = false;

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
						swimmable_found = true;
					}
					else
					{
						FTexture *rw_tex = nullptr;
						if (fover->flags & FF_UPPERTEXTURE)
						{
							rw_tex = TexMan.GetPalettedTexture(curline->sidedef->GetTexture(side_t::top), true);
						}
						else if (fover->flags & FF_LOWERTEXTURE)
						{
							rw_tex = TexMan.GetPalettedTexture(curline->sidedef->GetTexture(side_t::bottom), true);
						}
						else
						{
							rw_tex = TexMan.GetPalettedTexture(fover->master->sidedef[0]->GetTexture(side_t::mid), true);
						}
						rw_pic = rw_tex && rw_tex->isValid() ? rw_tex->GetSoftwareTexture() : nullptr;
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
							swimmable_found = true;
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
				if (!rw_pic && !swimmable_found)
				{
					fover = nullptr;
					FTexture *rw_tex;
					if (rover->flags & FF_UPPERTEXTURE)
					{
						rw_tex = TexMan.GetPalettedTexture(curline->sidedef->GetTexture(side_t::top), true);
					}
					else if (rover->flags & FF_LOWERTEXTURE)
					{
						rw_tex = TexMan.GetPalettedTexture(curline->sidedef->GetTexture(side_t::bottom), true);
					}
					else
					{
						rw_tex = TexMan.GetPalettedTexture(rover->master->sidedef[0]->GetTexture(side_t::mid), true);
					}
					rw_pic = rw_tex && rw_tex->isValid() ? rw_tex->GetSoftwareTexture() : nullptr;
				}

				if (rw_pic && !swimmable_found)
				{
					Render3DFloorWall(ds, x1, x2, fover ? fover : rover, clipTop, clipBottom, rw_pic);
				}
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
				FSoftwareTexture *rw_pic = nullptr;
				bool swimmable_found = false;

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
						swimmable_found = true; // don't ever draw (but treat as something has been found)
					}
					else
					{
						FTexture *rw_tex;
						if (fover->flags & FF_UPPERTEXTURE)
						{
							rw_tex = TexMan.GetPalettedTexture(curline->sidedef->GetTexture(side_t::top), true);
						}
						else if (fover->flags & FF_LOWERTEXTURE)
						{
							rw_tex = TexMan.GetPalettedTexture(curline->sidedef->GetTexture(side_t::bottom), true);
						}
						else
						{
							rw_tex = TexMan.GetPalettedTexture(fover->master->sidedef[0]->GetTexture(side_t::mid), true);
						}
						rw_pic = rw_tex && rw_tex->isValid() ? rw_tex->GetSoftwareTexture() : nullptr;
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
							swimmable_found = true;
						}
						fover = nullptr; // visible
						break;
					}
					if (fover && (unsigned)j != frontsector->e->XFloor.ffloors.Size())
					{ // not visible
						break;
					}
				}
				if (!rw_pic && !swimmable_found)
				{
					fover = nullptr;
					FTexture *rw_tex;
					if (rover->flags & FF_UPPERTEXTURE)
					{
						rw_tex = TexMan.GetPalettedTexture(curline->sidedef->GetTexture(side_t::top), true);
					}
					else if (rover->flags & FF_LOWERTEXTURE)
					{
						rw_tex = TexMan.GetPalettedTexture(curline->sidedef->GetTexture(side_t::bottom), true);
					}
					else
					{
						rw_tex = TexMan.GetPalettedTexture(rover->master->sidedef[0]->GetTexture(side_t::mid), true);
					}
					rw_pic = rw_tex && rw_tex->isValid() ? rw_tex->GetSoftwareTexture() : nullptr;
				}

				if (rw_pic && !swimmable_found)
				{
					Render3DFloorWall(ds, x1, x2, fover ? fover : rover, clipTop, clipBottom, rw_pic);
				}
				break;
			}
		}
	}

	void RenderDrawSegment::RenderFog(DrawSegment* ds, int x1, int x2)
	{
		const short* mfloorclip = ds->drawsegclip.sprbottomclip;
		const short* mceilingclip = ds->drawsegclip.sprtopclip;

		if (m3DFloor.clipTop)
		{
			wallupper.Project(Thread->Viewport.get(), m3DFloor.sclipTop - Thread->Viewport->viewpoint.Pos.Z, &ds->WallC);
			wallupper.ClipTop(x1, x2, ds->drawsegclip);
			mceilingclip = wallupper.ScreenY;
		}

		if (m3DFloor.clipBottom)
		{
			walllower.Project(Thread->Viewport.get(), m3DFloor.sclipBottom - Thread->Viewport->viewpoint.Pos.Z, &ds->WallC);
			walllower.ClipBottom(x1, x2, ds->drawsegclip);
			mfloorclip = walllower.ScreenY;
		}

		sector_t tempsec;
		const sector_t* lightsector = Thread->OpaquePass->FakeFlat(frontsector, &tempsec, nullptr, nullptr, nullptr, 0, 0, 0, 0);

		ProjectedWallLight walllight;
		walllight.SetColormap(lightsector, curline);
		walllight.SetLightLeft(Thread, ds->WallC);

		RenderFogBoundary renderfog;
		renderfog.Render(Thread, x1, x2, mceilingclip, mfloorclip, walllight);
	}

	// Clip a midtexture to the floor and ceiling of the sector in front of it.
	void RenderDrawSegment::ClipMidtex(DrawSegment* ds, int x1, int x2)
	{
		ProjectedWallLine most;

		RenderPortal *renderportal = Thread->Portal.get();

		most.Project(Thread->Viewport.get(), curline->frontsector->ceilingplane, &ds->WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
		for (int i = x1; i < x2; ++i)
		{
			if (wallupper.ScreenY[i] < most.ScreenY[i])
				wallupper.ScreenY[i] = most.ScreenY[i];
		}
		most.Project(Thread->Viewport.get(), curline->frontsector->floorplane, &ds->WallC, curline, renderportal->MirrorFlags & RF_XFLIP);
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

	void RenderDrawSegment::GetNoWrapMidTextureZ(DrawSegment* ds, FSoftwareTexture* tex, double& ceilZ, double& floorZ)
	{
		double texheight = tex->GetScaledHeightDouble() / fabs(curline->sidedef->GetTextureYScale(side_t::mid));
		double texturemid;
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
			texturemid = MAX(frontsector->GetPlaneTexZ(sector_t::floor), backsector->GetPlaneTexZ(sector_t::floor)) + texheight;
		else
			texturemid = MIN(frontsector->GetPlaneTexZ(sector_t::ceiling), backsector->GetPlaneTexZ(sector_t::ceiling));
		double rowoffset = curline->sidedef->GetTextureYOffset(side_t::mid);
		if (tex->useWorldPanning(curline->GetLevel()))
			rowoffset /= fabs(tex->GetScale().Y * curline->sidedef->GetTextureYScale(side_t::mid));
		double textop = texturemid + rowoffset - Thread->Viewport->viewpoint.Pos.Z;

		// Unclipped vanilla Doom range for the wall. Relies on ceiling/floor clip to clamp the wall in range.
		ceilZ = textop;
		floorZ = textop - texheight;
	}
}
