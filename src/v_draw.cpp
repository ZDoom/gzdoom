/*
** v_draw.cpp
** Draw patches and blocks to a canvas
**
**---------------------------------------------------------------------------
** Copyright 1998-2004 Randy Heit
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

void STACK_ARGS DCanvas::DrawTexture (FTexture *img, int x0, int y0, DWORD tags_first, ...)
{
	FTexture::Span unmaskedSpan[2];
	const FTexture::Span **spanptr, *spans;
	static BYTE identitymap[256];
	static short bottomclipper[MAXWIDTH], topclipper[MAXWIDTH];
	va_list tags;
	DWORD tag;
	BOOL boolval;
	int intval;

	if (img == NULL || img->UseType == FTexture::TEX_Null)
	{
		return;
	}

	int windowleft = 0;
	int windowright = img->GetWidth();
	int dclip = this->GetHeight();
	int uclip = 0;
	int lclip = 0;
	int rclip = this->GetWidth();
	int destwidth = windowright << FRACBITS;
	int destheight = img->GetHeight() << FRACBITS;
	int top = img->TopOffset;
	int left = img->LeftOffset;
	fixed_t alpha = FRACUNIT;
	int fillcolor = -1;
	const BYTE *translation = NULL;
	BOOL alphaChannel = false;
	BOOL flipX = false;
	fixed_t shadowAlpha = 0;
	int shadowColor = 0;
	int virtWidth = this->GetWidth();
	int virtHeight = this->GetHeight();

	x0 <<= FRACBITS;
	y0 <<= FRACBITS;

	spanptr = &spans;

	// Parse the tag list for attributes
	va_start (tags, tags_first);
	tag = tags_first;

	while (tag != TAG_DONE)
	{
		va_list more_p;
		DWORD data;

		switch (tag)
		{
		case TAG_IGNORE:
		default:
			data = va_arg (tags, DWORD);
			break;

		case TAG_MORE:
			more_p = va_arg (tags, va_list);
			va_end (tags);
			tags = more_p;
			break;

		case DTA_DestWidth:
			destwidth = va_arg (tags, int) << FRACBITS;
			break;

		case DTA_DestHeight:
			destheight = va_arg (tags, int) << FRACBITS;
			break;

		case DTA_Clean:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				x0 = (x0 - 160*FRACUNIT) * CleanXfac + (Width * (FRACUNIT/2));
				y0 = (y0 - 100*FRACUNIT) * CleanYfac + (Height * (FRACUNIT/2));
				destwidth = img->GetWidth() * CleanXfac * FRACUNIT;
				destheight = img->GetHeight() * CleanYfac * FRACUNIT;
			}
			break;

		case DTA_CleanNoMove:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				destwidth = img->GetWidth() * CleanXfac * FRACUNIT;
				destheight = img->GetHeight() * CleanYfac * FRACUNIT;
			}
			break;

		case DTA_320x200:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				virtWidth = 320;
				virtHeight = 200;
			}
			break;

		case DTA_HUDRules:
			{
				bool xright = x0 < 0;
				bool ybot = y0 < 0;
				intval = va_arg (tags, int);

				if (hud_scale)
				{
					x0 *= CleanXfac;
					if (intval == HUD_HorizCenter)
						x0 += Width * FRACUNIT / 2;
					else if (xright)
						x0 = Width * FRACUNIT + x0;
					y0 *= CleanYfac;
					if (ybot)
						y0 = Height * FRACUNIT + y0;
					destwidth = img->GetWidth() * CleanXfac * FRACUNIT;
					destheight = img->GetHeight() * CleanYfac * FRACUNIT;
				}
				else
				{
					if (intval == HUD_HorizCenter)
						x0 += Width * FRACUNIT / 2;
					else if (xright)
						x0 = Width * FRACUNIT + x0;
					if (ybot)
						y0 = Height * FRACUNIT + y0;
				}
			}
			break;

		case DTA_VirtualWidth:
			virtWidth = va_arg (tags, int);
			break;
			
		case DTA_VirtualHeight:
			virtHeight = va_arg (tags, int);
			break;

		case DTA_Alpha:
			alpha = MIN<fixed_t> (FRACUNIT, va_arg (tags, fixed_t));
			break;

		case DTA_AlphaChannel:
			alphaChannel = va_arg (tags, BOOL);
			break;

		case DTA_FillColor:
			fillcolor = va_arg (tags, int);
			break;

		case DTA_Translation:
			translation = va_arg (tags, const BYTE *);
			break;

		case DTA_FlipX:
			flipX = va_arg (tags, BOOL);
			break;

		case DTA_TopOffset:
			top = va_arg (tags, int);
			break;

		case DTA_LeftOffset:
			left = va_arg (tags, int);
			break;

		case DTA_CenterOffset:
			if (va_arg (tags, int))
			{
				left = img->GetWidth() / 2;
				top = img->GetHeight() / 2;
			}
			break;

		case DTA_CenterBottomOffset:
			if (va_arg (tags, int))
			{
				left = img->GetWidth() / 2;
				top = img->GetHeight();
			}
			break;

		case DTA_WindowLeft:
			windowleft = va_arg (tags, int);
			break;

		case DTA_WindowRight:
			windowright = va_arg (tags, int);
			break;

		case DTA_ClipTop:
			uclip = va_arg (tags, int);
			if (uclip < 0)
			{
				uclip = 0;
			}
			break;

		case DTA_ClipBottom:
			dclip = va_arg (tags, int);
			if (dclip > this->GetHeight())
			{
				dclip = this->GetHeight();
			}
			break;

		case DTA_ClipLeft:
			lclip = va_arg (tags, int);
			if (lclip < 0)
			{
				lclip = 0;
			}
			break;

		case DTA_ClipRight:
			rclip = va_arg (tags, int);
			if (rclip > this->GetWidth())
			{
				rclip = this->GetWidth();
			}
			break;

		case DTA_ShadowAlpha:
			shadowAlpha = MIN<fixed_t> (FRACUNIT, va_arg (tags, fixed_t));
			break;

		case DTA_ShadowColor:
			shadowColor = va_arg (tags, int);
			break;

		case DTA_Shadow:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				shadowAlpha = FRACUNIT/2;
				shadowColor = 0;
			}
			else
			{
				shadowAlpha = 0;
			}
			break;

		case DTA_Masked:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				spanptr = &spans;
			}
			else
			{
				spanptr = NULL;
			}
			break;
		}
		tag = va_arg (tags, DWORD);
	}
	va_end (tags);

	if (virtWidth != Width || virtHeight != Height)
	{
		int myratio = CheckRatio (Width, Height);
		if (myratio != 0)
		{ // The target surface is not 4:3, so expand the specified
		  // virtual size to avoid undesired stretching of the image.
		  // Does not handle non-4:3 virtual sizes. I'll worry about
		  // those if somebody expresses a desire to use them.
			x0 = Scale (Width*960, x0-virtWidth*FRACUNIT/2, virtWidth*BaseRatioSizes[myratio][0]) + Width*FRACUNIT/2;
			y0 = Scale (Height, y0, virtHeight);
			destwidth = FixedDiv (Width*960 * img->GetWidth(), virtWidth*BaseRatioSizes[myratio][0]);
			destheight = FixedDiv (Height * img->GetHeight(), virtHeight);
		}
		else
		{
			x0 = Scale (Width, x0, virtWidth);
			y0 = Scale (Height, y0, virtHeight);
			destwidth = FixedDiv (Width * img->GetWidth(), virtWidth);
			destheight = FixedDiv (Height * img->GetHeight(), virtHeight);
		}
	}

	if (destwidth <= 0 || destheight <= 0)
	{
		return;
	}

	ERenderStyle style;

	if (fillcolor >= 0)
	{
		if (alphaChannel)
		{
			style = STYLE_Shaded;
		}
		else if (alpha < FRACUNIT)
		{
			style = STYLE_TranslucentStencil;
		}
		else
		{
			style = STYLE_Stencil;
		}
	}
	else if (alpha < FRACUNIT)
	{
		style = STYLE_Translucent;
	}
	else
	{
		style = STYLE_Normal;
	}

	fixedcolormap = identitymap;
	ESPSResult mode = R_SetPatchStyle (style, alpha, 0, fillcolor<<24);

	if (style != STYLE_Shaded)
	{
		if (translation != NULL)
		{
			dc_colormap = (lighttable_t *)translation;
		}
		else
		{
			dc_colormap = identitymap;
		}
	}

	BYTE *destorgsave = dc_destorg;
	dc_destorg = screen->GetBuffer();

	x0 -= Scale (left, destwidth, img->GetWidth());
	y0 -= Scale (top, destheight, img->GetHeight());

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

		fixed_t centeryback = 0;
		centeryfrac = 0;

		sprtopscreen = y0;
		spryscale = destheight / img->GetHeight();

		// Fix precision errors that are noticeable at some resolutions
		if ((spryscale * img->GetHeight()) >> FRACBITS != destheight >> FRACBITS)
			spryscale++;

		sprflipvert = false;
		dc_iscale = 0xffffffffu / (unsigned)spryscale;
		dc_texturemid = FixedMul (-y0, dc_iscale);
		fixed_t frac = 0;
		fixed_t xiscale = DivScale32 (img->GetWidth(), destwidth);
		int x2 = (x0 + destwidth) >> FRACBITS;

		if (bottomclipper[0] != dclip)
		{
			clearbufshort (bottomclipper, screen->GetWidth(), (short)dclip);
			if (identitymap[1] != 1)
			{
				for (int i = 0; i < 256; ++i)
				{
					identitymap[i] = i;
				}
			}
		}
		if (uclip != 0)
		{
			if (topclipper[0] != uclip)
			{
				clearbufshort (topclipper, screen->GetWidth(), (short)uclip);
			}
			mceilingclip = topclipper;
		}
		else
		{
			mceilingclip = zeroarray;
		}
		mfloorclip = bottomclipper;

		if (flipX)
		{
			frac = (img->GetWidth() << FRACBITS) - 1;
			xiscale = -xiscale;
		}

		dc_x = x0 >> FRACBITS;
		if (windowleft > 0 || windowright < img->GetWidth())
		{
			fixed_t xscale = destwidth / img->GetWidth();
			dc_x += (windowleft * xscale) >> FRACBITS;
			frac += windowleft << FRACBITS;
			x2 -= ((img->GetWidth() - windowright) * xscale) >> FRACBITS;
		}
		if (dc_x < lclip)
		{
			frac += (lclip - dc_x) * xiscale;
			dc_x = lclip;
		}
		if (x2 > rclip)
		{
			x2 = rclip;
		}

		if (destheight < 32*FRACUNIT)
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
		Clear (0, 0, Width, bordtop, 0);									// Top
		Clear (0, bordtop, bordleft, Height - bordbottom, 0);				// Left
		Clear (Width - bordright, bordtop, Width, Height - bordbottom, 0);	// Right
		Clear (0, Height - bordbottom, Width, Height, 0);					// Bottom
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
void DCanvas::DrawBlock (int x, int y, int _width, int _height, const byte *src) const
{
	int srcpitch = _width;
	int destpitch;
	byte *dest;

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
void DCanvas::GetBlock (int x, int y, int _width, int _height, byte *dest) const
{
	const byte *src;

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
bool DCanvas::ClipBox (int &x, int &y, int &w, int &h, const byte *&src, const int srcpitch) const
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
