/*
** v_2ddrawer.h
** Device independent 2D draw list
**
**---------------------------------------------------------------------------
** Copyright 2016-2020 Christoph Oelckers
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

#include <stdarg.h>
#include "templates.h"
#include "v_2ddrawer.h"
#include "vectors.h"
#include "vm.h"
#include "c_cvars.h"
#include "v_draw.h"
#include "v_video.h"
#include "fcolormap.h"

static F2DDrawer drawer;
F2DDrawer* twod = &drawer;

EXTERN_CVAR(Float, transsouls)
CVAR(Float, classic_scaling_factor, 1.0, CVAR_ARCHIVE)
CVAR(Float, classic_scaling_pixelaspect, 1.2f, CVAR_ARCHIVE)

IMPLEMENT_CLASS(DShape2DTransform, false, false)

static void Shape2DTransform_Clear(DShape2DTransform* self)
{
	self->transform.Identity();
}

DEFINE_ACTION_FUNCTION_NATIVE(DShape2DTransform, Clear, Shape2DTransform_Clear)
{
	PARAM_SELF_PROLOGUE(DShape2DTransform);
	Shape2DTransform_Clear(self);
	return 0;
}

static void Shape2DTransform_Rotate(DShape2DTransform* self, double angle)
{
	self->transform = DMatrix3x3::Rotate2D(DEG2RAD(angle)) * self->transform;
}

DEFINE_ACTION_FUNCTION_NATIVE(DShape2DTransform, Rotate, Shape2DTransform_Rotate)
{
	PARAM_SELF_PROLOGUE(DShape2DTransform);
	PARAM_FLOAT(angle);
	Shape2DTransform_Rotate(self, angle);
	return 0;
}

static void Shape2DTransform_Scale(DShape2DTransform* self, double x, double y)
{
	self->transform = DMatrix3x3::Scale2D(DVector2(x, y)) * self->transform;
}

DEFINE_ACTION_FUNCTION_NATIVE(DShape2DTransform, Scale, Shape2DTransform_Scale)
{
	PARAM_SELF_PROLOGUE(DShape2DTransform);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	Shape2DTransform_Scale(self, x, y);
	return 0;
}

static void Shape2DTransform_Translate(DShape2DTransform* self, double x, double y)
{
	self->transform = DMatrix3x3::Translate2D(DVector2(x, y)) * self->transform;
}

DEFINE_ACTION_FUNCTION_NATIVE(DShape2DTransform, Translate, Shape2DTransform_Translate)
{
	PARAM_SELF_PROLOGUE(DShape2DTransform);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	Shape2DTransform_Translate(self, x, y);
	return 0;
}

IMPLEMENT_CLASS(DShape2D, false, false)

static void Shape2D_SetTransform(DShape2D* self, DShape2DTransform *transform)
{
	self->transform = transform->transform;
}

DEFINE_ACTION_FUNCTION_NATIVE(DShape2D, SetTransform, Shape2D_SetTransform)
{
	PARAM_SELF_PROLOGUE(DShape2D);
	PARAM_OBJECT(transform, DShape2DTransform);
	Shape2D_SetTransform(self, transform);
	return 0;
}

static void Shape2D_Clear(DShape2D* self, int which)
{
	if (which & C_Verts) self->mVertices.Clear();
	if (which & C_Coords) self->mCoords.Clear();
	if (which & C_Indices) self->mIndices.Clear();
	self->bufferInfo->needsVertexUpload = true;
}

DEFINE_ACTION_FUNCTION_NATIVE(DShape2D, Clear, Shape2D_Clear)
{
	PARAM_SELF_PROLOGUE(DShape2D);
	PARAM_INT(which);
	Shape2D_Clear(self, which);
	return 0;
}

static void Shape2D_PushVertex(DShape2D* self, double x, double y)
{
	self->mVertices.Push(DVector2(x, y));
	self->bufferInfo->needsVertexUpload = true;
}

DEFINE_ACTION_FUNCTION_NATIVE(DShape2D, PushVertex, Shape2D_PushVertex)
{
	PARAM_SELF_PROLOGUE(DShape2D);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	Shape2D_PushVertex(self, x, y);
	return 0;
}

static void Shape2D_PushCoord(DShape2D* self, double u, double v)
{
	self->mCoords.Push(DVector2(u, v));
	self->bufferInfo->needsVertexUpload = true;
}

DEFINE_ACTION_FUNCTION_NATIVE(DShape2D, PushCoord, Shape2D_PushCoord)
{
	PARAM_SELF_PROLOGUE(DShape2D);
	PARAM_FLOAT(u);
	PARAM_FLOAT(v);
	Shape2D_PushCoord(self, u, v);
	return 0;
}

static void Shape2D_PushTriangle(DShape2D* self, int a, int b, int c)
{
	self->mIndices.Push(a);
	self->mIndices.Push(b);
	self->mIndices.Push(c);
	self->bufferInfo->needsVertexUpload = true;
}

DEFINE_ACTION_FUNCTION_NATIVE(DShape2D, PushTriangle, Shape2D_PushTriangle)
{
	PARAM_SELF_PROLOGUE(DShape2D);
	PARAM_INT(a);
	PARAM_INT(b);
	PARAM_INT(c);
	Shape2D_PushTriangle(self, a, b, c);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

int F2DDrawer::AddCommand(RenderCommand *data) 
{
	data->mScreenFade = screenFade;
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

void F2DDrawer::AddIndices(int firstvert, TArray<int> &v)
{
	int addr = mIndices.Reserve(v.Size());
	for (unsigned i = 0; i < v.Size(); i++)
	{
		mIndices[addr + i] = firstvert + v[i];
	}
}

//==========================================================================
//
// SetStyle
//
// Patterned after R_SetPatchStyle.
//
//==========================================================================

bool F2DDrawer::SetStyle(FGameTexture *tex, DrawParms &parms, PalEntry &vertexcolor, RenderCommand &quad)
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

void F2DDrawer::AddTexture(FGameTexture* img, DrawParms& parms)
{
	if (parms.style.BlendOp == STYLEOP_None) return;	// not supposed to be drawn.
	assert(img && img->isValid());

	double xscale = parms.destwidth / parms.texwidth;
	double yscale = parms.destheight / parms.texheight;
	double u1, v1, u2, v2;
	PalEntry vertexcolor;

	RenderCommand dg;

	dg.mType = DrawTypeTriangles;
	dg.mVertCount = 4;
	dg.mTexture = img;
	if (img->isWarped()) dg.mFlags |= DTF_Wrap;
	if (parms.indexed) dg.mFlags |= DTF_Indexed;

	dg.mTranslationId = 0;
	SetStyle(img, parms, vertexcolor, dg);
	if (parms.indexed)
	{
		dg.mLightLevel = vertexcolor.Luminance();
		vertexcolor = 0xffffffff;
	}

	if (!img->isHardwareCanvas() && parms.TranslationId != -1)
	{
		dg.mTranslationId = parms.TranslationId;
	}
	u1 = parms.srcx;
	v1 = parms.srcy;
	u2 = parms.srcx + parms.srcwidth;
	v2 = parms.srcy + parms.srcheight;

	if (parms.flipX)
	{
		std::swap(u1, u2);
	}

	if (parms.flipY)
	{
		std::swap(v1, v2);
	}

	auto osave = offset;
	if (parms.nooffset) offset = { 0,0 };

	if (parms.rotateangle == 0)
	{
		double x = parms.x - parms.left * xscale;
		double y = parms.y - parms.top * yscale;
		double w = parms.destwidth;
		double h = parms.destheight;


		// This is crap. Only kept for backwards compatibility with scripts that may have used it.
		// Note that this only works for unflipped and unrotated full textures.
		if (parms.windowleft > 0 || parms.windowright < parms.texwidth)
		{
			double wi = std::min(parms.windowright, parms.texwidth);
			x += parms.windowleft * xscale;
			w -= (parms.texwidth - wi + parms.windowleft) * xscale;

			u1 = float(u1 + parms.windowleft / parms.texwidth);
			u2 = float(u2 - (parms.texwidth - wi) / parms.texwidth);
		}

		if (x < (double)parms.lclip || y < (double)parms.uclip || x + w >(double)parms.rclip || y + h >(double)parms.dclip)
		{
			dg.mScissor[0] = parms.lclip + int(offset.X);
			dg.mScissor[1] = parms.uclip + int(offset.Y);
			dg.mScissor[2] = parms.rclip + int(offset.X);
			dg.mScissor[3] = parms.dclip + int(offset.Y);
			dg.mFlags |= DTF_Scissor;
		}
		else
		{
			memset(dg.mScissor, 0, sizeof(dg.mScissor));
		}

		dg.mVertCount = 4;
		dg.mVertIndex = (int)mVertices.Reserve(4);
		TwoDVertex* ptr = &mVertices[dg.mVertIndex];
		Set(ptr, x, y, 0, u1, v1, vertexcolor); ptr++;
		Set(ptr, x, y + h, 0, u1, v2, vertexcolor); ptr++;
		Set(ptr, x + w, y, 0, u2, v1, vertexcolor); ptr++;
		Set(ptr, x + w, y + h, 0, u2, v2, vertexcolor); ptr++;
	}
	else
	{
		double radang = parms.rotateangle * (pi::pi() / 180.);
		double cosang = cos(radang);
		double sinang = sin(radang);
		double xd1 = -parms.left;
		double yd1 = -parms.top;
		double xd2 = xd1 + parms.texwidth;
		double yd2 = yd1 + parms.texheight;

		double x1 = parms.x + xscale * (xd1 * cosang + yd1 * sinang);
		double y1 = parms.y - yscale * (xd1 * sinang - yd1 * cosang);

		double x2 = parms.x + xscale * (xd1 * cosang + yd2 * sinang);
		double y2 = parms.y - yscale * (xd1 * sinang - yd2 * cosang);

		double x3 = parms.x + xscale * (xd2 * cosang + yd1 * sinang);
		double y3 = parms.y - yscale * (xd2 * sinang - yd1 * cosang);

		double x4 = parms.x + xscale * (xd2 * cosang + yd2 * sinang);
		double y4 = parms.y - yscale * (xd2 * sinang - yd2 * cosang);

		dg.mScissor[0] = parms.lclip + int(offset.X);
		dg.mScissor[1] = parms.uclip + int(offset.Y);
		dg.mScissor[2] = parms.rclip + int(offset.X);
		dg.mScissor[3] = parms.dclip + int(offset.Y);
		dg.mFlags |= DTF_Scissor;

		dg.mVertCount = 4;
		dg.mVertIndex = (int)mVertices.Reserve(4);
		TwoDVertex* ptr = &mVertices[dg.mVertIndex];
		Set(ptr, x1, y1, 0, u1, v1, vertexcolor); ptr++;
		Set(ptr, x2, y2, 0, u1, v2, vertexcolor); ptr++;
		Set(ptr, x3, y3, 0, u2, v1, vertexcolor); ptr++;
		Set(ptr, x4, y4, 0, u2, v2, vertexcolor); ptr++;

	}
	dg.mIndexIndex = mIndices.Size();
	dg.mIndexCount += 6;
	AddIndices(dg.mVertIndex, 6, 0, 1, 2, 1, 3, 2);
	AddCommand(&dg);
	offset = osave;
}

static TArray<RefCountedPtr<DShape2DBufferInfo>> buffersToDestroy;

void DShape2D::OnDestroy() {
	if (lastParms) delete lastParms;
	lastParms = nullptr;
	mIndices.Reset();
	mVertices.Reset();
	mCoords.Reset();
	buffersToDestroy.Push(std::move(bufferInfo));
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddShape(FGameTexture* img, DShape2D* shape, DrawParms& parms)
{
	// [MK] bail out if vertex/coord array sizes are mismatched
	if ( shape->mVertices.Size() != shape->mCoords.Size() )
		ThrowAbortException(X_OTHER, "Mismatch in vertex/coord count: %u != %u", shape->mVertices.Size(), shape->mCoords.Size());

	if (parms.style.BlendOp == STYLEOP_None) return;	// not supposed to be drawn.

	PalEntry vertexcolor;

	RenderCommand dg;

	dg.mType = DrawTypeTriangles;
	dg.mVertCount = shape->mVertices.Size();
	dg.mFlags |= DTF_Wrap;
	dg.mTexture = img;

	dg.mTranslationId = 0;
	SetStyle(img, parms, vertexcolor, dg);

	if (shape->lastParms == nullptr) {
		shape->lastParms = new DrawParms(parms);
	}
	else if (shape->lastParms->vertexColorChange(parms)) {
		shape->bufferInfo->needsVertexUpload = true;
		if (!shape->bufferInfo->uploadedOnce) {
			shape->bufferInfo->bufIndex = -1;
			shape->bufferInfo->buffers.Clear();
			shape->bufferInfo->lastCommand = -1;
		}
		delete shape->lastParms;
		shape->lastParms = new DrawParms(parms);
	}

	if (!img->isHardwareCanvas() && parms.TranslationId != -1)
		dg.mTranslationId = parms.TranslationId;

	auto osave = offset;
	if (parms.nooffset) offset = { 0,0 };

	if (shape->bufferInfo->needsVertexUpload)
	{
		shape->minx = 16383;
		shape->miny = 16383;
		shape->maxx = -16384;
		shape->maxy = -16384;
		for ( int i=0; i<dg.mVertCount; i++ )
		{
			if ( shape->mVertices[i].X < shape->minx ) shape->minx = shape->mVertices[i].X;
			if ( shape->mVertices[i].Y < shape->miny ) shape->miny = shape->mVertices[i].Y;
			if ( shape->mVertices[i].X > shape->maxx ) shape->maxx = shape->mVertices[i].X;
			if ( shape->mVertices[i].Y > shape->maxy ) shape->maxy = shape->mVertices[i].Y;
		}
	}
	auto tCorners = {
		(shape->transform * DVector3(shape->minx, shape->miny, 1.0)).XY(),
		(shape->transform * DVector3(shape->minx, shape->maxy, 1.0)).XY(),
		(shape->transform * DVector3(shape->maxx, shape->miny, 1.0)).XY(),
		(shape->transform * DVector3(shape->maxx, shape->maxy, 1.0)).XY()
	};
	double minx = std::min_element(tCorners.begin(), tCorners.end(), [] (auto d0, auto d1) { return d0.X < d1.X; })->X;
	double maxx = std::max_element(tCorners.begin(), tCorners.end(), [] (auto d0, auto d1) { return d0.X < d1.X; })->X;
	double miny = std::min_element(tCorners.begin(), tCorners.end(), [] (auto d0, auto d1) { return d0.Y < d1.Y; })->Y;
	double maxy = std::max_element(tCorners.begin(), tCorners.end(), [] (auto d0, auto d1) { return d0.Y < d1.Y; })->Y;
	if (minx < (double)parms.lclip || miny < (double)parms.uclip || maxx >(double)parms.rclip || maxy >(double)parms.dclip)
	{
		dg.mScissor[0] = parms.lclip + int(offset.X);
		dg.mScissor[1] = parms.uclip + int(offset.Y);
		dg.mScissor[2] = parms.rclip + int(offset.X);
		dg.mScissor[3] = parms.dclip + int(offset.Y);
		dg.mFlags |= DTF_Scissor;
	}
	else
		memset(dg.mScissor, 0, sizeof(dg.mScissor));

	dg.useTransform = true;
	dg.transform = shape->transform;
	dg.transform.Cells[0][2] += offset.X;
	dg.transform.Cells[1][2] += offset.Y;
	dg.shape2DBufInfo = shape->bufferInfo;
	dg.shape2DIndexCount = shape->mIndices.Size();
	if (shape->bufferInfo->needsVertexUpload)
	{
		shape->bufferInfo->bufIndex += 1;

		shape->bufferInfo->buffers.Reserve(1);

		auto buf = &shape->bufferInfo->buffers[shape->bufferInfo->bufIndex];

		auto verts = TArray<TwoDVertex>(dg.mVertCount, true);
		for ( int i=0; i<dg.mVertCount; i++ )
			verts[i].Set(shape->mVertices[i].X, shape->mVertices[i].Y, 0, shape->mCoords[i].X, shape->mCoords[i].Y, vertexcolor);

		for ( int i=0; i<int(shape->mIndices.Size()); i+=3 )
		{
			// [MK] bail out if any indices are out of bounds
			for ( int j=0; j<3; j++ )
			{
				if ( shape->mIndices[i+j] < 0 )
					ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Triangle %u index %u is negative: %i\n", i/3, j, shape->mIndices[i+j]);
				if ( shape->mIndices[i+j] >= dg.mVertCount )
					ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Triangle %u index %u: %u, max: %u\n", i/3, j, shape->mIndices[i+j], dg.mVertCount-1);
			}
		}

		buf->UploadData(&verts[0], dg.mVertCount, &shape->mIndices[0], shape->mIndices.Size());
		shape->bufferInfo->needsVertexUpload = false;
		shape->bufferInfo->uploadedOnce = true;
	}
	dg.shape2DBufIndex = shape->bufferInfo->bufIndex;
	shape->bufferInfo->lastCommand += 1;
	dg.shape2DCommandCounter = shape->bufferInfo->lastCommand;
	AddCommand(&dg);
	offset = osave;
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddPoly(FGameTexture *texture, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, const FColormap &colormap, PalEntry flatcolor, double fadelevel,
		uint32_t *indices, size_t indexcount)
{
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
		Set(&mVertices[poly.mVertIndex+i], points[i].X, points[i].Y, 0, u*uscale, v*vscale, color0);
	}
	poly.mIndexIndex = mIndices.Size();

	if (indices == nullptr || indexcount == 0)
	{
		poly.mIndexCount += (npoints - 2) * 3;
		for (int i = 2; i < npoints; ++i)
		{
			AddIndices(poly.mVertIndex, 3, 0, i - 1, i);
		}
	}
	else
	{
		poly.mIndexCount += (int)indexcount;
		int addr = mIndices.Reserve(indexcount);
		for (size_t i = 0; i < indexcount; i++)
		{
			mIndices[addr + i] = poly.mVertIndex + indices[i];
		}
	}

	AddCommand(&poly);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddPoly(FGameTexture* img, FVector4* vt, size_t vtcount, const unsigned int* ind, size_t idxcount, int translation, PalEntry color, FRenderStyle style, int clipx1, int clipy1, int clipx2, int clipy2)
{
	RenderCommand dg;
	int method = 0;

	if (!img || !img->isValid()) return;

	dg.mType = DrawTypeTriangles;
	if (clipx1 > 0 || clipy1 > 0 || clipx2 < GetWidth() - 1 || clipy2 < GetHeight() - 1)
	{
		dg.mScissor[0] = clipx1     + int(offset.X);
		dg.mScissor[1] = clipy1     + int(offset.Y);
		dg.mScissor[2] = clipx2 + 1 + int(offset.X);
		dg.mScissor[3] = clipy2 + 1 + int(offset.Y);
		dg.mFlags |= DTF_Scissor;
	}

	dg.mTexture = img;
	dg.mTranslationId = translation;
	dg.mVertCount = (int)vtcount;
	dg.mVertIndex = (int)mVertices.Reserve(vtcount);
	dg.mRenderStyle = LegacyRenderStyles[STYLE_Translucent];
	dg.mIndexIndex = mIndices.Size();
	dg.mFlags |= DTF_Wrap;
	auto ptr = &mVertices[dg.mVertIndex];

	for (size_t i=0;i<vtcount;i++)
	{
		Set(ptr, vt[i].X, vt[i].Y, 0.f, vt[i].Z, vt[i].W, color);
		ptr++;
	}
	dg.mIndexIndex = mIndices.Size();

	if (idxcount > 0)
	{
		mIndices.Reserve(idxcount);
		for (size_t i = 0; i < idxcount; i++)
		{
			mIndices[dg.mIndexIndex + i] = ind[i] + dg.mVertIndex;
		}
		dg.mIndexCount = (int)idxcount;
	}
	else
	{
		// If we have no index buffer, treat this as an unindexed list of triangles.
		mIndices.Reserve(vtcount);
		for (size_t i = 0; i < vtcount; i++)
		{
			mIndices[dg.mIndexIndex + i] = int(i + dg.mVertIndex);
		}
		dg.mIndexCount = (int)vtcount;

	}
	AddCommand(&dg);
}

//==========================================================================
//
//
//
//==========================================================================

float F2DDrawer::GetClassicFlatScalarWidth()
{
	float ar = 4.f / 3.f / (float)ActiveRatio((float)screen->GetWidth(), (float)screen->GetHeight());
	float sw = 320.f * classic_scaling_factor / (float)screen->GetWidth() / ar;
	return sw;
}

float F2DDrawer::GetClassicFlatScalarHeight()
{
	float sh = 240.f / classic_scaling_pixelaspect * classic_scaling_factor / (float)screen->GetHeight();
	return sh;
}

void F2DDrawer::AddFlatFill(int left, int top, int right, int bottom, FGameTexture *src, int local_origin, double flatscale, PalEntry color, ERenderStyle style)
{
	float fU1, fU2, fV1, fV2;

	RenderCommand dg;

	dg.mType = DrawTypeTriangles;
	dg.mRenderStyle = LegacyRenderStyles[style];
	dg.mTexture = src;
	dg.mVertCount = 4;
	dg.mTexture = src;
	dg.mFlags = DTF_Wrap;

	float fs = 1.f / float(flatscale);
	bool flipc = false;

	float sw = GetClassicFlatScalarWidth();
	float sh = GetClassicFlatScalarHeight();

	switch (local_origin)
	{
	case 0:
		fU1 = float(left) / (float)src->GetDisplayWidth() * fs;
		fV1 = float(top) / (float)src->GetDisplayHeight() * fs;
		fU2 = float(right) / (float)src->GetDisplayWidth() * fs;
		fV2 = float(bottom) / (float)src->GetDisplayHeight() * fs;
		break;

	case 1:
		fU1 = 0;
		fV1 = 0;
		fU2 = float(right - left) / (float)src->GetDisplayWidth() * fs;
		fV2 = float(bottom - top) / (float)src->GetDisplayHeight() * fs;
		break;

		// The following are for drawing frames with elements of pnly one orientation
	case 2: // flip vertically
		fU1 = 0;
		fV2 = 0;
		fU2 = float(right - left) / (float)src->GetDisplayWidth() * fs;
		fV1 = float(bottom - top) / (float)src->GetDisplayHeight() * fs;
		break;

	case 3:	// flip horizontally
		fU2 = 0;
		fV1 = 0;
		fU1 = float(right - left) / (float)src->GetDisplayWidth() * fs;
		fV2 = float(bottom - top) / (float)src->GetDisplayHeight() * fs;
		break;

	case 4:	// flip vertically and horizontally
		fU2 = 0;
		fV2 = 0;
		fU1 = float(right - left) / (float)src->GetDisplayWidth() * fs;
		fV1 = float(bottom - top) / (float)src->GetDisplayHeight() * fs;
		break;


	case 5:	// flip coordinates
		fU1 = 0;
		fV1 = 0;
		fU2 = float(bottom - top) / (float)src->GetDisplayWidth() * fs;
		fV2 = float(right - left) / (float)src->GetDisplayHeight() * fs;
		break;

	case 6:	// flip coordinates and vertically
		fU2 = 0;
		fV1 = 0;
		fU1 = float(bottom - top) / (float)src->GetDisplayWidth() * fs;
		fV2 = float(right - left) / (float)src->GetDisplayHeight() * fs;
		break;

	case 7:	// flip coordinates and horizontally
		fU1 = 0;
		fV2 = 0;
		fU2 = float(bottom - top) / (float)src->GetDisplayWidth() * fs;
		fV1 = float(right - left) / (float)src->GetDisplayHeight() * fs;
		break;

	case -1: // classic flat scaling
		fU1 = float(left) / (float)src->GetDisplayWidth() * fs * sw;
		fV1 = float(top) / (float)src->GetDisplayHeight() * fs * sh;
		fU2 = float(right) / (float)src->GetDisplayWidth() * fs * sw;
		fV2 = float(bottom) / (float)src->GetDisplayHeight() * fs * sh;
		break;

	case -2: // classic scaling for screen bevel
		fU1 = 0;
		fV1 = 0;
		fU2 = float(right - left) / (float)src->GetDisplayWidth() * fs * sw;
		fV2 = float(bottom - top) / (float)src->GetDisplayHeight() * fs * sh;
		break;
	}
	dg.mVertIndex = (int)mVertices.Reserve(4);
	auto ptr = &mVertices[dg.mVertIndex];

	Set(ptr, left, top, 0, fU1, fV1, color); ptr++;
	if (local_origin < 4)
	{
		Set(ptr, left, bottom, 0, fU1, fV2, color); ptr++;
		Set(ptr, right, top, 0, fU2, fV1, color); ptr++;
	}
	else
	{
		Set(ptr, left, bottom, 0, fU2, fV1, color); ptr++;
		Set(ptr, right, top, 0, fU1, fV2, color); ptr++;
	}
	Set(ptr, right, bottom, 0, fU2, fV2, color); ptr++;
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

void F2DDrawer::AddColorOnlyQuad(int x1, int y1, int w, int h, PalEntry color, FRenderStyle *style, bool prepend)
{
	RenderCommand dg;

	dg.mType = DrawTypeTriangles;
	dg.mVertCount = 4;
	dg.mVertIndex = (int)mVertices.Reserve(4);
	dg.mRenderStyle = style? *style : LegacyRenderStyles[STYLE_Translucent];
	auto ptr = &mVertices[dg.mVertIndex];
	Set(ptr, x1, y1, 0, 0, 0, color); ptr++;
	Set(ptr, x1, y1 + h, 0, 0, 0, color); ptr++;
	Set(ptr, x1 + w, y1, 0, 0, 0, color); ptr++;
	Set(ptr, x1 + w, y1 + h, 0, 0, 0, color); ptr++;
	dg.mIndexIndex = mIndices.Size();
	dg.mIndexCount += 6;
	AddIndices(dg.mVertIndex, 6, 0, 1, 2, 1, 3, 2);
	if (!prepend) AddCommand(&dg);
	else
	{
		// Only needed by Raze's fullscreen blends because they are being calculated late when half of the 2D content has already been submitted,
		// This ensures they are below the HUD, not above it.
		dg.mScreenFade = screenFade;
		mData.Insert(0, dg);
	}
}

void F2DDrawer::ClearScreen(PalEntry color)
{
	AddColorOnlyQuad(0, 0, GetWidth(), GetHeight(), color);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::AddLine(double x1, double y1, double x2, double y2, int clipx1, int clipy1, int clipx2, int clipy2, uint32_t color, uint8_t alpha)
{
	PalEntry p = (PalEntry)color;
	p.a = alpha;

	RenderCommand dg;

	if (clipx1 > 0 || clipy1 > 0 || clipx2 < GetWidth()- 1 || clipy2 < GetHeight() - 1)
	{
		dg.mScissor[0] = clipx1     + int(offset.X);
		dg.mScissor[1] = clipy1     + int(offset.Y);
		dg.mScissor[2] = clipx2 + 1 + int(offset.X);
		dg.mScissor[3] = clipy2 + 1 + int(offset.Y);
		dg.mFlags |= DTF_Scissor;
	}
	
	dg.mType = DrawTypeLines;
	dg.mRenderStyle = LegacyRenderStyles[STYLE_Translucent];
	dg.mVertCount = 2;
	dg.mVertIndex = (int)mVertices.Reserve(2);
	Set(&mVertices[dg.mVertIndex], x1, y1, 0, 0, 0, p);
	Set(&mVertices[dg.mVertIndex+1], x2, y2, 0, 0, 0, p);
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
	Set(ptr, corner0.X, corner0.Y, 0, 0, 0, p); ptr++;
	Set(ptr, corner1.X, corner1.Y, 0, 0, 0, p); ptr++;
	Set(ptr, corner2.X, corner2.Y, 0, 0, 0, p); ptr++;
	Set(ptr, corner3.X, corner3.Y, 0, 0, 0, p); ptr++;
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

void F2DDrawer::AddPixel(int x1, int y1, uint32_t color)
{
	PalEntry p = (PalEntry)color;
	p.a = 255;

	RenderCommand dg;

	dg.mType = DrawTypePoints;
	dg.mRenderStyle = LegacyRenderStyles[STYLE_Translucent];
	dg.mVertCount = 1;
	dg.mVertIndex = (int)mVertices.Reserve(1);
	Set(&mVertices[dg.mVertIndex], x1, y1, 0, 0, 0, p);
	AddCommand(&dg);
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::Clear()
{
	if (!locked)
	{
		mVertices.Clear();
		mIndices.Clear();
		mData.Clear();
		mIsFirstPass = true;
	}
	screenFade = 1.f;
}

//==========================================================================
//
//
//
//==========================================================================

void F2DDrawer::OnFrameDone()
{
	buffersToDestroy.Clear();
}

F2DVertexBuffer::F2DVertexBuffer()
{
	mVertexBuffer = screen->CreateVertexBuffer();
	mIndexBuffer = screen->CreateIndexBuffer();

	static const FVertexBufferAttribute format[] = {
		{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(F2DDrawer::TwoDVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(F2DDrawer::TwoDVertex, u) },
		{ 0, VATTR_COLOR, VFmt_Byte4, (int)myoffsetof(F2DDrawer::TwoDVertex, color0) }
	};
	mVertexBuffer->SetFormat(1, 3, sizeof(F2DDrawer::TwoDVertex), format);
}
