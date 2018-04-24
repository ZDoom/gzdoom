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

class PolyTranslucentObject;
class PolyDrawLinePortal;
class PolyCull;

class RenderPolyWall
{
public:
	static bool RenderLine(PolyRenderThread *thread, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth, uint32_t stencilValue, std::vector<PolyTranslucentObject*> &translucentWallsOutput, std::vector<std::unique_ptr<PolyDrawLinePortal>> &linePortals, size_t linePortalsStart, line_t *portalEnterLine);
	static void Render3DFloorLine(PolyRenderThread *thread, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth, uint32_t stencilValue, F3DFloor *fakeFloor, std::vector<PolyTranslucentObject*> &translucentWallsOutput);

	void SetCoords(const DVector2 &v1, const DVector2 &v2, double ceil1, double floor1, double ceil2, double floor2);
	void Render(PolyRenderThread *thread);

	DVector2 v1;
	DVector2 v2;
	double ceil1 = 0.0;
	double floor1 = 0.0;
	double ceil2 = 0.0;
	double floor2 = 0.0;

	const seg_t *LineSeg = nullptr;
	const line_t *LineSegLine = nullptr;
	const line_t *Line = nullptr;
	const side_t *Side = nullptr;
	FTexture *Texture = nullptr;
	side_t::ETexpart Wallpart = side_t::mid;
	double TopTexZ = 0.0;
	double BottomTexZ = 0.0;
	double UnpeggedCeil1 = 0.0;
	double UnpeggedCeil2 = 0.0;
	FSWColormap *Colormap = nullptr;
	int SectorLightLevel = 0;
	bool Masked = false;
	bool Additive = false;
	double Alpha = 1.0;
	bool FogBoundary = false;
	uint32_t SubsectorDepth = 0;
	uint32_t StencilValue = 0;
	PolyDrawLinePortal *Polyportal = nullptr;

private:
	void ClampHeight(TriVertex &v1, TriVertex &v2);
	int GetLightLevel();
	void DrawStripes(PolyRenderThread *thread, PolyDrawArgs &args, TriVertex *vertices);

	void SetDynLights(PolyRenderThread *thread, PolyDrawArgs &args);

	static bool IsFogBoundary(sector_t *front, sector_t *back);
	static FTexture *GetTexture(const line_t *Line, const side_t *Side, side_t::ETexpart texpart);
};

class PolyWallTextureCoordsU
{
public:
	PolyWallTextureCoordsU(FTexture *tex, const seg_t *lineseg, const line_t *linesegline, const side_t *side, side_t::ETexpart wallpart);

	double u1, u2;
};

class PolyWallTextureCoordsV
{
public:
	PolyWallTextureCoordsV(FTexture *tex, const line_t *line, const side_t *side, side_t::ETexpart wallpart, double topz, double bottomz, double unpeggedceil, double topTexZ, double bottomTexZ);

	double v1, v2;

private:
	void CalcVTopPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double yoffset);
	void CalcVMidPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double yoffset);
	void CalcVBottomPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double unpeggedceil, double yoffset);
};

class PolyTranslucentWall : public PolyTranslucentObject
{
public:
	PolyTranslucentWall(RenderPolyWall wall) : PolyTranslucentObject(wall.SubsectorDepth, 1e6), wall(wall) { }

	void Render(PolyRenderThread *thread) override
	{
		wall.Render(thread);
	}

	RenderPolyWall wall;
};
