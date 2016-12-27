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
void gl_SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog);

int gl_CalcLightLevel(int lightlevel, int rellight, bool weapon);
void gl_SetColor(int light, int rellight, const FColormap &cm, float alpha, bool weapon=false);

float gl_GetFogDensity(int lightlevel, PalEntry fogcolor);
struct sector_t;
bool gl_CheckFog(FColormap *cm, int lightlevel);
bool gl_CheckFog(sector_t *frontsector, sector_t *backsector);

void gl_SetFog(int lightlevel, int rellight, const FColormap *cm, bool isadditive);

inline bool gl_isBlack(PalEntry color)
{
	return color.r + color.g + color.b == 0;
}

inline bool gl_isWhite(PalEntry color)
{
	return color.r + color.g + color.b == 3*0xff;
}

extern DWORD gl_fixedcolormap;

inline bool gl_isFullbright(PalEntry color, int lightlevel)
{
	return gl_fixedcolormap || (gl_isWhite(color) && lightlevel==255);
}

void gl_DeleteAllAttachedLights();
void gl_RecreateAllAttachedLights();

extern int fogdensity;
extern int outsidefogdensity;
extern int skyfog;


inline void FColormap::CopyFrom3DLight(lightlist_t *light)
{
	LightColor = light->extra_colormap->Color;
	desaturation = light->extra_colormap->Desaturate;
	blendfactor = light->extra_colormap->Color.a;
	if (light->caster && (light->caster->flags&FF_FADEWALLS) && (light->extra_colormap->Fade & 0xffffff) != 0)
		FadeColor = light->extra_colormap->Fade;
}



#endif