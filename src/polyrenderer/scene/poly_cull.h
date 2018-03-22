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

#include "polyrenderer/drawers/poly_triangle.h"
#include <set>
#include <unordered_map>

class PolyCull
{
public:
	void CullScene(const PolyClipPlane &portalClipPlane);

	bool IsLineSegVisible(uint32_t subsectorDepth, uint32_t lineIndex)
	{
		return PvsLineVisible[PvsLineStart[subsectorDepth] + lineIndex];
	}

	std::vector<uint32_t> PvsSubsectors;
	double MaxCeilingHeight = 0.0;
	double MinFloorHeight = 0.0;

	std::vector<uint32_t> SeenSectors;
	std::vector<bool> SectorSeen;
	std::vector<uint32_t> SubsectorDepths;

	static angle_t PointToPseudoAngle(double x, double y);

private:
	struct SolidSegment
	{
		SolidSegment(angle_t start, angle_t end) : Start(start), End(end) { }
		angle_t Start, End;
	};

	void ClearSolidSegments();
	void MarkViewFrustum();

	bool GetAnglesForLine(double x1, double y1, double x2, double y2, angle_t &angle1, angle_t &angle2) const;
	bool IsSegmentCulled(angle_t angle1, angle_t angle2) const;
	void InvertSegments();

	void CullNode(void *node);
	void CullSubsector(subsector_t *sub);
	int PointOnSide(const DVector2 &pos, const node_t *node);

	// Checks BSP node/subtree bounding box.
	// Returns true if some part of the bbox might be visible.
	bool CheckBBox(float *bspcoord);

	void MarkSegmentCulled(angle_t angle1, angle_t angle2);

	FString lastLevelName;

	std::vector<SolidSegment> SolidSegments;
	std::vector<SolidSegment> TempInvertSolidSegments;
	const int SolidCullScale = 3000;
	bool FirstSkyHeight = true;

	PolyClipPlane PortalClipPlane;

	std::vector<uint32_t> PvsLineStart;
	std::vector<bool> PvsLineVisible;
	uint32_t NextPvsLineStart = 0;

	static angle_t AngleToPseudo(angle_t ang);
};
