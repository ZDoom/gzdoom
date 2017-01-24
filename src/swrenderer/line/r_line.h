//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//

#pragma once

#include "vectors.h"
#include "r_wallsetup.h"

struct seg_t;
struct subsector_t;
struct sector_t;
struct side_t;
struct line_t;
struct FDynamicColormap;

namespace swrenderer
{
	struct VisiblePlane;

	struct FWallCoords
	{
		FVector2	tleft;		// coords at left of wall in view space				rx1,ry1
		FVector2	tright;		// coords at right of wall in view space			rx2,ry2

		float		sz1, sz2;	// depth at left, right of wall in screen space		yb1,yb2
		short		sx1, sx2;	// x coords at left, right of wall in screen space	xb1,xb2

		bool Init(const DVector2 &pt1, const DVector2 &pt2, double too_close);
	};

	struct FWallTmapVals
	{
		float		UoverZorg, UoverZstep;
		float		InvZorg, InvZstep;

		void InitFromWallCoords(const FWallCoords *wallc);
		void InitFromLine(const DVector2 &left, const DVector2 &right);
	};

	class SWRenderLine
	{
	public:
		void Render(seg_t *line, subsector_t *subsector, sector_t *sector, sector_t *fakebacksector, VisiblePlane *floorplane, VisiblePlane *ceilingplane, bool foggy, FDynamicColormap *basecolormap);

	private:
		bool RenderWallSegment(int x1, int x2);
		void SetWallVariables(bool needlights);
		void RenderWallSegmentTextures(int x1, int x2);

		bool IsFogBoundary(sector_t *front, sector_t *back) const;
		bool SkyboxCompare(sector_t *frontsector, sector_t *backsector) const;

		subsector_t *InSubsector;
		sector_t *frontsector;
		sector_t *backsector;
		VisiblePlane *floorplane;
		VisiblePlane *ceilingplane;

		seg_t *curline;
		side_t *sidedef;
		line_t *linedef;

		FWallCoords WallC;
		FWallTmapVals WallT;

		double rw_backcz1;
		double rw_backcz2;
		double rw_backfz1;
		double rw_backfz2;
		double rw_frontcz1;
		double rw_frontcz2;
		double rw_frontfz1;
		double rw_frontfz2;

		fixed_t rw_offset_top;
		fixed_t rw_offset_mid;
		fixed_t rw_offset_bottom;

		ProjectedWallCull rw_ceilstat, rw_floorstat;
		bool rw_mustmarkfloor, rw_mustmarkceiling;
		bool rw_prepped;
		bool rw_markportal;
		bool rw_havehigh;
		bool rw_havelow;

		float rw_light;
		float rw_lightstep;
		float rw_lightleft;

		fixed_t rw_offset;
		double rw_midtexturemid;
		double rw_toptexturemid;
		double rw_bottomtexturemid;
		double rw_midtexturescalex;
		double rw_midtexturescaley;
		double rw_toptexturescalex;
		double rw_toptexturescaley;
		double rw_bottomtexturescalex;
		double rw_bottomtexturescaley;

		FTexture *rw_pic;

		bool doorclosed;
		int wallshade;

		bool markfloor; // False if the back side is the same plane.
		bool markceiling;
		FTexture *toptexture;
		FTexture *bottomtexture;
		FTexture *midtexture;

		bool foggy;
		FDynamicColormap *basecolormap;

		double lwallscale;

		ProjectedWallLine walltop;
		ProjectedWallLine wallbottom;
		ProjectedWallLine wallupper;
		ProjectedWallLine walllower;
		ProjectedWallTexcoords walltexcoords;
	};
}
