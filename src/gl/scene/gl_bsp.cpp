/*
** gl_bsp.cpp
** Main rendering loop / BSP traversal / visibility clipping
**
**---------------------------------------------------------------------------
** Copyright 2000-2005 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "p_lnspec.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "r_sky.h"
#include "p_effect.h"
#include "po_man.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_clipper.h"
#include "gl/scene/gl_portal.h"
#include "gl/scene/gl_wall.h"
#include "gl/utility/gl_clock.h"

EXTERN_CVAR(Bool, gl_render_segs)

Clipper clipper;


CVAR(Bool, gl_render_things, true, 0)
CVAR(Bool, gl_render_walls, true, 0)
CVAR(Bool, gl_render_flats, true, 0)


static void UnclipSubsector(subsector_t *sub)
{
	int count = sub->numlines;
	seg_t * seg = sub->firstline;

	while (count--)
	{
		angle_t startAngle = seg->v2->GetClipAngle();
		angle_t endAngle = seg->v1->GetClipAngle();

		// Back side, i.e. backface culling	- read: endAngle >= startAngle!
		if (startAngle-endAngle >= ANGLE_180)  
		{
			clipper.SafeRemoveClipRange(startAngle, endAngle);
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

// making these 2 variables global instead of passing them as function parameters is faster.
static subsector_t *currentsubsector;
static sector_t *currentsector;

static void AddLine (seg_t *seg)
{
#ifdef _DEBUG
	if (seg->linedef - lines == 38)
	{
		int a = 0;
	}
#endif

	angle_t startAngle, endAngle;
	sector_t * backsector = NULL;
	sector_t bs;

	if (GLRenderer->mCurrentPortal)
	{
		int clipres = GLRenderer->mCurrentPortal->ClipSeg(seg);
		if (clipres == GLPortal::PClip_InFront) return;
	}

	startAngle = seg->v2->GetClipAngle();
	endAngle = seg->v1->GetClipAngle();

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

	if (!seg->backsector)
	{
		clipper.SafeAddClipRange(startAngle, endAngle);
	}
	else if (!(seg->sidedef->Flags & WALLF_POLYOBJ))	// Two-sided polyobjects never obstruct the view
	{
		if (currentsector->sectornum == seg->backsector->sectornum)
		{
			FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::mid));
			if (!tex || tex->UseType==FTexture::TEX_Null) 
			{
				// nothing to do here!
				seg->linedef->validcount=validcount;
				return;
			}
			backsector=currentsector;
		}
		else
		{
			// clipping checks are only needed when the backsector is not the same as the front sector
			gl_CheckViewArea(seg->v1, seg->v2, seg->frontsector, seg->backsector);

			backsector = gl_FakeFlat(seg->backsector, &bs, true);

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

	if ((seg->sidedef->Flags & WALLF_POLYOBJ) || seg->linedef->validcount!=validcount) 
	{
		if (!(seg->sidedef->Flags & WALLF_POLYOBJ)) seg->linedef->validcount=validcount;

		if (gl_render_walls)
		{
			SetupWall.Clock();

			//if (!gl_multithreading)
			{
				GLWall wall;
				wall.sub = currentsubsector;
				wall.Process(seg, currentsector, backsector);
			}
			/*
			else
			{
				FJob *job = new FGLJobProcessWall(currentsubsector, seg, 
					currentsector->sectornum, backsector != NULL? backsector->sectornum : -1);
				GLRenderer->mThreadManager->AddJob(job);
			}
			*/
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

static void PolySubsector(subsector_t * sub)
{
	int count = sub->numlines;
	seg_t * line = sub->firstline;

	while (count--)
	{
		if (line->linedef)
		{
			AddLine (line);
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

static void RenderPolyBSPNode (void *node)
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
	PolySubsector ((subsector_t *)((BYTE *)node - 1));
}

//==========================================================================
//
// Unlilke the software renderer this function will only draw the walls,
// not the flats. Those are handled as a whole by the parent subsector.
//
//==========================================================================

static void AddPolyobjs(subsector_t *sub)
{
	if (sub->BSP == NULL || sub->BSP->bDirty)
	{
		sub->BuildPolyBSP();
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

static inline void AddLines(subsector_t * sub, sector_t * sector)
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
				if (!(sub->flags & SSECF_DRAWN)) AddLine (seg);
			}
			else if (!(seg->sidedef->Flags & WALLF_POLYOBJ)) 
			{
				AddLine (seg);
			}
			seg++;
		}
	}
	ClipWall.Unclock();
}


//==========================================================================
//
// R_RenderThings
//
//==========================================================================

static inline void RenderThings(subsector_t * sub, sector_t * sector)
{

	SetupSprite.Clock();
	sector_t * sec=sub->sector;
	if (sec->thinglist != NULL)
	{
		//if (!gl_multithreading)
		{
			// Handle all things in sector.
			for (AActor * thing = sec->thinglist; thing; thing = thing->snext)
			{
				GLRenderer->ProcessSprite(thing, sector);
			}
		}
		/*
		else if (sec->thinglist != NULL)
		{
			FJob *job = new FGLJobProcessSprites(sector);
			GLRenderer->mThreadManager->AddJob(job);
		}
		*/
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

static void DoSubsector(subsector_t * sub)
{
	unsigned int i;
	sector_t * sector;
	sector_t * fakesector;
	sector_t fake;
	
	// check for visibility of this entire subsector. This requires GL nodes.
	// (disabled because it costs more time than it saves.)
	//if (!clipper.CheckBox(sub->bbox)) return;


#ifdef _DEBUG
	if (sub->sector-sectors==931)
	{
		int a = 0;
	}
#endif

	sector=sub->sector;
	if (!sector) return;

	// If the mapsections differ this subsector can't possibly be visible from the current view point
	if (!(currentmapsection[sub->mapsection>>3] & (1 << (sub->mapsection & 7)))) return;

	if (gl_drawinfo->ss_renderflags[sub-subsectors] & SSRF_SEEN)
	{
		// This means that we have reached a subsector in a portal that has been marked 'seen'
		// from the other side of the portal. This means we must clear the clipper for the
		// range this subsector spans before going on.
		UnclipSubsector(sub);
	}

	fakesector=gl_FakeFlat(sector, &fake, false);

	if (sector->validcount != validcount)
	{
		GLRenderer->mVBO->CheckUpdate(sector);
	}

	// [RH] Add particles
	//int shade = LIGHT2SHADE((floorlightlevel + ceilinglightlevel)/2 + r_actualextralight);
	if (gl_render_things)
	{
		SetupSprite.Clock();

		//if (!gl_multithreading)
		{
			for (i = ParticlesInSubsec[DWORD(sub-subsectors)]; i != NO_PARTICLE; i = Particles[i].snext)
			{
				GLRenderer->ProcessParticle(&Particles[i], fakesector);
			}
		}
		/*
		else if (ParticlesInSubsec[DWORD(sub-subsectors)] != NO_PARTICLE)
		{
			FJob job = new FGLJobProcessParticles(sub);
			GLRenderer->mThreadManager->AddJob(job);
		}
		*/
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
		// Subsectors with only 2 lines cannot have any area!
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
					fakesector = gl_FakeFlat(sector, &fake, false);
				}

				BYTE &srf = gl_drawinfo->sectorrenderflags[sub->render_sector->sectornum];
				if (!(srf & SSRF_PROCESSED))
				{
					srf |= SSRF_PROCESSED;

					SetupFlat.Clock();
					//if (!gl_multithreading)
					{
						GLRenderer->ProcessSector(fakesector);
					}
					/*
					else
					{
						FJob *job = new FGLJobProcessFlats(sub);
						GLRenderer->mThreadManager->AddJob(job);
					}
					*/
					SetupFlat.Unclock();
				}
				// mark subsector as processed - but mark for rendering only if it has an actual area.
				gl_drawinfo->ss_renderflags[sub-subsectors] = 
					(sub->numlines > 2) ? SSRF_PROCESSED|SSRF_RENDERALL : SSRF_PROCESSED;
				if (sub->hacked & 1) gl_drawinfo->AddHackedSubsector(sub);

				FPortal *portal;

				portal = fakesector->portals[sector_t::ceiling];
				if (portal != NULL)
				{
					GLSectorStackPortal *glportal = portal->GetGLPortal();
					glportal->AddSubsector(sub);
				}

				portal = fakesector->portals[sector_t::floor];
				if (portal != NULL)
				{
					GLSectorStackPortal *glportal = portal->GetGLPortal();
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

void gl_RenderBSPNode (void *node)
{
	if (numnodes == 0)
	{
		DoSubsector (subsectors);
		return;
	}
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = R_PointOnSide(viewx, viewy, bsp);

		// Recursively divide front space (toward the viewer).
		gl_RenderBSPNode (bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;

		// It is not necessary to use the slower precise version here
		if (!clipper.CheckBox(bsp->bbox[side]))
		{
			if (!(gl_drawinfo->no_renderflags[bsp-nodes] & SSRF_SEEN))
				return;
		}

		node = bsp->children[side];
	}
	DoSubsector ((subsector_t *)((BYTE *)node - 1));
}


