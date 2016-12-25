
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

	class PalWall4Command : public DrawerCommand
	{
	public:
		PalWall4Command();
		FString DebugInfo() override { return "PalWallCommand"; }

	protected:
		uint8_t *_dest;
		int _count;
		int _pitch;
		int _fracbits;
		uint8_t *_colormap[4];
		const uint8_t *_source[4];
		uint32_t _iscale[4];
		uint32_t _texturefrac[4];
		uint32_t *_srcblend;
		uint32_t *_destblend;
	};

	class DrawWall1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWall4PalCommand : public PalWall4Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallMasked1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallMasked4PalCommand : public PalWall4Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallAdd1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallAdd4PalCommand : public PalWall4Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallAddClamp1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallAddClamp4PalCommand : public PalWall4Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallSubClamp1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallSubClamp4PalCommand : public PalWall4Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallRevSubClamp1PalCommand : public PalWall1Command { public: void Execute(DrawerThread *thread) override; };
	class DrawWallRevSubClamp4PalCommand : public PalWall4Command { public: void Execute(DrawerThread *thread) override; };

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
	class DrawSingleSky4PalCommand : public PalSkyCommand { public: using PalSkyCommand::PalSkyCommand; void Execute(DrawerThread *thread) override; };
	class DrawDoubleSky1PalCommand : public PalSkyCommand { public: using PalSkyCommand::PalSkyCommand; void Execute(DrawerThread *thread) override; };
	class DrawDoubleSky4PalCommand : public PalSkyCommand { public: using PalSkyCommand::PalSkyCommand; void Execute(DrawerThread *thread) override; };

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
		DrawTiltedSpanPalCommand(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy);
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

	class DrawSlabPalCommand : public PalSpanCommand
	{
	public:
		DrawSlabPalCommand(int dx, fixed_t v, int dy, fixed_t vi, const uint8_t *vptr, uint8_t *p, const uint8_t *colormap);
		void Execute(DrawerThread *thread) override;

	private:
		int _dx;
		fixed_t _v;
		int _dy;
		fixed_t _vi;
		const uint8_t *_vvptr;
		uint8_t *_p;
		const uint8_t *_colormap;
		int _pitch;
		int _start_y;
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
}
