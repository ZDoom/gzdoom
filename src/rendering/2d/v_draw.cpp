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

#include "doomtype.h"
#include "v_video.h"
#include "r_defs.h"
#include "r_utility.h"
#include "r_renderer.h"
#include "doomstat.h"
#include "gi.h"
#include "g_level.h"
#include "sbar.h"
#include "d_player.h"

#include "i_video.h"
#include "g_levellocals.h"
#include "vm.h"
#include "hwrenderer/utility/hw_cvars.h"

CVAR(Float, underwater_fade_scalar, 1.0f, CVAR_ARCHIVE) // [Nash] user-settable underwater blend intensity

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

// The new console font is twice as high, so the scaling calculation must factor that in.
int GetConScale(int altval)
{
	int scaleval;
	if (altval > 0) scaleval = (altval+1) / 2;
	else if (uiscale == 0)
	{
		// Default should try to scale to 640x400
		int vscale = screen->GetHeight() / 800;
		int hscale = screen->GetWidth() / 1280;
		scaleval = clamp(vscale, 1, hscale);
	}
	else scaleval = (uiscale+1) / 2;

	// block scales that result in something larger than the current screen.
	int vmax = screen->GetHeight() / 400;
	int hmax = screen->GetWidth() / 640;
	int max = MAX(vmax, hmax);
	return MAX(1, MIN(scaleval, max));
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
// ZScript wrappers for inlines
//
//==========================================================================

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

//==========================================================================
//
// Internal texture drawing function
//
//==========================================================================

void DFrameBuffer::DrawTexture (FTexture *img, double x, double y, int tags_first, ...)
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

//==========================================================================
//
// ZScript texture drawing function
//
//==========================================================================

int ListGetInt(VMVa_List &tags);

void DFrameBuffer::DrawTexture(FTexture *img, double x, double y, VMVa_List &args)
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

	PARAM_VA_POINTER(va_reginfo)	// Get the hidden type information array

	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");

	FTexture *tex = TexMan.ByIndex(texid, animate);
	VMVa_List args = { param + 4, 0, numparam - 5, va_reginfo + 4 };
	screen->DrawTexture(tex, x, y, args);
	return 0;
}

//==========================================================================
//
// common drawing function
//
//==========================================================================

void DFrameBuffer::DrawTextureParms(FTexture *img, DrawParms &parms)
{
	m2DDrawer.AddTexture(img, parms);
}

//==========================================================================
//
// ZScript arbitrary textured shape drawing functions
//
//==========================================================================

void DFrameBuffer::DrawShape(FTexture *img, DShape2D *shape, int tags_first, ...)
{
	Va_List tags;
	va_start(tags.list, tags_first);
	DrawParms parms;

	bool res = ParseDrawTextureTags(img, 0, 0, tags_first, tags, &parms, false);
	va_end(tags.list);
	if (!res) return;
	m2DDrawer.AddShape(img, shape, parms);
}

void DFrameBuffer::DrawShape(FTexture *img, DShape2D *shape, VMVa_List &args)
{
	DrawParms parms;
	uint32_t tag = ListGetInt(args);

	bool res = ParseDrawTextureTags(img, 0, 0, tag, args, &parms, false);
	if (!res) return;
	m2DDrawer.AddShape(img, shape, parms);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawShape)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	PARAM_BOOL(animate);
	PARAM_POINTER(shape, DShape2D);

	PARAM_VA_POINTER(va_reginfo)	// Get the hidden type information array

	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");

	FTexture *tex = TexMan.ByIndex(texid, animate);
	VMVa_List args = { param + 3, 0, numparam - 4, va_reginfo + 3 };

	screen->DrawShape(tex, shape, args);
	return 0;
}

//==========================================================================
//
// Clipping rect
//
//==========================================================================

void DFrameBuffer::SetClipRect(int x, int y, int w, int h)
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

void DFrameBuffer::GetClipRect(int *x, int *y, int *w, int *h)
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

//==========================================================================
//
// Draw parameter parsing
//
//==========================================================================

bool DFrameBuffer::SetTextureParms(DrawParms *parms, FTexture *img, double xx, double yy) const
{
	if (img != NULL)
	{
		parms->x = xx;
		parms->y = yy;
		parms->texwidth = img->GetDisplayWidthDouble();
		parms->texheight = img->GetDisplayHeightDouble();
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
			parms->destwidth = img->GetDisplayWidthDouble();
		}
		if (parms->destheight == INT_MAX || parms->fortext)
		{
			parms->destheight = img->GetDisplayHeightDouble();
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
bool DFrameBuffer::ParseDrawTextureTags(FTexture *img, double x, double y, uint32_t tag, T& tags, DrawParms *parms, bool fortext) const
{
	INTBOOL boolval;
	int intval;
	bool translationset = false;
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
	parms->flipY = false;
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
				parms->virtWidth = img->GetDisplayWidthDouble();
				parms->virtHeight = img->GetDisplayHeightDouble();
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

		case DTA_FlipY:
			parms->flipY = ListGetInt(tags);
			break;

		case DTA_SrcX:
			parms->srcx = ListGetDouble(tags) / img->GetDisplayWidthDouble();
			break;

		case DTA_SrcY:
			parms->srcy = ListGetDouble(tags) / img->GetDisplayHeightDouble();
			break;

		case DTA_SrcWidth:
			parms->srcwidth = ListGetDouble(tags) / img->GetDisplayWidthDouble();
			break;

		case DTA_SrcHeight:
			parms->srcheight = ListGetDouble(tags) / img->GetDisplayHeightDouble();
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
				parms->left = img->GetDisplayWidthDouble() * 0.5;
				parms->top = img->GetDisplayHeightDouble() * 0.5;
			}
			break;

		case DTA_CenterBottomOffset:
			assert(fortext == false);
			if (fortext) return false;
			if (ListGetInt(tags))
			{
				parms->left = img->GetDisplayWidthDouble() * 0.5;
				parms->top = img->GetDisplayHeightDouble();
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

template bool DFrameBuffer::ParseDrawTextureTags<Va_List>(FTexture *img, double x, double y, uint32_t tag, Va_List& tags, DrawParms *parms, bool fortext) const;
template bool DFrameBuffer::ParseDrawTextureTags<VMVa_List>(FTexture *img, double x, double y, uint32_t tag, VMVa_List& tags, DrawParms *parms, bool fortext) const;

//==========================================================================
//
// Coordinate conversion
//
//==========================================================================

void DFrameBuffer::VirtualToRealCoords(double &x, double &y, double &w, double &h,
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
	PARAM_BOOL(vbottom);
	PARAM_BOOL(handleaspect);
	screen->VirtualToRealCoords(x, y, w, h, vw, vh, vbottom, handleaspect);
	if (numret >= 1) ret[0].SetVector2(DVector2(x, y));
	if (numret >= 2) ret[1].SetVector2(DVector2(w, h));
	return MIN(numret, 2);
}

void DFrameBuffer::VirtualToRealCoordsInt(int &x, int &y, int &w, int &h,
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

//==========================================================================
//
//
//
//==========================================================================

void DFrameBuffer::FillBorder (FTexture *img)
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

//==========================================================================
//
// Draw a line
//
//==========================================================================

void DFrameBuffer::DrawLine(int x0, int y0, int x1, int y1, int palColor, uint32_t realcolor, uint8_t alpha)
{
	m2DDrawer.AddLine(x0, y0, x1, y1, palColor, realcolor, alpha);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawLine)
{
	PARAM_PROLOGUE;
	PARAM_INT(x0);
	PARAM_INT(y0);
	PARAM_INT(x1);
	PARAM_INT(y1);
	PARAM_INT(color);
	PARAM_INT(alpha);
	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	screen->DrawLine(x0, y0, x1, y1, -1, color, alpha);
	return 0;
}

void DFrameBuffer::DrawThickLine(int x0, int y0, int x1, int y1, double thickness, uint32_t realcolor, uint8_t alpha) {
	m2DDrawer.AddThickLine(x0, y0, x1, y1, thickness, realcolor, alpha);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawThickLine)
{
	PARAM_PROLOGUE;
	PARAM_INT(x0);
	PARAM_INT(y0);
	PARAM_INT(x1);
	PARAM_INT(y1);
	PARAM_FLOAT(thickness);
	PARAM_INT(color);
	PARAM_INT(alpha);
	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	screen->DrawThickLine(x0, y0, x1, y1, thickness, color, alpha);
	return 0;
}

//==========================================================================
//
// Draw a single pixel
//
//==========================================================================

void DFrameBuffer::DrawPixel(int x, int y, int palColor, uint32_t realcolor)
{
	m2DDrawer.AddPixel(x, y, palColor, realcolor);
}

//==========================================================================
//
// DCanvas :: Clear
//
// Set an area to a specified color.
//
//==========================================================================

void DFrameBuffer::Clear(int left, int top, int right, int bottom, int palcolor, uint32_t color)
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

	if (palcolor >= 0 && color == 0)
	{
		color = GPalette.BaseColors[palcolor] | 0xff000000;
	}
	m2DDrawer.AddColorOnlyQuad(left, top, right - left, bottom - top, color | 0xFF000000, nullptr);
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

void DFrameBuffer::DoDim(PalEntry color, float amount, int x1, int y1, int w, int h, FRenderStyle *style)
{
	if (amount <= 0)
	{
		return;
	}
	if (amount > 1)
	{
		amount = 1;
	}
	m2DDrawer.AddColorOnlyQuad(x1, y1, w, h, (color.d & 0xffffff) | (int(amount * 255) << 24), style);
}

void DFrameBuffer::Dim(PalEntry color, float damount, int x1, int y1, int w, int h, FRenderStyle *style)
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
	DoDim(color, damount, x1, y1, w, h, style);
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

void DFrameBuffer::FillSimplePoly(FTexture *tex, FVector2 *points, int npoints,
	double originx, double originy, double scalex, double scaley, DAngle rotation,
	const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip, uint32_t *indices, size_t indexcount)
{
	m2DDrawer.AddPoly(tex, points, npoints, originx, originy, scalex, scaley, rotation, colormap, flatcolor, lightlevel, indices, indexcount);
}

//==========================================================================
//
// DCanvas :: FlatFill
//
// Fill an area with a texture. If local_origin is false, then the origin
// used for the wrapping is (0,0). Otherwise, (left,right) is used.
//
//==========================================================================

void DFrameBuffer::FlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{
	m2DDrawer.AddFlatFill(left, top, right, bottom, src, local_origin);
}

//==========================================================================
//
// V_DrawFrame
//
// Draw a frame around the specified area using the view border
// frame graphics. The border is drawn outside the area, not in it.
//
//==========================================================================

void DFrameBuffer::DrawFrame (int left, int top, int width, int height)
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
	p = TexMan.GetTextureByName(border->t);
	FlatFill(left, top - p->GetDisplayHeight(), right, top, p, true);
	p = TexMan.GetTextureByName(border->b);
	FlatFill(left, bottom, right, bottom + p->GetDisplayHeight(), p, true);

	// Draw left and right sides.
	p = TexMan.GetTextureByName(border->l);
	FlatFill(left - p->GetDisplayWidth(), top, left, bottom, p, true);
	p = TexMan.GetTextureByName(border->r);
	FlatFill(right, top, right + p->GetDisplayWidth(), bottom, p, true);

	// Draw beveled corners.
	DrawTexture (TexMan.GetTextureByName(border->tl), left-offset, top-offset, TAG_DONE);
	DrawTexture (TexMan.GetTextureByName(border->tr), left+width, top-offset, TAG_DONE);
	DrawTexture (TexMan.GetTextureByName(border->bl), left-offset, top+height, TAG_DONE);
	DrawTexture (TexMan.GetTextureByName(border->br), left+width, top+height, TAG_DONE);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawFrame)
{
	PARAM_PROLOGUE;
	PARAM_INT(x);
	PARAM_INT(y);
	PARAM_INT(w);
	PARAM_INT(h);
	screen->DrawFrame(x, y, w, h);
	return 0;
}

//==========================================================================
//
// screen->DrawBorder
//
//==========================================================================

void DFrameBuffer::DrawBorder (FTextureID picnum, int x1, int y1, int x2, int y2)
{
	if (picnum.isValid())
	{
		FlatFill (x1, y1, x2, y2, TexMan.GetTexture(picnum, false));
	}
	else
	{
		Clear (x1, y1, x2, y2, 0, 0);
	}
}

//==========================================================================
//
// Draws a blend over the entire view
//
//==========================================================================

FVector4 DFrameBuffer::CalcBlend(sector_t * viewsector, PalEntry *modulateColor)
{
	float blend[4] = { 0,0,0,0 };
	PalEntry blendv = 0;
	float extra_red;
	float extra_green;
	float extra_blue;
	player_t *player = nullptr;
	bool fullbright = false;

	if (modulateColor) *modulateColor = 0xffffffff;

	if (players[consoleplayer].camera != nullptr)
	{
		player = players[consoleplayer].camera->player;
		if (player)
			fullbright = (player->fixedcolormap != NOFIXEDCOLORMAP || player->extralight == INT_MIN || player->fixedlightlevel != -1);
	}

	// don't draw sector based blends when any fullbright screen effect is active.
	if (!fullbright)
	{
		const auto &vpp = r_viewpoint.Pos;
		if (!viewsector->e->XFloor.ffloors.Size())
		{
			if (viewsector->GetHeightSec())
			{
				auto s = viewsector->heightsec;
				blendv = s->floorplane.PointOnSide(vpp) < 0 ? s->bottommap : s->ceilingplane.PointOnSide(vpp) < 0 ? s->topmap : s->midmap;
			}
		}
		else
		{
			TArray<lightlist_t> & lightlist = viewsector->e->XFloor.lightlist;

			for (unsigned int i = 0; i < lightlist.Size(); i++)
			{
				double lightbottom;
				if (i < lightlist.Size() - 1)
					lightbottom = lightlist[i + 1].plane.ZatPoint(vpp);
				else
					lightbottom = viewsector->floorplane.ZatPoint(vpp);

				if (lightbottom < vpp.Z && (!lightlist[i].caster || !(lightlist[i].caster->flags&FF_FADEWALLS)))
				{
					// 3d floor 'fog' is rendered as a blending value
					blendv = lightlist[i].blend;
					// If this is the same as the sector's it doesn't apply!
					if (blendv == viewsector->Colormap.FadeColor) blendv = 0;
					// a little hack to make this work for Legacy maps.
					if (blendv.a == 0 && blendv != 0) blendv.a = 128;
					break;
				}
			}
		}

		if (blendv.a == 0 && V_IsTrueColor())	// The paletted software renderer uses the original colormap as this frame's palette, but in true color that isn't doable.
		{
			blendv = R_BlendForColormap(blendv);
		}

		if (blendv.a == 255)
		{

			extra_red = blendv.r / 255.0f;
			extra_green = blendv.g / 255.0f;
			extra_blue = blendv.b / 255.0f;

			// If this is a multiplicative blend do it separately and add the additive ones on top of it.

			// black multiplicative blends are ignored
			if (extra_red || extra_green || extra_blue)
			{
				if (modulateColor) *modulateColor = blendv;
			}
			blendv = 0;
		}
		else if (blendv.a)
		{
			// [Nash] allow user to set blend intensity
			int cnt = blendv.a;
			cnt = (int)(cnt * underwater_fade_scalar);

			V_AddBlend(blendv.r / 255.f, blendv.g / 255.f, blendv.b / 255.f, cnt / 255.0f, blend);
		}
	}
	else if (player && player->fixedlightlevel != -1 && player->fixedcolormap == NOFIXEDCOLORMAP)
	{
		// Draw fixedlightlevel effects as a 2D overlay. The hardware renderer just processes such a scene fullbright without any lighting.
		auto torchtype = PClass::FindActor(NAME_PowerTorch);
		auto litetype = PClass::FindActor(NAME_PowerLightAmp);
		PalEntry color = 0xffffffff;
		for (AActor *in = player->mo->Inventory; in; in = in->Inventory)
		{
			// Need special handling for light amplifiers 
			if (in->IsKindOf(torchtype))
			{
				// The software renderer already bakes the torch flickering into its output, so this must be omitted here.
				float r = vid_rendermode < 4 ? 1.f : (0.8f + (7 - player->fixedlightlevel) / 70.0f);
				if (r > 1.0f) r = 1.0f;
				int rr = (int)(r * 255);
				int b = rr;
				if (gl_enhanced_nightvision) b = b * 3 / 4;
				color = PalEntry(255, rr, rr, b);
			}
			else if (in->IsKindOf(litetype))
			{
				if (gl_enhanced_nightvision)
				{
					color = PalEntry(255, 104, 255, 104);
				}
			}
		}
		if (modulateColor)
		{ 
			*modulateColor = color;
		}
	}

	if (player)
	{
		V_AddPlayerBlend(player, blend, 0.5, 175);
	}

	if (players[consoleplayer].camera != NULL)
	{
		// except for fadeto effects
		player_t *player = (players[consoleplayer].camera->player != NULL) ? players[consoleplayer].camera->player : &players[consoleplayer];
		V_AddBlend(player->BlendR, player->BlendG, player->BlendB, player->BlendA, blend);
	}

	const float br = clamp(blend[0] * 255.f, 0.f, 255.f);
	const float bg = clamp(blend[1] * 255.f, 0.f, 255.f);
	const float bb = clamp(blend[2] * 255.f, 0.f, 255.f);
	return { br, bg, bb, blend[3] };
}

//==========================================================================
//
// Draws a blend over the entire view
//
//==========================================================================

void DFrameBuffer::DrawBlend(sector_t * viewsector)
{
	PalEntry modulateColor;
	auto blend = CalcBlend(viewsector, &modulateColor);
	if (modulateColor != 0xffffffff)
	{
		Dim(modulateColor, 1, 0, 0, GetWidth(), GetHeight(), &LegacyRenderStyles[STYLE_Multiply]);
	}

	const PalEntry bcolor(255, uint8_t(blend.X), uint8_t(blend.Y), uint8_t(blend.Z));
	Dim(bcolor, blend.W, 0, 0, GetWidth(), GetHeight());
}


