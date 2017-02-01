
#pragma once

#include "r_draw.h"
#include "v_palette.h"
#include "r_thread.h"
#include "r_drawerargs.h"

namespace swrenderer
{
	class PalWall1Command : public DrawerCommand
	{
	public:
		PalWall1Command(const WallDrawerArgs &args);
		FString DebugInfo() override { return "PalWallCommand"; }

	protected:
		inline static uint8_t AddLights(const TriLight *lights, int num_lights, float viewpos_z, uint8_t fg, uint8_t material);

		uint32_t _iscale;
		uint32_t _texturefrac;
		uint8_t *_colormap;
		int _count;
		const uint8_t *_source;
		uint8_t *_dest;
		int _dest_y;
		int _fracbits;
		int _pitch;
		uint32_t *_srcblend;
		uint32_t *_destblend;
		TriLight *_dynlights;
		int _num_dynlights;
		float _viewpos_z;
		float _step_viewpos_z;
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
		FString DebugInfo() override { return "PalSkyCommand"; }

	protected:
		uint32_t solid_top;
		uint32_t solid_bottom;
		bool fadeSky;

		uint8_t *_dest;
		int _dest_y;
		int _count;
		int _pitch;
		const uint8_t *_source;
		const uint8_t *_source2;
		int _sourceheight[2];
		uint32_t _iscale;
		uint32_t _texturefrac;
	};

	class DrawSingleSky1PalCommand : public PalSkyCommand { public: using PalSkyCommand::PalSkyCommand; void Execute(DrawerThread *thread) override; };
	class DrawDoubleSky1PalCommand : public PalSkyCommand { public: using PalSkyCommand::PalSkyCommand; void Execute(DrawerThread *thread) override; };

	class PalColumnCommand : public DrawerCommand
	{
	public:
		PalColumnCommand(const SpriteDrawerArgs &args);
		FString DebugInfo() override { return "PalColumnCommand"; }

	protected:
		int _count;
		uint8_t *_dest;
		int _dest_y;
		int _pitch;
		fixed_t _iscale;
		fixed_t _texturefrac;
		const uint8_t *_colormap;
		const uint8_t *_source;
		const uint8_t *_translation;
		int _color;
		uint32_t *_srcblend;
		uint32_t *_destblend;
		uint32_t _srccolor;
		fixed_t _srcalpha;
		fixed_t _destalpha;
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
		FString DebugInfo() override { return "DrawFuzzColumnPalCommand"; }

	private:
		int _yl;
		int _yh;
		int _x;
		uint8_t *_destorg;
		int _pitch;
		int _fuzzpos;
		int _fuzzviewheight;
	};

	class PalSpanCommand : public DrawerCommand
	{
	public:
		PalSpanCommand(const SpanDrawerArgs &args);
		FString DebugInfo() override { return "PalSpanCommand"; }

	protected:
		inline static uint8_t AddLights(const TriLight *lights, int num_lights, float viewpos_x, uint8_t fg, uint8_t material);

		const uint8_t *_source;
		const uint8_t *_colormap;
		dsfixed_t _xfrac;
		dsfixed_t _yfrac;
		int _y;
		int _x1;
		int _x2;
		uint8_t *_dest;
		dsfixed_t _xstep;
		dsfixed_t _ystep;
		int _xbits;
		int _ybits;
		uint32_t *_srcblend;
		uint32_t *_destblend;
		int _color;
		fixed_t _srcalpha;
		fixed_t _destalpha;
		TriLight *_dynlights;
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
		DrawTiltedSpanPalCommand(const SpanDrawerArgs &args, int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override { return "DrawTiltedSpanPalCommand"; }

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
	};

	class DrawColoredSpanPalCommand : public PalSpanCommand
	{
	public:
		DrawColoredSpanPalCommand(const SpanDrawerArgs &args, int y, int x1, int x2);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override { return "DrawColoredSpanPalCommand"; }

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
		DrawFogBoundaryLinePalCommand(const SpanDrawerArgs &args, int y, int x1, int x2);
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
		FString DebugInfo() override;

	private:
		uint8_t *_dest;
		int _dest_y;
		int _pitch;
		int _count;
		uint32_t _fg;
		uint32_t _alpha;
		uint32_t _fracposx;
	};

	class SWPalDrawers : public SWPixelFormatDrawers
	{
	public:
		void DrawWallColumn(const WallDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawWall1PalCommand>(args); }
		void DrawWallMaskedColumn(const WallDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawWallMasked1PalCommand>(args); }

		void DrawWallAddColumn(const WallDrawerArgs &args) override
		{
			if (args.dc_num_lights == 0)
				DrawerCommandQueue::QueueCommand<DrawWallAdd1PalCommand>(args);
			else
				DrawerCommandQueue::QueueCommand<DrawWallAddClamp1PalCommand>(args);
		}

		void DrawWallAddClampColumn(const WallDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawWallAddClamp1PalCommand>(args); }
		void DrawWallSubClampColumn(const WallDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawWallSubClamp1PalCommand>(args); }
		void DrawWallRevSubClampColumn(const WallDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawWallRevSubClamp1PalCommand>(args); }
		void DrawSingleSkyColumn(const SkyDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawSingleSky1PalCommand>(args); }
		void DrawDoubleSkyColumn(const SkyDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawDoubleSky1PalCommand>(args); }
		void DrawColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnPalCommand>(args); }
		void FillColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<FillColumnPalCommand>(args); }
		void FillAddColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<FillColumnAddPalCommand>(args); }
		void FillAddClampColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<FillColumnAddClampPalCommand>(args); }
		void FillSubClampColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<FillColumnSubClampPalCommand>(args); }
		void FillRevSubClampColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<FillColumnRevSubClampPalCommand>(args); }
		void DrawFuzzColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawFuzzColumnPalCommand>(args); R_UpdateFuzzPos(args); }
		void DrawAddColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnAddPalCommand>(args); }
		void DrawTranslatedColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnTranslatedPalCommand>(args); }
		void DrawTranslatedAddColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnTlatedAddPalCommand>(args); }
		void DrawShadedColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnShadedPalCommand>(args); }
		void DrawAddClampColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnAddClampPalCommand>(args); }
		void DrawAddClampTranslatedColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnAddClampTranslatedPalCommand>(args); }
		void DrawSubClampColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnSubClampPalCommand>(args); }
		void DrawSubClampTranslatedColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnSubClampTranslatedPalCommand>(args); }
		void DrawRevSubClampColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampPalCommand>(args); }
		void DrawRevSubClampTranslatedColumn(const SpriteDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampTranslatedPalCommand>(args); }
		void DrawSpan(const SpanDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawSpanPalCommand>(args); }
		void DrawSpanMasked(const SpanDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawSpanMaskedPalCommand>(args); }
		void DrawSpanTranslucent(const SpanDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawSpanTranslucentPalCommand>(args); }
		void DrawSpanMaskedTranslucent(const SpanDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawSpanMaskedTranslucentPalCommand>(args); }
		void DrawSpanAddClamp(const SpanDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawSpanAddClampPalCommand>(args); }
		void DrawSpanMaskedAddClamp(const SpanDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<DrawSpanMaskedAddClampPalCommand>(args); }
		void FillSpan(const SpanDrawerArgs &args) override { DrawerCommandQueue::QueueCommand<FillSpanPalCommand>(args); }

		void DrawTiltedSpan(const SpanDrawerArgs &args, int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap) override
		{
			DrawerCommandQueue::QueueCommand<DrawTiltedSpanPalCommand>(args, y, x1, x2, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy, basecolormap);
		}

		void DrawColoredSpan(const SpanDrawerArgs &args, int y, int x1, int x2) override { DrawerCommandQueue::QueueCommand<DrawColoredSpanPalCommand>(args, y, x1, x2); }
		void DrawFogBoundaryLine(const SpanDrawerArgs &args, int y, int x1, int x2) override { DrawerCommandQueue::QueueCommand<DrawFogBoundaryLinePalCommand>(args, y, x1, x2); }
	};
}
