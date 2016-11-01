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

// Transform for a view position and its viewport
//
// World space uses map coordinates in the XY plane. Z is up.
// Eye space means relative to viewer, Y is up and Z is into the screen.
// Viewport means in normalized device coordinates (-1 to 1 range with perspective division). 0,0 is in the center of the viewport and Y is still up.
// Screen means in final screen coordinates. 0,0 is the upper left corner and Y is down. Z is still 1/z.
//
class ViewPosTransform
{
public:
	DVector3 WorldToEye(const DVector3 &worldPoint) const;
	DVector3 WorldToViewport(const DVector3 &worldPoint) const { return EyeToViewport(WorldToEye(worldPoint)); }
	DVector3 WorldToScreen(const DVector3 &worldPoint) const { return EyeToScreen(WorldToEye(worldPoint)); }

	DVector3 EyeToViewport(const DVector3 &eyePoint) const;
	DVector3 EyeToScreen(const DVector3 &eyePoint) const { return ViewportToScreen(EyeToViewport(eyePoint)); }

	DVector3 ViewportToScreen(const DVector3 &viewportPoint) const;

	double ScreenXToEye(int x, double z) const;
	double ScreenYToEye(int y, double z) const;

	double NearZ() const { return 0.0078125; };
};

// Screen space coordinates for a wall
class WallCoords
{
public:
	WallCoords() = default;
	WallCoords(const ViewPosTransform &transform, const DVector2 &v1, const DVector2 &v2, double ceil1, double floor1, double ceil2, double floor2);

	// True if transform and clip culled the wall
	bool Culled = true;

	// Plane for wall in eye space
	DVector3 PlaneNormal;
	double PlaneD = 0.0;

	// Z range of the wall in eye space
	double NearZ = 0.0;
	double FarZ = 0.0;

	// Screen space bounding box of the wall
	int ScreenX1 = 0;
	int ScreenX2 = 0;
	int ScreenY1 = 0;
	int ScreenY2 = 0;

	// Get the Y positions for the given column
	short Y1(int x) const;
	short Y2(int x) const;

	// Get the depth for a column
	double Z(int x) const;

	// Perspective correct interpolation from start to end (used to calculate texture coordinates)
	double VaryingX(int x, double start, double end) const;
	double VaryingY(int x, int y, double start, double end) const;

private:
	static DVector3 Mix(const DVector3 &a, const DVector3 &b, double t);
	static double Mix(double a, double b, double t);

	ViewPosTransform Transform;
	DVector3 ScreenTopLeft;
	DVector3 ScreenTopRight;
	DVector3 ScreenBottomLeft;
	DVector3 ScreenBottomRight;
	double RcpDeltaScreenX = 0.0;
	double VaryingXScale = 1.0;
	double VaryingXOffset = 0.0;
};

// Texture coordinates for a wall
class WallTextureCoords
{
public:
	WallTextureCoords(FTexture *tex, const seg_t *line, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil);

	double u1, u2;
	double v1, v2;

private:
	void CalcU(FTexture *tex, const seg_t *line, side_t::ETexpart texpart);
	void CalcV(FTexture *tex, const seg_t *line, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil);
	void CalcVTopPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double vscale, double yoffset);
	void CalcVMidPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double vscale, double yoffset);
	void CalcVBottomPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double unpeggedceil, double vscale, double yoffset);
};

// Clipping buffers used during rendering
class RenderClipBuffer
{
public:
	void Clear(short left, short right);
	void MarkSegmentCulled(const WallCoords &wallCoords, int drawIndex);
	void ClipVertical(const WallCoords &wallCoords, int drawIndex);
	void ClipTop(const WallCoords &wallCoords, int drawIndex);
	void ClipBottom(const WallCoords &wallCoords, int drawIndex);
	bool IsSegmentCulled(short x1, short x2) const;

	void SetupSpriteClip(short x1, short x2, const DVector3 &pos, bool wallSprite);
	void RenderMaskedWalls();

	short Top[MAXWIDTH];
	short Bottom[MAXWIDTH];

	std::function<void(short x1, short x2, int drawIndex, const short *clipTop, const short *clipBottom)> DrawMaskedWall;

private:
	void AddDrawSegment(short x1, short x2, const WallCoords &wall, bool clipTop, bool clipBottom, int drawIndex);

	struct SolidSegment
	{
		SolidSegment(short x1, short x2) : X1(x1), X2(x2) { }
		short X1, X2;
	};

	struct DrawSegment
	{
		short X1;
		short X2;
		int ClipOffset;
		bool ClipTop;
		bool ClipBottom;
		DVector3 PlaneNormal;
		double PlaneD;
		double NearZ;
		double FarZ;
		int DrawIndex;
	};

	std::vector<SolidSegment> SolidSegments;
	std::vector<DrawSegment> DrawSegments;
	std::vector<short> ClipValues;

	friend class VisibleSegmentsIterator;
};

// Walks the visible segments in a range
class VisibleSegmentsIterator
{
public:
	VisibleSegmentsIterator(const RenderClipBuffer &buffer, short startx, short endx);
	bool Step();

	short X1;
	short X2;

private:
	const std::vector<RenderClipBuffer::SolidSegment> &SolidSegments;
	short endx;
	int next = 0;
};

// Class used to group sector ceilings/floors sharing common properties
class VisiblePlaneKey
{
public:
	VisiblePlaneKey() { }
	VisiblePlaneKey(FTextureID picnum, FSWColormap *colormap, int lightlevel, secplane_t plane, const FTransform &xform) : Picnum(picnum), ColorMap(colormap), LightLevel(lightlevel), Plane(plane), Transform(xform) { }

	bool operator==(const VisiblePlaneKey &other) const
	{
		return Picnum == other.Picnum && LightLevel == other.LightLevel && Plane.fD() == other.Plane.fD();
	}

	FTextureID Picnum;
	FSWColormap *ColorMap;
	int LightLevel;
	secplane_t Plane;
	FTransform Transform;
};

// Visible plane to be rendered
class VisiblePlane
{
public:
	VisiblePlane(const VisiblePlaneKey &key) { Clear(key); }

	void Clear(const VisiblePlaneKey &key)
	{
		Key = key;
		Left = viewwidth;
		Right = 0;
		for (int i = 0; i < MAXWIDTH; i++)
		{
			Top[i] = UnsetValue;
			Bottom[i] = 0;
		}
	}

	VisiblePlaneKey Key;

	enum { UnsetValue = 0x7fff };
	short Left;
	short Right;
	short Top[MAXWIDTH];
	short Bottom[MAXWIDTH];
	std::unique_ptr<VisiblePlane> Next;
};

class RenderVisiblePlane
{
public:
	RenderVisiblePlane(VisiblePlane *plane, FTexture *tex);
	void Step();

	double viewx;
	double viewy;
	double planeheight;
	double basexfrac;
	double baseyfrac;
	double xstepscale;
	double ystepscale;
};

// Tracks plane locations and renders them
class RenderPlanes
{
public:
	void Clear();
	void MarkCeilingPlane(const VisiblePlaneKey &key, const RenderClipBuffer &clip, const WallCoords &wallCoords);
	void MarkFloorPlane(const VisiblePlaneKey &key, const RenderClipBuffer &clip, const WallCoords &wallCoords);
	void Render();

private:
	void RenderPlane(VisiblePlane *plane);
	void RenderSpan(int y, int x1, int x2, const VisiblePlaneKey &key, FTexture *texture, const RenderVisiblePlane &renderInfo);

	VisiblePlane *GetPlaneWithUnsetRange(const VisiblePlaneKey &key, int x0, int x1);
	VisiblePlane *GetPlane(const VisiblePlaneKey &key);
	std::unique_ptr<VisiblePlane> AllocPlane(const VisiblePlaneKey &key);
	static uint32_t Hash(const VisiblePlaneKey &key) { return ((unsigned)((key.Picnum.GetIndex()) * 3 + (key.LightLevel) + (FLOAT2FIXED(key.Plane.fD())) * 7) & (NumBuckets - 1)); }

	enum { NumBuckets = 128 /* must be a power of 2 */ };
	std::unique_ptr<VisiblePlane> PlaneBuckets[NumBuckets];
	std::vector<std::unique_ptr<VisiblePlane>> FreePlanes;
};

// Renders a wall texture
class RenderWall
{
public:
	void Render(const RenderClipBuffer &clip);
	void RenderMasked(short x1, short x2, const short *clipTop, const short *clipBottom);

	WallCoords Coords;
	const seg_t *Line;
	side_t::ETexpart Texpart;
	double TopZ;
	double BottomZ;
	double UnpeggedCeil;
	FSWColormap *Colormap;
	bool Masked;

private:
	FTexture *GetTexture();
	int GetShade();
	float GetLight(short x);
};

// Sprite thing to be rendered
class VisibleSprite
{
public:
	VisibleSprite(AActor *actor, const DVector3 &eyePos);
	void Render(RenderClipBuffer *clip);

private:
	AActor *Actor;
	DVector3 EyePos;

	FTexture *GetSpriteTexture(AActor *thing, /*out*/ bool &flipX);
	visstyle_t GetSpriteVisStyle(AActor *thing, double z);

	friend class RenderBsp; // For sorting
};

// DScreen accelerated sprite to be rendered
class ScreenSprite
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

// Renders a BSP tree in a scene
class RenderBsp
{
public:
	void Render();
	void RenderScreenSprites();

private:
	void RenderNode(void *node);
	void RenderSubsector(subsector_t *sub);
	void AddLine(seg_t *line, sector_t *frontsector);

	void AddSprite(AActor *thing);
	void AddWallSprite(AActor *thing);
	bool IsThingCulled(AActor *thing);
	void RenderMaskedObjects();

	void RenderPlayerSprites();
	void RenderPlayerSprite(DPSprite *sprite, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac);

	int PointOnSide(const DVector2 &pos, const node_t *node);

	// Checks BSP node/subtree bounding box.
	// Returns true if some part of the bbox might be visible.
	bool CheckBBox(float *bspcoord);

	ViewPosTransform Transform;
	RenderClipBuffer Clip;
	RenderPlanes Planes;
	std::vector<VisibleSprite> VisibleSprites;
	std::vector<RenderWall> VisibleMaskedWalls;
	std::vector<ScreenSprite> ScreenSprites;

	const int BaseXCenter = 160;
	const int BaseYCenter = 100;
};
