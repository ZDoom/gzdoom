/*
**  Potential visible set (PVS) handling
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
#include "r_poly_cull.h"
#include "r_poly.h"

void PolyCull::CullScene(const TriMatrix &worldToClip)
{
	ClearSolidSegments();
	PvsSectors.clear();
	frustumPlanes = FrustumPlanes(worldToClip);

	// Cull front to back
	if (numnodes == 0)
	{
		PvsSectors.push_back(subsectors);
		MaxCeilingHeight = subsectors->sector->ceilingplane.Zat0();
		MinFloorHeight = subsectors->sector->floorplane.Zat0();
	}
	else
	{
		MaxCeilingHeight = 0.0;
		MinFloorHeight = 0.0;
		CullNode(nodes + numnodes - 1);	// The head node is the last node output.
	}

	ClearSolidSegments();
}

void PolyCull::CullNode(void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = PointOnSide(ViewPos, bsp);

		// Recursively divide front space (toward the viewer).
		CullNode(bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;
		if (!CheckBBox(bsp->bbox[side]))
			return;

		node = bsp->children[side];
	}

	// Mark that we need to render this
	subsector_t *sub = (subsector_t *)((BYTE *)node - 1);
	MaxCeilingHeight = MAX(MaxCeilingHeight, sub->sector->ceilingplane.Zat0());
	MinFloorHeight = MIN(MinFloorHeight, sub->sector->floorplane.Zat0());
	PvsSectors.push_back(sub);

	// Update culling info for further bsp clipping
	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if ((line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ)) && line->backsector == nullptr)
		{
			int sx1, sx2;
			if (GetSegmentRangeForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), sx1, sx2))
			{
				MarkSegmentCulled(sx1, sx2);
			}
		}
	}
}

void PolyCull::ClearSolidSegments()
{
	SolidSegments.clear();
	SolidSegments.reserve(SolidCullScale + 2);
	SolidSegments.push_back({ -0x7fff, -SolidCullScale });
	SolidSegments.push_back({ SolidCullScale , 0x7fff });
}

bool PolyCull::IsSegmentCulled(int x1, int x2) const
{
	int next = 0;
	while (SolidSegments[next].X2 <= x2)
		next++;
	return (x1 >= SolidSegments[next].X1 && x2 <= SolidSegments[next].X2);
}

void PolyCull::MarkSegmentCulled(int x1, int x2)
{
	if (x1 >= x2)
		return;

	int cur = 1;
	while (true)
	{
		if (SolidSegments[cur].X1 <= x1 && SolidSegments[cur].X2 >= x2) // Already fully marked
		{
			break;
		}
		else if (cur + 1 != SolidSegments.size() && SolidSegments[cur].X2 >= x1 && SolidSegments[cur].X1 <= x2) // Merge segments
		{
			// Find last segment
			int merge = cur;
			while (merge + 2 != SolidSegments.size() && SolidSegments[merge + 1].X1 <= x2)
				merge++;

			// Apply new merged range
			SolidSegments[cur].X1 = MIN(SolidSegments[cur].X1, x1);
			SolidSegments[cur].X2 = MAX(SolidSegments[merge].X2, x2);

			// Remove additional segments we merged with
			if (merge > cur)
				SolidSegments.erase(SolidSegments.begin() + (cur + 1), SolidSegments.begin() + (merge + 1));

			break;
		}
		else if (SolidSegments[cur].X1 > x1) // Insert new segment
		{
			SolidSegments.insert(SolidSegments.begin() + cur, { x1, x2 });
			break;
		}
		cur++;
	}
}

int PolyCull::PointOnSide(const DVector2 &pos, const node_t *node)
{
	return DMulScale32(FLOAT2FIXED(pos.Y) - node->y, node->dx, node->x - FLOAT2FIXED(pos.X), node->dy) > 0;
}

bool PolyCull::CheckBBox(float *bspcoord)
{
	// Start using a quick frustum AABB test:

	AxisAlignedBoundingBox aabb(Vec3f(bspcoord[BOXLEFT], bspcoord[BOXBOTTOM], (float)ViewPos.Z - 1000.0f), Vec3f(bspcoord[BOXRIGHT], bspcoord[BOXTOP], (float)ViewPos.Z + 1000.0f));
	auto result = IntersectionTest::frustum_aabb(frustumPlanes, aabb);
	if (result == IntersectionTest::outside)
		return false;

	// Occlusion test using solid segments:

	int 				boxx;
	int 				boxy;
	int 				boxpos;

	double	 			x1, y1, x2, y2;

	// Find the corners of the box
	// that define the edges from current viewpoint.
	if (ViewPos.X <= bspcoord[BOXLEFT])
		boxx = 0;
	else if (ViewPos.X < bspcoord[BOXRIGHT])
		boxx = 1;
	else
		boxx = 2;

	if (ViewPos.Y >= bspcoord[BOXTOP])
		boxy = 0;
	else if (ViewPos.Y > bspcoord[BOXBOTTOM])
		boxy = 1;
	else
		boxy = 2;

	boxpos = (boxy << 2) + boxx;
	if (boxpos == 5)
		return true;

	static const int checkcoord[12][4] =
	{
		{ 3,0,2,1 },
		{ 3,0,2,0 },
		{ 3,1,2,0 },
		{ 0 },
		{ 2,0,2,1 },
		{ 0,0,0,0 },
		{ 3,1,3,0 },
		{ 0 },
		{ 2,0,3,1 },
		{ 2,1,3,1 },
		{ 2,1,3,0 }
	};

	x1 = bspcoord[checkcoord[boxpos][0]];
	y1 = bspcoord[checkcoord[boxpos][1]];
	x2 = bspcoord[checkcoord[boxpos][2]];
	y2 = bspcoord[checkcoord[boxpos][3]];

	int sx1, sx2;
	if (GetSegmentRangeForLine(x1, y1, x2, y2, sx1, sx2))
		return !IsSegmentCulled(sx1, sx2);
	else
		return true;
}

bool PolyCull::GetSegmentRangeForLine(double x1, double y1, double x2, double y2, int &sx1, int &sx2) const
{
	double znear = 5.0;

	// Transform to 2D view space:
	x1 = x1 - ViewPos.X;
	y1 = y1 - ViewPos.Y;
	x2 = x2 - ViewPos.X;
	y2 = y2 - ViewPos.Y;
	double rx1 = x1 * ViewSin - y1 * ViewCos;
	double rx2 = x2 * ViewSin - y2 * ViewCos;
	double ry1 = x1 * ViewCos + y1 * ViewSin;
	double ry2 = x2 * ViewCos + y2 * ViewSin;

	// Cull if line is entirely behind view
	if (ry1 < znear && ry2 < znear) return false;

	// Clip line, if needed
	double t1 = 0.0f, t2 = 1.0f;
	if (ry1 < znear)
		t1 = clamp((znear - ry1) / (ry2 - ry1), 0.0, 1.0);
	if (ry2 < znear)
		t2 = clamp((znear - ry1) / (ry2 - ry1), 0.0, 1.0);
	if (t1 != 0.0 || t2 != 1.0)
	{
		double nx1 = rx1 * (1.0 - t1) + rx2 * t1;
		double ny1 = ry1 * (1.0 - t1) + ry2 * t1;
		double nx2 = rx1 * (1.0 - t2) + rx2 * t2;
		double ny2 = ry1 * (1.0 - t2) + ry2 * t2;
		rx1 = nx1;
		rx2 = nx2;
		ry1 = ny1;
		ry2 = ny2;
	}

	sx1 = (int)floor(clamp(rx1 / ry1 * (SolidCullScale / 3), (double)-SolidCullScale, (double)SolidCullScale));
	sx2 = (int)floor(clamp(rx2 / ry2 * (SolidCullScale / 3), (double)-SolidCullScale, (double)SolidCullScale));

	if (sx1 > sx2)
		std::swap(sx1, sx2);
	return sx1 != sx2;
}
