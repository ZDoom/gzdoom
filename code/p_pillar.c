// [RH] p_pillar.c: New file to handle pillars

#include "z_zone.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "g_level.h"
#include "s_sndseq.h"

void T_Pillar (pillar_t *pillar)
{
	int r, s;

	if (pillar->type == pillarBuild) {
		r = T_MovePlane (pillar->sector, pillar->floorspeed, pillar->floortarget,
						 pillar->crush, 0, 1);
		s = T_MovePlane (pillar->sector, pillar->ceilingspeed, pillar->ceilingtarget,
						 pillar->crush, 1, -1);
	} else {
		r = T_MovePlane (pillar->sector, pillar->floorspeed, pillar->floortarget,
						 pillar->crush, 0, -1);
		s = T_MovePlane (pillar->sector, pillar->ceilingspeed, pillar->ceilingtarget,
						 pillar->crush, 1, 1);
	}

	if (r == pastdest && s == pastdest) {
		SN_StopSequence ((mobj_t *)&pillar->sector->soundorg);
		pillar->sector->floordata = NULL;
		pillar->sector->ceilingdata = NULL;
		P_RemoveThinker (&pillar->thinker);
	}
}

BOOL EV_DoPillar (pillar_e type, int tag, fixed_t speed, fixed_t height,
				  fixed_t height2, int crush)
{
	BOOL rtn = false;
	int secnum = -1;
	pillar_t *pillar;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0) {
		sector_t *sec = &sectors[secnum];
		fixed_t	ceilingdist, floordist;

		if (sec->floordata || sec->ceilingdata)
			continue;

		if (type == pillarBuild && sec->floorheight == sec->ceilingheight)
			continue;

		if (type == pillarOpen && sec->floorheight != sec->ceilingheight)
			continue;

		rtn = true;
		pillar = Z_Malloc (sizeof(*pillar), PU_LEVSPEC, 0);
		P_AddThinker (&pillar->thinker);
		sec->floordata = pillar;
		sec->ceilingdata = pillar;
		pillar->thinker.function.acp1 = (actionf_p1) T_Pillar;
		pillar->type = type;
		pillar->sector = sec;
		pillar->crush = crush;

		if (type == pillarBuild) {
			// If the pillar height is 0, have the floor and ceiling meet halfway
			if (height == 0) {
				pillar->floortarget = pillar->ceilingtarget =
					(sec->ceilingheight - sec->floorheight) / 2 + sec->floorheight;
				floordist = pillar->floortarget - sec->floorheight;
			} else {
				pillar->floortarget = pillar->ceilingtarget =
					sec->floorheight + height;
				floordist = height;
			}
			ceilingdist = sec->ceilingheight - pillar->ceilingtarget;
		} else {
			// If one of the heights is 0, figure it out based on the
			// surrounding sectors
			if (height == 0) {
				pillar->floortarget = P_FindLowestFloorSurrounding (sec);
				floordist = sec->floorheight - pillar->floortarget;
			} else {
				floordist = height;
				pillar->floortarget = sec->floorheight - height;
			}
			if (height2 == 0) {
				pillar->ceilingtarget = P_FindHighestCeilingSurrounding (sec);
				ceilingdist = pillar->ceilingtarget - sec->ceilingheight;
			} else {
				pillar->ceilingtarget = sec->ceilingheight + height2;
				ceilingdist = height2;
			}
		}

		// The speed parameter applies to whichever part of the pillar
		// travels the farthest. The other part's speed is then set so
		// that it arrives at its destination at the same time.
		if (floordist > ceilingdist) {
			pillar->floorspeed = speed;
			pillar->ceilingspeed = FixedDiv (FixedMul (speed, ceilingdist), floordist);
		} else {
			pillar->ceilingspeed = speed;
			pillar->floorspeed = FixedDiv (FixedMul (speed, floordist), ceilingdist);
		}

		if (sec->seqType >= 0)
			SN_StartSequence ((mobj_t *)&sec->soundorg, sec->seqType, SEQ_PLATFORM);
		else
			SN_StartSequenceName ((mobj_t *)&sec->soundorg, "Floor");
	}
	return rtn;
}