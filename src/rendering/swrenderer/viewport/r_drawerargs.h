
#pragma once


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
	class ColormapLight;
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
		void SetBaseColormap(FSWColormap *base_colormap);
		void SetLight(float light, int shade);
		void SetLight(float light, int lightlevel, bool foggy, RenderViewport *viewport);
		void SetTranslationMap(lighttable_t *translation);

		uint8_t *Colormap(RenderViewport *viewport) const;
		uint8_t *TranslationMap() const { return mTranslation; }
		FSWColormap* BaseColormap() const { return mBaseColormap; }

		ShadeConstants ColormapConstants() const;
		fixed_t Light() const { return LIGHTSCALE(mLight, mShade); }

		float FixedLight() const { return mLight; }
		int Shade() const { return mShade; }

	protected:
		void SetLight(const ColormapLight &light);

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
