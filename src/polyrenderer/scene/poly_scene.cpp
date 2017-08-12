/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "p_maputl.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "polyrenderer/scene/poly_scene.h"
#include "polyrenderer/poly_renderer.h"
#include "polyrenderer/scene/poly_light.h"

EXTERN_CVAR(Int, r_portal_recursions)

/////////////////////////////////////////////////////////////////////////////

RenderPolyScene::RenderPolyScene()
{
}

RenderPolyScene::~RenderPolyScene()
{
}

void RenderPolyScene::SetViewpoint(const TriMatrix &worldToClip, const PolyClipPlane &portalPlane, uint32_t stencilValue)
{
	WorldToClip = worldToClip;
	StencilValue = stencilValue;
	PortalPlane = portalPlane;
}

void RenderPolyScene::SetPortalSegments(const std::vector<PolyPortalSegment> &segments)
{
	if (!segments.empty())
	{
		Cull.ClearSolidSegments();
		for (const auto &segment : segments)
		{
			Cull.MarkSegmentCulled(segment.Start, segment.End);
		}
		Cull.InvertSegments();
		PortalSegmentsAdded = true;
	}
	else
	{
		PortalSegmentsAdded = false;
	}
}

void RenderPolyScene::Render(int portalDepth)
{
	ClearBuffers();
	if (!PortalSegmentsAdded)
		Cull.ClearSolidSegments();
	Cull.MarkViewFrustum();
	Cull.CullScene(WorldToClip, PortalPlane);
	Cull.ClearSolidSegments();
	RenderSectors();
	RenderPortals(portalDepth);
}

void RenderPolyScene::ClearBuffers()
{
	SeenSectors.clear();
	SubsectorDepths.clear();
	TranslucentObjects.clear();
	SectorPortals.clear();
	LinePortals.clear();
	NextSubsectorDepth = 0;
}

void RenderPolyScene::RenderSectors()
{
	int count = (int)Cull.PvsSectors.size();
	auto subsectors = Cull.PvsSectors.data();

	int nextCeilingZChange = 0;
	int nextFloorZChange = 0;
	uint32_t ceilingSubsectorDepth = 0;
	uint32_t floorSubsectorDepth = 0;

	for (int i = 0; i < count; i++)
	{
		// The software renderer only updates the clipping if the sector height changes.
		// Find the subsector depths for when that happens.
		if (i == nextCeilingZChange)
		{
			double z = subsectors[i]->sector->ceilingplane.Zat0();
			nextCeilingZChange++;
			while (nextCeilingZChange < count)
			{
				double nextZ = subsectors[nextCeilingZChange]->sector->ceilingplane.Zat0();
				if (nextZ > z)
					break;
				z = nextZ;
				nextCeilingZChange++;
			}
			ceilingSubsectorDepth = NextSubsectorDepth + nextCeilingZChange - i - 1;
		}
		if (i == nextFloorZChange)
		{
			double z = subsectors[i]->sector->floorplane.Zat0();
			nextFloorZChange++;
			while (nextFloorZChange < count)
			{
				double nextZ = subsectors[nextFloorZChange]->sector->floorplane.Zat0();
				if (nextZ < z)
					break;
				z = nextZ;
				nextFloorZChange++;
			}
			floorSubsectorDepth = NextSubsectorDepth + nextFloorZChange - i - 1;
		}

		RenderSubsector(subsectors[i], ceilingSubsectorDepth, floorSubsectorDepth);
	}
}

void RenderPolyScene::RenderSubsector(subsector_t *sub, uint32_t ceilingSubsectorDepth, uint32_t floorSubsectorDepth)
{
	sector_t *frontsector = sub->sector;
	frontsector->MoreFlags |= SECF_DRAWN;

	uint32_t subsectorDepth = NextSubsectorDepth++;

	bool mainBSP = sub->polys == nullptr;

	if (sub->polys)
	{
		if (sub->BSP == nullptr || sub->BSP->bDirty)
		{
			sub->BuildPolyBSP();

			// This is done by the GL renderer, but not the sw renderer. No idea what the purpose is..
			for (unsigned i = 0; i < sub->BSP->Segs.Size(); i++)
			{
				sub->BSP->Segs[i].Subsector = sub;
				sub->BSP->Segs[i].PartnerSeg = nullptr;
			}
		}

		if (sub->BSP->Nodes.Size() == 0)
		{
			RenderPolySubsector(&sub->BSP->Subsectors[0], subsectorDepth, frontsector);
		}
		else
		{
			RenderPolyNode(&sub->BSP->Nodes.Last(), subsectorDepth, frontsector);
		}
	}
	else
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			RenderLine(sub, line, frontsector, subsectorDepth);
		}
	}

	if (sub->sector->CenterFloor() != sub->sector->CenterCeiling())
	{
		RenderPolyPlane::RenderPlanes(WorldToClip, PortalPlane, Cull, sub, StencilValue, Cull.MaxCeilingHeight, Cull.MinFloorHeight, SectorPortals);
	}

	if (mainBSP)
	{
		RenderMemory &memory = PolyRenderer::Instance()->FrameMemory;
		int subsectorIndex = sub->Index();
		for (int i = ParticlesInSubsec[subsectorIndex]; i != NO_PARTICLE; i = Particles[i].snext)
		{
			particle_t *particle = Particles + i;
			TranslucentObjects.push_back(memory.NewObject<PolyTranslucentObject>(particle, sub, subsectorDepth));
		}
	}

	SeenSectors.insert(sub->sector);
	SubsectorDepths[sub] = subsectorDepth;
}

void RenderPolyScene::RenderPolyNode(void *node, uint32_t subsectorDepth, sector_t *frontsector)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = PointOnSide(PolyRenderer::Instance()->Viewpoint.Pos, bsp);

		// Recursively divide front space (toward the viewer).
		RenderPolyNode(bsp->children[side], subsectorDepth, frontsector);

		// Possibly divide back space (away from the viewer).
		side ^= 1;

		// Don't bother culling on poly objects
		//if (!CheckBBox(bsp->bbox[side]))
		//	return;

		node = bsp->children[side];
	}

	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
	RenderPolySubsector(sub, subsectorDepth, frontsector);
}

void RenderPolyScene::RenderPolySubsector(subsector_t *sub, uint32_t subsectorDepth, sector_t *frontsector)
{
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;

	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (line->linedef)
		{
			// Reject lines not facing viewer
			DVector2 pt1 = line->v1->fPos() - viewpoint.Pos;
			DVector2 pt2 = line->v2->fPos() - viewpoint.Pos;
			if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
				continue;

			// Cull wall if not visible
			angle_t angle1, angle2;
			if (!Cull.GetAnglesForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), angle1, angle2))
				continue;

			// Tell automap we saw this
			if (!PolyRenderer::Instance()->DontMapLines && line->linedef)
			{
				line->linedef->flags |= ML_MAPPED;
				sub->flags |= SSECF_DRAWN;
			}

			// Render wall, and update culling info if its an occlusion blocker
			if (RenderPolyWall::RenderLine(WorldToClip, PortalPlane, Cull, line, frontsector, subsectorDepth, StencilValue, TranslucentObjects, LinePortals, LastPortalLine))
			{
				Cull.MarkSegmentCulled(angle1, angle2);
			}
		}
	}
}

int RenderPolyScene::PointOnSide(const DVector2 &pos, const node_t *node)
{
	return DMulScale32(FLOAT2FIXED(pos.Y) - node->y, node->dx, node->x - FLOAT2FIXED(pos.X), node->dy) > 0;
}

void RenderPolyScene::RenderSprite(AActor *thing, double sortDistance, const DVector2 &left, const DVector2 &right)
{
	if (level.nodes.Size() == 0)
	{
		subsector_t *sub = &level.subsectors[0];
		auto it = SubsectorDepths.find(sub);
		if (it != SubsectorDepths.end())
			TranslucentObjects.push_back(PolyRenderer::Instance()->FrameMemory.NewObject<PolyTranslucentObject>(thing, sub, it->second, sortDistance, 0.0f, 1.0f));
	}
	else
	{
		RenderSprite(thing, sortDistance, left, right, 0.0, 1.0, level.HeadNode());
	}
}

void RenderPolyScene::RenderSprite(AActor *thing, double sortDistance, DVector2 left, DVector2 right, double t1, double t2, void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;
		
		DVector2 planePos(FIXED2DBL(bsp->x), FIXED2DBL(bsp->y));
		DVector2 planeNormal = DVector2(FIXED2DBL(-bsp->dy), FIXED2DBL(bsp->dx));
		double planeD = planeNormal | planePos;
		
		int sideLeft = (left | planeNormal) > planeD;
		int sideRight = (right | planeNormal) > planeD;
		
		if (sideLeft != sideRight)
		{
			double dotLeft = planeNormal | left;
			double dotRight = planeNormal | right;
			double t = (planeD - dotLeft) / (dotRight - dotLeft);
			
			DVector2 mid = left * (1.0 - t) + right * t;
			double tmid = t1 * (1.0 - t) + t2 * t;
			
			RenderSprite(thing, sortDistance, mid, right, tmid, t2, bsp->children[sideRight]);
			right = mid;
			t2 = tmid;
		}
		node = bsp->children[sideLeft];
	}
	
	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
	
	auto it = SubsectorDepths.find(sub);
	if (it != SubsectorDepths.end())
		TranslucentObjects.push_back(PolyRenderer::Instance()->FrameMemory.NewObject<PolyTranslucentObject>(thing, sub, it->second, sortDistance, (float)t1, (float)t2));
}

void RenderPolyScene::RenderLine(subsector_t *sub, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth)
{
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;

	// Reject lines not facing viewer
	DVector2 pt1 = line->v1->fPos() - viewpoint.Pos;
	DVector2 pt2 = line->v2->fPos() - viewpoint.Pos;
	if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
		return;

	// Cull wall if not visible
	angle_t angle1, angle2;
	if (!Cull.GetAnglesForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), angle1, angle2))
		return;

	// Tell automap we saw this
	if (!PolyRenderer::Instance()->DontMapLines && line->linedef)
	{
		line->linedef->flags |= ML_MAPPED;
		sub->flags |= SSECF_DRAWN;
	}

	// Render 3D floor sides
	if (line->backsector && frontsector->e && line->backsector->e->XFloor.ffloors.Size())
	{
		for (unsigned int i = 0; i < line->backsector->e->XFloor.ffloors.Size(); i++)
		{
			F3DFloor *fakeFloor = line->backsector->e->XFloor.ffloors[i];
			if (!(fakeFloor->flags & FF_EXISTS)) continue;
			if (!(fakeFloor->flags & FF_RENDERPLANES)) continue;
			if (!fakeFloor->model) continue;
			RenderPolyWall::Render3DFloorLine(WorldToClip, PortalPlane, Cull, line, frontsector, subsectorDepth, StencilValue, fakeFloor, TranslucentObjects);
		}
	}

	// Render wall, and update culling info if its an occlusion blocker
	if (RenderPolyWall::RenderLine(WorldToClip, PortalPlane, Cull, line, frontsector, subsectorDepth, StencilValue, TranslucentObjects, LinePortals, LastPortalLine))
	{
		Cull.MarkSegmentCulled(angle1, angle2);
	}
}

void RenderPolyScene::RenderPortals(int portalDepth)
{
	bool foggy = false;
	if (portalDepth < r_portal_recursions)
	{
		for (auto &portal : SectorPortals)
			portal->Render(portalDepth + 1);

		for (auto &portal : LinePortals)
			portal->Render(portalDepth + 1);
	}
	else // Fill with black
	{
		PolyDrawArgs args;
		args.SetTransform(&WorldToClip);
		args.SetLight(&NormalLight, 255, PolyRenderer::Instance()->Light.WallGlobVis(foggy), true);
		args.SetColor(0, 0);
		args.SetClipPlane(PortalPlane);
		args.SetStyle(TriBlendMode::FillOpaque);

		for (auto &portal : SectorPortals)
		{
			args.SetStencilTestValue(portal->StencilValue);
			args.SetWriteStencil(true, portal->StencilValue + 1);
			for (const auto &verts : portal->Shape)
			{
				args.SetFaceCullCCW(verts.Ccw);
				args.DrawArray(verts.Vertices, verts.Count, PolyDrawMode::TriangleFan);
			}
		}

		for (auto &portal : LinePortals)
		{
			args.SetStencilTestValue(portal->StencilValue);
			args.SetWriteStencil(true, portal->StencilValue + 1);
			for (const auto &verts : portal->Shape)
			{
				args.SetFaceCullCCW(verts.Ccw);
				args.DrawArray(verts.Vertices, verts.Count, PolyDrawMode::TriangleFan);
			}
		}
	}
}

void RenderPolyScene::RenderTranslucent(int portalDepth)
{
	if (portalDepth < r_portal_recursions)
	{
		for (auto it = SectorPortals.rbegin(); it != SectorPortals.rend(); ++it)
		{
			auto &portal = *it;
			portal->RenderTranslucent(portalDepth + 1);
		
			PolyDrawArgs args;
			args.SetTransform(&WorldToClip);
			args.SetStencilTestValue(portal->StencilValue + 1);
			args.SetWriteStencil(true, StencilValue + 1);
			args.SetClipPlane(PortalPlane);
			for (const auto &verts : portal->Shape)
			{
				args.SetFaceCullCCW(verts.Ccw);
				args.SetWriteColor(false);
				args.DrawArray(verts.Vertices, verts.Count, PolyDrawMode::TriangleFan);
			}
		}

		for (auto it = LinePortals.rbegin(); it != LinePortals.rend(); ++it)
		{
			auto &portal = *it;
			portal->RenderTranslucent(portalDepth + 1);
		
			PolyDrawArgs args;
			args.SetTransform(&WorldToClip);
			args.SetStencilTestValue(portal->StencilValue + 1);
			args.SetWriteStencil(true, StencilValue + 1);
			args.SetClipPlane(PortalPlane);
			for (const auto &verts : portal->Shape)
			{
				args.SetFaceCullCCW(verts.Ccw);
				args.SetWriteColor(false);
				args.DrawArray(verts.Vertices, verts.Count, PolyDrawMode::TriangleFan);
			}
		}
	}

	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	for (sector_t *sector : SeenSectors)
	{
		for (AActor *thing = sector->thinglist; thing != nullptr; thing = thing->snext)
		{
			DVector2 left, right;
			if (!RenderPolySprite::GetLine(thing, left, right))
				continue;
			double distanceSquared = (thing->Pos() - viewpoint.Pos).LengthSquared();
			RenderSprite(thing, distanceSquared, left, right);
		}
	}

	std::stable_sort(TranslucentObjects.begin(), TranslucentObjects.end(), [](auto a, auto b) { return *a < *b; });

	for (auto it = TranslucentObjects.rbegin(); it != TranslucentObjects.rend(); ++it)
	{
		PolyTranslucentObject *obj = *it;
		if (obj->particle)
		{
			RenderPolyParticle spr;
			spr.Render(WorldToClip, PortalPlane, obj->particle, obj->sub, StencilValue + 1);
		}
		else if (!obj->thing)
		{
			obj->wall.Render(WorldToClip, PortalPlane, Cull);
		}
		else if ((obj->thing->renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
		{
			RenderPolyWallSprite wallspr;
			wallspr.Render(WorldToClip, PortalPlane, obj->thing, obj->sub, StencilValue + 1);
		}
		else
		{
			RenderPolySprite spr;
			spr.Render(WorldToClip, PortalPlane, obj->thing, obj->sub, StencilValue + 1, obj->SpriteLeft, obj->SpriteRight);
		}
	}
}
