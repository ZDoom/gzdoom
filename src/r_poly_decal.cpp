/*
**  Handling drawing a decal
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
#include "r_poly_decal.h"
#include "r_poly.h"
#include "a_sharedglobal.h"

void RenderPolyDecal::RenderWallDecals(const TriMatrix &worldToClip, const seg_t *line, uint32_t subsectorDepth)
{
	for (DBaseDecal *decal = line->sidedef->AttachedDecals; decal != nullptr; decal = decal->WallNext)
	{
		RenderPolyDecal render;
		render.Render(worldToClip, decal, line, subsectorDepth);
	}
}

void RenderPolyDecal::Render(const TriMatrix &worldToClip, DBaseDecal *decal, const seg_t *line, uint32_t subsectorDepth)
{
	if (decal->RenderFlags & RF_INVISIBLE || !viewactive || !decal->PicNum.isValid())
		return;

	FTexture *tex = TexMan(decal->PicNum, true);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
		return;

	double edge_right = tex->GetWidth();
	double edge_left = tex->LeftOffset;
	edge_right = (edge_right - edge_left) * decal->ScaleX;
	edge_left *= decal->ScaleX;

	double dcx, dcy;
	decal->GetXY(line->sidedef, dcx, dcy);
	DVector2 decal_pos = { dcx, dcy };

	DVector2 angvec = (line->v2->fPos() - line->v1->fPos()).Unit();
	DVector2 decal_left = decal_pos - edge_left * angvec;
	DVector2 decal_right = decal_pos + edge_right * angvec;

	// Determine actor z
	double zpos = decal->Z;
	sector_t *front = line->frontsector;
	sector_t *back = (line->backsector != nullptr) ? line->backsector : line->frontsector;
	switch (decal->RenderFlags & RF_RELMASK)
	{
	default:
		zpos = decal->Z;
		break;
	case RF_RELUPPER:
		if (line->linedef->flags & ML_DONTPEGTOP)
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		else
			zpos = decal->Z + back->GetPlaneTexZ(sector_t::ceiling);
		break;
	case RF_RELLOWER:
		if (line->linedef->flags & ML_DONTPEGBOTTOM)
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
		else
			zpos = decal->Z + back->GetPlaneTexZ(sector_t::floor);
		break;
	case RF_RELMID:
		if (line->linedef->flags & ML_DONTPEGBOTTOM)
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::floor);
		else
			zpos = decal->Z + front->GetPlaneTexZ(sector_t::ceiling);
	}

	DVector2 spriteScale = { decal->ScaleX, decal->ScaleY };
	double thingxscalemul = spriteScale.X / tex->Scale.X;
	double thingyscalemul = spriteScale.Y / tex->Scale.Y;
	double spriteHeight = thingyscalemul * tex->GetHeight();

	bool flipTextureX = (decal->RenderFlags & RF_XFLIP) == RF_XFLIP;

	DVector2 points[2] = { decal_left, decal_right };

	TriVertex *vertices = PolyVertexBuffer::GetVertices(4);
	if (!vertices)
		return;

	bool foggy = false;
	int actualextralight = foggy ? 0 : extralight << 4;

	std::pair<float, float> offsets[4] =
	{
		{ 0.0f,  1.0f },
		{ 1.0f,  1.0f },
		{ 1.0f,  0.0f },
		{ 0.0f,  0.0f },
	};

	for (int i = 0; i < 4; i++)
	{
		auto &p = (i == 0 || i == 3) ? points[0] : points[1];

		vertices[i].x = (float)p.X;
		vertices[i].y = (float)p.Y;
		vertices[i].z = (float)(zpos + spriteHeight * offsets[i].second);
		vertices[i].w = 1.0f;
		vertices[i].varying[0] = (float)(offsets[i].first * tex->Scale.X);
		vertices[i].varying[1] = (float)((1.0f - offsets[i].second) * tex->Scale.Y);
		if (flipTextureX)
			vertices[i].varying[0] = 1.0f - vertices[i].varying[0];
	}

	bool fullbrightSprite = (decal->RenderFlags & RF_FULLBRIGHT) == RF_FULLBRIGHT;

	TriUniforms uniforms;
	uniforms.objectToClip = worldToClip;
	if (fullbrightSprite || fixedlightlev >= 0 || fixedcolormap)
	{
		uniforms.light = 256;
		uniforms.flags = TriUniforms::fixed_light;
	}
	else
	{
		uniforms.light = (uint32_t)((front->lightlevel + actualextralight) / 255.0f * 256.0f);
		uniforms.flags = 0;
	}
	uniforms.subsectorDepth = subsectorDepth;

	PolyDrawArgs args;
	args.uniforms = uniforms;
	args.vinput = vertices;
	args.vcount = 4;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = true;
	args.clipleft = 0;
	args.cliptop = 0;
	args.clipright = viewwidth;
	args.clipbottom = viewheight;
	args.stenciltestvalue = 0;
	args.stencilwritevalue = 1;
	args.SetTexture(tex);
	PolyTriangleDrawer::draw(args, TriDrawVariant::DrawSubsector);
}
