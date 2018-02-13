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
#include "poly_playersprite.h"
#include "poly_cull.h"

class PolyTranslucentObject
{
public:
	PolyTranslucentObject(uint32_t subsectorDepth = 0, double distanceSquared = 0.0) : subsectorDepth(subsectorDepth), DistanceSquared(distanceSquared) { }
	virtual ~PolyTranslucentObject() { }

	virtual void Render(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &portalPlane) = 0;

	bool operator<(const PolyTranslucentObject &other) const
	{
		return subsectorDepth != other.subsectorDepth ? subsectorDepth < other.subsectorDepth : DistanceSquared < other.DistanceSquared;
	}

	uint32_t subsectorDepth;
	double DistanceSquared;
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
	void SetViewpoint(const TriMatrix &worldToClip, const PolyClipPlane &portalPlane, uint32_t stencilValue);
	void Render(int portalDepth);
	void RenderTranslucent(int portalDepth);

	static const uint32_t SkySubsectorDepth = 0x7fffffff;

	line_t *LastPortalLine = nullptr;

private:
	void RenderPortals(int portalDepth);
	void RenderSectors();
	void RenderSubsector(PolyRenderThread *thread, subsector_t *sub, uint32_t subsectorDepth);
	void RenderLine(PolyRenderThread *thread, subsector_t *sub, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth);
	void RenderSprite(PolyRenderThread *thread, AActor *thing, double sortDistance, const DVector2 &left, const DVector2 &right);
	void RenderSprite(PolyRenderThread *thread, AActor *thing, double sortDistance, DVector2 left, DVector2 right, double t1, double t2, void *node);

	void RenderPolySubsector(PolyRenderThread *thread, subsector_t *sub, uint32_t subsectorDepth, sector_t *frontsector);
	void RenderPolyNode(PolyRenderThread *thread, void *node, uint32_t subsectorDepth, sector_t *frontsector);
	static int PointOnSide(const DVector2 &pos, const node_t *node);

	TriMatrix WorldToClip;
	PolyClipPlane PortalPlane;
	uint32_t StencilValue = 0;
	PolyCull Cull;
	bool PortalSegmentsAdded = false;

	std::vector<std::vector<PolyTranslucentObject *>> TranslucentObjects;
	std::vector<std::unique_ptr<PolyDrawSectorPortal>> SectorPortals;
	std::vector<std::unique_ptr<PolyDrawLinePortal>> LinePortals;
};

enum class PolyWaterFakeSide
{
	Center,
	BelowFloor,
	AboveCeiling
};

class PolyTransferHeights
{
public:
	PolyTransferHeights(subsector_t *sub);

	subsector_t *Subsector = nullptr;
	sector_t *FrontSector = nullptr;
	PolyWaterFakeSide FakeSide = PolyWaterFakeSide::Center;
	int FloorLightLevel = 0;
	int CeilingLightLevel = 0;

private:
	sector_t tempsec;
};
