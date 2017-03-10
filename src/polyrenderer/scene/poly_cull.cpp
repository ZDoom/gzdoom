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
#include "poly_cull.h"
#include "polyrenderer/poly_renderer.h"

void PolyCull::CullScene(const TriMatrix &worldToClip, const Vec4f &portalClipPlane)
{
	PvsSectors.clear();
	frustumPlanes = FrustumPlanes(worldToClip);
	PortalClipPlane = portalClipPlane;

	// Cull front to back
	MaxCeilingHeight = 0.0;
	MinFloorHeight = 0.0;
	if (numnodes == 0)
		CullSubsector(subsectors);
	else
		CullNode(nodes + numnodes - 1);	// The head node is the last node output.
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

	subsector_t *sub = (subsector_t *)((BYTE *)node - 1);
	CullSubsector(sub);
}

void PolyCull::CullSubsector(subsector_t *sub)
{
	// Update sky heights for the scene
	MaxCeilingHeight = MAX(MaxCeilingHeight, sub->sector->ceilingplane.Zat0());
	MinFloorHeight = MIN(MinFloorHeight, sub->sector->floorplane.Zat0());

	// Mark that we need to render this
	PvsSectors.push_back(sub);

	// Update culling info for further bsp clipping
	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if ((line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ)) && line->backsector == nullptr)
		{
			// Skip lines not facing viewer
			DVector2 pt1 = line->v1->fPos() - ViewPos;
			DVector2 pt2 = line->v2->fPos() - ViewPos;
			if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
				continue;

			int sx1, sx2;
			if (GetSegmentRangeForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), sx1, sx2) == LineSegmentRange::HasSegment)
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

void PolyCull::InvertSegments()
{
	TempInvertSolidSegments.swap(SolidSegments);
	ClearSolidSegments();
	int x = -0x7fff;
	for (const auto &segment : TempInvertSolidSegments)
	{
		MarkSegmentCulled(x, segment.X1 - 1);
		x = segment.X2 + 1;
	}
}

bool PolyCull::IsSegmentCulled(int x1, int x2) const
{
	x1 = clamp(x1, -0x7ffe, 0x7ffd);
	x2 = clamp(x2, -0x7ffd, 0x7ffe);

	int next = 0;
	while (SolidSegments[next].X2 <= x2)
		next++;
	return (x1 >= SolidSegments[next].X1 && x2 <= SolidSegments[next].X2);
}

void PolyCull::MarkSegmentCulled(int x1, int x2)
{
	if (x1 >= x2)
		return;

	x1 = clamp(x1, -0x7ffe, 0x7ffd);
	x2 = clamp(x2, -0x7ffd, 0x7ffe);

	int cur = 0;
	while (true)
	{
		if (SolidSegments[cur].X1 <= x1 && SolidSegments[cur].X2 >= x2) // Already fully marked
		{
			break;
		}
		else if (SolidSegments[cur].X2 >= x1 && SolidSegments[cur].X1 <= x2) // Merge segments
		{
			// Find last segment
			int merge = cur;
			while (merge + 1 != (int)SolidSegments.size() && SolidSegments[merge + 1].X1 <= x2)
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

	// Skip if its in front of the portal:

	if (IntersectionTest::plane_aabb(PortalClipPlane, aabb) == IntersectionTest::outside)
		return false;

	// Occlusion test using solid segments:

	static const int lines[4][4] =
	{
		{ BOXLEFT,  BOXBOTTOM, BOXRIGHT, BOXBOTTOM },
		{ BOXRIGHT, BOXBOTTOM, BOXRIGHT, BOXTOP    },
		{ BOXRIGHT, BOXTOP,    BOXLEFT,  BOXTOP    },
		{ BOXLEFT,  BOXTOP,    BOXLEFT,  BOXBOTTOM }
	};

	bool foundline = false;
	int minsx1, maxsx2;
	for (int i = 0; i < 4; i++)
	{
		int j = i < 3 ? i + 1 : 0;
		float x1 = bspcoord[lines[i][0]];
		float y1 = bspcoord[lines[i][1]];
		float x2 = bspcoord[lines[i][2]];
		float y2 = bspcoord[lines[i][3]];
		int sx1, sx2;
		LineSegmentRange result = GetSegmentRangeForLine(x1, y1, x2, y2, sx1, sx2);
		if (result == LineSegmentRange::HasSegment)
		{
			if (foundline)
			{
				minsx1 = MIN(minsx1, sx1);
				maxsx2 = MAX(maxsx2, sx2);
			}
			else
			{
				minsx1 = sx1;
				maxsx2 = sx2;
				foundline = true;
			}
		}
		else if (result == LineSegmentRange::AlwaysVisible)
		{
			return true;
		}
	}
	if (!foundline)
		return false;
		
	return !IsSegmentCulled(minsx1, maxsx2);
}

LineSegmentRange PolyCull::GetSegmentRangeForLine(double x1, double y1, double x2, double y2, int &sx1, int &sx2) const
{
	double znear = 5.0;
	double updownnear = -400.0;
	double sidenear = 400.0;

	// Clip line to the portal clip plane
	float distance1 = Vec4f::dot(PortalClipPlane, Vec4f((float)x1, (float)y1, 0.0f, 1.0f));
	float distance2 = Vec4f::dot(PortalClipPlane, Vec4f((float)x2, (float)y2, 0.0f, 1.0f));
	if (distance1 < 0.0f && distance2 < 0.0f)
	{
		return LineSegmentRange::NotVisible;
	}
	else if (distance1 < 0.0f || distance2 < 0.0f)
	{
		double t1 = 0.0f, t2 = 1.0f;
		if (distance1 < 0.0f)
			t1 = clamp(distance1 / (distance1 - distance2), 0.0f, 1.0f);
		else
			t2 = clamp(distance2 / (distance1 - distance2), 0.0f, 1.0f);
		double nx1 = x1 * (1.0 - t1) + x2 * t1;
		double ny1 = y1 * (1.0 - t1) + y2 * t1;
		double nx2 = x1 * (1.0 - t2) + x2 * t2;
		double ny2 = y1 * (1.0 - t2) + y2 * t2;
		x1 = nx1;
		x2 = nx2;
		y1 = ny1;
		y2 = ny2;
	}

	// Transform to 2D view space:
	x1 = x1 - ViewPos.X;
	y1 = y1 - ViewPos.Y;
	x2 = x2 - ViewPos.X;
	y2 = y2 - ViewPos.Y;
	double rx1 = x1 * ViewSin - y1 * ViewCos;
	double rx2 = x2 * ViewSin - y2 * ViewCos;
	double ry1 = x1 * ViewCos + y1 * ViewSin;
	double ry2 = x2 * ViewCos + y2 * ViewSin;

	// Is it potentially visible when looking straight up or down?
	if (!(ry1 < updownnear && ry2 < updownnear) && !(ry1 > znear && ry2 > znear) &&
		!(rx1 < -sidenear && rx2 < -sidenear) && !(rx1 > sidenear && rx2 > sidenear))
		return LineSegmentRange::AlwaysVisible;

	// Cull if line is entirely behind view
	if (ry1 < znear && ry2 < znear)
		return LineSegmentRange::NotVisible;

	// Clip line, if needed
	double t1 = 0.0f, t2 = 1.0f;
	if (ry1 < znear)
		t1 = clamp((znear - ry1) / (ry2 - ry1), 0.0, 1.0);
	if (ry2 < znear)
		t2 = clamp((znear - ry2) / (ry2 - ry1), 0.0, 1.0);
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
	return (sx1 != sx2) ? LineSegmentRange::HasSegment : LineSegmentRange::AlwaysVisible;
}
