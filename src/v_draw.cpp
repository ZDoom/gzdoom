/*
** v_draw.cpp
** Draw patches and blocks to a canvas
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
#include "m_swap.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_things.h"

#include "i_system.h"
#include "i_video.h"
#include "templates.h"

// [RH] Stretch values to make a 320x200 image best fit the screen
// without using fractional steppings
int CleanXfac, CleanYfac;

// [RH] Effective screen sizes that the above scale values give you
int CleanWidth, CleanHeight;

CVAR (Bool, hud_scale, false, CVAR_ARCHIVE);

void STACK_ARGS DCanvas::DrawTexture (FTexture *img, int x, int y, int tags_first, ...)
{
	va_list tags;
	va_start(tags, tags_first);
	DrawTextureV(img, x, y, tags_first, tags);
}

void STACK_ARGS DCanvas::DrawTextureV(FTexture *img, int x, int y, uint32 tag, va_list tags)
{
	FTexture::Span unmaskedSpan[2];
	const FTexture::Span **spanptr, *spans;
	static BYTE identitymap[256];
	static short bottomclipper[MAXWIDTH], topclipper[MAXWIDTH];

	DrawParms parms;

	if (!ParseDrawTextureTags(img, x, y, tag, tags, &parms))
	{
		return;
	}

	if (parms.masked)
	{
		spanptr = &spans;
	}
	else
	{
		spanptr = NULL;
	}

	fixedcolormap = identitymap;
	ESPSResult mode = R_SetPatchStyle (parms.style, parms.alpha, 0, parms.fillcolor);

	if (parms.style != STYLE_Shaded)
	{
		if (parms.font != NULL)
		{
			dc_colormap = parms.font->GetColorTranslation (EColorRange(parms.translation));
		}
		else if (parms.translation != 0)
		{
			dc_colormap = translationtables[(parms.translation&0xff00)>>8] + (parms.translation&0x00ff)*256;
		}
		else
		{
			dc_colormap = identitymap;
		}
	}

	BYTE *destorgsave = dc_destorg;
	dc_destorg = screen->GetBuffer();

	fixed_t x0 = parms.x - Scale (parms.left, parms.destwidth, parms.texwidth);
	fixed_t y0 = parms.y - Scale (parms.top, parms.destheight, parms.texheight);

	if (mode != DontDraw)
	{
		const BYTE *pixels;
		int stop4;

		if (spanptr == NULL)
		{ // Create a single span for forced unmasked images
			spans = unmaskedSpan;
			unmaskedSpan[0].TopOffset = 0;
			unmaskedSpan[0].Length = img->GetHeight();
			unmaskedSpan[1].TopOffset = 0;
			unmaskedSpan[1].Length = 0;
		}

		fixed_t centeryback = centeryfrac;
		centeryfrac = 0;

		sprtopscreen = y0;
		spryscale = parms.destheight / img->GetHeight();

		// Fix precision errors that are noticeable at some resolutions
		if (((y0 + parms.destheight) >> FRACBITS) > ((y0 + spryscale * img->GetHeight()) >> FRACBITS))
		{
			spryscale++;
		}

		sprflipvert = false;
		dc_iscale = 0xffffffffu / (unsigned)spryscale;
		dc_texturemid = FixedMul (-y0, dc_iscale);
		fixed_t frac = 0;
		fixed_t xiscale = DivScale32 (img->GetWidth(), parms.destwidth);
		int x2 = (x0 + parms.destwidth) >> FRACBITS;

		if (bottomclipper[0] != parms.dclip)
		{
			clearbufshort (bottomclipper, screen->GetWidth(), (short)parms.dclip);
			if (identitymap[1] != 1)
			{
				for (int i = 0; i < 256; ++i)
				{
					identitymap[i] = i;
				}
			}
		}
		if (parms.uclip != 0)
		{
			if (topclipper[0] != parms.uclip)
			{
				clearbufshort (topclipper, screen->GetWidth(), (short)parms.uclip);
			}
			mceilingclip = topclipper;
		}
		else
		{
			mceilingclip = zeroarray;
		}
		mfloorclip = bottomclipper;

		if (parms.flipX)
		{
			frac = (img->GetWidth() << FRACBITS) - 1;
			xiscale = -xiscale;
		}

		dc_x = x0 >> FRACBITS;
		if (parms.windowleft > 0 || parms.windowright < parms.texwidth)
		{
			fixed_t xscale = parms.destwidth / parms.texwidth;
			dc_x += (parms.windowleft * xscale) >> FRACBITS;
			frac += parms.windowleft << FRACBITS;
			x2 -= ((parms.texwidth - parms.windowright) * xscale) >> FRACBITS;
		}
		if (dc_x < parms.lclip)
		{
			frac += (parms.lclip - dc_x) * xiscale;
			dc_x = parms.lclip;
		}
		if (x2 > parms.rclip)
		{
			x2 = parms.rclip;
		}

		if (parms.destheight < 32*FRACUNIT)
		{
			mode = DoDraw0;
		}

		if (mode == DoDraw0)
		{
			// One column at a time
			stop4 = dc_x;
		}
		else	 // DoDraw1
		{
			// Up to four columns at a time
			stop4 = x2 & ~3;
		}

		if (dc_x < x2)
		{
			while ((dc_x < stop4) && (dc_x & 3))
			{
				pixels = img->GetColumn (frac >> FRACBITS, spanptr);
				R_DrawMaskedColumn (pixels, spans);
				dc_x++;
				frac += xiscale;
			}

			while (dc_x < stop4)
			{
				rt_initcols();
				for (int zz = 4; zz; --zz)
				{
					pixels = img->GetColumn (frac >> FRACBITS, spanptr);
					R_DrawMaskedColumnHoriz (pixels, spans);
					dc_x++;
					frac += xiscale;
				}
				rt_draw4cols (dc_x - 4);
			}

			while (dc_x < x2)
			{
				pixels = img->GetColumn (frac >> FRACBITS, spanptr);
				R_DrawMaskedColumn (pixels, spans);
				dc_x++;
				frac += xiscale;
			}
		}
		centeryfrac = centeryback;
	}
	R_FinishSetPatchStyle ();

	dc_destorg = destorgsave;

	if (ticdup != 0 && menuactive == MENU_Off)
	{
		NetUpdate ();
	}
}

bool DCanvas::ParseDrawTextureTags (FTexture *img, int x, int y, DWORD tag, va_list tags, DrawParms *parms) const
{
	INTBOOL boolval;
	int intval;
	bool translationset = false;

	if (img == NULL || img->UseType == FTexture::TEX_Null)
	{
		return false;
	}

	parms->texwidth = img->GetScaledWidth();
	parms->texheight = img->GetScaledHeight();

	parms->windowleft = 0;
	parms->windowright = parms->texwidth;
	parms->dclip = this->GetHeight();
	parms->uclip = 0;
	parms->lclip = 0;
	parms->rclip = this->GetWidth();
	parms->destwidth = parms->windowright << FRACBITS;
	parms->destheight = parms->texheight << FRACBITS;
	parms->top = img->GetScaledTopOffset();
	parms->left = img->GetScaledLeftOffset();
	parms->alpha = FRACUNIT;
	parms->fillcolor = -1;
	parms->font = NULL;
	parms->translation = 0;
	parms->alphaChannel = false;
	parms->flipX = false;
	parms->shadowAlpha = 0;
	parms->shadowColor = 0;
	parms->virtWidth = this->GetWidth();
	parms->virtHeight = this->GetHeight();
	parms->keepratio = false;
	parms->style = STYLE_Count;
	parms->masked = true;

	parms->x = x << FRACBITS;
	parms->y = y << FRACBITS;

	// Parse the tag list for attributes
	while (tag != TAG_DONE)
	{
		va_list *more_p;
		DWORD data;

		switch (tag)
		{
		case TAG_IGNORE:
		default:
			data = va_arg (tags, DWORD);
			break;

		case TAG_MORE:
			more_p = va_arg (tags, va_list *);
			va_end (tags);
#ifdef __GNUC__
			__va_copy (tags, *more_p);
#else
			tags = *more_p;
#endif
			break;

		case DTA_DestWidth:
			parms->destwidth = va_arg (tags, int) << FRACBITS;
			break;

		case DTA_DestHeight:
			parms->destheight = va_arg (tags, int) << FRACBITS;
			break;

		case DTA_Clean:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				parms->x = (parms->x - 160*FRACUNIT) * CleanXfac + (Width * (FRACUNIT/2));
				parms->y = (parms->y - 100*FRACUNIT) * CleanYfac + (Height * (FRACUNIT/2));
				parms->destwidth = parms->texwidth * CleanXfac * FRACUNIT;
				parms->destheight = parms->texheight * CleanYfac * FRACUNIT;
			}
			break;

		case DTA_CleanNoMove:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				parms->destwidth = parms->texwidth * CleanXfac * FRACUNIT;
				parms->destheight = parms->texheight * CleanYfac * FRACUNIT;
			}
			break;

		case DTA_320x200:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				parms->virtWidth = 320;
				parms->virtHeight = 200;
			}
			break;

		case DTA_HUDRules:
			{
				bool xright = parms->x < 0;
				bool ybot = parms->y < 0;
				intval = va_arg (tags, int);

				if (hud_scale)
				{
					parms->x *= CleanXfac;
					if (intval == HUD_HorizCenter)
						parms->x += Width * FRACUNIT / 2;
					else if (xright)
						parms->x = Width * FRACUNIT + parms->x;
					parms->y *= CleanYfac;
					if (ybot)
						parms->y = Height * FRACUNIT + parms->y;
					parms->destwidth = parms->texwidth * CleanXfac * FRACUNIT;
					parms->destheight = parms->texheight * CleanYfac * FRACUNIT;
				}
				else
				{
					if (intval == HUD_HorizCenter)
						parms->x += Width * FRACUNIT / 2;
					else if (xright)
						parms->x = Width * FRACUNIT + parms->x;
					if (ybot)
						parms->y = Height * FRACUNIT + parms->y;
				}
			}
			break;

		case DTA_VirtualWidth:
			parms->virtWidth = va_arg (tags, int);
			break;
			
		case DTA_VirtualHeight:
			parms->virtHeight = va_arg (tags, int);
			break;

		case DTA_Alpha:
			parms->alpha = MIN<fixed_t> (FRACUNIT, va_arg (tags, fixed_t));
			break;

		case DTA_AlphaChannel:
			parms->alphaChannel = va_arg (tags, INTBOOL);
			break;

		case DTA_FillColor:
			parms->fillcolor = va_arg (tags, int);
			break;

		case DTA_Font:
			parms->font = va_arg(tags, FFont*);
			if (!translationset)
			{
				// default translation for fonts should be untranslated which unfortunately is not 0.
				parms->translation = CR_UNTRANSLATED;
				translationset = true;
			}
			break;

		case DTA_Translation:
			parms->translation = va_arg(tags, int);
			translationset = true;
			break;

		case DTA_FlipX:
			parms->flipX = va_arg (tags, INTBOOL);
			break;

		case DTA_TopOffset:
			parms->top = va_arg (tags, int);
			break;

		case DTA_LeftOffset:
			parms->left = va_arg (tags, int);
			break;

		case DTA_CenterOffset:
			if (va_arg (tags, int))
			{
				parms->left = parms->texwidth / 2;
				parms->top = parms->texheight / 2;
			}
			break;

		case DTA_CenterBottomOffset:
			if (va_arg (tags, int))
			{
				parms->left = parms->texwidth / 2;
				parms->top = parms->texheight;
			}
			break;

		case DTA_WindowLeft:
			parms->windowleft = va_arg (tags, int);
			break;

		case DTA_WindowRight:
			parms->windowright = va_arg (tags, int);
			break;

		case DTA_ClipTop:
			parms->uclip = va_arg (tags, int);
			if (parms->uclip < 0)
			{
				parms->uclip = 0;
			}
			break;

		case DTA_ClipBottom:
			parms->dclip = va_arg (tags, int);
			if (parms->dclip > this->GetHeight())
			{
				parms->dclip = this->GetHeight();
			}
			break;

		case DTA_ClipLeft:
			parms->lclip = va_arg (tags, int);
			if (parms->lclip < 0)
			{
				parms->lclip = 0;
			}
			break;

		case DTA_ClipRight:
			parms->rclip = va_arg (tags, int);
			if (parms->rclip > this->GetWidth())
			{
				parms->rclip = this->GetWidth();
			}
			break;

		case DTA_ShadowAlpha:
			parms->shadowAlpha = MIN<fixed_t> (FRACUNIT, va_arg (tags, fixed_t));
			break;

		case DTA_ShadowColor:
			parms->shadowColor = va_arg (tags, int);
			break;

		case DTA_Shadow:
			boolval = va_arg (tags, INTBOOL);
			if (boolval)
			{
				parms->shadowAlpha = FRACUNIT/2;
				parms->shadowColor = 0;
			}
			else
			{
				parms->shadowAlpha = 0;
			}
			break;

		case DTA_Masked:
			parms->masked = va_arg (tags, INTBOOL);
			break;

		case DTA_KeepRatio:
			parms->keepratio = va_arg (tags, INTBOOL);
			break;

		case DTA_RenderStyle:
			parms->style = ERenderStyle(va_arg (tags, int));
			break;
		}
		tag = va_arg (tags, DWORD);
	}
	va_end (tags);

	if (parms->virtWidth != Width || parms->virtHeight != Height)
	{
		int myratio = CheckRatio (Width, Height);
		int right = parms->x + parms->destwidth;
		int bottom = parms->y + parms->destheight;

		if (myratio != 0 && myratio != 4 && !parms->keepratio)
		{ // The target surface is not 4:3, so expand the specified
		  // virtual size to avoid undesired stretching of the image.
		  // Does not handle non-4:3 virtual sizes. I'll worry about
		  // those if somebody expresses a desire to use them.
			parms->x = Scale(parms->x - parms->virtWidth*FRACUNIT/2,
							 Width*960,
							 parms->virtWidth*BaseRatioSizes[myratio][0])
						+ Width*FRACUNIT/2;
			parms->destwidth = Scale(right - parms->virtWidth*FRACUNIT/2,
							 Width*960,
							 parms->virtWidth*BaseRatioSizes[myratio][0])
						+ Width*FRACUNIT/2 - parms->x;
		}
		else
		{
			parms->x = Scale (parms->x, Width, parms->virtWidth);
			parms->destwidth = Scale (right, Width, parms->virtWidth) - parms->x;
		}
		parms->y = Scale (parms->y, Height, parms->virtHeight);
		parms->destheight = Scale (bottom, Height, parms->virtHeight) - parms->y;
	}

	if (parms->destwidth <= 0 || parms->destheight <= 0)
	{
		return false;
	}

	if (parms->style == STYLE_Count)
	{
		if (parms->fillcolor != -1)
		{
			if (parms->alphaChannel)
			{
				parms->style = STYLE_Shaded;
			}
			else if (parms->alpha < FRACUNIT)
			{
				parms->style = STYLE_TranslucentStencil;
			}
			else
			{
				parms->style = STYLE_Stencil;
			}
		}
		else if (parms->alpha < FRACUNIT)
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

void DCanvas::FillBorder (FTexture *img)
{
	int myratio = CheckRatio (Width, Height);
	if (myratio == 0)
	{ // This is a 4:3 display, so no border to show
		return;
	}
	int bordtop, bordbottom, bordleft, bordright, bord;
	if (myratio & 4)
	{ // Screen is taller than it is wide
		bordleft = bordright = 0;
		bord = Height - Height * BaseRatioSizes[myratio][3] / 48;
		bordtop = bord / 2;
		bordbottom = bord - bordtop;
	}
	else
	{ // Screen is wider than it is tall
		bordtop = bordbottom = 0;
		bord = Width - Width * BaseRatioSizes[myratio][3] / 48;
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
		Clear (0, 0, Width, bordtop, 0, 0);										// Top
		Clear (0, bordtop, bordleft, Height - bordbottom, 0, 0);				// Left
		Clear (Width - bordright, bordtop, Width, Height - bordbottom, 0, 0);	// Right
		Clear (0, Height - bordbottom, Width, Height, 0, 0);					// Bottom
	}
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
void DCanvas::DrawBlock (int x, int y, int _width, int _height, const BYTE *src) const
{
	int srcpitch = _width;
	int destpitch;
	BYTE *dest;

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
void DCanvas::GetBlock (int x, int y, int _width, int _height, BYTE *dest) const
{
	const BYTE *src;

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
bool DCanvas::ClipBox (int &x, int &y, int &w, int &h, const BYTE *&src, const int srcpitch) const
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
