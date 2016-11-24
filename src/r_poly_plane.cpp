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
#include "r_poly_portal.h"
#include "r_poly.h"
#include "r_sky.h" // for skyflatnum

EXTERN_CVAR(Int, r_3dfloors)

void RenderPolyPlane::RenderPlanes(const TriMatrix &worldToClip, subsector_t *sub, uint32_t subsectorDepth, double skyCeilingHeight, double skyFloorHeight)
{
	RenderPolyPlane plane;

	if (r_3dfloors)
	{
		auto frontsector = sub->sector;
		auto &ffloors = frontsector->e->XFloor.ffloors;

		// 3D floor floors
		for (int i = 0; i < (int)ffloors.Size(); i++)
		{
			F3DFloor *fakeFloor = ffloors[i];
			if (!(fakeFloor->flags & FF_EXISTS)) continue;
			if (!fakeFloor->model) continue;
			if (fakeFloor->bottom.plane->isSlope()) continue;
			//if (!(fakeFloor->flags & FF_NOSHADE) || (fakeFloor->flags & (FF_RENDERPLANES | FF_RENDERSIDES)))
			//	R_3D_AddHeight(fakeFloor->top.plane, frontsector);
			if (!(fakeFloor->flags & FF_RENDERPLANES)) continue;
			if (fakeFloor->alpha == 0) continue;
			if (fakeFloor->flags & FF_THISINSIDE && fakeFloor->flags & FF_INVERTSECTOR) continue;
			//fakeFloor->alpha

			double fakeHeight = fakeFloor->top.plane->ZatPoint(frontsector->centerspot);
			if (fakeHeight < ViewPos.Z && fakeHeight > frontsector->floorplane.ZatPoint(frontsector->centerspot))
			{
				plane.Render3DFloor(worldToClip, sub, subsectorDepth, false, fakeFloor);
			}
		}

		// 3D floor ceilings
		for (int i = 0; i < (int)ffloors.Size(); i++)
		{
			F3DFloor *fakeFloor = ffloors[i];
			if (!(fakeFloor->flags & FF_EXISTS)) continue;
			if (!fakeFloor->model) continue;
			if (fakeFloor->top.plane->isSlope()) continue;
			//if (!(fakeFloor->flags & FF_NOSHADE) || (fakeFloor->flags & (FF_RENDERPLANES | FF_RENDERSIDES)))
			//	R_3D_AddHeight(fakeFloor->bottom.plane, frontsector);
			if (!(fakeFloor->flags & FF_RENDERPLANES)) continue;
			if (fakeFloor->alpha == 0) continue;
			if (!(fakeFloor->flags & FF_THISINSIDE) && (fakeFloor->flags & (FF_SWIMMABLE | FF_INVERTSECTOR)) == (FF_SWIMMABLE | FF_INVERTSECTOR)) continue;
			//fakeFloor->alpha

			double fakeHeight = fakeFloor->bottom.plane->ZatPoint(frontsector->centerspot);
			if (fakeHeight > ViewPos.Z && fakeHeight < frontsector->ceilingplane.ZatPoint(frontsector->centerspot))
			{
				plane.Render3DFloor(worldToClip, sub, subsectorDepth, true, fakeFloor);
			}
		}
	}

	plane.Render(worldToClip, sub, subsectorDepth, true, skyCeilingHeight);
	plane.Render(worldToClip, sub, subsectorDepth, false, skyFloorHeight);
}

void RenderPolyPlane::Render3DFloor(const TriMatrix &worldToClip, subsector_t *sub, uint32_t subsectorDepth, bool ceiling, F3DFloor *fakeFloor)
{
	FTextureID picnum = ceiling ? *fakeFloor->bottom.texture : *fakeFloor->top.texture;
	FTexture *tex = TexMan(picnum);
	if (tex->UseType == FTexture::TEX_Null)
		return;

	int lightlevel = 255;
	if (fixedlightlev < 0 && sub->sector->e->XFloor.lightlist.Size())
	{
		lightlist_t *light = P_GetPlaneLight(sub->sector, &sub->sector->ceilingplane, false);
		basecolormap = light->extra_colormap;
		lightlevel = *light->p_lightlevel;
	}

	TriUniforms uniforms;
	uniforms.light = (uint32_t)(lightlevel / 255.0f * 256.0f);
	if (fixedlightlev >= 0 || fixedcolormap)
		uniforms.light = 256;
	uniforms.flags = 0;
	uniforms.subsectorDepth = subsectorDepth;

	TriVertex *vertices = PolyVertexBuffer::GetVertices(sub->numlines);
	if (!vertices)
		return;

	if (ceiling)
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[sub->numlines - 1 - i] = PlaneVertex(line->v1, fakeFloor->bottom.plane->ZatPoint(line->v1));
		}
	}
	else
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[i] = PlaneVertex(line->v1, fakeFloor->top.plane->ZatPoint(line->v1));
		}
	}

	PolyDrawArgs args;
	args.uniforms = uniforms;
	args.objectToClip = &worldToClip;
	args.vinput = vertices;
	args.vcount = sub->numlines;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = true;
	args.stenciltestvalue = 0;
	args.stencilwritevalue = 1;
	args.SetTexture(tex);
	args.SetColormap(sub->sector->ColorMap);
	PolyTriangleDrawer::draw(args, TriDrawVariant::DrawNormal, TriBlendMode::Copy);
	PolyTriangleDrawer::draw(args, TriDrawVariant::Stencil, TriBlendMode::Copy);
}

void RenderPolyPlane::Render(const TriMatrix &worldToClip, subsector_t *sub, uint32_t subsectorDepth, bool ceiling, double skyHeight)
{
	sector_t *fakesector = sub->sector->heightsec;
	if (fakesector && (fakesector == sub->sector || (fakesector->MoreFlags & SECF_IGNOREHEIGHTSEC) == SECF_IGNOREHEIGHTSEC))
		fakesector = nullptr;

	bool fakeflooronly = fakesector && (fakesector->MoreFlags & SECF_FAKEFLOORONLY) != SECF_FAKEFLOORONLY;

	FTextureID picnum;
	bool ccw;
	sector_t *frontsector;
	if (ceiling && fakesector && ViewPos.Z < fakesector->floorplane.Zat0())
	{
		picnum = fakesector->GetTexture(sector_t::ceiling);
		ccw = false;
		ceiling = false;
		frontsector = fakesector;
	}
	else if (!ceiling && fakesector && ViewPos.Z >= fakesector->floorplane.Zat0())
	{
		picnum = fakesector->GetTexture(sector_t::ceiling);
		ccw = true;
		frontsector = fakesector;
	}
	else if (ceiling && fakesector && ViewPos.Z > fakesector->ceilingplane.Zat0() && !fakeflooronly)
	{
		picnum = fakesector->GetTexture(sector_t::floor);
		ccw = true;
		frontsector = fakesector;
	}
	else if (!ceiling && fakesector && ViewPos.Z <= fakesector->ceilingplane.Zat0() && !fakeflooronly)
	{
		picnum = fakesector->GetTexture(sector_t::floor);
		ccw = false;
		ceiling = true;
		frontsector = fakesector;
	}
	else
	{
		picnum = sub->sector->GetTexture(ceiling ? sector_t::ceiling : sector_t::floor);
		ccw = true;
		frontsector = sub->sector;
	}

	FTexture *tex = TexMan(picnum);
	if (tex->UseType == FTexture::TEX_Null)
		return;

	bool isSky = picnum == skyflatnum;

	TriUniforms uniforms;
	uniforms.light = (uint32_t)(frontsector->lightlevel / 255.0f * 256.0f);
	if (fixedlightlev >= 0 || fixedcolormap)
		uniforms.light = 256;
	uniforms.flags = 0;
	uniforms.subsectorDepth = isSky ? RenderPolyPortal::SkySubsectorDepth : subsectorDepth;

	TriVertex *vertices = PolyVertexBuffer::GetVertices(sub->numlines);
	if (!vertices)
		return;

	if (ceiling)
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[sub->numlines - 1 - i] = PlaneVertex(line->v1, isSky ? skyHeight : frontsector->ceilingplane.ZatPoint(line->v1));
		}
	}
	else
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[i] = PlaneVertex(line->v1, isSky ? skyHeight : frontsector->floorplane.ZatPoint(line->v1));
		}
	}

	PolyDrawArgs args;
	args.uniforms = uniforms;
	args.objectToClip = &worldToClip;
	args.vinput = vertices;
	args.vcount = sub->numlines;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = ccw;
	args.stenciltestvalue = 0;
	args.stencilwritevalue = 1;
	args.SetColormap(frontsector->ColorMap);

	if (!isSky)
	{
		args.SetTexture(tex);
		PolyTriangleDrawer::draw(args, TriDrawVariant::DrawNormal, TriBlendMode::Copy);
		PolyTriangleDrawer::draw(args, TriDrawVariant::Stencil, TriBlendMode::Copy);
	}
	else
	{
		args.stencilwritevalue = 255;
		PolyTriangleDrawer::draw(args, TriDrawVariant::Stencil, TriBlendMode::Copy);

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
				else if (bothSkyCeiling)
				{
					continue;
				}
			}

			if (ceiling)
			{
				wallvert[0] = PlaneVertex(line->v1, skyHeight);
				wallvert[1] = PlaneVertex(line->v2, skyHeight);
				wallvert[2] = PlaneVertex(line->v2, skyBottomz2);
				wallvert[3] = PlaneVertex(line->v1, skyBottomz1);
			}
			else
			{
				wallvert[0] = PlaneVertex(line->v1, frontsector->floorplane.ZatPoint(line->v1));
				wallvert[1] = PlaneVertex(line->v2, frontsector->floorplane.ZatPoint(line->v2));
				wallvert[2] = PlaneVertex(line->v2, skyHeight);
				wallvert[3] = PlaneVertex(line->v1, skyHeight);
			}

			args.vinput = wallvert;
			args.vcount = 4;
			PolyTriangleDrawer::draw(args, TriDrawVariant::Stencil, TriBlendMode::Copy);
		}
	}
}

TriVertex RenderPolyPlane::PlaneVertex(vertex_t *v1, double height)
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
