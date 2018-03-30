#ifndef __GL_COLORMAP
#define __GL_COLORMAP

#include "doomtype.h"
#include "v_palette.h"
#include "r_data/colormaps.h"

struct lightlist_t;

enum EColorManipulation
{
	CM_SPECIAL2D = -3,			// the special colormaps get passed as color pair from the 2D drawer so they need a different value here.
	CM_PLAIN2D = -2,			// regular 2D drawing.
	CM_INVALID=-1,
	CM_DEFAULT=0,					// untranslated
	CM_FIRSTSPECIALCOLORMAP,		// first special fixed colormap
	CM_FIRSTSPECIALCOLORMAPFORCED= 0x08000000,	// first special fixed colormap, application forced (for 2D overlays)

	CM_FOGLAYER = 0x10000000,	// Sprite shaped fog layer

	// These are not to be passed to the texture manager
	CM_LITE	= 0x20000000,		// special values to handle these items without excessive hacking
	CM_TORCH= 0x20000010,		// These are not real color manipulations
};

#define CM_MAXCOLORMAP int(CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size())
#define CM_MAXCOLORMAPFORCED int(CM_FIRSTSPECIALCOLORMAPFORCED + SpecialColormaps.Size())


#endif
