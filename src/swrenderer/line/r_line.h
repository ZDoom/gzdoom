
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

	extern FWallCoords WallC;
	extern FWallTmapVals WallT;
}
