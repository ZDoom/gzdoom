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
#include "poly_decal.h"
#include "polyrenderer/poly_renderer.h"
#include "a_sharedglobal.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"

void RenderPolyDecal::RenderWallDecals(const TriMatrix &worldToClip, const Vec4f &clipPlane, const seg_t *line, uint32_t subsectorDepth, uint32_t stencilValue)
{
	if (line->linedef == nullptr && line->sidedef == nullptr)
		return;

	for (DBaseDecal *decal = line->sidedef->AttachedDecals; decal != nullptr; decal = decal->WallNext)
	{
		RenderPolyDecal render;
		render.Render(worldToClip, clipPlane, decal, line, subsectorDepth, stencilValue);
	}
}

void RenderPolyDecal::Render(const TriMatrix &worldToClip, const Vec4f &clipPlane, DBaseDecal *decal, const seg_t *line, uint32_t subsectorDepth, uint32_t stencilValue)
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
		vertices[i].z = (float)(zpos + spriteHeight * offsets[i].second - spriteHeight * 0.5);
		vertices[i].w = 1.0f;
		vertices[i].varying[0] = (float)(offsets[i].first * tex->Scale.X);
		vertices[i].varying[1] = (float)((1.0f - offsets[i].second) * tex->Scale.Y);
		if (flipTextureX)
			vertices[i].varying[0] = 1.0f - vertices[i].varying[0];
	}

	bool fullbrightSprite = (decal->RenderFlags & RF_FULLBRIGHT) == RF_FULLBRIGHT;

	swrenderer::CameraLight *cameraLight = swrenderer::CameraLight::Instance();

	PolyDrawArgs args;
	args.uniforms.flags = 0;
	args.SetColormap(front->ColorMap);
	args.SetTexture(tex, decal->Translation, true);
	args.uniforms.globvis = (float)swrenderer::LightVisibility::Instance()->WallGlobVis();
	if (fullbrightSprite || cameraLight->FixedLightLevel() >= 0 || cameraLight->FixedColormap())
	{
		args.uniforms.light = 256;
		args.uniforms.flags |= TriUniforms::fixed_light;
	}
	else
	{
		args.uniforms.light = (uint32_t)((front->lightlevel + actualextralight) / 255.0f * 256.0f);
	}
	args.uniforms.subsectorDepth = subsectorDepth;

	if (swrenderer::RenderViewport::Instance()->RenderTarget->IsBgra())
	{
		args.uniforms.color = 0xff000000 | decal->AlphaColor;
	}
	else
	{
		args.uniforms.color = ((uint32_t)decal->AlphaColor) >> 24;
	}
	
	args.uniforms.srcalpha = (uint32_t)(decal->Alpha * 256.0 + 0.5);
	args.uniforms.destalpha = 256 - args.uniforms.srcalpha;

	args.objectToClip = &worldToClip;
	args.vinput = vertices;
	args.vcount = 4;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = true;
	args.stenciltestvalue = stencilValue;
	args.stencilwritevalue = stencilValue;
	//mode = R_SetPatchStyle (decal->RenderStyle, (float)decal->Alpha, decal->Translation, decal->AlphaColor);
	args.SetClipPlane(clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w);
	args.blendmode = TriBlendMode::Shaded;
	args.subsectorTest = true;
	args.writeStencil = false;
	args.writeSubsector = false;
	PolyTriangleDrawer::draw(args);
}
