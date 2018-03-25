// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_bsp.cpp
** Main rendering loop / BSP traversal / visibility clipping
**
**/

#include "p_lnspec.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "g_levellocals.h"
#include "r_sky.h"
#include "p_effect.h"
#include "po_man.h"
#include "doomdata.h"
#include "g_levellocals.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/scene/gl_portal.h"
#include "gl/scene/gl_wall.h"
#include "gl/utility/gl_clock.h"

EXTERN_CVAR(Bool, gl_render_segs)

CVAR(Bool, gl_render_things, true, 0)
CVAR(Bool, gl_render_walls, true, 0)
CVAR(Bool, gl_render_flats, true, 0)

void GLSceneDrawer::UnclipSubsector(subsector_t *sub)
{
	int count = sub->numlines;
	seg_t * seg = sub->firstline;

	while (count--)
	{
		angle_t startAngle = clipper.GetClipAngle(seg->v2);
		angle_t endAngle = clipper.GetClipAngle(seg->v1);

		// Back side, i.e. backface culling	- read: endAngle >= startAngle!
		if (startAngle-endAngle >= ANGLE_180)  
		{
			clipper.SafeRemoveClipRange(startAngle, endAngle);
			clipper.SetBlocked(false);
		}
		seg++;
	}
}

//==========================================================================
//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//
//==========================================================================

void GLSceneDrawer::AddLine (seg_t *seg, bool portalclip)
{
#ifdef _DEBUG
	if (seg->linedef->Index() == 38)
	{
		int a = 0;
	}
#endif

	sector_t * backsector = NULL;
	sector_t bs;

	if (portalclip)
	{
		int clipres = GLRenderer->mClipPortal->ClipSeg(seg);
		if (clipres == GLPortal::PClip_InFront) return;
	}

	angle_t startAngle = clipper.GetClipAngle(seg->v2);
	angle_t endAngle = clipper.GetClipAngle(seg->v1);

	// Back side, i.e. backface culling	- read: endAngle >= startAngle!
	if (startAngle-endAngle<ANGLE_180)  
	{
		return;
	}

	if (seg->sidedef == NULL)
	{
		if (!(currentsubsector->flags & SSECF_DRAWN))
		{
			if (clipper.SafeCheckRange(startAngle, endAngle)) 
			{
				currentsubsector->flags |= SSECF_DRAWN;
			}
		}
		return;
	}

	if (!clipper.SafeCheckRange(startAngle, endAngle)) 
	{
		return;
	}
	currentsubsector->flags |= SSECF_DRAWN;

	uint8_t ispoly = uint8_t(seg->sidedef->Flags & WALLF_POLYOBJ);

	if (!seg->backsector)
	{
		clipper.SafeAddClipRange(startAngle, endAngle);
	}
	else if (!ispoly)	// Two-sided polyobjects never obstruct the view
	{
		if (currentsector->sectornum == seg->backsector->sectornum)
		{
			if (!seg->linedef->isVisualPortal())
			{
				FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::mid));
				if (!tex || tex->UseType==ETextureType::Null) 
				{
					// nothing to do here!
					seg->linedef->validcount=validcount;
					return;
				}
			}
			backsector=currentsector;
		}
		else
		{
			// clipping checks are only needed when the backsector is not the same as the front sector
			CheckViewArea(seg->v1, seg->v2, seg->frontsector, seg->backsector);

			backsector = gl_FakeFlat(seg->backsector, &bs, in_area, true);

			if (gl_CheckClip(seg->sidedef, currentsector, backsector))
			{
				clipper.SafeAddClipRange(startAngle, endAngle);
			}
		}
	}
	else 
	{
		// Backsector for polyobj segs is always the containing sector itself
		backsector = currentsector;
	}

	seg->linedef->flags |= ML_MAPPED;

	if (ispoly || seg->linedef->validcount!=validcount) 
	{
		if (!ispoly) seg->linedef->validcount=validcount;

		if (gl_render_walls)
		{
			SetupWall.Clock();

			GLWall wall(this);
			wall.sub = currentsubsector;
			wall.Process(seg, currentsector, backsector);
			rendered_lines++;

			SetupWall.Unclock();
		}
	}
}

//==========================================================================
//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
//==========================================================================

void GLSceneDrawer::PolySubsector(subsector_t * sub)
{
	int count = sub->numlines;
	seg_t * line = sub->firstline;

	while (count--)
	{
		if (line->linedef)
		{
			AddLine (line, GLRenderer->mClipPortal != NULL);
		}
		line++;
	}
}

//==========================================================================
//
// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
//
//==========================================================================

void GLSceneDrawer::RenderPolyBSPNode (void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = R_PointOnSide(viewx, viewy, bsp);

		// Recursively divide front space (toward the viewer).
		RenderPolyBSPNode (bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;

		// It is not necessary to use the slower precise version here
		if (!clipper.CheckBox(bsp->bbox[side]))
		{
			return;
		}

		node = bsp->children[side];
	}
	PolySubsector ((subsector_t *)((uint8_t *)node - 1));
}

//==========================================================================
//
// Unlilke the software renderer this function will only draw the walls,
// not the flats. Those are handled as a whole by the parent subsector.
//
//==========================================================================

void GLSceneDrawer::AddPolyobjs(subsector_t *sub)
{
	if (sub->BSP == NULL || sub->BSP->bDirty)
	{
		sub->BuildPolyBSP();
		for (unsigned i = 0; i < sub->BSP->Segs.Size(); i++)
		{
			sub->BSP->Segs[i].Subsector = sub;
			sub->BSP->Segs[i].PartnerSeg = NULL;
		}
	}
	if (sub->BSP->Nodes.Size() == 0)
	{
		PolySubsector(&sub->BSP->Subsectors[0]);
	}
	else
	{
		RenderPolyBSPNode(&sub->BSP->Nodes.Last());
	}
}


//==========================================================================
//
//
//
//==========================================================================

void GLSceneDrawer::AddLines(subsector_t * sub, sector_t * sector)
{
	currentsector = sector;
	currentsubsector = sub;

	ClipWall.Clock();
	if (sub->polys != NULL)
	{
		AddPolyobjs(sub);
	}
	else
	{
		int count = sub->numlines;
		seg_t * seg = sub->firstline;

		while (count--)
		{
			if (seg->linedef == NULL)
			{
				if (!(sub->flags & SSECF_DRAWN)) AddLine (seg, GLRenderer->mClipPortal != NULL);
			}
			else if (!(seg->sidedef->Flags & WALLF_POLYOBJ)) 
			{
				AddLine (seg, GLRenderer->mClipPortal != NULL);
			}
			seg++;
		}
	}
	ClipWall.Unclock();
}

//==========================================================================
//
// Adds lines that lie directly on the portal boundary.
// Only two-sided lines will be handled here, and no polyobjects
//
//==========================================================================

inline bool PointOnLine(const DVector2 &pos, const line_t *line)
{
	double v = (pos.Y - line->v1->fY()) * line->Delta().X + (line->v1->fX() - pos.X) * line->Delta().Y;
	return fabs(v) <= EQUAL_EPSILON;
}

void GLSceneDrawer::AddSpecialPortalLines(subsector_t * sub, sector_t * sector, line_t *line)
{
	currentsector = sector;
	currentsubsector = sub;

	ClipWall.Clock();
	int count = sub->numlines;
	seg_t * seg = sub->firstline;

	while (count--)
	{
		if (seg->linedef != NULL && seg->PartnerSeg != NULL)
		{
			if (PointOnLine(seg->v1->fPos(), line) && PointOnLine(seg->v2->fPos(), line))
				AddLine(seg, false);
		}
		seg++;
	}
	ClipWall.Unclock();
}


//==========================================================================
//
// R_RenderThings
//
//==========================================================================

void GLSceneDrawer::RenderThings(subsector_t * sub, sector_t * sector)
{
	SetupSprite.Clock();
	sector_t * sec=sub->sector;
	// Handle all things in sector.
	for (auto p = sec->touching_renderthings; p != nullptr; p = p->m_snext)
	{
		auto thing = p->m_thing;
		if (thing->validcount == validcount) continue;
		thing->validcount = validcount;

		FIntCVar *cvar = thing->GetInfo()->distancecheck;
		if (cvar != NULL && *cvar >= 0)
		{
			double dist = (thing->Pos() - r_viewpoint.Pos).LengthSquared();
			double check = (double)**cvar;
			if (dist >= check * check)
			{
				continue;
			}
		}

		GLSprite sprite(this);
		sprite.Process(thing, sector, false);
	}
	
	for (msecnode_t *node = sec->sectorportal_thinglist; node; node = node->m_snext)
	{
		AActor *thing = node->m_thing;
		FIntCVar *cvar = thing->GetInfo()->distancecheck;
		if (cvar != NULL && *cvar >= 0)
		{
			double dist = (thing->Pos() - r_viewpoint.Pos).LengthSquared();
			double check = (double)**cvar;
			if (dist >= check * check)
			{
				continue;
			}
		}

		GLSprite sprite(this);
		sprite.Process(thing, sector, true);
	}
	SetupSprite.Unclock();
}


//==========================================================================
//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
//==========================================================================

void GLSceneDrawer::DoSubsector(subsector_t * sub)
{
	unsigned int i;
	sector_t * sector;
	sector_t * fakesector;
	sector_t fake;
	
#ifdef _DEBUG
	if (sub->sector->sectornum==931)
	{
		int a = 0;
	}
#endif

	sector=sub->sector;
	if (!sector) return;

	// If the mapsections differ this subsector can't possibly be visible from the current view point
	if (!(currentmapsection[sub->mapsection>>3] & (1 << (sub->mapsection & 7)))) return;
	if (sub->flags & SSECF_POLYORG) return;	// never render polyobject origin subsectors because their vertices no longer are where one may expect.

	if (gl_drawinfo->ss_renderflags[sub->Index()] & SSRF_SEEN)
	{
		// This means that we have reached a subsector in a portal that has been marked 'seen'
		// from the other side of the portal. This means we must clear the clipper for the
		// range this subsector spans before going on.
		UnclipSubsector(sub);
	}
	if (clipper.IsBlocked()) return;	// if we are inside a stacked sector portal which hasn't unclipped anything yet.

	fakesector=gl_FakeFlat(sector, &fake, in_area, false);

	if (GLRenderer->mClipPortal)
	{
		int clipres = GLRenderer->mClipPortal->ClipSubsector(sub);
		if (clipres == GLPortal::PClip_InFront)
		{
			line_t *line = GLRenderer->mClipPortal->ClipLine();
			// The subsector is out of range, but we still have to check lines that lie directly on the boundary and may expose their upper or lower parts.
			if (line) AddSpecialPortalLines(sub, fakesector, line);
			return;
		}
	}

	if (sector->validcount != validcount)
	{
		GLRenderer->mVBO->CheckUpdate(sector);
	}

	// [RH] Add particles
	//int shade = LIGHT2SHADE((floorlightlevel + ceilinglightlevel)/2 + r_actualextralight);
	if (gl_render_things)
	{
		SetupSprite.Clock();

		for (i = ParticlesInSubsec[sub->Index()]; i != NO_PARTICLE; i = Particles[i].snext)
		{
			GLSprite sprite(this);
			sprite.ProcessParticle(&Particles[i], fakesector);
		}
		SetupSprite.Unclock();
	}

	AddLines(sub, fakesector);

	// BSP is traversed by subsector.
	// A sector might have been split into several
	//	subsectors during BSP building.
	// Thus we check whether it was already added.
	if (sector->validcount != validcount)
	{
		// Well, now it will be done.
		sector->validcount = validcount;

		if (gl_render_things)
		{
			RenderThings(sub, fakesector);
		}
		sector->MoreFlags |= SECF_DRAWN;
	}

	if (gl_render_flats)
	{
		// Subsectors with only 2 lines cannot have any area
		if (sub->numlines>2 || (sub->hacked&1)) 
		{
			// Exclude the case when it tries to render a sector with a heightsec
			// but undetermined heightsec state. This can only happen if the
			// subsector is obstructed but not excluded due to a large bounding box.
			// Due to the way a BSP works such a subsector can never be visible
			if (!sector->heightsec || sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC || in_area!=area_default)
			{
				if (sector != sub->render_sector)
				{
					sector = sub->render_sector;
					// the planes of this subsector are faked to belong to another sector
					// This means we need the heightsec parts and light info of the render sector, not the actual one.
					fakesector = gl_FakeFlat(sector, &fake, in_area, false);
				}

				uint8_t &srf = gl_drawinfo->sectorrenderflags[sub->render_sector->sectornum];
				if (!(srf & SSRF_PROCESSED))
				{
					srf |= SSRF_PROCESSED;

					SetupFlat.Clock();
					GLFlat flat(this);
					flat.ProcessSector(fakesector);
					SetupFlat.Unclock();
				}
				// mark subsector as processed - but mark for rendering only if it has an actual area.
				gl_drawinfo->ss_renderflags[sub->Index()] = 
					(sub->numlines > 2) ? SSRF_PROCESSED|SSRF_RENDERALL : SSRF_PROCESSED;
				if (sub->hacked & 1) gl_drawinfo->AddHackedSubsector(sub);

				FPortal *portal;

				portal = fakesector->GetGLPortal(sector_t::ceiling);
				if (portal != NULL)
				{
					GLSectorStackPortal *glportal = portal->GetRenderState();
					glportal->AddSubsector(sub);
				}

				portal = fakesector->GetGLPortal(sector_t::floor);
				if (portal != NULL)
				{
					GLSectorStackPortal *glportal = portal->GetRenderState();
					glportal->AddSubsector(sub);
				}
			}
		}
	}
}




//==========================================================================
//
// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
//
//==========================================================================

void GLSceneDrawer::RenderBSPNode (void *node)
{
	if (level.nodes.Size() == 0)
	{
		DoSubsector (&level.subsectors[0]);
		return;
	}
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = R_PointOnSide(viewx, viewy, bsp);

		// Recursively divide front space (toward the viewer).
		RenderBSPNode (bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;

		// It is not necessary to use the slower precise version here
		if (!clipper.CheckBox(bsp->bbox[side]))
		{
			if (!(gl_drawinfo->no_renderflags[bsp->Index()] & SSRF_SEEN))
				return;
		}

		node = bsp->children[side];
	}
	DoSubsector ((subsector_t *)((uint8_t *)node - 1));
}


