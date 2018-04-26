#ifndef __GL_LIGHTDATA
#define __GL_LIGHTDATA

#include "v_palette.h"
#include "p_3dfloors.h"
#include "r_data/renderstyle.h"
#include "hwrenderer/utility/hw_lighting.h"
#include "r_data/colormaps.h"

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be);

void gl_SetColor(int light, int rellight, bool fullbright, const FColormap &cm, float alpha, bool weapon=false);
void gl_SetFog(int lightlevel, int rellight, bool fullbright, const FColormap *cm, bool isadditive);



#endif
