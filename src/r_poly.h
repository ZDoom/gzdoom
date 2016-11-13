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
#include "r_poly_triangle.h"
#include "r_poly_intersection.h"

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

// Used for sorting things by distance to the camera
class PolySortedSprite
{
public:
	PolySortedSprite(AActor *thing, double distanceSquared) : Thing(thing), DistanceSquared(distanceSquared) { }
	bool operator<(const PolySortedSprite &other) const { return DistanceSquared < other.DistanceSquared; }

	AActor *Thing;
	double DistanceSquared;
};

class PolySubsectoredSprite
{
public:
	PolySubsectoredSprite(AActor *thing, subsector_t *sub, uint32_t subsectorDepth) : thing(thing), sub(sub), subsectorDepth(subsectorDepth) { }
	AActor *thing;
	subsector_t *sub;
	uint32_t subsectorDepth;
};

class SpriteRange
{
public:
	SpriteRange() = default;
	SpriteRange(int start, int count) : Start(start), Count(count) { }
	int Start = -1;
	int Count = 0;
};

class PolySkyDome
{
public:
	PolySkyDome() { CreateDome(); }
	void Render(const TriMatrix &worldToClip);

private:
	TArray<TriVertex> mVertices;
	TArray<unsigned int> mPrimStart;
	int mRows, mColumns;

	void SkyVertex(int r, int c, bool yflip);
	void CreateSkyHemisphere(bool zflip);
	void CreateDome();
	void RenderRow(PolyDrawArgs &args, int row);
	void RenderCapColorRow(PolyDrawArgs &args, FTexture *skytex, int row, bool bottomCap);

	TriVertex SetVertex(float xx, float yy, float zz, float uu = 0, float vv = 0);
	TriVertex SetVertexXYZ(float xx, float yy, float zz, float uu = 0, float vv = 0);
};

// Renders a GL BSP tree in a scene
class RenderPolyBsp
{
public:
	void Render();
	void RenderScreenSprites();

	static const uint32_t SkySubsectorDepth = 0x7fffffff;

private:
	void RenderNode(void *node);
	void RenderSubsector(subsector_t *sub);
	void RenderPlane(subsector_t *sub, uint32_t subsectorDepth, bool ceiling);
	void AddLine(seg_t *line, sector_t *frontsector, uint32_t subsectorDepth);
	TriVertex PlaneVertex(vertex_t *v1, sector_t *sector, double height);

	void RenderSprites();
	void AddSprite(AActor *thing, subsector_t *sub, uint32_t subsectorDepth);
	void AddWallSprite(AActor *thing, subsector_t *sub, uint32_t subsectorDepth);
	bool IsThingCulled(AActor *thing);
	visstyle_t GetSpriteVisStyle(AActor *thing, double z);
	FTexture *GetSpriteTexture(AActor *thing, /*out*/ bool &flipX);
	SpriteRange GetSpritesForSector(sector_t *sector);

	void RenderPlayerSprites();
	void RenderPlayerSprite(DPSprite *sprite, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac);

	int PointOnSide(const DVector2 &pos, const node_t *node);

	// Checks BSP node/subtree bounding box.
	// Returns true if some part of the bbox might be visible.
	bool CheckBBox(float *bspcoord);

	bool GetSegmentRangeForLine(double x1, double y1, double x2, double y2, int &sx1, int &sx2) const;

	void MarkSegmentCulled(int x1, int x2);
	bool IsSegmentCulled(int x1, int x2) const;

	std::vector<subsector_t *> PvsSectors;
	uint32_t NextSubsectorDepth = 0;
	double MaxCeilingHeight = 0.0;
	double MinFloorHeight = 0.0;

	TriMatrix worldToClip;
	FrustumPlanes frustumPlanes;

	std::vector<SpriteRange> SectorSpriteRanges;
	std::vector<PolySortedSprite> SortedSprites;
	std::vector<PolySubsectoredSprite> SubsectoredSprites;

	std::vector<PolyScreenSprite> ScreenSprites;

	const int BaseXCenter = 160;
	const int BaseYCenter = 100;

	struct SolidSegment
	{
		SolidSegment(int x1, int x2) : X1(x1), X2(x2) { }
		int X1, X2;
	};

	std::vector<SolidSegment> SolidSegments;
	const int SolidCullScale = 3000;

	PolySkyDome skydome;
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
	uint32_t SubsectorDepth = 0;

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
