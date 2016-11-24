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
#include "sbar.h"
#include "r_data/r_translate.h"
#include "r_poly_portal.h"
#include "gl/data/gl_data.h"

CVAR(Bool, r_debug_cull, 0, 0)

/////////////////////////////////////////////////////////////////////////////

void RenderPolyPortal::SetViewpoint(const TriMatrix &worldToClip, uint32_t stencilValue)
{
	WorldToClip = worldToClip;
	StencilValue = stencilValue;
}

void RenderPolyPortal::Render()
{
	ClearBuffers();
	Cull.CullScene(WorldToClip);
	RenderSectors();
	for (auto &portal : Portals)
		portal->Render();
}

void RenderPolyPortal::ClearBuffers()
{
	SectorSpriteRanges.clear();
	SectorSpriteRanges.resize(numsectors);
	SortedSprites.clear();
	TranslucentObjects.clear();
	Portals.clear();
	NextSubsectorDepth = 0;
}

void RenderPolyPortal::RenderSectors()
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

void RenderPolyPortal::RenderSubsector(subsector_t *sub)
{
	sector_t *frontsector = sub->sector;
	frontsector->MoreFlags |= SECF_DRAWN;

	uint32_t subsectorDepth = NextSubsectorDepth++;

	if (sub->sector->CenterFloor() != sub->sector->CenterCeiling())
	{
		RenderPolyPlane::RenderPlanes(WorldToClip, sub, subsectorDepth, Cull.MaxCeilingHeight, Cull.MinFloorHeight);

		FSectorPortal *ceilingPortal = frontsector->ValidatePortal(sector_t::ceiling);
		FSectorPortal *floorPortal = frontsector->ValidatePortal(sector_t::floor);
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

	SpriteRange sprites = GetSpritesForSector(sub->sector);
	for (int i = 0; i < sprites.Count; i++)
	{
		AActor *thing = SortedSprites[sprites.Start + i].Thing;
		TranslucentObjects.push_back({ thing, sub, subsectorDepth });
	}

	TranslucentObjects.insert(TranslucentObjects.end(), SubsectorTranslucentWalls.begin(), SubsectorTranslucentWalls.end());
	SubsectorTranslucentWalls.clear();
}

SpriteRange RenderPolyPortal::GetSpritesForSector(sector_t *sector)
{
	if (SectorSpriteRanges.size() < sector->sectornum || sector->sectornum < 0)
		return SpriteRange();

	auto &range = SectorSpriteRanges[sector->sectornum];
	if (range.Start == -1)
	{
		range.Start = (int)SortedSprites.size();
		range.Count = 0;
		for (AActor *thing = sector->thinglist; thing != nullptr; thing = thing->snext)
		{
			SortedSprites.push_back({ thing, (thing->Pos() - ViewPos).LengthSquared() });
			range.Count++;
		}
		std::stable_sort(SortedSprites.begin() + range.Start, SortedSprites.begin() + range.Start + range.Count);
	}
	return range;
}

void RenderPolyPortal::RenderLine(subsector_t *sub, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth)
{
	// Reject lines not facing viewer
	DVector2 pt1 = line->v1->fPos() - ViewPos;
	DVector2 pt2 = line->v2->fPos() - ViewPos;
	if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
		return;

	// Cull wall if not visible
	int sx1, sx2;
	bool hasSegmentRange = Cull.GetSegmentRangeForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), sx1, sx2);
	if (!hasSegmentRange || Cull.IsSegmentCulled(sx1, sx2))
		return;

	// Tell automap we saw this
	if (!r_dontmaplines && line->linedef)
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
			RenderPolyWall::Render3DFloorLine(WorldToClip, line, frontsector, subsectorDepth, fakeFloor, SubsectorTranslucentWalls);
		}
	}

	// Render wall, and update culling info if its an occlusion blocker
	if (RenderPolyWall::RenderLine(WorldToClip, line, frontsector, subsectorDepth, SubsectorTranslucentWalls))
	{
		if (hasSegmentRange)
			Cull.MarkSegmentCulled(sx1, sx2);
	}
}

void RenderPolyPortal::RenderTranslucent()
{
	for (auto it = Portals.rbegin(); it != Portals.rend(); ++it)
		(*it)->RenderTranslucent();

	for (auto it = TranslucentObjects.rbegin(); it != TranslucentObjects.rend(); ++it)
	{
		auto &obj = *it;
		if (obj.particle)
		{
			RenderPolyParticle spr;
			spr.Render(WorldToClip, obj.particle, obj.sub, obj.subsectorDepth);
		}
		else if (!obj.thing)
		{
			obj.wall.Render(WorldToClip);
		}
		else if ((obj.thing->renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
		{
			RenderPolyWallSprite wallspr;
			wallspr.Render(WorldToClip, obj.thing, obj.sub, obj.subsectorDepth);
		}
		else
		{
			RenderPolySprite spr;
			spr.Render(WorldToClip, obj.thing, obj.sub, obj.subsectorDepth);
		}
	}
}
