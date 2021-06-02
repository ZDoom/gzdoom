
#pragma once

#include "r_drawerargs.h"

struct FSWColormap;
struct FLightNode;

namespace swrenderer
{
	class RenderThread;
	struct FWallCoords;
	class ProjectedWallLight;
	
	class VoxelBlock
	{
	public:
		int x, y;
		uint16_t width, height;
		fixed_t vPos;
		fixed_t vStep;
		const uint8_t *voxels;
		int voxelsCount;
	};

	class SpriteDrawerArgs : public DrawerArgs
	{
	public:
		SpriteDrawerArgs();

		bool SetStyle(RenderViewport *viewport, FRenderStyle style, fixed_t alpha, int translation, uint32_t color, const ColormapLight &light);
		bool SetStyle(RenderViewport *viewport, FRenderStyle style, float alpha, int translation, uint32_t color, const ColormapLight &light);
		void SetSolidColor(int color) { dc_color = color; dc_color_bgra = GPalette.BaseColors[color]; }
		void SetDynamicLight(uint32_t color) { dynlightcolor = color; }

		void DrawMasked(RenderThread* thread, double topZ, double scale, bool flipX, bool flipY, const FWallCoords& WallC, int clipx1, int clipx2, const ProjectedWallLight& light, FSoftwareTexture* texture, const short* mfloorclip, const short* mceilingclip, FRenderStyle style);
		void DrawMasked2D(RenderThread *thread, double x0, double x1, double y0, double y1, FSoftwareTexture* texture, FRenderStyle style);
		void DrawVoxelBlocks(RenderThread *thread, const VoxelBlock *blocks, int blockcount);

		uint8_t *Dest() const { return dc_dest; }
		int DestY() const { return dc_dest_y; }
		int Count() const { return dc_count; }

		int FuzzX() const { return dc_x; }
		int FuzzY1() const { return dc_yl; }
		int FuzzY2() const { return dc_yh; }

		uint32_t TextureUPos() const { return dc_texturefracx; }
		fixed_t TextureVPos() const { return dc_texturefrac; }
		fixed_t TextureVStep() const { return dc_iscale; }

		int SolidColor() const { return dc_color; }
		uint32_t SolidColorBgra() const { return dc_color_bgra; }
		uint32_t SrcColorIndex() const { return dc_srccolor; }
		uint32_t SrcColorBgra() const { return dc_srccolor_bgra; }

		const uint8_t *TexturePixels() const { return dc_source; }
		const uint8_t *TexturePixels2() const { return dc_source2; }
		uint32_t TextureHeight() const { return dc_textureheight; }

		uint32_t *SrcBlend() const { return dc_srcblend; }
		uint32_t *DestBlend() const { return dc_destblend; }
		fixed_t SrcAlpha() const { return dc_srcalpha; }
		fixed_t DestAlpha() const { return dc_destalpha; }
		
		uint32_t DynamicLight() const { return dynlightcolor; }

		bool DrawerNeedsPalInput() const { return drawer_needs_pal_input; }
		RenderViewport *Viewport() const { return dc_viewport; }

	private:
		void DrawMaskedColumn(RenderThread* thread, int x, float y1, int cliptop, int clipbottom, uint32_t texelX, uint32_t texelStepX, uint32_t texelStepY, float scaleV, bool flipY, FSoftwareTexture* tex, int texwidth, int texheight, bool bgra, FRenderStyle style);

		void SetDest(RenderViewport* viewport, int x, int y);
		void SetCount(int count) { dc_count = count; }

		bool SetBlendFunc(int op, fixed_t fglevel, fixed_t bglevel, int flags);
		static fixed_t GetAlpha(int type, fixed_t alpha);

		uint8_t *dc_dest = nullptr;
		int dc_dest_y = 0;
		int dc_count = 0;

		fixed_t dc_iscale;
		fixed_t dc_texturefrac;
		uint32_t dc_texturefracx;

		uint32_t dc_textureheight = 0;
		const uint8_t *dc_source = nullptr;
		const uint8_t *dc_source2 = nullptr;
		bool drawer_needs_pal_input = false;

		uint32_t *dc_srcblend = nullptr;
		uint32_t *dc_destblend = nullptr;
		fixed_t dc_srcalpha = OPAQUE;
		fixed_t dc_destalpha = 0;

		int dc_x = 0;
		int dc_yl = 0;
		int dc_yh = 0;

		int dc_color = 0;
		uint32_t dc_color_bgra = 0;
		uint32_t dc_srccolor = 0;
		uint32_t dc_srccolor_bgra = 0;
		
		uint32_t dynlightcolor = 0;

		typedef void(SWPixelFormatDrawers::*SpriteDrawerFunc)(const SpriteDrawerArgs &args);
		SpriteDrawerFunc colfunc;

		RenderViewport *dc_viewport = nullptr;

		friend class SWTruecolorDrawers;
		friend class SWPalDrawers;
	};
}
