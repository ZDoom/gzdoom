#ifndef __GL_LIGHTDATA
#define __GL_LIGHTDATA

#include "v_palette.h"
#include "p_3dfloors.h"
#include "r_data/renderstyle.h"
#include "gl/renderer/gl_colormap.h"

inline int gl_ClampLight(int lightlevel)
{
	return clamp(lightlevel, 0, 255);
}

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be);

int gl_CalcLightLevel(int lightlevel, int rellight, bool weapon);
void gl_SetColor(int light, int rellight, bool fullbright, const FColormap &cm, float alpha, bool weapon=false);

float gl_GetFogDensity(int lightlevel, PalEntry fogcolor, int sectorfogdensity);
struct sector_t;
bool gl_CheckFog(sector_t *frontsector, sector_t *backsector);

void gl_SetFog(int lightlevel, int rellight, bool fullbright, const FColormap *cm, bool isadditive);



#endif
