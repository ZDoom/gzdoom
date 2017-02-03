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
#include "poly_plane.h"
#include "poly_portal.h"
#include "polyrenderer/poly_renderer.h"
#include "r_sky.h"
#include "swrenderer/scene/r_light.h"

EXTERN_CVAR(Int, r_3dfloors)

void RenderPolyPlane::RenderPlanes(const TriMatrix &worldToClip, const Vec4f &clipPlane, PolyCull &cull, subsector_t *sub, uint32_t subsectorDepth, uint32_t stencilValue, double skyCeilingHeight, double skyFloorHeight, std::vector<std::unique_ptr<PolyDrawSectorPortal>> &sectorPortals)
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
				plane.Render3DFloor(worldToClip, clipPlane, sub, subsectorDepth, stencilValue, false, fakeFloor);
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
				plane.Render3DFloor(worldToClip, clipPlane, sub, subsectorDepth, stencilValue, true, fakeFloor);
			}
		}
	}

	plane.Render(worldToClip, clipPlane, cull, sub, subsectorDepth, stencilValue, true, skyCeilingHeight, sectorPortals);
	plane.Render(worldToClip, clipPlane, cull, sub, subsectorDepth, stencilValue, false, skyFloorHeight, sectorPortals);
}

void RenderPolyPlane::Render3DFloor(const TriMatrix &worldToClip, const Vec4f &clipPlane, subsector_t *sub, uint32_t subsectorDepth, uint32_t stencilValue, bool ceiling, F3DFloor *fakeFloor)
{
	FTextureID picnum = ceiling ? *fakeFloor->bottom.texture : *fakeFloor->top.texture;
	FTexture *tex = TexMan(picnum);
	if (tex->UseType == FTexture::TEX_Null)
		return;

	swrenderer::CameraLight *cameraLight = swrenderer::CameraLight::Instance();

	int lightlevel = 255;
	if (cameraLight->FixedLightLevel() < 0 && sub->sector->e->XFloor.lightlist.Size())
	{
		lightlist_t *light = P_GetPlaneLight(sub->sector, &sub->sector->ceilingplane, false);
		//basecolormap = light->extra_colormap;
		lightlevel = *light->p_lightlevel;
	}

	UVTransform xform(ceiling ? fakeFloor->top.model->planes[sector_t::ceiling].xform : fakeFloor->top.model->planes[sector_t::floor].xform, tex);

	PolyDrawArgs args;
	args.uniforms.globvis = (float)swrenderer::LightVisibility::Instance()->SlopePlaneGlobVis() * 48.0f;
	args.uniforms.light = (uint32_t)(lightlevel / 255.0f * 256.0f);
	if (cameraLight->FixedLightLevel() >= 0 || cameraLight->FixedColormap())
		args.uniforms.light = 256;
	args.uniforms.flags = 0;
	args.uniforms.subsectorDepth = subsectorDepth;

	TriVertex *vertices = PolyVertexBuffer::GetVertices(sub->numlines);
	if (!vertices)
		return;

	if (ceiling)
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[sub->numlines - 1 - i] = PlaneVertex(line->v1, fakeFloor->bottom.plane->ZatPoint(line->v1), xform);
		}
	}
	else
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[i] = PlaneVertex(line->v1, fakeFloor->top.plane->ZatPoint(line->v1), xform);
		}
	}

	args.objectToClip = &worldToClip;
	args.vinput = vertices;
	args.vcount = sub->numlines;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = true;
	args.stenciltestvalue = stencilValue;
	args.stencilwritevalue = stencilValue + 1;
	args.SetTexture(tex);
	args.SetColormap(sub->sector->ColorMap);
	args.SetClipPlane(clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w);
	args.blendmode = TriBlendMode::Copy;
	PolyTriangleDrawer::draw(args);
}

void RenderPolyPlane::Render(const TriMatrix &worldToClip, const Vec4f &clipPlane, PolyCull &cull, subsector_t *sub, uint32_t subsectorDepth, uint32_t stencilValue, bool ceiling, double skyHeight, std::vector<std::unique_ptr<PolyDrawSectorPortal>> &sectorPortals)
{
	std::vector<PolyPortalSegment> portalSegments;
	FSectorPortal *portal = nullptr;// sub->sector->ValidatePortal(ceiling ? sector_t::ceiling : sector_t::floor);
	PolyDrawSectorPortal *polyportal = nullptr;
	if (portal && (portal->mFlags & PORTSF_INSKYBOX) == PORTSF_INSKYBOX) // Do not recurse into portals we already recursed into
		portal = nullptr;
	if (portal)
	{
		for (auto &p : sectorPortals)
		{
			if (p->Portal == portal) // To do: what other criteria do we need to check for?
			{
				polyportal = p.get();
				break;
			}
		}
		if (!polyportal)
		{
			sectorPortals.push_back(std::make_unique<PolyDrawSectorPortal>(portal, ceiling));
			polyportal = sectorPortals.back().get();
		}

		// Calculate portal clipping

		DVector2 v;
		bool inside = true;
		double vdist = 1.0e10;

		portalSegments.reserve(sub->numlines);
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];

			DVector2 pt1 = line->v1->fPos() - ViewPos;
			DVector2 pt2 = line->v2->fPos() - ViewPos;
			if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
				inside = false;

			double dist = pt1.LengthSquared();
			if (dist < vdist)
			{
				v = line->v1->fPos();
				vdist = dist;
			}
			dist = pt2.LengthSquared();
			if (dist < vdist)
			{
				v = line->v2->fPos();
				vdist = dist;
			}

			int sx1, sx2;
			LineSegmentRange range = cull.GetSegmentRangeForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), sx1, sx2);
			if (range == LineSegmentRange::HasSegment)
				portalSegments.push_back({ sx1, sx2 });
		}

		if (inside)
		{
			polyportal->PortalPlane = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
		}
		else if(polyportal->PortalPlane == Vec4f(0.0f) || Vec4f::dot(polyportal->PortalPlane, Vec4f((float)v.X, (float)v.Y, 0.0f, 1.0f)) > 0.0f)
		{
			DVector2 planePos = v;
			DVector2 planeNormal = v - ViewPos;
			planeNormal.MakeUnit();
			double planeD = -(planeNormal | (planePos + planeNormal * 0.001));
			polyportal->PortalPlane = Vec4f((float)planeNormal.X, (float)planeNormal.Y, 0.0f, (float)planeD);
		}
	}
	
	sector_t *fakesector = sub->sector->heightsec;
	if (fakesector && (fakesector == sub->sector || (fakesector->MoreFlags & SECF_IGNOREHEIGHTSEC) == SECF_IGNOREHEIGHTSEC))
		fakesector = nullptr;

	bool fakeflooronly = fakesector && (fakesector->MoreFlags & SECF_FAKEFLOORONLY) == SECF_FAKEFLOORONLY;

	FTextureID picnum;
	bool ccw;
	sector_t *frontsector;
	if (fakesector)
	{
		// Floor and ceiling texture needs to be swapped sometimes? Why?? :(

		if (ViewPos.Z < fakesector->floorplane.Zat0()) // In water
		{
			if (ceiling)
			{
				picnum = fakesector->GetTexture(sector_t::ceiling);
				ceiling = false;
				frontsector = fakesector;
				ccw = false;
			}
			else
			{
				picnum = fakesector->GetTexture(sector_t::floor);
				frontsector = sub->sector;
				ccw = true;
			}
		}
		else if (ViewPos.Z >= fakesector->ceilingplane.Zat0() && !fakeflooronly) // In ceiling water
		{
			if (ceiling)
			{
				picnum = fakesector->GetTexture(sector_t::ceiling);
				frontsector = sub->sector;
				ccw = true;
			}
			else
			{
				picnum = fakesector->GetTexture(sector_t::floor);
				frontsector = fakesector;
				ccw = false;
				ceiling = true;
			}
		}
		else if (!ceiling) // Water surface
		{
			picnum = fakesector->GetTexture(sector_t::ceiling);
			frontsector = fakesector;
			ccw = true;
		}
		else if (!fakeflooronly) // Ceiling water surface
		{
			picnum = fakesector->GetTexture(sector_t::floor);
			frontsector = fakesector;
			ccw = true;
		}
		else // Upper ceiling
		{
			picnum = sub->sector->GetTexture(sector_t::ceiling);
			ccw = true;
			frontsector = sub->sector;
		}
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

	UVTransform transform(ceiling ? frontsector->planes[sector_t::ceiling].xform : frontsector->planes[sector_t::floor].xform, tex);

	swrenderer::CameraLight *cameraLight = swrenderer::CameraLight::Instance();

	PolyDrawArgs args;
	args.uniforms.globvis = (float)swrenderer::LightVisibility::Instance()->SlopePlaneGlobVis() * 48.0f;
	args.uniforms.light = (uint32_t)(frontsector->lightlevel / 255.0f * 256.0f);
	if (cameraLight->FixedLightLevel() >= 0 || cameraLight->FixedColormap())
		args.uniforms.light = 256;
	args.uniforms.flags = 0;
	args.uniforms.subsectorDepth = isSky ? RenderPolyScene::SkySubsectorDepth : subsectorDepth;

	TriVertex *vertices = PolyVertexBuffer::GetVertices(sub->numlines);
	if (!vertices)
		return;

	if (ceiling)
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[sub->numlines - 1 - i] = PlaneVertex(line->v1, isSky ? skyHeight : frontsector->ceilingplane.ZatPoint(line->v1), transform);
		}
	}
	else
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[i] = PlaneVertex(line->v1, isSky ? skyHeight : frontsector->floorplane.ZatPoint(line->v1), transform);
		}
	}

	args.objectToClip = &worldToClip;
	args.vinput = vertices;
	args.vcount = sub->numlines;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = ccw;
	args.stenciltestvalue = stencilValue;
	args.stencilwritevalue = stencilValue + 1;
	args.SetColormap(frontsector->ColorMap);
	args.SetClipPlane(clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w);

	if (!isSky)
	{
		if (!portal)
		{
			args.SetTexture(tex);
			args.blendmode = TriBlendMode::Copy;
			PolyTriangleDrawer::draw(args);
		}
		else
		{
			args.stencilwritevalue = polyportal->StencilValue;
			args.writeColor = false;
			args.writeSubsector = false;
			PolyTriangleDrawer::draw(args);
			polyportal->Shape.push_back({ args.vinput, args.vcount, args.ccw, subsectorDepth });
			polyportal->Segments.insert(polyportal->Segments.end(), portalSegments.begin(), portalSegments.end());
		}
	}
	else
	{
		if (portal)
		{
			args.stencilwritevalue = polyportal->StencilValue;
			polyportal->Shape.push_back({ args.vinput, args.vcount, args.ccw, subsectorDepth });
			polyportal->Segments.insert(polyportal->Segments.end(), portalSegments.begin(), portalSegments.end());
		}
		else
		{
			args.stencilwritevalue = 255;
		}

		args.writeColor = false;
		args.writeSubsector = false;
		PolyTriangleDrawer::draw(args);

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
				wallvert[0] = PlaneVertex(line->v1, skyHeight, transform);
				wallvert[1] = PlaneVertex(line->v2, skyHeight, transform);
				wallvert[2] = PlaneVertex(line->v2, skyBottomz2, transform);
				wallvert[3] = PlaneVertex(line->v1, skyBottomz1, transform);
			}
			else
			{
				wallvert[0] = PlaneVertex(line->v1, frontsector->floorplane.ZatPoint(line->v1), transform);
				wallvert[1] = PlaneVertex(line->v2, frontsector->floorplane.ZatPoint(line->v2), transform);
				wallvert[2] = PlaneVertex(line->v2, skyHeight, transform);
				wallvert[3] = PlaneVertex(line->v1, skyHeight, transform);
			}

			args.vinput = wallvert;
			args.vcount = 4;
			PolyTriangleDrawer::draw(args);
			
			if (portal)
			{
				polyportal->Shape.push_back({ args.vinput, args.vcount, args.ccw, subsectorDepth });
				polyportal->Segments.insert(polyportal->Segments.end(), portalSegments.begin(), portalSegments.end());
			}
		}
	}
}

TriVertex RenderPolyPlane::PlaneVertex(vertex_t *v1, double height, const UVTransform &transform)
{
	TriVertex v;
	v.x = (float)v1->fPos().X;
	v.y = (float)v1->fPos().Y;
	v.z = (float)height;
	v.w = 1.0f;
	v.varying[0] = transform.GetU(v.x, v.y);
	v.varying[1] = transform.GetV(v.x, v.y);
	return v;
}

RenderPolyPlane::UVTransform::UVTransform(const FTransform &transform, FTexture *tex)
{
	if (tex)
	{
		xscale = (float)(transform.xScale * tex->Scale.X / tex->GetWidth());
		yscale = (float)(transform.yScale * tex->Scale.Y / tex->GetHeight());

		double planeang = (transform.Angle + transform.baseAngle).Radians();
		cosine = (float)cos(planeang);
		sine = (float)sin(planeang);

		xOffs = (float)transform.xOffs;
		yOffs = (float)transform.yOffs;
	}
	else
	{
		xscale = 1.0f / 64.0f;
		yscale = 1.0f / 64.0f;
		cosine = 1.0f;
		sine = 0.0f;
		xOffs = 0.0f;
		yOffs = 0.0f;
	}
}

float RenderPolyPlane::UVTransform::GetU(float x, float y) const
{
	return (xOffs + x * cosine - y * sine) * xscale;
}

float RenderPolyPlane::UVTransform::GetV(float x, float y) const
{
	return (yOffs - x * sine - y * cosine) * yscale;
}
