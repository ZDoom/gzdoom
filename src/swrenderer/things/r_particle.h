
#pragma once

#include "r_visiblesprite.h"

namespace swrenderer
{
	void R_ProjectParticle(particle_t *, const sector_t *sector, int shade, int fakeside);
	void R_DrawParticle(vissprite_t *);
	void R_DrawMaskedSegsBehindParticle(const vissprite_t *vis);
}
