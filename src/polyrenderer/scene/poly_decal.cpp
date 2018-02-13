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
#include "sbar.h"
#include "r_data/r_translate.h"
#include "poly_decal.h"
#include "polyrenderer/poly_renderer.h"
#include "polyrenderer/scene/poly_light.h"
#include "polyrenderer/poly_renderthread.h"
#include "a_sharedglobal.h"
#include "swrenderer/scene/r_scene.h"

void RenderPolyDecal::RenderWallDecals(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, const seg_t *line, uint32_t stencilValue)
{
	if (line->linedef == nullptr && line->sidedef == nullptr)
		return;

	for (DBaseDecal *decal = line->sidedef->AttachedDecals; decal != nullptr; decal = decal->WallNext)
	{
		RenderPolyDecal render;
		render.Render(thread, worldToClip, clipPlane, decal, line, stencilValue);
	}
}

void RenderPolyDecal::Render(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, DBaseDecal *decal, const seg_t *line, uint32_t stencilValue)
{
	if (decal->RenderFlags & RF_INVISIBLE || !viewactive || !decal->PicNum.isValid())
		return;

	FTexture *tex = TexMan(decal->PicNum, true);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
		return;

	sector_t *front, *back;
	GetDecalSectors(decal, line, &front, &back);

	// Calculate unclipped position and UV coordinates

	double edge_left = tex->LeftOffset * decal->ScaleX;
	double edge_right = (tex->GetWidth() - tex->LeftOffset) * decal->ScaleX;

	DVector2 angvec = (line->v2->fPos() - line->v1->fPos()).Unit();
	DVector2 normal = { angvec.Y, -angvec.X };

	double dcx, dcy;
	decal->GetXY(line->sidedef, dcx, dcy);
	DVector2 decal_pos = DVector2(dcx, dcy) + normal;
	DVector2 decal_left = decal_pos - edge_left * angvec;
	DVector2 decal_right = decal_pos + edge_right * angvec;

	bool flipTextureX = (decal->RenderFlags & RF_XFLIP) == RF_XFLIP;
	double u_left = flipTextureX ? 1.0 : 0.0;
	double u_right = flipTextureX ? 1.0 - tex->Scale.X : tex->Scale.X;
	double u_unit = (u_right - u_left) / (edge_left + edge_right);

	double zpos = GetDecalZ(decal, line, front, back);
	double spriteHeight = decal->ScaleY / tex->Scale.Y * tex->GetHeight();
	double ztop = zpos + spriteHeight - spriteHeight * 0.5;
	double zbottom = zpos - spriteHeight * 0.5;

	double v_top = 0.0;
	double v_bottom = tex->Scale.Y;
	double v_unit = (v_bottom - v_top) / (zbottom - ztop);

	// Clip decal to wall part

	double walltopz, wallbottomz;
	GetWallZ(decal, line, front, back, walltopz, wallbottomz);

	double clip_left_v1 = (decal_left - line->v1->fPos()) | angvec;
	double clip_right_v1 = (decal_right - line->v1->fPos()) | angvec;
	double clip_left_v2 = (decal_left - line->v2->fPos()) | angvec;
	double clip_right_v2 = (decal_right - line->v2->fPos()) | angvec;

	if ((clip_left_v1 <= 0.0 && clip_right_v1 <= 0.0) || (clip_left_v2 >= 0.0 && clip_right_v2 >= 0.0))
		return;

	if (clip_left_v1 < 0.0)
	{
		decal_left -= angvec * clip_left_v1;
		u_left -= u_unit * clip_left_v1;
	}
	if (clip_right_v1 < 0.0)
	{
		decal_right -= angvec * clip_right_v1;
		u_right -= u_unit * clip_right_v1;
	}
	if (clip_left_v2 > 0.0)
	{
		decal_left -= angvec * clip_left_v2;
		u_left -= u_unit * clip_left_v2;
	}
	if (clip_right_v2 > 0.0)
	{
		decal_right -= angvec * clip_right_v2;
		u_right -= u_unit * clip_right_v2;
	}

	double clip_top_floor = ztop - wallbottomz;
	double clip_bottom_floor = zbottom - wallbottomz;
	double clip_top_ceiling = ztop - walltopz;
	double clip_bottom_ceiling = zbottom - walltopz;

	if ((clip_top_floor <= 0.0 && clip_bottom_floor <= 0.0) || (clip_top_ceiling >= 0.0 && clip_bottom_ceiling >= 0.0))
		return;

	if (clip_top_floor < 0.0)
	{
		ztop -= clip_top_floor;
		v_top -= v_unit * clip_top_floor;
	}
	if (clip_bottom_floor < 0.0)
	{
		zbottom -= clip_bottom_floor;
		v_bottom -= v_unit * clip_bottom_floor;
	}
	if (clip_top_ceiling > 0.0)
	{
		ztop -= clip_top_ceiling;
		v_top -= v_unit * clip_top_ceiling;
	}
	if (clip_bottom_ceiling > 0.0)
	{
		zbottom -= clip_bottom_ceiling;
		v_bottom -= v_unit * clip_bottom_ceiling;
	}

	// Generate vertices for the decal

	TriVertex *vertices = thread->FrameMemory->AllocMemory<TriVertex>(4);
	vertices[0].x = (float)decal_left.X;
	vertices[0].y = (float)decal_left.Y;
	vertices[0].z = (float)ztop;
	vertices[0].w = 1.0f;
	vertices[0].u = (float)u_left;
	vertices[0].v = (float)v_top;
	vertices[1].x = (float)decal_right.X;
	vertices[1].y = (float)decal_right.Y;
	vertices[1].z = (float)ztop;
	vertices[1].w = 1.0f;
	vertices[1].u = (float)u_right;
	vertices[1].v = (float)v_top;
	vertices[2].x = (float)decal_right.X;
	vertices[2].y = (float)decal_right.Y;
	vertices[2].z = (float)zbottom;
	vertices[2].w = 1.0f;
	vertices[2].u = (float)u_right;
	vertices[2].v = (float)v_bottom;
	vertices[3].x = (float)decal_left.X;
	vertices[3].y = (float)decal_left.Y;
	vertices[3].z = (float)zbottom;
	vertices[3].w = 1.0f;
	vertices[3].u = (float)u_left;
	vertices[3].v = (float)v_bottom;

	// Light calculations

	bool foggy = false;
	int actualextralight = foggy ? 0 : PolyRenderer::Instance()->Viewpoint.extralight << 4;
	bool fullbrightSprite = (decal->RenderFlags & RF_FULLBRIGHT) == RF_FULLBRIGHT;
	int lightlevel = fullbrightSprite ? 255 : front->lightlevel + actualextralight;

	PolyDrawArgs args;
	args.SetLight(GetColorTable(front->Colormap), lightlevel, PolyRenderer::Instance()->Light.WallGlobVis(foggy), fullbrightSprite);
	args.SetColor(0xff000000 | decal->AlphaColor, decal->AlphaColor >> 24);
	args.SetStyle(decal->RenderStyle, decal->Alpha, decal->AlphaColor, decal->Translation, tex, false);
	args.SetTransform(&worldToClip);
	args.SetFaceCullCCW(true);
	args.SetStencilTestValue(stencilValue);
	args.SetWriteStencil(true, stencilValue);
	args.SetClipPlane(0, clipPlane);
	args.SetDepthTest(true);
	args.SetWriteStencil(false);
	args.SetWriteDepth(false);
	args.DrawArray(thread, vertices, 4, PolyDrawMode::TriangleFan);
}

void RenderPolyDecal::GetDecalSectors(DBaseDecal *decal, const seg_t *line, sector_t **front, sector_t **back)
{
	// for 3d-floor segments use the model sector as reference
	if ((decal->RenderFlags&RF_CLIPMASK) == RF_CLIPMID)
		*front = decal->Sector;
	else
		*front = line->frontsector;

	*back = (line->backsector != nullptr) ? line->backsector : line->frontsector;
}

double RenderPolyDecal::GetDecalZ(DBaseDecal *decal, const seg_t *line, sector_t *front, sector_t *back)
{
	switch (decal->RenderFlags & RF_RELMASK)
	{
	default:
		return decal->Z;
	case RF_RELUPPER:
		if (line->linedef->flags & ML_DONTPEGTOP)
			return decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		else
			return decal->Z + back->GetPlaneTexZ(sector_t::ceiling);
	case RF_RELLOWER:
		if (line->linedef->flags & ML_DONTPEGBOTTOM)
			return decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		else
			return decal->Z + back->GetPlaneTexZ(sector_t::floor);
		break;
	case RF_RELMID:
		if (line->linedef->flags & ML_DONTPEGBOTTOM)
			return decal->Z + front->GetPlaneTexZ(sector_t::floor);
		else
			return decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
	}
}

void RenderPolyDecal::GetWallZ(DBaseDecal *decal, const seg_t *line, sector_t *front, sector_t *back, double &walltopz, double &wallbottomz)
{
	double frontceilz1 = front->ceilingplane.ZatPoint(line->v1);
	double frontfloorz1 = front->floorplane.ZatPoint(line->v1);
	double frontceilz2 = front->ceilingplane.ZatPoint(line->v2);
	double frontfloorz2 = front->floorplane.ZatPoint(line->v2);
	if (back == nullptr)
	{
		walltopz = MAX(frontceilz1, frontceilz2);
		wallbottomz = MIN(frontfloorz1, frontfloorz2);
	}
	else
	{
		double backceilz1 = back->ceilingplane.ZatPoint(line->v1);
		double backfloorz1 = back->floorplane.ZatPoint(line->v1);
		double backceilz2 = back->ceilingplane.ZatPoint(line->v2);
		double backfloorz2 = back->floorplane.ZatPoint(line->v2);
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

		switch (decal->RenderFlags & RF_RELMASK)
		{
		default:
			walltopz = MAX(frontceilz1, frontceilz2);
			wallbottomz = MIN(frontfloorz1, frontfloorz2);
			break;
		case RF_RELUPPER:
			walltopz = MAX(topceilz1, topceilz2);
			wallbottomz = MIN(topfloorz1, topfloorz2);
			break;
		case RF_RELLOWER:
			walltopz = MAX(bottomceilz1, bottomceilz2);
			wallbottomz = MIN(bottomfloorz1, bottomfloorz2);
			break;
		case RF_RELMID:
			walltopz = MAX(middleceilz1, middleceilz2);
			wallbottomz = MIN(middlefloorz1, middlefloorz2);
		}
	}
}
