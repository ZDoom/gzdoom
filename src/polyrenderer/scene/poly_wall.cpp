/*
**  Handling drawing a wall
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
#include "g_levellocals.h"

EXTERN_CVAR(Bool, r_drawmirrors)

bool RenderPolyWall::RenderLine(const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, PolyCull &cull, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth, uint32_t stencilValue, std::vector<PolyTranslucentObject*> &translucentWallsOutput, std::vector<std::unique_ptr<PolyDrawLinePortal>> &linePortals, line_t *lastPortalLine)
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
	wall.Colormap = GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::walltop]);
	wall.Masked = false;
	wall.SubsectorDepth = subsectorDepth;
	wall.StencilValue = stencilValue;

	if (line->backsector == nullptr)
	{
		if (line->sidedef)
		{
			wall.SetCoords(line->v1->fPos(), line->v2->fPos(), frontceilz1, frontfloorz1, frontceilz2, frontfloorz2);
			wall.TopTexZ = topTexZ;
			wall.BottomTexZ = bottomTexZ;
			wall.Texpart = side_t::mid;
			wall.Polyportal = polyportal;
			wall.Render(worldToClip, clipPlane, cull);
			return true;
		}
	}
	else
	{
		sector_t *backsector = (line->backsector != line->frontsector) ? line->backsector : line->frontsector;

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
			wall.Texpart = side_t::top;
			wall.Render(worldToClip, clipPlane, cull);
		}

		if ((bottomfloorz1 < bottomceilz1 || bottomfloorz2 < bottomceilz2) && line->sidedef && !bothSkyFloor)
		{
			wall.SetCoords(line->v1->fPos(), line->v2->fPos(), bottomceilz1, bottomfloorz1, bottomceilz2, bottomfloorz2);
			wall.TopTexZ = MAX(MAX(frontfloorz1, backfloorz1), MAX(frontfloorz2, backfloorz2));
			wall.BottomTexZ = bottomTexZ;
			wall.UnpeggedCeil1 = topceilz1;
			wall.UnpeggedCeil2 = topceilz2;
			wall.Texpart = side_t::bottom;
			wall.Render(worldToClip, clipPlane, cull);
		}

		if (line->sidedef)
		{
			wall.SetCoords(line->v1->fPos(), line->v2->fPos(), middleceilz1, middlefloorz1, middleceilz2, middlefloorz2);
			wall.TopTexZ = MAX(middleceilz1, middleceilz2);
			wall.BottomTexZ = MIN(middlefloorz1, middlefloorz2);
			wall.Texpart = side_t::mid;
			wall.Masked = true;

			FTexture *midtex = TexMan(line->sidedef->GetTexture(side_t::mid), true);
			if (midtex && midtex->UseType != FTexture::TEX_Null)
				translucentWallsOutput.push_back(PolyRenderer::Instance()->FrameMemory.NewObject<PolyTranslucentObject>(wall));

			if (polyportal)
			{
				wall.Polyportal = polyportal;
				wall.Render(worldToClip, clipPlane, cull);
			}
		}
	}
	return polyportal != nullptr;
}

void RenderPolyWall::Render3DFloorLine(const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, PolyCull &cull, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth, uint32_t stencilValue, F3DFloor *fakeFloor, std::vector<PolyTranslucentObject*> &translucentWallsOutput)
{
	double frontceilz1 = fakeFloor->top.plane->ZatPoint(line->v1);
	double frontfloorz1 = fakeFloor->bottom.plane->ZatPoint(line->v1);
	double frontceilz2 = fakeFloor->top.plane->ZatPoint(line->v2);
	double frontfloorz2 = fakeFloor->bottom.plane->ZatPoint(line->v2);
	double topTexZ = frontsector->GetPlaneTexZ(sector_t::ceiling);
	double bottomTexZ = frontsector->GetPlaneTexZ(sector_t::floor);

	RenderPolyWall wall;
	wall.LineSeg = line;
	wall.Line = fakeFloor->master;
	wall.Side = fakeFloor->master->sidedef[0];
	wall.Colormap = GetColorTable(frontsector->Colormap, frontsector->SpecialColors[sector_t::walltop]);
	wall.Masked = false;
	wall.SubsectorDepth = subsectorDepth;
	wall.StencilValue = stencilValue;
	wall.SetCoords(line->v1->fPos(), line->v2->fPos(), frontceilz1, frontfloorz1, frontceilz2, frontfloorz2);
	wall.TopTexZ = topTexZ;
	wall.BottomTexZ = bottomTexZ;
	wall.UnpeggedCeil1 = frontceilz1;
	wall.UnpeggedCeil2 = frontceilz2;
	wall.Texpart = side_t::mid;
	wall.Render(worldToClip, clipPlane, cull);
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

void RenderPolyWall::Render(const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, PolyCull &cull)
{
	bool foggy = false;
	FTexture *tex = GetTexture();
	if (!tex && !Polyportal)
		return;

	TriVertex *vertices = PolyRenderer::Instance()->FrameMemory.AllocMemory<TriVertex>(4);

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

	if (tex)
	{
		PolyWallTextureCoordsU texcoordsU(tex, LineSeg, Line, Side, Texpart);
		PolyWallTextureCoordsV texcoordsVLeft(tex, Line, Side, Texpart, ceil1, floor1, UnpeggedCeil1, TopTexZ, BottomTexZ);
		PolyWallTextureCoordsV texcoordsVRght(tex, Line, Side, Texpart, ceil2, floor2, UnpeggedCeil2, TopTexZ, BottomTexZ);
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
		ClampHeight(vertices[0], vertices[3]);
		ClampHeight(vertices[1], vertices[2]);
	}

	PolyDrawArgs args;
	args.SetLight(GetColorTable(Line->frontsector->Colormap, Line->frontsector->SpecialColors[sector_t::walltop]), GetLightLevel(), PolyRenderer::Instance()->Light.WallGlobVis(foggy), false);
	args.SetTransform(&worldToClip);
	args.SetFaceCullCCW(true);
	args.SetStencilTestValue(StencilValue);
	args.SetWriteStencil(true, StencilValue + 1);
	if (tex && !Polyportal)
		args.SetTexture(tex);
	args.SetClipPlane(clipPlane);

	if (Polyportal)
	{
		args.SetWriteStencil(true, Polyportal->StencilValue);
		args.SetWriteColor(false);
		args.SetWriteDepth(false);
		args.DrawArray(vertices, 4, PolyDrawMode::TriangleFan);
		Polyportal->Shape.push_back({ vertices, 4, true });
	}
	else if (!Masked)
	{
		args.SetStyle(TriBlendMode::TextureOpaque);
		args.DrawArray(vertices, 4, PolyDrawMode::TriangleFan);
	}
	else
	{
		bool addtrans = !!(Line->flags & ML_ADDTRANS);
		double srcalpha = MIN(Line->alpha, 1.0);
		double destalpha = addtrans ? 1.0 : 1.0 - srcalpha;
		args.SetStyle(TriBlendMode::TextureAdd, srcalpha, destalpha);
		args.SetDepthTest(true);
		args.SetWriteDepth(true);
		args.SetWriteStencil(false);
		args.DrawArray(vertices, 4, PolyDrawMode::TriangleFan);
	}

	RenderPolyDecal::RenderWallDecals(worldToClip, clipPlane, LineSeg, StencilValue);
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

FTexture *RenderPolyWall::GetTexture()
{
	FTexture *tex = TexMan(Side->GetTexture(Texpart), true);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
	{
		// Mapping error. Doom floodfills this with a plane.
		// This code doesn't do that, but at least it uses the "right" texture..

		if (Line && Line->backsector && Line->sidedef[0] == Side)
		{
			if (Texpart == side_t::top)
				tex = TexMan(Line->backsector->GetTexture(sector_t::ceiling), true);
			else if (Texpart == side_t::bottom)
				tex = TexMan(Line->backsector->GetTexture(sector_t::floor), true);
		}
		if (Line && Line->backsector && Line->sidedef[1] == Side)
		{
			if (Texpart == side_t::top)
				tex = TexMan(Line->frontsector->GetTexture(sector_t::ceiling), true);
			else if (Texpart == side_t::bottom)
				tex = TexMan(Line->frontsector->GetTexture(sector_t::floor), true);
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
		return clamp(Side->GetLightLevel(foggy, LineSeg->frontsector->lightlevel) + actualextralight, 0, 255);
	}
}

/////////////////////////////////////////////////////////////////////////////

PolyWallTextureCoordsU::PolyWallTextureCoordsU(FTexture *tex, const seg_t *lineseg, const line_t *line, const side_t *side, side_t::ETexpart texpart)
{
	double t1, t2;
	double deltaX = line->v2->fX() - line->v1->fX();
	double deltaY = line->v2->fY() - line->v1->fY();
	if (fabs(deltaX) > fabs(deltaY))
	{
		t1 = (lineseg->v1->fX() - line->v1->fX()) / deltaX;
		t2 = (lineseg->v2->fX() - line->v1->fX()) / deltaX;
	}
	else
	{
		t1 = (lineseg->v1->fY() - line->v1->fY()) / deltaY;
		t2 = (lineseg->v2->fY() - line->v1->fY()) / deltaY;
	}

	int texWidth = tex->GetWidth();
	double uscale = side->GetTextureXScale(texpart) * tex->Scale.X;
	u1 = t1 * side->TexelLength + side->GetTextureXOffset(texpart);
	u2 = t2 * side->TexelLength + side->GetTextureXOffset(texpart);
	u1 *= uscale;
	u2 *= uscale;
	u1 /= texWidth;
	u2 /= texWidth;
}

/////////////////////////////////////////////////////////////////////////////

PolyWallTextureCoordsV::PolyWallTextureCoordsV(FTexture *tex, const line_t *line, const side_t *side, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil, double topTexZ, double bottomTexZ)
{
	double vscale = side->GetTextureYScale(texpart) * tex->Scale.Y;

	double yoffset = side->GetTextureYOffset(texpart);
	if (tex->bWorldPanning)
		yoffset *= vscale;

	switch (texpart)
	{
	default:
	case side_t::mid:
		CalcVMidPart(tex, line, side, topTexZ, bottomTexZ, vscale, yoffset);
		break;
	case side_t::top:
		CalcVTopPart(tex, line, side, topTexZ, bottomTexZ, vscale, yoffset);
		break;
	case side_t::bottom:
		CalcVBottomPart(tex, line, side, topTexZ, bottomTexZ, unpeggedceil, vscale, yoffset);
		break;
	}

	int texHeight = tex->GetHeight();
	v1 /= texHeight;
	v2 /= texHeight;

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

void PolyWallTextureCoordsV::CalcVTopPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double vscale, double yoffset)
{
	bool pegged = (line->flags & ML_DONTPEGTOP) == 0;
	if (pegged) // bottom to top
	{
		int texHeight = tex->GetHeight();
		v1 = -yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
		v1 = texHeight - v1;
		v2 = texHeight - v2;
		std::swap(v1, v2);
	}
	else // top to bottom
	{
		v1 = yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
}

void PolyWallTextureCoordsV::CalcVMidPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double vscale, double yoffset)
{
	bool pegged = (line->flags & ML_DONTPEGBOTTOM) == 0;
	if (pegged) // top to bottom
	{
		v1 = yoffset * vscale;
		v2 = (yoffset + (topz - bottomz)) * vscale;
	}
	else // bottom to top
	{
		int texHeight = tex->GetHeight();
		v1 = texHeight - (-yoffset + (topz - bottomz)) * vscale;
		v2 = texHeight + yoffset * vscale;
	}
}

void PolyWallTextureCoordsV::CalcVBottomPart(FTexture *tex, const line_t *line, const side_t *side, double topz, double bottomz, double unpeggedceil, double vscale, double yoffset)
{
	bool pegged = (line->flags & ML_DONTPEGBOTTOM) == 0;
	if (pegged) // top to bottom
	{
		v1 = yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
	else
	{
		v1 = yoffset + (unpeggedceil - topz);
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
}
