//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//


#pragma once

#include "r_defs.h"

namespace swrenderer
{
	struct DrawSegmentClipInfo;

	struct FWallCoords
	{
		FVector2	tleft;		// coords at left of wall in view space				rx1,ry1
		FVector2	tright;		// coords at right of wall in view space			rx2,ry2

		float		sz1, sz2;	// depth at left, right of wall in screen space		yb1,yb2
		short		sx1, sx2;	// x coords at left, right of wall in screen space	xb1,xb2

		float tx1, tx2; // texture coordinate fractions

		bool Init(RenderThread* thread, const DVector2& pt1, const DVector2& pt2, seg_t* lineseg = nullptr);
	};

	enum class ProjectedWallCull
	{
		Visible,
		OutsideAbove,
		OutsideBelow
	};

	class ProjectedWallLine
	{
	public:
		short ScreenY[MAXWIDTH];

		ProjectedWallCull Project(RenderViewport *viewport, double z1, double z2, const FWallCoords *wallc);
		ProjectedWallCull Project(RenderViewport *viewport, const secplane_t &plane, const FWallCoords *wallc, seg_t *line, bool xflip);
		ProjectedWallCull Project(RenderViewport *viewport, double z, const FWallCoords *wallc);

		void ClipTop(int x1, int x2, const DrawSegmentClipInfo& clip);
		void ClipBottom(int x1, int x2, const DrawSegmentClipInfo& clip);
	};

	class ProjectedWallTexcoords
	{
	public:
		void ProjectTop(RenderViewport* viewport, sector_t* frontsector, sector_t* backsector, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic);
		void ProjectMid(RenderViewport* viewport, sector_t* frontsector, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic);
		void ProjectBottom(RenderViewport* viewport, sector_t* frontsector, sector_t* backsector, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic);
		void ProjectTranslucent(RenderViewport* viewport, sector_t* frontsector, sector_t* backsector, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic);
		void Project3DFloor(RenderViewport* viewport, F3DFloor* rover, seg_t* lineseg, const FWallCoords& WallC, FSoftwareTexture* pic);

		// Gradients
		float upos, ustepX, ustepY;
		float vpos, vstepX, vstepY;
		float wpos, wstepX, wstepY;
		float startX;

	private:
		void Project(RenderViewport* viewport, double walxrepeat, const FWallCoords& WallC, FSoftwareTexture* pic, fixed_t xoffset, double texturemid, float yscale, bool flipx);

		static fixed_t GetXOffset(seg_t* lineseg, FSoftwareTexture* tex, side_t::ETexpart texpart);
		static double GetRowOffset(seg_t* lineseg, FSoftwareTexture* tex, side_t::ETexpart texpart);
		static double GetXScale(side_t* sidedef, FSoftwareTexture* tex, side_t::ETexpart texpart);
		static double GetYScale(side_t* sidedef, FSoftwareTexture* tex, side_t::ETexpart texpart);

		struct Vertex
		{
			float x, y, w;
			float u, v;
		};

		float FindGradientX(float bottomX, float c0, float c1, float c2, const Vertex& v1, const Vertex& v2, const Vertex& v3)
		{
			c0 *= v1.w;
			c1 *= v2.w;
			c2 *= v3.w;
			return ((c1 - c2) * (v1.y - v3.y) - (c0 - c2) * (v2.y - v3.y)) / bottomX;
		}

		float FindGradientY(float bottomY, float c0, float c1, float c2, const Vertex& v1, const Vertex& v2, const Vertex& v3)
		{
			c0 *= v1.w;
			c1 *= v2.w;
			c2 *= v3.w;
			return ((c1 - c2) * (v1.x - v3.x) - (c0 - c2) * (v2.x - v3.x)) / bottomY;
		}
	};

	class ProjectedWallLight
	{
	public:
		int GetLightLevel() const { return lightlevel; }
		bool GetFoggy() const { return foggy; }
		FDynamicColormap *GetBaseColormap() const { return basecolormap; }

		float GetLightPos(int x) const { return lightleft + lightstep * (x - x1); }
		float GetLightStep() const { return lightstep; }
		bool IsSpriteLight() const { return spritelight; }

		void SetColormap(const sector_t *frontsector, seg_t *lineseg, lightlist_t *lit = nullptr);
		void SetLightLeft(RenderThread *thread, const FWallCoords &wallc);
		void SetSpriteLight() { lightleft = 0.0f; lightstep = 0.0f; spritelight = true; }

	private:
		int lightlevel;
		bool foggy;
		FDynamicColormap *basecolormap;
		bool spritelight;

		int x1;
		float lightleft;
		float lightstep;
	};
}
