#ifndef __V_PALETTE_H__
#define __V_PALETTE_H__

#include "doomtype.h"

#define MAKERGB(r,g,b)		(((r)<<16)|((g)<<8)|(b))
#define MAKEARGB(a,r,g,b)	(((a)<<24)|((r)<<16)|((g)<<8)|(b))

#define APART(c)			(((c)>>24)&0xff)
#define RPART(c)			(((c)>>16)&0xff)
#define GPART(c)			(((c)>>8)&0xff)
#define BPART(c)			((c)&0xff)

struct palette_s {
	struct palette_s *next, *prev;

	union {
		// Which of these is used is determined by screen.is8bit

		byte		*colormaps;		// Colormaps for 8-bit graphics
		unsigned	*shades;		// ARGB8888 values for 32-bit graphics
	} maps;
	byte			*colormapsbase;
	union {
		byte		name[8];
		int			nameint[2];
	} name;
	unsigned		*colors;		// gamma corrected colors
	unsigned		*basecolors;	// non-gamma corrected colors
	unsigned		numcolors;
	unsigned		flags;
	unsigned		shadeshift;
	int				usecount;
};
typedef struct palette_s palette_t;

// Generate shading ramps for lighting
#define PALETTEB_SHADE		(0)
#define PALETTEF_SHADE		(1<<PALETTEB_SHADE)

// Apply blend color specified in V_SetBlend()
#define PALETTEB_BLEND		(1)
#define PALETTEF_BLEND		(1<<PALETTEB_SHADE)

// Default palette when none is specified (Do not set directly!)
#define PALETTEB_DEFAULT	(30)
#define PALETTEF_DEFAULT	(1<<PALETTEB_DEFAULT)



// Type values for LoadAttachedPalette():
#define LAP_PALETTE			(~0)	// Just pass thru to LoadPalette()
#define LAP_PATCH			(0)
#define LAP_SPRITE			(1)
#define LAP_FLAT			(2)
#define LAP_TEXTURE			(3)


struct dyncolormap_s {
	byte *maps;
	unsigned int color;
	unsigned int fade;
	struct dyncolormap_s *next;
};
typedef struct dyncolormap_s dyncolormap_t;


// InitPalettes()
//	input: name:  the name of the default palette lump
//				  (normally GAMEPAL)
//
// Returns a pointer to the default palette.
palette_t *InitPalettes (char *name);

// GetDefaultPalette()
//
//	Returns the palette created through InitPalettes()
palette_t *GetDefaultPalette (void);

// MakePalette()
//	input: colors: ptr to 256 3-byte RGB values
//		   name:   the palette's name (not checked for duplicates)
//		   flags:  the flags for the new palette
//
palette_t *MakePalette (byte *colors, char *name, unsigned flags);

// LoadPalette()
//	input: name:  the name of the palette lump
//		   flags: the flags for the palette
//
//	This function will try and find an already loaded
//	palette and return that if possible.
palette_t *LoadPalette (char *name, unsigned flags);

// LoadAttachedPalette()
//	input: name:  the name of a graphic whose palette should be loaded
//		   type:  the type of graphic whose palette is being requested
//		   flags: the flags for the palette
//
//	This function looks through the PALETTES lump for a palette
//	associated with the given graphic and returns that if possible.
palette_t *LoadAttachedPalette (char *name, int type, unsigned flags);

// FreePalette()
//	input: palette: the palette to free
//
//	This function decrements the palette's usecount and frees it
//	when it hits zero.
void FreePalette (palette_t *palette);

// FindPalette()
//	input: name:  the name of the palette
//		   flags: the flags to match on (~0 if it doesn't matter)
//
palette_t *FindPalette (char *name, unsigned flags);

// RefreshPalette()
//	input: pal: the palette to refresh
//
// Generates all colormaps or shadings for the specified palette
// with the current blending levels.
void RefreshPalette (palette_t *pal);

// RefreshPalettes()
//
// Calls RefreshPalette() for all palettes.
void RefreshPalettes (void);

// GammaAdjustPalette()
//
// Builds the colors table for the specified palette based
// on the current gamma correction setting. It will not rebuild
// the shading table if the palette has one.
void GammaAdjustPalette (palette_t *pal);

// GammaAdjustPalettes()
//
// Calls GammaAdjustPalette() for all palettes.
void GammaAdjustPalettes (void);

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

dyncolormap_t *GetSpecialLights (int lr, int lg, int lb, int fr, int fg, int fb);

struct cvar_s;

// Called whenever the gamma cvar changes.
void GammaCallback (struct cvar_s *var);

#endif //__V_PALETTE_H__