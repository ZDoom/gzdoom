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
#include "p_effect.h"
#include "po_man.h"
#include "m_fixed.h"
#include "ctpl.h"
#include "texturemanager.h"
#include "hwrenderer/scene/hw_fakeflat.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hw_clock.h"
#include "flatvertices.h"
#include "hw_vertexbuilder.h"
#include "hw_walldispatcher.h"

#ifdef ARCH_IA32
#include <immintrin.h>
#endif // ARCH_IA32

CVAR(Bool, gl_multithread, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

EXTERN_CVAR(Float, r_actorspriteshadowdist)

thread_local bool isWorkerThread;
ctpl::thread_pool renderPool(1);
bool inited = false;

struct RenderJob
{
	enum
	{
		FlatJob,
		WallJob,
		SpriteJob,
		ParticleJob,
		PortalJob,
		TerminateJob	// inserted when all work is done so that the worker can return.
	};
	
	int type;
	subsector_t *sub;
	seg_t *seg;
};


class RenderJobQueue
{
	RenderJob pool[300000];	// Way more than ever needed. The largest ever seen on a single viewpoint is around 40000.
	std::atomic<int> readindex{};
	std::atomic<int> writeindex{};
public:
	void AddJob(int type, subsector_t *sub, seg_t *seg = nullptr)
	{
		// This does not check for array overflows. The pool should be large enough that it never hits the limit.

		pool[writeindex] = { type, sub, seg };
		writeindex++;	// update index only after the value has been written.
	}

	RenderJob *GetJob()
	{
		if (readindex < writeindex) return &pool[readindex++];
		return nullptr;
	}
	
	void ReleaseAll()
	{
		readindex = 0;
		writeindex = 0;
	}
};

static RenderJobQueue jobQueue;	// One static queue is sufficient here. This code will never be called recursively.

void HWDrawInfo::WorkerThread()
{
	sector_t *front, *back;
	HWWallDispatcher disp(this);

	WTTotal.Clock();
	isWorkerThread = true;	// for adding asserts in GL API code. The worker thread may never call any GL API.
	while (true)
	{
		auto job = jobQueue.GetJob();
		if (job == nullptr)
		{
#ifdef ARCH_IA32
			// The queue is empty. But yielding would be too costly here and possibly cause further delays down the line if the thread is halted.
			// So instead add a few pause instructions and retry immediately.
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
			_mm_pause();
#endif // ARCH_IA32
		}
		// Note that the main thread MUST have prepared the fake sectors that get used below!
		// This worker thread cannot prepare them itself without costly synchronization.
		else switch (job->type)
		{
		case RenderJob::TerminateJob:
			WTTotal.Unclock();
			return;

		case RenderJob::WallJob:
		{
			HWWall wall;
			SetupWall.Clock();
			wall.sub = job->sub;

			front = hw_FakeFlat(job->sub->sector, in_area, false);
			auto seg = job->seg;
			auto backsector = seg->backsector;
			if (!backsector && seg->linedef->isVisualPortal() && seg->sidedef == seg->linedef->sidedef[0]) // For one-sided portals use the portal's destination sector as backsector.
			{
				auto portal = seg->linedef->getPortal();
				backsector = portal->mDestination->frontsector;
				back = hw_FakeFlat(backsector, in_area, true);
				if (front->floorplane.isSlope() || front->ceilingplane.isSlope() || back->floorplane.isSlope() || back->ceilingplane.isSlope())
				{
					// Having a one-sided portal like this with slopes is too messy so let's ignore that case.
					back = nullptr;
				}
			}
			else if (backsector)
			{
				if (front->sectornum == backsector->sectornum || (seg->sidedef->Flags & WALLF_POLYOBJ))
				{
					back = front;
				}
				else
				{
					back = hw_FakeFlat(backsector, in_area, true);
				}
			}
			else back = nullptr;

			wall.Process(&disp, job->seg, front, back);
			rendered_lines++;
			SetupWall.Unclock();
			break;
		}

		case RenderJob::FlatJob:
		{
			HWFlat flat;
			SetupFlat.Clock();
			flat.section = job->sub->section;
			front = hw_FakeFlat(job->sub->render_sector, in_area, false);
			flat.ProcessSector(this, front);
			SetupFlat.Unclock();
			break;
		}

		case RenderJob::SpriteJob:
			SetupSprite.Clock();
			front = hw_FakeFlat(job->sub->sector, in_area, false);
			RenderThings(job->sub, front);
			SetupSprite.Unclock();
			break;

		case RenderJob::ParticleJob:
			SetupSprite.Clock();
			front = hw_FakeFlat(job->sub->sector, in_area, false);
			RenderParticles(job->sub, front);
			SetupSprite.Unclock();
			break;

		case RenderJob::PortalJob:
			AddSubsectorToPortal((FSectorPortalGroup *)job->seg, job->sub);
			break;
		}

	}
}





EXTERN_CVAR(Bool, gl_render_segs)

CVAR(Bool, gl_render_things, true, 0)
CVAR(Bool, gl_render_walls, true, 0)
CVAR(Bool, gl_render_flats, true, 0)

void HWDrawInfo::UnclipSubsector(subsector_t *sub)
{
	int count = sub->numlines;
	seg_t * seg = sub->firstline;
	auto &clipper = *mClipper;

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

void HWDrawInfo::AddLine (seg_t *seg, bool portalclip)
{
#ifdef _DEBUG
	if (seg->linedef && seg->linedef->Index() == 38)
	{
		int a = 0;
	}
#endif

	sector_t * backsector = nullptr;

	if (portalclip)
	{
		int clipres = mClipPortal->ClipSeg(seg, Viewpoint.Pos);
		if (clipres == PClip_InFront) return;
	}

	auto &clipper = *mClipper;
	angle_t startAngle = clipper.GetClipAngle(seg->v2);
	angle_t endAngle = clipper.GetClipAngle(seg->v1);

	// Back side, i.e. backface culling	- read: endAngle >= startAngle!
	if (startAngle-endAngle<ANGLE_180)  
	{
		return;
	}

	if (seg->sidedef == nullptr)
	{
		if (!(currentsubsector->flags & SSECMF_DRAWN))
		{
			if (clipper.SafeCheckRange(startAngle, endAngle)) 
			{
				currentsubsector->flags |= SSECMF_DRAWN;
			}
		}
		return;
	}

	if (!clipper.SafeCheckRange(startAngle, endAngle)) 
	{
		return;
	}
	currentsubsector->flags |= SSECMF_DRAWN;

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
				auto tex = TexMan.GetGameTexture(seg->sidedef->GetTexture(side_t::mid), true);
				if (!tex || !tex->isValid()) 
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
			if (in_area == area_default) in_area = hw_CheckViewArea(seg->v1, seg->v2, seg->frontsector, seg->backsector);

			backsector = hw_FakeFlat(seg->backsector, in_area, true);

			if (hw_CheckClip(seg->sidedef, currentsector, backsector))
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
			if (multithread)
			{
				jobQueue.AddJob(RenderJob::WallJob, seg->Subsector, seg);
			}
			else
			{
				HWWall wall;
				HWWallDispatcher disp(this);
				SetupWall.Clock();
				wall.sub = seg->Subsector;
				wall.Process(&disp, seg, currentsector, backsector);
				rendered_lines++;
				SetupWall.Unclock();
			}
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

void HWDrawInfo::PolySubsector(subsector_t * sub)
{
	int count = sub->numlines;
	seg_t * line = sub->firstline;

	while (count--)
	{
		if (line->linedef)
		{
			AddLine (line, mClipPortal != nullptr);
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

void HWDrawInfo::RenderPolyBSPNode (void *node)
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
		if (!mClipper->CheckBox(bsp->bbox[side]))
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

void HWDrawInfo::AddPolyobjs(subsector_t *sub)
{
	if (sub->BSP == nullptr || sub->BSP->bDirty)
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

void HWDrawInfo::AddLines(subsector_t * sub, sector_t * sector)
{
	currentsector = sector;
	currentsubsector = sub;

	ClipWall.Clock();
	if (sub->polys != nullptr)
	{
		AddPolyobjs(sub);
	}
	else
	{
		int count = sub->numlines;
		seg_t * seg = sub->firstline;

		while (count--)
		{
			if (seg->linedef == nullptr)
			{
				if (!(sub->flags & SSECMF_DRAWN)) AddLine (seg, mClipPortal != nullptr);
			}
			else if (!(seg->sidedef->Flags & WALLF_POLYOBJ)) 
			{
				AddLine (seg, mClipPortal != nullptr);
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

inline bool PointOnLine(const DVector2 &pos, const linebase_t *line)
{
	double v = (pos.Y - line->v1->fY()) * line->Delta().X + (line->v1->fX() - pos.X) * line->Delta().Y;
	return fabs(v) <= EQUAL_EPSILON;
}

void HWDrawInfo::AddSpecialPortalLines(subsector_t * sub, sector_t * sector, linebase_t *line)
{
	currentsector = sector;
	currentsubsector = sub;

	ClipWall.Clock();
	int count = sub->numlines;
	seg_t * seg = sub->firstline;

	while (count--)
	{
		if (seg->linedef != nullptr && seg->PartnerSeg != nullptr)
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

void HWDrawInfo::RenderThings(subsector_t * sub, sector_t * sector)
{
	sector_t * sec=sub->sector;
	// Handle all things in sector.
    const auto &vp = Viewpoint;
	for (auto p = sec->touching_renderthings; p != nullptr; p = p->m_snext)
	{
		auto thing = p->m_thing;
		if (thing->validcount == validcount) continue;
		thing->validcount = validcount;

		FIntCVar *cvar = thing->GetInfo()->distancecheck;
		if (cvar != nullptr && *cvar >= 0)
		{
			double dist = (thing->Pos() - vp.Pos).LengthSquared();
			double check = (double)**cvar;
			if (dist >= check * check)
			{
				continue;
			}
		}
		// If this thing is in a map section that's not in view it can't possibly be visible
		if (CurrentMapSections[thing->subsector->mapsection])
		{
			HWSprite sprite;

			// [Nash] draw sprite shadow
			if (R_ShouldDrawSpriteShadow(thing))
			{
				double dist = (thing->Pos() - vp.Pos).LengthSquared();
				double check = r_actorspriteshadowdist;
				if (dist <= check * check)
				{
					sprite.Process(this, thing, sector, in_area, false, true);
				}
			}

			sprite.Process(this, thing, sector, in_area, false);
		}
	}
	
	for (msecnode_t *node = sec->sectorportal_thinglist; node; node = node->m_snext)
	{
		AActor *thing = node->m_thing;
		FIntCVar *cvar = thing->GetInfo()->distancecheck;
		if (cvar != nullptr && *cvar >= 0)
		{
			double dist = (thing->Pos() - vp.Pos).LengthSquared();
			double check = (double)**cvar;
			if (dist >= check * check)
			{
				continue;
			}
		}

		HWSprite sprite;

		// [Nash] draw sprite shadow
		if (R_ShouldDrawSpriteShadow(thing))
		{
			double dist = (thing->Pos() - vp.Pos).LengthSquared();
			double check = r_actorspriteshadowdist;
			if (dist <= check * check)
			{
				sprite.Process(this, thing, sector, in_area, true, true);
			}
		}

		sprite.Process(this, thing, sector, in_area, true);
	}
}

void HWDrawInfo::RenderParticles(subsector_t *sub, sector_t *front)
{
	SetupSprite.Clock();
	for (uint32_t i = 0; i < sub->sprites.Size(); i++)
	{
		DVisualThinker *sp = sub->sprites[i];
		if (!sp || sp->ObjectFlags & OF_EuthanizeMe)
			continue;
		if (mClipPortal)
		{
			int clipres = mClipPortal->ClipPoint(sp->PT.Pos.XY());
			if (clipres == PClip_InFront) continue;
		}
		
		assert(sp->spr);

		sp->spr->ProcessParticle(this, &sp->PT, front, sp);
	}
	for (int i = Level->ParticlesInSubsec[sub->Index()]; i != NO_PARTICLE; i = Level->Particles[i].snext)
	{
		if (mClipPortal)
		{
			int clipres = mClipPortal->ClipPoint(Level->Particles[i].Pos.XY());
			if (clipres == PClip_InFront) continue;
		}

		HWSprite sprite;
		sprite.ProcessParticle(this, &Level->Particles[i], front, nullptr);
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

void HWDrawInfo::DoSubsector(subsector_t * sub)
{
	sector_t * sector;
	sector_t * fakesector;
	
#ifdef _DEBUG
	if (sub->sector->sectornum==931)
	{
		int a = 0;
	}
#endif

	sector=sub->sector;
	if (!sector) return;

	// If the mapsections differ this subsector can't possibly be visible from the current view point
	if (!CurrentMapSections[sub->mapsection]) return;
	if (sub->flags & SSECF_POLYORG) return;	// never render polyobject origin subsectors because their vertices no longer are where one may expect.

	if (ss_renderflags[sub->Index()] & SSRF_SEEN)
	{
		// This means that we have reached a subsector in a portal that has been marked 'seen'
		// from the other side of the portal. This means we must clear the clipper for the
		// range this subsector spans before going on.
		UnclipSubsector(sub);
	}
	if (mClipper->IsBlocked()) return;	// if we are inside a stacked sector portal which hasn't unclipped anything yet.

	fakesector=hw_FakeFlat(sector, in_area, false);

	if (mClipPortal)
	{
		int clipres = mClipPortal->ClipSubsector(sub);
		if (clipres == PClip_InFront)
		{
			auto line = mClipPortal->ClipLine();
			// The subsector is out of range, but we still have to check lines that lie directly on the boundary and may expose their upper or lower parts.
			if (line) AddSpecialPortalLines(sub, fakesector, line);
			return;
		}
	}

	if (sector->validcount != validcount)
	{
		CheckUpdate(screen->mVertexData, sector);
	}

	// [RH] Add particles
	if (gl_render_things && (sub->sprites.Size() > 0 || Level->ParticlesInSubsec[sub->Index()] != NO_PARTICLE))
	{
		if (multithread)
		{
			jobQueue.AddJob(RenderJob::ParticleJob, sub, nullptr);
		}
		else
		{
			SetupSprite.Clock();
			RenderParticles(sub, fakesector);
			SetupSprite.Unclock();
		}
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
		sector->MoreFlags |= SECMF_DRAWN;

		if (gl_render_things && (sector->touching_renderthings || sector->sectorportal_thinglist))
		{
			if (multithread)
			{
				jobQueue.AddJob(RenderJob::SpriteJob, sub, nullptr);
			}
			else
			{
				SetupSprite.Clock();
				RenderThings(sub, fakesector);
				SetupSprite.Unclock();
			}
		}
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
			if (!sector->GetHeightSec() || in_area!=area_default)
			{
				if (sector != sub->render_sector)
				{
					sector = sub->render_sector;
					// the planes of this subsector are faked to belong to another sector
					// This means we need the heightsec parts and light info of the render sector, not the actual one.
					fakesector = hw_FakeFlat(sector, in_area, false);
				}

				uint8_t &srf = section_renderflags[Level->sections.SectionIndex(sub->section)];
				if (!(srf & SSRF_PROCESSED))
				{
					srf |= SSRF_PROCESSED;

					if (multithread)
					{
						jobQueue.AddJob(RenderJob::FlatJob, sub);
					}
					else
					{
						HWFlat flat;
						flat.section = sub->section;
						SetupFlat.Clock();
						flat.ProcessSector(this, fakesector);
						SetupFlat.Unclock();
					}
				}
				// mark subsector as processed - but mark for rendering only if it has an actual area.
				ss_renderflags[sub->Index()] = 
					(sub->numlines > 2) ? SSRF_PROCESSED|SSRF_RENDERALL : SSRF_PROCESSED;
				if (sub->hacked & 1) AddHackedSubsector(sub);

				// This is for portal coverage.
				FSectorPortalGroup *portal;

				// AddSubsectorToPortal cannot be called here when using multithreaded processing,
				// because the wall processing code in the worker can also modify the portal state.
				// To avoid costly synchronization for every access to the portal list,
				// the call to AddSubsectorToPortal will be deferred to the worker.
				// (GetPortalGruop only accesses static sector data so this check can be done here, restricting the new job to the minimum possible extent.)
				portal = fakesector->GetPortalGroup(sector_t::ceiling);
				if (portal != nullptr)
				{
					if (multithread)
					{
						jobQueue.AddJob(RenderJob::PortalJob, sub, (seg_t *)portal);
					}
					else
					{
						AddSubsectorToPortal(portal, sub);
					}
				}

				portal = fakesector->GetPortalGroup(sector_t::floor);
				if (portal != nullptr)
				{
					if (multithread)
					{
						jobQueue.AddJob(RenderJob::PortalJob, sub, (seg_t *)portal);
					}
					else
					{
						AddSubsectorToPortal(portal, sub);
					}
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

void HWDrawInfo::RenderBSPNode (void *node)
{
	if (Level->nodes.Size() == 0)
	{
		DoSubsector (&Level->subsectors[0]);
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
		if (!mClipper->CheckBox(bsp->bbox[side]))
		{
			if (!(no_renderflags[bsp->Index()] & SSRF_SEEN))
				return;
		}

		node = bsp->children[side];
	}
	DoSubsector ((subsector_t *)((uint8_t *)node - 1));
}

void HWDrawInfo::RenderBSP(void *node, bool drawpsprites)
{
	Bsp.Clock();

	// Give the DrawInfo the viewpoint in fixed point because that's what the nodes are.
	viewx = FLOAT2FIXED(Viewpoint.Pos.X);
	viewy = FLOAT2FIXED(Viewpoint.Pos.Y);

	validcount++;	// used for processing sidedefs only once by the renderer.

	multithread = gl_multithread;
	if (multithread)
	{
		jobQueue.ReleaseAll();
		auto future = renderPool.push([&](int id) {
			WorkerThread();
		});
		RenderBSPNode(node);

		jobQueue.AddJob(RenderJob::TerminateJob, nullptr, nullptr);
		Bsp.Unclock();
		MTWait.Clock();
		future.wait();
		MTWait.Unclock();
	}
	else
	{
		RenderBSPNode(node);
		Bsp.Unclock();
	}
	// Process all the sprites on the current portal's back side which touch the portal.
	if (mCurrentPortal != nullptr) mCurrentPortal->RenderAttached(this);

	if (drawpsprites)
		PreparePlayerSprites(Viewpoint.sector, in_area);
}
