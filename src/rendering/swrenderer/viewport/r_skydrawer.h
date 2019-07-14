
#pragma once

#include "r_drawerargs.h"

struct FSWColormap;
struct FLightNode;

namespace swrenderer
{
	class RenderThread;
	
	class SkyDrawerArgs : public DrawerArgs
	{
	public:
		void SetStyle();
		void SetDest(RenderViewport *viewport, int x, int y);
		void SetCount(int count) { dc_count = count; }
		void SetFrontTexture(RenderThread *thread, FSoftwareTexture *texture, fixed_t column);
		void SetBackTexture(RenderThread *thread, FSoftwareTexture *texture, fixed_t column);
		void SetTextureVPos(uint32_t texturefrac) { dc_texturefrac = texturefrac; }
		void SetTextureVStep(uint32_t iscale) { dc_iscale = iscale; }
		void SetSolidTop(uint32_t color) { solid_top = color; }
		void SetSolidBottom(uint32_t color) { solid_bottom = color; }
		void SetFadeSky(bool enable) { fadeSky = enable; }

		uint8_t *Dest() const { return dc_dest; }
		int DestY() const { return dc_dest_y; }
		int Count() const { return dc_count; }

		uint32_t TextureVPos() const { return dc_texturefrac; }
		uint32_t TextureVStep() const { return dc_iscale; }

		uint32_t SolidTopColor() const { return solid_top; }
		uint32_t SolidBottomColor() const { return solid_bottom; }
		bool FadeSky() const { return fadeSky; }

		const uint8_t *FrontTexturePixels() const { return dc_source; }
		const uint8_t *BackTexturePixels() const { return dc_source2; }
		int FrontTextureHeight() const { return dc_sourceheight; }
		int BackTextureHeight() const { return dc_sourceheight2; }

		RenderViewport *Viewport() const { return dc_viewport; }

		void DrawDepthSkyColumn(RenderThread *thread, float idepth);
		void DrawSingleSkyColumn(RenderThread *thread);
		void DrawDoubleSkyColumn(RenderThread *thread);

	private:
		uint8_t *dc_dest = nullptr;
		int dc_dest_y = 0;
		int dc_count = 0;
		const uint8_t *dc_source;
		const uint8_t *dc_source2;
		uint32_t dc_sourceheight;
		uint32_t dc_sourceheight2;
		uint32_t dc_texturefrac;
		uint32_t dc_iscale;
		uint32_t solid_top;
		uint32_t solid_bottom;
		bool fadeSky;
		RenderViewport *dc_viewport = nullptr;
	};
}
