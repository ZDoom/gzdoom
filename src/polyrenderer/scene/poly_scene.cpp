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
#include "gl/data/gl_data.h"
#include "swrenderer/scene/r_light.h"

CVAR(Bool, r_debug_cull, 0, 0)
EXTERN_CVAR(Int, r_portal_recursions)

/////////////////////////////////////////////////////////////////////////////

RenderPolyScene::RenderPolyScene()
{
}

RenderPolyScene::~RenderPolyScene()
{
}

void RenderPolyScene::SetViewpoint(const TriMatrix &worldToClip, const Vec4f &portalPlane, uint32_t stencilValue)
{
	WorldToClip = worldToClip;
	StencilValue = stencilValue;
	PortalPlane = portalPlane;
}

void RenderPolyScene::SetPortalSegments(const std::vector<PolyPortalSegment> &segments)
{
	Cull.ClearSolidSegments();
	for (const auto &segment : segments)
	{
		Cull.MarkSegmentCulled(segment.X1, segment.X2);
	}
	Cull.InvertSegments();
	PortalSegmentsAdded = true;
}

void RenderPolyScene::Render(int portalDepth)
{
	ClearBuffers();
	if (!PortalSegmentsAdded)
		Cull.ClearSolidSegments();
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
	if (r_debug_cull)
	{
		for (auto it = Cull.PvsSectors.rbegin(); it != Cull.PvsSectors.rend(); ++it)
			RenderSubsector(*it);
	}
	else
	{
		for (auto it = Cull.PvsSectors.begin(); it != Cull.PvsSectors.end(); ++it)
			RenderSubsector(*it);
	}
}

void RenderPolyScene::RenderSubsector(subsector_t *sub)
{
	sector_t *frontsector = sub->sector;
	frontsector->MoreFlags |= SECF_DRAWN;

	uint32_t subsectorDepth = NextSubsectorDepth++;

	if (sub->sector->CenterFloor() != sub->sector->CenterCeiling())
	{
		RenderPolyPlane::RenderPlanes(WorldToClip, PortalPlane, Cull, sub, subsectorDepth, StencilValue, Cull.MaxCeilingHeight, Cull.MinFloorHeight, SectorPortals);
	}

	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ))
		{
			RenderLine(sub, line, frontsector, subsectorDepth);
		}
	}

	bool mainBSP = ((unsigned int)(sub - subsectors) < (unsigned int)numsubsectors);
	if (mainBSP)
	{
		int subsectorIndex = (int)(sub - subsectors);
		for (int i = ParticlesInSubsec[subsectorIndex]; i != NO_PARTICLE; i = Particles[i].snext)
		{
			particle_t *particle = Particles + i;
			TranslucentObjects.push_back({ particle, sub, subsectorDepth });
		}
	}

	SeenSectors.insert(sub->sector);
	SubsectorDepths[sub] = subsectorDepth;
}

void RenderPolyScene::RenderSprite(AActor *thing, double sortDistance, const DVector2 &left, const DVector2 &right)
{
	if (numnodes == 0)
	{
		subsector_t *sub = subsectors;
		auto it = SubsectorDepths.find(sub);
		if (it != SubsectorDepths.end())
			TranslucentObjects.push_back({ thing, sub, it->second, sortDistance, 0.0f, 1.0f });
	}
	else
	{
		RenderSprite(thing, sortDistance, left, right, 0.0, 1.0, nodes + numnodes - 1); // The head node is the last node output.
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
		TranslucentObjects.push_back({ thing, sub, it->second, sortDistance, (float)t1, (float)t2 });
}

void RenderPolyScene::RenderLine(subsector_t *sub, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth)
{
	// Reject lines not facing viewer
	DVector2 pt1 = line->v1->fPos() - ViewPos;
	DVector2 pt2 = line->v2->fPos() - ViewPos;
	if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
		return;

	// Cull wall if not visible
	int sx1, sx2;
	LineSegmentRange segmentRange = Cull.GetSegmentRangeForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), sx1, sx2);
	if (segmentRange == LineSegmentRange::NotVisible || (segmentRange == LineSegmentRange::HasSegment && Cull.IsSegmentCulled(sx1, sx2)))
		return;

	// Tell automap we saw this
	if (!PolyRenderer::Instance()->DontMapLines && line->linedef && segmentRange != LineSegmentRange::AlwaysVisible)
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
	if (RenderPolyWall::RenderLine(WorldToClip, PortalPlane, Cull, line, frontsector, subsectorDepth, StencilValue, TranslucentObjects, LinePortals))
	{
		if (segmentRange == LineSegmentRange::HasSegment)
			Cull.MarkSegmentCulled(sx1, sx2);
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
		args.objectToClip = &WorldToClip;
		args.mode = TriangleDrawMode::Fan;
		args.uniforms.globvis = (float)swrenderer::LightVisibility::Instance()->WallGlobVis(foggy);
		args.uniforms.color = 0;
		args.uniforms.light = 256;
		args.uniforms.flags = TriUniforms::fixed_light;
		args.SetClipPlane(PortalPlane.x, PortalPlane.y, PortalPlane.z, PortalPlane.w);

		for (auto &portal : SectorPortals)
		{
			args.stenciltestvalue = portal->StencilValue;
			args.stencilwritevalue = portal->StencilValue + 1;
			for (const auto &verts : portal->Shape)
			{
				args.vinput = verts.Vertices;
				args.vcount = verts.Count;
				args.ccw = verts.Ccw;
				args.uniforms.subsectorDepth = verts.SubsectorDepth;
				args.blendmode = TriBlendMode::Copy;
				PolyTriangleDrawer::draw(args);
			}
		}

		for (auto &portal : LinePortals)
		{
			args.stenciltestvalue = portal->StencilValue;
			args.stencilwritevalue = portal->StencilValue + 1;
			for (const auto &verts : portal->Shape)
			{
				args.vinput = verts.Vertices;
				args.vcount = verts.Count;
				args.ccw = verts.Ccw;
				args.uniforms.subsectorDepth = verts.SubsectorDepth;
				PolyTriangleDrawer::draw(args);
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
			args.objectToClip = &WorldToClip;
			args.mode = TriangleDrawMode::Fan;
			args.stenciltestvalue = portal->StencilValue + 1;
			args.stencilwritevalue = StencilValue + 1;
			args.SetClipPlane(PortalPlane.x, PortalPlane.y, PortalPlane.z, PortalPlane.w);
			for (const auto &verts : portal->Shape)
			{
				args.vinput = verts.Vertices;
				args.vcount = verts.Count;
				args.ccw = verts.Ccw;
				args.uniforms.subsectorDepth = verts.SubsectorDepth;
				args.writeColor = false;
				PolyTriangleDrawer::draw(args);
			}
		}

		for (auto it = LinePortals.rbegin(); it != LinePortals.rend(); ++it)
		{
			auto &portal = *it;
			portal->RenderTranslucent(portalDepth + 1);
		
			PolyDrawArgs args;
			args.objectToClip = &WorldToClip;
			args.mode = TriangleDrawMode::Fan;
			args.stenciltestvalue = portal->StencilValue + 1;
			args.stencilwritevalue = StencilValue + 1;
			args.SetClipPlane(PortalPlane.x, PortalPlane.y, PortalPlane.z, PortalPlane.w);
			for (const auto &verts : portal->Shape)
			{
				args.vinput = verts.Vertices;
				args.vcount = verts.Count;
				args.ccw = verts.Ccw;
				args.uniforms.subsectorDepth = verts.SubsectorDepth;
				args.writeColor = false;
				PolyTriangleDrawer::draw(args);
			}
		}
	}

	for (sector_t *sector : SeenSectors)
	{
		for (AActor *thing = sector->thinglist; thing != nullptr; thing = thing->snext)
		{
			DVector2 left, right;
			if (!RenderPolySprite::GetLine(thing, left, right))
				continue;
			double distanceSquared = (thing->Pos() - ViewPos).LengthSquared();
			RenderSprite(thing, distanceSquared, left, right);
		}
	}

	std::stable_sort(TranslucentObjects.begin(), TranslucentObjects.end());

	for (auto it = TranslucentObjects.rbegin(); it != TranslucentObjects.rend(); ++it)
	{
		auto &obj = *it;
		if (obj.particle)
		{
			RenderPolyParticle spr;
			spr.Render(WorldToClip, PortalPlane, obj.particle, obj.sub, obj.subsectorDepth, StencilValue + 1);
		}
		else if (!obj.thing)
		{
			obj.wall.Render(WorldToClip, PortalPlane, Cull);
		}
		else if ((obj.thing->renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
		{
			RenderPolyWallSprite wallspr;
			wallspr.Render(WorldToClip, PortalPlane, obj.thing, obj.sub, obj.subsectorDepth, StencilValue + 1);
		}
		else
		{
			RenderPolySprite spr;
			spr.Render(WorldToClip, PortalPlane, obj.thing, obj.sub, obj.subsectorDepth, StencilValue + 1, obj.SpriteLeft, obj.SpriteRight);
		}
	}
}
