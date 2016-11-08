/*
**  Experimental Doom software renderer
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
#include "r_main.h"
#include "r_triangle.h"

// DScreen accelerated sprite to be rendered
class PolyScreenSprite
{
public:
	void Render();

	FTexture *Pic = nullptr;
	double X1 = 0.0;
	double Y1 = 0.0;
	double Width = 0.0;
	double Height = 0.0;
	FRemapTable *Translation = nullptr;
	bool Flip = false;
	visstyle_t visstyle;
	uint32_t FillColor = 0;
	FDynamicColormap *Colormap = nullptr;
};

// Renders a GL BSP tree in a scene
class RenderPolyBsp
{
public:
	void Render();
	void RenderScreenSprites();

private:
	void RenderNode(void *node);
	void RenderSubsector(subsector_t *sub);
	void AddLine(seg_t *line, sector_t *frontsector);
	TriVertex PlaneVertex(vertex_t *v1, sector_t *sector, const secplane_t &plane);

	void AddSprite(AActor *thing, subsector_t *sub);
	void AddWallSprite(AActor *thing, subsector_t *sub);
	bool IsThingCulled(AActor *thing);
	visstyle_t GetSpriteVisStyle(AActor *thing, double z);
	FTexture *GetSpriteTexture(AActor *thing, /*out*/ bool &flipX);

	void RenderPlayerSprites();
	void RenderPlayerSprite(DPSprite *sprite, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac);

	int PointOnSide(const DVector2 &pos, const node_t *node);

	// Checks BSP node/subtree bounding box.
	// Returns true if some part of the bbox might be visible.
	bool CheckBBox(float *bspcoord);

	std::vector<subsector_t *> PvsSectors;
	TriMatrix worldToClip;

	std::vector<PolyScreenSprite> ScreenSprites;

	const int BaseXCenter = 160;
	const int BaseYCenter = 100;
};

class RenderPolyWall
{
public:
	void Render(const TriMatrix &worldToClip);

	void SetCoords(const DVector2 &v1, const DVector2 &v2, double ceil1, double floor1, double ceil2, double floor2)
	{
		this->v1 = v1;
		this->v2 = v2;
		this->ceil1 = ceil1;
		this->floor1 = floor1;
		this->ceil2 = ceil2;
		this->floor2 = floor2;
	}

	DVector2 v1;
	DVector2 v2;
	double ceil1 = 0.0;
	double floor1 = 0.0;
	double ceil2 = 0.0;
	double floor2 = 0.0;

	const seg_t *Line = nullptr;
	side_t::ETexpart Texpart = side_t::mid;
	double TopZ = 0.0;
	double BottomZ = 0.0;
	double UnpeggedCeil = 0.0;
	FSWColormap *Colormap = nullptr;
	bool Masked = false;

private:
	FTexture *GetTexture();
	int GetLightLevel();
};

// Texture coordinates for a wall
class PolyWallTextureCoords
{
public:
	PolyWallTextureCoords(FTexture *tex, const seg_t *line, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil);

	double u1, u2;
	double v1, v2;

private:
	void CalcU(FTexture *tex, const seg_t *line, side_t::ETexpart texpart);
	void CalcV(FTexture *tex, const seg_t *line, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil);
	void CalcVTopPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double vscale, double yoffset);
	void CalcVMidPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double vscale, double yoffset);
	void CalcVBottomPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double unpeggedceil, double vscale, double yoffset);
};

class PolyVertexBuffer
{
public:
	static TriVertex *GetVertices(int count);
	static void Clear();
};
