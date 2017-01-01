
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
#include "swrenderer/scene/r_bsp.h"
#include "r_skyplane.h"
#include "swrenderer/scene/r_segs.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "gl/dynlights/gl_dynlight.h"
#include "swrenderer/segments/r_clipsegment.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/r_memory.h"

CVAR(Bool, r_linearsky, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
EXTERN_CVAR(Int, r_skymode)

namespace swrenderer
{
	namespace
	{
		FTexture *frontskytex, *backskytex;
		angle_t skyflip;
		int frontpos, backpos;
		double frontyScale;
		fixed_t frontcyl, backcyl;
		double skymid;
		angle_t skyangle;
		double frontiScale;

		// Allow for layer skies up to 512 pixels tall. This is overkill,
		// since the most anyone can ever see of the sky is 500 pixels.
		// We need 4 skybufs because R_DrawSkySegment can draw up to 4 columns at a time.
		// Need two versions - one for true color and one for palette
		#define MAXSKYBUF 3072
		uint8_t skybuf[4][512];
		uint32_t skybuf_bgra[MAXSKYBUF][512];
		uint32_t lastskycol[4];
		uint32_t lastskycol_bgra[MAXSKYBUF];
		int skycolplace;
		int skycolplace_bgra;
	}

	void R_DrawSkyPlane(visplane_t *pl)
	{
		FTextureID sky1tex, sky2tex;
		double frontdpos = 0, backdpos = 0;

		if ((level.flags & LEVEL_SWAPSKIES) && !(level.flags & LEVEL_DOUBLESKY))
		{
			sky1tex = sky2texture;
		}
		else
		{
			sky1tex = sky1texture;
		}
		sky2tex = sky2texture;
		skymid = skytexturemid;
		skyangle = ViewAngle.BAMs();

		if (pl->picnum == skyflatnum)
		{
			if (!(pl->sky & PL_SKYFLAT))
			{	// use sky1
			sky1:
				frontskytex = TexMan(sky1tex, true);
				if (level.flags & LEVEL_DOUBLESKY)
					backskytex = TexMan(sky2tex, true);
				else
					backskytex = NULL;
				skyflip = 0;
				frontdpos = sky1pos;
				backdpos = sky2pos;
				frontcyl = sky1cyl;
				backcyl = sky2cyl;
			}
			else if (pl->sky == PL_SKYFLAT)
			{	// use sky2
				frontskytex = TexMan(sky2tex, true);
				backskytex = NULL;
				frontcyl = sky2cyl;
				skyflip = 0;
				frontdpos = sky2pos;
			}
			else
			{	// MBF's linedef-controlled skies
				// Sky Linedef
				const line_t *l = &lines[(pl->sky & ~PL_SKYFLAT) - 1];

				// Sky transferred from first sidedef
				const side_t *s = l->sidedef[0];
				int pos;

				// Texture comes from upper texture of reference sidedef
				// [RH] If swapping skies, then use the lower sidedef
				if (level.flags & LEVEL_SWAPSKIES && s->GetTexture(side_t::bottom).isValid())
				{
					pos = side_t::bottom;
				}
				else
				{
					pos = side_t::top;
				}

				frontskytex = TexMan(s->GetTexture(pos), true);
				if (frontskytex == NULL || frontskytex->UseType == FTexture::TEX_Null)
				{ // [RH] The blank texture: Use normal sky instead.
					goto sky1;
				}
				backskytex = NULL;

				// Horizontal offset is turned into an angle offset,
				// to allow sky rotation as well as careful positioning.
				// However, the offset is scaled very small, so that it
				// allows a long-period of sky rotation.
				skyangle += FLOAT2FIXED(s->GetTextureXOffset(pos));

				// Vertical offset allows careful sky positioning.
				skymid = s->GetTextureYOffset(pos) - 28;

				// We sometimes flip the picture horizontally.
				//
				// Doom always flipped the picture, so we make it optional,
				// to make it easier to use the new feature, while to still
				// allow old sky textures to be used.
				skyflip = l->args[2] ? 0u : ~0u;

				int frontxscale = int(frontskytex->Scale.X * 1024);
				frontcyl = MAX(frontskytex->GetWidth(), frontxscale);
				if (skystretch)
				{
					skymid = skymid * frontskytex->GetScaledHeightDouble() / SKYSTRETCH_HEIGHT;
				}
			}
		}
		frontpos = int(fmod(frontdpos, sky1cyl * 65536.0));
		if (backskytex != NULL)
		{
			backpos = int(fmod(backdpos, sky2cyl * 65536.0));
		}

		bool fakefixed = false;
		if (fixedcolormap)
		{
			R_SetColorMapLight(fixedcolormap, 0, 0);
		}
		else
		{
			fakefixed = true;
			fixedcolormap = &NormalLight;
			R_SetColorMapLight(fixedcolormap, 0, 0);
		}

		R_DrawSky(pl);

		if (fakefixed)
			fixedcolormap = NULL;
	}


	// Get a column of sky when there is only one sky texture.
	const uint8_t *R_GetOneSkyColumn(FTexture *fronttex, int x)
	{
		int tx;
		if (r_linearsky)
		{
			angle_t xangle = (angle_t)((0.5 - x / (double)viewwidth) * FocalTangent * ANGLE_90);
			angle_t column = (skyangle + xangle) ^ skyflip;
			tx = (UMulScale16(column, frontcyl) + frontpos) >> FRACBITS;
		}
		else
		{
			angle_t column = (skyangle + xtoviewangle[x]) ^ skyflip;
			tx = (UMulScale16(column, frontcyl) + frontpos) >> FRACBITS;
		}

		if (!r_swtruecolor)
			return fronttex->GetColumn(tx, NULL);
		else
		{
			return (const uint8_t *)fronttex->GetColumnBgra(tx, NULL);
		}
	}

	// Get a column of sky when there are two overlapping sky textures
	const uint8_t *R_GetTwoSkyColumns(FTexture *fronttex, int x)
	{
		uint32_t ang, angle1, angle2;

		if (r_linearsky)
		{
			angle_t xangle = (angle_t)((0.5 - x / (double)viewwidth) * FocalTangent * ANGLE_90);
			ang = (skyangle + xangle) ^ skyflip;
		}
		else
		{
			ang = (skyangle + xtoviewangle[x]) ^ skyflip;
		}
		angle1 = (uint32_t)((UMulScale16(ang, frontcyl) + frontpos) >> FRACBITS);
		angle2 = (uint32_t)((UMulScale16(ang, backcyl) + backpos) >> FRACBITS);

		// Check if this column has already been built. If so, there's
		// no reason to waste time building it again.
		uint32_t skycol = (angle1 << 16) | angle2;
		int i;

		if (!r_swtruecolor)
		{
			for (i = 0; i < 4; ++i)
			{
				if (lastskycol[i] == skycol)
				{
					return skybuf[i];
				}
			}

			lastskycol[skycolplace] = skycol;
			uint8_t *composite = skybuf[skycolplace];
			skycolplace = (skycolplace + 1) & 3;

			// The ordering of the following code has been tuned to allow VC++ to optimize
			// it well. In particular, this arrangement lets it keep count in a register
			// instead of on the stack.
			const uint8_t *front = fronttex->GetColumn(angle1, NULL);
			const uint8_t *back = backskytex->GetColumn(angle2, NULL);

			int count = MIN<int>(512, MIN(backskytex->GetHeight(), fronttex->GetHeight()));
			i = 0;
			do
			{
				if (front[i])
				{
					composite[i] = front[i];
				}
				else
				{
					composite[i] = back[i];
				}
			} while (++i, --count);
			return composite;
		}
		else
		{
			//return R_GetOneSkyColumn(fronttex, x);
			for (i = skycolplace_bgra - 4; i < skycolplace_bgra; ++i)
			{
				int ic = (i % MAXSKYBUF); // i "checker" - can wrap around the ends of the array
				if (lastskycol_bgra[ic] == skycol)
				{
					return (uint8_t*)(skybuf_bgra[ic]);
				}
			}

			lastskycol_bgra[skycolplace_bgra] = skycol;
			uint32_t *composite = skybuf_bgra[skycolplace_bgra];
			skycolplace_bgra = (skycolplace_bgra + 1) % MAXSKYBUF;

			// The ordering of the following code has been tuned to allow VC++ to optimize
			// it well. In particular, this arrangement lets it keep count in a register
			// instead of on the stack.
			const uint32_t *front = (const uint32_t *)fronttex->GetColumnBgra(angle1, NULL);
			const uint32_t *back = (const uint32_t *)backskytex->GetColumnBgra(angle2, NULL);

			//[SP] Paletted version is used for comparison only
			const uint8_t *frontcompare = fronttex->GetColumn(angle1, NULL);

			int count = MIN<int>(512, MIN(backskytex->GetHeight(), fronttex->GetHeight()));
			i = 0;
			do
			{
				if (frontcompare[i])
				{
					composite[i] = front[i];
				}
				else
				{
					composite[i] = back[i];
				}
			} while (++i, --count);
			return (uint8_t*)composite;
		}
	}

	void R_DrawSkyColumnStripe(int start_x, int y1, int y2, int columns, double scale, double texturemid, double yrepeat)
	{
		using namespace drawerargs;

		uint32_t height = frontskytex->GetHeight();

		for (int i = 0; i < columns; i++)
		{
			double uv_stepd = skyiscale * yrepeat;
			double v = (texturemid + uv_stepd * (y1 - CenterY + 0.5)) / height;
			double v_step = uv_stepd / height;

			uint32_t uv_pos = (uint32_t)(v * 0x01000000);
			uint32_t uv_step = (uint32_t)(v_step * 0x01000000);

			int x = start_x + i;
			if (MirrorFlags & RF_XFLIP)
				x = (viewwidth - x);

			uint32_t ang, angle1, angle2;

			if (r_linearsky)
			{
				angle_t xangle = (angle_t)((0.5 - x / (double)viewwidth) * FocalTangent * ANGLE_90);
				ang = (skyangle + xangle) ^ skyflip;
			}
			else
			{
				ang = (skyangle + xtoviewangle[x]) ^ skyflip;
			}
			angle1 = (uint32_t)((UMulScale16(ang, frontcyl) + frontpos) >> FRACBITS);
			angle2 = (uint32_t)((UMulScale16(ang, backcyl) + backpos) >> FRACBITS);

			if (r_swtruecolor)
			{
				dc_wall_source[i] = (const uint8_t *)frontskytex->GetColumnBgra(angle1, nullptr);
				dc_wall_source2[i] = backskytex ? (const uint8_t *)backskytex->GetColumnBgra(angle2, nullptr) : nullptr;
			}
			else
			{
				dc_wall_source[i] = (const uint8_t *)frontskytex->GetColumn(angle1, nullptr);
				dc_wall_source2[i] = backskytex ? (const uint8_t *)backskytex->GetColumn(angle2, nullptr) : nullptr;
			}

			dc_wall_iscale[i] = uv_step;
			dc_wall_texturefrac[i] = uv_pos;
		}

		dc_wall_sourceheight[0] = height;
		dc_wall_sourceheight[1] = backskytex ? backskytex->GetHeight() : height;
		int pixelsize = r_swtruecolor ? 4 : 1;
		dc_dest = (ylookup[y1] + start_x) * pixelsize + dc_destorg;
		dc_count = y2 - y1;

		uint32_t solid_top = frontskytex->GetSkyCapColor(false);
		uint32_t solid_bottom = frontskytex->GetSkyCapColor(true);

		if (!backskytex)
			R_Drawers()->DrawSingleSkyColumn(solid_top, solid_bottom);
		else
			R_Drawers()->DrawDoubleSkyColumn(solid_top, solid_bottom);
	}

	void R_DrawSkyColumn(int start_x, int y1, int y2, int columns)
	{
		if (1 << frontskytex->HeightBits == frontskytex->GetHeight())
		{
			double texturemid = skymid * frontskytex->Scale.Y + frontskytex->GetHeight();
			R_DrawSkyColumnStripe(start_x, y1, y2, columns, frontskytex->Scale.Y, texturemid, frontskytex->Scale.Y);
		}
		else
		{
			double yrepeat = frontskytex->Scale.Y;
			double scale = frontskytex->Scale.Y * skyscale;
			double iscale = 1 / scale;
			short drawheight = short(frontskytex->GetHeight() * scale);
			double topfrac = fmod(skymid + iscale * (1 - CenterY), frontskytex->GetHeight());
			if (topfrac < 0) topfrac += frontskytex->GetHeight();
			double texturemid = topfrac - iscale * (1 - CenterY);
			R_DrawSkyColumnStripe(start_x, y1, y2, columns, scale, texturemid, yrepeat);
		}
	}

	void R_DrawCapSky(visplane_t *pl)
	{
		int x1 = pl->left;
		int x2 = pl->right;
		short *uwal = (short *)pl->top;
		short *dwal = (short *)pl->bottom;

		for (int x = x1; x < x2; x++)
		{
			int y1 = uwal[x];
			int y2 = dwal[x];
			if (y2 <= y1)
				continue;

			R_DrawSkyColumn(x, y1, y2, 1);
		}
	}

	void R_DrawSky(visplane_t *pl)
	{
		if (r_skymode == 2)
		{
			R_DrawCapSky(pl);
			return;
		}

		int x;
		float swal;

		if (pl->left >= pl->right)
			return;

		swal = skyiscale;
		for (x = pl->left; x < pl->right; ++x)
		{
			swall[x] = swal;
		}

		if (MirrorFlags & RF_XFLIP)
		{
			for (x = pl->left; x < pl->right; ++x)
			{
				lwall[x] = (viewwidth - x) << FRACBITS;
			}
		}
		else
		{
			for (x = pl->left; x < pl->right; ++x)
			{
				lwall[x] = x << FRACBITS;
			}
		}

		for (x = 0; x < 4; ++x)
		{
			lastskycol[x] = 0xffffffff;
			lastskycol_bgra[x] = 0xffffffff;
		}

		rw_pic = frontskytex;
		rw_offset = 0;

		frontyScale = rw_pic->Scale.Y;
		dc_texturemid = skymid * frontyScale;

		if (1 << frontskytex->HeightBits == frontskytex->GetHeight())
		{ // The texture tiles nicely
			for (x = 0; x < 4; ++x)
			{
				lastskycol[x] = 0xffffffff;
				lastskycol_bgra[x] = 0xffffffff;
			}
			R_DrawSkySegment(pl->left, pl->right, (short *)pl->top, (short *)pl->bottom, swall, lwall,
				frontyScale, 0, backskytex == NULL ? R_GetOneSkyColumn : R_GetTwoSkyColumns);
		}
		else
		{ // The texture does not tile nicely
			frontyScale *= skyscale;
			frontiScale = 1 / frontyScale;
			R_DrawSkyStriped(pl);
		}
	}

	void R_DrawSkyStriped(visplane_t *pl)
	{
		short drawheight = short(frontskytex->GetHeight() * frontyScale);
		double topfrac;
		double iscale = frontiScale;
		short top[MAXWIDTH], bot[MAXWIDTH];
		short yl, yh;
		int x;

		topfrac = fmod(skymid + iscale * (1 - CenterY), frontskytex->GetHeight());
		if (topfrac < 0) topfrac += frontskytex->GetHeight();
		yl = 0;
		yh = short((frontskytex->GetHeight() - topfrac) * frontyScale);
		dc_texturemid = topfrac - iscale * (1 - CenterY);

		while (yl < viewheight)
		{
			for (x = pl->left; x < pl->right; ++x)
			{
				top[x] = MAX(yl, (short)pl->top[x]);
				bot[x] = MIN(yh, (short)pl->bottom[x]);
			}
			for (x = 0; x < 4; ++x)
			{
				lastskycol[x] = 0xffffffff;
				lastskycol_bgra[x] = 0xffffffff;
			}
			R_DrawSkySegment(pl->left, pl->right, top, bot, swall, lwall, rw_pic->Scale.Y, 0, backskytex == NULL ? R_GetOneSkyColumn : R_GetTwoSkyColumns);
			yl = yh;
			yh += drawheight;
			dc_texturemid = iscale * (centery - yl - 1);
		}
	}
}
