
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "p_lnspec.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_things.h"
#include "swrenderer/things/r_particle.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "v_video.h"
#include "sc_man.h"
#include "s_sound.h"
#include "sbar.h"
#include "gi.h"
#include "r_sky.h"
#include "cmdlib.h"
#include "g_level.h"
#include "d_net.h"
#include "colormatcher.h"
#include "d_netinf.h"
#include "p_effect.h"
#include "swrenderer/scene/r_bsp.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/drawers/r_draw_pal.h"
#include "v_palette.h"
#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "r_data/voxels.h"
#include "p_local.h"
#include "p_maputl.h"
#include "r_voxel.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/r_memory.h"

namespace swrenderer
{
	void R_ProjectParticle(particle_t *particle, const sector_t *sector, int shade, int fakeside)
	{
		double 				tr_x, tr_y;
		double 				tx, ty;
		double	 			tz, tiz;
		double	 			xscale, yscale;
		int 				x1, x2, y1, y2;
		vissprite_t*		vis;
		sector_t*			heightsec = NULL;
		FSWColormap*			map;

		// [ZZ] Particle not visible through the portal plane
		if (CurrentPortal && !!P_PointOnLineSide(particle->Pos, CurrentPortal->dst))
			return;

		// transform the origin point
		tr_x = particle->Pos.X - ViewPos.X;
		tr_y = particle->Pos.Y - ViewPos.Y;

		tz = tr_x * ViewTanCos + tr_y * ViewTanSin;

		// particle is behind view plane?
		if (tz < MINZ)
			return;

		tx = tr_x * ViewSin - tr_y * ViewCos;

		// Flip for mirrors
		if (MirrorFlags & RF_XFLIP)
		{
			tx = viewwidth - tx - 1;
		}

		// too far off the side?
		if (tz <= fabs(tx))
			return;

		tiz = 1 / tz;
		xscale = centerx * tiz;

		// calculate edges of the shape
		double psize = particle->size / 8.0;

		x1 = MAX<int>(WindowLeft, centerx + xs_RoundToInt((tx - psize) * xscale));
		x2 = MIN<int>(WindowRight, centerx + xs_RoundToInt((tx + psize) * xscale));

		if (x1 >= x2)
			return;

		yscale = xscale; // YaspectMul is not needed for particles as they should always be square
		ty = particle->Pos.Z - ViewPos.Z;
		y1 = xs_RoundToInt(CenterY - (ty + psize) * yscale);
		y2 = xs_RoundToInt(CenterY - (ty - psize) * yscale);

		// Clip the particle now. Because it's a point and projected as its subsector is
		// entered, we don't need to clip it to drawsegs like a normal sprite.

		// Clip particles behind walls.
		if (y1 <  ceilingclip[x1])		y1 = ceilingclip[x1];
		if (y1 <  ceilingclip[x2 - 1])	y1 = ceilingclip[x2 - 1];
		if (y2 >= floorclip[x1])		y2 = floorclip[x1] - 1;
		if (y2 >= floorclip[x2 - 1])		y2 = floorclip[x2 - 1] - 1;

		if (y1 > y2)
			return;

		// Clip particles above the ceiling or below the floor.
		heightsec = sector->GetHeightSec();

		const secplane_t *topplane;
		const secplane_t *botplane;
		FTextureID toppic;
		FTextureID botpic;

		if (heightsec)	// only clip things which are in special sectors
		{
			if (fakeside == FAKED_AboveCeiling)
			{
				topplane = &sector->ceilingplane;
				botplane = &heightsec->ceilingplane;
				toppic = sector->GetTexture(sector_t::ceiling);
				botpic = heightsec->GetTexture(sector_t::ceiling);
				map = heightsec->ColorMap;
			}
			else if (fakeside == FAKED_BelowFloor)
			{
				topplane = &heightsec->floorplane;
				botplane = &sector->floorplane;
				toppic = heightsec->GetTexture(sector_t::floor);
				botpic = sector->GetTexture(sector_t::floor);
				map = heightsec->ColorMap;
			}
			else
			{
				topplane = &heightsec->ceilingplane;
				botplane = &heightsec->floorplane;
				toppic = heightsec->GetTexture(sector_t::ceiling);
				botpic = heightsec->GetTexture(sector_t::floor);
				map = sector->ColorMap;
			}
		}
		else
		{
			topplane = &sector->ceilingplane;
			botplane = &sector->floorplane;
			toppic = sector->GetTexture(sector_t::ceiling);
			botpic = sector->GetTexture(sector_t::floor);
			map = sector->ColorMap;
		}

		if (botpic != skyflatnum && particle->Pos.Z < botplane->ZatPoint(particle->Pos))
			return;
		if (toppic != skyflatnum && particle->Pos.Z >= topplane->ZatPoint(particle->Pos))
			return;

		// store information in a vissprite
		vis = R_NewVisSprite();
		vis->CurrentPortalUniq = CurrentPortalUniq;
		vis->heightsec = heightsec;
		vis->xscale = FLOAT2FIXED(xscale);
		vis->yscale = (float)xscale;
		//	vis->yscale *= InvZtoScale;
		vis->depth = (float)tz;
		vis->idepth = float(1 / tz);
		vis->gpos = { (float)particle->Pos.X, (float)particle->Pos.Y, (float)particle->Pos.Z };
		vis->y1 = y1;
		vis->y2 = y2;
		vis->x1 = x1;
		vis->x2 = x2;
		vis->Translation = 0;
		vis->startfrac = 255 & (particle->color >> 24);
		vis->pic = NULL;
		vis->bIsVoxel = false;
		vis->renderflags = particle->trans;
		vis->FakeFlatStat = fakeside;
		vis->floorclip = 0;
		vis->Style.ColormapNum = 0;

		if (fixedlightlev >= 0)
		{
			vis->Style.BaseColormap = map;
			vis->Style.ColormapNum = fixedlightlev >> COLORMAPSHIFT;
		}
		else if (fixedcolormap)
		{
			vis->Style.BaseColormap = fixedcolormap;
			vis->Style.ColormapNum = 0;
		}
		else if (particle->bright)
		{
			vis->Style.BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : map;
			vis->Style.ColormapNum = 0;
		}
		else
		{
			// Particles are slightly more visible than regular sprites.
			vis->Style.ColormapNum = GETPALOOKUP(tiz * r_SpriteVisibility * 0.5, shade);
			vis->Style.BaseColormap = map;
		}
	}

	void R_DrawParticle(vissprite_t *vis)
	{
		using namespace drawerargs;

		int spacing;
		BYTE color = vis->Style.BaseColormap->Maps[vis->startfrac];
		int yl = vis->y1;
		int ycount = vis->y2 - yl + 1;
		int x1 = vis->x1;
		int countbase = vis->x2 - x1;

		if (ycount <= 0 || countbase <= 0)
			return;

		R_DrawMaskedSegsBehindParticle(vis);

		uint32_t fg = LightBgra::shade_pal_index_simple(color, LightBgra::calc_light_multiplier(LIGHTSCALE(0, vis->Style.ColormapNum << FRACBITS)));

		// vis->renderflags holds translucency level (0-255)
		fixed_t fglevel = ((vis->renderflags + 1) << 8) & ~0x3ff;
		uint32_t alpha = fglevel * 256 / FRACUNIT;

		spacing = RenderTarget->GetPitch();

		uint32_t fracstepx = 16 * FRACUNIT / countbase;
		uint32_t fracposx = fracstepx / 2;

		if (r_swtruecolor)
		{
			for (int x = x1; x < (x1 + countbase); x++, fracposx += fracstepx)
			{
				dc_x = x;
				if (R_ClipSpriteColumnWithPortals(vis))
					continue;
				uint32_t *dest = ylookup[yl] + x + (uint32_t*)dc_destorg;
				DrawerCommandQueue::QueueCommand<DrawParticleColumnRGBACommand>(dest, yl, spacing, ycount, fg, alpha, fracposx);
			}
		}
		else
		{
			for (int x = x1; x < (x1 + countbase); x++, fracposx += fracstepx)
			{
				dc_x = x;
				if (R_ClipSpriteColumnWithPortals(vis))
					continue;
				uint8_t *dest = ylookup[yl] + x + dc_destorg;
				DrawerCommandQueue::QueueCommand<DrawParticleColumnPalCommand>(dest, yl, spacing, ycount, fg, alpha, fracposx);
			}
		}
	}

	void R_DrawMaskedSegsBehindParticle(const vissprite_t *vis)
	{
		const int x1 = vis->x1;
		const int x2 = vis->x2;

		// Draw any masked textures behind this particle so that when the
		// particle is drawn, it will be in front of them.
		for (unsigned int p = InterestingDrawsegs.Size(); p-- > FirstInterestingDrawseg; )
		{
			drawseg_t *ds = &drawsegs[InterestingDrawsegs[p]];
			// kg3D - no fake segs
			if (ds->fake) continue;
			if (ds->x1 >= x2 || ds->x2 <= x1)
			{
				continue;
			}
			if ((ds->siz2 - ds->siz1) * ((x2 + x1) / 2 - ds->sx1) / (ds->sx2 - ds->sx1) + ds->siz1 < vis->idepth)
			{
				// [ZZ] only draw stuff that's inside the same portal as the particle, other portals will care for themselves
				if (ds->CurrentPortalUniq == vis->CurrentPortalUniq)
					R_RenderMaskedSegRange(ds, MAX<int>(ds->x1, x1), MIN<int>(ds->x2, x2));
			}
		}
	}
}
