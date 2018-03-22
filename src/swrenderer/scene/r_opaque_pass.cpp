//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2006-2016 Christoph Oelckers
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
// DESCRIPTION:
//		BSP traversal, handling of LineSegs for rendering.
//
//-----------------------------------------------------------------------------


#include <stdlib.h>

#include "templates.h"

#include "doomdef.h"

#include "m_bbox.h"

#include "i_system.h"
#include "p_lnspec.h"
#include "p_setup.h"

#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/things/r_wallsprite.h"
#include "swrenderer/things/r_voxel.h"
#include "swrenderer/things/r_particle.h"
#include "swrenderer/things/r_model.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_farclip_line.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/r_renderthread.h"
#include "r_3dfloors.h"
#include "r_portal.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "c_console.h"
#include "p_maputl.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#include "r_opaque_pass.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "g_levellocals.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);
EXTERN_CVAR(Bool, r_drawvoxels);
EXTERN_CVAR(Bool, r_debug_disable_vis_filter);
extern uint32_t r_renderercaps;

namespace
{
	double sprite_distance_cull = 1e16;
	double line_distance_cull = 1e16;
	double model_distance_cull = 1e16;
}

CUSTOM_CVAR(Float, r_sprite_distance_cull, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (r_sprite_distance_cull > 0.0)
	{
		sprite_distance_cull = r_sprite_distance_cull * r_sprite_distance_cull;
	}
	else
	{
		sprite_distance_cull = 1e16;
	}
}

CUSTOM_CVAR(Float, r_line_distance_cull, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (r_line_distance_cull > 0.0)
	{
		line_distance_cull = r_line_distance_cull * r_line_distance_cull;
	}
	else
	{
		line_distance_cull = 1e16;
	}
}

CUSTOM_CVAR(Float, r_model_distance_cull, 1024, 0/*CVAR_ARCHIVE | CVAR_GLOBALCONFIG*/) // Experimental for the moment until a good default is chosen 
{
	if (r_model_distance_cull > 0.0)
	{
		model_distance_cull = r_model_distance_cull * r_model_distance_cull;
	}
	else
	{
		model_distance_cull = 1e16;
	}
}

namespace swrenderer
{
	RenderOpaquePass::RenderOpaquePass(RenderThread *thread) : renderline(thread)
	{
		Thread = thread;
	}

	sector_t *RenderOpaquePass::FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, seg_t *backline, int backx1, int backx2, double frontcz1, double frontcz2)
	{
		// If player's view height is underneath fake floor, lower the
		// drawn ceiling to be just under the floor height, and replace
		// the drawn floor and ceiling textures, and light level, with
		// the control sector's.
		//
		// Similar for ceiling, only reflected.

		// [RH] allow per-plane lighting
		if (floorlightlevel != nullptr)
		{
			*floorlightlevel = sec->GetFloorLight();
		}

		if (ceilinglightlevel != nullptr)
		{
			*ceilinglightlevel = sec->GetCeilingLight();
		}

		FakeSide = WaterFakeSide::Center;

		const sector_t *s = sec->GetHeightSec();
		if (s != nullptr)
		{
			sector_t *heightsec = Thread->Viewport->viewpoint.sector->heightsec;
			bool underwater = r_fakingunderwater ||
				(heightsec && heightsec->floorplane.PointOnSide(Thread->Viewport->viewpoint.Pos) <= 0);
			bool doorunderwater = false;
			int diffTex = (s->MoreFlags & SECF_CLIPFAKEPLANES);

			// Replace sector being drawn with a copy to be hacked
			*tempsec = *sec;

			// Replace floor and ceiling height with control sector's heights.
			if (diffTex)
			{
				if (s->floorplane.CopyPlaneIfValid(&tempsec->floorplane, &sec->ceilingplane))
				{
					tempsec->SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
				}
				else if (s->MoreFlags & SECF_FAKEFLOORONLY)
				{
					if (underwater)
					{
						tempsec->Colormap = s->Colormap;
						if (!(s->MoreFlags & SECF_NOFAKELIGHT))
						{
							tempsec->lightlevel = s->lightlevel;

							if (floorlightlevel != nullptr)
							{
								*floorlightlevel = s->GetFloorLight();
							}

							if (ceilinglightlevel != nullptr)
							{
								*ceilinglightlevel = s->GetCeilingLight();
							}
						}
						FakeSide = WaterFakeSide::BelowFloor;
						return tempsec;
					}
					return sec;
				}
			}
			else
			{
				tempsec->floorplane = s->floorplane;
			}

			if (!(s->MoreFlags & SECF_FAKEFLOORONLY))
			{
				if (diffTex)
				{
					if (s->ceilingplane.CopyPlaneIfValid(&tempsec->ceilingplane, &sec->floorplane))
					{
						tempsec->SetTexture(sector_t::ceiling, s->GetTexture(sector_t::ceiling), false);
					}
				}
				else
				{
					tempsec->ceilingplane = s->ceilingplane;
				}
			}

			double refceilz = s->ceilingplane.ZatPoint(Thread->Viewport->viewpoint.Pos);
			double orgceilz = sec->ceilingplane.ZatPoint(Thread->Viewport->viewpoint.Pos);

#if 1
			// [RH] Allow viewing underwater areas through doors/windows that
			// are underwater but not in a water sector themselves.
			// Only works if you cannot see the top surface of any deep water
			// sectors at the same time.
			if (backline && !r_fakingunderwater && backline->frontsector->heightsec == nullptr)
			{
				if (frontcz1 <= s->floorplane.ZatPoint(backline->v1) &&
					frontcz2 <= s->floorplane.ZatPoint(backline->v2))
				{
					// Check that the window is actually visible
					for (int z = backx1; z < backx2; ++z)
					{
						if (floorclip[z] > ceilingclip[z])
						{
							doorunderwater = true;
							r_fakingunderwater = true;
							break;
						}
					}
				}
			}
#endif

			if (underwater || doorunderwater)
			{
				tempsec->floorplane = sec->floorplane;
				tempsec->ceilingplane = s->floorplane;
				tempsec->ceilingplane.FlipVert();
				tempsec->ceilingplane.ChangeHeight(-1 / 65536.);
				tempsec->Colormap = s->Colormap;
			}

			// killough 11/98: prevent sudden light changes from non-water sectors:
			if ((underwater && !backline) || doorunderwater)
			{					// head-below-floor hack
				tempsec->SetTexture(sector_t::floor, diffTex ? sec->GetTexture(sector_t::floor) : s->GetTexture(sector_t::floor), false);
				tempsec->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;

				tempsec->ceilingplane = s->floorplane;
				tempsec->ceilingplane.FlipVert();
				tempsec->ceilingplane.ChangeHeight(-1 / 65536.);
				if (s->GetTexture(sector_t::ceiling) == skyflatnum)
				{
					tempsec->floorplane = tempsec->ceilingplane;
					tempsec->floorplane.FlipVert();
					tempsec->floorplane.ChangeHeight(+1 / 65536.);
					tempsec->SetTexture(sector_t::ceiling, tempsec->GetTexture(sector_t::floor), false);
					tempsec->planes[sector_t::ceiling].xform = tempsec->planes[sector_t::floor].xform;
				}
				else
				{
					tempsec->SetTexture(sector_t::ceiling, diffTex ? s->GetTexture(sector_t::floor) : s->GetTexture(sector_t::ceiling), false);
					tempsec->planes[sector_t::ceiling].xform = s->planes[sector_t::ceiling].xform;
				}

				if (!(s->MoreFlags & SECF_NOFAKELIGHT))
				{
					tempsec->lightlevel = s->lightlevel;

					if (floorlightlevel != nullptr)
					{
						*floorlightlevel = s->GetFloorLight();
					}

					if (ceilinglightlevel != nullptr)
					{
						*ceilinglightlevel = s->GetCeilingLight();
					}
				}
				FakeSide = WaterFakeSide::BelowFloor;
			}
			else if (heightsec && heightsec->ceilingplane.PointOnSide(Thread->Viewport->viewpoint.Pos) <= 0 &&
				orgceilz > refceilz && !(s->MoreFlags & SECF_FAKEFLOORONLY))
			{	// Above-ceiling hack
				tempsec->ceilingplane = s->ceilingplane;
				tempsec->floorplane = s->ceilingplane;
				tempsec->floorplane.FlipVert();
				tempsec->floorplane.ChangeHeight(+1 / 65536.);
				tempsec->Colormap = s->Colormap;

				tempsec->SetTexture(sector_t::ceiling, diffTex ? sec->GetTexture(sector_t::ceiling) : s->GetTexture(sector_t::ceiling), false);
				tempsec->SetTexture(sector_t::floor, s->GetTexture(sector_t::ceiling), false);
				tempsec->planes[sector_t::ceiling].xform = tempsec->planes[sector_t::floor].xform = s->planes[sector_t::ceiling].xform;

				if (s->GetTexture(sector_t::floor) != skyflatnum)
				{
					tempsec->ceilingplane = sec->ceilingplane;
					tempsec->SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
					tempsec->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;
				}

				if (!(s->MoreFlags & SECF_NOFAKELIGHT))
				{
					tempsec->lightlevel = s->lightlevel;

					if (floorlightlevel != nullptr)
					{
						*floorlightlevel = s->GetFloorLight();
					}

					if (ceilinglightlevel != nullptr)
					{
						*ceilinglightlevel = s->GetCeilingLight();
					}
				}
				FakeSide = WaterFakeSide::AboveCeiling;
			}
			sec = tempsec;					// Use other sector
		}
		return sec;
	}



	// Checks BSP node/subtree bounding box.
	// Returns true if some part of the bbox might be visible.
	bool RenderOpaquePass::CheckBBox(float *bspcoord)
	{
		static const int checkcoord[12][4] =
		{
			{ 3,0,2,1 },
			{ 3,0,2,0 },
			{ 3,1,2,0 },
			{ 0 },
			{ 2,0,2,1 },
			{ 0,0,0,0 },
			{ 3,1,3,0 },
			{ 0 },
			{ 2,0,3,1 },
			{ 2,1,3,1 },
			{ 2,1,3,0 }
		};

		int 				boxx;
		int 				boxy;
		int 				boxpos;

		double	 			x1, y1, x2, y2;
		double				rx1, ry1, rx2, ry2;
		int					sx1, sx2;

		// Find the corners of the box
		// that define the edges from current viewpoint.
		if (Thread->Viewport->viewpoint.Pos.X <= bspcoord[BOXLEFT])
			boxx = 0;
		else if (Thread->Viewport->viewpoint.Pos.X < bspcoord[BOXRIGHT])
			boxx = 1;
		else
			boxx = 2;

		if (Thread->Viewport->viewpoint.Pos.Y >= bspcoord[BOXTOP])
			boxy = 0;
		else if (Thread->Viewport->viewpoint.Pos.Y > bspcoord[BOXBOTTOM])
			boxy = 1;
		else
			boxy = 2;

		boxpos = (boxy << 2) + boxx;
		if (boxpos == 5)
			return true;

		x1 = bspcoord[checkcoord[boxpos][0]] - Thread->Viewport->viewpoint.Pos.X;
		y1 = bspcoord[checkcoord[boxpos][1]] - Thread->Viewport->viewpoint.Pos.Y;
		x2 = bspcoord[checkcoord[boxpos][2]] - Thread->Viewport->viewpoint.Pos.X;
		y2 = bspcoord[checkcoord[boxpos][3]] - Thread->Viewport->viewpoint.Pos.Y;

		// check clip list for an open space

		// Sitting on a line?
		if (y1 * (x1 - x2) + x1 * (y2 - y1) >= -EQUAL_EPSILON)
			return true;

		rx1 = x1 * Thread->Viewport->viewpoint.Sin - y1 * Thread->Viewport->viewpoint.Cos;
		rx2 = x2 * Thread->Viewport->viewpoint.Sin - y2 * Thread->Viewport->viewpoint.Cos;
		ry1 = x1 * Thread->Viewport->viewpoint.TanCos + y1 * Thread->Viewport->viewpoint.TanSin;
		ry2 = x2 * Thread->Viewport->viewpoint.TanCos + y2 * Thread->Viewport->viewpoint.TanSin;

		if (Thread->Portal->MirrorFlags & RF_XFLIP)
		{
			double t = -rx1;
			rx1 = -rx2;
			rx2 = t;
			swapvalues(ry1, ry2);
		}
		
		auto viewport = Thread->Viewport.get();

		if (rx1 >= -ry1)
		{
			if (rx1 > ry1) return false;	// left edge is off the right side
			if (ry1 == 0) return false;
			sx1 = xs_RoundToInt(viewport->CenterX + rx1 * viewport->CenterX / ry1);
		}
		else
		{
			if (rx2 < -ry2) return false;	// wall is off the left side
			if (rx1 - rx2 - ry2 + ry1 == 0) return false;	// wall does not intersect view volume
			sx1 = 0;
		}

		if (rx2 <= ry2)
		{
			if (rx2 < -ry2) return false;	// right edge is off the left side
			if (ry2 == 0) return false;
			sx2 = xs_RoundToInt(viewport->CenterX + rx2 * viewport->CenterX / ry2);
		}
		else
		{
			if (rx1 > ry1) return false;	// wall is off the right side
			if (ry2 - ry1 - rx2 + rx1 == 0) return false;	// wall does not intersect view volume
			sx2 = viewwidth;
		}

		// Find the first clippost that touches the source post
		//	(adjacent pixels are touching).

		return Thread->ClipSegments->IsVisible(sx1, sx2);
	}

	void RenderOpaquePass::AddPolyobjs(subsector_t *sub)
	{
		Thread->PreparePolyObject(sub);

		if (sub->BSP->Nodes.Size() == 0)
		{
			RenderSubsector(&sub->BSP->Subsectors[0]);
		}
		else
		{
			RenderBSPNode(&sub->BSP->Nodes.Last());
		}
	}

	// kg3D - add fake segs, never rendered
	void RenderOpaquePass::FakeDrawLoop(subsector_t *sub, VisiblePlane *floorplane, VisiblePlane *ceilingplane, bool foggy, FDynamicColormap *basecolormap, Fake3DOpaque opaque3dfloor)
	{
		int 		 count;
		seg_t*		 line;

		count = sub->numlines;
		line = sub->firstline;

		while (count--)
		{
			if ((line->sidedef) && !(line->sidedef->Flags & WALLF_POLYOBJ))
			{
				renderline.Render(line, InSubsector, frontsector, nullptr, floorplane, ceilingplane, foggy, basecolormap, opaque3dfloor);
			}
			line++;
		}
	}

	void RenderOpaquePass::RenderSubsector(subsector_t *sub)
	{
		// Determine floor/ceiling planes.
		// Add sprites of things in sector.
		// Draw one or more line segments.

		int 		 count;
		seg_t*		 line;
		sector_t     tempsec;				// killough 3/7/98: deep water hack
		int          floorlightlevel;		// killough 3/16/98: set floor lightlevel
		int          ceilinglightlevel;		// killough 4/11/98
		bool		 outersubsector;
		int	fll, cll, position;
		FSectorPortal *portal;

		// kg3D - fake floor stuff
		VisiblePlane *backupfp;
		VisiblePlane *backupcp;
		//secplane_t templane;
		lightlist_t *light;

		if (InSubsector != nullptr)
		{ // InSubsector is not nullptr. This means we are rendering from a mini-BSP.
			outersubsector = false;
		}
		else
		{
			outersubsector = true;
			InSubsector = sub;
		}

#ifdef RANGECHECK
		if (outersubsector && (unsigned)sub->Index() >= level.subsectors.Size())
			I_Error("RenderSubsector: ss %i with numss = %u", sub->Index(), level.subsectors.Size());
#endif

		assert(sub->sector != nullptr);

		if (sub->polys)
		{ // Render the polyobjs in the subsector first
			AddPolyobjs(sub);
			if (outersubsector)
			{
				InSubsector = nullptr;
			}
			return;
		}

		frontsector = sub->sector;
		frontsector->MoreFlags |= SECF_DRAWN;
		count = sub->numlines;
		line = sub->firstline;

		// killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
		frontsector = FakeFlat(frontsector, &tempsec, &floorlightlevel, &ceilinglightlevel, nullptr, 0, 0, 0, 0);

		fll = floorlightlevel;
		cll = ceilinglightlevel;

		// [RH] set foggy flag
		bool foggy = level.fadeto || frontsector->Colormap.FadeColor || (level.flags & LEVEL_HASFADETABLE);

		// kg3D - fake lights
		CameraLight *cameraLight = CameraLight::Instance();
		FDynamicColormap *basecolormap;
		if (cameraLight->FixedLightLevel() < 0 && frontsector->e && frontsector->e->XFloor.lightlist.Size())
		{
			light = P_GetPlaneLight(frontsector, &frontsector->ceilingplane, false);
			basecolormap = GetColorTable(light->extra_colormap, frontsector->SpecialColors[sector_t::ceiling]);
			// If this is the real ceiling, don't discard plane lighting R_FakeFlat()
			// accounted for.
			if (light->p_lightlevel != &frontsector->lightlevel)
			{
				ceilinglightlevel = *light->p_lightlevel;
			}
		}
		else
		{
			basecolormap = (r_fullbrightignoresectorcolor && cameraLight->FixedLightLevel() >= 0) ? &FullNormalLight : GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::ceiling]);
		}

		portal = frontsector->ValidatePortal(sector_t::ceiling);

		VisiblePlane *ceilingplane = frontsector->ceilingplane.PointOnSide(Thread->Viewport->viewpoint.Pos) > 0 ||
			frontsector->GetTexture(sector_t::ceiling) == skyflatnum ||
			portal != nullptr ||
			(frontsector->heightsec &&
				!(frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
				frontsector->heightsec->GetTexture(sector_t::floor) == skyflatnum) ?
			Thread->PlaneList->FindPlane(frontsector->ceilingplane,		// killough 3/8/98
				frontsector->GetTexture(sector_t::ceiling),
				ceilinglightlevel + LightVisibility::ActualExtraLight(foggy, Thread->Viewport.get()),				// killough 4/11/98
				frontsector->GetAlpha(sector_t::ceiling),
				!!(frontsector->GetFlags(sector_t::ceiling) & PLANEF_ADDITIVE),
				frontsector->planes[sector_t::ceiling].xform,
				frontsector->sky,
				portal,
				basecolormap,
				Fake3DOpaque::Normal,
				0
			) : nullptr;

		if (ceilingplane)
			ceilingplane->AddLights(Thread, frontsector->lighthead);

		if (cameraLight->FixedLightLevel() < 0 && frontsector->e && frontsector->e->XFloor.lightlist.Size())
		{
			light = P_GetPlaneLight(frontsector, &frontsector->floorplane, false);
			basecolormap = GetColorTable(light->extra_colormap, frontsector->SpecialColors[sector_t::floor]);
			// If this is the real floor, don't discard plane lighting R_FakeFlat()
			// accounted for.
			if (light->p_lightlevel != &frontsector->lightlevel)
			{
				floorlightlevel = *light->p_lightlevel;
			}
		}
		else
		{
			basecolormap = (r_fullbrightignoresectorcolor && cameraLight->FixedLightLevel() >= 0) ? &FullNormalLight : GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::floor]);
		}

		// killough 3/7/98: Add (x,y) offsets to flats, add deep water check
		// killough 3/16/98: add floorlightlevel
		// killough 10/98: add support for skies transferred from sidedefs
		portal = frontsector->ValidatePortal(sector_t::floor);

		VisiblePlane *floorplane = frontsector->floorplane.PointOnSide(Thread->Viewport->viewpoint.Pos) > 0 || // killough 3/7/98
			frontsector->GetTexture(sector_t::floor) == skyflatnum ||
			portal != nullptr ||
			(frontsector->heightsec &&
				!(frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
				frontsector->heightsec->GetTexture(sector_t::ceiling) == skyflatnum) ?
			Thread->PlaneList->FindPlane(frontsector->floorplane,
				frontsector->GetTexture(sector_t::floor),
				floorlightlevel + LightVisibility::ActualExtraLight(foggy, Thread->Viewport.get()),				// killough 3/16/98
				frontsector->GetAlpha(sector_t::floor),
				!!(frontsector->GetFlags(sector_t::floor) & PLANEF_ADDITIVE),
				frontsector->planes[sector_t::floor].xform,
				frontsector->sky,
				portal,
				basecolormap,
				Fake3DOpaque::Normal,
				0
			) : nullptr;

		if (floorplane)
			floorplane->AddLights(Thread, frontsector->lighthead);

		// kg3D - fake planes rendering
		if (r_3dfloors && frontsector->e && frontsector->e->XFloor.ffloors.Size())
		{
			backupfp = floorplane;
			backupcp = ceilingplane;

			Clip3DFloors *clip3d = Thread->Clip3D.get();

			// first check all floors
			for (int i = 0; i < (int)frontsector->e->XFloor.ffloors.Size(); i++)
			{
				clip3d->SetFakeFloor(frontsector->e->XFloor.ffloors[i]);
				if (!(clip3d->fakeFloor->fakeFloor->flags & FF_EXISTS)) continue;
				if (!clip3d->fakeFloor->fakeFloor->model) continue;
				if (clip3d->fakeFloor->fakeFloor->bottom.plane->isSlope()) continue;
				if (!(clip3d->fakeFloor->fakeFloor->flags & FF_NOSHADE) || (clip3d->fakeFloor->fakeFloor->flags & (FF_RENDERPLANES | FF_RENDERSIDES)))
				{
					clip3d->AddHeight(clip3d->fakeFloor->fakeFloor->top.plane, frontsector);
				}
				if (!(clip3d->fakeFloor->fakeFloor->flags & FF_RENDERPLANES)) continue;
				if (clip3d->fakeFloor->fakeFloor->alpha == 0) continue;
				if (clip3d->fakeFloor->fakeFloor->flags & FF_THISINSIDE && clip3d->fakeFloor->fakeFloor->flags & FF_INVERTSECTOR) continue;
				fixed_t fakeAlpha = MIN<fixed_t>(Scale(clip3d->fakeFloor->fakeFloor->alpha, OPAQUE, 255), OPAQUE);
				if (clip3d->fakeFloor->validcount != validcount)
				{
					clip3d->fakeFloor->validcount = validcount;
					clip3d->NewClip();
				}
				double fakeHeight = clip3d->fakeFloor->fakeFloor->top.plane->ZatPoint(frontsector->centerspot);
				if (fakeHeight < Thread->Viewport->viewpoint.Pos.Z &&
					fakeHeight > frontsector->floorplane.ZatPoint(frontsector->centerspot))
				{
					tempsec = *clip3d->fakeFloor->fakeFloor->model;
					tempsec.floorplane = *clip3d->fakeFloor->fakeFloor->top.plane;
					tempsec.ceilingplane = *clip3d->fakeFloor->fakeFloor->bottom.plane;
					if (!(clip3d->fakeFloor->fakeFloor->flags & FF_THISINSIDE) && !(clip3d->fakeFloor->fakeFloor->flags & FF_INVERTSECTOR))
					{
						tempsec.SetTexture(sector_t::floor, tempsec.GetTexture(sector_t::ceiling));
						position = sector_t::ceiling;
					}
					else position = sector_t::floor;
					frontsector = &tempsec;

					if (cameraLight->FixedLightLevel() < 0 && sub->sector->e->XFloor.lightlist.Size())
					{
						light = P_GetPlaneLight(sub->sector, &frontsector->floorplane, false);
						basecolormap = GetColorTable(light->extra_colormap);
						floorlightlevel = *light->p_lightlevel;
					}

					ceilingplane = nullptr;
					floorplane = Thread->PlaneList->FindPlane(frontsector->floorplane,
						frontsector->GetTexture(sector_t::floor),
						floorlightlevel + LightVisibility::ActualExtraLight(foggy, Thread->Viewport.get()),				// killough 3/16/98
						frontsector->GetAlpha(sector_t::floor),
						!!(clip3d->fakeFloor->fakeFloor->flags & FF_ADDITIVETRANS),
						frontsector->planes[position].xform,
						frontsector->sky,
						nullptr,
						basecolormap,
						Fake3DOpaque::FakeFloor,
						fakeAlpha);

					if (floorplane)
						floorplane->AddLights(Thread, frontsector->lighthead);

					FakeDrawLoop(sub, floorplane, ceilingplane, foggy, basecolormap, Fake3DOpaque::FakeFloor);
					frontsector = sub->sector;
				}
			}
			// and now ceilings
			for (unsigned int i = 0; i < frontsector->e->XFloor.ffloors.Size(); i++)
			{
				clip3d->SetFakeFloor(frontsector->e->XFloor.ffloors[i]);
				if (!(clip3d->fakeFloor->fakeFloor->flags & FF_EXISTS)) continue;
				if (!clip3d->fakeFloor->fakeFloor->model) continue;
				if (clip3d->fakeFloor->fakeFloor->top.plane->isSlope()) continue;
				if (!(clip3d->fakeFloor->fakeFloor->flags & FF_NOSHADE) || (clip3d->fakeFloor->fakeFloor->flags & (FF_RENDERPLANES | FF_RENDERSIDES)))
				{
					clip3d->AddHeight(clip3d->fakeFloor->fakeFloor->bottom.plane, frontsector);
				}
				if (!(clip3d->fakeFloor->fakeFloor->flags & FF_RENDERPLANES)) continue;
				if (clip3d->fakeFloor->fakeFloor->alpha == 0) continue;
				if (!(clip3d->fakeFloor->fakeFloor->flags & FF_THISINSIDE) && (clip3d->fakeFloor->fakeFloor->flags & (FF_SWIMMABLE | FF_INVERTSECTOR)) == (FF_SWIMMABLE | FF_INVERTSECTOR)) continue;
				fixed_t fakeAlpha = MIN<fixed_t>(Scale(clip3d->fakeFloor->fakeFloor->alpha, OPAQUE, 255), OPAQUE);

				if (clip3d->fakeFloor->validcount != validcount)
				{
					clip3d->fakeFloor->validcount = validcount;
					clip3d->NewClip();
				}
				double fakeHeight = clip3d->fakeFloor->fakeFloor->bottom.plane->ZatPoint(frontsector->centerspot);
				if (fakeHeight > Thread->Viewport->viewpoint.Pos.Z &&
					fakeHeight < frontsector->ceilingplane.ZatPoint(frontsector->centerspot))
				{
					tempsec = *clip3d->fakeFloor->fakeFloor->model;
					tempsec.floorplane = *clip3d->fakeFloor->fakeFloor->top.plane;
					tempsec.ceilingplane = *clip3d->fakeFloor->fakeFloor->bottom.plane;
					if ((!(clip3d->fakeFloor->fakeFloor->flags & FF_THISINSIDE) && !(clip3d->fakeFloor->fakeFloor->flags & FF_INVERTSECTOR)) ||
						(clip3d->fakeFloor->fakeFloor->flags & FF_THISINSIDE && clip3d->fakeFloor->fakeFloor->flags & FF_INVERTSECTOR))
					{
						tempsec.SetTexture(sector_t::ceiling, tempsec.GetTexture(sector_t::floor));
						position = sector_t::floor;
					}
					else position = sector_t::ceiling;
					frontsector = &tempsec;

					tempsec.ceilingplane.ChangeHeight(-1 / 65536.);
					if (cameraLight->FixedLightLevel() < 0 && sub->sector->e->XFloor.lightlist.Size())
					{
						light = P_GetPlaneLight(sub->sector, &frontsector->ceilingplane, false);
						basecolormap = GetColorTable(light->extra_colormap);
						ceilinglightlevel = *light->p_lightlevel;
					}
					tempsec.ceilingplane.ChangeHeight(1 / 65536.);

					floorplane = nullptr;
					ceilingplane = Thread->PlaneList->FindPlane(frontsector->ceilingplane,		// killough 3/8/98
						frontsector->GetTexture(sector_t::ceiling),
						ceilinglightlevel + LightVisibility::ActualExtraLight(foggy, Thread->Viewport.get()),				// killough 4/11/98
						frontsector->GetAlpha(sector_t::ceiling),
						!!(clip3d->fakeFloor->fakeFloor->flags & FF_ADDITIVETRANS),
						frontsector->planes[position].xform,
						frontsector->sky,
						nullptr,
						basecolormap,
						Fake3DOpaque::FakeCeiling,
						fakeAlpha);

					if (ceilingplane)
						ceilingplane->AddLights(Thread, frontsector->lighthead);

					FakeDrawLoop(sub, floorplane, ceilingplane, foggy, basecolormap, Fake3DOpaque::FakeCeiling);
					frontsector = sub->sector;
				}
			}
			clip3d->fakeFloor = nullptr;
			floorplane = backupfp;
			ceilingplane = backupcp;
		}

		basecolormap = GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::sprites], true);
		floorlightlevel = fll;
		ceilinglightlevel = cll;

		// killough 9/18/98: Fix underwater slowdown, by passing real sector 
		// instead of fake one. Improve sprite lighting by basing sprite
		// lightlevels on floor & ceiling lightlevels in the surrounding area.
		// [RH] Handle sprite lighting like Duke 3D: If the ceiling is a sky, sprites are lit by
		// it, otherwise they are lit by the floor.
		AddSprites(sub->sector, frontsector->GetTexture(sector_t::ceiling) == skyflatnum ? ceilinglightlevel : floorlightlevel, FakeSide, foggy, basecolormap);

		// [RH] Add particles
		if ((unsigned int)(sub->Index()) < level.subsectors.Size())
		{ // Only do it for the main BSP.
			int shade = LightVisibility::LightLevelToShade((floorlightlevel + ceilinglightlevel) / 2 + LightVisibility::ActualExtraLight(foggy, Thread->Viewport.get()), foggy);
			for (int i = ParticlesInSubsec[sub->Index()]; i != NO_PARTICLE; i = Particles[i].snext)
			{
				RenderParticle::Project(Thread, Particles + i, sub->sector, shade, FakeSide, foggy);
			}
		}

		count = sub->numlines;
		line = sub->firstline;

		DVector2 viewpointPos = Thread->Viewport->viewpoint.Pos.XY();

		basecolormap = GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::walltop]);
		while (count--)
		{
			double dist1 = (line->v1->fPos() - viewpointPos).LengthSquared();
			double dist2 = (line->v2->fPos() - viewpointPos).LengthSquared();
			if (dist1 > line_distance_cull && dist2 > line_distance_cull)
			{
				FarClipLine farclip(Thread);
				farclip.Render(line, InSubsector, floorplane, ceilingplane, Fake3DOpaque::Normal);
			}
			else if (!outersubsector || line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ))
			{
				// kg3D - fake planes bounding calculation
				if (r_3dfloors && line->backsector && frontsector->e && line->backsector->e->XFloor.ffloors.Size())
				{
					backupfp = floorplane;
					backupcp = ceilingplane;
					floorplane = nullptr;
					ceilingplane = nullptr;
					Clip3DFloors *clip3d = Thread->Clip3D.get();
					for (unsigned int i = 0; i < line->backsector->e->XFloor.ffloors.Size(); i++)
					{
						clip3d->SetFakeFloor(line->backsector->e->XFloor.ffloors[i]);
						if (!(clip3d->fakeFloor->fakeFloor->flags & FF_EXISTS)) continue;
						if (!(clip3d->fakeFloor->fakeFloor->flags & FF_RENDERPLANES)) continue;
						if (!clip3d->fakeFloor->fakeFloor->model) continue;
						Fake3DOpaque opaque3dfloor = Fake3DOpaque::FakeBack;
						tempsec = *clip3d->fakeFloor->fakeFloor->model;
						tempsec.floorplane = *clip3d->fakeFloor->fakeFloor->top.plane;
						tempsec.ceilingplane = *clip3d->fakeFloor->fakeFloor->bottom.plane;
						if (clip3d->fakeFloor->validcount != validcount)
						{
							clip3d->fakeFloor->validcount = validcount;
							clip3d->NewClip();
						}
						renderline.Render(line, InSubsector, frontsector, &tempsec, floorplane, ceilingplane, foggy, basecolormap, opaque3dfloor); // fake
					}
					clip3d->fakeFloor = nullptr;
					floorplane = backupfp;
					ceilingplane = backupcp;
				}
				renderline.Render(line, InSubsector, frontsector, nullptr, floorplane, ceilingplane, foggy, basecolormap, Fake3DOpaque::Normal); // now real
			}
			line++;
		}
		if (outersubsector)
		{
			InSubsector = nullptr;
		}
	}

	void RenderOpaquePass::RenderScene()
	{
		if (Thread->MainThread)
			WallCycles.Clock();

		SeenSpriteSectors.clear();
		SeenActors.clear();

		InSubsector = nullptr;
		RenderBSPNode(level.HeadNode());	// The head node is the last node output.

		if (Thread->MainThread)
			WallCycles.Unclock();
	}

	//
	// RenderBSPNode
	// Renders all subsectors below a given node, traversing subtree recursively.
	// Just call with BSP root and -1.
	// killough 5/2/98: reformatted, removed tail recursion

	void RenderOpaquePass::RenderBSPNode(void *node)
	{
		if (level.nodes.Size() == 0)
		{
			RenderSubsector(&level.subsectors[0]);
			return;
		}
		while (!((size_t)node & 1))  // Keep going until found a subsector
		{
			node_t *bsp = (node_t *)node;

			// Decide which side the view point is on.
			int side = R_PointOnSide(Thread->Viewport->viewpoint.Pos, bsp);

			// Recursively divide front space (toward the viewer).
			RenderBSPNode(bsp->children[side]);

			// Possibly divide back space (away from the viewer).
			side ^= 1;
			if (!CheckBBox(bsp->bbox[side]))
				return;

			node = bsp->children[side];
		}
		RenderSubsector((subsector_t *)((uint8_t *)node - 1));
	}

	void RenderOpaquePass::ClearClip()
	{
		// clip ceiling to console bottom
		fillshort(floorclip, viewwidth, viewheight);
		fillshort(ceilingclip, viewwidth, !screen->Accel2D && ConBottom > viewwindowy && !Thread->Viewport->RenderingToCanvas() ? (ConBottom - viewwindowy) : 0);
	}

	void RenderOpaquePass::AddSprites(sector_t *sec, int lightlevel, WaterFakeSide fakeside, bool foggy, FDynamicColormap *basecolormap)
	{
		// BSP is traversed by subsector.
		// A sector might have been split into several
		//	subsectors during BSP building.
		// Thus we check whether it was already added.
		if (sec->touching_renderthings == nullptr || SeenSpriteSectors.find(sec) != SeenSpriteSectors.end()/*|| sec->validcount == validcount*/)
			return;

		// Well, now it will be done.
		//sec->validcount = validcount;
		SeenSpriteSectors.insert(sec);

		int spriteshade = LightVisibility::LightLevelToShade(lightlevel + LightVisibility::ActualExtraLight(foggy, Thread->Viewport.get()), foggy);

		// Handle all things in sector.
		for (auto p = sec->touching_renderthings; p != nullptr; p = p->m_snext)
		{
			auto thing = p->m_thing;
			if (SeenActors.find(thing) != SeenActors.end()) continue;
			SeenActors.insert(thing);
			//if (thing->validcount == validcount) continue;
			//thing->validcount = validcount;

			FIntCVar *cvar = thing->GetInfo()->distancecheck;
			if (cvar != nullptr && *cvar >= 0)
			{
				double dist = (thing->Pos() - Thread->Viewport->viewpoint.Pos).LengthSquared();
				double check = (double)**cvar;
				if (dist >= check * check)
				{
					continue;
				}
			}

			// find fake level
			F3DFloor *fakeceiling = nullptr;
			F3DFloor *fakefloor = nullptr;
			for (auto rover : thing->Sector->e->XFloor.ffloors)
			{
				if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES)) continue;
				if (!(rover->flags & FF_SOLID) || rover->alpha != 255) continue;
				if (!fakefloor)
				{
					if (!rover->top.plane->isSlope())
					{
						if (rover->top.plane->ZatPoint(0., 0.) <= thing->Z()) fakefloor = rover;
					}
				}
				if (!rover->bottom.plane->isSlope())
				{
					if (rover->bottom.plane->ZatPoint(0., 0.) >= thing->Top()) fakeceiling = rover;
				}
			}

			if (IsPotentiallyVisible(thing))
			{
				ThingSprite sprite;
				if (GetThingSprite(thing, sprite))
				{
					FDynamicColormap *thingColormap = basecolormap;
					int thingShade = spriteshade;
					if (sec->sectornum != thing->Sector->sectornum)	// compare sectornums to account for R_FakeFlat copies.
					{
						int lightlevel = thing->Sector->GetTexture(sector_t::ceiling) == skyflatnum ? thing->Sector->GetCeilingLight() : thing->Sector->GetFloorLight();
						thingShade = LightVisibility::LightLevelToShade(lightlevel + LightVisibility::ActualExtraLight(foggy, Thread->Viewport.get()), foggy);
						thingColormap = GetColorTable(thing->Sector->Colormap, thing->Sector->SpecialColors[sector_t::sprites], true);
					}

					if ((sprite.renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
					{
						RenderWallSprite::Project(Thread, thing, sprite.pos, sprite.tex, sprite.spriteScale, sprite.renderflags, thingShade, foggy, thingColormap);
					}
					else if (sprite.voxel)
					{
						RenderVoxel::Project(Thread, thing, sprite.pos, sprite.voxel, sprite.spriteScale, sprite.renderflags, fakeside, fakefloor, fakeceiling, sec, thingShade, foggy, thingColormap);
					}
					else if (!r_models)
					{
						RenderSprite::Project(Thread, thing, sprite.pos, sprite.tex, sprite.spriteScale, sprite.renderflags, fakeside, fakefloor, fakeceiling, sec, thingShade, foggy, thingColormap);
					}
					else
					{
						int spritenum = thing->sprite;
						bool isPicnumOverride = thing->picnum.isValid();
						FSpriteModelFrame *modelframe = isPicnumOverride ? nullptr : gl_FindModelFrame(thing->GetClass(), spritenum, thing->frame, !!(thing->flags & MF_DROPPED));
						if (modelframe && (thing->Pos() - Thread->Viewport->viewpoint.Pos).LengthSquared() < model_distance_cull)
						{
							DVector3 pos = thing->InterpolatedPosition(Thread->Viewport->viewpoint.TicFrac);
							RenderModel::Project(Thread, (float)pos.X, (float)pos.Y, (float)pos.Z, modelframe, thing);
						}
						else
						{
							RenderSprite::Project(Thread, thing, sprite.pos, sprite.tex, sprite.spriteScale, sprite.renderflags, fakeside, fakefloor, fakeceiling, sec, thingShade, foggy, thingColormap);
						}
					}
				}
			}
		}
	}

	bool RenderOpaquePass::IsPotentiallyVisible(AActor *thing)
	{
		// Don't waste time projecting sprites that are definitely not visible.
		if (thing == nullptr ||
			(thing->renderflags & RF_INVISIBLE) ||
			!thing->RenderStyle.IsVisible(thing->Alpha) ||
			!thing->IsVisibleToPlayer() ||
			!thing->IsInsideVisibleAngles())
		{
			return false;
		}

		// check renderrequired vs ~r_rendercaps, if anything matches we don't support that feature,
		// check renderhidden vs r_rendercaps, if anything matches we do support that feature and should hide it.
		if ((!r_debug_disable_vis_filter && !!(thing->RenderRequired & ~r_renderercaps)) ||
			(!!(thing->RenderHidden & r_renderercaps)))
			return false;

		// [ZZ] Or less definitely not visible (hue)
		// [ZZ] 10.01.2016: don't try to clip stuff inside a skybox against the current portal.
		RenderPortal *renderportal = Thread->Portal.get();
		if (!renderportal->CurrentPortalInSkybox && renderportal->CurrentPortal && !!P_PointOnLineSidePrecise(thing->Pos(), renderportal->CurrentPortal->dst))
			return false;

		double distanceSquared = (thing->Pos() - Thread->Viewport->viewpoint.Pos).LengthSquared();
		if (distanceSquared > sprite_distance_cull)
			return false;

		return true;
	}

	bool RenderOpaquePass::GetThingSprite(AActor *thing, ThingSprite &sprite)
	{
		sprite.pos = thing->InterpolatedPosition(Thread->Viewport->viewpoint.TicFrac);
		sprite.pos.Z += thing->GetBobOffset(Thread->Viewport->viewpoint.TicFrac);

		sprite.spritenum = thing->sprite;
		sprite.tex = nullptr;
		sprite.voxel = nullptr;
		sprite.spriteScale = thing->Scale;
		sprite.renderflags = thing->renderflags;

		if (thing->player != nullptr)
		{
			P_CheckPlayerSprite(thing, sprite.spritenum, sprite.spriteScale);
		}

		if (thing->picnum.isValid())
		{
			sprite.picnum = thing->picnum;

			sprite.tex = TexMan(sprite.picnum);
			if (sprite.tex->UseType == FTexture::TEX_Null)
			{
				return false;
			}

			if (sprite.tex->Rotations != 0xFFFF)
			{
				// choose a different rotation based on player view
				spriteframe_t *sprframe = &SpriteFrames[sprite.tex->Rotations];
				DAngle ang = (sprite.pos - Thread->Viewport->viewpoint.Pos).Angle();
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
				sprite.picnum = sprframe->Texture[rot];
				if (sprframe->Flip & (1 << rot))
				{
					sprite.renderflags ^= RF_XFLIP;
				}
				sprite.tex = TexMan[sprite.picnum];	// Do not animate the rotation
			}
		}
		else
		{
			// decide which texture to use for the sprite
			if ((unsigned)sprite.spritenum >= sprites.Size())
			{
				DPrintf(DMSG_ERROR, "R_ProjectSprite: invalid sprite number %u\n", sprite.spritenum);
				return false;
			}
			spritedef_t *sprdef = &sprites[sprite.spritenum];
			if (thing->frame >= sprdef->numframes)
			{
				// If there are no frames at all for this sprite, don't draw it.
				return false;
			}
			else
			{
				auto &viewpoint = Thread->Viewport->viewpoint;
				DAngle sprangle = thing->GetSpriteAngle((sprite.pos - viewpoint.Pos).Angle(), viewpoint.TicFrac);
				bool flipX;

				FTextureID tex = sprdef->GetSpriteFrame(thing->frame, -1, sprangle, &flipX, !!(thing->renderflags & RF_SPRITEFLIP));
				if (tex.isValid())
				{
					if (flipX)
					{
						sprite.renderflags ^= RF_XFLIP;
					}
					sprite.tex = TexMan[tex];	// Do not animate the rotation
				}

				if (r_drawvoxels)
				{
					sprite.voxel = SpriteFrames[sprdef->spriteframes + thing->frame].Voxel;
				}

				if (sprite.voxel == nullptr && !tex.isValid())
					return false;
			}

			if (sprite.voxel == nullptr && (sprite.tex == nullptr || sprite.tex->UseType == FTexture::TEX_Null))
			{
				return false;
			}

			if (sprite.spriteScale.Y < 0)
			{
				sprite.spriteScale.Y = -sprite.spriteScale.Y;
				sprite.renderflags ^= RF_YFLIP;
			}
			if (sprite.spriteScale.X < 0)
			{
				sprite.spriteScale.X = -sprite.spriteScale.X;
				sprite.renderflags ^= RF_XFLIP;
			}
		}

		return true;
	}
}
