/*
**  Polygon Doom software renderer
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
#include "doomstat.h"
#include "doomdata.h"
#include "p_lnspec.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "poly_wall.h"
#include "poly_decal.h"
#include "polyrenderer/poly_renderer.h"
#include "r_sky.h"
#include "polyrenderer/scene/poly_light.h"
#include "polyrenderer/poly_renderthread.h"
#include "g_levellocals.h"
#include "a_dynlight.h"

EXTERN_CVAR(Bool, r_drawmirrors)
EXTERN_CVAR(Bool, r_fogboundary)

bool RenderPolyWall::RenderLine(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth, uint32_t stencilValue, std::vector<PolyTranslucentObject*> &translucentWallsOutput, std::vector<std::unique_ptr<PolyDrawLinePortal>> &linePortals, line_t *lastPortalLine)
{
	double frontceilz1 = frontsector->ceilingplane.ZatPoint(line->v1);
	double frontfloorz1 = frontsector->floorplane.ZatPoint(line->v1);
	double frontceilz2 = frontsector->ceilingplane.ZatPoint(line->v2);
	double frontfloorz2 = frontsector->floorplane.ZatPoint(line->v2);
	double topTexZ = frontsector->GetPlaneTexZ(sector_t::ceiling);
	double bottomTexZ = frontsector->GetPlaneTexZ(sector_t::floor);

	PolyDrawLinePortal *polyportal = nullptr;
	if (line->backsector == nullptr && line->linedef && line->sidedef == line->linedef->sidedef[0] && (line->linedef->special == Line_Mirror && r_drawmirrors))
	{
		if (lastPortalLine == line->linedef ||
			(line->linedef->v1->fX() * clipPlane.A + line->linedef->v1->fY() * clipPlane.B + clipPlane.D <= 0.0f) ||
			(line->linedef->v2->fX() * clipPlane.A + line->linedef->v2->fY() * clipPlane.B + clipPlane.D <= 0.0f))
		{
			return false;
		}

		linePortals.push_back(std::unique_ptr<PolyDrawLinePortal>(new PolyDrawLinePortal(line->linedef)));
		polyportal = linePortals.back().get();
	}
	else if (line->linedef && line->linedef->isVisualPortal())
	{
		if (lastPortalLine == line->linedef ||
			(line->linedef->v1->fX() * clipPlane.A + line->linedef->v1->fY() * clipPlane.B + clipPlane.D <= 0.0f) ||
			(line->linedef->v2->fX() * clipPlane.A + line->linedef->v2->fY() * clipPlane.B + clipPlane.D <= 0.0f))
		{
			return false;
		}

		FLinePortal *portal = line->linedef->getPortal();
		for (auto &p : linePortals)
		{
			if (p->Portal == portal) // To do: what other criterias do we need to check for?
			{
				polyportal = p.get();
				break;
			}
		}
		if (!polyportal)
		{
			linePortals.push_back(std::unique_ptr<PolyDrawLinePortal>(new PolyDrawLinePortal(portal)));
			polyportal = linePortals.back().get();
		}
	}

	RenderPolyWall wall;
	wall.LineSeg = line;
	wall.Line = line->linedef;
	wall.Side = line->sidedef;
	wall.LineSegLine = line->linedef;
	wall.Colormap = GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::walltop]);
	wall.Masked = false;
	wall.SubsectorDepth = subsectorDepth;
	wall.StencilValue = stencilValue;
	wall.SectorLightLevel = frontsector->lightlevel;

	if (line->backsector == nullptr)
	{
		if (line->sidedef)
		{
			wall.SetCoords(line->v1->fPos(), line->v2->fPos(), frontceilz1, frontfloorz1, frontceilz2, frontfloorz2);
			wall.TopTexZ = topTexZ;
			wall.BottomTexZ = bottomTexZ;
			wall.Wallpart = side_t::mid;
			wall.Texture = GetTexture(wall.Line, wall.Side, side_t::mid);
			wall.Polyportal = polyportal;
			wall.Render(thread, worldToClip, clipPlane);
			return true;
		}
	}
	else if (line->PartnerSeg)
	{
		PolyTransferHeights fakeback(line->PartnerSeg->Subsector);
		sector_t *backsector = fakeback.FrontSector;

		double backceilz1 = backsector->ceilingplane.ZatPoint(line->v1);
		double backfloorz1 = backsector->floorplane.ZatPoint(line->v1);
		double backceilz2 = backsector->ceilingplane.ZatPoint(line->v2);
		double backfloorz2 = backsector->floorplane.ZatPoint(line->v2);

		double topceilz1 = frontceilz1;
		double topceilz2 = frontceilz2;
		double topfloorz1 = MAX(MIN(backceilz1, frontceilz1), frontfloorz1);
		double topfloorz2 = MAX(MIN(backceilz2, frontceilz2), frontfloorz2);
		double bottomceilz1 = MIN(MAX(frontfloorz1, backfloorz1), frontceilz1);
		double bottomceilz2 = MIN(MAX(frontfloorz2, backfloorz2), frontceilz2);
		double bottomfloorz1 = frontfloorz1;
		double bottomfloorz2 = frontfloorz2;
		double middleceilz1 = topfloorz1;
		double middleceilz2 = topfloorz2;
		double middlefloorz1 = MIN(bottomceilz1, middleceilz1);
		double middlefloorz2 = MIN(bottomceilz2, middleceilz2);

		bool bothSkyCeiling = frontsector->GetTexture(sector_t::ceiling) == skyflatnum && backsector->GetTexture(sector_t::ceiling) == skyflatnum;
		bool bothSkyFloor = frontsector->GetTexture(sector_t::floor) == skyflatnum && backsector->GetTexture(sector_t::floor) == skyflatnum;

		if ((topceilz1 > topfloorz1 || topceilz2 > topfloorz2) && line->sidedef && !bothSkyCeiling)
		{
			wall.SetCoords(line->v1->fPos(), line->v2->fPos(), topceilz1, topfloorz1, topceilz2, topfloorz2);
			wall.TopTexZ = topTexZ;
			wall.BottomTexZ = MIN(MIN(backceilz1, frontceilz1), MIN(backceilz2, frontceilz2));
			wall.Wallpart = side_t::top;
			wall.Texture = GetTexture(wall.Line, wall.Side, side_t::top);
			wall.Render(thread, worldToClip, clipPlane);
		}

		if ((bottomfloorz1 < bottomceilz1 || bottomfloorz2 < bottomceilz2) && line->sidedef && !bothSkyFloor)
		{
			wall.SetCoords(line->v1->fPos(), line->v2->fPos(), bottomceilz1, bottomfloorz1, bottomceilz2, bottomfloorz2);
			wall.TopTexZ = MAX(MAX(frontfloorz1, backfloorz1), MAX(frontfloorz2, backfloorz2));
			wall.BottomTexZ = bottomTexZ;
			wall.UnpeggedCeil1 = topceilz1;
			wall.UnpeggedCeil2 = topceilz2;
			wall.Wallpart = side_t::bottom;
			wall.Texture = GetTexture(wall.Line, wall.Side, side_t::bottom);
			wall.Render(thread, worldToClip, clipPlane);
		}

		if (line->sidedef)
		{
			wall.SetCoords(line->v1->fPos(), line->v2->fPos(), middleceilz1, middlefloorz1, middleceilz2, middlefloorz2);
			wall.TopTexZ = MAX(middleceilz1, middleceilz2);
			wall.BottomTexZ = MIN(middlefloorz1, middlefloorz2);
			wall.Wallpart = side_t::mid;
			wall.Texture = GetTexture(wall.Line, wall.Side, side_t::mid);
			wall.Masked = true;
			wall.Additive = !!(wall.Line->flags & ML_ADDTRANS);
			wall.Alpha = wall.Line->alpha;
			wall.FogBoundary = IsFogBoundary(frontsector, backsector);

			FTexture *midtex = TexMan(line->sidedef->GetTexture(side_t::mid), true);
			if ((midtex && midtex->UseType != FTexture::TEX_Null) || wall.FogBoundary)
				translucentWallsOutput.push_back(thread->FrameMemory->NewObject<PolyTranslucentWall>(wall));

			if (polyportal)
			{
				wall.Polyportal = polyportal;
				wall.Render(thread, worldToClip, clipPlane);
			}
		}
	}
	return polyportal != nullptr;
}

bool RenderPolyWall::IsFogBoundary(sector_t *front, sector_t *back)
{
	return r_fogboundary && PolyCameraLight::Instance()->FixedColormap() == nullptr && front->Colormap.FadeColor &&
		front->Colormap.FadeColor != back->Colormap.FadeColor &&
		(front->GetTexture(sector_t::ceiling) != skyflatnum || back->GetTexture(sector_t::ceiling) != skyflatnum);
}

void RenderPolyWall::Render3DFloorLine(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth, uint32_t stencilValue, F3DFloor *fakeFloor, std::vector<PolyTranslucentObject*> &translucentWallsOutput)
{
	if (!(fakeFloor->flags & FF_EXISTS)) return;
	if (!(fakeFloor->flags & FF_RENDERPLANES)) return;
	if (fakeFloor->flags & FF_SWIMMABLE) return;
	if (!fakeFloor->model) return;
	if (fakeFloor->alpha == 0) return;

	double frontceilz1 = fakeFloor->top.plane->ZatPoint(line->v1);
	double frontfloorz1 = fakeFloor->bottom.plane->ZatPoint(line->v1);
	double frontceilz2 = fakeFloor->top.plane->ZatPoint(line->v2);
	double frontfloorz2 = fakeFloor->bottom.plane->ZatPoint(line->v2);
	double topTexZ = fakeFloor->model->GetPlaneTexZ(sector_t::ceiling);
	double bottomTexZ = fakeFloor->model->GetPlaneTexZ(sector_t::floor);

	if (frontceilz1 <= frontfloorz1 || frontceilz2 <= frontfloorz2)
		return;

	RenderPolyWall wall;
	wall.LineSeg = line;
	wall.LineSegLine = line->linedef;
	wall.Line = fakeFloor->master;
	wall.Side = fakeFloor->master->sidedef[0];
	wall.Colormap = GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::walltop]);
	wall.SectorLightLevel = frontsector->lightlevel;
	wall.Additive = !!(fakeFloor->flags & FF_ADDITIVETRANS);
	if (!wall.Additive && fakeFloor->alpha == 255)
	{
		wall.Masked = false;
		wall.Alpha = 1.0;
	}
	else
	{
		wall.Masked = true;
		wall.Alpha = fakeFloor->alpha / 255.0;
	}
	wall.SubsectorDepth = subsectorDepth;
	wall.StencilValue = stencilValue;
	wall.SetCoords(line->v1->fPos(), line->v2->fPos(), frontceilz1, frontfloorz1, frontceilz2, frontfloorz2);
	wall.TopTexZ = topTexZ;
	wall.BottomTexZ = bottomTexZ;
	wall.Wallpart = side_t::mid;
	if (fakeFloor->flags & FF_UPPERTEXTURE)
		wall.Texture = GetTexture(line->linedef, line->sidedef, side_t::top);
	else if (fakeFloor->flags & FF_LOWERTEXTURE)
		wall.Texture = GetTexture(line->linedef, line->sidedef, side_t::bottom);
	else
		wall.Texture = GetTexture(wall.Line, wall.Side, side_t::mid);

	if (!wall.Masked)
		wall.Render(thread, worldToClip, clipPlane);
	else
		translucentWallsOutput.push_back(thread->FrameMemory->NewObject<PolyTranslucentWall>(wall));
}

void RenderPolyWall::SetCoords(const DVector2 &v1, const DVector2 &v2, double ceil1, double floor1, double ceil2, double floor2)
{
	this->v1 = v1;
	this->v2 = v2;
	this->ceil1 = ceil1;
	this->floor1 = floor1;
	this->ceil2 = ceil2;
	this->floor2 = floor2;
}

void RenderPolyWall::Render(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane)
{
	bool foggy = false;
	if (!Texture && !Polyportal && !FogBoundary)
		return;

	TriVertex *vertices = thread->FrameMemory->AllocMemory<TriVertex>(4);

	vertices[0].x = (float)v1.X;
	vertices[0].y = (float)v1.Y;
	vertices[0].z = (float)ceil1;
	vertices[0].w = 1.0f;

	vertices[1].x = (float)v2.X;
	vertices[1].y = (float)v2.Y;
	vertices[1].z = (float)ceil2;
	vertices[1].w = 1.0f;

	vertices[2].x = (float)v2.X;
	vertices[2].y = (float)v2.Y;
	vertices[2].z = (float)floor2;
	vertices[2].w = 1.0f;

	vertices[3].x = (float)v1.X;
	vertices[3].y = (float)v1.Y;
	vertices[3].z = (float)floor1;
	vertices[3].w = 1.0f;

	if (Texture)
	{
		PolyWallTextureCoordsU texcoordsU(Texture, LineSeg, LineSegLine, Side, Wallpart);
		PolyWallTextureCoordsV texcoordsVLeft(Texture, Line, Side, Wallpart, ceil1, floor1, UnpeggedCeil1, TopTexZ, BottomTexZ);
		PolyWallTextureCoordsV texcoordsVRght(Texture, Line, Side, Wallpart, ceil2, floor2, UnpeggedCeil2, TopTexZ, BottomTexZ);
		vertices[0].u = (float)texcoordsU.u1;
		vertices[0].v = (float)texcoordsVLeft.v1;
		vertices[1].u = (float)texcoordsU.u2;
		vertices[1].v = (float)texcoordsVRght.v1;
		vertices[2].u = (float)texcoordsU.u2;
		vertices[2].v = (float)texcoordsVRght.v2;
		vertices[3].u = (float)texcoordsU.u1;
		vertices[3].v = (float)texcoordsVLeft.v2;
	}
	else
	{
		for (int i = 0; i < 4; i++)
		{
			vertices[i].u = 0.0f;
			vertices[i].v = 0.0f;
		}
	}

	// Masked walls clamp to the 0-1 range (no texture repeat)
	if (Masked)
	{
		bool wrap = (Line->flags & ML_WRAP_MIDTEX) || (Side->Flags & WALLF_WRAP_MIDTEX);
		if (!wrap)
		{
			ClampHeight(vertices[0], vertices[3]);
			ClampHeight(vertices[1], vertices[2]);
		}
	}

	PolyDrawArgs args;
	args.SetLight(Colormap, GetLightLevel(), PolyRenderer::Instance()->Light.WallGlobVis(foggy), false);
	args.SetTransform(&worldToClip);
	args.SetFaceCullCCW(true);
	args.SetStencilTestValue(StencilValue);
	args.SetWriteStencil(true, StencilValue + 1);
	if (Texture && !Polyportal)
		args.SetTexture(Texture);
	args.SetClipPlane(0, clipPlane);

	SetDynLights(thread, args);

	if (FogBoundary)
	{
		args.SetStyle(TriBlendMode::FogBoundary);
		args.SetColor(0xffffffff, 254);
		args.SetDepthTest(true);
		args.SetWriteDepth(true);
		args.SetWriteStencil(false);
		args.DrawArray(thread, vertices, 4, PolyDrawMode::TriangleFan);
		if (!Texture)
			return;
	}

	if (Polyportal)
	{
		args.SetWriteStencil(true, Polyportal->StencilValue);
		args.SetWriteColor(false);
		args.SetWriteDepth(false);
		args.DrawArray(thread, vertices, 4, PolyDrawMode::TriangleFan);
		Polyportal->Shape.push_back({ vertices, 4, true });
	}
	else if (!Masked)
	{
		args.SetStyle(TriBlendMode::TextureOpaque);
		DrawStripes(thread, args, vertices);
	}
	else
	{
		double srcalpha = MIN(Alpha, 1.0);
		double destalpha = Additive ? 1.0 : 1.0 - srcalpha;
		args.SetStyle(TriBlendMode::TextureAdd, srcalpha, destalpha);
		args.SetDepthTest(true);
		args.SetWriteDepth(true);
		args.SetWriteStencil(false);
		DrawStripes(thread, args, vertices);
	}

	RenderPolyDecal::RenderWallDecals(thread, worldToClip, clipPlane, LineSeg, StencilValue);
}

void RenderPolyWall::SetDynLights(PolyRenderThread *thread, PolyDrawArgs &args)
{
	FLightNode *light_list = (LineSeg && LineSeg->sidedef) ? LineSeg->sidedef->lighthead : nullptr;

	auto cameraLight = PolyCameraLight::Instance();
	if ((cameraLight->FixedLightLevel() >= 0) || (cameraLight->FixedColormap() != nullptr))
	{
		args.SetLights(nullptr, 0); // [SP] Don't draw dynlights if invul/lightamp active
		return;
	}

	// Calculate max lights that can touch the wall so we can allocate memory for the list
	int max_lights = 0;
	FLightNode *cur_node = light_list;
	while (cur_node)
	{
		if (!(cur_node->lightsource->flags2&MF2_DORMANT))
			max_lights++;
		cur_node = cur_node->nextLight;
	}

	if (max_lights == 0)
	{
		args.SetLights(nullptr, 0);
		return;
	}

	int dc_num_lights = 0;
	PolyLight *dc_lights = thread->FrameMemory->AllocMemory<PolyLight>(max_lights);

	// Setup lights
	cur_node = light_list;
	while (cur_node)
	{
		if (!(cur_node->lightsource->flags2&MF2_DORMANT))
		{
			bool is_point_light = (cur_node->lightsource->lightflags & LF_ATTENUATE) != 0;

			// To do: cull lights not touching wall

			uint32_t red = cur_node->lightsource->GetRed();
			uint32_t green = cur_node->lightsource->GetGreen();
			uint32_t blue = cur_node->lightsource->GetBlue();

			auto &light = dc_lights[dc_num_lights++];
			light.x = (float)cur_node->lightsource->X();
			light.y = (float)cur_node->lightsource->Y();
			light.z = (float)cur_node->lightsource->Z();
			light.radius = 256.0f / cur_node->lightsource->GetRadius();
			light.color = (red << 16) | (green << 8) | blue;
			if (is_point_light)
				light.radius = -light.radius;
		}

		cur_node = cur_node->nextLight;
	}

	args.SetLights(dc_lights, dc_num_lights);

	// Face normal:
	float dx = (float)(v2.X - v1.X);
	float dy = (float)(v2.Y - v1.Y);
	float nx = dy;
	float ny = -dx;
	float lensqr = nx * nx + ny * ny;
	float rcplen = 1.0f / sqrt(lensqr);
	nx *= rcplen;
	ny *= rcplen;
	args.SetNormal({ nx, ny, 0.0f });
}

void RenderPolyWall::DrawStripes(PolyRenderThread *thread, PolyDrawArgs &args, TriVertex *vertices)
{
	const auto &lightlist = Line->frontsector->e->XFloor.lightlist;
	if (lightlist.Size() > 0)
	{
		PolyClipPlane topPlane;

		for (unsigned int i = 0; i < lightlist.Size(); i++)
		{
			lightlist_t *lit = &lightlist[i];

			DVector3 normal = lit->plane.Normal();
			double d = lit->plane.fD();
			if (normal.Z < 0.0)
			{
				normal = -normal;
				d = -d;
			}

			PolyClipPlane bottomPlane = { (float)normal.X, (float)normal.Y, (float)normal.Z, (float)d };

			args.SetClipPlane(1, topPlane);
			args.SetClipPlane(2, bottomPlane);
			args.DrawArray(thread, vertices, 4, PolyDrawMode::TriangleFan);

			FDynamicColormap *basecolormap = GetColorTable(lit->extra_colormap, Line->frontsector->SpecialColors[sector_t::walltop]);

			bool foggy = false;
			int lightlevel;
			PolyCameraLight *cameraLight = PolyCameraLight::Instance();
			if (cameraLight->FixedLightLevel() >= 0 || cameraLight->FixedColormap())
			{
				lightlevel = 255;
			}
			else
			{
				int actualextralight = foggy ? 0 : PolyRenderer::Instance()->Viewpoint.extralight << 4;
				lightlevel = clamp(Side->GetLightLevel(foggy, *lit->p_lightlevel) + actualextralight, 0, 255);
			}
			args.SetLight(basecolormap, lightlevel, PolyRenderer::Instance()->Light.WallGlobVis(foggy), false);

			topPlane = { (float)-normal.X, (float)-normal.Y, (float)-normal.Z, (float)-d };
		}

		args.SetClipPlane(1, topPlane);
		args.SetClipPlane(2, PolyClipPlane());
		args.DrawArray(thread, vertices, 4, PolyDrawMode::TriangleFan);
	}
	else
	{
		args.DrawArray(thread, vertices, 4, PolyDrawMode::TriangleFan);
	}
}

void RenderPolyWall::ClampHeight(TriVertex &v1, TriVertex &v2)
{
	float top = v1.z;
	float bottom = v2.z;
	float texv1 = v1.v;
	float texv2 = v2.v;
	float delta = (texv2 - texv1);

	float t1 = texv1 < 0.0f ? -texv1 / delta : 0.0f;
	float t2 = texv2 > 1.0f ? (1.0f - texv1) / delta : 1.0f;
	float inv_t1 = 1.0f - t1;
	float inv_t2 = 1.0f - t2;

	v1.z = top * inv_t1 + bottom * t1;
	v1.v = texv1 * inv_t1 + texv2 * t1;

	v2.z = top * inv_t2 + bottom * t2;
	v2.v = texv1 * inv_t2 + texv2 * t2;
}

FTexture *RenderPolyWall::GetTexture(const line_t *line, const side_t *side, side_t::ETexpart texpart)
{
	FTexture *tex = TexMan(side->GetTexture(texpart), true);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
	{
		// Mapping error. Doom floodfills this with a plane.
		// This code doesn't do that, but at least it uses the "right" texture..

		if (line && line->backsector && line->sidedef[0] == side)
		{
			if (texpart == side_t::top)
				tex = TexMan(line->backsector->GetTexture(sector_t::ceiling), true);
			else if (texpart == side_t::bottom)
				tex = TexMan(line->backsector->GetTexture(sector_t::floor), true);
		}
		if (line && line->backsector && line->sidedef[1] == side)
		{
			if (texpart == side_t::top)
				tex = TexMan(line->frontsector->GetTexture(sector_t::ceiling), true);
			else if (texpart == side_t::bottom)
				tex = TexMan(line->frontsector->GetTexture(sector_t::floor), true);
		}

		if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
			return nullptr;
	}
	return tex;
}

int RenderPolyWall::GetLightLevel()
{
	PolyCameraLight *cameraLight = PolyCameraLight::Instance();
	if (cameraLight->FixedLightLevel() >= 0 || cameraLight->FixedColormap())
	{
		return 255;
	}
	else
	{
		bool foggy = false;
		int actualextralight = foggy ? 0 : PolyRenderer::Instance()->Viewpoint.extralight << 4;
		return clamp(Side->GetLightLevel(foggy, SectorLightLevel) + actualextralight, 0, 255);
	}
}

/////////////////////////////////////////////////////////////////////////////

PolyWallTextureCoordsU::PolyWallTextureCoordsU(FTexture *tex, const seg_t *lineseg, const line_t *line, const side_t *side, side_t::ETexpart wallpart)
{
	// Calculate the U texture coordinate for the line
	double lineu1 = side->GetTextureXOffset(wallpart);
	double lineu2 = side->GetTextureXOffset(wallpart) + line->sidedef[0]->TexelLength * side->GetTextureXScale(wallpart);
	lineu1 *= tex->Scale.X / tex->GetWidth();
	lineu2 *= tex->Scale.X / tex->GetWidth();

	// Calculate where we are on the lineseg
	double t1, t2;
	if (fabs(line->delta.X) > fabs(line->delta.Y))
	{
		t1 = (lineseg->v1->fX() - line->v1->fX()) / line->delta.X;
		t2 = (lineseg->v2->fX() - line->v1->fX()) / line->delta.X;
	}
	else
	{
		t1 = (lineseg->v1->fY() - line->v1->fY()) / line->delta.Y;
		t2 = (lineseg->v2->fY() - line->v1->fY()) / line->delta.Y;
	}

	// Check if lineseg is the backside of the line
	if (t2 < t1)
	{
		std::swap(lineu1, lineu2);
	}

	// Calculate texture coordinates for the lineseg
	u1 = (1.0 - t1) * lineu1 + t1 * lineu2;
	u2 = (1.0 - t2) * lineu1 + t2 * lineu2;
}

/////////////////////////////////////////////////////////////////////////////

PolyWallTextureCoordsV::PolyWallTextureCoordsV(FTexture *tex, const line_t *line, const side_t *side, side_t::ETexpart wallpart, double topz, double bottomz, double unpeggedceil, double topTexZ, double bottomTexZ)
{
	double yoffset = side->GetTextureYOffset(wallpart);
	if (tex->bWorldPanning)
		yoffset *= side->GetTextureYScale(wallpart) * tex->Scale.Y;

	switch (wallpart)
	{
	default:
	case side_t::mid:
		CalcVMidPart(tex, line, side, topTexZ, bottomTexZ, yoffset);
		break;
	case side_t::top:
		CalcVTopPart(tex, line, side, topTexZ, bottomTexZ, yoffset);
		break;
	case side_t::bottom:
		CalcVBottomPart(tex, line, side, topTexZ, bottomTexZ, unpeggedceil, yoffset);
		break;
	}

	v1 *= tex->Scale.Y / tex->GetHeight();
	v2 *= tex->Scale.Y / tex->GetHeight();

	double texZHeight = (bottomTexZ - topTexZ);
	if (texZHeight > 0.0f || texZHeight < -0.0f)
	{
		double t1 = (topz - topTexZ) / texZHeight;
		double t2 = (bottomz - topTexZ) / texZHeight;
		double vorig1 = v1;
		double vorig2 = v2;
		v1 = vorig1 * (1.0f - t1) + vorig2 * t1;
		v2 = vorig1 * (1.0f - t2) + vorig2 * t2;
	}
}

void PolyWallTextureCoordsV::CalcVTopPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double yoffset)
{
	bool pegged = (line->flags & ML_DONTPEGTOP) == 0;
	if (pegged) // bottom to top
	{
		double texHeight = tex->GetHeight() / tex->Scale.Y;
		v1 = (topz - bottomz) * side->GetTextureYScale(side_t::top) - yoffset;
		v2 = -yoffset;
		v1 = texHeight - v1;
		v2 = texHeight - v2;
	}
	else // top to bottom
	{
		v1 = yoffset;
		v2 = (topz - bottomz) * side->GetTextureYScale(side_t::top) + yoffset;
	}
}

void PolyWallTextureCoordsV::CalcVMidPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double yoffset)
{
	bool pegged = (line->flags & ML_DONTPEGBOTTOM) == 0;
	if (pegged) // top to bottom
	{
		v1 = yoffset;
		v2 = (topz - bottomz) * side->GetTextureYScale(side_t::mid) + yoffset;
	}
	else // bottom to top
	{
		double texHeight = tex->GetHeight() / tex->Scale.Y;
		v1 = yoffset - (topz - bottomz) * side->GetTextureYScale(side_t::mid);
		v2 = yoffset;
		v1 = texHeight + v1;
		v2 = texHeight + v2;
	}
}

void PolyWallTextureCoordsV::CalcVBottomPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double unpeggedceil, double yoffset)
{
	bool pegged = (line->flags & ML_DONTPEGBOTTOM) == 0;
	if (pegged) // top to bottom
	{
		v1 = yoffset;
		v2 = yoffset + (topz - bottomz) * side->GetTextureYScale(side_t::bottom);
	}
	else
	{
		v1 = yoffset + (unpeggedceil - topz) * side->GetTextureYScale(side_t::bottom);
		v2 = yoffset + (unpeggedceil - bottomz) * side->GetTextureYScale(side_t::bottom);
	}
}
