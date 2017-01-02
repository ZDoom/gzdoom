
#include <stdlib.h>
#include <stddef.h>
#include "templates.h"
#include "i_system.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomdata.h"
#include "p_lnspec.h"
#include "r_sky.h"
#include "v_video.h"
#include "m_swap.h"
#include "w_wad.h"
#include "stats.h"
#include "a_sharedglobal.h"
#include "d_net.h"
#include "g_level.h"
#include "r_wallsetup.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "swrenderer/r_main.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/scene/r_bsp.h"
#include "swrenderer/line/r_line.h"

namespace swrenderer
{
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

	int rw_ceilstat, rw_floorstat;
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

	// Transform and clip coordinates. Returns true if it was clipped away
	bool FWallCoords::Init(const DVector2 &pt1, const DVector2 &pt2, double too_close)
	{
		tleft.X = float(pt1.X * ViewSin - pt1.Y * ViewCos);
		tright.X = float(pt2.X * ViewSin - pt2.Y * ViewCos);

		tleft.Y = float(pt1.X * ViewTanCos + pt1.Y * ViewTanSin);
		tright.Y = float(pt2.X * ViewTanCos + pt2.Y * ViewTanSin);

		if (MirrorFlags & RF_XFLIP)
		{
			float t = -tleft.X;
			tleft.X = -tright.X;
			tright.X = t;
			swapvalues(tleft.Y, tright.Y);
		}

		if (tleft.X >= -tleft.Y)
		{
			if (tleft.X > tleft.Y) return true;	// left edge is off the right side
			if (tleft.Y == 0) return true;
			sx1 = xs_RoundToInt(CenterX + tleft.X * CenterX / tleft.Y);
			sz1 = tleft.Y;
		}
		else
		{
			if (tright.X < -tright.Y) return true;	// wall is off the left side
			float den = tleft.X - tright.X - tright.Y + tleft.Y;
			if (den == 0) return true;
			sx1 = 0;
			sz1 = tleft.Y + (tright.Y - tleft.Y) * (tleft.X + tleft.Y) / den;
		}

		if (sz1 < too_close)
			return true;

		if (tright.X <= tright.Y)
		{
			if (tright.X < -tright.Y) return true;	// right edge is off the left side
			if (tright.Y == 0) return true;
			sx2 = xs_RoundToInt(CenterX + tright.X * CenterX / tright.Y);
			sz2 = tright.Y;
		}
		else
		{
			if (tleft.X > tleft.Y) return true;	// wall is off the right side
			float den = tright.Y - tleft.Y - tright.X + tleft.X;
			if (den == 0) return true;
			sx2 = viewwidth;
			sz2 = tleft.Y + (tright.Y - tleft.Y) * (tleft.X - tleft.Y) / den;
		}

		if (sz2 < too_close || sx2 <= sx1)
			return true;

		return false;
	}

	/////////////////////////////////////////////////////////////////////////

	void FWallTmapVals::InitFromWallCoords(const FWallCoords *wallc)
	{
		const FVector2 *left = &wallc->tleft;
		const FVector2 *right = &wallc->tright;

		if (MirrorFlags & RF_XFLIP)
		{
			swapvalues(left, right);
		}
		UoverZorg = left->X * centerx;
		UoverZstep = -left->Y;
		InvZorg = (left->X - right->X) * centerx;
		InvZstep = right->Y - left->Y;
	}

	void FWallTmapVals::InitFromLine(const DVector2 &left, const DVector2 &right)
	{
		// Coordinates should have already had viewx,viewy subtracted

		double fullx1 = left.X * ViewSin - left.Y * ViewCos;
		double fullx2 = right.X * ViewSin - right.Y * ViewCos;
		double fully1 = left.X * ViewTanCos + left.Y * ViewTanSin;
		double fully2 = right.X * ViewTanCos + right.Y * ViewTanSin;

		if (MirrorFlags & RF_XFLIP)
		{
			fullx1 = -fullx1;
			fullx2 = -fullx2;
		}

		UoverZorg = float(fullx1 * centerx);
		UoverZstep = float(-fully1);
		InvZorg = float((fullx1 - fullx2) * centerx);
		InvZstep = float(fully2 - fully1);
	}
}
