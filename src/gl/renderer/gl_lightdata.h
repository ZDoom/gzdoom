#ifndef __GL_LIGHTDATA
#define __GL_LIGHTDATA

#include "v_palette.h"
#include "r_defs.h"
#include "r_data/renderstyle.h"
#include "hwrenderer/utility/hw_lighting.h"
#include "r_data/colormaps.h"

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be);


#endif
