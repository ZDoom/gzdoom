
#pragma once

#include "templates.h"
#include "doomtype.h"
#include "doomdef.h"
#include "r_defs.h"
#include "swrenderer/drawers/r_draw.h"
#include "v_video.h"
#include "r_data/colormaps.h"
#include "r_data/r_translate.h"
#include "swrenderer/scene/r_light.h"

struct FSWColormap;
struct FLightNode;

namespace swrenderer
{
	class SWPixelFormatDrawers;
	class DrawerArgs;
	struct ShadeConstants;

	struct DrawerLight
	{
		uint32_t color;
		float x, y, z;
		float radius;
	};

	class DrawerArgs
	{
	public:
		void SetLight(FSWColormap *base_colormap, float light, int shade);
		void SetTranslationMap(lighttable_t *translation);

		uint8_t *Colormap(RenderViewport *viewport) const;
		uint8_t *TranslationMap() const { return mTranslation; }

		ShadeConstants ColormapConstants() const;
		fixed_t Light() const { return LIGHTSCALE(mLight, mShade); }

	private:
		FSWColormap *mBaseColormap = nullptr;
		float mLight = 0.0f;
		int mShade = 0;
		uint8_t *mTranslation = nullptr;
	};

	struct ShadeConstants
	{
		uint16_t light_alpha;
		uint16_t light_red;
		uint16_t light_green;
		uint16_t light_blue;
		uint16_t fade_alpha;
		uint16_t fade_red;
		uint16_t fade_green;
		uint16_t fade_blue;
		uint16_t desaturate;
		bool simple_shade;
	};
}
