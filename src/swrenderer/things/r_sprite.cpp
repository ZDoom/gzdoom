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
#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_things.h"
#include "swrenderer/things/r_wallsprite.h"
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
#include "swrenderer/things/r_sprite.h"
#include "swrenderer/r_memory.h"

namespace swrenderer
{
	void R_DrawVisSprite(vissprite_t *vis, const short *mfloorclip, const short *mceilingclip)
	{
		fixed_t 		frac;
		FTexture		*tex;
		int				x2;
		fixed_t			xiscale;
		bool			ispsprite = (!vis->sector && vis->gpos != FVector3(0, 0, 0));

		double spryscale, sprtopscreen;
		bool sprflipvert;

		if (vis->xscale == 0 || fabs(vis->yscale) < (1.0f / 32000.0f))
		{ // scaled to 0; can't see
			return;
		}

		fixed_t centeryfrac = FLOAT2FIXED(CenterY);
		R_SetColorMapLight(vis->Style.BaseColormap, 0, vis->Style.ColormapNum << FRACBITS);

		bool visible = R_SetPatchStyle(vis->Style.RenderStyle, vis->Style.Alpha, vis->Translation, vis->FillColor);

		if (vis->Style.RenderStyle == LegacyRenderStyles[STYLE_Shaded])
		{ // For shaded sprites, R_SetPatchStyle sets a dc_colormap to an alpha table, but
		  // it is the brightest one. We need to get back to the proper light level for
		  // this sprite.
			R_SetColorMapLight(drawerargs::dc_fcolormap, 0, vis->Style.ColormapNum << FRACBITS);
		}

		if (visible)
		{
			tex = vis->pic;
			spryscale = vis->yscale;
			sprflipvert = false;
			fixed_t iscale = FLOAT2FIXED(1 / vis->yscale);
			frac = vis->startfrac;
			xiscale = vis->xiscale;
			dc_texturemid = vis->texturemid;

			if (vis->renderflags & RF_YFLIP)
			{
				sprflipvert = true;
				spryscale = -spryscale;
				iscale = -iscale;
				dc_texturemid -= vis->pic->GetHeight();
				sprtopscreen = CenterY + dc_texturemid * spryscale;
			}
			else
			{
				sprflipvert = false;
				sprtopscreen = CenterY - dc_texturemid * spryscale;
			}

			int x = vis->x1;
			x2 = vis->x2;

			if (x < x2)
			{
				while (x < x2)
				{
					if (ispsprite || !R_ClipSpriteColumnWithPortals(x, vis))
						R_DrawMaskedColumn(x, iscale, tex, frac, spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, false);
					x++;
					frac += xiscale;
				}
			}
		}

		R_FinishSetPatchStyle();

		NetUpdate();
	}
}
