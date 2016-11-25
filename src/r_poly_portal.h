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

#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include "doomdata.h"
#include "r_utility.h"
#include "r_main.h"
#include "r_poly_triangle.h"
#include "r_poly_intersection.h"
#include "r_poly_wall.h"
#include "r_poly_sprite.h"
#include "r_poly_wallsprite.h"
#include "r_poly_playersprite.h"
#include "r_poly_particle.h"
#include "r_poly_plane.h"
#include "r_poly_cull.h"

// Used for sorting things by distance to the camera
class PolySortedSprite
{
public:
	PolySortedSprite(AActor *thing, double distanceSquared) : Thing(thing), DistanceSquared(distanceSquared) { }
	bool operator<(const PolySortedSprite &other) const { return DistanceSquared < other.DistanceSquared; }

	AActor *Thing;
	double DistanceSquared;
};

class PolyTranslucentObject
{
public:
	PolyTranslucentObject(particle_t *particle, subsector_t *sub, uint32_t subsectorDepth) : particle(particle), sub(sub), subsectorDepth(subsectorDepth) { }
	PolyTranslucentObject(AActor *thing, subsector_t *sub, uint32_t subsectorDepth) : thing(thing), sub(sub), subsectorDepth(subsectorDepth) { }
	PolyTranslucentObject(RenderPolyWall wall) : wall(wall) { }

	particle_t *particle = nullptr;
	AActor *thing = nullptr;
	subsector_t *sub = nullptr;
	uint32_t subsectorDepth = 0;

	RenderPolyWall wall;
};

class SpriteRange
{
public:
	SpriteRange() = default;
	SpriteRange(int start, int count) : Start(start), Count(count) { }
	int Start = -1;
	int Count = 0;
};

class PolyDrawSectorPortal;
class PolyDrawLinePortal;

// Renders everything from a specific viewpoint
class RenderPolyPortal
{
public:
	RenderPolyPortal();
	~RenderPolyPortal();
	void SetViewpoint(const TriMatrix &worldToClip, uint32_t stencilValue);
	void Render();
	void RenderTranslucent();

	static const uint32_t SkySubsectorDepth = 0x7fffffff;

private:
	void ClearBuffers();
	void RenderSectors();
	void RenderSubsector(subsector_t *sub);
	void RenderLine(subsector_t *sub, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth);

	SpriteRange GetSpritesForSector(sector_t *sector);

	TriMatrix WorldToClip;
	uint32_t StencilValue = 0;
	PolyCull Cull;
	uint32_t NextSubsectorDepth = 0;
	std::vector<SpriteRange> SectorSpriteRanges;
	std::vector<PolySortedSprite> SortedSprites;
	std::vector<PolyTranslucentObject> TranslucentObjects;
	std::vector<PolyTranslucentObject> SubsectorTranslucentWalls;

	std::vector<std::unique_ptr<PolyDrawSectorPortal>> SectorPortals;
	std::vector<std::unique_ptr<PolyDrawLinePortal>> LinePortals;
};

struct PolyPortalVertexRange
{
	PolyPortalVertexRange(const TriVertex *vertices, int count, bool ccw) : Vertices(vertices), Count(count), Ccw(ccw) { }
	const TriVertex *Vertices;
	int Count;
	bool Ccw;
};

class PolyDrawSectorPortal
{
public:
	PolyDrawSectorPortal(FSectorPortal *portal, bool ceiling);

	void Render();
	void RenderTranslucent();
	
	FSectorPortal *Portal;
	std::vector<PolyPortalVertexRange> Shape;

private:
	void SaveGlobals();
	void RestoreGlobals();

	bool Ceiling;
	RenderPolyPortal RenderPortal;
	
	int savedextralight;
	DVector3 savedpos;
	DAngle savedangle;
	double savedvisibility;
	AActor *savedcamera;
	sector_t *savedsector;
};

class PolyDrawLinePortal
{
public:
	PolyDrawLinePortal(line_t *src, line_t *dest, bool mirror);

	void Render();
	void RenderTranslucent();

	std::vector<PolyPortalVertexRange> Shape;

private:
	line_t *Src;
	line_t *Dest;
	RenderPolyPortal RenderPortal;
};
