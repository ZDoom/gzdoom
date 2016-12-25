/*
**  Voxel rendering
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "r_data/voxels.h"
#include "r_data/sprites.h"
#include "d_net.h"
#include "po_man.h"
#include "r_things.h"
#include "r_draw.h"
#include "r_thread.h"
#include "r_utility.h"
#include "r_main.h"
#include "r_voxel.h"

namespace swrenderer
{
	void R_DrawVisVoxel(vissprite_t *sprite, int minZ, int maxZ, short *cliptop, short *clipbottom)
	{
		R_SetColorMapLight(sprite->Style.BaseColormap, 0, sprite->Style.ColormapNum << FRACBITS);
		bool visible = R_SetPatchStyle(sprite->Style.RenderStyle, sprite->Style.Alpha, sprite->Translation, sprite->FillColor);
		if (!visible)
			return;

		FVector3 view_origin = sprite->pa.vpos;
		FAngle view_angle = sprite->pa.vang;
		DVector3 sprite_origin = { sprite->gpos.X, sprite->gpos.Y, sprite->gpos.Z };
		DAngle sprite_angle = sprite->Angle;
		double sprite_xscale = FIXED2DBL(sprite->xscale);
		double sprite_yscale = sprite->yscale;
		FVoxel *voxel = sprite->voxel;

		int miplevel = 0;//voxel->NumMips;
		const FVoxelMipLevel &mip = voxel->Mips[miplevel];
		if (mip.SlabData == nullptr)
			return;

		/*
		double spriteSin = sprite_angle.Sin();
		double spriteCos = sprite_angle.Cos();
		double pivotX = (mip.Pivot.X * spriteSin - mip.Pivot.Y * spriteCos);
		double pivotY = (mip.Pivot.X * spriteCos + mip.Pivot.Y * spriteSin);
		double pivotZ = -mip.Pivot.Z;
		*/

		R_FillBox(sprite_origin, 5.0 * sprite_xscale, 5.0 * sprite_yscale, 16, cliptop, clipbottom, false, false);
	}

	void R_FillBox(DVector3 origin, double extentX, double extentY, int color, short *cliptop, short *clipbottom, bool viewspace, bool pixelstretch)
	{
		double viewX, viewY, viewZ;
		if (viewspace)
		{
			viewX = origin.X;
			viewY = origin.Y;
			viewZ = origin.Z;
		}
		else // world space
		{
			double translatedX = origin.X - ViewPos.X;
			double translatedY = origin.Y - ViewPos.Y;
			double translatedZ = origin.Z - ViewPos.Z;
			viewX = translatedX * ViewSin - translatedY * ViewCos;
			viewY = translatedZ;
			viewZ = translatedX * ViewTanCos + translatedY * ViewTanSin;
		}

		if (viewZ < 0.01f)
			return;

		double screenX = CenterX + viewX / viewZ * CenterX;
		double screenY = CenterY - viewY / viewZ * InvZtoScale;
		double screenExtentX = extentX / viewZ * CenterX;
		double screenExtentY = pixelstretch ? screenExtentX * YaspectMul : screenExtentX;

		int x1 = MAX((int)(screenX - screenExtentX), 0);
		int x2 = MIN((int)(screenX + screenExtentX + 0.5f), viewwidth - 1);
		int y1 = MAX((int)(screenY - screenExtentY), 0);
		int y2 = MIN((int)(screenY + screenExtentY + 0.5f), viewheight - 1);

		int pixelsize = r_swtruecolor ? 4 : 1;

		if (y1 < y2)
		{
			for (int x = x1; x < x2; x++)
			{
				int columnY1 = MAX(y1, (int)cliptop[x]);
				int columnY2 = MIN(y2, (int)clipbottom[x]);
				if (columnY1 < columnY2)
				{
					using namespace drawerargs;
					dc_dest = dc_destorg + (dc_pitch * columnY1 + x) * pixelsize;
					dc_color = color;
					dc_count = columnY2 - columnY1;
					R_FillColumn();
				}
			}
		}
	}
}
