
#include <stdlib.h>
#include <float.h>

#include "templates.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_things.h"
#include "r_sky.h"
#include "stats.h"
#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "swrenderer/scene/r_bsp.h"
#include "r_visibleplane.h"

namespace swrenderer
{
	// [RH] Allocate one extra for sky box planes.
	visplane_t *visplanes[MAXVISPLANES + 1];
	visplane_t *freetail;
	visplane_t **freehead = &freetail;

	namespace
	{
		enum { max_plane_lights = 32 * 1024 };
		visplane_light plane_lights[max_plane_lights];
		int next_plane_light = 0;
	}

	void R_DeinitPlanes()
	{
		// do not use R_ClearPlanes because at this point the screen pointer is no longer valid.
		for (int i = 0; i <= MAXVISPLANES; i++)	// new code -- killough
		{
			for (*freehead = visplanes[i], visplanes[i] = NULL; *freehead; )
			{
				freehead = &(*freehead)->next;
			}
		}
		for (visplane_t *pl = freetail; pl != NULL; )
		{
			visplane_t *next = pl->next;
			free(pl);
			pl = next;
		}
	}

	visplane_t *new_visplane(unsigned hash)
	{
		visplane_t *check = freetail;

		if (check == NULL)
		{
			check = (visplane_t *)M_Malloc(sizeof(*check) + 3 + sizeof(*check->top)*(MAXWIDTH * 2));
			memset(check, 0, sizeof(*check) + 3 + sizeof(*check->top)*(MAXWIDTH * 2));
			check->bottom = check->top + MAXWIDTH + 2;
		}
		else if (NULL == (freetail = freetail->next))
		{
			freehead = &freetail;
		}

		check->lights = nullptr;

		check->next = visplanes[hash];
		visplanes[hash] = check;
		return check;
	}

	void R_PlaneInitData()
	{
		int i;
		visplane_t *pl;

		// Free all visplanes and let them be re-allocated as needed.
		pl = freetail;

		while (pl)
		{
			visplane_t *next = pl->next;
			M_Free(pl);
			pl = next;
		}
		freetail = NULL;
		freehead = &freetail;

		for (i = 0; i < MAXVISPLANES; i++)
		{
			pl = visplanes[i];
			visplanes[i] = NULL;
			while (pl)
			{
				visplane_t *next = pl->next;
				M_Free(pl);
				pl = next;
			}
		}
	}
}
