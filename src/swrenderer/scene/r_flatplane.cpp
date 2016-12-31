
#include <stdlib.h>
#include <float.h>
#include "templates.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_things.h"
#include "r_sky.h"
#include "stats.h"
#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "r_bsp.h"
#include "r_flatplane.h"
#include "r_segs.h"
#include "r_3dfloors.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "gl/dynlights/gl_dynlight.h"
#include "r_walldraw.h"
#include "r_clip_segment.h"
#include "r_draw_segment.h"
#include "r_portal.h"
#include "r_plane.h"
#include "swrenderer/r_memory.h"

namespace swrenderer
{
	namespace
	{
		double planeheight;
		bool plane_shade;
		int planeshade;
		fixed_t pviewx, pviewy;
		float yslope[MAXHEIGHT];
		fixed_t xscale, yscale;
		double xstepscale, ystepscale;
		double basexfrac, baseyfrac;
	}

	void R_DrawNormalPlane(visplane_t *pl, double _xscale, double _yscale, fixed_t alpha, bool additive, bool masked)
	{
		using namespace drawerargs;

		if (alpha <= 0)
		{
			return;
		}

		double planeang = (pl->xform.Angle + pl->xform.baseAngle).Radians();
		double xstep, ystep, leftxfrac, leftyfrac, rightxfrac, rightyfrac;
		double x;

		xscale = xs_ToFixed(32 - ds_xbits, _xscale);
		yscale = xs_ToFixed(32 - ds_ybits, _yscale);
		if (planeang != 0)
		{
			double cosine = cos(planeang), sine = sin(planeang);
			pviewx = FLOAT2FIXED(pl->xform.xOffs + ViewPos.X * cosine - ViewPos.Y * sine);
			pviewy = FLOAT2FIXED(pl->xform.yOffs - ViewPos.X * sine - ViewPos.Y * cosine);
		}
		else
		{
			pviewx = FLOAT2FIXED(pl->xform.xOffs + ViewPos.X);
			pviewy = FLOAT2FIXED(pl->xform.yOffs - ViewPos.Y);
		}

		pviewx = FixedMul(xscale, pviewx);
		pviewy = FixedMul(yscale, pviewy);

		// left to right mapping
		planeang += (ViewAngle - 90).Radians();

		// Scale will be unit scale at FocalLengthX (normally SCREENWIDTH/2) distance
		xstep = cos(planeang) / FocalLengthX;
		ystep = -sin(planeang) / FocalLengthX;

		// [RH] flip for mirrors
		if (MirrorFlags & RF_XFLIP)
		{
			xstep = -xstep;
			ystep = -ystep;
		}

		planeang += M_PI / 2;
		double cosine = cos(planeang), sine = -sin(planeang);
		x = pl->right - centerx - 0.5;
		rightxfrac = _xscale * (cosine + x * xstep);
		rightyfrac = _yscale * (sine + x * ystep);
		x = pl->left - centerx - 0.5;
		leftxfrac = _xscale * (cosine + x * xstep);
		leftyfrac = _yscale * (sine + x * ystep);

		basexfrac = rightxfrac;
		baseyfrac = rightyfrac;
		xstepscale = (rightxfrac - leftxfrac) / (pl->right - pl->left);
		ystepscale = (rightyfrac - leftyfrac) / (pl->right - pl->left);

		planeheight = fabs(pl->height.Zat0() - ViewPos.Z);

		GlobVis = r_FloorVisibility / planeheight;
		ds_light = 0;
		if (fixedlightlev >= 0)
		{
			R_SetDSColorMapLight(basecolormap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
			plane_shade = false;
		}
		else if (fixedcolormap)
		{
			R_SetDSColorMapLight(fixedcolormap, 0, 0);
			plane_shade = false;
		}
		else
		{
			plane_shade = true;
			planeshade = LIGHT2SHADE(pl->lightlevel);
		}

		if (spanfunc != &SWPixelFormatDrawers::FillSpan)
		{
			if (masked)
			{
				if (alpha < OPAQUE || additive)
				{
					if (!additive)
					{
						spanfunc = &SWPixelFormatDrawers::DrawSpanMaskedTranslucent;
						dc_srcblend = Col2RGB8[alpha >> 10];
						dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
						dc_srcalpha = alpha;
						dc_destalpha = OPAQUE - alpha;
					}
					else
					{
						spanfunc = &SWPixelFormatDrawers::DrawSpanMaskedAddClamp;
						dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
						dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
						dc_srcalpha = alpha;
						dc_destalpha = FRACUNIT;
					}
				}
				else
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanMasked;
				}
			}
			else
			{
				if (alpha < OPAQUE || additive)
				{
					if (!additive)
					{
						spanfunc = &SWPixelFormatDrawers::DrawSpanTranslucent;
						dc_srcblend = Col2RGB8[alpha >> 10];
						dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
						dc_srcalpha = alpha;
						dc_destalpha = OPAQUE - alpha;
					}
					else
					{
						spanfunc = &SWPixelFormatDrawers::DrawSpanAddClamp;
						dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
						dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
						dc_srcalpha = alpha;
						dc_destalpha = FRACUNIT;
					}
				}
				else
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpan;
				}
			}
		}
		R_MapVisPlane(pl, R_MapPlane, R_StepPlane);
	}

	void R_StepPlane()
	{
		basexfrac -= xstepscale;
		baseyfrac -= ystepscale;
	}

	void R_MapPlane(int y, int x1, int x2)
	{
		using namespace drawerargs;

		double distance;

#ifdef RANGECHECK
		if (x2 < x1 || x1<0 || x2 >= viewwidth || (unsigned)y >= (unsigned)viewheight)
		{
			I_FatalError("R_MapPlane: %i, %i at %i", x1, x2, y);
		}
#endif

		// [RH] Notice that I dumped the caching scheme used by Doom.
		// It did not offer any appreciable speedup.

		distance = planeheight * yslope[y];

		if (ds_xbits != 0)
		{
			ds_xstep = xs_ToFixed(32 - ds_xbits, distance * xstepscale);
			ds_xfrac = xs_ToFixed(32 - ds_xbits, distance * basexfrac) + pviewx;
		}
		else
		{
			ds_xstep = 0;
			ds_xfrac = 0;
		}
		if (ds_ybits != 0)
		{
			ds_ystep = xs_ToFixed(32 - ds_ybits, distance * ystepscale);
			ds_yfrac = xs_ToFixed(32 - ds_ybits, distance * baseyfrac) + pviewy;
		}
		else
		{
			ds_ystep = 0;
			ds_yfrac = 0;
		}

		if (r_swtruecolor)
		{
			double distance2 = planeheight * yslope[(y + 1 < viewheight) ? y + 1 : y - 1];
			double xmagnitude = fabs(ystepscale * (distance2 - distance) * FocalLengthX);
			double ymagnitude = fabs(xstepscale * (distance2 - distance) * FocalLengthX);
			double magnitude = MAX(ymagnitude, xmagnitude);
			double min_lod = -1000.0;
			ds_lod = MAX(log2(magnitude) + r_lod_bias, min_lod);
		}

		if (plane_shade)
		{
			// Determine lighting based on the span's distance from the viewer.
			R_SetDSColorMapLight(basecolormap, (float)(GlobVis * fabs(CenterY - y)), planeshade);
		}

		if (r_dynlights)
		{
			// Find row position in view space
			float zspan = (float)(planeheight / (fabs(y + 0.5 - CenterY) / InvZtoScale));
			dc_viewpos.X = (float)((x1 + 0.5 - CenterX) / CenterX * zspan);
			dc_viewpos.Y = zspan;
			dc_viewpos.Z = (float)((CenterY - y - 0.5) / InvZtoScale * zspan);
			dc_viewpos_step.X = (float)(zspan / CenterX);

			static TriLight lightbuffer[64 * 1024];
			static int nextlightindex = 0;

			// Setup lights for column
			dc_num_lights = 0;
			dc_lights = lightbuffer + nextlightindex;
			visplane_light *cur_node = ds_light_list;
			while (cur_node && nextlightindex < 64 * 1024)
			{
				double lightX = cur_node->lightsource->X() - ViewPos.X;
				double lightY = cur_node->lightsource->Y() - ViewPos.Y;
				double lightZ = cur_node->lightsource->Z() - ViewPos.Z;

				float lx = (float)(lightX * ViewSin - lightY * ViewCos);
				float ly = (float)(lightX * ViewTanCos + lightY * ViewTanSin) - dc_viewpos.Y;
				float lz = (float)lightZ - dc_viewpos.Z;

				// Precalculate the constant part of the dot here so the drawer doesn't have to.
				float lconstant = ly * ly + lz * lz;

				// Include light only if it touches this row
				float radius = cur_node->lightsource->GetRadius();
				if (radius * radius >= lconstant)
				{
					uint32_t red = cur_node->lightsource->GetRed();
					uint32_t green = cur_node->lightsource->GetGreen();
					uint32_t blue = cur_node->lightsource->GetBlue();

					nextlightindex++;
					auto &light = dc_lights[dc_num_lights++];
					light.x = lx;
					light.y = lconstant;
					light.radius = 256.0f / radius;
					light.color = (red << 16) | (green << 8) | blue;
				}

				cur_node = cur_node->next;
			}

			if (nextlightindex == 64 * 1024)
				nextlightindex = 0;
		}
		else
		{
			dc_num_lights = 0;
		}

		ds_y = y;
		ds_x1 = x1;
		ds_x2 = x2;

		(R_Drawers()->*spanfunc)();
	}

	void R_DrawColoredPlane(visplane_t *pl)
	{
		R_MapVisPlane(pl, R_MapColoredPlane, nullptr);
	}

	void R_MapColoredPlane(int y, int x1, int x2)
	{
		R_Drawers()->DrawColoredSpan(y, x1, x2);
	}

	void R_SetupPlaneSlope()
	{
		int e, i;

		i = 0;
		e = viewheight;
		float focus = float(FocalLengthY);
		float den;
		float cy = float(CenterY);
		if (i < centery)
		{
			den = cy - i - 0.5f;
			if (e <= centery)
			{
				do {
					yslope[i] = focus / den;
					den -= 1;
				} while (++i < e);
			}
			else
			{
				do {
					yslope[i] = focus / den;
					den -= 1;
				} while (++i < centery);
				den = i - cy + 0.5f;
				do {
					yslope[i] = focus / den;
					den += 1;
				} while (++i < e);
			}
		}
		else
		{
			den = i - cy + 0.5f;
			do {
				yslope[i] = focus / den;
				den += 1;
			} while (++i < e);
		}
	}
}
