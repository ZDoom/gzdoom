#include "doomtype.h"
#include "doomstat.h"
#include "p_local.h"
#include "p_mobj.h"
#include "m_bbox.h"
#include "m_random.h"
#include "z_zone.h"
#include "s_sound.h"

quake_t *ActiveQuakes;

void P_RunQuakes (void)
{
	quake_t *quake = ActiveQuakes;
	quake_t **prev = &ActiveQuakes;

	while (quake) {
		int i;

		if (level.time % 48 == 0)
			S_Sound (quake->quakespot, CHAN_BODY, "world/quake", 1, ATTN_NORM);

		for (i = 0; i < MAXPLAYERS; i++) {
			if (playeringame[i] && !(players[i].cheats & CF_NOCLIP)) {
				mobj_t *mo = players[i].mo;

				if (!(level.time & 7) &&
					 mo->x >= quake->damagebox[BOXLEFT] && mo->x < quake->damagebox[BOXRIGHT] &&
					 mo->y >= quake->damagebox[BOXTOP] && mo->y < quake->damagebox[BOXBOTTOM]) {
					int mult = 1024 * quake->intensity;
					P_DamageMobj (mo, NULL, NULL, quake->intensity / 2, MOD_UNKNOWN);
					mo->momx += (P_Random (pr_quake)-128) * mult;
					mo->momy += (P_Random (pr_quake)-128) * mult;
				}

				if (mo->x >= quake->tremorbox[BOXLEFT] && mo->x < quake->tremorbox[BOXRIGHT] &&
					 mo->y >= quake->tremorbox[BOXTOP] && mo->y < quake->tremorbox[BOXBOTTOM]) {
					players[i].xviewshift = quake->intensity;
				}
			}
		}
		if (--quake->countdown == 0) {
			*prev = quake->next;
			Z_Free (quake);
		} else {
			prev = &quake->next;
		}
		quake = *prev;
	}
}

static void setbox (fixed_t *box, mobj_t *c, fixed_t size)
{
	if (size) {
		box[BOXLEFT] = c->x - size + 1;
		box[BOXRIGHT] = c->x + size;
		box[BOXTOP] = c->y - size + 1;
		box[BOXBOTTOM] = c->y + size;
	} else
		box[BOXLEFT] = box[BOXRIGHT] = box[BOXTOP] = box[BOXBOTTOM] = 0;
}

BOOL P_StartQuake (int tid, int intensity, int duration, int damrad, int tremrad)
{
	quake_t *quake;
	mobj_t *center = NULL;
	BOOL res = false;

	if (intensity > 9)
		intensity = 9;
	else if (intensity < 1)
		intensity = 1;

	while ( (center = P_FindMobjByTid (center, tid)) ) {
		res = true;
		quake = Z_Malloc (sizeof(*quake), PU_LEVSPEC, 0);
		quake->quakespot = center;
		setbox (quake->tremorbox, center, tremrad * FRACUNIT * 64);
		setbox (quake->damagebox, center, damrad * FRACUNIT * 64);
		quake->intensity = intensity;
		quake->countdown = duration;
		quake->next = ActiveQuakes;
		ActiveQuakes = quake;
	}
	
	return res;
}