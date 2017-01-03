
#pragma once

namespace swrenderer
{
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

	void R_AddLine(seg_t *line);
	bool R_StoreWallRange(int start, int stop);
	void R_NewWall(bool needlights);
	void R_RenderSegLoop(int x1, int x2);

	bool IsFogBoundary(sector_t *front, sector_t *back);
	bool R_SkyboxCompare(sector_t *frontsector, sector_t *backsector);

	extern subsector_t *InSubsector;
	extern sector_t *frontsector;
	extern sector_t *backsector;

	extern seg_t *curline;
	extern side_t *sidedef;
	extern line_t *linedef;

	extern FWallCoords WallC;
	extern FWallTmapVals WallT;

	extern double rw_backcz1;
	extern double rw_backcz2;
	extern double rw_backfz1;
	extern double rw_backfz2;
	extern double rw_frontcz1;
	extern double rw_frontcz2;
	extern double rw_frontfz1;
	extern double rw_frontfz2;

	extern fixed_t rw_offset_top;
	extern fixed_t rw_offset_mid;
	extern fixed_t rw_offset_bottom;

	extern int rw_ceilstat, rw_floorstat;
	extern bool rw_mustmarkfloor, rw_mustmarkceiling;
	extern bool rw_prepped;
	extern bool rw_markportal;
	extern bool rw_havehigh;
	extern bool rw_havelow;

	extern float rw_light;
	extern float rw_lightstep;
	extern float rw_lightleft;

	extern fixed_t rw_offset;
	extern double rw_midtexturemid;
	extern double rw_toptexturemid;
	extern double rw_bottomtexturemid;
	extern double rw_midtexturescalex;
	extern double rw_midtexturescaley;
	extern double rw_toptexturescalex;
	extern double rw_toptexturescaley;
	extern double rw_bottomtexturescalex;
	extern double rw_bottomtexturescaley;

	extern FTexture *rw_pic;
}
