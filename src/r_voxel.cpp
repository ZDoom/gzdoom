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

		// To do: calculate the mipmap level based on distance, sprite scale and voxel extents
		int miplevel = 0;//voxel->NumMips;
		
		const FVoxelMipLevel &mip = voxel->Mips[miplevel];
		if (mip.SlabData == nullptr)
			return;

		double spriteSin = sprite_angle.Sin();
		double spriteCos = sprite_angle.Cos();
		
		DVector2 dirX(spriteSin * sprite_xscale, -spriteCos * sprite_xscale);
		DVector2 dirY(spriteCos * sprite_xscale, spriteSin * sprite_xscale);
		float dirZ = -sprite_yscale;
		
		DVector3 voxel_origin = sprite_origin;
		voxel_origin.X -= dirX.X * mip.Pivot.X + dirX.Y * mip.Pivot.Y;
		voxel_origin.Y -= dirY.X * mip.Pivot.X + dirY.Y * mip.Pivot.Y;
		voxel_origin.Z -= dirZ * mip.Pivot.Z;
		
		// To do: do this loop sorted back to front:
		
		for (int x = 0; x < mip.SizeX; x++)
		{
			for (int y = 0; y < mip.SizeY; y++)
			{
				kvxslab_t *slab_start = R_GetSlabStart(mip, x, y);
				kvxslab_t *slab_end = R_GetSlabEnd(mip, x, y);
				
				for (kvxslab_t *slab = slab_start; slab != slab_end; slab = R_NextSlab(slab))
				{
					// To do: check slab->backfacecull
				
					for (int i = 0; i < slab->zleng; i++)
					{
						int z = slab->ztop + i;
						uint8_t color = slab->col[i];
						
						DVector3 voxel_pos = voxel_origin;
						voxel_pos.X += dirX.X * x + dirX.Y * y;
						voxel_pos.Y += dirY.X * x + dirY.Y * y;
						voxel_pos.Z += dirZ * z;
						
						R_FillBox(voxel_pos, sprite_xscale, sprite_yscale, color, cliptop, clipbottom, false, false);
					}
				}
			}
		}
	}
	
	kvxslab_t *R_GetSlabStart(const FVoxelMipLevel &mip, int x, int y)
	{
		return (kvxslab_t *)&mip.SlabData[mip.OffsetX[x] + (int)mip.OffsetXY[x * (mip.SizeY + 1) + y]];
	}

	kvxslab_t *R_GetSlabEnd(const FVoxelMipLevel &mip, int x, int y)
	{
		return R_GetSlabStart(mip, x, y + 1);
	}
	
	kvxslab_t *R_NextSlab(kvxslab_t *slab)
	{
		return (kvxslab_t*)(((uint8_t*)slab) + 3 + slab->zleng);
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
