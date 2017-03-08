/*
**  Handling drawing a wall
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
class Vec4f;

class RenderPolyWall
{
public:
	static bool RenderLine(const TriMatrix &worldToClip, const Vec4f &clipPlane, PolyCull &cull, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth, uint32_t stencilValue, std::vector<PolyTranslucentObject> &translucentWallsOutput, std::vector<std::unique_ptr<PolyDrawLinePortal>> &linePortals);
	static void Render3DFloorLine(const TriMatrix &worldToClip, const Vec4f &clipPlane, PolyCull &cull, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth, uint32_t stencilValue, F3DFloor *fakeFloor, std::vector<PolyTranslucentObject> &translucentWallsOutput);

	void SetCoords(const DVector2 &v1, const DVector2 &v2, double ceil1, double floor1, double ceil2, double floor2);
	void Render(const TriMatrix &worldToClip, const Vec4f &clipPlane, PolyCull &cull);

	DVector2 v1;
	DVector2 v2;
	double ceil1 = 0.0;
	double floor1 = 0.0;
	double ceil2 = 0.0;
	double floor2 = 0.0;

	const seg_t *LineSeg = nullptr;
	const line_t *Line = nullptr;
	const side_t *Side = nullptr;
	side_t::ETexpart Texpart = side_t::mid;
	double TopZ = 0.0;
	double BottomZ = 0.0;
	double UnpeggedCeil = 0.0;
	FSWColormap *Colormap = nullptr;
	bool Masked = false;
	uint32_t SubsectorDepth = 0;
	uint32_t StencilValue = 0;
	PolyDrawLinePortal *Polyportal = nullptr;

private:
	void ClampHeight(TriVertex &v1, TriVertex &v2);
	FTexture *GetTexture();
	int GetLightLevel();
};

// Texture coordinates for a wall
class PolyWallTextureCoords
{
public:
	PolyWallTextureCoords(FTexture *tex, const seg_t *lineseg, const line_t *line, const side_t *side, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil);

	double u1, u2;
	double v1, v2;

private:
	void CalcU(FTexture *tex, const seg_t *lineseg, const line_t *line, const side_t *side, side_t::ETexpart texpart);
	void CalcV(FTexture *tex, const line_t *line, const side_t *side, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil);
	void CalcVTopPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double vscale, double yoffset);
	void CalcVMidPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double vscale, double yoffset);
	void CalcVBottomPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double unpeggedceil, double vscale, double yoffset);
};
