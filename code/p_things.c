// [RH] new file to deal with general things

#include "doomtype.h"
#include "p_local.h"
#include "p_effect.h"
#include "info.h"
#include "s_sound.h"
#include "tables.h"
#include "doomstat.h"
#include "m_random.h"

// List of spawnable things for the Thing_Spawn and Thing_Projectile specials.

int SpawnableThings[] = {
	0,
	MT_SHOTGUY,
	MT_CHAINGUY,
	MT_BRUISER,
	MT_POSSESSED,
	MT_TROOP,
	MT_BABY,
	MT_SPIDER,
	MT_SERGEANT,
	MT_SHADOWS,
	MT_TROOPSHOT,
	MT_CLIP,
	MT_MISC22,	// Shells
	0,
	0,
	0,
	0,
	0,
	0,
	MT_HEAD,
	MT_UNDEAD,
	MT_BRIDGE,
	MT_MISC3,	// Armor bonus
	MT_MISC10,	// Stim pack
	MT_MISC11,	// Medkit
	MT_MISC12,	// Soul sphere
	0,
	MT_SHOTGUN,
	MT_CHAINGUN,
	MT_MISC27,	// Rocket launcher
	MT_MISC28,	// Plasma gun
	MT_MISC25,	// BFG
	MT_MISC26,	// Chainsaw
	MT_SUPERSHOTGUN,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,	// T_MORPHBLAST
	0,	// T_ROCK1
	0,	// T_ROCK2
	0,	// T_ROCK3
	0,	// T_DIRT1
	0,	// T_DIRT2
	0,	// T_DIRT3
	0,	// T_DIRT4
	0,	// T_DIRT5
	0,	// T_DIRT6
	MT_PLASMA,
	0,	// T_POISONDART
	MT_TRACER,
	0,	// T_STAINEDGLASS1
	0,	// T_STAINEDGLASS2
	0,	// T_STAINEDGLASS3
	0,	// T_STAINEDGLASS4
	0,	// T_STAINEDGLASS5
	0,	// T_STAINEDGLASS6
	0,	// T_STAINEDGLASS7
	0,	// T_STAINEDGLASS8
	0,	// T_STAINEDGLASS9
	0,	// T_STAINEDGLASS0
	0,
	0,
	0,
	0,
	MT_MISC0,	// Green armor
	MT_MISC1,	// Blue armor
	0,
	0,
	0,
	0,
	0,
	MT_MISC20,	// Energy cell
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	MT_MISC4,	// Blue key card
	MT_MISC5,	// Red key card
	MT_MISC6,	// Yellow key card
	MT_MISC7,	// Yellow skull key
	MT_MISC8,	// Red skull key
	MT_MISC9,	// Blue skull key
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	MT_FIRE,
	0,
	MT_STEALTHBRUISER,
	MT_STEALTHKNIGHT,
	MT_STEALTHZOMBIE,
	MT_STEALTHSHOTGUY,
	0,
	0,
	0,
	0,
	0,
	0,
	MT_SKULL,
	MT_VILE,
	MT_FATSO,
	MT_KNIGHT,
	MT_CYBORG,
	MT_PAIN,
	MT_WOLFSS,
	MT_STEALTHBABY,
	MT_STEALTHVILE,
	MT_STEALTHHEAD,
	MT_STEALTHCHAINGUY,
	MT_STEALTHSERGEANT,
	MT_STEALTHIMP,
	MT_STEALTHFATSO,
	MT_STEALTHUNDEAD,
	MT_BARREL,
	MT_HEADSHOT,
	MT_ROCKET,
	MT_BFG,
	MT_ARACHPLAZ,
	MT_BLOOD,
	MT_PUFF,
	MT_MEGA,	// Mega sphere
	MT_INV,
	MT_MISC13,	// Berserk
	MT_INS,		// Blur sphere
	MT_MISC14,	// Rad suit
	MT_MISC15,	// Computer map
	MT_MISC16,	// Light amp
	MT_MISC17,	// Box of ammo
	MT_MISC18,	// Rocket
	MT_MISC19,	// Box of rockets
	MT_MISC21,	// Battery pack
	MT_MISC23,	// Box of shells
	MT_MISC24,	// Backpack
	MT_MISC68,	// Guts
	MT_MISC71,	// Blood pool
	MT_MISC84,	// Pool o' blood 1
	MT_MISC85,	// Pool o' blood 2
	MT_MISC77,	// Flaming barrel
	MT_MISC86	// Brains
};

const int NumSpawnableThings = sizeof(SpawnableThings);

BOOL P_Thing_Spawn (int tid, int type, angle_t angle, BOOL fog)
{
	fixed_t z;
	int rtn = 0;
	int kind;
	mobj_t *spot = NULL, *mobj;

	if (type >= NumSpawnableThings)
		return false;

	if ( (kind = SpawnableThings[type]) == 0)
		return false;

	if ((mobjinfo[kind].flags & MF_COUNTKILL) && (dmflags & DF_NO_MONSTERS))
		return false;

	while ( (spot = P_FindMobjByTid (spot, tid)) ) {
		if (spot->type != MT_MAPSPOT && spot->type != MT_MAPSPOTGRAV)
			continue;

		if (mobjinfo[kind].flags2 & MF2_FLOATBOB) {
			z = spot->z - spot->floorz;
		} else {
			z = spot->z;
		}
		mobj = P_SpawnMobj (spot->x, spot->y, z, kind);

		if (mobj) {
			if (P_TestMobjLocation (mobj)) {
				rtn++;
				mobj->angle = angle;
				if (fog)
					S_Sound (P_SpawnMobj (spot->x, spot->y, spot->z, MT_TFOG),
							 CHAN_VOICE, "misc/teleport", 1, ATTN_NORM);
				mobj->flags2 |= MF2_DROPPED;	// Don't respawn
				if (mobj->flags2 & MF2_FLOATBOB) {
					mobj->special1 = mobj->z - mobj->floorz;
				}
			} else {
				P_RemoveMobj (mobj);
			}
		}
	}

	return rtn != 0;
}

BOOL P_CheckMissileSpawn (mobj_t *th);

BOOL P_Thing_Projectile (int tid, int type, angle_t angle,
						 fixed_t speed, fixed_t vspeed, BOOL gravity)
{
	int rtn = 0;
	int kind;
	mobj_t *spot = NULL, *mobj;

	if (type >= NumSpawnableThings)
		return false;

	if ( (kind = SpawnableThings[type]) == 0)
		return false;

	if ((mobjinfo[kind].flags & MF_COUNTKILL) && (dmflags & DF_NO_MONSTERS))
		return false;

	while ( (spot = P_FindMobjByTid (spot, tid)) ) {
		if (spot->type != MT_MAPSPOT && spot->type != MT_MAPSPOTGRAV)
			continue;

		mobj = P_SpawnMobj (spot->x, spot->y, spot->z, kind);

		if (mobj) {
			if (mobj->info->seesound)
				S_Sound (mobj, CHAN_VOICE, mobj->info->seesound, 1, ATTN_NORM);
			if (gravity) {
				mobj->flags &= ~MF_NOGRAVITY;
				if (!(mobj->flags & MF_COUNTKILL))
					mobj->flags2 |= MF2_LOGRAV;
			} else
				mobj->flags |= MF_NOGRAVITY;
			mobj->target = spot;
			mobj->angle = angle;
			mobj->momx = FixedMul (speed, finecosine[angle>>ANGLETOFINESHIFT]);
			mobj->momy = FixedMul (speed, finesine[angle>>ANGLETOFINESHIFT]);
			mobj->momz = vspeed;
			mobj->flags |= MF_DROPPED;

			rtn = P_CheckMissileSpawn (mobj);
		}
	}

	return rtn;
}

BOOL P_ActivateMobj (mobj_t *mobj)
{
	if (mobj->flags & MF_COUNTKILL)
	{
		mobj->flags2 &= !(MF2_DORMANT | MF2_INVULNERABLE);
		return true;
	}
	else
	{
		switch (mobj->type)
		{
			case MT_SPARK:
			{
				int count = mobj->args[0];
				char sound[16];

				if (count == 0)
					count = 32;

				P_DrawSplash (count, mobj->x, mobj->y, mobj->z, mobj->angle, 1);
				sprintf (sound, "world/spark%d", 1+(M_Random() % 3));
				S_Sound (mobj, CHAN_AUTO, sound, 1, ATTN_IDLE);
				break;
			}
			case MT_FOUNTAIN:
				mobj->effects &= ~FX_FOUNTAINMASK;
				mobj->effects |= mobj->args[0] << FX_FOUNTAINSHIFT;
				break;
			default:
				break;
		}
	}
	return false;
}

BOOL P_DeactivateMobj (mobj_t *mobj)
{
	if (mobj->flags & MF_COUNTKILL)
	{
		mobj->flags2 |= MF2_DORMANT | MF2_INVULNERABLE;
		return true;
	}
	else
	{
		switch (mobj->type)
		{
			case MT_FOUNTAIN:
				mobj->effects &= ~FX_FOUNTAINMASK;
				break;
			default:
				break;
		}
	}
	return false;
}
