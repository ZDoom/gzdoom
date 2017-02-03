/*
**  Handling drawing a sprite
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
#include "poly_wallsprite.h"
#include "polyrenderer/poly_renderer.h"
#include "swrenderer/scene/r_light.h"

void RenderPolyWallSprite::Render(const TriMatrix &worldToClip, const Vec4f &clipPlane, AActor *thing, subsector_t *sub, uint32_t subsectorDepth, uint32_t stencilValue)
{
	if (RenderPolySprite::IsThingCulled(thing))
		return;

	DVector3 pos = thing->InterpolatedPosition(r_TicFracF);
	pos.Z += thing->GetBobOffset(r_TicFracF);

	bool flipTextureX = false;
	FTexture *tex = RenderPolySprite::GetSpriteTexture(thing, flipTextureX);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
		return;

	DVector2 spriteScale = thing->Scale;
	double thingxscalemul = spriteScale.X / tex->Scale.X;
	double thingyscalemul = spriteScale.Y / tex->Scale.Y;
	double spriteHeight = thingyscalemul * tex->GetHeight();

	DAngle ang = thing->Angles.Yaw + 90;
	double angcos = ang.Cos();
	double angsin = ang.Sin();

	// Determine left and right edges of sprite. The sprite's angle is its normal,
	// so the edges are 90 degrees each side of it.
	double x2 = tex->GetScaledWidth() * spriteScale.X;
	double x1 = tex->GetScaledLeftOffset() * spriteScale.X;
	DVector2 left, right;
	left.X = pos.X - x1 * angcos;
	left.Y = pos.Y - x1 * angsin;
	right.X = left.X + x2 * angcos;
	right.Y = right.Y + x2 * angsin;

	//int scaled_to = tex->GetScaledTopOffset();
	//int scaled_bo = scaled_to - tex->GetScaledHeight();
	//gzt = pos.Z + scale.Y * scaled_to;
	//gzb = pos.Z + scale.Y * scaled_bo;

	DVector2 points[2] = { left, right };

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
		vertices[i].z = (float)(pos.Z + spriteHeight * offsets[i].second);
		vertices[i].w = 1.0f;
		vertices[i].varying[0] = (float)(offsets[i].first * tex->Scale.X);
		vertices[i].varying[1] = (float)((1.0f - offsets[i].second) * tex->Scale.Y);
		if (flipTextureX)
			vertices[i].varying[0] = 1.0f - vertices[i].varying[0];
	}

	bool fullbrightSprite = ((thing->renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT));
	swrenderer::CameraLight *cameraLight = swrenderer::CameraLight::Instance();

	PolyDrawArgs args;
	args.uniforms.globvis = (float)swrenderer::LightVisibility::Instance()->WallGlobVis();
	if (fullbrightSprite || cameraLight->FixedLightLevel() >= 0 || cameraLight->FixedColormap())
	{
		args.uniforms.light = 256;
		args.uniforms.flags = TriUniforms::fixed_light;
	}
	else
	{
		args.uniforms.light = (uint32_t)((thing->Sector->lightlevel + actualextralight) / 255.0f * 256.0f);
		args.uniforms.flags = 0;
	}
	args.uniforms.subsectorDepth = subsectorDepth;

	args.objectToClip = &worldToClip;
	args.vinput = vertices;
	args.vcount = 4;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = true;
	args.stenciltestvalue = stencilValue;
	args.stencilwritevalue = stencilValue;
	args.SetTexture(tex);
	args.SetColormap(sub->sector->ColorMap);
	args.SetClipPlane(clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w);
	args.subsectorTest = true;
	args.writeSubsector = false;
	args.writeStencil = false;
	args.blendmode = TriBlendMode::AlphaBlend;
	PolyTriangleDrawer::draw(args);
}
