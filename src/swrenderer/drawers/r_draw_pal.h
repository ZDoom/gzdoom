
#pragma once

#include "r_draw.h"
#include "v_palette.h"
#include "r_thread.h"

namespace swrenderer
{
	class PalWall1Command : public DrawerCommand
	{
	public:
		PalWall1Command();
		FString DebugInfo() override { return "PalWallCommand"; }

	protected:
		inline static uint8_t AddLights(const TriLight *lights, int num_lights, float viewpos_z, uint8_t fg, uint8_t material);

		uint32_t _iscale;
		uint32_t _texturefrac;
		uint8_t *_colormap;
		int _count;
		const uint8_t *_source;
		uint8_t *_dest;
		int _fracbits;
		int _pitch;
		uint32_t *_srcblend;
		uint32_t *_destblend;
		TriLight *_dynlights;
		int _num_dynlights;
		float _viewpos_z;
		float _step_viewpos_z;
	};

	class DrawWall1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallMasked1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallAdd1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallAddClamp1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallSubClamp1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallRevSubClamp1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };

	class PalSkyCommand : public DrawerCommand
	{
	public:
		PalSkyCommand(uint32_t solid_top, uint32_t solid_bottom);
		FString DebugInfo() override { return "PalSkyCommand"; }

	protected:
		uint32_t solid_top;
		uint32_t solid_bottom;

		uint8_t *_dest;
		int _count;
		int _pitch;
		const uint8_t *_source[4];
		const uint8_t *_source2[4];
		int _sourceheight[4];
		uint32_t _iscale[4];
		uint32_t _texturefrac[4];
	};

	class DrawSingleSky1PalCommand : public PalSkyCommand { public: using PalSkyCommand::PalSkyCommand; void Execute(DrawerThread *thread) override; };
	class DrawDoubleSky1PalCommand : public PalSkyCommand { public: using PalSkyCommand::PalSkyCommand; void Execute(DrawerThread *thread) override; };

	class PalColumnCommand : public DrawerCommand
	{
	public:
		PalColumnCommand();
		FString DebugInfo() override { return "PalColumnCommand"; }

	protected:
		int _count;
		uint8_t *_dest;
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

	class DrawColumnPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class FillColumnPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class FillColumnAddPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class FillColumnAddClampPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class FillColumnSubClampPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class FillColumnRevSubClampPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnAddPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnTranslatedPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnTlatedAddPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnShadedPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnAddClampPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnAddClampTranslatedPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnSubClampPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnSubClampTranslatedPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnRevSubClampPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawColumnRevSubClampTranslatedPalCommand : public PalColumnCommand { public: void Execute(DrawerThread *thread) override; };

	class DrawFuzzColumnPalCommand : public DrawerCommand
	{
	public:
		DrawFuzzColumnPalCommand();
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
		PalSpanCommand();
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
		uint8_t *_destorg;
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

	class DrawSpanPalCommand : public PalSpanCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawSpanMaskedPalCommand : public PalSpanCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawSpanTranslucentPalCommand : public PalSpanCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawSpanMaskedTranslucentPalCommand : public PalSpanCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawSpanAddClampPalCommand : public PalSpanCommand { public: void Execute(DrawerThread *thread) override; };
	class DrawSpanMaskedAddClampPalCommand : public PalSpanCommand { public: void Execute(DrawerThread *thread) override; };
	class FillSpanPalCommand : public PalSpanCommand { public: void Execute(DrawerThread *thread) override; };

	class DrawTiltedSpanPalCommand : public DrawerCommand
	{
	public:
		DrawTiltedSpanPalCommand(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap);
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
		uint8_t *_destorg;
		int _ybits;
		int _xbits;
		const uint8_t *_source;
		uint8_t *basecolormapdata;
	};

	class DrawColoredSpanPalCommand : public PalSpanCommand
	{
	public:
		DrawColoredSpanPalCommand(int y, int x1, int x2);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override { return "DrawColoredSpanPalCommand"; }

	private:
		int y;
		int x1;
		int x2;
		int color;
		uint8_t *destorg;
	};

	class DrawFogBoundaryLinePalCommand : public PalSpanCommand
	{
	public:
		DrawFogBoundaryLinePalCommand(int y, int x1, int x2);
		void Execute(DrawerThread *thread) override;

	private:
		int y, x1, x2;
		const uint8_t *_colormap;
		uint8_t *_destorg;
	};
	
	class DrawParticleColumnPalCommand : public DrawerCommand
	{
	public:
		DrawParticleColumnPalCommand(uint8_t *dest, int dest_y, int pitch, int count, uint32_t fg, uint32_t alpha, uint32_t fracposx);
		void Execute(DrawerThread *thread) override;
		FString DebugInfo() override;

	private:
		uint8_t *_dest;
		int _pitch;
		int _count;
		uint32_t _fg;
		uint32_t _alpha;
		uint32_t _fracposx;
	};

	class SWPalDrawers : public SWPixelFormatDrawers
	{
	public:
		void DrawWallColumn() override { DrawerCommandQueue::QueueCommand<DrawWall1PalCommand>(); }
		void DrawWallMaskedColumn() override { DrawerCommandQueue::QueueCommand<DrawWallMasked1PalCommand>(); }

		void DrawWallAddColumn() override
		{
			if (drawerargs::dc_num_lights == 0)
				DrawerCommandQueue::QueueCommand<DrawWallAdd1PalCommand>();
			else
				DrawerCommandQueue::QueueCommand<DrawWallAddClamp1PalCommand>();
		}

		void DrawWallAddClampColumn() override { DrawerCommandQueue::QueueCommand<DrawWallAddClamp1PalCommand>(); }
		void DrawWallSubClampColumn() override { DrawerCommandQueue::QueueCommand<DrawWallSubClamp1PalCommand>(); }
		void DrawWallRevSubClampColumn() override { DrawerCommandQueue::QueueCommand<DrawWallRevSubClamp1PalCommand>(); }
		void DrawSingleSkyColumn(uint32_t solid_top, uint32_t solid_bottom) override { DrawerCommandQueue::QueueCommand<DrawSingleSky1PalCommand>(solid_top, solid_bottom); }
		void DrawDoubleSkyColumn(uint32_t solid_top, uint32_t solid_bottom) override { DrawerCommandQueue::QueueCommand<DrawDoubleSky1PalCommand>(solid_top, solid_bottom); }
		void DrawColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnPalCommand>(); }
		void FillColumn() override { DrawerCommandQueue::QueueCommand<FillColumnPalCommand>(); }
		void FillAddColumn() override { DrawerCommandQueue::QueueCommand<FillColumnAddPalCommand>(); }
		void FillAddClampColumn() override { DrawerCommandQueue::QueueCommand<FillColumnAddClampPalCommand>(); }
		void FillSubClampColumn() override { DrawerCommandQueue::QueueCommand<FillColumnSubClampPalCommand>(); }
		void FillRevSubClampColumn() override { DrawerCommandQueue::QueueCommand<FillColumnRevSubClampPalCommand>(); }
		void DrawFuzzColumn() override { DrawerCommandQueue::QueueCommand<DrawFuzzColumnPalCommand>(); R_UpdateFuzzPos(); }
		void DrawAddColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnAddPalCommand>(); }
		void DrawTranslatedColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnTranslatedPalCommand>(); }
		void DrawTranslatedAddColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnTlatedAddPalCommand>(); }
		void DrawShadedColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnShadedPalCommand>(); }
		void DrawAddClampColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnAddClampPalCommand>(); }
		void DrawAddClampTranslatedColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnAddClampTranslatedPalCommand>(); }
		void DrawSubClampColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnSubClampPalCommand>(); }
		void DrawSubClampTranslatedColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnSubClampTranslatedPalCommand>(); }
		void DrawRevSubClampColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampPalCommand>(); }
		void DrawRevSubClampTranslatedColumn() override { DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampTranslatedPalCommand>(); }
		void DrawSpan() override { DrawerCommandQueue::QueueCommand<DrawSpanPalCommand>(); }
		void DrawSpanMasked() override { DrawerCommandQueue::QueueCommand<DrawSpanMaskedPalCommand>(); }
		void DrawSpanTranslucent() override { DrawerCommandQueue::QueueCommand<DrawSpanTranslucentPalCommand>(); }
		void DrawSpanMaskedTranslucent() override { DrawerCommandQueue::QueueCommand<DrawSpanMaskedTranslucentPalCommand>(); }
		void DrawSpanAddClamp() override { DrawerCommandQueue::QueueCommand<DrawSpanAddClampPalCommand>(); }
		void DrawSpanMaskedAddClamp() override { DrawerCommandQueue::QueueCommand<DrawSpanMaskedAddClampPalCommand>(); }
		void FillSpan() override { DrawerCommandQueue::QueueCommand<FillSpanPalCommand>(); }

		void DrawTiltedSpan(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap) override
		{
			DrawerCommandQueue::QueueCommand<DrawTiltedSpanPalCommand>(y, x1, x2, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy, basecolormap);
		}

		void DrawColoredSpan(int y, int x1, int x2) override { DrawerCommandQueue::QueueCommand<DrawColoredSpanPalCommand>(y, x1, x2); }
		void DrawFogBoundaryLine(int y, int x1, int x2) override { DrawerCommandQueue::QueueCommand<DrawFogBoundaryLinePalCommand>(y, x1, x2); }
	};
}
