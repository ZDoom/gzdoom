#ifndef __V_PALETTE_H__
#define __V_PALETTE_H__

#include "doomtype.h"
#include "r_main.h"

#define MAKERGB(r,g,b)		(((r)<<16)|((g)<<8)|(b))
#define MAKEARGB(a,r,g,b)	(((a)<<24)|((r)<<16)|((g)<<8)|(b))

#define APART(c)			(((c)>>24)&0xff)
#define RPART(c)			(((c)>>16)&0xff)
#define GPART(c)			(((c)>>8)&0xff)
#define BPART(c)			((c)&0xff)

struct PalEntry
{
	PalEntry () {}
	PalEntry (BYTE ir, BYTE ig, BYTE ib) : a(0), r(ir), g(ig), b(ib) {}
	PalEntry (BYTE ia, BYTE ir, BYTE ig, BYTE ib) : a(ia), r(ir), g(ig), b(ib) {}
	PalEntry (DWORD argb) { *(DWORD *)this = argb; }
	operator DWORD () { return *(DWORD *)this; }
	PalEntry &operator= (DWORD other) { *(DWORD *)this = other; return *this; }

#ifdef __BIG_ENDIAN__
	BYTE a,r,g,b;
#else
	BYTE b,g,r,a;
#endif
};

inline FArchive &operator<< (FArchive &arc, PalEntry &p)
{
	return arc << p.a << p.r << p.g << p.b;
}

struct FPalette
{
	FPalette ();
	FPalette (BYTE *colors);

	void SetPalette (BYTE *colors);
	void GammaAdjust ();

	PalEntry	Colors[256];		// gamma corrected palette
	PalEntry	BaseColors[256];	// non-gamma corrected palette
};

struct FDynamicColormap
{
	void ChangeFade (PalEntry fadecolor);
	void ChangeColor (PalEntry lightcolor);
	void ChangeColorFade (PalEntry lightcolor, PalEntry fadecolor);
	void BuildLights ();

	BYTE *Maps;
	PalEntry Color;
	PalEntry Fade;
	FDynamicColormap *Next;
};

extern BYTE *InvulnerabilityColormap;
extern FPalette GPalette;
extern "C" {
extern FDynamicColormap NormalLight;
}
extern int Near0;		// A color near 0 in appearance, but not 0

int BestColor (const DWORD *pal, int r, int g, int b, int first = 0);

void InitPalette ();

// V_SetBlend()
//	input: blendr: red component of blend
//		   blendg: green component of blend
//		   blendb: blue component of blend
//		   blenda: alpha component of blend
//
// Applies the blend to all palettes with PALETTEF_BLEND flag
void V_SetBlend (int blendr, int blendg, int blendb, int blenda);

// V_ForceBlend()
//
// Normally, V_SetBlend() does nothing if the new blend is the
// same as the old. This function will performing the blending
// even if the blend hasn't changed.
void V_ForceBlend (int blendr, int blendg, int blendb, int blenda);


// Colorspace conversion RGB <-> HSV
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v);
void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v);

FDynamicColormap *GetSpecialLights (PalEntry lightcolor, PalEntry fadecolor);

#endif //__V_PALETTE_H__
