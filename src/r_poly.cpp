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
#include "r_poly.h"
#include "gl/data/gl_data.h"

CVAR(Bool, r_debug_cull, 0, 0)
void InitGLRMapinfoData();

/////////////////////////////////////////////////////////////////////////////

void RenderPolyScene::Render()
{
	if (!r_swtruecolor) // Disable pal rendering for now
		return;

	ClearBuffers();
	SetupPerspectiveMatrix();
	Cull.CullScene(WorldToClip);
	RenderSectors();
	skydome.Render(WorldToClip);
	RenderTranslucent();
	PlayerSprites.Render();

	DrawerCommandQueue::WaitForWorkers();
}

void RenderPolyScene::RenderRemainingPlayerSprites()
{
	PlayerSprites.RenderRemainingSprites();
}

void RenderPolyScene::ClearBuffers()
{
	PolyVertexBuffer::Clear();
	SectorSpriteRanges.clear();
	SectorSpriteRanges.resize(numsectors);
	SortedSprites.clear();
	TranslucentObjects.clear();
	PolyStencilBuffer::Instance()->Clear(viewwidth, viewheight, 0);
	PolySubsectorGBuffer::Instance()->Resize(dc_pitch, viewheight);
	NextSubsectorDepth = 0;
}

void RenderPolyScene::SetupPerspectiveMatrix()
{
	static bool bDidSetup = false;

	if (!bDidSetup)
	{
		InitGLRMapinfoData();
		bDidSetup = true;
	}

	float pixelstretch = (glset.pixelstretch) ? glset.pixelstretch : 1.2;

	float ratio = WidescreenRatio;
	float fovratio = (WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(FieldOfView.Radians() / 2) / fovratio)).Degrees);
	TriMatrix worldToView =
		TriMatrix::scale(1.0f, 1.2f, 1.0f) *
		TriMatrix::rotate((float)ViewPitch.Radians(), 1.0f, 0.0f, 0.0f) *
		TriMatrix::rotate((float)(ViewAngle - 90).Radians(), 0.0f, -1.0f, 0.0f) *
		TriMatrix::scale(1.0f, pixelstretch, 1.0f) *
		TriMatrix::swapYZ() *
		TriMatrix::translate((float)-ViewPos.X, (float)-ViewPos.Y, (float)-ViewPos.Z);
	WorldToClip = TriMatrix::perspective(fovy, ratio, 5.0f, 65535.0f) * worldToView;
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
		RenderPolyPlane::RenderPlanes(WorldToClip, sub, subsectorDepth, Cull.MaxCeilingHeight, Cull.MinFloorHeight);
	}

	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ))
			RenderLine(sub, line, frontsector, subsectorDepth);
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

SpriteRange RenderPolyScene::GetSpritesForSector(sector_t *sector)
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

void RenderPolyScene::RenderLine(subsector_t *sub, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth)
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

	// Render wall, and update culling info if its an occlusion blocker
	if (RenderPolyWall::RenderLine(WorldToClip, line, frontsector, subsectorDepth, SubsectorTranslucentWalls))
	{
		if (hasSegmentRange)
			Cull.MarkSegmentCulled(sx1, sx2);
	}
}

void RenderPolyScene::RenderTranslucent()
{
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

RenderPolyScene *RenderPolyScene::Instance()
{
	static RenderPolyScene scene;
	return &scene;
}

/////////////////////////////////////////////////////////////////////////////

namespace
{
	int NextBufferVertex = 0;
}

TriVertex *PolyVertexBuffer::GetVertices(int count)
{
	enum { VertexBufferSize = 16 * 1024 };
	static TriVertex Vertex[VertexBufferSize];

	if (NextBufferVertex + count > VertexBufferSize)
		return nullptr;
	TriVertex *v = Vertex + NextBufferVertex;
	NextBufferVertex += count;
	return v;
}

void PolyVertexBuffer::Clear()
{
	NextBufferVertex = 0;
}
