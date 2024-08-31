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



#include "doomdef.h"

#include "m_bbox.h"

#include "engineerrors.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "texturemanager.h"

#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "swrenderer/plane/r_visibleplanelist.h"
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/things/r_wallsprite.h"
#include "swrenderer/things/r_voxel.h"
#include "swrenderer/things/r_particle.h"
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

double model_distance_cull = 1e16;

EXTERN_CVAR(Float, r_actorspriteshadowdist)

namespace
{
	double sprite_distance_cull = 1e16;
	double line_distance_cull = 1e16;
}

CUSTOM_CVAR(Float, r_sprite_distance_cull, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
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

CUSTOM_CVAR(Float, r_line_distance_cull, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
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

CUSTOM_CVAR(Float, r_model_distance_cull, 1024.f, 0/*CVAR_ARCHIVE | CVAR_GLOBALCONFIG*/) // Experimental for the moment until a good default is chosen 
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
			int diffTex = (s->MoreFlags & SECMF_CLIPFAKEPLANES);

			// Replace sector being drawn with a copy to be hacked
			*tempsec = *sec;

			// Replace floor and ceiling height with control sector's heights.
			if (diffTex)
			{
				if (s->floorplane.CopyPlaneIfValid(&tempsec->floorplane, &sec->ceilingplane))
				{
					tempsec->SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
				}
				else if (s->MoreFlags & SECMF_FAKEFLOORONLY)
				{
					if (underwater)
					{
						tempsec->Colormap = s->Colormap;
						if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
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

			if (!(s->MoreFlags & SECMF_FAKEFLOORONLY))
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

				if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
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
				orgceilz > refceilz && !(s->MoreFlags & SECMF_FAKEFLOORONLY))
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

				if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
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
			std::swap(ry1, ry2);
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
	void RenderOpaquePass::FakeDrawLoop(subsector_t *sub, sector_t *frontsector, VisiblePlane *floorplane, VisiblePlane *ceilingplane, Fake3DOpaque opaque3dfloor)
	{
		int 		 count;
		seg_t*		 line;

		count = sub->numlines;
		line = sub->firstline;

		while (count--)
		{
			if ((line->sidedef) && !(line->sidedef->Flags & WALLF_POLYOBJ))
			{
				renderline.Render(line, InSubsector, frontsector, nullptr, floorplane, ceilingplane, opaque3dfloor);
			}
			line++;
		}
	}

	void RenderOpaquePass::RenderSubsector(subsector_t *sub)
	{
		// Determine floor/ceiling planes.
		// Add sprites of things in sector.
		// Draw one or more line segments.

		bool outersubsector;

		if (InSubsector != nullptr)
		{ // InSubsector is not nullptr. This means we are rendering from a mini-BSP.
			outersubsector = false;
		}
		else
		{
			outersubsector = true;
			InSubsector = sub;

			// Mark the visual sorting depth of this subsector
			uint32_t subsectorDepth = (uint32_t)PvsSubsectors.size();
			SubsectorDepths[sub->Index()] = subsectorDepth;
			PvsSubsectors.push_back(sub->Index());
		}

		auto Level = sub->sector->Level;
#ifdef RANGECHECK
		if (outersubsector && (unsigned)sub->Index() >= Level->subsectors.Size())
			I_Error("RenderSubsector: ss %i with numss = %u", sub->Index(), Level->subsectors.Size());
#endif

		if (sub->polys)
		{ // Render the polyobjs in the subsector first
			AddPolyobjs(sub);
			if (outersubsector)
			{
				InSubsector = nullptr;
			}
			return;
		}

		sub->sector->MoreFlags |= SECMF_DRAWN;

		// killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
		sector_t tempsec;
		int floorlightlevel, ceilinglightlevel;
		sector_t *frontsector = FakeFlat(sub->sector, &tempsec, &floorlightlevel, &ceilinglightlevel, nullptr, 0, 0, 0, 0);

		// [RH] set foggy flag
		bool foggy = Level->fadeto || frontsector->Colormap.FadeColor || (Level->flags & LEVEL_HASFADETABLE);

		// kg3D - fake lights
		CameraLight *cameraLight = CameraLight::Instance();
		FDynamicColormap *basecolormap;
		int adjusted_ceilinglightlevel = ceilinglightlevel;
		if (cameraLight->FixedLightLevel() < 0 && frontsector->e && frontsector->e->XFloor.lightlist.Size())
		{
			lightlist_t *light = P_GetPlaneLight(frontsector, &frontsector->ceilingplane, false);
			basecolormap = GetColorTable(light->extra_colormap, frontsector->SpecialColors[sector_t::ceiling]);
			// If this is the real ceiling, don't discard plane lighting R_FakeFlat()
			// accounted for.
			if (light->p_lightlevel != &frontsector->lightlevel)
			{
				adjusted_ceilinglightlevel = *light->p_lightlevel;
			}
		}
		else
		{
			basecolormap = (r_fullbrightignoresectorcolor && cameraLight->FixedLightLevel() >= 0) ? &FullNormalLight : GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::ceiling]);
		}

		FSectorPortal *portal = frontsector->ValidatePortal(sector_t::ceiling);

		VisiblePlane *ceilingplane = nullptr;
		if (frontsector->ceilingplane.PointOnSide(Thread->Viewport->viewpoint.Pos) > 0 ||
			frontsector->GetTexture(sector_t::ceiling) == skyflatnum ||
			portal ||
			(frontsector->GetHeightSec() && frontsector->heightsec->GetTexture(sector_t::floor) == skyflatnum))
		{
			ceilingplane = Thread->PlaneList->FindPlane(
				frontsector->ceilingplane,
				frontsector->GetTexture(sector_t::ceiling),
				adjusted_ceilinglightlevel, foggy,
				frontsector->GetAlpha(sector_t::ceiling),
				!!(frontsector->GetFlags(sector_t::ceiling) & PLANEF_ADDITIVE),
				frontsector->planes[sector_t::ceiling].xform,
				frontsector->skytransfer,
				portal,
				basecolormap,
				Fake3DOpaque::Normal,
				0);

			ceilingplane->AddLights(Thread, sub->section->lighthead);
		}

		int adjusted_floorlightlevel = floorlightlevel;
		if (cameraLight->FixedLightLevel() < 0 && frontsector->e && frontsector->e->XFloor.lightlist.Size())
		{
			lightlist_t *light = P_GetPlaneLight(frontsector, &frontsector->floorplane, false);
			basecolormap = GetColorTable(light->extra_colormap, frontsector->SpecialColors[sector_t::floor]);
			// If this is the real floor, don't discard plane lighting R_FakeFlat()
			// accounted for.
			if (light->p_lightlevel != &frontsector->lightlevel)
			{
				adjusted_floorlightlevel = *light->p_lightlevel;
			}
		}
		else
		{
			basecolormap = (r_fullbrightignoresectorcolor && cameraLight->FixedLightLevel() >= 0) ? &FullNormalLight : GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::floor]);
		}

		portal = frontsector->ValidatePortal(sector_t::floor);

		VisiblePlane *floorplane = nullptr;
		if (frontsector->floorplane.PointOnSide(Thread->Viewport->viewpoint.Pos) > 0 ||
			frontsector->GetTexture(sector_t::floor) == skyflatnum ||
			portal ||
			(frontsector->GetHeightSec() && frontsector->heightsec->GetTexture(sector_t::ceiling) == skyflatnum))
		{
			floorplane = Thread->PlaneList->FindPlane(frontsector->floorplane,
				frontsector->GetTexture(sector_t::floor),
				adjusted_floorlightlevel, foggy,
				frontsector->GetAlpha(sector_t::floor),
				!!(frontsector->GetFlags(sector_t::floor) & PLANEF_ADDITIVE),
				frontsector->planes[sector_t::floor].xform,
				frontsector->skytransfer,
				portal,
				basecolormap,
				Fake3DOpaque::Normal,
				0);

			floorplane->AddLights(Thread, sub->section->lighthead);
		}

		Add3DFloorPlanes(sub, frontsector, basecolormap, foggy, adjusted_ceilinglightlevel, adjusted_floorlightlevel);
		auto nc = !!(frontsector->Level->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING);
		AddSprites(sub->sector, frontsector->GetTexture(sector_t::ceiling) == skyflatnum ? ceilinglightlevel : floorlightlevel, FakeSide, foggy, GetSpriteColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::sprites], nc));

		// [RH] Add particles
		if ((unsigned int)(sub->Index()) < Level->subsectors.Size())
		{ // Only do it for the main BSP.
			int lightlevel = (floorlightlevel + ceilinglightlevel) / 2;
			for (int i = frontsector->Level->ParticlesInSubsec[sub->Index()]; i != NO_PARTICLE; i = frontsector->Level->Particles[i].snext)
			{
				RenderParticle::Project(Thread, &frontsector->Level->Particles[i], sub->sector, lightlevel, FakeSide, foggy);
			}
		}

		DVector2 viewpointPos = Thread->Viewport->viewpoint.Pos.XY();

		seg_t *line = sub->firstline;
		int count = sub->numlines;
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
				Add3DFloorLine(line, frontsector);
				renderline.Render(line, InSubsector, frontsector, nullptr, floorplane, ceilingplane, Fake3DOpaque::Normal); // now real
			}
			line++;
		}
		if (outersubsector)
		{
			InSubsector = nullptr;
		}
	}

	void RenderOpaquePass::Add3DFloorLine(seg_t *line, sector_t *frontsector)
	{
		// kg3D - fake planes bounding calculation
		if (r_3dfloors && line->backsector && frontsector->e && line->backsector->e->XFloor.ffloors.Size())
		{
			Clip3DFloors *clip3d = Thread->Clip3D.get();
			for (unsigned int i = 0; i < line->backsector->e->XFloor.ffloors.Size(); i++)
			{
				clip3d->SetFakeFloor(line->backsector->e->XFloor.ffloors[i]);
				if (!(clip3d->fakeFloor->fakeFloor->flags & FF_EXISTS)) continue;
				if (!(clip3d->fakeFloor->fakeFloor->flags & FF_RENDERPLANES)) continue;
				if (!clip3d->fakeFloor->fakeFloor->model) continue;
				Fake3DOpaque opaque3dfloor = Fake3DOpaque::FakeBack;
				sector_t tempsec = *clip3d->fakeFloor->fakeFloor->model;
				tempsec.floorplane = *clip3d->fakeFloor->fakeFloor->top.plane;
				tempsec.ceilingplane = *clip3d->fakeFloor->fakeFloor->bottom.plane;
				if (clip3d->fakeFloor->validcount != validcount)
				{
					clip3d->fakeFloor->validcount = validcount;
					clip3d->NewClip();
				}
				renderline.Render(line, InSubsector, frontsector, &tempsec, nullptr, nullptr, opaque3dfloor); // fake
			}
			clip3d->fakeFloor = nullptr;
		}
	}

	void RenderOpaquePass::Add3DFloorPlanes(subsector_t *sub, sector_t *frontsector, FDynamicColormap *basecolormap, bool foggy, int ceilinglightlevel, int floorlightlevel)
	{
		// kg3D - fake planes rendering
		if (r_3dfloors && frontsector->e && frontsector->e->XFloor.ffloors.Size())
		{
			CameraLight *cameraLight = CameraLight::Instance();
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
				fixed_t fakeAlpha = min<fixed_t>(Scale(clip3d->fakeFloor->fakeFloor->alpha, OPAQUE, 255), OPAQUE);
				if (clip3d->fakeFloor->validcount != validcount)
				{
					clip3d->fakeFloor->validcount = validcount;
					clip3d->NewClip();
				}
				double fakeHeight = clip3d->fakeFloor->fakeFloor->top.plane->ZatPoint(frontsector->centerspot);
				if (fakeHeight < Thread->Viewport->viewpoint.Pos.Z &&
					fakeHeight > frontsector->floorplane.ZatPoint(frontsector->centerspot))
				{
					sector_t tempsec = *clip3d->fakeFloor->fakeFloor->model;
					tempsec.floorplane = *clip3d->fakeFloor->fakeFloor->top.plane;
					tempsec.ceilingplane = *clip3d->fakeFloor->fakeFloor->bottom.plane;

					int	position;
					if (!(clip3d->fakeFloor->fakeFloor->flags & FF_THISINSIDE) && !(clip3d->fakeFloor->fakeFloor->flags & FF_INVERTSECTOR))
					{
						tempsec.SetTexture(sector_t::floor, tempsec.GetTexture(sector_t::ceiling));
						position = sector_t::ceiling;
					}
					else
					{
						position = sector_t::floor;
					}

					if (cameraLight->FixedLightLevel() < 0 && sub->sector->e->XFloor.lightlist.Size())
					{
						lightlist_t *light = P_GetPlaneLight(sub->sector, &tempsec.floorplane, false);
						basecolormap = GetColorTable(light->extra_colormap);
						floorlightlevel = *light->p_lightlevel;
					}

					VisiblePlane *floorplane3d = Thread->PlaneList->FindPlane(
						tempsec.floorplane,
						tempsec.GetTexture(sector_t::floor),
						floorlightlevel, foggy,
						tempsec.GetAlpha(sector_t::floor),
						!!(clip3d->fakeFloor->fakeFloor->flags & FF_ADDITIVETRANS),
						tempsec.planes[position].xform,
						tempsec.skytransfer,
						nullptr,
						basecolormap,
						Fake3DOpaque::FakeFloor,
						fakeAlpha);

					floorplane3d->AddLights(Thread, sub->section->lighthead);

					FakeDrawLoop(sub, &tempsec, floorplane3d, nullptr, Fake3DOpaque::FakeFloor);
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
				fixed_t fakeAlpha = min<fixed_t>(Scale(clip3d->fakeFloor->fakeFloor->alpha, OPAQUE, 255), OPAQUE);

				if (clip3d->fakeFloor->validcount != validcount)
				{
					clip3d->fakeFloor->validcount = validcount;
					clip3d->NewClip();
				}
				double fakeHeight = clip3d->fakeFloor->fakeFloor->bottom.plane->ZatPoint(frontsector->centerspot);
				if (fakeHeight > Thread->Viewport->viewpoint.Pos.Z &&
					fakeHeight < frontsector->ceilingplane.ZatPoint(frontsector->centerspot))
				{
					sector_t tempsec = *clip3d->fakeFloor->fakeFloor->model;
					tempsec.floorplane = *clip3d->fakeFloor->fakeFloor->top.plane;
					tempsec.ceilingplane = *clip3d->fakeFloor->fakeFloor->bottom.plane;

					int	position;
					if ((!(clip3d->fakeFloor->fakeFloor->flags & FF_THISINSIDE) && !(clip3d->fakeFloor->fakeFloor->flags & FF_INVERTSECTOR)) ||
						(clip3d->fakeFloor->fakeFloor->flags & FF_THISINSIDE && clip3d->fakeFloor->fakeFloor->flags & FF_INVERTSECTOR))
					{
						tempsec.SetTexture(sector_t::ceiling, tempsec.GetTexture(sector_t::floor));
						position = sector_t::floor;
					}
					else
					{
						position = sector_t::ceiling;
					}

					tempsec.ceilingplane.ChangeHeight(-1 / 65536.);
					if (cameraLight->FixedLightLevel() < 0 && sub->sector->e->XFloor.lightlist.Size())
					{
						lightlist_t *light = P_GetPlaneLight(sub->sector, &tempsec.ceilingplane, false);
						basecolormap = GetColorTable(light->extra_colormap);
						ceilinglightlevel = *light->p_lightlevel;
					}
					tempsec.ceilingplane.ChangeHeight(1 / 65536.);

					VisiblePlane *ceilingplane3d = Thread->PlaneList->FindPlane(
						tempsec.ceilingplane,
						tempsec.GetTexture(sector_t::ceiling),
						ceilinglightlevel, foggy,
						tempsec.GetAlpha(sector_t::ceiling),
						!!(clip3d->fakeFloor->fakeFloor->flags & FF_ADDITIVETRANS),
						tempsec.planes[position].xform,
						tempsec.skytransfer,
						nullptr,
						basecolormap,
						Fake3DOpaque::FakeCeiling,
						fakeAlpha);

					ceilingplane3d->AddLights(Thread, sub->section->lighthead);

					FakeDrawLoop(sub, &tempsec, nullptr, ceilingplane3d, Fake3DOpaque::FakeCeiling);
				}
			}
			clip3d->fakeFloor = nullptr;
		}
	}

	void RenderOpaquePass::RenderScene(FLevelLocals *Level)
	{
		if (Thread->MainThread)
			WallCycles.Clock();

		for (uint32_t sub : PvsSubsectors)
			SubsectorDepths[sub] = 0xffffffff;
		SubsectorDepths.resize(Level->subsectors.Size(), 0xffffffff);

		PvsSubsectors.clear();
		SeenSpriteSectors.clear();
		SeenActors.clear();

		InSubsector = nullptr;
		RenderBSPNode(Level->HeadNode());	// The head node is the last node output.

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
		if (Thread->Viewport->Level()->nodes.Size() == 0)
		{
			RenderSubsector(&Thread->Viewport->Level()->subsectors[0]);
			return;
		}
		while (!((size_t)node & 1))  // Keep going until found a subsector
		{
			node_t *bsp = (node_t *)node;

			// Decide which side the view point is on.
			int side = R_PointOnSide(Thread->Viewport->viewpoint.Pos.XY(), bsp);

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
		fillshort(floorclip, viewwidth, viewheight);
		fillshort(ceilingclip, viewwidth, 0);
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
				int spritenum = thing->sprite;
				bool isPicnumOverride = thing->picnum.isValid();
				if (GetThingSprite(thing, sprite))
				{
					FDynamicColormap *thingColormap = basecolormap;
					int thinglightlevel = lightlevel;
					if (sec->sectornum != thing->Sector->sectornum)	// compare sectornums to account for R_FakeFlat copies.
					{
						thinglightlevel = thing->Sector->GetTexture(sector_t::ceiling) == skyflatnum ? thing->Sector->GetCeilingLight() : thing->Sector->GetFloorLight();
						auto nc = !!(thing->Level->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING);
						thingColormap = GetSpriteColorTable(thing->Sector->Colormap, thing->Sector->SpecialColors[sector_t::sprites], nc);					
					}
					if (thing->LightLevel > -1)
					{
						thinglightlevel = thing->LightLevel;

						if (thing->flags8 & MF8_ADDLIGHTLEVEL)
						{
							thinglightlevel += thing->Sector->GetTexture(sector_t::ceiling) == skyflatnum ? thing->Sector->GetCeilingLight() : thing->Sector->GetFloorLight();
							thinglightlevel = clamp(thinglightlevel, 0, 255);
						}
					}
					if ((sprite.renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
					{
						RenderWallSprite::Project(Thread, thing, sprite.pos, sprite.tex, sprite.spriteScale, sprite.renderflags, thinglightlevel, foggy, thingColormap);
					}
					else if (sprite.voxel)
					{
						RenderVoxel::Project(Thread, thing, sprite.pos, sprite.voxel, sprite.spriteScale, sprite.renderflags, fakeside, fakefloor, fakeceiling, sec, thinglightlevel, foggy, thingColormap);
					}
					else
					{
						RenderSprite::Project(Thread, thing, sprite.pos, sprite.tex, sprite.spriteScale, sprite.renderflags, fakeside, fakefloor, fakeceiling, sec, thinglightlevel, foggy, thingColormap);

						// [Nash] draw sprite shadow
						if (R_ShouldDrawSpriteShadow(thing))
						{
							double dist = (thing->Pos() - Thread->Viewport->viewpoint.Pos).LengthSquared();
							double distCheck = r_actorspriteshadowdist;
							if (dist <= distCheck * distCheck)
							{
								// squash Y scale
								DVector2 shadowScale = sprite.spriteScale;
								shadowScale.Y *= 0.15;

								// snap to floor Z
								DVector3 shadowPos = sprite.pos;
								shadowPos.Z = thing->floorz;

								RenderSprite::Project(Thread, thing, shadowPos, sprite.tex, shadowScale, sprite.renderflags, fakeside, fakefloor, fakeceiling, sec, thinglightlevel, foggy, thingColormap, true);
							}
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
			(thing->renderflags & RF_MAYBEINVISIBLE) ||
			!thing->RenderStyle.IsVisible(thing->Alpha) ||
			!thing->IsVisibleToPlayer() ||
			!thing->IsInsideVisibleAngles())
		{
			return false;
		}

		if ((thing->flags8 & MF8_MASTERNOSEE) && thing->master != nullptr)
		{
			// Make MASTERNOSEE actors invisible if their master
			// is invisible due to viewpoint shenanigans.
			if (thing->master->renderflags & RF_MAYBEINVISIBLE)
			{
				return false;
			}
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

		// [Nash] filter visibility in mirrors
		bool isInMirror = renderportal != nullptr && renderportal->IsInMirrorRecursively;
		if (thing->renderflags2 & RF2_INVISIBLEINMIRRORS && isInMirror)
		{
			return false;
		}
		else if (thing->renderflags2 & RF2_ONLYVISIBLEINMIRRORS && !isInMirror)
		{
			return false;
		}

		double distanceSquared = (thing->Pos() - Thread->Viewport->viewpoint.Pos).LengthSquared();
		if (distanceSquared > sprite_distance_cull)
			return false;

		return true;
	}

	bool RenderOpaquePass::GetThingSprite(AActor *thing, ThingSprite &sprite)
	{
		// The X offsetting (SpriteOffset.X) is performed in r_sprite.cpp, in RenderSprite::Project().
		sprite.pos = thing->InterpolatedPosition(Thread->Viewport->viewpoint.TicFrac);
		sprite.pos += thing->WorldOffset;
		sprite.pos.Z += thing->GetBobOffset(Thread->Viewport->viewpoint.TicFrac) + thing->GetSpriteOffset(true);
		sprite.spritenum = thing->sprite;
		sprite.tex = nullptr;
		sprite.voxel = nullptr;
		sprite.spriteScale = DVector2(thing->Scale.X, thing->Scale.Y);
		sprite.renderflags = thing->renderflags;

		if (thing->player != nullptr)
		{
			P_CheckPlayerSprite(thing, sprite.spritenum, sprite.spriteScale);
		}

		if (thing->picnum.isValid())
		{
			sprite.picnum = thing->picnum;

			sprite.tex = GetPalettedSWTexture(sprite.picnum, true);
			if (!sprite.tex) return false;

			if (sprite.tex->GetRotations() != 0xFFFF)
			{
				// choose a different rotation based on player view
				spriteframe_t *sprframe = &SpriteFrames[sprite.tex->GetRotations()];
				DAngle ang = (sprite.pos - Thread->Viewport->viewpoint.Pos).Angle();
				angle_t rot;
				if (sprframe->Texture[0] == sprframe->Texture[1])
				{
					if (thing->flags7 & MF7_SPRITEANGLE)
						rot = (thing->SpriteAngle + DAngle::fromDeg(45.0 / 2 * 9)).BAMs() >> 28;
					else
						rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + DAngle::fromDeg(45.0 / 2 * 9)).BAMs() >> 28;
				}
				else
				{
					if (thing->flags7 & MF7_SPRITEANGLE)
						rot = (thing->SpriteAngle + DAngle::fromDeg(45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
					else
						rot = (ang - (thing->Angles.Yaw + thing->SpriteRotation) + DAngle::fromDeg(45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
				}
				sprite.picnum = sprframe->Texture[rot];
				if (sprframe->Flip & (1 << rot))
				{
					sprite.renderflags ^= RF_XFLIP;
				}
				sprite.tex = GetPalettedSWTexture(sprite.picnum, false);	// Do not animate the rotation
				if (!sprite.tex) return false;
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
					sprite.tex = GetPalettedSWTexture(tex, false);	// Do not animate the rotation
				}

				if (r_drawvoxels)
				{
					sprite.voxel = SpriteFrames[sprdef->spriteframes + thing->frame].Voxel;
				}

				if (sprite.voxel == nullptr && !tex.isValid())
					return false;
			}

			if (sprite.voxel == nullptr && sprite.tex == nullptr)
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
