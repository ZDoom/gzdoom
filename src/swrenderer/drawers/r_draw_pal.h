
#pragma once

#include "r_draw.h"
#include "v_palette.h"
#include "r_thread.h"
#include "swrenderer/viewport/r_skydrawer.h"
#include "swrenderer/viewport/r_spandrawer.h"
#include "swrenderer/viewport/r_walldrawer.h"
#include "swrenderer/viewport/r_spritedrawer.h"

namespace swrenderer
{
	class PalWall1Command : public DrawerCommand
	{
	public:
		PalWall1Command(const WallDrawerArgs &args);

	protected:
		inline static uint8_t AddLights(const DrawerLight *lights, int num_lights, float viewpos_z, uint8_t fg, uint8_t material);

		WallDrawerArgs args;
	};

	class DrawWall1PalCommand : public PalWall1Command { public: using PalWall1Command::PalWall1Command; void Execute(DrawerThread *thread) override; };
	class DrawWallMasked1PalCommand : public PalWall1Command { public: using PalWall1Command::PalWall1Command; void Execute(DrawerThread *thread) override; };
	class DrawWallAdd1PalCommand : public PalWall1Command { public: using PalWall1Command::PalWall1Command; void Execute(DrawerThread *thread) override; };
	class DrawWallAddClamp1PalCommand : public PalWall1Command { public: using PalWall1Command::PalWall1Command; void Execute(DrawerThread *thread) override; };
	class DrawWallSubClamp1PalCommand : public PalWall1Command { public: using PalWall1Command::PalWall1Command; void Execute(DrawerThread *thread) override; };
	class DrawWallRevSubClamp1PalCommand : public PalWall1Command { public: using PalWall1Command::PalWall1Command; void Execute(DrawerThread *thread) override; };

	class PalSkyCommand : public DrawerCommand
	{
	public:
		PalSkyCommand(const SkyDrawerArgs &args);

	protected:
		SkyDrawerArgs args;
	};

	class DrawSingleSky1PalCommand : public PalSkyCommand { public: using PalSkyCommand::PalSkyCommand; void Execute(DrawerThread *thread) override; };
	class DrawDoubleSky1PalCommand : public PalSkyCommand { public: using PalSkyCommand::PalSkyCommand; void Execute(DrawerThread *thread) override; };

	class PalColumnCommand : public DrawerCommand
	{
	public:
		PalColumnCommand(const SpriteDrawerArgs &args);

		SpriteDrawerArgs args;

	protected:
		uint8_t AddLights(uint8_t fg, uint8_t material, uint32_t lit_r, uint32_t lit_g, uint32_t lit_b);
	};

	class DrawColumnPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class FillColumnPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class FillColumnAddPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class FillColumnAddClampPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class FillColumnSubClampPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class FillColumnRevSubClampPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnAddPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnTranslatedPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnTlatedAddPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnShadedPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnAddClampShadedPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnAddClampPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnAddClampTranslatedPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnSubClampPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnSubClampTranslatedPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnRevSubClampPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };
	class DrawColumnRevSubClampTranslatedPalCommand : public PalColumnCommand { public: using PalColumnCommand::PalColumnCommand; void Execute(DrawerThread *thread) override; };

	class DrawFuzzColumnPalCommand : public DrawerCommand
	{
	public:
		DrawFuzzColumnPalCommand(const SpriteDrawerArgs &args);
		void Execute(DrawerThread *thread) override;

	private:
		int _yl;
		int _yh;
		int _x;
		uint8_t *_destorg;
		int _pitch;
		int _fuzzpos;
		int _fuzzviewheight;
	};

	class DrawScaledFuzzColumnPalCommand : public DrawerCommand
	{
	public:
		DrawScaledFuzzColumnPalCommand(const SpriteDrawerArgs &drawerargs);
		void Execute(DrawerThread *thread) override;

	private:
		int _x;
		int _yl;
		int _yh;
		uint8_t *_destorg;
		int _pitch;
		int _fuzzpos;
		int _fuzzviewheight;
	};

	class PalSpanCommand : public DrawerCommand
	{
	public:
		PalSpanCommand(const SpanDrawerArgs &args);

	protected:
		inline static uint8_t AddLights(const DrawerLight *lights, int num_lights, float viewpos_x, uint8_t fg, uint8_t material);

		const uint8_t *_source;
		const uint8_t *_colormap;
		uint32_t _xfrac;
		uint32_t _yfrac;
		int _y;
		int _x1;
		int _x2;
		uint8_t *_dest;
		uint32_t _xstep;
		uint32_t _ystep;
		int _srcwidth;
		int _srcheight;
		uint32_t *_srcblend;
		uint32_t *_destblend;
		int _color;
		fixed_t _srcalpha;
		fixed_t _destalpha;
		DrawerLight *_dynlights;
		int _num_dynlights;
		float _viewpos_x;
		float _step_viewpos_x;
	};

	class DrawSpanPalCommand : public PalSpanCommand { public: using PalSpanCommand::PalSpanCommand; void Execute(DrawerThread *thread) override; };
	class DrawSpanMaskedPalCommand : public PalSpanCommand { public: using PalSpanCommand::PalSpanCommand; void Execute(DrawerThread *thread) override; };
	class DrawSpanTranslucentPalCommand : public PalSpanCommand { public: using PalSpanCommand::PalSpanCommand; void Execute(DrawerThread *thread) override; };
	class DrawSpanMaskedTranslucentPalCommand : public PalSpanCommand { public: using PalSpanCommand::PalSpanCommand; void Execute(DrawerThread *thread) override; };
	class DrawSpanAddClampPalCommand : public PalSpanCommand { public: using PalSpanCommand::PalSpanCommand; void Execute(DrawerThread *thread) override; };
	class DrawSpanMaskedAddClampPalCommand : public PalSpanCommand { public: using PalSpanCommand::PalSpanCommand; void Execute(DrawerThread *thread) override; };
	class FillSpanPalCommand : public PalSpanCommand { public: using PalSpanCommand::PalSpanCommand; void Execute(DrawerThread *thread) override; };

	class DrawTiltedSpanPalCommand : public DrawerCommand
	{
	public:
		DrawTiltedSpanPalCommand(const SpanDrawerArgs &args, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap);
		void Execute(DrawerThread *thread) override;

	private:
		void CalcTiltedLighting(double lval, double lend, int width, DrawerThread *thread);

		int y;
		int x1;
		int x2;
		FVector3 plane_sz;
		FVector3 plane_su;
		FVector3 plane_sv;
		bool plane_shade;
		int planeshade;
		float planelightfloat;
		fixed_t pviewx;
		fixed_t pviewy;

		const uint8_t *_colormap;
		uint8_t *_dest;
		int _ybits;
		int _xbits;
		const uint8_t *_source;
		uint8_t *basecolormapdata;
		RenderViewport *viewport;
	};

	class DrawColoredSpanPalCommand : public PalSpanCommand
	{
	public:
		DrawColoredSpanPalCommand(const SpanDrawerArgs &args);
		void Execute(DrawerThread *thread) override;

	private:
		int y;
		int x1;
		int x2;
		int color;
		uint8_t *dest;
	};

	class DrawFogBoundaryLinePalCommand : public PalSpanCommand
	{
	public:
		DrawFogBoundaryLinePalCommand(const SpanDrawerArgs &args);
		void Execute(DrawerThread *thread) override;

	private:
		int y, x1, x2;
		const uint8_t *_colormap;
		uint8_t *_dest;
	};
	
	class DrawParticleColumnPalCommand : public DrawerCommand
	{
	public:
		DrawParticleColumnPalCommand(uint8_t *dest, int dest_y, int pitch, int count, uint32_t fg, uint32_t alpha, uint32_t fracposx);
		void Execute(DrawerThread *thread) override;

	private:
		uint8_t *_dest;
		int _dest_y;
		int _pitch;
		int _count;
		uint32_t _fg;
		uint32_t _alpha;
		uint32_t _fracposx;
	};

	class DrawVoxelBlocksPalCommand : public DrawerCommand
	{
	public:
		DrawVoxelBlocksPalCommand(const SpriteDrawerArgs &args, const VoxelBlock *blocks, int blockcount);
		void Execute(DrawerThread *thread) override;

	private:
		SpriteDrawerArgs args;
		const VoxelBlock *blocks;
		int blockcount;
	};

	class SWPalDrawers : public SWPixelFormatDrawers
	{
	public:
		using SWPixelFormatDrawers::SWPixelFormatDrawers;
		
		void DrawWallColumn(const WallDrawerArgs &args) override { Queue->Push<DrawWall1PalCommand>(args); }
		void DrawWallMaskedColumn(const WallDrawerArgs &args) override { Queue->Push<DrawWallMasked1PalCommand>(args); }

		void DrawWallAddColumn(const WallDrawerArgs &args) override
		{
			if (args.dc_num_lights == 0)
				Queue->Push<DrawWallAdd1PalCommand>(args);
			else
				Queue->Push<DrawWallAddClamp1PalCommand>(args);
		}

		void DrawWallAddClampColumn(const WallDrawerArgs &args) override { Queue->Push<DrawWallAddClamp1PalCommand>(args); }
		void DrawWallSubClampColumn(const WallDrawerArgs &args) override { Queue->Push<DrawWallSubClamp1PalCommand>(args); }
		void DrawWallRevSubClampColumn(const WallDrawerArgs &args) override { Queue->Push<DrawWallRevSubClamp1PalCommand>(args); }
		void DrawSingleSkyColumn(const SkyDrawerArgs &args) override { Queue->Push<DrawSingleSky1PalCommand>(args); }
		void DrawDoubleSkyColumn(const SkyDrawerArgs &args) override { Queue->Push<DrawDoubleSky1PalCommand>(args); }
		void DrawColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnPalCommand>(args); }
		void FillColumn(const SpriteDrawerArgs &args) override { Queue->Push<FillColumnPalCommand>(args); }
		void FillAddColumn(const SpriteDrawerArgs &args) override { Queue->Push<FillColumnAddPalCommand>(args); }
		void FillAddClampColumn(const SpriteDrawerArgs &args) override { Queue->Push<FillColumnAddClampPalCommand>(args); }
		void FillSubClampColumn(const SpriteDrawerArgs &args) override { Queue->Push<FillColumnSubClampPalCommand>(args); }
		void FillRevSubClampColumn(const SpriteDrawerArgs &args) override { Queue->Push<FillColumnRevSubClampPalCommand>(args); }
		void DrawFuzzColumn(const SpriteDrawerArgs &args) override
		{
			if (r_fuzzscale)
				Queue->Push<DrawScaledFuzzColumnPalCommand>(args);
			else
				Queue->Push<DrawFuzzColumnPalCommand>(args);
			R_UpdateFuzzPos(args);
		}
		void DrawAddColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnAddPalCommand>(args); }
		void DrawTranslatedColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnTranslatedPalCommand>(args); }
		void DrawTranslatedAddColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnTlatedAddPalCommand>(args); }
		void DrawShadedColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnShadedPalCommand>(args); }
		void DrawAddClampShadedColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnAddClampShadedPalCommand>(args); }
		void DrawAddClampColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnAddClampPalCommand>(args); }
		void DrawAddClampTranslatedColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnAddClampTranslatedPalCommand>(args); }
		void DrawSubClampColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnSubClampPalCommand>(args); }
		void DrawSubClampTranslatedColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnSubClampTranslatedPalCommand>(args); }
		void DrawRevSubClampColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnRevSubClampPalCommand>(args); }
		void DrawRevSubClampTranslatedColumn(const SpriteDrawerArgs &args) override { Queue->Push<DrawColumnRevSubClampTranslatedPalCommand>(args); }
		void DrawVoxelBlocks(const SpriteDrawerArgs &args, const VoxelBlock *blocks, int blockcount) override { Queue->Push<DrawVoxelBlocksPalCommand>(args, blocks, blockcount); }
		void DrawSpan(const SpanDrawerArgs &args) override { Queue->Push<DrawSpanPalCommand>(args); }
		void DrawSpanMasked(const SpanDrawerArgs &args) override { Queue->Push<DrawSpanMaskedPalCommand>(args); }
		void DrawSpanTranslucent(const SpanDrawerArgs &args) override { Queue->Push<DrawSpanTranslucentPalCommand>(args); }
		void DrawSpanMaskedTranslucent(const SpanDrawerArgs &args) override { Queue->Push<DrawSpanMaskedTranslucentPalCommand>(args); }
		void DrawSpanAddClamp(const SpanDrawerArgs &args) override { Queue->Push<DrawSpanAddClampPalCommand>(args); }
		void DrawSpanMaskedAddClamp(const SpanDrawerArgs &args) override { Queue->Push<DrawSpanMaskedAddClampPalCommand>(args); }
		void FillSpan(const SpanDrawerArgs &args) override { Queue->Push<FillSpanPalCommand>(args); }

		void DrawTiltedSpan(const SpanDrawerArgs &args, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap) override
		{
			Queue->Push<DrawTiltedSpanPalCommand>(args, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy, basecolormap);
		}

		void DrawColoredSpan(const SpanDrawerArgs &args) override { Queue->Push<DrawColoredSpanPalCommand>(args); }
		void DrawFogBoundaryLine(const SpanDrawerArgs &args) override { Queue->Push<DrawFogBoundaryLinePalCommand>(args); }
	};
}
