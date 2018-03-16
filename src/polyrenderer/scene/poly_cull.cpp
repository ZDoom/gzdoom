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
#include "poly_cull.h"
#include "polyrenderer/poly_renderer.h"

void PolyCull::CullScene(const PolyClipPlane &portalClipPlane)
{
	ClearSolidSegments();
	MarkViewFrustum();

	for (uint32_t sub : PvsSubsectors)
		SubsectorDepths[sub] = 0xffffffff;
	SubsectorDepths.resize(level.subsectors.Size(), 0xffffffff);

	for (uint32_t sector : SeenSectors)
		SectorSeen[sector] = false;
	SectorSeen.resize(level.sectors.Size());

	PvsSubsectors.clear();
	SeenSectors.clear();

	NextPvsLineStart = 0;
	PvsLineStart.clear();
	PvsLineVisible.resize(level.segs.Size());

	PortalClipPlane = portalClipPlane;

	// Cull front to back
	FirstSkyHeight = true;
	MaxCeilingHeight = 0.0;
	MinFloorHeight = 0.0;
	if (level.nodes.Size() == 0)
		CullSubsector(&level.subsectors[0]);
	else
		CullNode(level.HeadNode());
}

void PolyCull::CullNode(void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = PointOnSide(PolyRenderer::Instance()->Viewpoint.Pos, bsp);

		// Recursively divide front space (toward the viewer).
		CullNode(bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;

		if (!CheckBBox(bsp->bbox[side]))
			return;

		node = bsp->children[side];
	}

	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
	CullSubsector(sub);
}

void PolyCull::CullSubsector(subsector_t *sub)
{
	// Check if subsector is clipped entirely by the portal clip plane
	bool visible = false;
	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (PortalClipPlane.A * line->v1->fX() + PortalClipPlane.B * line->v1->fY() + PortalClipPlane.D > 0.0)
		{
			visible = true;
			break;
		}
	}
	if (!visible)
		return;

	// Update sky heights for the scene
	if (!FirstSkyHeight)
	{
		MaxCeilingHeight = MAX(MaxCeilingHeight, sub->sector->ceilingplane.Zat0());
		MinFloorHeight = MIN(MinFloorHeight, sub->sector->floorplane.Zat0());
	}
	else
	{
		MaxCeilingHeight = sub->sector->ceilingplane.Zat0();
		MinFloorHeight = sub->sector->floorplane.Zat0();
		FirstSkyHeight = false;
	}

	uint32_t subsectorDepth = (uint32_t)PvsSubsectors.size();

	// Mark that we need to render this
	PvsSubsectors.push_back(sub->Index());
	PvsLineStart.push_back(NextPvsLineStart);

	DVector3 viewpos = PolyRenderer::Instance()->Viewpoint.Pos;

	// Update culling info for further bsp clipping
	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];

		// Skip lines not facing viewer
		DVector2 pt1 = line->v1->fPos() - viewpos;
		DVector2 pt2 = line->v2->fPos() - viewpos;
		if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
		{
			PvsLineVisible[NextPvsLineStart++] = false;
			continue;
		}

		// Skip line if entirely behind portal clipping plane
		if ((PortalClipPlane.A * line->v1->fX() + PortalClipPlane.B * line->v1->fY() + PortalClipPlane.D <= 0.0) ||
			(PortalClipPlane.A * line->v2->fX() + PortalClipPlane.B * line->v2->fY() + PortalClipPlane.D <= 0.0))
		{
			PvsLineVisible[NextPvsLineStart++] = false;
			continue;
		}

		angle_t angle1, angle2;
		bool lineVisible = GetAnglesForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), angle1, angle2);
		if (lineVisible && line->backsector == nullptr)
		{
			MarkSegmentCulled(angle1, angle2);
		}

		// Mark if this line was visible
		PvsLineVisible[NextPvsLineStart++] = lineVisible;
	}

	if (!SectorSeen[sub->sector->Index()])
	{
		SectorSeen[sub->sector->Index()] = true;
		SeenSectors.push_back(sub->sector->Index());
	}

	SubsectorDepths[sub->Index()] = subsectorDepth;
}

void PolyCull::ClearSolidSegments()
{
	SolidSegments.clear();
}

void PolyCull::InvertSegments()
{
	TempInvertSolidSegments.swap(SolidSegments);
	ClearSolidSegments();
	angle_t cur = 0;
	for (const auto &segment : TempInvertSolidSegments)
	{
		if (cur < segment.Start)
			MarkSegmentCulled(cur, segment.Start - 1);
		cur = segment.End + 1;
	}
	if (cur < ANGLE_MAX)
		MarkSegmentCulled(cur, ANGLE_MAX);
}

bool PolyCull::IsSegmentCulled(angle_t startAngle, angle_t endAngle) const
{
	if (startAngle > endAngle)
	{
		return IsSegmentCulled(startAngle, ANGLE_MAX) && IsSegmentCulled(0, endAngle);
	}

	for (const auto &segment : SolidSegments)
	{
		if (startAngle >= segment.Start && endAngle <= segment.End)
			return true;
		else if (endAngle < segment.Start)
			return false;
	}
	return false;
}

void PolyCull::MarkSegmentCulled(angle_t startAngle, angle_t endAngle)
{
	if (startAngle > endAngle)
	{
		MarkSegmentCulled(startAngle, ANGLE_MAX);
		MarkSegmentCulled(0, endAngle);
		return;
	}

	int count = (int)SolidSegments.size();
	int cur = 0;
	while (cur < count)
	{
		if (SolidSegments[cur].Start <= startAngle && SolidSegments[cur].End >= endAngle) // Already fully marked
		{
			return;
		}
		else if (SolidSegments[cur].End >= startAngle && SolidSegments[cur].Start <= endAngle) // Merge segments
		{
			// Find last segment
			int merge = cur;
			while (merge + 1 != count && SolidSegments[merge + 1].Start <= endAngle)
				merge++;

			// Apply new merged range
			SolidSegments[cur].Start = MIN(SolidSegments[cur].Start, startAngle);
			SolidSegments[cur].End = MAX(SolidSegments[merge].End, endAngle);

			// Remove additional segments we merged with
			if (merge > cur)
				SolidSegments.erase(SolidSegments.begin() + (cur + 1), SolidSegments.begin() + (merge + 1));

			return;
		}
		else if (SolidSegments[cur].Start > startAngle) // Insert new segment
		{
			SolidSegments.insert(SolidSegments.begin() + cur, { startAngle, endAngle });
			return;
		}
		cur++;
	}
	SolidSegments.push_back({ startAngle, endAngle });

#if 0
	count = (int)SolidSegments.size();
	for (int i = 1; i < count; i++)
	{
		if (SolidSegments[i - 1].Start >= SolidSegments[i].Start ||
			SolidSegments[i - 1].End >= SolidSegments[i].Start ||
			SolidSegments[i - 1].End + 1 == SolidSegments[i].Start ||
			SolidSegments[i].Start > SolidSegments[i].End)
		{
			I_FatalError("MarkSegmentCulled is broken!");
		}
	}
#endif
}

int PolyCull::PointOnSide(const DVector2 &pos, const node_t *node)
{
	return DMulScale32(FLOAT2FIXED(pos.Y) - node->y, node->dx, node->x - FLOAT2FIXED(pos.X), node->dy) > 0;
}

bool PolyCull::CheckBBox(float *bspcoord)
{
	// Occlusion test using solid segments:
	static const uint8_t checkcoord[12][4] =
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

	// Find the corners of the box that define the edges from current viewpoint.
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	int boxpos = (viewpoint.Pos.X <= bspcoord[BOXLEFT] ? 0 : viewpoint.Pos.X < bspcoord[BOXRIGHT] ? 1 : 2) +
		(viewpoint.Pos.Y >= bspcoord[BOXTOP] ? 0 : viewpoint.Pos.Y > bspcoord[BOXBOTTOM] ? 4 : 8);

	if (boxpos == 5) return true;

	const uint8_t *check = checkcoord[boxpos];
	angle_t angle1 = PointToPseudoAngle(bspcoord[check[0]], bspcoord[check[1]]);
	angle_t angle2 = PointToPseudoAngle(bspcoord[check[2]], bspcoord[check[3]]);

	return !IsSegmentCulled(angle2, angle1);
}

bool PolyCull::GetAnglesForLine(double x1, double y1, double x2, double y2, angle_t &angle1, angle_t &angle2) const
{
	angle2 = PointToPseudoAngle(x1, y1);
	angle1 = PointToPseudoAngle(x2, y2);
	return !IsSegmentCulled(angle1, angle2);
}

void PolyCull::MarkViewFrustum()
{
	// Clips things outside the viewing frustum.
	auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	auto &viewwindow = PolyRenderer::Instance()->Viewwindow;
	double tilt = fabs(viewpoint.Angles.Pitch.Degrees);
	if (tilt > 46.0) // If the pitch is larger than this you can look all around
		return;

	double floatangle = 2.0 + (45.0 + ((tilt / 1.9)))*viewpoint.FieldOfView.Degrees*48.0 / AspectMultiplier(viewwindow.WidescreenRatio) / 90.0;
	angle_t a1 = DAngle(floatangle).BAMs();
	if (a1 < ANGLE_180)
	{
		MarkSegmentCulled(AngleToPseudo(viewpoint.Angles.Yaw.BAMs() + a1), AngleToPseudo(viewpoint.Angles.Yaw.BAMs() - a1));
	}
}

//-----------------------------------------------------------------------------
//
// ! Returns the pseudoangle between the line p1 to (infinity, p1.y) and the 
// line from p1 to p2. The pseudoangle has the property that the ordering of 
// points by true angle around p1 and ordering of points by pseudoangle are the 
// same.
//
// For clipping exact angles are not needed. Only the ordering matters.
// This is about as fast as the fixed point R_PointToAngle2 but without
// the precision issues associated with that function.
//
//-----------------------------------------------------------------------------

angle_t PolyCull::PointToPseudoAngle(double x, double y)
{
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	double vecx = x - viewpoint.Pos.X;
	double vecy = y - viewpoint.Pos.Y;

	if (vecx == 0 && vecy == 0)
	{
		return 0;
	}
	else
	{
		double result = vecy / (fabs(vecx) + fabs(vecy));
		if (vecx < 0)
		{
			result = 2. - result;
		}
		return xs_Fix<30>::ToFix(result);
	}
}

angle_t PolyCull::AngleToPseudo(angle_t ang)
{
	double vecx = cos(ang * M_PI / ANGLE_180);
	double vecy = sin(ang * M_PI / ANGLE_180);

	double result = vecy / (fabs(vecx) + fabs(vecy));
	if (vecx < 0)
	{
		result = 2.f - result;
	}
	return xs_Fix<30>::ToFix(result);
}
