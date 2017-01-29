
#pragma once

#include "r_defs.h"

struct FSWColormap;
struct FLightNode;
struct TriLight;

EXTERN_CVAR(Bool, r_multithreaded);
EXTERN_CVAR(Bool, r_magfilter);
EXTERN_CVAR(Bool, r_minfilter);
EXTERN_CVAR(Bool, r_mipmap);
EXTERN_CVAR(Float, r_lod_bias);
EXTERN_CVAR(Int, r_drawfuzz);
EXTERN_CVAR(Bool, r_drawtrans);
EXTERN_CVAR(Float, transsouls);
EXTERN_CVAR(Bool, r_dynlights);

namespace swrenderer
{
	class DrawerArgs;

	extern int ylookup[MAXHEIGHT];
	extern uint8_t shadetables[/*NUMCOLORMAPS*16*256*/];
	extern FDynamicColormap ShadeFakeColormap[16];
	extern uint8_t identitymap[256];
	extern FDynamicColormap identitycolormap;

	// Constant arrays used for psprite clipping and initializing clipping.
	extern short zeroarray[MAXWIDTH];
	extern short screenheightarray[MAXWIDTH];

	// Spectre/Invisibility.
	#define FUZZTABLE 50
	extern int fuzzoffset[FUZZTABLE + 1];
	extern int fuzzpos;
	extern int fuzzviewheight;

	#define PARTICLE_TEXTURE_SIZE 64
	extern uint32_t particle_texture[PARTICLE_TEXTURE_SIZE * PARTICLE_TEXTURE_SIZE];

	extern bool r_swtruecolor;

	class SWPixelFormatDrawers
	{
	public:
		virtual ~SWPixelFormatDrawers() { }
		virtual void DrawWallColumn(const DrawerArgs &args) = 0;
		virtual void DrawWallMaskedColumn(const DrawerArgs &args) = 0;
		virtual void DrawWallAddColumn(const DrawerArgs &args) = 0;
		virtual void DrawWallAddClampColumn(const DrawerArgs &args) = 0;
		virtual void DrawWallSubClampColumn(const DrawerArgs &args) = 0;
		virtual void DrawWallRevSubClampColumn(const DrawerArgs &args) = 0;
		virtual void DrawSingleSkyColumn(const DrawerArgs &args, uint32_t solid_top, uint32_t solid_bottom, bool fadeSky) = 0;
		virtual void DrawDoubleSkyColumn(const DrawerArgs &args, uint32_t solid_top, uint32_t solid_bottom, bool fadeSky) = 0;
		virtual void DrawColumn(const DrawerArgs &args) = 0;
		virtual void FillColumn(const DrawerArgs &args) = 0;
		virtual void FillAddColumn(const DrawerArgs &args) = 0;
		virtual void FillAddClampColumn(const DrawerArgs &args) = 0;
		virtual void FillSubClampColumn(const DrawerArgs &args) = 0;
		virtual void FillRevSubClampColumn(const DrawerArgs &args) = 0;
		virtual void DrawFuzzColumn(const DrawerArgs &args) = 0;
		virtual void DrawAddColumn(const DrawerArgs &args) = 0;
		virtual void DrawTranslatedColumn(const DrawerArgs &args) = 0;
		virtual void DrawTranslatedAddColumn(const DrawerArgs &args) = 0;
		virtual void DrawShadedColumn(const DrawerArgs &args) = 0;
		virtual void DrawAddClampColumn(const DrawerArgs &args) = 0;
		virtual void DrawAddClampTranslatedColumn(const DrawerArgs &args) = 0;
		virtual void DrawSubClampColumn(const DrawerArgs &args) = 0;
		virtual void DrawSubClampTranslatedColumn(const DrawerArgs &args) = 0;
		virtual void DrawRevSubClampColumn(const DrawerArgs &args) = 0;
		virtual void DrawRevSubClampTranslatedColumn(const DrawerArgs &args) = 0;
		virtual void DrawSpan(const DrawerArgs &args) = 0;
		virtual void DrawSpanMasked(const DrawerArgs &args) = 0;
		virtual void DrawSpanTranslucent(const DrawerArgs &args) = 0;
		virtual void DrawSpanMaskedTranslucent(const DrawerArgs &args) = 0;
		virtual void DrawSpanAddClamp(const DrawerArgs &args) = 0;
		virtual void DrawSpanMaskedAddClamp(const DrawerArgs &args) = 0;
		virtual void FillSpan(const DrawerArgs &args) = 0;
		virtual void DrawTiltedSpan(const DrawerArgs &args, int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap) = 0;
		virtual void DrawColoredSpan(const DrawerArgs &args, int y, int x1, int x2) = 0;
		virtual void DrawFogBoundaryLine(const DrawerArgs &args, int y, int x1, int x2) = 0;
	};

	void R_InitShadeMaps();
	void R_InitFuzzTable(int fuzzoff);
	void R_InitParticleTexture();

	void R_UpdateFuzzPos(const DrawerArgs &args);
}
