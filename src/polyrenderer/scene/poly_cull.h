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

#pragma once

#include "polyrenderer/drawers/poly_triangle.h"
#include "polyrenderer/math/poly_intersection.h"

enum class LineSegmentRange
{
	NotVisible,
	HasSegment,
	AlwaysVisible
};

class PolyCull
{
public:
	void ClearSolidSegments();
	void CullScene(const TriMatrix &worldToClip, const Vec4f &portalClipPlane);

	LineSegmentRange GetSegmentRangeForLine(double x1, double y1, double x2, double y2, int &sx1, int &sx2) const;
	void MarkSegmentCulled(int x1, int x2);
	bool IsSegmentCulled(int x1, int x2) const;
	void InvertSegments();

	std::vector<subsector_t *> PvsSectors;
	double MaxCeilingHeight = 0.0;
	double MinFloorHeight = 0.0;

private:
	struct SolidSegment
	{
		SolidSegment(int x1, int x2) : X1(x1), X2(x2) { }
		int X1, X2;
	};

	void CullNode(void *node);
	void CullSubsector(subsector_t *sub);
	int PointOnSide(const DVector2 &pos, const node_t *node);

	// Checks BSP node/subtree bounding box.
	// Returns true if some part of the bbox might be visible.
	bool CheckBBox(float *bspcoord);

	std::vector<SolidSegment> SolidSegments;
	std::vector<SolidSegment> TempInvertSolidSegments;
	const int SolidCullScale = 3000;

	FrustumPlanes frustumPlanes;
	Vec4f PortalClipPlane;
};
