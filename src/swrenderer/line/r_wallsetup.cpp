
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
#include "r_walldraw.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/line/r_line.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"

namespace swrenderer
{
	ProjectedWallCull ProjectedWallLine::Project(double z, const FWallCoords *wallc)
	{
		return Project(z, z, wallc);
	}

	ProjectedWallCull ProjectedWallLine::Project(double z1, double z2, const FWallCoords *wallc)
	{
		auto viewport = RenderViewport::Instance();
		
		float y1 = (float)(viewport->CenterY - z1 * viewport->InvZtoScale / wallc->sz1);
		float y2 = (float)(viewport->CenterY - z2 * viewport->InvZtoScale / wallc->sz2);

		if (y1 < 0 && y2 < 0) // entire line is above screen
		{
			memset(&ScreenY[wallc->sx1], 0, (wallc->sx2 - wallc->sx1) * sizeof(ScreenY[0]));
			return ProjectedWallCull::OutsideAbove;
		}
		else if (y1 > viewheight && y2 > viewheight) // entire line is below screen
		{
			fillshort(&ScreenY[wallc->sx1], wallc->sx2 - wallc->sx1, viewheight);
			return ProjectedWallCull::OutsideBelow;
		}

		if (wallc->sx2 <= wallc->sx1)
			return ProjectedWallCull::Visible;

		float rcp_delta = 1.0f / (wallc->sx2 - wallc->sx1);
		if (y1 >= 0.0f && y2 >= 0.0f && xs_RoundToInt(y1) <= viewheight && xs_RoundToInt(y2) <= viewheight)
		{
			for (int x = wallc->sx1; x < wallc->sx2; x++)
			{
				float t = (x - wallc->sx1) * rcp_delta;
				float y = y1 * (1.0f - t) + y2 * t;
				ScreenY[x] = (short)xs_RoundToInt(y);
			}
		}
		else
		{
			for (int x = wallc->sx1; x < wallc->sx2; x++)
			{
				float t = (x - wallc->sx1) * rcp_delta;
				float y = y1 * (1.0f - t) + y2 * t;
				ScreenY[x] = (short)clamp(xs_RoundToInt(y), 0, viewheight);
			}
		}

		return ProjectedWallCull::Visible;
	}

	ProjectedWallCull ProjectedWallLine::Project(const secplane_t &plane, const FWallCoords *wallc, seg_t *curline, bool xflip)
	{
		if (!plane.isSlope())
		{
			return Project(plane.Zat0() - ViewPos.Z, wallc);
		}
		else
		{
			// Get Z coordinates at both ends of the line
			double x, y, den, z1, z2;
			if (xflip)
			{
				x = curline->v2->fX();
				y = curline->v2->fY();
				if (wallc->sx1 == 0 && 0 != (den = wallc->tleft.X - wallc->tright.X + wallc->tleft.Y - wallc->tright.Y))
				{
					double frac = (wallc->tleft.Y + wallc->tleft.X) / den;
					x -= frac * (x - curline->v1->fX());
					y -= frac * (y - curline->v1->fY());
				}
				z1 = plane.ZatPoint(x, y) - ViewPos.Z;

				if (wallc->sx2 > wallc->sx1 + 1)
				{
					x = curline->v1->fX();
					y = curline->v1->fY();
					if (wallc->sx2 == viewwidth && 0 != (den = wallc->tleft.X - wallc->tright.X - wallc->tleft.Y + wallc->tright.Y))
					{
						double frac = (wallc->tright.Y - wallc->tright.X) / den;
						x += frac * (curline->v2->fX() - x);
						y += frac * (curline->v2->fY() - y);
					}
					z2 = plane.ZatPoint(x, y) - ViewPos.Z;
				}
				else
				{
					z2 = z1;
				}
			}
			else
			{
				x = curline->v1->fX();
				y = curline->v1->fY();
				if (wallc->sx1 == 0 && 0 != (den = wallc->tleft.X - wallc->tright.X + wallc->tleft.Y - wallc->tright.Y))
				{
					double frac = (wallc->tleft.Y + wallc->tleft.X) / den;
					x += frac * (curline->v2->fX() - x);
					y += frac * (curline->v2->fY() - y);
				}
				z1 = plane.ZatPoint(x, y) - ViewPos.Z;

				if (wallc->sx2 > wallc->sx1 + 1)
				{
					x = curline->v2->fX();
					y = curline->v2->fY();
					if (wallc->sx2 == viewwidth && 0 != (den = wallc->tleft.X - wallc->tright.X - wallc->tleft.Y + wallc->tright.Y))
					{
						double frac = (wallc->tright.Y - wallc->tright.X) / den;
						x -= frac * (x - curline->v1->fX());
						y -= frac * (y - curline->v1->fY());
					}
					z2 = plane.ZatPoint(x, y) - ViewPos.Z;
				}
				else
				{
					z2 = z1;
				}
			}

			return Project(z1, z2, wallc);
		}
	}

	/////////////////////////////////////////////////////////////////////////

	void ProjectedWallTexcoords::Project(double walxrepeat, int x1, int x2, const FWallTmapVals &WallT)
	{
		auto viewport = RenderViewport::Instance();
		
		float uOverZ = WallT.UoverZorg + WallT.UoverZstep * (float)(x1 + 0.5 - viewport->CenterX);
		float invZ = WallT.InvZorg + WallT.InvZstep * (float)(x1 + 0.5 - viewport->CenterX);
		float uGradient = WallT.UoverZstep;
		float zGradient = WallT.InvZstep;
		float xrepeat = (float)fabs(walxrepeat);
		float depthScale = (float)(WallT.InvZstep * viewport->WallTMapScale2);
		float depthOrg = (float)(-WallT.UoverZstep * viewport->WallTMapScale2);

		if (walxrepeat < 0.0)
		{
			for (int x = x1; x < x2; x++)
			{
				float u = uOverZ / invZ;

				UPos[x] = (fixed_t)((xrepeat - u * xrepeat) * FRACUNIT);
				VStep[x] = depthOrg + u * depthScale;

				uOverZ += uGradient;
				invZ += zGradient;
			}
		}
		else
		{
			for (int x = x1; x < x2; x++)
			{
				float u = uOverZ / invZ;

				UPos[x] = (fixed_t)(u * xrepeat * FRACUNIT);
				VStep[x] = depthOrg + u * depthScale;

				uOverZ += uGradient;
				invZ += zGradient;
			}
		}
	}

	void ProjectedWallTexcoords::ProjectPos(double walxrepeat, int x1, int x2, const FWallTmapVals &WallT)
	{
		auto viewport = RenderViewport::Instance();
		
		float uOverZ = WallT.UoverZorg + WallT.UoverZstep * (float)(x1 + 0.5 - viewport->CenterX);
		float invZ = WallT.InvZorg + WallT.InvZstep * (float)(x1 + 0.5 - viewport->CenterX);
		float uGradient = WallT.UoverZstep;
		float zGradient = WallT.InvZstep;
		float xrepeat = (float)fabs(walxrepeat);

		if (walxrepeat < 0.0f)
		{
			for (int x = x1; x < x2; x++)
			{
				float u = uOverZ / invZ * xrepeat - xrepeat;

				UPos[x] = (fixed_t)(u * FRACUNIT);

				uOverZ += uGradient;
				invZ += zGradient;
			}
		}
		else
		{
			for (int x = x1; x < x2; x++)
			{
				float u = uOverZ / invZ * xrepeat;

				UPos[x] = (fixed_t)(u * FRACUNIT);

				uOverZ += uGradient;
				invZ += zGradient;
			}
		}
	}
}
