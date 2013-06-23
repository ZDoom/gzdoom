#ifndef __GL_LIGHTDATA
#define __GL_LIGHTDATA

#include "v_palette.h"
#include "r_data/renderstyle.h"
#include "gl/renderer/gl_colormap.h"

bool gl_BrightmapsActive();
bool gl_GlowActive();

inline int gl_ClampLight(int lightlevel)
{
	return clamp(lightlevel, 0, 255);
}

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be);
void gl_SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog);

int gl_CalcLightLevel(int lightlevel, int rellight, bool weapon);
PalEntry gl_CalcLightColor(int light, PalEntry pe, int blendfactor, bool force = false);
void gl_GetLightColor(int lightlevel, int rellight, const FColormap * cm, float * pred, float * pgreen, float * pblue, bool weapon=false);
void gl_SetColor(int light, int rellight, const FColormap * cm, float alpha, PalEntry ThingColor = 0xffffff, bool weapon=false);
void gl_SetColor(int light, int rellight, const FColormap * cm, float *red, float *green, float *blue, PalEntry ThingColor=0xffffff, bool weapon=false);

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

__forceinline void gl_Desaturate(int gray, int ired, int igreen, int iblue, BYTE & red, BYTE & green, BYTE & blue, int fac)
{
	red = (ired*(31-fac) + gray*fac)/31;
	green = (igreen*(31-fac) + gray*fac)/31;
	blue = (iblue*(31-fac) + gray*fac)/31;
}

void gl_ModifyColor(BYTE & red, BYTE & green, BYTE & blue, int cm);
void gl_DeleteAllAttachedLights();
void gl_RecreateAllAttachedLights();

extern int fogdensity;
extern int outsidefogdensity;
extern int skyfog;


#endif