/*
** r_swcanvas.cpp
** Draw to a canvas using software rendering
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

#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/r_renderthread.h"
#include "v_palette.h"
#include "v_video.h"
#include "m_png.h"
#include "r_swcolormaps.h"
#include "colormatcher.h"
#include "r_swcanvas.h"
#include "textures/textures.h"
#include "r_data/voxels.h"
#include "drawers/r_draw_rgba.h"

EXTERN_CVAR(Bool, r_blendmethod)

using namespace swrenderer;

void SWCanvas::DrawTexture(DCanvas *canvas, FTexture *img, DrawParms &parms)
{
	static short bottomclipper[MAXWIDTH], topclipper[MAXWIDTH];

	static RenderThread thread(nullptr);
	thread.DrawQueue->ThreadedRender = false;

	auto viewport = thread.Viewport.get();
	viewport->RenderTarget = canvas;
	viewport->RenderTarget->Lock(true);

	lighttable_t *translation = nullptr;
	FDynamicColormap *basecolormap = &identitycolormap;
	int shade = 0;

	if (APART(parms.colorOverlay) != 0)
	{
		// The software renderer cannot invert the source without inverting the overlay
		// too. That means if the source is inverted, we need to do the reverse of what
		// the invert overlay flag says to do.
		INTBOOL invertoverlay = (parms.style.Flags & STYLEF_InvertOverlay);

		if (parms.style.Flags & STYLEF_InvertSource)
		{
			invertoverlay = !invertoverlay;
		}
		if (invertoverlay)
		{
			parms.colorOverlay = PalEntry(parms.colorOverlay).InverseColor();
		}
		// Note that this overrides the translation in software, but not in hardware.
		FDynamicColormap *colormap = GetSpecialLights(MAKERGB(255, 255, 255), parms.colorOverlay & MAKEARGB(0, 255, 255, 255), 0);

		if (viewport->RenderTarget->IsBgra())
		{
			basecolormap = colormap;
			shade = (APART(parms.colorOverlay)*NUMCOLORMAPS / 255) << FRACBITS;
		}
		else
		{
			translation = &colormap->Maps[(APART(parms.colorOverlay)*NUMCOLORMAPS / 255) * 256];
		}
	}
	else if (parms.remap != NULL)
	{
		if (viewport->RenderTarget->IsBgra())
			translation = (lighttable_t *)parms.remap->Palette;
		else
			translation = parms.remap->Remap;
	}

	SpriteDrawerArgs drawerargs;

	drawerargs.SetTranslationMap(translation);
	drawerargs.SetLight(basecolormap, 0.0f, shade);

	uint32_t myfillcolor = (RGB256k.All[((parms.fillcolor & 0xfc0000) >> 6) |
		((parms.fillcolor & 0xfc00) >> 4) | ((parms.fillcolor & 0xfc) >> 2)]) << 24 |
		(parms.fillcolor & 0xffffff);
	bool visible = drawerargs.SetStyle(viewport, parms.style, parms.Alpha, -1, myfillcolor, basecolormap);

	double x0 = parms.x - parms.left * parms.destwidth / parms.texwidth;
	double y0 = parms.y - parms.top * parms.destheight / parms.texheight;

	if (visible)
	{
		double centeryback = viewport->CenterY;
		viewport->CenterY = 0;

		int oldviewwindowx = 0;
		int oldviewwindowy = 0;
		oldviewwindowx = viewwindowx;
		oldviewwindowy = viewwindowy;
		viewwindowx = 0;
		viewwindowy = 0;

		// There is not enough precision in the drawing routines to keep the full
		// precision for y0. :(
		double sprtopscreen;
		modf(y0 + 0.5, &sprtopscreen);

		double yscale = parms.destheight / img->GetHeight();
		double iyscale = 1 / yscale;

		double spryscale = yscale;
		assert(spryscale > 0);

		bool sprflipvert = false;
		fixed_t iscale = FLOAT2FIXED(1 / spryscale);
		//dc_texturemid = (CenterY - 1 - sprtopscreen) * iscale / 65536;
		fixed_t frac = 0;
		double xiscale = img->GetWidth() / parms.destwidth;
		double x2 = x0 + parms.destwidth;

		short *mfloorclip;
		short *mceilingclip;

		if (bottomclipper[0] != parms.dclip)
		{
			fillshort(bottomclipper, screen->GetWidth(), (short)parms.dclip);
		}
		if (parms.uclip != 0)
		{
			if (topclipper[0] != parms.uclip)
			{
				fillshort(topclipper, screen->GetWidth(), (short)parms.uclip);
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

		if (parms.windowleft > 0 || parms.windowright < parms.texwidth)
		{
			double wi = MIN(parms.windowright, parms.texwidth);
			double xscale = parms.destwidth / parms.texwidth;
			x0 += parms.windowleft * xscale;
			frac += FLOAT2FIXED(parms.windowleft);
			x2 -= (parms.texwidth - wi) * xscale;
		}
		if (x0 < parms.lclip)
		{
			frac += FLOAT2FIXED((parms.lclip - x0) * xiscale);
			x0 = parms.lclip;
		}
		if (x2 > parms.rclip)
		{
			x2 = parms.rclip;
		}

		int x = int(x0);
		int x2_i = int(x2);
		fixed_t xiscale_i = FLOAT2FIXED(xiscale);

		frac += xiscale_i / 2;

		while (x < x2_i)
		{
			drawerargs.DrawMaskedColumn(&thread, x, iscale, img, frac, spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, !parms.masked);
			x++;
			frac += xiscale_i;
		}

		viewport->CenterY = centeryback;
		viewwindowx = oldviewwindowx;
		viewwindowy = oldviewwindowy;
	}

	viewport->RenderTarget->Unlock();
	viewport->RenderTarget = canvas;
}

void SWCanvas::FillSimplePoly(DCanvas *canvas, FTexture *tex, FVector2 *points, int npoints,
	double originx, double originy, double scalex, double scaley, DAngle rotation,
	const FColormap &fcolormap, PalEntry flatcolor, int lightlevel, int bottomclip)
{
	// Use an equation similar to player sprites to determine shade
	fixed_t shade = LightVisibility::LightLevelToShade(lightlevel, true) - 12 * FRACUNIT;
	float topy, boty, leftx, rightx;
	int toppt, botpt, pt1, pt2;
	int i;
	int y1, y2, y;
	fixed_t x;
	bool dorotate = rotation != 0.;
	double cosrot, sinrot;
	auto colormap = GetColorTable(fcolormap);

	if (--npoints < 2)
	{ // not a polygon or we're not locked
		return;
	}

	if (bottomclip <= 0)
	{
		bottomclip = canvas->GetHeight();
	}

	// Find the extents of the polygon, in particular the highest and lowest points.
	for (botpt = toppt = 0, boty = topy = points[0].Y, leftx = rightx = points[0].X, i = 1; i <= npoints; ++i)
	{
		if (points[i].Y < topy)
		{
			topy = points[i].Y;
			toppt = i;
		}
		if (points[i].Y > boty)
		{
			boty = points[i].Y;
			botpt = i;
		}
		if (points[i].X < leftx)
		{
			leftx = points[i].X;
		}
		if (points[i].X > rightx)
		{
			rightx = points[i].X;
		}
	}
	if (topy >= bottomclip ||	// off the bottom of the screen
		boty <= 0 ||			// off the top of the screen
		leftx >= canvas->GetWidth() || // off the right of the screen
		rightx <= 0)			// off the left of the screen
	{
		return;
	}

	static RenderThread thread(nullptr);
	thread.DrawQueue->ThreadedRender = false;

	auto viewport = thread.Viewport.get();
	viewport->RenderTarget = canvas;

	viewport->RenderTarget->Lock(true);

	scalex = tex->Scale.X / scalex;
	scaley = tex->Scale.Y / scaley;

	// Use the CRT's functions here.
	cosrot = cos(rotation.Radians());
	sinrot = sin(rotation.Radians());

	// Setup constant texture mapping parameters.
	SpanDrawerArgs drawerargs;
	drawerargs.SetTexture(&thread, tex);
	if (colormap)
		drawerargs.SetLight(colormap, 0, clamp(shade >> FRACBITS, 0, NUMCOLORMAPS - 1));
	else
		drawerargs.SetLight(&identitycolormap, 0, 0);

	scalex /= drawerargs.TextureWidth();
	scaley /= drawerargs.TextureHeight();

	drawerargs.SetTextureUStep(cosrot * scalex);
	drawerargs.SetTextureVStep(sinrot * scaley);

	int width = canvas->GetWidth();

	// Travel down the right edge and create an outline of that edge.
	static short spanend[MAXHEIGHT];
	pt1 = toppt;
	pt2 = toppt + 1;	if (pt2 > npoints) pt2 = 0;
	y1 = xs_RoundToInt(points[pt1].Y + 0.5f);
	do
	{
		x = FLOAT2FIXED(points[pt1].X + 0.5f);
		y2 = xs_RoundToInt(points[pt2].Y + 0.5f);
		if (y1 >= y2 || (y1 < 0 && y2 < 0) || (y1 >= bottomclip && y2 >= bottomclip))
		{
		}
		else
		{
			fixed_t xinc = FLOAT2FIXED((points[pt2].X - points[pt1].X) / (points[pt2].Y - points[pt1].Y));
			int y3 = MIN(y2, bottomclip);
			if (y1 < 0)
			{
				x += xinc * -y1;
				y1 = 0;
			}
			for (y = y1; y < y3; ++y)
			{
				spanend[y] = clamp<short>(x >> FRACBITS, -1, width);
				x += xinc;
			}
		}
		y1 = y2;
		pt1 = pt2;
		pt2++;			if (pt2 > npoints) pt2 = 0;
	} while (pt1 != botpt);

	// Travel down the left edge and fill it in.
	pt1 = toppt;
	pt2 = toppt - 1;	if (pt2 < 0) pt2 = npoints;
	y1 = xs_RoundToInt(points[pt1].Y + 0.5f);
	do
	{
		x = FLOAT2FIXED(points[pt1].X + 0.5f);
		y2 = xs_RoundToInt(points[pt2].Y + 0.5f);
		if (y1 >= y2 || (y1 < 0 && y2 < 0) || (y1 >= bottomclip && y2 >= bottomclip))
		{
		}
		else
		{
			fixed_t xinc = FLOAT2FIXED((points[pt2].X - points[pt1].X) / (points[pt2].Y - points[pt1].Y));
			int y3 = MIN(y2, bottomclip);
			if (y1 < 0)
			{
				x += xinc * -y1;
				y1 = 0;
			}
			for (y = y1; y < y3; ++y)
			{
				int x1 = x >> FRACBITS;
				int x2 = spanend[y];
				if (x2 > x1 && x2 > 0 && x1 < width)
				{
					x1 = MAX(x1, 0);
					x2 = MIN(x2, width);
#if 0
					memset(this->Buffer + y * this->Pitch + x1, (int)tex, x2 - x1);
#else
					drawerargs.SetDestY(viewport, y);
					drawerargs.SetDestX1(x1);
					drawerargs.SetDestX2(x2 - 1);

					DVector2 tex(x1 - originx, y - originy);
					if (dorotate)
					{
						double t = tex.X;
						tex.X = t * cosrot - tex.Y * sinrot;
						tex.Y = tex.Y * cosrot + t * sinrot;
					}
					drawerargs.SetTextureUPos(tex.X * scalex);
					drawerargs.SetTextureVPos(tex.Y * scaley);

					drawerargs.DrawSpan(&thread);
#endif
				}
				x += xinc;
			}
		}
		y1 = y2;
		pt1 = pt2;
		pt2--;			if (pt2 < 0) pt2 = npoints;
	} while (pt1 != botpt);

	viewport->RenderTarget->Unlock();
	viewport->RenderTarget = screen;
}

void SWCanvas::DrawLine(DCanvas *canvas, int x0, int y0, int x1, int y1, int palColor, uint32_t realcolor)
{
	const int WeightingScale = 0;
	const int WEIGHTBITS = 6;
	const int WEIGHTSHIFT = 16 - WEIGHTBITS;
	const int NUMWEIGHTS = (1 << WEIGHTBITS);
	const int WEIGHTMASK = (NUMWEIGHTS - 1);

	if (palColor < 0)
	{
		palColor = PalFromRGB(realcolor);
	}

	canvas->Lock();
	int deltaX, deltaY, xDir;

	if (y0 > y1)
	{
		int temp = y0; y0 = y1; y1 = temp;
		temp = x0; x0 = x1; x1 = temp;
	}

	PUTTRANSDOT(canvas, x0, y0, palColor, 0);

	if ((deltaX = x1 - x0) >= 0)
	{
		xDir = 1;
	}
	else
	{
		xDir = -1;
		deltaX = -deltaX;
	}

	if ((deltaY = y1 - y0) == 0)
	{ // horizontal line
		if (x0 > x1)
		{
			swapvalues(x0, x1);
		}
		if (canvas->IsBgra())
		{
			uint32_t fillColor = GPalette.BaseColors[palColor].d;
			uint32_t *spot = (uint32_t*)canvas->GetBuffer() + y0*canvas->GetPitch() + x0;
			for (int i = 0; i <= deltaX; i++)
				spot[i] = fillColor;
		}
		else
		{
			memset(canvas->GetBuffer() + y0*canvas->GetPitch() + x0, palColor, deltaX + 1);
		}
	}
	else if (deltaX == 0)
	{ // vertical line
		if (canvas->IsBgra())
		{
			uint32_t fillColor = GPalette.BaseColors[palColor].d;
			uint32_t *spot = (uint32_t*)canvas->GetBuffer() + y0*canvas->GetPitch() + x0;
			int pitch = canvas->GetPitch();
			do
			{
				*spot = fillColor;
				spot += pitch;
			} while (--deltaY != 0);
		}
		else
		{
			uint8_t *spot = canvas->GetBuffer() + y0*canvas->GetPitch() + x0;
			int pitch = canvas->GetPitch();
			do
			{
				*spot = palColor;
				spot += pitch;
			} while (--deltaY != 0);
		}
	}
	else if (deltaX == deltaY)
	{ // diagonal line.
		if (canvas->IsBgra())
		{
			uint32_t fillColor = GPalette.BaseColors[palColor].d;
			uint32_t *spot = (uint32_t*)canvas->GetBuffer() + y0*canvas->GetPitch() + x0;
			int advance = canvas->GetPitch() + xDir;
			do
			{
				*spot = fillColor;
				spot += advance;
			} while (--deltaY != 0);
		}
		else
		{
			uint8_t *spot = canvas->GetBuffer() + y0*canvas->GetPitch() + x0;
			int advance = canvas->GetPitch() + xDir;
			do
			{
				*spot = palColor;
				spot += advance;
			} while (--deltaY != 0);
		}
	}
	else
	{
		// line is not horizontal, diagonal, or vertical
		fixed_t errorAcc = 0;

		if (deltaY > deltaX)
		{ // y-major line
			fixed_t errorAdj = (((unsigned)deltaX << 16) / (unsigned)deltaY) & 0xffff;
			if (xDir < 0)
			{
				if (WeightingScale == 0)
				{
					while (--deltaY)
					{
						errorAcc += errorAdj;
						y0++;
						int weighting = (errorAcc >> WEIGHTSHIFT) & WEIGHTMASK;
						PUTTRANSDOT(canvas, x0 - (errorAcc >> 16), y0, palColor, weighting);
						PUTTRANSDOT(canvas, x0 - (errorAcc >> 16) - 1, y0,
							palColor, WEIGHTMASK - weighting);
					}
				}
				else
				{
					while (--deltaY)
					{
						errorAcc += errorAdj;
						y0++;
						int weighting = ((errorAcc * WeightingScale) >> (WEIGHTSHIFT + 8)) & WEIGHTMASK;
						PUTTRANSDOT(canvas, x0 - (errorAcc >> 16), y0, palColor, weighting);
						PUTTRANSDOT(canvas, x0 - (errorAcc >> 16) - 1, y0,
							palColor, WEIGHTMASK - weighting);
					}
				}
			}
			else
			{
				if (WeightingScale == 0)
				{
					while (--deltaY)
					{
						errorAcc += errorAdj;
						y0++;
						int weighting = (errorAcc >> WEIGHTSHIFT) & WEIGHTMASK;
						PUTTRANSDOT(canvas, x0 + (errorAcc >> 16), y0, palColor, weighting);
						PUTTRANSDOT(canvas, x0 + (errorAcc >> 16) + xDir, y0,
							palColor, WEIGHTMASK - weighting);
					}
				}
				else
				{
					while (--deltaY)
					{
						errorAcc += errorAdj;
						y0++;
						int weighting = ((errorAcc * WeightingScale) >> (WEIGHTSHIFT + 8)) & WEIGHTMASK;
						PUTTRANSDOT(canvas, x0 + (errorAcc >> 16), y0, palColor, weighting);
						PUTTRANSDOT(canvas, x0 + (errorAcc >> 16) + xDir, y0,
							palColor, WEIGHTMASK - weighting);
					}
				}
			}
		}
		else
		{ // x-major line
			fixed_t errorAdj = (((uint32_t)deltaY << 16) / (uint32_t)deltaX) & 0xffff;

			if (WeightingScale == 0)
			{
				while (--deltaX)
				{
					errorAcc += errorAdj;
					x0 += xDir;
					int weighting = (errorAcc >> WEIGHTSHIFT) & WEIGHTMASK;
					PUTTRANSDOT(canvas, x0, y0 + (errorAcc >> 16), palColor, weighting);
					PUTTRANSDOT(canvas, x0, y0 + (errorAcc >> 16) + 1,
						palColor, WEIGHTMASK - weighting);
				}
			}
			else
			{
				while (--deltaX)
				{
					errorAcc += errorAdj;
					x0 += xDir;
					int weighting = ((errorAcc * WeightingScale) >> (WEIGHTSHIFT + 8)) & WEIGHTMASK;
					PUTTRANSDOT(canvas, x0, y0 + (errorAcc >> 16), palColor, weighting);
					PUTTRANSDOT(canvas, x0, y0 + (errorAcc >> 16) + 1,
						palColor, WEIGHTMASK - weighting);
				}
			}
		}
		PUTTRANSDOT(canvas, x1, y1, palColor, 0);
	}
	canvas->Unlock();
}

void SWCanvas::DrawPixel(DCanvas *canvas, int x, int y, int palColor, uint32_t realcolor)
{
	if (palColor < 0)
	{
		palColor = PalFromRGB(realcolor);
	}

	canvas->GetBuffer()[canvas->GetPitch() * y + x] = (uint8_t)palColor;
}

void SWCanvas::PUTTRANSDOT(DCanvas *canvas, int xx, int yy, int basecolor, int level)
{
	static int oldyy;
	static int oldyyshifted;

	if (yy == oldyy + 1)
	{
		oldyy++;
		oldyyshifted += canvas->GetPitch();
	}
	else if (yy == oldyy - 1)
	{
		oldyy--;
		oldyyshifted -= canvas->GetPitch();
	}
	else if (yy != oldyy)
	{
		oldyy = yy;
		oldyyshifted = yy * canvas->GetPitch();
	}

	if (canvas->IsBgra())
	{
		uint32_t *spot = (uint32_t*)canvas->GetBuffer() + oldyyshifted + xx;

		uint32_t fg = swrenderer::LightBgra::shade_pal_index_simple(basecolor, swrenderer::LightBgra::calc_light_multiplier(0));
		uint32_t fg_red = (((fg >> 16) & 0xff) * (63 - level)) >> 6;
		uint32_t fg_green = (((fg >> 8) & 0xff) * (63 - level)) >> 6;
		uint32_t fg_blue = ((fg & 0xff) * (63 - level)) >> 6;

		uint32_t bg_red = (((*spot >> 16) & 0xff) * level) >> 6;
		uint32_t bg_green = (((*spot >> 8) & 0xff) * level) >> 6;
		uint32_t bg_blue = (((*spot) & 0xff) * level) >> 6;

		uint32_t red = fg_red + bg_red;
		uint32_t green = fg_green + bg_green;
		uint32_t blue = fg_blue + bg_blue;

		*spot = 0xff000000 | (red << 16) | (green << 8) | blue;
	}
	else if (!r_blendmethod)
	{
		uint8_t *spot = canvas->GetBuffer() + oldyyshifted + xx;
		uint32_t *bg2rgb = Col2RGB8[1 + level];
		uint32_t *fg2rgb = Col2RGB8[63 - level];
		uint32_t fg = fg2rgb[basecolor];
		uint32_t bg = bg2rgb[*spot];
		bg = (fg + bg) | 0x1f07c1f;
		*spot = RGB32k.All[bg&(bg >> 15)];
	}
	else
	{
		uint8_t *spot = canvas->GetBuffer() + oldyyshifted + xx;

		uint32_t r = (GPalette.BaseColors[*spot].r * (64 - level) + GPalette.BaseColors[basecolor].r * level) / 256;
		uint32_t g = (GPalette.BaseColors[*spot].g * (64 - level) + GPalette.BaseColors[basecolor].g * level) / 256;
		uint32_t b = (GPalette.BaseColors[*spot].b * (64 - level) + GPalette.BaseColors[basecolor].b * level) / 256;

		*spot = (uint8_t)RGB256k.RGB[r][g][b];
	}
}

void SWCanvas::Clear(DCanvas *canvas, int left, int top, int right, int bottom, int palcolor, uint32_t color)
{
	int x, y;

	if (left == right || top == bottom)
	{
		return;
	}

	assert(left < right);
	assert(top < bottom);

	int Width = canvas->GetWidth();
	int Height = canvas->GetHeight();
	int Pitch = canvas->GetPitch();

	if (left >= Width || right <= 0 || top >= Height || bottom <= 0)
	{
		return;
	}
	left = MAX(0, left);
	right = MIN(Width, right);
	top = MAX(0, top);
	bottom = MIN(Height, bottom);

	if (palcolor < 0)
	{
		palcolor = PalFromRGB(color);
	}

	if (canvas->IsBgra())
	{
		uint32_t fill_color = GPalette.BaseColors[palcolor];

		uint32_t *dest = (uint32_t*)canvas->GetBuffer() + top * Pitch + left;
		x = right - left;
		for (y = top; y < bottom; y++)
		{
			for (int i = 0; i < x; i++)
				dest[i] = fill_color;
			dest += Pitch;
		}
	}
	else
	{
		uint8_t *dest = canvas->GetBuffer() + top * Pitch + left;
		x = right - left;
		for (y = top; y < bottom; y++)
		{
			memset(dest, palcolor, x);
			dest += Pitch;
		}
	}
}

void SWCanvas::Dim(DCanvas *canvas, PalEntry color, float damount, int x1, int y1, int w, int h)
{
	if (damount == 0.f)
		return;

	int Width = canvas->GetWidth();
	int Height = canvas->GetHeight();
	int Pitch = canvas->GetPitch();

	if (x1 >= Width || y1 >= Height)
	{
		return;
	}
	if (x1 + w > Width)
	{
		w = Width - x1;
	}
	if (y1 + h > Height)
	{
		h = Height - y1;
	}
	if (w <= 0 || h <= 0)
	{
		return;
	}

	if (canvas->IsBgra())
	{
		uint32_t *spot = (uint32_t*)canvas->GetBuffer() + x1 + y1*Pitch;
		int gap = Pitch - w;

		uint32_t fg = color.d;
		uint32_t fg_red = (fg >> 16) & 0xff;
		uint32_t fg_green = (fg >> 8) & 0xff;
		uint32_t fg_blue = fg & 0xff;

		uint32_t alpha = (uint32_t)clamp(damount * 256 + 0.5f, 0.0f, 256.0f);
		uint32_t inv_alpha = 256 - alpha;

		fg_red *= alpha;
		fg_green *= alpha;
		fg_blue *= alpha;

		for (int y = h; y != 0; y--)
		{
			for (int x = w; x != 0; x--)
			{
				uint32_t bg_red = (*spot >> 16) & 0xff;
				uint32_t bg_green = (*spot >> 8) & 0xff;
				uint32_t bg_blue = (*spot) & 0xff;

				uint32_t red = (fg_red + bg_red * inv_alpha) / 256;
				uint32_t green = (fg_green + bg_green * inv_alpha) / 256;
				uint32_t blue = (fg_blue + bg_blue * inv_alpha) / 256;

				*spot = 0xff000000 | (red << 16) | (green << 8) | blue;
				spot++;
			}
			spot += gap;
		}
	}
	else
	{
		uint32_t *bg2rgb;
		uint32_t fg;

		uint8_t *spot = canvas->GetBuffer() + x1 + y1*Pitch;
		int gap = Pitch - w;

		int alpha = (int)((float)64 * damount);
		int ialpha = 64 - alpha;
		int dimmedcolor_r = color.r * alpha;
		int dimmedcolor_g = color.g * alpha;
		int dimmedcolor_b = color.b * alpha;

		if (!r_blendmethod)
		{
			{
				int amount;

				amount = (int)(damount * 64);
				bg2rgb = Col2RGB8[64 - amount];

				fg = (((color.r * amount) >> 4) << 20) |
					((color.g * amount) >> 4) |
					(((color.b * amount) >> 4) << 10);
			}

			for (int y = h; y != 0; y--)
			{
				for (int x = w; x != 0; x--)
				{
					uint32_t bg;

					bg = bg2rgb[(*spot) & 0xff];
					bg = (fg + bg) | 0x1f07c1f;
					*spot = RGB32k.All[bg&(bg >> 15)];
					spot++;
				}
				spot += gap;
			}
		}
		else
		{
			for (int y = h; y != 0; y--)
			{
				for (int x = w; x != 0; x--)
				{
					uint32_t r = (dimmedcolor_r + GPalette.BaseColors[*spot].r * ialpha) >> 8;
					uint32_t g = (dimmedcolor_g + GPalette.BaseColors[*spot].g * ialpha) >> 8;
					uint32_t b = (dimmedcolor_b + GPalette.BaseColors[*spot].b * ialpha) >> 8;
					*spot = (uint8_t)RGB256k.RGB[r][g][b];
					spot++;
				}
				spot += gap;
			}
		}
	}
}

int SWCanvas::PalFromRGB(uint32_t rgb)
{
	// For routines that take RGB colors, cache the previous lookup in case there
	// are several repetitions with the same color.
	static int LastPal = -1;
	static uint32_t LastRGB;

	if (LastPal >= 0 && LastRGB == rgb)
	{
		return LastPal;
	}
	// Quick check for black and white.
	if (rgb == MAKEARGB(255, 0, 0, 0))
	{
		LastPal = GPalette.BlackIndex;
	}
	else if (rgb == MAKEARGB(255, 255, 255, 255))
	{
		LastPal = GPalette.WhiteIndex;
	}
	else
	{
		LastPal = ColorMatcher.Pick(RPART(rgb), GPART(rgb), BPART(rgb));
	}
	LastRGB = rgb;
	return LastPal;
}
