
#pragma once

#include "r_defs.h"
#include "c_cvars.h"
#include <memory>

struct FSWColormap;
struct FLightNode;

EXTERN_CVAR(Bool, r_multithreaded);
EXTERN_CVAR(Bool, r_magfilter);
EXTERN_CVAR(Bool, r_minfilter);
EXTERN_CVAR(Bool, r_mipmap);
EXTERN_CVAR(Float, r_lod_bias);
EXTERN_CVAR(Int, r_drawfuzz);
EXTERN_CVAR(Bool, r_drawtrans);
EXTERN_CVAR(Float, transsouls);
EXTERN_CVAR(Bool, r_dynlights);
EXTERN_CVAR(Bool, r_fuzzscale);

class DrawerCommandQueue;
typedef std::shared_ptr<DrawerCommandQueue> DrawerCommandQueuePtr;

namespace swrenderer
{
	class DrawerArgs;
	class SkyDrawerArgs;
	class WallDrawerArgs;
	class SpanDrawerArgs;
	class SpriteDrawerArgs;
	class VoxelBlock;

	extern uint8_t shadetables[/*NUMCOLORMAPS*16*256*/];
	extern FDynamicColormap ShadeFakeColormap[16];
	extern uint8_t identitymap[256];
	extern FDynamicColormap identitycolormap;

	// Constant arrays used for psprite clipping and initializing clipping.
	extern short zeroarray[MAXWIDTH];
	extern short screenheightarray[MAXWIDTH];

	// Spectre/Invisibility.
	#define FUZZTABLE 50
	#define FUZZ_RANDOM_X_SIZE 100
	extern int fuzzoffset[FUZZTABLE + 1];
	extern int fuzz_random_x_offset[FUZZ_RANDOM_X_SIZE];
	extern int fuzzpos;
	extern int fuzzviewheight;

	#define NUM_PARTICLE_TEXTURES 3
	#define PARTICLE_TEXTURE_SIZE 64
	extern uint32_t particle_texture[NUM_PARTICLE_TEXTURES][PARTICLE_TEXTURE_SIZE * PARTICLE_TEXTURE_SIZE];

	class SWPixelFormatDrawers
	{
	public:
		SWPixelFormatDrawers(DrawerCommandQueuePtr queue) : Queue(queue) { }
		virtual ~SWPixelFormatDrawers() { }
		virtual void DrawWallColumn(const WallDrawerArgs &args) = 0;
		virtual void DrawWallMaskedColumn(const WallDrawerArgs &args) = 0;
		virtual void DrawWallAddColumn(const WallDrawerArgs &args) = 0;
		virtual void DrawWallAddClampColumn(const WallDrawerArgs &args) = 0;
		virtual void DrawWallSubClampColumn(const WallDrawerArgs &args) = 0;
		virtual void DrawWallRevSubClampColumn(const WallDrawerArgs &args) = 0;
		virtual void DrawSingleSkyColumn(const SkyDrawerArgs &args) = 0;
		virtual void DrawDoubleSkyColumn(const SkyDrawerArgs &args) = 0;
		virtual void DrawColumn(const SpriteDrawerArgs &args) = 0;
		virtual void FillColumn(const SpriteDrawerArgs &args) = 0;
		virtual void FillAddColumn(const SpriteDrawerArgs &args) = 0;
		virtual void FillAddClampColumn(const SpriteDrawerArgs &args) = 0;
		virtual void FillSubClampColumn(const SpriteDrawerArgs &args) = 0;
		virtual void FillRevSubClampColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawFuzzColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawAddColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawTranslatedColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawTranslatedAddColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawShadedColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawAddClampShadedColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawAddClampColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawAddClampTranslatedColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawSubClampColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawSubClampTranslatedColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawRevSubClampColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawRevSubClampTranslatedColumn(const SpriteDrawerArgs &args) = 0;
		virtual void DrawVoxelBlocks(const SpriteDrawerArgs &args, const VoxelBlock *blocks, int blockcount) = 0;
		virtual void DrawSpan(const SpanDrawerArgs &args) = 0;
		virtual void DrawSpanMasked(const SpanDrawerArgs &args) = 0;
		virtual void DrawSpanTranslucent(const SpanDrawerArgs &args) = 0;
		virtual void DrawSpanMaskedTranslucent(const SpanDrawerArgs &args) = 0;
		virtual void DrawSpanAddClamp(const SpanDrawerArgs &args) = 0;
		virtual void DrawSpanMaskedAddClamp(const SpanDrawerArgs &args) = 0;
		virtual void FillSpan(const SpanDrawerArgs &args) = 0;
		virtual void DrawTiltedSpan(const SpanDrawerArgs &args, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap) = 0;
		virtual void DrawColoredSpan(const SpanDrawerArgs &args) = 0;
		virtual void DrawFogBoundaryLine(const SpanDrawerArgs &args) = 0;

		void DrawDepthSkyColumn(const SkyDrawerArgs &args, float idepth);
		void DrawDepthWallColumn(const WallDrawerArgs &args, float idepth);
		void DrawDepthSpan(const SpanDrawerArgs &args, float idepth);
		
		DrawerCommandQueuePtr Queue;
	};

	void R_InitShadeMaps();
	void R_InitFuzzTable(int fuzzoff);
	void R_InitParticleTexture();

	void R_UpdateFuzzPosFrameStart();
	void R_UpdateFuzzPos(const SpriteDrawerArgs &args);
}
