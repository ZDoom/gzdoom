#include <stddef.h>
#include <string.h>
#include <math.h>

#include "templates.h"
#include "v_video.h"
#include "m_alloc.h"
#include "r_main.h"		// For lighting constants
#include "w_wad.h"
#include "z_zone.h"
#include "i_video.h"
#include "c_dispatch.h"
#include "g_level.h"
#include "st_stuff.h"
#include "gi.h"

extern "C" {
FDynamicColormap NormalLight;
}
FPalette GPalette;
BYTE *InvulnerabilityColormap;
int Near0;

FColorMatcher ColorMatcher;

/* Current color blending values */
int		BlendR, BlendG, BlendB, BlendA;


/**************************/
/* Gamma correction stuff */
/**************************/

byte newgamma[256];
CUSTOM_CVAR (Float, Gamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self == 0.f)
	{ // Gamma values of 0 are illegal.
		self = 1.f;
		return;
	}

	if (screen != NULL)
	{
		screen->SetGamma (self);
	}
}


/****************************/
/* Palette management stuff */
/****************************/

extern "C"
{
	byte BestColor_MMX (DWORD rgb, const DWORD *pal);
}

int BestColor (const DWORD *pal_in, int r, int g, int b, int first)
{
#ifdef USEASM
	if (UseMMX)
	{
		return BestColor_MMX ((first<<24)|(r<<16)|(g<<8)|b, pal_in);
	}
#endif
	const PalEntry *pal = (const PalEntry *)pal_in;
	int bestcolor = first;
	int bestdist = 257*257+257*257+257*257;

	for (int color = first; color < 256; color++)
	{
		int dist = (r-pal[color].r)*(r-pal[color].r)+
				   (g-pal[color].g)*(g-pal[color].g)+
				   (b-pal[color].b)*(b-pal[color].b);
		if (dist < bestdist)
		{
			if (dist == 0)
				return color;

			bestdist = dist;
			bestcolor = color;
		}
	}
	return bestcolor;
}

FPalette::FPalette ()
{
}

FPalette::FPalette (BYTE *colors)
{
	SetPalette (colors);
}

void FPalette::SetPalette (BYTE *colors)
{
	int i;

	for (i = 0; i < 256; i++, colors += 3)
		BaseColors[i] = PalEntry (colors[0], colors[1], colors[2]);

	GammaAdjust ();
}

void InitPalette ()
{
	BYTE *shade;
	int c;

	GPalette.SetPalette ((BYTE *)W_CacheLumpName ("PLAYPAL", PU_CACHE));
	ColorMatcher.SetPalette ((DWORD *)GPalette.BaseColors);
	Near0 = BestColor ((DWORD *)GPalette.BaseColors,
		GPalette.BaseColors[0].r, GPalette.BaseColors[0].g, GPalette.BaseColors[0].b, 1);

// NormalLight.Maps will be set to realcolormaps no later than G_InitLevelLocals()
// (which occurs before it is ever needed)
//	NormalLight.Maps = (BYTE *)Z_Malloc (NUMCOLORMAPS*256+255, PU_STATIC, 0);
//	NormalLight.Maps = (BYTE *)(((ptrdiff_t)NormalLight.Maps + 255) & ~0xff);
	NormalLight.Color = PalEntry (255, 255, 255);
	NormalLight.Fade = 0;

	InvulnerabilityColormap = (BYTE *)Z_Malloc (NUMCOLORMAPS*256+255, PU_STATIC, 0);
	InvulnerabilityColormap = (BYTE *)(((ptrdiff_t)InvulnerabilityColormap + 255) & ~0xff);

	// build special maps (e.g. invulnerability)
	shade = InvulnerabilityColormap;
	if (gameinfo.gametype == GAME_Doom)
	{
		int grayint;
		for (c = 0; c < 256; c++)
		{
			grayint = (65535 -
				(GPalette.BaseColors[c].r * 77 +
				 GPalette.BaseColors[c].g * 143 +
				 GPalette.BaseColors[c].b * 37)) >> 8;
			*shade++ = ColorMatcher.Pick (grayint, grayint, grayint);
		}
	}
	else
	{
		int intensity;

		for (c = 0; c < 256; c++)
		{
			intensity = GPalette.BaseColors[c].r * 77 +
						GPalette.BaseColors[c].g * 143 +
						GPalette.BaseColors[c].b * 37;
			*shade++ = ColorMatcher.Pick (
				MIN (255, (intensity+intensity/2)>>8), intensity>>8, 0);
		}
	}
}

extern "C"
{
	void STACK_ARGS DoBlending_MMX (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);
}

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a)
{
	if (a == 0)
	{
		if (from != to)
		{
			memcpy (to, from, count * sizeof(DWORD));
		}
	}
	else if (a == 256)
	{
		DWORD t = MAKERGB(r,g,b);
		int i;

		for (i = 0; i < count; i++)
		{
			to[i] = t;
		}
	}
#ifdef USEASM
	else if (UseMMX && !(count & 1))
	{
		DoBlending_MMX (from, to, count, r, g, b, a);
	}
#endif
	else
	{
		int i, ia;

		ia = 256 - a;
		r *= a;
		g *= a;
		b *= a;

		for (i = count; i > 0; i--, to++, from++)
		{
			to->r = (r + from->r*ia) >> 8;
			to->g = (g + from->g*ia) >> 8;
			to->b = (b + from->b*ia) >> 8;
		}
	}

}

void FPalette::GammaAdjust ()
{
	int i;

	for (i = 0; i < 256; i++)
	{
		PalEntry color = BaseColors[i];
		Colors[i] = PalEntry (newgamma[color.r], newgamma[color.g], newgamma[color.b]);
	}
}

void V_SetBlend (int blendr, int blendg, int blendb, int blenda)
{
	// Don't do anything if the new blend is the same as the old
	if (((blenda|BlendA) == 0) ||
		(blendr == BlendR &&
		 blendg == BlendG &&
		 blendb == BlendB &&
		 blenda == BlendA))
		return;

	V_ForceBlend (blendr, blendg, blendb, blenda);
}

void V_ForceBlend (int blendr, int blendg, int blendb, int blenda)
{
	BlendR = blendr;
	BlendG = blendg;
	BlendB = blendb;
	BlendA = blenda;

	screen->SetFlash (PalEntry (BlendR, BlendG, BlendB), BlendA);
}

CCMD (testblend)
{
	char *colorstring;
	int color;
	float amt;

	if (argv.argc() < 3)
	{
		Printf ("testblend <color> <amount>\n");
	}
	else
	{
		if ( (colorstring = V_GetColorStringByName (argv[1])) )
		{
			color = V_GetColorFromString (NULL, colorstring);
			delete[] colorstring;
		}
		else
		{
			color = V_GetColorFromString (NULL, argv[1]);
		}
		amt = (float)atof (argv[2]);
		if (amt > 1.0f)
			amt = 1.0f;
		else if (amt < 0.0f)
			amt = 0.0f;
		BaseBlendR = RPART(color);
		BaseBlendG = GPART(color);
		BaseBlendB = BPART(color);
		BaseBlendA = amt;
	}
}

CCMD (testfade)
{
	char *colorstring;
	DWORD color;

	if (argv.argc() < 2)
	{
		Printf ("testfade <color>\n");
	}
	else
	{
		if ( (colorstring = V_GetColorStringByName (argv[1])) )
		{
			color = V_GetColorFromString (NULL, colorstring);
			delete[] colorstring;
		}
		else
		{
			color = V_GetColorFromString (NULL, argv[1]);
		}
		level.fadeto = color;
		NormalLight.ChangeFade (color);
	}
}

/****** Colorspace Conversion Functions ******/

// Code from http://www.cs.rit.edu/~yxv4997/t_convert.html

// r,g,b values are from 0 to 1
// h = [0,360], s = [0,1], v = [0,1]
//				if s == 0, then h = -1 (undefined)

// Green Doom guy colors:
// RGB - 0: {    .46  1 .429 } 7: {    .254 .571 .206 } 15: {    .0317 .0794 .0159 }
// HSV - 0: { 116.743 .571 1 } 7: { 112.110 .639 .571 } 15: { 105.071  .800 .0794 }
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v)
{
	float min, max, delta, foo;

	if (r == g && g == b)
	{
		*h = 0;
		*s = 0;
		*v = r;
		return;
	}

	foo = r < g ? r : g;
	min = (foo < b) ? foo : b;
	foo = r > g ? r : g;
	max = (foo > b) ? foo : b;

	*v = max;									// v

	delta = max - min;

	*s = delta / max;							// s

	if (r == max)
		*h = (g - b) / delta;					// between yellow & magenta
	else if (g == max)
		*h = 2 + (b - r) / delta;				// between cyan & yellow
	else
		*h = 4 + (r - g) / delta;				// between magenta & cyan

	*h *= 60;									// degrees
	if (*h < 0)
		*h += 360;
}

void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v)
{
	int i;
	float f, p, q, t;

	if (s == 0)
	{ // achromatic (grey)
		*r = *g = *b = v;
		return;
	}

	h /= 60;									// sector 0 to 5
	i = (int)floor (h);
	f = h - i;									// factorial part of h
	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));

	switch (i)
	{
	case 0:		*r = v; *g = t; *b = p; break;
	case 1:		*r = q; *g = v; *b = p; break;
	case 2:		*r = p; *g = v; *b = t; break;
	case 3:		*r = p; *g = q; *b = v; break;
	case 4:		*r = t; *g = p; *b = v; break;
	default:	*r = v; *g = p; *b = q; break;
	}
}

/****** Colored Lighting Stuffs ******/

FDynamicColormap *GetSpecialLights (PalEntry color, PalEntry fade)
{
	FDynamicColormap *colormap;

	// If this colormap have already been created, just return it
	for (colormap = &NormalLight; colormap != NULL; colormap = colormap->Next)
	{
		if (color == colormap->Color && fade == colormap->Fade)
		{
			return colormap;
		}
	}

	// Not found. Create it.
	colormap = (FDynamicColormap *)Z_Malloc (sizeof(*colormap), PU_LEVEL, 0);
	colormap->Maps = (BYTE *)Z_Malloc (NUMCOLORMAPS*256+255, PU_LEVEL, 0);
	colormap->Maps = (BYTE *)(((ptrdiff_t)colormap->Maps + 255) & ~0xff);
	colormap->Next = NormalLight.Next;
	colormap->Color = color;
	colormap->Fade = fade;
	NormalLight.Next = colormap;

	colormap->BuildLights ();

	return colormap;
}

// Builds NUMCOLORMAPS colormaps lit with the specified color
void FDynamicColormap::BuildLights ()
{
	int l, c;
	int lr, lg, lb;
	PalEntry colors[256];
	BYTE *shade;

	// Scale light to the range 0-256, so we can avoid
	// dividing by 255 in the bottom loop.
	lr = Color.r*256/255;
	lg = Color.g*256/255;
	lb = Color.b*256/255;

	// build normal (but colored) light mappings
	for (l = 0; l < NUMCOLORMAPS; l++)
	{
		DoBlending (GPalette.BaseColors, colors, 256,
			Fade.r, Fade.g, Fade.b, l * (256 / NUMCOLORMAPS));

		shade = Maps + 256*l;
		if ((DWORD)Color == MAKERGB(255,255,255))
		{ // White light, so we can just pick the colors directly
			for (c = 0; c < 256; c++)
			{
				*shade++ = ColorMatcher.Pick (colors[c].r, colors[c].g, colors[c].b);
			}
		}
		else
		{ // Colored light, so do the slower thing
			for (c = 0; c < 256; c++)
			{
				*shade++ = ColorMatcher.Pick (
					(colors[c].r*lr)>>8,
					(colors[c].g*lg)>>8,
					(colors[c].b*lb)>>8);
			}
		}
	}
}

void FDynamicColormap::ChangeColor (PalEntry lightcolor)
{
	if (lightcolor != Color)
	{
		Color = lightcolor;
		BuildLights ();
	}
}

void FDynamicColormap::ChangeFade (PalEntry fadecolor)
{
	if (fadecolor != Fade)
	{
		Fade = fadecolor;
		BuildLights ();
	}
}

void FDynamicColormap::ChangeColorFade (PalEntry lightcolor, PalEntry fadecolor)
{
	if (lightcolor != Color || fadecolor != Fade)
	{
		Color = lightcolor;
		Fade = fadecolor;
		BuildLights ();
	}
}

CCMD (testcolor)
{
	char *colorstring;
	DWORD color;

	if (argv.argc() < 2)
	{
		Printf ("testcolor <color>\n");
	}
	else
	{
		if ( (colorstring = V_GetColorStringByName (argv[1])) )
		{
			color = V_GetColorFromString (NULL, colorstring);
			delete[] colorstring;
		}
		else
		{
			color = V_GetColorFromString (NULL, argv[1]);
		}
		NormalLight.ChangeColor (color);
	}
}
