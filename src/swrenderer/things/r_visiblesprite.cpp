
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "p_lnspec.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "swrenderer/r_main.h"
#include "swrenderer/things/r_visiblesprite.h"

namespace swrenderer
{
	int MaxVisSprites;
	vissprite_t **vissprites;
	vissprite_t **firstvissprite;
	vissprite_t **vissprite_p;
	vissprite_t **lastvissprite;

	void R_DeinitVisSprites()
	{
		// Free vissprites
		for (int i = 0; i < MaxVisSprites; ++i)
		{
			delete vissprites[i];
		}
		free(vissprites);
		vissprites = nullptr;
		vissprite_p = lastvissprite = nullptr;
		MaxVisSprites = 0;
	}

	void R_ClearVisSprites()
	{
		vissprite_p = firstvissprite;
	}

	vissprite_t *R_NewVisSprite()
	{
		if (vissprite_p == lastvissprite)
		{
			ptrdiff_t firstvisspritenum = firstvissprite - vissprites;
			ptrdiff_t prevvisspritenum = vissprite_p - vissprites;

			MaxVisSprites = MaxVisSprites ? MaxVisSprites * 2 : 128;
			vissprites = (vissprite_t **)M_Realloc(vissprites, MaxVisSprites * sizeof(vissprite_t));
			lastvissprite = &vissprites[MaxVisSprites];
			firstvissprite = &vissprites[firstvisspritenum];
			vissprite_p = &vissprites[prevvisspritenum];
			DPrintf(DMSG_NOTIFY, "MaxVisSprites increased to %d\n", MaxVisSprites);

			// Allocate sprites from the new pile
			for (vissprite_t **p = vissprite_p; p < lastvissprite; ++p)
			{
				*p = new vissprite_t;
			}
		}

		vissprite_p++;
		return *(vissprite_p - 1);
	}
}
