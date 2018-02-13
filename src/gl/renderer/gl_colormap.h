#ifndef __GL_COLORMAP
#define __GL_COLORMAP

#include "doomtype.h"
#include "v_palette.h"
#include "r_data/colormaps.h"

struct lightlist_t;

enum EColorManipulation
{

	CM_INVALID=-1,
	CM_DEFAULT=0,					// untranslated
	CM_FIRSTSPECIALCOLORMAP,		// first special fixed colormap

	CM_FOGLAYER = 0x10000000,	// Sprite shaped fog layer

	// These are not to be passed to the texture manager
	CM_LITE	= 0x20000000,		// special values to handle these items without excessive hacking
	CM_TORCH= 0x20000010,		// These are not real color manipulations
};

#define CM_MAXCOLORMAP int(CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size())


#endif
