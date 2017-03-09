
#pragma once

#include "r_drawerargs.h"

struct FSWColormap;
struct FLightNode;

namespace swrenderer
{
	class RenderThread;
	
	class SpanDrawerArgs : public DrawerArgs
	{
	public:
		SpanDrawerArgs();

		void SetStyle(bool masked, bool additive, fixed_t alpha);
		void SetDestY(int y) { ds_y = y; }
		void SetDestX1(int x) { ds_x1 = x; }
		void SetDestX2(int x) { ds_x2 = x; }
		void SetTexture(FTexture *tex);
		void SetTextureLOD(double lod) { ds_lod = lod; }
		void SetTextureUPos(dsfixed_t xfrac) { ds_xfrac = xfrac; }
		void SetTextureVPos(dsfixed_t yfrac) { ds_yfrac = yfrac; }
		void SetTextureUStep(dsfixed_t xstep) { ds_xstep = xstep; }
		void SetTextureVStep(dsfixed_t vstep) { ds_ystep = vstep; }
		void SetSolidColor(int colorIndex) { ds_color = colorIndex; }

		void DrawSpan(RenderThread *thread);
		void DrawTiltedSpan(RenderThread *thread, int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap);
		void DrawColoredSpan(RenderThread *thread, int y, int x1, int x2);
		void DrawFogBoundaryLine(RenderThread *thread, int y, int x1, int x2);

		uint32_t *SrcBlend() const { return dc_srcblend; }
		uint32_t *DestBlend() const { return dc_destblend; }
		fixed_t SrcAlpha() const { return dc_srcalpha; }
		fixed_t DestAlpha() const { return dc_destalpha; }
		int DestY() const { return ds_y; }
		int DestX1() const { return ds_x1; }
		int DestX2() const { return ds_x2; }
		dsfixed_t TextureUPos() const { return ds_xfrac; }
		dsfixed_t TextureVPos() const { return ds_yfrac; }
		dsfixed_t TextureUStep() const { return ds_xstep; }
		dsfixed_t TextureVStep() const { return ds_ystep; }
		int SolidColor() const { return ds_color; }
		int TextureWidthBits() const { return ds_xbits; }
		int TextureHeightBits() const { return ds_ybits; }
		const uint8_t *TexturePixels() const { return ds_source; }
		bool MipmappedTexture() const { return ds_source_mipmapped; }
		double TextureLOD() const { return ds_lod; }

		FVector3 dc_normal;
		FVector3 dc_viewpos;
		FVector3 dc_viewpos_step;
		DrawerLight *dc_lights = nullptr;
		int dc_num_lights = 0;

	private:
		typedef void(SWPixelFormatDrawers::*SpanDrawerFunc)(const SpanDrawerArgs &args);
		SpanDrawerFunc spanfunc;

		int ds_y;
		int ds_x1;
		int ds_x2;
		int ds_xbits;
		int ds_ybits;
		const uint8_t *ds_source;
		bool ds_source_mipmapped;
		dsfixed_t ds_xfrac;
		dsfixed_t ds_yfrac;
		dsfixed_t ds_xstep;
		dsfixed_t ds_ystep;
		uint32_t *dc_srcblend;
		uint32_t *dc_destblend;
		fixed_t dc_srcalpha;
		fixed_t dc_destalpha;
		int ds_color = 0;
		double ds_lod;
	};
}
