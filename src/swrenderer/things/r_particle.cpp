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
		
		RenderPortal *renderportal = thread->Portal.get();

		// [ZZ] Particle not visible through the portal plane
		if (renderportal->CurrentPortal && !!P_PointOnLineSide(particle->Pos, renderportal->CurrentPortal->dst))
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
		if (renderportal->MirrorFlags & RF_XFLIP)
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

		x1 = MAX<int>(renderportal->WindowLeft, centerx + xs_RoundToInt((tx - psize) * xscale));
		x2 = MIN<int>(renderportal->WindowRight, centerx + xs_RoundToInt((tx + psize) * xscale));

		if (x1 >= x2)
			return;
		
		auto viewport = RenderViewport::Instance();

		yscale = xscale; // YaspectMul is not needed for particles as they should always be square
		ty = particle->Pos.Z - ViewPos.Z;
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
				map = heightsec->ColorMap;
			}
			else if (fakeside == WaterFakeSide::BelowFloor)
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
		RenderParticle *vis = thread->FrameMemory->NewObject<RenderParticle>();

		vis->CurrentPortalUniq = renderportal->CurrentPortalUniq;
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
		vis->renderflags = (short)(particle->alpha * 255.0f + 0.5f);
		vis->FakeFlatStat = fakeside;
		vis->floorclip = 0;
		vis->foggy = foggy;

		vis->Light.SetColormap(tiz * LightVisibility::Instance()->ParticleGlobVis(), shade, map, particle->bright != 0, false, false);

		thread->SpriteList->Push(vis);
	}

	void RenderParticle::Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ)
	{
		auto vis = this;

		int spacing;
		BYTE color = vis->Light.BaseColormap->Maps[vis->startfrac];
		int yl = vis->y1;
		int ycount = vis->y2 - yl + 1;
		int x1 = vis->x1;
		int countbase = vis->x2 - x1;

		if (ycount <= 0 || countbase <= 0)
			return;

		DrawMaskedSegsBehindParticle(thread);

		uint32_t fg = LightBgra::shade_pal_index_simple(color, LightBgra::calc_light_multiplier(LIGHTSCALE(0, vis->Light.ColormapNum << FRACBITS)));

		// vis->renderflags holds translucency level (0-255)
		fixed_t fglevel = ((vis->renderflags + 1) << 8) & ~0x3ff;
		uint32_t alpha = fglevel * 256 / FRACUNIT;
		
		auto viewport = RenderViewport::Instance();

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

	void RenderParticle::DrawMaskedSegsBehindParticle(RenderThread *thread)
	{
		// Draw any masked textures behind this particle so that when the
		// particle is drawn, it will be in front of them.
		DrawSegmentList *segmentlist = thread->DrawSegments.get();
		for (unsigned int index = 0; index != segmentlist->InterestingSegmentsCount(); index++)
		{
			DrawSegment *ds = segmentlist->InterestingSegment(index);

			// kg3D - no fake segs
			if (ds->fake) continue;
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
					renderer.Render(ds, MAX<int>(ds->x1, x1), MIN<int>(ds->x2, x2));
				}
			}
		}
	}
}
