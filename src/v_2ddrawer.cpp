// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2016-2018 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** v_2ddrawer.h
** Device independent 2D draw list
**
**/

#include <stdarg.h>
#include "doomtype.h"
#include "templates.h"
#include "r_utility.h"
#include "v_video.h"
#include "g_levellocals.h"
#include "vm.h"

EXTERN_CVAR(Float, transsouls)

IMPLEMENT_CLASS(DShape2D, false, false)

DEFINE_ACTION_FUNCTION(DShape2D, Clear)
{
	PARAM_SELF_PROLOGUE(DShape2D);
	PARAM_INT(which);
	if ( which&C_Verts ) self->mVertices.Clear();
	if ( which&C_Coords ) self->mCoords.Clear();
	if ( which&C_Indices ) self->mIndices.Clear();
	return 0;
}

DEFINE_ACTION_FUNCTION(DShape2D, PushVertex)
{
	PARAM_SELF_PROLOGUE(DShape2D);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	self->mVertices.Push(DVector2(x,y));
	return 0;
}

DEFINE_ACTION_FUNCTION(DShape2D, PushCoord)
{
	PARAM_SELF_PROLOGUE(DShape2D);
	PARAM_FLOAT(u);
	PARAM_FLOAT(v);
	self->mCoords.Push(DVector2(u,v));
	return 0;
}

DEFINE_ACTION_FUNCTION(DShape2D, PushTriangle)
{
	PARAM_SELF_PROLOGUE(DShape2D);
	PARAM_INT(a);
	PARAM_INT(b);
	PARAM_INT(c);
	self->mIndices.Push(a);
	self->mIndices.Push(b);
	self->mIndices.Push(c);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

int F2DDrawer::AddCommand(const RenderCommand *data) 
{
	if (mData.Size() > 0 && data->isCompatible(mData.Last()))
	{
		// Merge with the last command.
		mData.Last().mIndexCount += data->mIndexCount;
		mData.Last().mVertCount += data->mVertCount;
		return mData.Size();
	}
	else
	{
		return mData.Push(*data);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddIndices(int firstvert, int count, ...)
{
	va_list ap;
	va_start(ap, count);
	int addr = mIndices.Reserve(count);
	for (int i = 0; i < count; i++)
	{
		mIndices[addr + i] = firstvert + va_arg(ap, int);
	}
}

//==========================================================================
//
// SetStyle
//
// Patterned after R_SetPatchStyle.
//
//==========================================================================

bool F2DDrawer::SetStyle(FTexture *tex, DrawParms &parms, PalEntry &vertexcolor, RenderCommand &quad)
{
	FRenderStyle style = parms.style;
	float alpha;
	bool stencilling;

	if (style.Flags & STYLEF_TransSoulsAlpha)
	{
		alpha = transsouls;
	}
	else if (style.Flags & STYLEF_Alpha1)
	{
		alpha = 1;
	}
	else
	{
		alpha = clamp(parms.Alpha, 0.f, 1.f);
	}

	style.CheckFuzz();
	if (style.BlendOp == STYLEOP_Shadow || style.BlendOp == STYLEOP_Fuzz)
	{
		style = LegacyRenderStyles[STYLE_TranslucentStencil];
		alpha = 0.3f;
		parms.fillcolor = 0;
	}
	else if (style.BlendOp == STYLEOP_FuzzOrAdd)
	{
		style.BlendOp = STYLEOP_Add;
	}
	else if (style.BlendOp == STYLEOP_FuzzOrSub)
	{
		style.BlendOp = STYLEOP_Sub;
	}
	else if (style.BlendOp == STYLEOP_FuzzOrRevSub)
	{
		style.BlendOp = STYLEOP_RevSub;
	}

	stencilling = false;

	if (style.Flags & STYLEF_InvertOverlay)
	{
		// Only the overlay color is inverted, not the overlay alpha.
		parms.colorOverlay.r = 255 - parms.colorOverlay.r;
		parms.colorOverlay.g = 255 - parms.colorOverlay.g;
		parms.colorOverlay.b = 255 - parms.colorOverlay.b;
	}

	SetColorOverlay(parms.colorOverlay, alpha, vertexcolor, quad.mColor1);

	if (style.Flags & STYLEF_ColorIsFixed)
	{
		if (style.Flags & STYLEF_InvertSource)
		{ // Since the source color is a constant, we can invert it now
		  // without spending time doing it in the shader.
			parms.fillcolor.r = 255 - parms.fillcolor.r;
			parms.fillcolor.g = 255 - parms.fillcolor.g;
			parms.fillcolor.b = 255 - parms.fillcolor.b;
			style.Flags &= ~STYLEF_InvertSource;
		}
		if (parms.desaturate > 0)
		{
			// Desaturation can also be computed here without having to do it in the shader.
			auto gray = parms.fillcolor.Luminance();
			auto notgray = 255 - gray;
			parms.fillcolor.r = uint8_t((parms.fillcolor.r * notgray + gray * 255) / 255);
			parms.fillcolor.g = uint8_t((parms.fillcolor.g * notgray + gray * 255) / 255);
			parms.fillcolor.b = uint8_t((parms.fillcolor.b * notgray + gray * 255) / 255);
			parms.desaturate = 0;
		}

		// Set up the color mod to replace the color from the image data.
		vertexcolor.r = parms.fillcolor.r;
		vertexcolor.g = parms.fillcolor.g;
		vertexcolor.b = parms.fillcolor.b;

		if (style.Flags & STYLEF_RedIsAlpha)
		{
			quad.mDrawMode = TM_ALPHATEXTURE;
		}
		else
		{
			quad.mDrawMode = TM_STENCIL;
		}
	}
	else
	{
		if (style.Flags & STYLEF_RedIsAlpha)
		{
			quad.mDrawMode = TM_ALPHATEXTURE;
		}
		else if (style.Flags & STYLEF_InvertSource)
		{
			quad.mDrawMode = TM_INVERSE;
		}

		if (parms.specialcolormap != nullptr)
		{ // draw with an invulnerability or similar colormap.

			auto scm = parms.specialcolormap;

			quad.mSpecialColormap[0] = PalEntry(255, int(scm->ColorizeStart[0] * 127.5f), int(scm->ColorizeStart[1] * 127.5f), int(scm->ColorizeStart[2] * 127.5f));
			quad.mSpecialColormap[1] = PalEntry(255, int(scm->ColorizeEnd[0] * 127.5f), int(scm->ColorizeEnd[1] * 127.5f), int(scm->ColorizeEnd[2] * 127.5f));
			quad.mColor1 = 0;	// this disables the color overlay.
		}
		quad.mDesaturate = parms.desaturate;
	}
	// apply the element's own color. This is being blended with anything that came before.
	vertexcolor = PalEntry((vertexcolor.a * parms.color.a) / 255, (vertexcolor.r * parms.color.r) / 255, (vertexcolor.g * parms.color.g) / 255, (vertexcolor.b * parms.color.b) / 255);

	if (!parms.masked)
	{
		// For TM_ALPHATEXTURE and TM_STENCIL the mask cannot be turned off because it would not yield a usable result.
		if (quad.mDrawMode == TM_NORMAL) quad.mDrawMode = TM_OPAQUE;
		else if (quad.mDrawMode == TM_INVERSE) quad.mDrawMode = TM_INVERTOPAQUE;
	}
	quad.mRenderStyle = parms.style;	// this  contains the blend mode and blend equation settings.
    if (parms.burn) quad.mFlags |= DTF_Burn;
	return true;
}

//==========================================================================
//
// Draws a texture
//
//==========================================================================

void F2DDrawer::SetColorOverlay(PalEntry color, float alpha, PalEntry &vertexcolor, PalEntry &overlaycolor)
{
	if (color.a != 0 && (color & 0xffffff) != 0)
	{
		// overlay color uses premultiplied alpha.
		int a = color.a * 256 / 255;
		overlaycolor.r = (color.r * a) >> 8;
		overlaycolor.g = (color.g * a) >> 8;
		overlaycolor.b = (color.b * a) >> 8;
		overlaycolor.a = 0;	// The overlay gets added on top of the texture data so to preserve the pixel's alpha this must be 0.
	}
	else
	{
		overlaycolor = 0;
	}
	// Vertex intensity is the inverse of the overlay so that the shader can do a simple addition to combine them.
	uint8_t light = 255 - color.a;
	vertexcolor = PalEntry(int(alpha * 255), light, light, light);

	// The real color gets multiplied into vertexcolor later.
}

//==========================================================================
//
// Draws a texture
//
//==========================================================================

void F2DDrawer::AddTexture(FTexture *img, DrawParms &parms)
{
	if (parms.style.BlendOp == STYLEOP_None) return;	// not supposed to be drawn.

	double xscale = parms.destwidth / parms.texwidth;
	double yscale = parms.destheight / parms.texheight;
	double x = parms.x - parms.left * xscale;
	double y = parms.y - parms.top * yscale;
	double w = parms.destwidth;
	double h = parms.destheight;
	double u1, v1, u2, v2;
	PalEntry vertexcolor;

	RenderCommand dg;

	dg.mType = DrawTypeTriangles;
	dg.mVertCount = 4;
	dg.mTexture = img;
	if (img->isWarped()) dg.mFlags |= DTF_Wrap;

	dg.mTranslation = 0;
	SetStyle(img, parms, vertexcolor, dg);

	if (!img->isHardwareCanvas() && parms.remap != nullptr && !parms.remap->Inactive)
	{
		dg.mTranslation = parms.remap;
	}
	u1 = parms.srcx;
	v1 = parms.srcy;
	u2 = parms.srcx + parms.srcwidth;
	v2 = parms.srcy + parms.srcheight;

	if (parms.flipX) 
		std::swap(u1, u2);

	if (parms.flipY)
		std::swap(v1, v2);

	// This is crap. Only kept for backwards compatibility with scripts that may have used it.
	// Note that this only works for unflipped full textures.
	if (parms.windowleft > 0 || parms.windowright < parms.texwidth)
	{
		double wi = MIN(parms.windowright, parms.texwidth);
		x += parms.windowleft * xscale;
		w -= (parms.texwidth - wi + parms.windowleft) * xscale;

		u1 = float(u1 + parms.windowleft / parms.texwidth);
		u2 = float(u2 - (parms.texwidth - wi) / parms.texwidth);
	}

	if (x < (double)parms.lclip || y < (double)parms.uclip || x + w >(double)parms.rclip || y + h >(double)parms.dclip)
	{
		dg.mScissor[0] = parms.lclip;
		dg.mScissor[1] = parms.uclip;
		dg.mScissor[2] = parms.rclip;
		dg.mScissor[3] = parms.dclip;
		dg.mFlags |= DTF_Scissor;
	}
	else
	{
		memset(dg.mScissor, 0, sizeof(dg.mScissor));
	}

	dg.mVertCount = 4;
	dg.mVertIndex = (int)mVertices.Reserve(4);
	TwoDVertex *ptr = &mVertices[dg.mVertIndex];
	ptr->Set(x, y, 0, u1, v1, vertexcolor); ptr++;
	ptr->Set(x, y + h, 0, u1, v2, vertexcolor); ptr++;
	ptr->Set(x + w, y, 0, u2, v1, vertexcolor); ptr++;
	ptr->Set(x + w, y + h, 0, u2, v2, vertexcolor); ptr++;
	dg.mIndexIndex = mIndices.Size();
	dg.mIndexCount += 6;
	AddIndices(dg.mVertIndex, 6, 0, 1, 2, 1, 3, 2);
	AddCommand(&dg);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddShape( FTexture *img, DShape2D *shape, DrawParms &parms )
{
	if (parms.style.BlendOp == STYLEOP_None) return;	// not supposed to be drawn.

	PalEntry vertexcolor;

	RenderCommand dg;

	dg.mType = DrawTypeTriangles;
	dg.mVertCount = shape->mVertices.Size();
	dg.mFlags |= DTF_Wrap;
	dg.mTexture = img;

	dg.mTranslation = 0;
	SetStyle(img, parms, vertexcolor, dg);

	if (!img->isHardwareCanvas() && parms.remap != nullptr && !parms.remap->Inactive)
		dg.mTranslation = parms.remap;

	double minx = 16383, miny = 16383, maxx = -16384, maxy = -16384;
	for ( int i=0; i<dg.mVertCount; i++ )
	{
		if ( shape->mVertices[i].X < minx ) minx = shape->mVertices[i].X;
		if ( shape->mVertices[i].Y < miny ) miny = shape->mVertices[i].Y;
		if ( shape->mVertices[i].X > maxx ) maxx = shape->mVertices[i].X;
		if ( shape->mVertices[i].Y > maxy ) maxy = shape->mVertices[i].Y;
	}
	if (minx < (double)parms.lclip || miny < (double)parms.uclip || maxx >(double)parms.rclip || maxy >(double)parms.dclip)
	{
		dg.mScissor[0] = parms.lclip;
		dg.mScissor[1] = parms.uclip;
		dg.mScissor[2] = parms.rclip;
		dg.mScissor[3] = parms.dclip;
		dg.mFlags |= DTF_Scissor;
	}
	else
		memset(dg.mScissor, 0, sizeof(dg.mScissor));

	dg.mVertIndex = (int)mVertices.Reserve(dg.mVertCount);
	TwoDVertex *ptr = &mVertices[dg.mVertIndex];
	for ( int i=0; i<dg.mVertCount; i++ )
		ptr[i].Set(shape->mVertices[i].X, shape->mVertices[i].Y, 0, shape->mCoords[i].X, shape->mCoords[i].Y, vertexcolor);
	dg.mIndexIndex = mIndices.Size();
	dg.mIndexCount += shape->mIndices.Size();
	for ( int i=0; i<int(shape->mIndices.Size()); i+=3 )
		AddIndices(dg.mVertIndex, 3, shape->mIndices[i], shape->mIndices[i+1], shape->mIndices[i+2]);
	AddCommand(&dg);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddPoly(FTexture *texture, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel, ELightMode lightMode,
		uint32_t *indices, size_t indexcount)
{
	// Use an equation similar to player sprites to determine shade

	// Convert a light level into an unbounded colormap index (shade). 
	// Why the +12? I wish I knew, but experimentation indicates it
	// is necessary in order to best reproduce Doom's original lighting.
	double fadelevel;

	if (vid_rendermode != 4 || lightMode == ELightMode::Doom || lightMode == ELightMode::ZDoomSoftware || lightMode == ELightMode::DoomSoftware)
	{
		double map = (NUMCOLORMAPS * 2.) - ((lightlevel + 12) * (NUMCOLORMAPS / 128.));
		fadelevel = clamp((map - 12) / NUMCOLORMAPS, 0.0, 1.0);
	}
	else
	{
		// The hardware renderer's light modes 0, 1 and 4 use a linear light scale which must be used here as well. Otherwise the automap gets too dark.
		fadelevel = 1. - clamp(lightlevel, 0, 255) / 255.f;
	}

	RenderCommand poly;

	poly.mType = DrawTypeTriangles;
	poly.mTexture = texture;
	poly.mRenderStyle = DefaultRenderStyle();
	poly.mFlags |= DTF_Wrap;
	poly.mDesaturate = colormap.Desaturation;

	PalEntry color0; 
	double invfade = 1. - fadelevel;

	color0.r = uint8_t(colormap.LightColor.r * invfade);
	color0.g = uint8_t(colormap.LightColor.g * invfade);
	color0.b = uint8_t(colormap.LightColor.b * invfade);
	color0.a = 255;

	poly.mColor1.a = 0;
	poly.mColor1.r = uint8_t(colormap.FadeColor.r * fadelevel);
	poly.mColor1.g = uint8_t(colormap.FadeColor.g * fadelevel);
	poly.mColor1.b = uint8_t(colormap.FadeColor.b * fadelevel);

	bool dorotate = rotation != 0;

	float cosrot = (float)cos(rotation.Radians());
	float sinrot = (float)sin(rotation.Radians());

	float uscale = float(1.f / (texture->GetDisplayWidth() * scalex));
	float vscale = float(1.f / (texture->GetDisplayHeight() * scaley));
	float ox = float(originx);
	float oy = float(originy);

	poly.mVertCount = npoints;
	poly.mVertIndex = (int)mVertices.Reserve(npoints);
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
		mVertices[poly.mVertIndex+i].Set(points[i].X, points[i].Y, 0, u*uscale, v*vscale, color0);
	}
	poly.mIndexIndex = mIndices.Size();
	poly.mIndexCount += (npoints - 2) * 3;

	if (indices == nullptr || indexcount == 0)
	{
		for (int i = 2; i < npoints; ++i)
		{
			AddIndices(poly.mVertIndex, 3, 0, i - 1, i);
		}
	}
	else
	{
		int addr = mIndices.Reserve(indexcount);
		for (size_t i = 0; i < indexcount; i++)
		{
			mIndices[addr + i] = addr + indices[i];
		}
	}

	AddCommand(&poly);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddFlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{
	float fU1, fU2, fV1, fV2;

	RenderCommand dg;

	dg.mType = DrawTypeTriangles;
	dg.mRenderStyle = DefaultRenderStyle();
	dg.mTexture = src;
	dg.mVertCount = 4;
	dg.mTexture = src;
	dg.mFlags = DTF_Wrap;

	// scaling is not used here.
	if (!local_origin)
	{
		fU1 = float(left) / src->GetDisplayWidth();
		fV1 = float(top) / src->GetDisplayHeight();
		fU2 = float(right) / src->GetDisplayWidth();
		fV2 = float(bottom) / src->GetDisplayHeight();
	}
	else
	{
		fU1 = 0;
		fV1 = 0;
		fU2 = float(right - left) / src->GetDisplayWidth();
		fV2 = float(bottom - top) / src->GetDisplayHeight();
	}
	dg.mVertIndex = (int)mVertices.Reserve(4);
	auto ptr = &mVertices[dg.mVertIndex];

	ptr->Set(left, top, 0, fU1, fV1, 0xffffffff); ptr++;
	ptr->Set(left, bottom, 0, fU1, fV2, 0xffffffff); ptr++;
	ptr->Set(right, top, 0, fU2, fV1, 0xffffffff); ptr++;
	ptr->Set(right, bottom, 0, fU2, fV2, 0xffffffff); ptr++;
	dg.mIndexIndex = mIndices.Size();
	dg.mIndexCount += 6;
	AddIndices(dg.mVertIndex, 6, 0, 1, 2, 1, 3, 2);
	AddCommand(&dg);
}


//===========================================================================
// 
//
//
//===========================================================================

void F2DDrawer::AddColorOnlyQuad(int x1, int y1, int w, int h, PalEntry color, FRenderStyle *style)
{
	RenderCommand dg;

	dg.mType = DrawTypeTriangles;
	dg.mVertCount = 4;
	dg.mVertIndex = (int)mVertices.Reserve(4);
	dg.mRenderStyle = style? *style : LegacyRenderStyles[STYLE_Translucent];
	auto ptr = &mVertices[dg.mVertIndex];
	ptr->Set(x1, y1, 0, 0, 0, color); ptr++;
	ptr->Set(x1, y1 + h, 0, 0, 0, color); ptr++;
	ptr->Set(x1 + w, y1, 0, 0, 0, color); ptr++;
	ptr->Set(x1 + w, y1 + h, 0, 0, 0, color); ptr++;
	dg.mIndexIndex = mIndices.Size();
	dg.mIndexCount += 6;
	AddIndices(dg.mVertIndex, 6, 0, 1, 2, 1, 3, 2);
	AddCommand(&dg);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddLine(int x1, int y1, int x2, int y2, int palcolor, uint32_t color, uint8_t alpha)
{
	PalEntry p = color ? (PalEntry)color : GPalette.BaseColors[palcolor];
	p.a = alpha;

	RenderCommand dg;

	dg.mType = DrawTypeLines;
	dg.mRenderStyle = LegacyRenderStyles[STYLE_Translucent];
	dg.mVertCount = 2;
	dg.mVertIndex = (int)mVertices.Reserve(2);
	mVertices[dg.mVertIndex].Set(x1, y1, 0, 0, 0, p);
	mVertices[dg.mVertIndex+1].Set(x2, y2, 0, 0, 0, p);
	AddCommand(&dg);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddThickLine(int x1, int y1, int x2, int y2, double thickness, uint32_t color, uint8_t alpha)
{
	PalEntry p = (PalEntry)color;
	p.a = alpha;

	DVector2 point0(x1, y1);
	DVector2 point1(x2, y2);

	DVector2 delta = point1 - point0;
	DVector2 perp(-delta.Y, delta.X);
	perp.MakeUnit();
	perp *= thickness / 2;

	DVector2 corner0 = point0 + perp;
	DVector2 corner1 = point0 - perp;
	DVector2 corner2 = point1 + perp;
	DVector2 corner3 = point1 - perp;

	RenderCommand dg;

	dg.mType = DrawTypeTriangles;
	dg.mVertCount = 4;
	dg.mVertIndex = (int)mVertices.Reserve(4);
	dg.mRenderStyle = LegacyRenderStyles[STYLE_Translucent];
	auto ptr = &mVertices[dg.mVertIndex];
	ptr->Set(corner0.X, corner0.Y, 0, 0, 0, p); ptr++;
	ptr->Set(corner1.X, corner1.Y, 0, 0, 0, p); ptr++;
	ptr->Set(corner2.X, corner2.Y, 0, 0, 0, p); ptr++;
	ptr->Set(corner3.X, corner3.Y, 0, 0, 0, p); ptr++;
	dg.mIndexIndex = mIndices.Size();
	dg.mIndexCount += 6;
	AddIndices(dg.mVertIndex, 6, 0, 1, 2, 1, 3, 2);
	AddCommand(&dg);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddPixel(int x1, int y1, int palcolor, uint32_t color)
{
	PalEntry p = color ? (PalEntry)color : GPalette.BaseColors[palcolor];
	p.a = 255;

	RenderCommand dg;

	dg.mType = DrawTypePoints;
	dg.mRenderStyle = LegacyRenderStyles[STYLE_Translucent];
	dg.mVertCount = 1;
	dg.mVertIndex = (int)mVertices.Reserve(1);
	mVertices[dg.mVertIndex].Set(x1, y1, 0, 0, 0, p);
	AddCommand(&dg);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::Clear()
{
	mVertices.Clear();
	mIndices.Clear();
	mData.Clear();
}
