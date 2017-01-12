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
#include "polyrenderer/drawers/poly_triangle.h"
#include "polyrenderer/math/poly_intersection.h"
#include "poly_wall.h"
#include "poly_sprite.h"
#include "poly_wallsprite.h"
#include "poly_playersprite.h"
#include "poly_particle.h"
#include "poly_plane.h"
#include "poly_cull.h"
#include <set>
#include <unordered_map>

class PolyTranslucentObject
{
public:
	PolyTranslucentObject(particle_t *particle, subsector_t *sub, uint32_t subsectorDepth) : particle(particle), sub(sub), subsectorDepth(subsectorDepth) { }
	PolyTranslucentObject(AActor *thing, subsector_t *sub, uint32_t subsectorDepth, double dist, float t1, float t2) : thing(thing), sub(sub), subsectorDepth(subsectorDepth), DistanceSquared(dist), SpriteLeft(t1), SpriteRight(t2) { }
	PolyTranslucentObject(RenderPolyWall wall) : wall(wall), subsectorDepth(wall.SubsectorDepth), DistanceSquared(1.e6) { }

	bool operator<(const PolyTranslucentObject &other) const
	{
		return subsectorDepth != other.subsectorDepth ? subsectorDepth < other.subsectorDepth : DistanceSquared < other.DistanceSquared;
	}

	particle_t *particle = nullptr;
	AActor *thing = nullptr;
	subsector_t *sub = nullptr;

	RenderPolyWall wall;
	
	uint32_t subsectorDepth = 0;
	double DistanceSquared = 0.0;
	
	float SpriteLeft = 0.0f, SpriteRight = 1.0f;
};

class PolyDrawSectorPortal;
class PolyDrawLinePortal;
class PolyPortalSegment;

// Renders everything from a specific viewpoint
class RenderPolyScene
{
public:
	RenderPolyScene();
	~RenderPolyScene();
	void SetViewpoint(const TriMatrix &worldToClip, const Vec4f &portalPlane, uint32_t stencilValue);
	void SetPortalSegments(const std::vector<PolyPortalSegment> &segments);
	void Render(int portalDepth);
	void RenderTranslucent(int portalDepth);

	static const uint32_t SkySubsectorDepth = 0x7fffffff;

private:
	void ClearBuffers();
	void RenderPortals(int portalDepth);
	void RenderSectors();
	void RenderSubsector(subsector_t *sub);
	void RenderLine(subsector_t *sub, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth);
	void RenderSprite(AActor *thing, double sortDistance, const DVector2 &left, const DVector2 &right);
	void RenderSprite(AActor *thing, double sortDistance, DVector2 left, DVector2 right, double t1, double t2, void *node);

	TriMatrix WorldToClip;
	Vec4f PortalPlane;
	uint32_t StencilValue = 0;
	PolyCull Cull;
	uint32_t NextSubsectorDepth = 0;
	std::set<sector_t *> SeenSectors;
	std::unordered_map<subsector_t *, uint32_t> SubsectorDepths;
	std::vector<PolyTranslucentObject> TranslucentObjects;

	std::vector<std::unique_ptr<PolyDrawSectorPortal>> SectorPortals;
	std::vector<std::unique_ptr<PolyDrawLinePortal>> LinePortals;
	bool PortalSegmentsAdded = false;
};
