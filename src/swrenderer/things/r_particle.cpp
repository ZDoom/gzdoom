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

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "p_lnspec.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
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
#include "v_palette.h"
#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "r_data/voxels.h"
#include "p_local.h"
#include "p_maputl.h"
#include "r_voxel.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_translucent_pass.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/line/r_renderdrawsegment.h"
#include "swrenderer/things/r_particle.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/drawers/r_draw_pal.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_renderthread.h"

EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor);

namespace swrenderer
{
	void RenderParticle::Project(RenderThread *thread, particle_t *particle, const sector_t *sector, int shade, WaterFakeSide fakeside, bool foggy)
	{
		double 				tr_x, tr_y;
		double 				tx, ty;
		double	 			tz, tiz;
		double	 			xscale, yscale;
		int 				x1, x2, y1, y2;
		sector_t*			heightsec = NULL;

		double timefrac = r_viewpoint.TicFrac;
		if (paused || bglobal.freeze || (level.flags2 & LEVEL2_FROZEN))
			timefrac = 0.;

		double ippx = particle->Pos.X + particle->Vel.X * timefrac;
		double ippy = particle->Pos.Y + particle->Vel.Y * timefrac;
		double ippz = particle->Pos.Z + particle->Vel.Z * timefrac;

		RenderPortal *renderportal = thread->Portal.get();

		// [ZZ] Particle not visible through the portal plane
		if (renderportal->CurrentPortal && !!P_PointOnLineSide(particle->Pos, renderportal->CurrentPortal->dst))
			return;

		// transform the origin point
		tr_x = ippx - thread->Viewport->viewpoint.Pos.X;
		tr_y = ippy - thread->Viewport->viewpoint.Pos.Y;

		tz = tr_x * thread->Viewport->viewpoint.TanCos + tr_y * thread->Viewport->viewpoint.TanSin;

		// particle is behind view plane?
		if (tz < MINZ)
			return;

		tx = tr_x * thread->Viewport->viewpoint.Sin - tr_y * thread->Viewport->viewpoint.Cos;

		// Flip for mirrors
		if (renderportal->MirrorFlags & RF_XFLIP)
		{
			tx = viewwidth - tx - 1;
		}

		// too far off the side?
		if (tz <= fabs(tx))
			return;

		tiz = 1 / tz;
		xscale = thread->Viewport->viewwindow.centerx * tiz;

		// calculate edges of the shape
		double psize = particle->size / 8.0;

		x1 = MAX<int>(renderportal->WindowLeft, thread->Viewport->viewwindow.centerx + xs_RoundToInt((tx - psize) * xscale));
		x2 = MIN<int>(renderportal->WindowRight, thread->Viewport->viewwindow.centerx + xs_RoundToInt((tx + psize) * xscale));

		if (x1 >= x2)
			return;
		
		auto viewport = thread->Viewport.get();

		yscale = xscale; // YaspectMul is not needed for particles as they should always be square
		ty = (ippz - viewport->viewpoint.Pos.Z) * thread->Viewport->YaspectMul;
		y1 = xs_RoundToInt(viewport->CenterY - (ty + psize) * yscale);
		y2 = xs_RoundToInt(viewport->CenterY - (ty - psize) * yscale);

		// Clip the particle now. Because it's a point and projected as its subsector is
		// entered, we don't need to clip it to drawsegs like a normal sprite.

		// Clip particles behind walls.
		auto ceilingclip = thread->OpaquePass->ceilingclip;
		auto floorclip = thread->OpaquePass->floorclip;
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
		FDynamicColormap *map;

		if (heightsec)	// only clip things which are in special sectors
		{
			if (fakeside == WaterFakeSide::AboveCeiling)
			{
				topplane = &sector->ceilingplane;
				botplane = &heightsec->ceilingplane;
				toppic = sector->GetTexture(sector_t::ceiling);
				botpic = heightsec->GetTexture(sector_t::ceiling);
				map = GetColorTable(heightsec->Colormap, heightsec->SpecialColors[sector_t::sprites], true);
			}
			else if (fakeside == WaterFakeSide::BelowFloor)
			{
				topplane = &heightsec->floorplane;
				botplane = &sector->floorplane;
				toppic = heightsec->GetTexture(sector_t::floor);
				botpic = sector->GetTexture(sector_t::floor);
				map = GetColorTable(heightsec->Colormap, heightsec->SpecialColors[sector_t::sprites], true);
			}
			else
			{
				topplane = &heightsec->ceilingplane;
				botplane = &heightsec->floorplane;
				toppic = heightsec->GetTexture(sector_t::ceiling);
				botpic = heightsec->GetTexture(sector_t::floor);
				map = GetColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], true);
			}
		}
		else
		{
			topplane = &sector->ceilingplane;
			botplane = &sector->floorplane;
			toppic = sector->GetTexture(sector_t::ceiling);
			botpic = sector->GetTexture(sector_t::floor);
			map = GetColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], true);
		}

		if (botpic != skyflatnum && ippz < botplane->ZatPoint(particle->Pos))
			return;
		if (toppic != skyflatnum && ippz >= topplane->ZatPoint(particle->Pos))
			return;

		// store information in a vissprite
		RenderParticle *vis = thread->FrameMemory->NewObject<RenderParticle>();

		vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
		vis->heightsec = heightsec;
		vis->xscale = FLOAT2FIXED(xscale);
		vis->yscale = (float)xscale;
		//	vis->yscale *= InvZtoScale;
		vis->depth = (float)tz;
		vis->idepth = float(1 / tz);
		vis->gpos = { (float)ippx, (float)ippy, (float)ippz };
		vis->y1 = y1;
		vis->y2 = y2;
		vis->x1 = x1;
		vis->x2 = x2;
		vis->Translation = 0;
		vis->startfrac = 255 & (particle->color >> 24);
		vis->pic = NULL;
		vis->renderflags = (short)(particle->alpha * 255.0f + 0.5f);
		vis->FakeFlatStat = fakeside;
		vis->floorclip = 0;
		vis->foggy = foggy;

		vis->Light.SetColormap(tiz * thread->Light->ParticleGlobVis(foggy), shade, map, particle->bright != 0, false, false);

		thread->SpriteList->Push(vis);
	}

	void RenderParticle::Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor)
	{
		auto vis = this;

		int spacing;
		uint8_t color = vis->Light.BaseColormap->Maps[vis->startfrac];
		int yl = vis->y1;
		int ycount = vis->y2 - yl + 1;
		int x1 = vis->x1;
		int countbase = vis->x2 - x1;

		if (ycount <= 0 || countbase <= 0)
			return;

		DrawMaskedSegsBehindParticle(thread, clip3DFloor);

		uint32_t fg = LightBgra::shade_pal_index_simple(color, LightBgra::calc_light_multiplier(LIGHTSCALE(0, vis->Light.ColormapNum << FRACBITS)));

		// vis->renderflags holds translucency level (0-255)
		fixed_t fglevel = ((vis->renderflags + 1) << 8) & ~0x3ff;
		uint32_t alpha = fglevel * 256 / FRACUNIT;
		
		auto viewport = thread->Viewport.get();

		spacing = viewport->RenderTarget->GetPitch();

		uint32_t fracstepx = PARTICLE_TEXTURE_SIZE * FRACUNIT / countbase;
		uint32_t fracposx = fracstepx / 2;

		RenderTranslucentPass *translucentPass = thread->TranslucentPass.get();

		if (viewport->RenderTarget->IsBgra())
		{
			for (int x = x1; x < (x1 + countbase); x++, fracposx += fracstepx)
			{
				if (translucentPass->ClipSpriteColumnWithPortals(x, vis))
					continue;
				uint32_t *dest = (uint32_t*)viewport->GetDest(x, yl);
				thread->DrawQueue->Push<DrawParticleColumnRGBACommand>(dest, yl, spacing, ycount, fg, alpha, fracposx);
			}
		}
		else
		{
			for (int x = x1; x < (x1 + countbase); x++, fracposx += fracstepx)
			{
				if (translucentPass->ClipSpriteColumnWithPortals(x, vis))
					continue;
				uint8_t *dest = viewport->GetDest(x, yl);
				thread->DrawQueue->Push<DrawParticleColumnPalCommand>(dest, yl, spacing, ycount, fg, alpha, fracposx);
			}
		}
	}

	void RenderParticle::DrawMaskedSegsBehindParticle(RenderThread *thread, const Fake3DTranslucent &clip3DFloor)
	{
		// Draw any masked textures behind this particle so that when the
		// particle is drawn, it will be in front of them.
		DrawSegmentList *segmentlist = thread->DrawSegments.get();
		for (unsigned int index = 0; index != segmentlist->TranslucentSegmentsCount(); index++)
		{
			DrawSegment *ds = segmentlist->TranslucentSegment(index);

			if (ds->x1 >= x2 || ds->x2 <= x1)
			{
				continue;
			}
			if ((ds->siz2 - ds->siz1) * ((x2 + x1) / 2 - ds->sx1) / (ds->sx2 - ds->sx1) + ds->siz1 < idepth)
			{
				// [ZZ] only draw stuff that's inside the same portal as the particle, other portals will care for themselves
				if (ds->CurrentPortalUniq == CurrentPortalUniq)
				{
					RenderDrawSegment renderer(thread);
					renderer.Render(ds, MAX<int>(ds->x1, x1), MIN<int>(ds->x2, x2), clip3DFloor);
				}
			}
		}
	}
}
