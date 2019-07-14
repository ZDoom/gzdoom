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

#include "poly_scene.h"

struct PolyPortalVertexRange
{
	PolyPortalVertexRange(const TriVertex *vertices, int count) : Vertices(vertices), Count(count) { }
	const TriVertex *Vertices;
	int Count;
};

class PolyPortalSegment
{
public:
	PolyPortalSegment(angle_t start, angle_t end) : Start(start), End(end) { }
	angle_t Start, End;
};

class PolyDrawSectorPortal
{
public:
	PolyDrawSectorPortal(FSectorPortal *portal, bool ceiling);

	void Render(int portalDepth);
	
	FSectorPortal *Portal = nullptr;
	uint32_t StencilValue = 0;
	std::vector<PolyPortalVertexRange> Shape;

private:
	void SaveGlobals();
	void RestoreGlobals();

	bool Ceiling;
	PolyPortalViewpoint PortalViewpoint;
	
	FRenderViewpoint SavedViewpoint;
	bool SavedInvisibility;
};

class PolyDrawLinePortal
{
public:
	PolyDrawLinePortal(FLinePortal *portal);
	PolyDrawLinePortal(line_t *mirror);

	void Render(int portalDepth);

	FLinePortal *Portal = nullptr;
	line_t *Mirror = nullptr;
	uint32_t StencilValue = 0;
	std::vector<PolyPortalVertexRange> Shape;

private:
	void SaveGlobals();
	void RestoreGlobals();

	PolyPortalViewpoint PortalViewpoint;

	FRenderViewpoint SavedViewpoint;
	bool SavedInvisibility;
};
