/*
**  Handling drawing a plane (ceiling, floor)
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
#include "r_poly_plane.h"
#include "r_poly.h"
#include "r_sky.h" // for skyflatnum

void RenderPolyPlane::Render(const TriMatrix &worldToClip, subsector_t *sub, uint32_t subsectorDepth, bool ceiling, double skyHeight)
{
	sector_t *frontsector = sub->sector;

	FTextureID picnum = frontsector->GetTexture(ceiling ? sector_t::ceiling : sector_t::floor);
	FTexture *tex = TexMan(picnum);
	if (tex->UseType == FTexture::TEX_Null)
		return;

	bool isSky = picnum == skyflatnum;

	TriUniforms uniforms;
	uniforms.objectToClip = worldToClip;
	uniforms.light = (uint32_t)(frontsector->lightlevel / 255.0f * 256.0f);
	if (fixedlightlev >= 0)
		uniforms.light = (uint32_t)(fixedlightlev / 255.0f * 256.0f);
	else if (fixedcolormap)
		uniforms.light = 256;
	uniforms.flags = 0;
	uniforms.subsectorDepth = isSky ? RenderPolyScene::SkySubsectorDepth : subsectorDepth;

	TriVertex *vertices = PolyVertexBuffer::GetVertices(sub->numlines);
	if (!vertices)
		return;

	if (ceiling)
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[sub->numlines - 1 - i] = PlaneVertex(line->v1, frontsector, isSky ? skyHeight : frontsector->ceilingplane.ZatPoint(line->v1));
		}
	}
	else
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[i] = PlaneVertex(line->v1, frontsector, isSky ? skyHeight : frontsector->floorplane.ZatPoint(line->v1));
		}
	}

	PolyDrawArgs args;
	args.uniforms = uniforms;
	args.vinput = vertices;
	args.vcount = sub->numlines;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = true;
	args.clipleft = 0;
	args.cliptop = 0;
	args.clipright = viewwidth;
	args.clipbottom = viewheight;
	args.stenciltestvalue = 0;
	args.stencilwritevalue = 1;

	if (!isSky)
	{
		args.SetTexture(tex);
		PolyTriangleDrawer::draw(args, TriDrawVariant::Draw);
		PolyTriangleDrawer::draw(args, TriDrawVariant::Stencil);
	}
	else
	{
		args.stencilwritevalue = 255;
		PolyTriangleDrawer::draw(args, TriDrawVariant::Stencil);

		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			TriVertex *wallvert = PolyVertexBuffer::GetVertices(4);
			if (!wallvert)
				return;

			seg_t *line = &sub->firstline[i];

			double skyBottomz1 = frontsector->ceilingplane.ZatPoint(line->v1);
			double skyBottomz2 = frontsector->ceilingplane.ZatPoint(line->v2);
			if (line->backsector)
			{
				sector_t *backsector = (line->backsector != line->frontsector) ? line->backsector : line->frontsector;

				double frontceilz1 = frontsector->ceilingplane.ZatPoint(line->v1);
				double frontfloorz1 = frontsector->floorplane.ZatPoint(line->v1);
				double frontceilz2 = frontsector->ceilingplane.ZatPoint(line->v2);
				double frontfloorz2 = frontsector->floorplane.ZatPoint(line->v2);

				double backceilz1 = backsector->ceilingplane.ZatPoint(line->v1);
				double backfloorz1 = backsector->floorplane.ZatPoint(line->v1);
				double backceilz2 = backsector->ceilingplane.ZatPoint(line->v2);
				double backfloorz2 = backsector->floorplane.ZatPoint(line->v2);

				double topceilz1 = frontceilz1;
				double topceilz2 = frontceilz2;
				double topfloorz1 = MIN(backceilz1, frontceilz1);
				double topfloorz2 = MIN(backceilz2, frontceilz2);
				double bottomceilz1 = MAX(frontfloorz1, backfloorz1);
				double bottomceilz2 = MAX(frontfloorz2, backfloorz2);
				double bottomfloorz1 = frontfloorz1;
				double bottomfloorz2 = frontfloorz2;
				double middleceilz1 = topfloorz1;
				double middleceilz2 = topfloorz2;
				double middlefloorz1 = MIN(bottomceilz1, middleceilz1);
				double middlefloorz2 = MIN(bottomceilz2, middleceilz2);

				bool bothSkyCeiling = frontsector->GetTexture(sector_t::ceiling) == skyflatnum && backsector->GetTexture(sector_t::ceiling) == skyflatnum;

				bool closedSector = backceilz1 == backfloorz1 && backceilz2 == backfloorz2;
				if (ceiling && bothSkyCeiling && closedSector)
				{
					skyBottomz1 = middlefloorz1;
					skyBottomz2 = middlefloorz2;
				}
				else
				{
					bool topwall = (topceilz1 > topfloorz1 || topceilz2 > topfloorz2) && line->sidedef && !bothSkyCeiling;
					bool bottomwall = (bottomfloorz1 < bottomceilz1 || bottomfloorz2 < bottomceilz2) && line->sidedef;
					if ((ceiling && !topwall) || (!ceiling && !bottomwall))
						continue;
				}
			}

			if (ceiling)
			{
				wallvert[0] = PlaneVertex(line->v1, frontsector, skyHeight);
				wallvert[1] = PlaneVertex(line->v2, frontsector, skyHeight);
				wallvert[2] = PlaneVertex(line->v2, frontsector, skyBottomz2);
				wallvert[3] = PlaneVertex(line->v1, frontsector, skyBottomz1);
			}
			else
			{
				wallvert[0] = PlaneVertex(line->v1, frontsector, frontsector->floorplane.ZatPoint(line->v1));
				wallvert[1] = PlaneVertex(line->v2, frontsector, frontsector->floorplane.ZatPoint(line->v2));
				wallvert[2] = PlaneVertex(line->v2, frontsector, skyHeight);
				wallvert[3] = PlaneVertex(line->v1, frontsector, skyHeight);
			}

			args.vinput = wallvert;
			args.vcount = 4;
			PolyTriangleDrawer::draw(args, TriDrawVariant::Stencil);
		}
	}
}

TriVertex RenderPolyPlane::PlaneVertex(vertex_t *v1, sector_t *sector, double height)
{
	TriVertex v;
	v.x = (float)v1->fPos().X;
	v.y = (float)v1->fPos().Y;
	v.z = (float)height;
	v.w = 1.0f;
	v.varying[0] = v.x / 64.0f;
	v.varying[1] = 1.0f - v.y / 64.0f;
	return v;
}
