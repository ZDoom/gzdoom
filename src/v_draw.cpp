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

// #define NO_SWRENDER 	// set this if you want to exclude the software renderer. Without the software renderer software canvas drawing does nothing.

#include <stdio.h>
#include <stdarg.h>

#include "doomtype.h"
#include "v_video.h"
#include "m_swap.h"
#include "r_defs.h"
#include "r_utility.h"
#include "r_renderer.h"
#ifndef NO_SWRENDER
#include "swrenderer/r_swcanvas.h"
#endif
#include "r_data/r_translate.h"
#include "doomstat.h"
#include "v_palette.h"
#include "gi.h"
#include "g_level.h"
#include "st_stuff.h"
#include "sbar.h"

#include "i_system.h"
#include "i_video.h"
#include "templates.h"
#include "d_net.h"
#include "colormatcher.h"
#include "r_data/colormaps.h"
#include "g_levellocals.h"
#include "textures.h"
#include "vm.h"

CUSTOM_CVAR(Int, uiscale, 0, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = 0;
		return;
	}
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
	setsizeneeded = true;
}

int GetUIScale(int altval)
{
	int scaleval;
	if (altval > 0) scaleval = altval;
	else if (uiscale == 0)
	{
		// Default should try to scale to 640x400
		int vscale = screen->GetHeight() / 400;
		int hscale = screen->GetWidth() / 640;
		scaleval = clamp(vscale, 1, hscale);
	}
	else scaleval = uiscale;

	// block scales that result in something larger than the current screen.
	int vmax = screen->GetHeight() / 200;
	int hmax = screen->GetWidth() / 320;
	int max = MAX(vmax, hmax);
	return MAX(1,MIN(scaleval, max));
}

// [RH] Stretch values to make a 320x200 image best fit the screen
// without using fractional steppings
int CleanXfac, CleanYfac;

// [RH] Effective screen sizes that the above scale values give you
int CleanWidth, CleanHeight;

// Above minus 1 (or 1, if they are already 1)
int CleanXfac_1, CleanYfac_1, CleanWidth_1, CleanHeight_1;


DEFINE_ACTION_FUNCTION(_Screen, GetWidth)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(screen->GetWidth());
}

DEFINE_ACTION_FUNCTION(_Screen, GetHeight)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(screen->GetHeight());
}

DEFINE_ACTION_FUNCTION(_Screen, PaletteColor)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	if (index < 0 || index > 255) index = 0;
	else index = GPalette.BaseColors[index];
	ACTION_RETURN_INT(index);
}

void DCanvas::DrawTexture (FTexture *img, double x, double y, int tags_first, ...)
{
	Va_List tags;
	va_start(tags.list, tags_first);
	DrawParms parms;

	bool res = ParseDrawTextureTags(img, x, y, tags_first, tags, &parms, false);
	va_end(tags.list);
	if (!res)
	{
		return;
	}
	DrawTextureParms(img, parms);
}

int ListGetInt(VMVa_List &tags);

void DCanvas::DrawTexture(FTexture *img, double x, double y, VMVa_List &args)
{
	DrawParms parms;
	uint32_t tag = ListGetInt(args);
	bool res = ParseDrawTextureTags(img, x, y, tag, args, &parms, false);
	if (!res) return;
	DrawTextureParms(img, parms);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawTexture)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	PARAM_BOOL(animate);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);

	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");

	FTexture *tex = animate ? TexMan(FSetTextureID(texid)) : TexMan[FSetTextureID(texid)];
	VMVa_List args = { param + 4, 0, numparam - 4 };
	screen->DrawTexture(tex, x, y, args);
	return 0;
}

void DCanvas::DrawTextureParms(FTexture *img, DrawParms &parms)
{
#ifndef NO_SWRENDER
	SWCanvas::DrawTexture(this, img, parms);
#endif

	if (ticdup != 0 && menuactive == MENU_Off)
	{
		NetUpdate();
	}
}

void DCanvas::SetClipRect(int x, int y, int w, int h)
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
	screen->SetClipRect(x, y, w, h);
	return 0;
}

DEFINE_ACTION_FUNCTION(_Screen, ClearClipRect)
{
	PARAM_PROLOGUE;
	screen->ClearClipRect();
	return 0;
}

void DCanvas::GetClipRect(int *x, int *y, int *w, int *h)
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
	screen->GetClipRect(&x, &y, &w, &h);
	if (numret > 0) ret[0].SetInt(x);
	if (numret > 1) ret[1].SetInt(y);
	if (numret > 2) ret[2].SetInt(w);
	if (numret > 3) ret[3].SetInt(h);
	return MIN(numret, 4);
}

DEFINE_ACTION_FUNCTION(_Screen, GetViewWindow)
{
	PARAM_PROLOGUE;
	if (numret > 0) ret[0].SetInt(viewwindowx);
	if (numret > 1) ret[1].SetInt(viewwindowy);
	if (numret > 2) ret[2].SetInt(viewwidth);
	if (numret > 3) ret[3].SetInt(viewheight);
	return MIN(numret, 4);
}

bool DCanvas::SetTextureParms(DrawParms *parms, FTexture *img, double xx, double yy) const
{
	if (img != NULL)
	{
		parms->x = xx;
		parms->y = yy;
		parms->texwidth = img->GetScaledWidthDouble();
		parms->texheight = img->GetScaledHeightDouble();
		if (parms->top == INT_MAX || parms->fortext)
		{
			parms->top = img->GetScaledTopOffset();
		}
		if (parms->left == INT_MAX || parms->fortext)
		{
			parms->left = img->GetScaledLeftOffset();
		}
		if (parms->destwidth == INT_MAX || parms->fortext)
		{
			parms->destwidth = img->GetScaledWidthDouble();
		}
		if (parms->destheight == INT_MAX || parms->fortext)
		{
			parms->destheight = img->GetScaledHeightDouble();
		}

		switch (parms->cleanmode)
		{
		default:
			break;

		case DTA_Clean:
			parms->x = (parms->x - 160.0) * CleanXfac + (Width * 0.5);
			parms->y = (parms->y - 100.0) * CleanYfac + (Height * 0.5);
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

		case DTA_Fullscreen:
			parms->x = parms->y = 0;
			break;

		case DTA_HUDRules:
		case DTA_HUDRulesC:
		{
			// Note that this has been deprecated because the HUD should be drawn by the status bar.
			bool xright = parms->x < 0;
			bool ybot = parms->y < 0;
			DVector2 scale = StatusBar->GetHUDScale();

			parms->x *= scale.X;
			if (parms->cleanmode == DTA_HUDRulesC)
				parms->x += Width * 0.5;
			else if (xright)
				parms->x = Width + parms->x;
			parms->y *= scale.Y;
			if (ybot)
				parms->y = Height + parms->y;
			parms->destwidth = parms->texwidth * scale.X;
			parms->destheight = parms->texheight * scale.Y;
			break;
		}
		}
		if (parms->virtWidth != Width || parms->virtHeight != Height)
		{
			VirtualToRealCoords(parms->x, parms->y, parms->destwidth, parms->destheight,
				parms->virtWidth, parms->virtHeight, parms->virtBottom, !parms->keepratio);
		}
	}

	return false;
}

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

// These two options are only being used by the D3D version of the HUD weapon drawer, they serve no purpose anywhere else.
static inline FSpecialColormap * ListGetSpecialColormap(Va_List &tags)
{
	return va_arg(tags.list, FSpecialColormap *);
}

static inline FColormapStyle * ListGetColormapStyle(Va_List &tags)
{
	return va_arg(tags.list, FColormapStyle *);
}

static void ListEnd(VMVa_List &tags)
{
}

int ListGetInt(VMVa_List &tags)
{
	if (tags.curindex < tags.numargs)
	{
		if (tags.args[tags.curindex].Type == REGT_INT)
		{
			return tags.args[tags.curindex++].i;
		}
		ThrowAbortException(X_OTHER, "Invalid parameter in draw function, int expected");
	}
	return TAG_DONE;
}

static inline double ListGetDouble(VMVa_List &tags)
{
	if (tags.curindex < tags.numargs && tags.args[tags.curindex].Type == REGT_FLOAT)
	{
		return tags.args[tags.curindex++].f;
	}
	ThrowAbortException(X_OTHER, "Invalid parameter in draw function, float expected");
	return 0;
}

static inline FSpecialColormap * ListGetSpecialColormap(VMVa_List &tags)
{
	ThrowAbortException(X_OTHER, "Invalid tag in draw function");
	return nullptr;
}

static inline FColormapStyle * ListGetColormapStyle(VMVa_List &tags)
{
	ThrowAbortException(X_OTHER, "Invalid tag in draw function");
	return nullptr;
}

template<class T>
bool DCanvas::ParseDrawTextureTags(FTexture *img, double x, double y, uint32_t tag, T& tags, DrawParms *parms, bool fortext) const
{
	INTBOOL boolval;
	int intval;
	bool translationset = false;
	bool fillcolorset = false;

	if (!fortext)
	{
		if (img == NULL || img->UseType == FTexture::TEX_Null)
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
	parms->dclip = this->GetHeight();
	parms->uclip = 0;
	parms->lclip = 0;
	parms->rclip = this->GetWidth();
	parms->left = INT_MAX;
	parms->top = INT_MAX;
	parms->destwidth = INT_MAX;
	parms->destheight = INT_MAX;
	parms->Alpha = 1.f;
	parms->fillcolor = -1;
	parms->remap = NULL;
	parms->colorOverlay = 0;
	parms->alphaChannel = false;
	parms->flipX = false;
	parms->color = 0xffffffff;
	//parms->shadowAlpha = 0;
	parms->shadowColor = 0;
	parms->virtWidth = this->GetWidth();
	parms->virtHeight = this->GetHeight();
	parms->keepratio = false;
	parms->style.BlendOp = 255;		// Dummy "not set" value
	parms->masked = true;
	parms->bilinear = false;
	parms->specialcolormap = NULL;
	parms->colormapstyle = NULL;
	parms->cleanmode = DTA_Base;
	parms->scalex = parms->scaley = 1;
	parms->cellx = parms->celly = 0;
	parms->maxstrlen = INT_MAX;
	parms->virtBottom = false;

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

		case DTA_Fullscreen:
			boolval = ListGetInt(tags);
			if (boolval)
			{
				assert(fortext == false);
				if (img == NULL) return false;
				parms->cleanmode = DTA_Fullscreen;
				parms->virtWidth = img->GetScaledWidthDouble();
				parms->virtHeight = img->GetScaledHeightDouble();
			}
			break;

		case DTA_Alpha:
			parms->Alpha = (float)(MIN<double>(1., ListGetDouble(tags)));
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
			parms->remap = TranslationToTable(ListGetInt(tags));
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

		case DTA_CenterOffset:
			assert(fortext == false);
			if (fortext) return false;
			if (ListGetInt(tags))
			{
				parms->left = img->GetScaledWidthDouble() * 0.5;
				parms->top = img->GetScaledHeightDouble() * 0.5;
			}
			break;

		case DTA_CenterBottomOffset:
			assert(fortext == false);
			if (fortext) return false;
			if (ListGetInt(tags))
			{
				parms->left = img->GetScaledWidthDouble() * 0.5;
				parms->top = img->GetScaledHeightDouble();
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
			if (parms->dclip > this->GetHeight())
			{
				parms->dclip = this->GetHeight();
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
			if (parms->rclip > this->GetWidth())
			{
				parms->rclip = this->GetWidth();
			}
			break;

		case DTA_ShadowAlpha:
			//parms->shadowAlpha = (float)MIN(1., ListGetDouble(tags));
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

		case DTA_Masked:
			parms->masked = ListGetInt(tags);
			break;

		case DTA_BilinearFilter:
			parms->bilinear = ListGetInt(tags);
			break;

		case DTA_KeepRatio:
			// I think this is a terribly misleading name, since it actually turns
			// *off* aspect ratio correction.
			parms->keepratio = ListGetInt(tags);
			break;

		case DTA_RenderStyle:
			parms->style.AsDWORD = ListGetInt(tags);
			break;

		case DTA_SpecialColormap:
			parms->specialcolormap = ListGetSpecialColormap(tags);
			break;

		case DTA_ColormapStyle:
			parms->colormapstyle = ListGetColormapStyle(tags);
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

		}
		tag = ListGetInt(tags);
	}
	ListEnd(tags);

	if (parms->remap != nullptr && parms->remap->Inactive)
	{ // If it's inactive, pretend we were passed NULL instead.
		parms->remap = nullptr;
	}

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
		SetTextureParms(parms, img, x, y);

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

template bool DCanvas::ParseDrawTextureTags<Va_List>(FTexture *img, double x, double y, uint32_t tag, Va_List& tags, DrawParms *parms, bool fortext) const;
template bool DCanvas::ParseDrawTextureTags<VMVa_List>(FTexture *img, double x, double y, uint32_t tag, VMVa_List& tags, DrawParms *parms, bool fortext) const;

void DCanvas::VirtualToRealCoords(double &x, double &y, double &w, double &h,
	double vwidth, double vheight, bool vbottom, bool handleaspect) const
{
	float myratio = handleaspect ? ActiveRatio (Width, Height) : (4.0f / 3.0f);

    // if 21:9 AR, map to 16:9 for all callers.
    // this allows for black bars and stops the stretching of fullscreen images
    if (myratio > 1.7f) {
        myratio = 16.0f / 9.0f;
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

DEFINE_ACTION_FUNCTION(_Screen, VirtualToRealCoords)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_FLOAT(vw);
	PARAM_FLOAT(vh);
	PARAM_BOOL_DEF(vbottom);
	PARAM_BOOL_DEF(handleaspect);
	screen->VirtualToRealCoords(x, y, w, h, vw, vh, vbottom, handleaspect);
	if (numret >= 1) ret[0].SetVector2(DVector2(x, y));
	if (numret >= 2) ret[1].SetVector2(DVector2(w, h));
	return MIN(numret, 2);
}

void DCanvas::VirtualToRealCoordsInt(int &x, int &y, int &w, int &h,
	int vwidth, int vheight, bool vbottom, bool handleaspect) const
{
	double dx, dy, dw, dh;

	dx = x;
	dy = y;
	dw = w;
	dh = h;
	VirtualToRealCoords(dx, dy, dw, dh, vwidth, vheight, vbottom, handleaspect);
	x = int(dx + 0.5);
	y = int(dy + 0.5);
	w = int(dx + dw + 0.5) - x;
	h = int(dy + dh + 0.5) - y;
}

void DCanvas::FillBorder (FTexture *img)
{
	float myratio = ActiveRatio (Width, Height);

    // if 21:9 AR, fill borders akin to 16:9, since all fullscreen
    // images are being drawn to that scale.
    if (myratio > 1.7f) {
        myratio = 16 / 9.0f;
    }

	if (myratio >= 1.3f && myratio <= 1.4f)
	{ // This is a 4:3 display, so no border to show
		return;
	}
	int bordtop, bordbottom, bordleft, bordright, bord;
	if (AspectTallerThanWide(myratio))
	{ // Screen is taller than it is wide
		bordleft = bordright = 0;
		bord = Height - Height * AspectMultiplier(myratio) / 48;
		bordtop = bord / 2;
		bordbottom = bord - bordtop;
	}
	else
	{ // Screen is wider than it is tall
		bordtop = bordbottom = 0;
		bord = Width - Width * AspectMultiplier(myratio) / 48;
		bordleft = bord / 2;
		bordright = bord - bordleft;
	}

	if (img != NULL)
	{
		FlatFill (0, 0, Width, bordtop, img);									// Top
		FlatFill (0, bordtop, bordleft, Height - bordbottom, img);				// Left
		FlatFill (Width - bordright, bordtop, Width, Height - bordbottom, img);	// Right
		FlatFill (0, Height - bordbottom, Width, Height, img);					// Bottom
	}
	else
	{
		Clear (0, 0, Width, bordtop, GPalette.BlackIndex, 0);									// Top
		Clear (0, bordtop, bordleft, Height - bordbottom, GPalette.BlackIndex, 0);				// Left
		Clear (Width - bordright, bordtop, Width, Height - bordbottom, GPalette.BlackIndex, 0);	// Right
		Clear (0, Height - bordbottom, Width, Height, GPalette.BlackIndex, 0);					// Bottom
	}
}

void DCanvas::DrawLine(int x0, int y0, int x1, int y1, int palColor, uint32_t realcolor)
{
#ifndef NO_SWRENDER
	SWCanvas::DrawLine(this, x0, y0, x1, y1, palColor, realcolor);
#endif
}

DEFINE_ACTION_FUNCTION(_Screen, DrawLine)
{
	PARAM_PROLOGUE;
	PARAM_INT(x0);
	PARAM_INT(y0);
	PARAM_INT(x1);
	PARAM_INT(y1);
	PARAM_INT(color);
	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	screen->DrawLine(x0, y0, x1, y1, -1, color);
	return 0;
}

void DCanvas::DrawPixel(int x, int y, int palColor, uint32_t realcolor)
{
#ifndef NO_SWRENDER
	SWCanvas::DrawPixel(this, x, y, palColor, realcolor);
#endif
}

//==========================================================================
//
// DCanvas :: Clear
//
// Set an area to a specified color.
//
//==========================================================================

void DCanvas::DoClear (int left, int top, int right, int bottom, int palcolor, uint32_t color)
{
#ifndef NO_SWRENDER
	if (palcolor < 0 && APART(color) != 255)
	{
		Dim(color, APART(color) / 255.f, left, top, right - left, bottom - top);
	}
	else
	{
		SWCanvas::Clear(this, left, top, right, bottom, palcolor, color);
	}
#endif
}

void DCanvas::Clear(int left, int top, int right, int bottom, int palcolor, uint32_t color)
{
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
	DoClear(left, top, right, bottom, palcolor, color);
}

DEFINE_ACTION_FUNCTION(_Screen, Clear)
{
	PARAM_PROLOGUE;
	PARAM_INT(x1);
	PARAM_INT(y1);
	PARAM_INT(x2);
	PARAM_INT(y2);
	PARAM_INT(color);
	PARAM_INT_DEF(palcol);
	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	screen->Clear(x1, y1, x2, y2, palcol, color);
	return 0;
}

//==========================================================================
//
// DCanvas :: Dim
//
// Applies a colored overlay to an area of the screen.
//
//==========================================================================

void DCanvas::DoDim(PalEntry color, float damount, int x1, int y1, int w, int h)
{
#ifndef NO_SWRENDER
	SWCanvas::Dim(this, color, damount, x1, y1, w, h);
#endif
}

void DCanvas::Dim(PalEntry color, float damount, int x1, int y1, int w, int h)
{
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
	DoDim(color, damount, x1, y1, w, h);
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
	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	screen->Dim(color, float(amount), x1, y1, w, h);
	return 0;
}

//==========================================================================
//
// DCanvas :: FillSimplePoly
//
// Fills a simple polygon with a texture. Here, "simple" means that a
// horizontal scanline at any vertical position within the polygon will
// not cross it more than twice.
//
// The originx, originy, scale, and rotation parameters specify
// transformation of the filling texture, not of the points.
//
// The points must be specified in clockwise order.
//
//==========================================================================

void DCanvas::FillSimplePoly(FTexture *tex, FVector2 *points, int npoints,
	double originx, double originy, double scalex, double scaley, DAngle rotation,
	const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip)
{
#ifndef NO_SWRENDER
	SWCanvas::FillSimplePoly(this, tex, points, npoints, originx, originy, scalex, scaley, rotation, colormap, flatcolor, lightlevel, bottomclip);
#endif
}


/********************************/
/*								*/
/* Other miscellaneous routines */
/*								*/
/********************************/


//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//
void DCanvas::DrawBlock (int x, int y, int _width, int _height, const uint8_t *src) const
{
	if (IsBgra())
		return;

	int srcpitch = _width;
	int destpitch;
	uint8_t *dest;

	if (ClipBox (x, y, _width, _height, src, srcpitch))
	{
		return;		// Nothing to draw
	}

	destpitch = Pitch;
	dest = Buffer + y*Pitch + x;

	do
	{
		memcpy (dest, src, _width);
		src += srcpitch;
		dest += destpitch;
	} while (--_height);
}

//
// V_GetBlock
// Gets a linear block of pixels from the view buffer.
//
void DCanvas::GetBlock (int x, int y, int _width, int _height, uint8_t *dest) const
{
	if (IsBgra())
		return;

	const uint8_t *src;

#ifdef RANGECHECK 
	if (x<0
		||x+_width > Width
		|| y<0
		|| y+_height>Height)
	{
		I_Error ("Bad V_GetBlock");
	}
#endif

	src = Buffer + y*Pitch + x;

	while (_height--)
	{
		memcpy (dest, src, _width);
		src += Pitch;
		dest += _width;
	}
}

// Returns true if the box was completely clipped. False otherwise.
bool DCanvas::ClipBox (int &x, int &y, int &w, int &h, const uint8_t *&src, const int srcpitch) const
{
	if (x >= Width || y >= Height || x+w <= 0 || y+h <= 0)
	{ // Completely clipped off screen
		return true;
	}
	if (x < 0)				// clip left edge
	{
		src -= x;
		w += x;
		x = 0;
	}
	if (x+w > Width)		// clip right edge
	{
		w = Width - x;
	}
	if (y < 0)				// clip top edge
	{
		src -= y*srcpitch;
		h += y;
		y = 0;
	}
	if (y+h > Height)		// clip bottom edge
	{
		h = Height - y;
	}
	return false;
}

//==========================================================================
//
// V_SetBorderNeedRefresh
//
// Flag the border as in need of updating. (Probably because something that
// was on top of it has changed.
//
//==========================================================================

void V_SetBorderNeedRefresh()
{
	if (screen != NULL)
	{
		BorderNeedRefresh = screen->GetPageCount();
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

void V_DrawFrame (int left, int top, int width, int height)
{
	FTexture *p;
	const gameborder_t *border = &gameinfo.Border;
	// Sanity check for incomplete gameinfo
	if (border == NULL)
		return;
	int offset = border->offset;
	int right = left + width;
	int bottom = top + height;

	// Draw top and bottom sides.
	p = TexMan[border->t];
	screen->FlatFill(left, top - p->GetHeight(), right, top, p, true);
	p = TexMan[border->b];
	screen->FlatFill(left, bottom, right, bottom + p->GetHeight(), p, true);

	// Draw left and right sides.
	p = TexMan[border->l];
	screen->FlatFill(left - p->GetWidth(), top, left, bottom, p, true);
	p = TexMan[border->r];
	screen->FlatFill(right, top, right + p->GetWidth(), bottom, p, true);

	// Draw beveled corners.
	screen->DrawTexture (TexMan[border->tl], left-offset, top-offset, TAG_DONE);
	screen->DrawTexture (TexMan[border->tr], left+width, top-offset, TAG_DONE);
	screen->DrawTexture (TexMan[border->bl], left-offset, top+height, TAG_DONE);
	screen->DrawTexture (TexMan[border->br], left+width, top+height, TAG_DONE);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawFrame)
{
	PARAM_PROLOGUE;
	PARAM_INT(x);
	PARAM_INT(y);
	PARAM_INT(w);
	PARAM_INT(h);
	V_DrawFrame(x, y, w, h);
	return 0;
}

//==========================================================================
//
// V_DrawBorder
//
//==========================================================================

void V_DrawBorder (int x1, int y1, int x2, int y2)
{
	FTextureID picnum;

	if (level.info != NULL && level.info->BorderTexture.Len() != 0)
	{
		picnum = TexMan.CheckForTexture (level.info->BorderTexture, FTexture::TEX_Flat);
	}
	else
	{
		picnum = TexMan.CheckForTexture (gameinfo.BorderFlat, FTexture::TEX_Flat);
	}

	if (picnum.isValid())
	{
		screen->FlatFill (x1, y1, x2, y2, TexMan(picnum));
	}
	else
	{
		screen->Clear (x1, y1, x2, y2, 0, 0);
	}
}

//==========================================================================
//
// R_DrawViewBorder
//
// Draws the border around the view for different size windows
//
//==========================================================================

int BorderNeedRefresh;


static void V_DrawViewBorder (void)
{
	if (viewwidth == SCREENWIDTH)
	{
		return;
	}

	V_DrawBorder (0, 0, SCREENWIDTH, viewwindowy);
	V_DrawBorder (0, viewwindowy, viewwindowx, viewheight + viewwindowy);
	V_DrawBorder (viewwindowx + viewwidth, viewwindowy, SCREENWIDTH, viewheight + viewwindowy);
	V_DrawBorder (0, viewwindowy + viewheight, SCREENWIDTH, StatusBar->GetTopOfStatusbar());

	V_DrawFrame (viewwindowx, viewwindowy, viewwidth, viewheight);
}

//==========================================================================
//
// R_DrawTopBorder
//
// Draws the top border around the view for different size windows
//
//==========================================================================

static void V_DrawTopBorder ()
{
	FTexture *p;
	int offset;

	if (viewwidth == SCREENWIDTH)
		return;

	offset = gameinfo.Border.offset;

	if (viewwindowy < 34)
	{
		V_DrawBorder (0, 0, viewwindowx, 34);
		V_DrawBorder (viewwindowx, 0, viewwindowx + viewwidth, viewwindowy);
		V_DrawBorder (viewwindowx + viewwidth, 0, SCREENWIDTH, 34);
		p = TexMan(gameinfo.Border.t);
		screen->FlatFill(viewwindowx, viewwindowy - p->GetHeight(),
						 viewwindowx + viewwidth, viewwindowy, p, true);

		p = TexMan(gameinfo.Border.l);
		screen->FlatFill(viewwindowx - p->GetWidth(), viewwindowy,
						 viewwindowx, 35, p, true);
		p = TexMan(gameinfo.Border.r);
		screen->FlatFill(viewwindowx + viewwidth, viewwindowy,
						 viewwindowx + viewwidth + p->GetWidth(), 35, p, true);

		p = TexMan(gameinfo.Border.tl);
		screen->DrawTexture (p, viewwindowx - offset, viewwindowy - offset, TAG_DONE);

		p = TexMan(gameinfo.Border.tr);
		screen->DrawTexture (p, viewwindowx + viewwidth, viewwindowy - offset, TAG_DONE);
	}
	else
	{
		V_DrawBorder (0, 0, SCREENWIDTH, 34);
	}
}

//==========================================================================
//
// R_RefreshViewBorder
//
// Draws the border around the player view, if needed.
//
//==========================================================================

void V_RefreshViewBorder ()
{
	if (setblocks < 10)
	{
		if (BorderNeedRefresh)
		{
			BorderNeedRefresh--;
			if (BorderTopRefresh)
			{
				BorderTopRefresh--;
			}
			V_DrawViewBorder();
		}
		else if (BorderTopRefresh)
		{
			BorderTopRefresh--;
			V_DrawTopBorder();
		}
	}
}

