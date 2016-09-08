/*
** gl_2ddrawer.h
** Container class for drawing 2d graphics with a vertex buffer
**
**---------------------------------------------------------------------------
** Copyright 2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/system/gl_system.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_2ddrawer.h"
#include "gl/textures/gl_material.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/textures/gl_translate.h"
#include "vectors.h"

//==========================================================================
//
//
//
//==========================================================================

int F2DDrawer::AddData(const F2DDrawer::DataGeneric *data)
{
	int addr = mData.Reserve(data->mLen);
	memcpy(&mData[addr], data, data->mLen);
	mLastLineCmd = -1;
	return addr;
}

//==========================================================================
//
// Draws a texture
//
//==========================================================================

void F2DDrawer::AddTexture(FTexture *img, DrawParms &parms)
{
	double xscale = parms.destwidth / parms.texwidth;
	double yscale = parms.destheight / parms.texheight;
	double x = parms.x - parms.left * xscale;
	double y = parms.y - parms.top * yscale;
	double w = parms.destwidth;
	double h = parms.destheight;
	float u1, v1, u2, v2;
	int light = 255;

	FMaterial * gltex = FMaterial::ValidateTexture(img, false);
	if (gltex == nullptr) return;

	DataTexture dg;

	dg.mType = DrawTypeTexture;
	dg.mLen = (sizeof(dg) + 7) & ~7;
	dg.mVertCount = 4;
	dg.mRenderStyle = parms.style;
	dg.mMasked = !!parms.masked;
	dg.mTexture = gltex;

	if (parms.colorOverlay && (parms.colorOverlay & 0xffffff) == 0)
	{
		// handle black overlays as reduced light.
		light = 255 - APART(parms.colorOverlay);
		parms.colorOverlay = 0;
	}
	dg.mVertIndex = (int)mVertices.Reserve(parms.colorOverlay == 0? 4 : 8);
	dg.mColorOverlay = parms.colorOverlay;
	dg.mTranslation = 0;

	if (!img->bHasCanvas)
	{
		if (!parms.alphaChannel)
		{
			if (parms.remap != NULL && !parms.remap->Inactive)
			{
				GLTranslationPalette * pal = static_cast<GLTranslationPalette*>(parms.remap->GetNative());
				if (pal) dg.mTranslation = -pal->GetIndex();
			}
		}
		dg.mAlphaTexture = !!(parms.style.Flags & STYLEF_RedIsAlpha);
		u1 = gltex->GetUL();
		v1 = gltex->GetVT();
		u2 = gltex->GetUR();
		v2 = gltex->GetVB();

	}
	else
	{
		dg.mAlphaTexture = false;
		u1 = 0.f;
		v1 = 1.f;
		u2 = 1.f;
		v2 = 0.f;
	}

	if (parms.flipX) 
		std::swap(u1, u2);


	if (parms.windowleft > 0 || parms.windowright < parms.texwidth)
	{
		double wi = MIN(parms.windowright, parms.texwidth);
		x += parms.windowleft * xscale;
		w -= (parms.texwidth - wi + parms.windowleft) * xscale;

		u1 = float(u1 + parms.windowleft / parms.texwidth);
		u2 = float(u2 - (parms.texwidth - wi) / parms.texwidth);
	}

	PalEntry color;
	if (parms.style.Flags & STYLEF_ColorIsFixed)
	{
		color = parms.fillcolor;
		std::swap(color.r, color.b);
	}
	else
	{
		color = PalEntry(light, light, light);
	}
	color.a = (BYTE)(parms.Alpha * 255);

	// scissor test doesn't use the current viewport for the coordinates, so use real screen coordinates
	dg.mScissor[0] = GLRenderer->ScreenToWindowX(parms.lclip);
	dg.mScissor[1] = GLRenderer->ScreenToWindowY(parms.dclip);
	dg.mScissor[2] = GLRenderer->ScreenToWindowX(parms.rclip) - dg.mScissor[0];
	dg.mScissor[3] = GLRenderer->ScreenToWindowY(parms.uclip) - dg.mScissor[1];

	FSimpleVertex *ptr = &mVertices[dg.mVertIndex];
	ptr->Set(x, y, 0, u1, v1, color); ptr++;
	ptr->Set(x, y + h, 0, u1, v2, color); ptr++;
	ptr->Set(x + w, y, 0, u2, v1, color); ptr++;
	ptr->Set(x + w, y + h, 0, u2, v2, color); ptr++;
	if (parms.colorOverlay != 0)
	{
		color = parms.colorOverlay;
		std::swap(color.r, color.b);
		ptr->Set(x, y, 0, u1, v1, color); ptr++;
		ptr->Set(x, y + h, 0, u1, v2, color); ptr++;
		ptr->Set(x + w, y, 0, u2, v1, color); ptr++;
		ptr->Set(x + w, y + h, 0, u2, v2, color); ptr++;
	}
	AddData(&dg);
}


//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddPoly(FTexture *texture, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, FDynamicColormap *colormap, int lightlevel)
{
	FMaterial *gltexture = FMaterial::ValidateTexture(texture, false);

	if (gltexture == nullptr)
	{
		return;
	}
	DataSimplePoly poly;

	poly.mType = DrawTypePoly;
	poly.mLen = (sizeof(poly) + 7) & ~7;
	poly.mTexture = gltexture;
	poly.mColormap = colormap;
	poly.mLightLevel = lightlevel;
	poly.mVertCount = npoints;
	poly.mVertIndex = (int)mVertices.Reserve(npoints);

	bool dorotate = rotation != 0;

	float cosrot = cos(rotation.Radians());
	float sinrot = sin(rotation.Radians());

	float uscale = float(1.f / (texture->GetScaledWidth() * scalex));
	float vscale = float(1.f / (texture->GetScaledHeight() * scaley));
	if (texture->bHasCanvas)
	{
		vscale = 0 - vscale;
	}
	float ox = float(originx);
	float oy = float(originy);

	for (int i = 0; i < npoints; ++i)
	{
		float u = points[i].X - 0.5f - ox;
		float v = points[i].Y - 0.5f - oy;
		if (dorotate)
		{
			float t = u;
			u = t * cosrot - v * sinrot;
			v = v * cosrot + t * sinrot;
		}
		mVertices[poly.mVertIndex+i].Set(points[i].X, points[i].Y, 0, u*uscale, v*vscale);
	}
	AddData(&poly);
}

//===========================================================================
// 
//
//
//===========================================================================

void F2DDrawer::AddDim(PalEntry color, float damount, int x1, int y1, int w, int h)
{
	color.a = uint8_t(damount * 255);
	std::swap(color.r, color.b);

	DataGeneric dg;

	dg.mType = DrawTypeDim;
	dg.mLen = (sizeof(dg) + 7) & ~7;
	dg.mVertCount = 4;
	dg.mVertIndex = (int)mVertices.Reserve(4);
	FSimpleVertex *ptr = &mVertices[dg.mVertIndex];
	ptr->Set(x1, y1, 0, 0, 0, color); ptr++;
	ptr->Set(x1, y1 + h, 0, 0, 0, color); ptr++;
	ptr->Set(x1 + w, y1 + h, 0, 0, 0, color); ptr++;
	ptr->Set(x1 + w, y1, 0, 0, 0, color); ptr++;
	AddData(&dg);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddClear(int left, int top, int right, int bottom, int palcolor, uint32 color)
{
	PalEntry p = palcolor == -1 || color != 0 ? (PalEntry)color : GPalette.BaseColors[palcolor];
	AddDim(p, 1.f, left, top, right - left, bottom - top);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddFlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{
	float fU1, fU2, fV1, fV2;

	FMaterial *gltexture = FMaterial::ValidateTexture(src, false);

	if (!gltexture) return;

	DataFlatFill dg;

	dg.mType = DrawTypeFlatFill;
	dg.mLen = (sizeof(dg) + 7) & ~7;
	dg.mVertCount = 4;
	dg.mVertIndex = (int)mVertices.Reserve(4);
	dg.mTexture = gltexture;

	// scaling is not used here.
	if (!local_origin)
	{
		fU1 = float(left) / src->GetWidth();
		fV1 = float(top) / src->GetHeight();
		fU2 = float(right) / src->GetWidth();
		fV2 = float(bottom) / src->GetHeight();
	}
	else
	{
		fU1 = 0;
		fV1 = 0;
		fU2 = float(right - left) / src->GetWidth();
		fV2 = float(bottom - top) / src->GetHeight();
	}
	FSimpleVertex *ptr = &mVertices[dg.mVertIndex];
	ptr->Set(left, top, 0, fU1, fV1); ptr++;
	ptr->Set(left, bottom, 0, fU1, fV2); ptr++;
	ptr->Set(right, top, 0, fU2, fV1); ptr++;
	ptr->Set(right, bottom, 0, fU2, fV2); ptr++;
	AddData(&dg);
}


//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddLine(int x1, int y1, int x2, int y2, int palcolor, uint32 color)
{
	PalEntry p = color ? (PalEntry)color : GPalette.BaseColors[palcolor];
	p.a = 255;
	std::swap(p.r, p.b);

	DataGeneric dg;

	dg.mType = DrawTypeLine;
	dg.mLen = (sizeof(dg) + 7) & ~7;
	dg.mVertCount = 2;
	dg.mVertIndex = (int)mVertices.Reserve(2);
	mVertices[dg.mVertIndex].Set(x1, y1, 0, 0, 0, p);
	mVertices[dg.mVertIndex+1].Set(x2, y2, 0, 0, 0, p);

	// Test if we can batch multiple line commands
	if (mLastLineCmd == -1)
	{
		mLastLineCmd = AddData(&dg);
	}
	else
	{
		DataGeneric *dg = (DataGeneric *)&mData[mLastLineCmd];
		dg->mVertCount += 2;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddPixel(int x1, int y1, int palcolor, uint32 color)
{
	PalEntry p = color ? (PalEntry)color : GPalette.BaseColors[palcolor];
	p.a = 255;
	std::swap(p.r, p.b);

	DataGeneric dg;

	dg.mType = DrawTypePixel;
	dg.mLen = (sizeof(dg) + 7) & ~7;
	dg.mVertCount = 2;
	dg.mVertIndex = (int)mVertices.Reserve(1);
	mVertices[dg.mVertIndex].Set(x1, y1, 0, 0, 0, p);
	AddData(&dg);
}


//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::Flush()
{
	F2DDrawer::EDrawType lasttype = DrawTypeTexture;

	if (mData.Size() == 0) return;
	SBYTE savedlightmode = glset.lightmode;
	// lightmode is only relevant for automap subsectors,
	// but We cannot use the software light mode here because it doesn't properly calculate the light for 2D rendering.
	if (glset.lightmode == 8) glset.lightmode = 0;

	set(&mVertices[0], mVertices.Size());
	for (unsigned i = 0; i < mData.Size();)
	{
		DataGeneric *dg = (DataGeneric *)&mData[i];
		// DrawTypePoly may not use the color part of the vertex buffer because it needs to use gl_SetColor to produce proper output.
		if (lasttype == DrawTypePoly && dg->mType != DrawTypePoly)
		{
			gl_RenderState.ResetColor();	// this is needed to reset the desaturation.
			EnableColorArray(true);
		}
		else if (lasttype != DrawTypePoly && dg->mType == DrawTypePoly)
		{
			EnableColorArray(false);
		}
		lasttype = dg->mType;

		switch (dg->mType)
		{
		default:
			break;

		case DrawTypeTexture:
		{
			DataTexture *dt = static_cast<DataTexture*>(dg);

			gl_SetRenderStyle(dt->mRenderStyle, !dt->mMasked, false);
			gl_RenderState.SetMaterial(dt->mTexture, CLAMP_XY_NOMIP, dt->mTranslation, -1, dt->mAlphaTexture);
			if (dt->mTexture->tex->bHasCanvas) gl_RenderState.SetTextureMode(TM_OPAQUE);

			glEnable(GL_SCISSOR_TEST);
			glScissor(dt->mScissor[0], dt->mScissor[1], dt->mScissor[2], dt->mScissor[3]);

			gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
			gl_RenderState.Apply();

			glDrawArrays(GL_TRIANGLE_STRIP, dt->mVertIndex, 4);

			gl_RenderState.BlendEquation(GL_FUNC_ADD);
			if (dt->mVertCount > 4)
			{
				gl_RenderState.SetTextureMode(TM_MASK);
				gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				gl_RenderState.Apply();
				glDrawArrays(GL_TRIANGLE_STRIP, dt->mVertIndex + 4, 4);
			}

			const auto &viewport = GLRenderer->mScreenViewport;
			glScissor(viewport.left, viewport.top, viewport.width, viewport.height);
			glDisable(GL_SCISSOR_TEST);
			gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			gl_RenderState.SetTextureMode(TM_MODULATE);
			break;
		}

		case DrawTypePoly:
		{
			DataSimplePoly *dsp = static_cast<DataSimplePoly*>(dg);

			FColormap cm;
			cm = dsp->mColormap;
			gl_SetColor(dsp->mLightLevel, 0, cm, 1.f);
			gl_RenderState.SetMaterial(dsp->mTexture, CLAMP_NONE, 0, -1, false);
			gl_RenderState.Apply();
			glDrawArrays(GL_TRIANGLE_FAN, dsp->mVertIndex, dsp->mVertCount);
			break;
		}

		case DrawTypeFlatFill:
		{
			DataFlatFill *dff = static_cast<DataFlatFill*>(dg);
			gl_RenderState.SetMaterial(dff->mTexture, CLAMP_NONE, 0, -1, false);
			gl_RenderState.Apply();
			glDrawArrays(GL_TRIANGLE_STRIP, dg->mVertIndex, dg->mVertCount);
			break;
		}

		case DrawTypeDim:
			gl_RenderState.EnableTexture(false);
			gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			gl_RenderState.AlphaFunc(GL_GREATER, 0);
			gl_RenderState.Apply();
			glDrawArrays(GL_TRIANGLE_FAN, dg->mVertIndex, dg->mVertCount);
			gl_RenderState.EnableTexture(true);
			break;

		case DrawTypeLine:
			gl_RenderState.EnableTexture(false);
			gl_RenderState.Apply();
			glDrawArrays(GL_LINES, dg->mVertIndex, dg->mVertCount);
			gl_RenderState.EnableTexture(true);
			break;

		case DrawTypePixel:
			gl_RenderState.EnableTexture(false);
			gl_RenderState.Apply();
			glDrawArrays(GL_POINTS, dg->mVertIndex, dg->mVertCount);
			gl_RenderState.EnableTexture(true);
			break;

		}
		i += dg->mLen;
	}
	mVertices.Clear();
	mData.Clear();
	gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
	glset.lightmode = savedlightmode;
}


