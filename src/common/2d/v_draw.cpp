/*
** v_draw.cpp
** Draw patches and blocks to a canvas
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#include <stdio.h>
#include <stdarg.h>
#include "v_draw.h"
#include "vm.h"

#include "texturemanager.h"
#include "r_videoscale.h"
#include "c_cvars.h"

EXTERN_CVAR(Int, vid_aspect)
EXTERN_CVAR(Int, uiscale)
CVAR(Bool, ui_screenborder_classic_scaling, true, CVAR_ARCHIVE)

// vid_allowtrueultrawide - preserve the classic behavior of stretching screen elements to 16:9 when false
// Defaults to "true" now because "21:9" (actually actually 64:27) screens are becoming more common, it's
// nonsense that graphics should not be able to actually use that extra screen space.

extern bool setsizeneeded;
CUSTOM_CVAR(Int, vid_allowtrueultrawide, 1, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	setsizeneeded = true;
}

static void VirtualToRealCoords(F2DDrawer* drawer, double Width, double Height, double& x, double& y, double& w, double& h,
	double vwidth, double vheight, bool vbottom, bool handleaspect);

// Helper for ActiveRatio and CheckRatio. Returns the forced ratio type, or -1 if none.
int ActiveFakeRatio(int width, int height)
{
	int fakeratio = -1;
	if ((vid_aspect >= 1) && (vid_aspect <= 6))
	{
		// [SP] User wants to force aspect ratio; let them.
		fakeratio = int(vid_aspect);
		if (fakeratio == 3)
		{
			fakeratio = 0;
		}
		else if (fakeratio == 5)
		{
			fakeratio = 3;
		}
	}
	return fakeratio;
}

// Active screen ratio based on cvars and size
float ActiveRatio(int width, int height, float* trueratio)
{
	static float forcedRatioTypes[] =
	{
		4 / 3.0f,
		16 / 9.0f,
		16 / 10.0f,
		17 / 10.0f,
		5 / 4.0f,
		17 / 10.0f,
		64 / 27.0f	// 21:9 is actually 64:27 in reality - pow(4/3, 3.0) - https://en.wikipedia.org/wiki/21:9_aspect_ratio
	};

	float ratio = width / (float)height;
	int fakeratio = ActiveFakeRatio(width, height);

	if (trueratio)
		*trueratio = ratio;
	return (fakeratio != -1) ? forcedRatioTypes[fakeratio] : (ratio / ViewportPixelAspect());
}


bool AspectTallerThanWide(float aspect)
{
	return aspect < 1.333f;
}

int AspectBaseWidth(float aspect)
{
	return (int)round(240.0f * aspect * 3.0f);
}

int AspectBaseHeight(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return (int)round(200.0f * (320.0f / (AspectBaseWidth(aspect) / 3.0f)) * 3.0f);
	else
		return (int)round((200.0f * (4.0f / 3.0f)) / aspect * 3.0f);
}

double AspectPspriteOffset(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return 0.0;
	else
		return ((4.0 / 3.0) / aspect - 1.0) * 97.5;
}

int AspectMultiplier(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return (int)round(320.0f / (AspectBaseWidth(aspect) / 3.0f) * 48.0f);
	else
		return (int)round(200.0f / (AspectBaseHeight(aspect) / 3.0f) * 48.0f);
}

int GetUIScale(F2DDrawer *drawer, int altval)
{
	int scaleval;
	if (altval > 0) scaleval = altval;
	else if (uiscale == 0)
	{
		// Default should try to scale to 640x400
		int vscale = drawer->GetHeight() / 400;
		int hscale = drawer->GetWidth() / 640;
		scaleval = max(1, min(vscale, hscale));
	}
	else scaleval = uiscale;

	// block scales that result in something larger than the current screen.
	int vmax = drawer->GetHeight() / 200;
	int hmax = drawer->GetWidth() / 320;
	int max = std::max(vmax, hmax);
	return std::max(1,min(scaleval, max));
}

// The new console font is twice as high, so the scaling calculation must factor that in.
int GetConScale(F2DDrawer* drawer, int altval)
{
	int scaleval;
	if (altval > 0) scaleval = (altval+1) / 2;
	else if (uiscale == 0)
	{
		// Default should try to scale to 640x400
		int vscale = drawer->GetHeight() / 800;
		int hscale = drawer->GetWidth() / 1280;
		scaleval = max(1, min(vscale, hscale));
	}
	else scaleval = (uiscale+1) / 2;

	// block scales that result in something larger than the current screen.
	int vmax = drawer->GetHeight() / 400;
	int hmax = drawer->GetWidth() / 640;
	int max = std::max(vmax, hmax);
	return std::max(1, min(scaleval, max));
}


// [RH] Stretch values to make a 320x200 image best fit the screen
// without using fractional steppings
int CleanXfac, CleanYfac;

// [RH] Effective screen sizes that the above scale values give you
int CleanWidth, CleanHeight;

// Above minus 1 (or 1, if they are already 1)
int CleanXfac_1, CleanYfac_1, CleanWidth_1, CleanHeight_1;


//==========================================================================
//
// Internal texture drawing function
//
//==========================================================================

void DrawTexture(F2DDrawer *drawer, FGameTexture* img, double x, double y, int tags_first, ...)
{
	Va_List tags;
	va_start(tags.list, tags_first);
	DrawParms parms;

	if (!img || !img->isValid()) return;
	bool res = ParseDrawTextureTags(drawer, img, x, y, tags_first, tags, &parms, false);
	va_end(tags.list);
	if (!res)
	{
		return;
	}
	drawer->AddTexture(img, parms);
}

//==========================================================================
//
// ZScript texture drawing function
//
//==========================================================================

int ListGetInt(VMVa_List &tags);

static void DrawTexture(F2DDrawer *drawer, FGameTexture *img, double x, double y, VMVa_List &args)
{
	DrawParms parms;
	uint32_t tag = ListGetInt(args);
	if (!img || !img->isValid()) return;
	bool res = ParseDrawTextureTags(drawer, img, x, y, tag, args, &parms, false);
	if (!res) return;
	drawer->AddTexture(img, parms);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawTexture)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	PARAM_BOOL(animate);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);

	PARAM_VA_POINTER(va_reginfo)	// Get the hidden type information array

	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");

	auto tex = TexMan.GameByIndex(texid, animate);
	VMVa_List args = { param + 4, 0, numparam - 5, va_reginfo + 4 };
	DrawTexture(twod, tex, x, y, args);
	return 0;
}

//==========================================================================
//
// ZScript arbitrary textured shape drawing functions
//
//==========================================================================

void DrawShape(F2DDrawer *drawer, FGameTexture *img, DShape2D *shape, int tags_first, ...)
{
	Va_List tags;
	va_start(tags.list, tags_first);
	DrawParms parms;

	bool res = ParseDrawTextureTags(drawer, img, 0, 0, tags_first, tags, &parms, false);
	va_end(tags.list);
	if (!res) return;
	drawer->AddShape(img, shape, parms);
}

void DrawShape(F2DDrawer *drawer, FGameTexture *img, DShape2D *shape, VMVa_List &args)
{
	DrawParms parms;
	uint32_t tag = ListGetInt(args);

	bool res = ParseDrawTextureTags(drawer, img, 0, 0, tag, args, &parms, false);
	if (!res) return;
	drawer->AddShape(img, shape, parms);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawShape)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	PARAM_BOOL(animate);
	PARAM_POINTER(shape, DShape2D);

	PARAM_VA_POINTER(va_reginfo)	// Get the hidden type information array

	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");

	auto tex = TexMan.GameByIndex(texid, animate);
	VMVa_List args = { param + 3, 0, numparam - 4, va_reginfo + 3 };

	DrawShape(twod, tex, shape, args);
	return 0;
}

//==========================================================================
//
// Clipping rect
//
//==========================================================================

void F2DDrawer::SetClipRect(int x, int y, int w, int h)
{
	clipleft = clamp(x, 0, GetWidth());
	clipwidth = clamp(w, -1, GetWidth() - x);
	cliptop = clamp(y, 0, GetHeight());
	clipheight = clamp(h, -1, GetHeight() - y);
}

DEFINE_ACTION_FUNCTION(_Screen, SetClipRect)
{
	PARAM_PROLOGUE;
	PARAM_INT(x);
	PARAM_INT(y);
	PARAM_INT(w);
	PARAM_INT(h);
	twod->SetClipRect(x, y, w, h);
	return 0;
}

DEFINE_ACTION_FUNCTION(_Screen, ClearClipRect)
{
	PARAM_PROLOGUE;
	twod->ClearClipRect();
	return 0;
}

DEFINE_ACTION_FUNCTION(_Screen, ClearScreen)
{
	PARAM_PROLOGUE;
	twod->ClearScreen();
	return 0;
}

DEFINE_ACTION_FUNCTION(_Screen, SetScreenFade)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x);
	twod->SetScreenFade(float(x));
	return 0;
}


void F2DDrawer::GetClipRect(int *x, int *y, int *w, int *h)
{
	if (x) *x = clipleft;
	if (y) *y = cliptop;
	if (w) *w = clipwidth;
	if (h) *h = clipheight;
}

DEFINE_ACTION_FUNCTION(_Screen, GetClipRect)
{
	PARAM_PROLOGUE;
	int x, y, w, h;
	twod->GetClipRect(&x, &y, &w, &h);
	if (numret > 0) ret[0].SetInt(x);
	if (numret > 1) ret[1].SetInt(y);
	if (numret > 2) ret[2].SetInt(w);
	if (numret > 3) ret[3].SetInt(h);
	return min(numret, 4);
}


void CalcFullscreenScale(DrawParms *parms, double srcwidth, double srcheight, int oautoaspect, DoubleRect &rect)
{
	auto GetWidth = [=]() { return parms->viewport.width; };
	auto GetHeight = [=]() {return parms->viewport.height; };

	int autoaspect = oautoaspect;
	if (autoaspect == FSMode_ScaleToScreen)
	{
		rect.left = rect.top = 0;
		rect.width = GetWidth();
		rect.height = GetHeight();
		return;
	}

	double aspect;
	if (srcheight == 200) srcheight = 240.;
	else if (srcheight == 400) srcheight = 480;
	aspect = srcwidth / srcheight;
	rect.left = rect.top = 0;
	auto screenratio = ActiveRatio(GetWidth(), GetHeight());
	if (autoaspect == FSMode_ScaleToFit43 || autoaspect == FSMode_ScaleToFit43Top || autoaspect == FSMode_ScaleToFit43Bottom)
	{
		// screen is wider than the image -> pillarbox it. 4:3 images must also be pillarboxed if the screen is taller than the image
		if (screenratio >= aspect || aspect < 1.4) autoaspect = FSMode_ScaleToFit; 
		else if (screenratio > 1.32) autoaspect = FSMode_ScaleToFill;				// on anything 4:3 and wider crop the sides of the image.
		else
		{
			// special case: Crop image to 4:3 and then letterbox this. This avoids too much cropping on narrow windows.
			double width4_3 = srcheight * (4. / 3.);
			rect.width = (double)GetWidth() * srcwidth / width4_3;
			rect.height = GetHeight() * screenratio * (3. / 4.);	// use 4:3 for the image
			rect.left = (double)GetWidth() * (-(srcwidth - width4_3) / 2) / width4_3;
			switch (oautoaspect)
			{
			default:
				rect.top = (GetHeight() - rect.height) / 2;
				break;
			case FSMode_ScaleToFit43Top:
				rect.top = 0;
				break;
			case FSMode_ScaleToFit43Bottom:
				rect.top = (GetHeight() - rect.height);
				break;
			}
			return;
		}
	}

	if (autoaspect == FSMode_ScaleToHeight || (screenratio > aspect) ^ (autoaspect == FSMode_ScaleToFill))
	{
		// pillarboxed or vertically cropped (i.e. scale to height)
		rect.height = GetHeight();
		rect.width = GetWidth() * aspect / screenratio;
		rect.left = (GetWidth() - rect.width) / 2;
	}
	else
	{
		// letterboxed or horizontally cropped (i.e. scale to width)
		rect.width = GetWidth();
		rect.height = GetHeight() * screenratio / aspect;
		switch (oautoaspect)
		{
		default:
			rect.top = (GetHeight() - rect.height) / 2;
			break;
		case FSMode_ScaleToFit43Top:
			rect.top = 0;
			break;
		case FSMode_ScaleToFit43Bottom:
			rect.top = (GetHeight() - rect.height);
			break;
		}
	}
}

void GetFullscreenRect(double width, double height, int fsmode, DoubleRect* rect)
{
	DrawParms parms;
	parms.viewport.width = twod->GetWidth();
	parms.viewport.height = twod->GetHeight();
	CalcFullscreenScale(&parms, width, height, fsmode, *rect);
}

DEFINE_ACTION_FUNCTION(_Screen, GetFullscreenRect)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(virtw);
	PARAM_FLOAT(virth);
	PARAM_INT(fsmode);

	DrawParms parms;
	DoubleRect rect;
	parms.viewport.width = twod->GetWidth();
	parms.viewport.height = twod->GetHeight();
	CalcFullscreenScale(&parms, virtw, virth, fsmode, rect);
	if (numret >= 1) ret[0].SetFloat(rect.left);
	if (numret >= 2) ret[1].SetFloat(rect.top);
	if (numret >= 3) ret[2].SetFloat(rect.width);
	if (numret >= 4) ret[3].SetFloat(rect.height);
	return min(numret, 4);
}



//==========================================================================
//
// Draw parameter parsing
//
//==========================================================================

bool SetTextureParms(F2DDrawer * drawer, DrawParms *parms, FGameTexture *img, double xx, double yy)
{
	auto GetWidth = [=]() { return parms->viewport.width; };
	auto GetHeight = [=]() {return parms->viewport.height; };
	if (img != NULL)
	{
		parms->x = xx;
		parms->y = yy;
		parms->texwidth = img->GetDisplayWidth();
		parms->texheight = img->GetDisplayHeight();
		if (parms->top == INT_MAX || parms->fortext)
		{
			parms->top = img->GetDisplayTopOffset();
		}
		if (parms->left == INT_MAX || parms->fortext)
		{
			parms->left = img->GetDisplayLeftOffset();
		}
		if (parms->destwidth == INT_MAX || parms->fortext)
		{
			parms->destwidth = parms->texwidth;
		}
		if (parms->destheight == INT_MAX || parms->fortext)
		{
			parms->destheight = parms->texheight;
		}
		parms->destwidth *= parms->patchscalex;
		parms->destheight *= parms->patchscaley;

		if (parms->flipoffsets && parms->flipY) parms->top = parms->texheight - parms->top;
		if (parms->flipoffsets && parms->flipX) parms->left = parms->texwidth - parms->left;

		switch (parms->cleanmode)
		{
		default:
			break;

		case DTA_Clean:
			parms->x = (parms->x - 160.0) * CleanXfac + (GetWidth() * 0.5);
			parms->y = (parms->y - 100.0) * CleanYfac + (GetHeight() * 0.5);
			parms->destwidth = parms->texwidth * CleanXfac;
			parms->destheight = parms->texheight * CleanYfac;
			break;

		case DTA_CleanTop:
			parms->x = (parms->x - 160.0) * CleanXfac + (GetWidth() * 0.5);
			parms->y = (parms->y) * CleanYfac;
			parms->destwidth = parms->texwidth * CleanXfac;
			parms->destheight = parms->texheight * CleanYfac;
			break;

		case DTA_CleanNoMove:
			parms->destwidth = parms->texwidth * CleanXfac;
			parms->destheight = parms->texheight * CleanYfac;
			break;

		case DTA_CleanNoMove_1:
			parms->destwidth = parms->texwidth * CleanXfac_1;
			parms->destheight = parms->texheight * CleanYfac_1;
			break;

		case DTA_Base:
			if (parms->fsscalemode > 0)
			{
				// First calculate the destination rect for an image of the given size and then reposition this object in it.
				DoubleRect rect;
				CalcFullscreenScale(parms, parms->virtWidth, parms->virtHeight, parms->fsscalemode, rect);
				double adder = parms->keepratio < 0 ? 0 : parms->keepratio == 0 ? rect.left : 2 * rect.left;
				parms->x = parms->viewport.left + adder + parms->x * rect.width / parms->virtWidth;
				parms->y = parms->viewport.top + rect.top + parms->y * rect.height / parms->virtHeight;
				parms->destwidth = parms->destwidth * rect.width / parms->virtWidth;
				parms->destheight = parms->destheight * rect.height / parms->virtHeight;
				return false;
			}
			break;

		case DTA_Fullscreen:
		case DTA_FullscreenEx:
		{
			DoubleRect rect;
			CalcFullscreenScale(parms, parms->texwidth, parms->texheight, parms->fsscalemode, rect);
			parms->keepratio = -1;
			parms->x = parms->viewport.left + rect.left;
			parms->y = parms->viewport.top + rect.top;
			parms->destwidth = rect.width;
			parms->destheight = rect.height;
			parms->top = parms->left = 0;
			return false; // Do not call VirtualToRealCoords for this!
		}

		case DTA_HUDRules:
		case DTA_HUDRulesC:
		{
			// Note that this has been deprecated and become non-functional. The HUD should be drawn by the status bar.
			bool xright = parms->x < 0;
			bool ybot = parms->y < 0;
			DVector2 scale = { 1., 1. };

			parms->x *= scale.X;
			if (parms->cleanmode == DTA_HUDRulesC)
				parms->x += GetWidth() * 0.5;
			else if (xright)
				parms->x = GetWidth() + parms->x;
			parms->y *= scale.Y;
			if (ybot)
				parms->y = GetHeight() + parms->y;
			parms->destwidth = parms->texwidth * scale.X;
			parms->destheight = parms->texheight * scale.Y;
			break;
		}
		}
		if (parms->virtWidth != GetWidth() || parms->virtHeight != GetHeight())
		{
			VirtualToRealCoords(drawer, GetWidth(), GetHeight(), parms->x, parms->y, parms->destwidth, parms->destheight,
				parms->virtWidth, parms->virtHeight, parms->virtBottom, !parms->keepratio);
		}
		parms->x += parms->viewport.left;
		parms->y += parms->viewport.top;
	}

	return false;
}

//==========================================================================
//
// template helpers
//
//==========================================================================

static void ListEnd(Va_List &tags)
{
	va_end(tags.list);
}

static int ListGetInt(Va_List &tags)
{
	return va_arg(tags.list, int);
}

static inline double ListGetDouble(Va_List &tags)
{
	return va_arg(tags.list, double);
}

static inline FSpecialColormap * ListGetSpecialColormap(Va_List &tags)
{
	return va_arg(tags.list, FSpecialColormap *);
}

static void ListEnd(VMVa_List &tags)
{
}

int ListGetInt(VMVa_List &tags)
{
	if (tags.curindex < tags.numargs)
	{
		if (tags.reginfo[tags.curindex] == REGT_INT)
		{
			return tags.args[tags.curindex++].i;
		}
		ThrowAbortException(X_OTHER, "Invalid parameter in draw function, int expected");
	}
	return TAG_DONE;
}

static inline double ListGetDouble(VMVa_List &tags)
{
	if (tags.curindex < tags.numargs)
	{
		if (tags.reginfo[tags.curindex] == REGT_FLOAT)
		{
			return tags.args[tags.curindex++].f;
		}
		if (tags.reginfo[tags.curindex] == REGT_INT)
		{
			return tags.args[tags.curindex++].i;
		}
		ThrowAbortException(X_OTHER, "Invalid parameter in draw function, float expected");
	}
	return 0;
}

static inline FSpecialColormap * ListGetSpecialColormap(VMVa_List &tags)
{
	ThrowAbortException(X_OTHER, "Invalid tag in draw function");
	return nullptr;
}

//==========================================================================
//
// Main taglist parsing
//
//==========================================================================

template<class T>
bool ParseDrawTextureTags(F2DDrawer *drawer, FGameTexture *img, double x, double y, uint32_t tag, T& tags, DrawParms *parms, bool fortext)
{
	INTBOOL boolval;
	int intval;
	bool fillcolorset = false;

	if (!fortext)
	{
		if (img == NULL || !img->isValid())
		{
			ListEnd(tags);
			return false;
		}
	}

	// Do some sanity checks on the coordinates.
	if (x < -16383 || x > 16383 || y < -16383 || y > 16383)
	{
		ListEnd(tags);
		return false;
	}

	parms->fortext = fortext;
	parms->windowleft = 0;
	parms->windowright = INT_MAX;
	parms->dclip = drawer->GetHeight();
	parms->uclip = 0;
	parms->lclip = 0;
	parms->rclip = drawer->GetWidth();
	parms->left = INT_MAX;
	parms->top = INT_MAX;
	parms->destwidth = INT_MAX;
	parms->destheight = INT_MAX;
	parms->Alpha = 1.f;
	parms->fillcolor = -1;
	parms->TranslationId = -1;
	parms->colorOverlay = 0;
	parms->alphaChannel = false;
	parms->flipX = false;
	parms->flipY = false;
	parms->color = 0xffffffff;
	//parms->shadowAlpha = 0;
	parms->shadowColor = 0;
	parms->virtWidth = INT_MAX;		// these need to match the viewport if not explicitly set, but we do not know that yet.
	parms->virtHeight = INT_MAX;
	parms->keepratio = false;
	parms->style.BlendOp = 255;		// Dummy "not set" value
	parms->masked = true;
	parms->bilinear = false;
	parms->specialcolormap = NULL;
	parms->desaturate = 0;
	parms->cleanmode = DTA_Base;
	parms->scalex = parms->scaley = 1;
	parms->cellx = parms->celly = 0;
	parms->maxstrlen = INT_MAX;
	parms->virtBottom = false;
	parms->srcx = 0.;
	parms->srcy = 0.;
	parms->srcwidth = 1.;
	parms->srcheight = 1.;
	parms->burn = false;
	parms->monospace = EMonospacing::Off;
	parms->spacing = 0;
	parms->fsscalemode = -1;
	parms->patchscalex = parms->patchscaley = 1;
	parms->viewport = { 0,0,drawer->GetWidth(), drawer->GetHeight() };
	parms->rotateangle = 0;
	parms->flipoffsets = false;
	parms->indexed = false;
	parms->nooffset = false;

	// Parse the tag list for attributes. (For floating point attributes,
	// consider that the C ABI dictates that all floats be promoted to
	// doubles when passed as function arguments.)
	while (tag != TAG_DONE)
	{
		switch (tag)
		{
		default:
			ListGetInt(tags);
			break;

		case DTA_DestWidth:
			assert(fortext == false);
			if (fortext) return false;
			parms->cleanmode = DTA_Base;
			parms->destwidth = ListGetInt(tags);
			break;

		case DTA_DestWidthF:
			assert(fortext == false);
			if (fortext) return false;
			parms->cleanmode = DTA_Base;
			parms->destwidth = ListGetDouble(tags);
			break;

		case DTA_DestHeight:
			assert(fortext == false);
			if (fortext) return false;
			parms->cleanmode = DTA_Base;
			parms->destheight = ListGetInt(tags);
			break;

		case DTA_DestHeightF:
			assert(fortext == false);
			if (fortext) return false;
			parms->cleanmode = DTA_Base;
			parms->destheight = ListGetDouble(tags);
			break;

		case DTA_Clean:
		case DTA_CleanTop:
			boolval = ListGetInt(tags);
			if (boolval)
			{
				parms->scalex = 1;
				parms->scaley = 1;
				parms->cleanmode = tag;
			}
			break;

		case DTA_CleanNoMove:
			boolval = ListGetInt(tags);
			if (boolval)
			{
				parms->scalex = CleanXfac;
				parms->scaley = CleanYfac;
				parms->cleanmode = tag;
			}
			break;

		case DTA_CleanNoMove_1:
			boolval = ListGetInt(tags);
			if (boolval)
			{
				parms->scalex = CleanXfac_1;
				parms->scaley = CleanYfac_1;
				parms->cleanmode = tag;
			}
			break;

		case DTA_320x200:
			boolval = ListGetInt(tags);
			if (boolval)
			{
				parms->cleanmode = DTA_Base;
				parms->scalex = 1;
				parms->scaley = 1;
				parms->virtWidth = 320;
				parms->virtHeight = 200;
			}
			break;

		case DTA_Bottom320x200:
			boolval = ListGetInt(tags);
			if (boolval)
			{
				parms->cleanmode = DTA_Base;
				parms->scalex = 1;
				parms->scaley = 1;
				parms->virtWidth = 320;
				parms->virtHeight = 200;
			}
			parms->virtBottom = true;
			break;

		case DTA_HUDRules:
			intval = ListGetInt(tags);
			parms->cleanmode = intval == HUD_HorizCenter ? DTA_HUDRulesC : DTA_HUDRules;
			break;

		case DTA_VirtualWidth:
			parms->cleanmode = DTA_Base;
			parms->virtWidth = ListGetInt(tags);
			break;

		case DTA_VirtualWidthF:
			parms->cleanmode = DTA_Base;
			parms->virtWidth = ListGetDouble(tags);
			break;

		case DTA_VirtualHeight:
			parms->cleanmode = DTA_Base;
			parms->virtHeight = ListGetInt(tags);
			break;

		case DTA_VirtualHeightF:
			parms->cleanmode = DTA_Base;
			parms->virtHeight = ListGetDouble(tags);
			break;

		case DTA_FullscreenScale:
			intval = ListGetInt(tags);
			if (intval >= FSMode_None && intval < FSMode_Max)
			{
				parms->fsscalemode = (int8_t)intval;
			}
			else if (intval >= FSMode_Predefined && intval < FSMode_Predefined_Max)
			{
				static const uint8_t modes[] = { FSMode_ScaleToFit43, FSMode_ScaleToFit43, FSMode_ScaleToFit43, FSMode_ScaleToFit43, FSMode_ScaleToFit43Top};
				static const uint16_t widths[] = { 320, 320, 640, 640, 320};
				static const uint16_t heights[] = { 200, 240, 400, 480, 200};
				parms->fsscalemode = modes[intval - FSMode_Predefined];
				parms->virtWidth = widths[intval - FSMode_Predefined];
				parms->virtHeight = heights[intval - FSMode_Predefined];
			}
			break;

		case DTA_Fullscreen:

			boolval = ListGetInt(tags);
			if (boolval)
			{
				assert(fortext == false);
				if (img == NULL) return false;
				parms->cleanmode = DTA_Fullscreen;
				parms->fsscalemode = (uint8_t)twod->fullscreenautoaspect;
				parms->virtWidth = img->GetDisplayWidth();
				parms->virtHeight = img->GetDisplayHeight();
			}
			break;

		case DTA_FullscreenEx:

			intval = ListGetInt(tags);
			if (intval >= 0 && intval <= 3)
			{
				assert(fortext == false);
				if (img == NULL) return false;
				parms->cleanmode = DTA_Fullscreen;
				parms->fsscalemode = (uint8_t)intval;
				parms->virtWidth = img->GetDisplayWidth();
				parms->virtHeight = img->GetDisplayHeight();
			}
			break;

		case DTA_Alpha:
			parms->Alpha = (float)(min<double>(1., ListGetDouble(tags)));
			break;

		case DTA_AlphaChannel:
			parms->alphaChannel = ListGetInt(tags);
			break;

		case DTA_FillColor:
			parms->fillcolor = ListGetInt(tags);
			if (parms->fillcolor != ~0u)
			{
				fillcolorset = true;
			}
			else if (parms->fillcolor != 0)
			{
				// The crosshair is the only thing which uses a non-black fill color.
				parms->fillcolor = PalEntry(ColorMatcher.Pick(parms->fillcolor), RPART(parms->fillcolor), GPART(parms->fillcolor), BPART(parms->fillcolor));
			}
			break;

		case DTA_TranslationIndex:
			parms->TranslationId = ListGetInt(tags);
			break;

		case DTA_ColorOverlay:
			parms->colorOverlay = ListGetInt(tags);
			break;

		case DTA_Color:
			parms->color = ListGetInt(tags);
			break;

		case DTA_FlipX:
			parms->flipX = ListGetInt(tags);
			break;

		case DTA_FlipY:
			parms->flipY = ListGetInt(tags);
			break;

		case DTA_FlipOffsets:
			parms->flipoffsets = ListGetInt(tags);
			break;

		case DTA_NoOffset:
			parms->nooffset = ListGetInt(tags);
			break;

		case DTA_SrcX:
			parms->srcx = ListGetDouble(tags) / img->GetDisplayWidth();
			break;

		case DTA_SrcY:
			parms->srcy = ListGetDouble(tags) / img->GetDisplayHeight();
			break;

		case DTA_SrcWidth:
			parms->srcwidth = ListGetDouble(tags) / img->GetDisplayWidth();
			break;

		case DTA_SrcHeight:
			parms->srcheight = ListGetDouble(tags) / img->GetDisplayHeight();
			break;

		case DTA_TopOffset:
			assert(fortext == false);
			if (fortext) return false;
			parms->top = ListGetInt(tags);
			break;

		case DTA_TopOffsetF:
			assert(fortext == false);
			if (fortext) return false;
			parms->top = ListGetDouble(tags);
			break;

		case DTA_LeftOffset:
			assert(fortext == false);
			if (fortext) return false;
			parms->left = ListGetInt(tags);
			break;

		case DTA_LeftOffsetF:
			assert(fortext == false);
			if (fortext) return false;
			parms->left = ListGetDouble(tags);
			break;

		case DTA_TopLeft:
			assert(fortext == false);
			if (fortext) return false;
			if (ListGetInt(tags))
			{
				parms->left = 0;
				parms->top = 0;
			}
			break;

		case DTA_CenterOffset:
			assert(fortext == false);
			if (fortext) return false;
			if (ListGetInt(tags))
			{
				parms->left = img->GetDisplayWidth() * 0.5;
				parms->top = img->GetDisplayHeight() * 0.5;
			}
			break;

		case DTA_CenterOffsetRel:
			assert(fortext == false);
			if (fortext) return false;
			intval = ListGetInt(tags);
			if (intval == 1)
			{
				parms->left = img->GetDisplayLeftOffset() + (img->GetDisplayWidth() * 0.5);
				parms->top = img->GetDisplayTopOffset() + (img->GetDisplayHeight() * 0.5);
			}
			else if (intval == 2)
			{
				parms->left = img->GetDisplayLeftOffset() + floor(img->GetDisplayWidth() * 0.5);
				parms->top = img->GetDisplayTopOffset() + floor(img->GetDisplayHeight() * 0.5);
			}
			break;

		case DTA_CenterBottomOffset:
			assert(fortext == false);
			if (fortext) return false;
			if (ListGetInt(tags))
			{
				parms->left = img->GetDisplayWidth() * 0.5;
				parms->top = img->GetDisplayHeight();
			}
			break;

		case DTA_WindowLeft:
			assert(fortext == false);
			if (fortext) return false;
			parms->windowleft = ListGetInt(tags);
			break;

		case DTA_WindowLeftF:
			assert(fortext == false);
			if (fortext) return false;
			parms->windowleft = ListGetDouble(tags);
			break;

		case DTA_WindowRight:
			assert(fortext == false);
			if (fortext) return false;
			parms->windowright = ListGetInt(tags);
			break;

		case DTA_WindowRightF:
			assert(fortext == false);
			if (fortext) return false;
			parms->windowright = ListGetDouble(tags);
			break;

		case DTA_ClipTop:
			parms->uclip = ListGetInt(tags);
			if (parms->uclip < 0)
			{
				parms->uclip = 0;
			}
			break;

		case DTA_ClipBottom:
			parms->dclip = ListGetInt(tags);
			if (parms->dclip > drawer->GetHeight())
			{
				parms->dclip = drawer->GetHeight();
			}
			break;

		case DTA_ClipLeft:
			parms->lclip = ListGetInt(tags);
			if (parms->lclip < 0)
			{
				parms->lclip = 0;
			}
			break;

		case DTA_ClipRight:
			parms->rclip = ListGetInt(tags);
			if (parms->rclip > drawer->GetWidth())
			{
				parms->rclip = drawer->GetWidth();
			}
			break;

		case DTA_ShadowAlpha:
			//parms->shadowAlpha = (float)min(1., ListGetDouble(tags));
			break;

		case DTA_ShadowColor:
			parms->shadowColor = ListGetInt(tags);
			break;

		case DTA_Shadow:
			boolval = ListGetInt(tags);
			if (boolval)
			{
				//parms->shadowAlpha = 0.5;
				parms->shadowColor = 0;
			}
			else
			{
				//parms->shadowAlpha = 0;
			}
			break;

		case DTA_ScaleX:
			parms->patchscalex = ListGetDouble(tags);
			break;

		case DTA_ScaleY:
			parms->patchscaley = ListGetDouble(tags);
			break;

		case DTA_Masked:
			parms->masked = ListGetInt(tags);
			break;

		case DTA_BilinearFilter:
			parms->bilinear = ListGetInt(tags);
			break;

		case DTA_KeepRatio:
			parms->keepratio = ListGetInt(tags) ? -1 : 0;
			break;

		case DTA_Pin:
			parms->keepratio = ListGetInt(tags);
			break;

		case DTA_RenderStyle:
			parms->style.AsDWORD = ListGetInt(tags);
			break;

		case DTA_LegacyRenderStyle:	// mainly for ZScript which does not handle FRenderStyle that well.
			parms->style = (ERenderStyle)ListGetInt(tags);
			break;

		case DTA_SpecialColormap:
			parms->specialcolormap = ListGetSpecialColormap(tags);
			break;

		case DTA_Desaturate:
			parms->desaturate = ListGetInt(tags);
			break;

		case DTA_TextLen:
			parms->maxstrlen = ListGetInt(tags);
			break;

		case DTA_CellX:
			parms->cellx = ListGetInt(tags);
			break;

		case DTA_CellY:
			parms->celly = ListGetInt(tags);
			break;

		case DTA_Monospace:
			parms->monospace = ListGetInt(tags);
			break;

		case DTA_Spacing:
			parms->spacing = ListGetInt(tags);
			break;

		case DTA_Burn:
			parms->burn = true;
			break;

		case DTA_ViewportX:
			parms->viewport.left = ListGetInt(tags);
			break;

		case DTA_ViewportY:
			parms->viewport.top = ListGetInt(tags);
			break;

		case DTA_ViewportWidth:
			parms->viewport.width = ListGetInt(tags);
			break;

		case DTA_ViewportHeight:
			parms->viewport.height = ListGetInt(tags);
			break;

		case DTA_Rotate:
			assert(fortext == false);
			if (fortext) return false;
			parms->rotateangle = ListGetDouble(tags);
			break;

		case DTA_Indexed:
			parms->indexed = !!ListGetInt(tags);
			break;
		}
		tag = ListGetInt(tags);
	}
	ListEnd(tags);

	if (parms->virtWidth == INT_MAX) parms->virtWidth = parms->viewport.width;
	if (parms->virtHeight == INT_MAX) parms->virtHeight = parms->viewport.height;

	auto clipleft = drawer->clipleft;
	auto cliptop = drawer->cliptop;
	auto clipwidth = drawer->clipwidth;
	auto clipheight = drawer->clipheight;
	// intersect with the canvas's clipping rectangle.
	if (clipwidth >= 0 && clipheight >= 0)
	{
		if (parms->lclip < clipleft) parms->lclip = clipleft;
		if (parms->rclip > clipleft + clipwidth) parms->rclip = clipleft + clipwidth;
		if (parms->uclip < cliptop) parms->uclip = cliptop;
		if (parms->dclip > cliptop + clipheight) parms->dclip = cliptop + clipheight;
	}

	if (parms->uclip >= parms->dclip || parms->lclip >= parms->rclip)
	{
		return false;
	}

	if (img != NULL)
	{
		SetTextureParms(drawer, parms, img, x, y);

		if (parms->destwidth <= 0 || parms->destheight <= 0)
		{
			return false;
		}
	}

	if (parms->style.BlendOp == 255)
	{
		if (fillcolorset)
		{
			if (parms->alphaChannel)
			{
				parms->style = STYLE_Shaded;
			}
			else if (parms->Alpha < 1.f)
			{
				parms->style = STYLE_TranslucentStencil;
			}
			else
			{
				parms->style = STYLE_Stencil;
			}
		}
		else if (parms->Alpha < 1.f)
		{
			parms->style = STYLE_Translucent;
		}
		else
		{
			parms->style = STYLE_Normal;
		}
	}
	return true;
}
// explicitly instantiate both versions for v_text.cpp.

template bool ParseDrawTextureTags<Va_List>(F2DDrawer* drawer, FGameTexture *img, double x, double y, uint32_t tag, Va_List& tags, DrawParms *parms, bool fortext);
template bool ParseDrawTextureTags<VMVa_List>(F2DDrawer* drawer, FGameTexture *img, double x, double y, uint32_t tag, VMVa_List& tags, DrawParms *parms, bool fortext);

//==========================================================================
//
// Coordinate conversion
//
//==========================================================================

static void VirtualToRealCoords(F2DDrawer *drawer, double Width, double Height, double &x, double &y, double &w, double &h,
	double vwidth, double vheight, bool vbottom, bool handleaspect) 
{
	float myratio = float(handleaspect ? ActiveRatio (Width, Height) : (4.0 / 3.0));

	// if 21:9 AR, map to 16:9 for all callers.
	// this allows for black bars and stops the stretching of fullscreen images

	switch (vid_allowtrueultrawide)
	{
	case 1:
	default:
		myratio = min(64.0f / 27.0f, myratio);
		break;
	case 0:
		myratio = min(16.0f / 9.0f, myratio);
	case -1:
		break;
	}

	double right = x + w;
	double bottom = y + h;

	if (myratio > 1.334f)
	{ // The target surface is either 16:9 or 16:10, so expand the
	  // specified virtual size to avoid undesired stretching of the
	  // image. Does not handle non-4:3 virtual sizes. I'll worry about
	  // those if somebody expresses a desire to use them.
		x = (x - vwidth * 0.5) * Width * 960 / (vwidth * AspectBaseWidth(myratio)) + Width * 0.5;
		w = (right - vwidth * 0.5) * Width * 960 / (vwidth * AspectBaseWidth(myratio)) + Width * 0.5 - x;
	}
	else
	{
		x = x * Width / vwidth;
		w = right * Width / vwidth - x;
	}
	if (AspectTallerThanWide(myratio))
	{ // The target surface is 5:4
		y = (y - vheight * 0.5) * Height * 600 / (vheight * AspectBaseHeight(myratio)) + Height * 0.5;
		h = (bottom - vheight * 0.5) * Height * 600 / (vheight * AspectBaseHeight(myratio)) + Height * 0.5 - y;
		if (vbottom)
		{
			y += (Height - Height * AspectMultiplier(myratio) / 48.0) * 0.5;
		}
	}
	else
	{
		y = y * Height / vheight;
		h = bottom * Height / vheight - y;
	}
}

void VirtualToRealCoords(F2DDrawer* drawer, double& x, double& y, double& w, double& h,
	double vwidth, double vheight, bool vbottom, bool handleaspect)
{
	auto Width = drawer->GetWidth();
	auto Height = drawer->GetHeight();
	VirtualToRealCoords(drawer, Width, Height, x, y, w, h, vwidth, vheight, vbottom, handleaspect);
}

DEFINE_ACTION_FUNCTION(_Screen, VirtualToRealCoords)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_FLOAT(vw);
	PARAM_FLOAT(vh);
	PARAM_BOOL(vbottom);
	PARAM_BOOL(handleaspect);
	VirtualToRealCoords(twod, x, y, w, h, vw, vh, vbottom, handleaspect);
	if (numret >= 1) ret[0].SetVector2(DVector2(x, y));
	if (numret >= 2) ret[1].SetVector2(DVector2(w, h));
	return min(numret, 2);
}

void VirtualToRealCoordsInt(F2DDrawer *drawer, int &x, int &y, int &w, int &h,
	int vwidth, int vheight, bool vbottom, bool handleaspect)
{
	double dx, dy, dw, dh;

	dx = x;
	dy = y;
	dw = w;
	dh = h;
	VirtualToRealCoords(drawer, dx, dy, dw, dh, vwidth, vheight, vbottom, handleaspect);
	x = int(dx + 0.5);
	y = int(dy + 0.5);
	w = int(dx + dw + 0.5) - x;
	h = int(dy + dh + 0.5) - y;
}

//==========================================================================
//
// Draw a line
//
//==========================================================================

static void DrawLine(int x0, int y0, int x1, int y1, uint32_t realcolor, int alpha)
{
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	twod->AddLine((float)x0, (float)y0, (float)x1, (float)y1, -1, -1, INT_MAX, INT_MAX, realcolor | MAKEARGB(255, 0, 0, 0), alpha);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Screen, DrawLine, DrawLine)
{
	PARAM_PROLOGUE;
	PARAM_INT(x0);
	PARAM_INT(y0);
	PARAM_INT(x1);
	PARAM_INT(y1);
	PARAM_INT(color);
	PARAM_INT(alpha);
	DrawLine(x0, y0, x1, y1, color, alpha);
	return 0;
}

static void DrawThickLine(int x0, int y0, int x1, int y1, double thickness, uint32_t realcolor, int alpha) 
{
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	twod->AddThickLine(x0, y0, x1, y1, thickness, realcolor, alpha);
}

DEFINE_ACTION_FUNCTION_NATIVE(_Screen, DrawThickLine, DrawThickLine)
{
	PARAM_PROLOGUE;
	PARAM_INT(x0);
	PARAM_INT(y0);
	PARAM_INT(x1);
	PARAM_INT(y1);
	PARAM_FLOAT(thickness);
	PARAM_INT(color);
	PARAM_INT(alpha);
	DrawThickLine(x0, y0, x1, y1, thickness, color, alpha);
	return 0;
}

//==========================================================================
//
// ClearRect
//
// Set an area to a specified color.
//
//==========================================================================

void ClearRect(F2DDrawer *drawer, int left, int top, int right, int bottom, int palcolor, uint32_t color)
{
	auto clipleft = drawer->clipleft;
	auto cliptop = drawer->cliptop;
	auto clipwidth = drawer->clipwidth;
	auto clipheight = drawer->clipheight;

	if (clipwidth >= 0 && clipheight >= 0)
	{
		int w = right - left;
		int h = bottom - top;
		if (left < clipleft)
		{
			w -= (clipleft - left);
			left = clipleft;
		}
		if (w > clipwidth) w = clipwidth;
		if (w <= 0) return;

		if (top < cliptop)
		{
			h -= (cliptop - top);
			top = cliptop;
		}
		if (h > clipheight) w = clipheight;
		if (h <= 0) return;
		right = left + w;
		bottom = top + h;
	}

	if (palcolor >= 0 && color == 0)
	{
		color = GPalette.BaseColors[palcolor] | 0xff000000;
	}
	drawer->AddColorOnlyQuad(left, top, right - left, bottom - top, color | 0xFF000000, nullptr);
}

DEFINE_ACTION_FUNCTION(_Screen, Clear)
{
	PARAM_PROLOGUE;
	PARAM_INT(x1);
	PARAM_INT(y1);
	PARAM_INT(x2);
	PARAM_INT(y2);
	PARAM_INT(color);
	PARAM_INT(palcol);
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	ClearRect(twod, x1, y1, x2, y2, palcol, color);
	return 0;
}

//==========================================================================
//
// DoDim
//
// Applies a colored overlay to an area of the screen.
//
//==========================================================================

void DoDim(F2DDrawer *drawer, PalEntry color, float amount, int x1, int y1, int w, int h, FRenderStyle *style)
{
	if (amount <= 0)
	{
		return;
	}
	if (amount > 1)
	{
		amount = 1;
	}
	drawer->AddColorOnlyQuad(x1, y1, w, h, (color.d & 0xffffff) | (int(amount * 255) << 24), style);
}

void Dim(F2DDrawer *drawer, PalEntry color, float damount, int x1, int y1, int w, int h, FRenderStyle *style)
{
	auto clipleft = drawer->clipleft;
	auto cliptop = drawer->cliptop;
	auto clipwidth = drawer->clipwidth;
	auto clipheight = drawer->clipheight;

	if (clipwidth >= 0 && clipheight >= 0)
	{
		if (x1 < clipleft)
		{
			w -= (clipleft - x1);
			x1 = clipleft;
		}
		if (w > clipwidth) w = clipwidth;
		if (w <= 0) return;

		if (y1 < cliptop)
		{
			h -= (cliptop - y1);
			y1 = cliptop;
		}
		if (h > clipheight) h = clipheight;
		if (h <= 0) return;
	}
	DoDim(drawer, color, damount, x1, y1, w, h, style);
}

DEFINE_ACTION_FUNCTION(_Screen, Dim)
{
	PARAM_PROLOGUE;
	PARAM_INT(color);
	PARAM_FLOAT(amount);
	PARAM_INT(x1);
	PARAM_INT(y1);
	PARAM_INT(w);
	PARAM_INT(h);
	PARAM_INT(style);
	if (!twod->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	Dim(twod, color, float(amount), x1, y1, w, h, &LegacyRenderStyles[style]);
	return 0;
}


//==========================================================================
//
// screen->DrawBorder
//
//==========================================================================

void DrawBorder (F2DDrawer *drawer, FTextureID picnum, int x1, int y1, int x2, int y2)
{
	int filltype = (ui_screenborder_classic_scaling) ? -1 : 0;
	if (picnum.isValid())
	{
		drawer->AddFlatFill (x1, y1, x2, y2, TexMan.GetGameTexture(picnum, false), filltype);
	}
	else
	{
		ClearRect(drawer, x1, y1, x2, y2, 0, 0);
	}
}

//==========================================================================
//
// V_DrawFrame
//
// Draw a frame around the specified area using the view border
// frame graphics. The border is drawn outside the area, not in it.
//
//==========================================================================

void DrawFrame(F2DDrawer* twod, PalEntry color, int left, int top, int width, int height, int thickness)
{
	// Sanity check for incomplete gameinfo
	int offset = thickness == -1 ? twod->GetHeight() / 400 : thickness;
	int right = left + width;
	int bottom = top + height;

	// Draw top and bottom sides.
	twod->AddColorOnlyQuad(left, top - offset, width, offset, color);
	twod->AddColorOnlyQuad(left - offset, top - offset, offset, height + 2 * offset, color);
	twod->AddColorOnlyQuad(left, bottom, width, offset, color);
	twod->AddColorOnlyQuad(right, top - offset, offset, height + 2 * offset, color);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawLineFrame)
{
	PARAM_PROLOGUE;
	PARAM_COLOR(color);
	PARAM_INT(left);
	PARAM_INT(top);
	PARAM_INT(width);
	PARAM_INT(height);
	PARAM_INT(thickness);
	DrawFrame(twod, color, left, top, width, height, thickness);
	return 0;
}

void V_CalcCleanFacs(int designwidth, int designheight, int realwidth, int realheight, int* cleanx, int* cleany, int* _cx1, int* _cx2)
{
	if (designheight < 240 && realheight >= 480) designheight = 240;
	*cleanx = *cleany = min(realwidth / designwidth, realheight / designheight);
}


DEFINE_ACTION_FUNCTION(_Screen, SetOffset)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	ACTION_RETURN_VEC2(twod->SetOffset(DVector2(x, y)));
}