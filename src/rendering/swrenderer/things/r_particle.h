#pragma once

#include "r_visiblesprite.h"
#include "swrenderer/scene/r_opaque_pass.h"

struct particle_t;

namespace swrenderer
{
	class RenderParticle : public VisibleSprite
	{
	public:
		static void Project(RenderThread *thread, particle_t *, const sector_t *sector, int shade, WaterFakeSide fakeside, bool foggy);

	protected:
		bool IsParticle() const override { return true; }
		void Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor) override;

	private:
		void DrawMaskedSegsBehindParticle(RenderThread *thread, const Fake3DTranslucent &clip3DFloor);

		fixed_t xscale = 0;
		fixed_t	startfrac = 0; // horizontal position of x1
		int y1 = 0, y2 = 0;

		uint32_t Translation = 0;
		uint32_t FillColor = 0;
	};
}
