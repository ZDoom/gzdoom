// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Moving object handling. Spawn functions.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#include "m_alloc.h"
#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_effect.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "doomstat.h"
#include "v_video.h"
#include "c_cvars.h"
#include "b_bot.h"	//Added by MC:
#include "stats.h"

// MACROS ------------------------------------------------------------------

#define WATER_SINK_FACTOR		3
#define WATER_SINK_SMALL_FACTOR	4
#define WATER_SINK_SPEED		(FRACUNIT/2)
#define WATER_JUMP_SPEED		(FRACUNIT*7/2)

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void G_PlayerReborn (int player);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern cycle_t BotSupportCycles;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (sv_gravity, "800", CVAR_SERVERINFO)
CVAR (sv_friction, "0.90625", CVAR_SERVERINFO)

fixed_t FloatBobOffsets[64] =
{
	0, 51389, 102283, 152192,
	200636, 247147, 291278, 332604,
	370727, 405280, 435929, 462380,
	484378, 501712, 514213, 521763,
	524287, 521763, 514213, 501712,
	484378, 462380, 435929, 405280,
	370727, 332604, 291278, 247147,
	200636, 152192, 102283, 51389,
	-1, -51390, -102284, -152193,
	-200637, -247148, -291279, -332605,
	-370728, -405281, -435930, -462381,
	-484380, -501713, -514215, -521764,
	-524288, -521764, -514214, -501713,
	-484379, -462381, -435930, -405280,
	-370728, -332605, -291279, -247148,
	-200637, -152193, -102284, -51389
};

// CODE --------------------------------------------------------------------

IMPLEMENT_SERIAL (AActor, DThinker)

void AActor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
	{
		arc << x
			<< y
			<< z
			<< pitch
			<< angle
			<< roll
			<< sprite
			<< frame
			<< effects
			<< floorz
			<< ceilingz
			<< radius
			<< height
			<< momx
			<< momy
			<< momz
			<< type
			<< tics
			<< state
			<< damage
			<< flags
			<< flags2
			<< special1
			<< special2
			<< health
			<< movedir
			<< visdir
			<< movecount
			<< target
			<< lastenemy
			<< reactiontime
			<< threshold
			<< player
			<< lastlook
			<< tracer
			<< tid
			<< special
			<< args[0] << args[1] << args[2] << args[3] << args[4]
			<< goal
			<< targettic
			<< translucency
			<< waterlevel;

		if (translation)
			arc << (DWORD)(translation - translationtables);
		else
			arc << (DWORD)0xffffffff;
		spawnpoint.Serialize (arc);
	}
	else
	{
		arc >> x
			>> y
			>> z
			>> pitch
			>> angle
			>> roll
			>> sprite
			>> frame
			>> effects
			>> floorz
			>> ceilingz
			>> radius
			>> height
			>> momx
			>> momy
			>> momz
			>> type
			>> tics
			>> state
			>> damage
			>> flags
			>> flags2
			>> special1
			>> special2
			>> health
			>> movedir
			>> visdir
			>> movecount
			>> target
			>> lastenemy
			>> reactiontime
			>> threshold
			>> player
			>> lastlook
			>> tracer
			>> tid
			>> special
			>> args[0]
			>> args[1]
			>> args[2]
			>> args[3]
			>> args[4]
			>> goal
			>> targettic
			>> translucency
			>> waterlevel;

		DWORD trans;
		arc >> trans;
		if (trans == (DWORD)0xffffffff)
			translation = NULL;
		else
			translation = translationtables + trans;
		spawnpoint.Serialize (arc);
		info = &mobjinfo[type];
		touching_sectorlist = NULL;
		LinkToWorld ();
		AddToHash ();
	}
}

void MapThing::Serialize (FArchive &arc)
{
	if (arc.IsStoring ())
	{
		arc << thingid << x << y << z << angle << type << flags << special
			<< args[0] << args[1] << args[2] << args[3] << args[4];
	}
	else
	{
		arc >> thingid >> x >> y >> z >> angle >> type >> flags >> special
			>> args[0] >> args[1] >> args[2] >> args[3] >> args[4];
	}
}

AActor::AActor ()
{
	memset (&x, 0, (byte *)&this[1] - (byte *)&x);
}

AActor::AActor (const AActor &other)
{
	memcpy (&x, &other.x, (byte *)&this[1] - (byte *)&x);
}

AActor &AActor::operator= (const AActor &other)
{
	memcpy (&x, &other.x, (byte *)&this[1] - (byte *)&x);
	return *this;
}

//==========================================================================
//
// P_SetMobjState
//
// Returns true if the mobj is still present.
//
//==========================================================================

BOOL P_SetMobjState (AActor *mobj, statenum_t state)
{
	state_t *st;

	// killough 4/9/98: remember states seen, to detect cycles:

	static statenum_t seenstate_tab[NUMSTATES];	// fast transition table
	statenum_t *seenstate = seenstate_tab;		// pointer to table
	static int recursion;						// detects recursion
	statenum_t i = state;						// initial state
	BOOL ret = true;							// return value
	statenum_t tempstate[NUMSTATES];			// for use with recursion

	if (recursion++)							// if recursion detected,
		memset(seenstate=tempstate,0,sizeof tempstate);	// clear state table

	do
	{
		if (state == S_NULL)
		{
			mobj->state = (state_t *) S_NULL;
			delete mobj;
			ret = false;
			break;					// killough 4/9/98
		}

		st = &states[state];
		mobj->state = st;
		mobj->tics = st->tics;
		if (st->sprite != SPR_PLAY)
			mobj->sprite = st->sprite;	// [RH] Only change sprite if not a player
		mobj->frame = st->frame;

		// Modified handling.
		// Call action functions when the state is set

		if (st->action.acp1)
			st->action.acp1(mobj);

		seenstate[state] = (statenum_t)(1 + st->nextstate);		// killough 4/9/98

		state = st->nextstate;
	} while (!mobj->tics && !seenstate[state]);		// killough 4/9/98

	if (ret && !mobj->tics)  // killough 4/9/98: detect state cycles
		Printf (PRINT_HIGH, "Warning: State Cycle Detected\n");

	if (!--recursion)
		for (;(state=seenstate[i]);i=(statenum_t)(state-1))
			seenstate[i] = S_NULL;  // killough 4/9/98: erase memory of states

	return ret;
}

//----------------------------------------------------------------------------
//
// PROC P_ExplodeMissile
//
//----------------------------------------------------------------------------

void P_ExplodeMissile (AActor* mo)
{
	mo->momx = mo->momy = mo->momz = 0;

	P_SetMobjState (mo, mobjinfo[mo->type].deathstate);
	if (mobjinfo[mo->type].deathstate != S_NULL)
	{
		// [RH] If the object is already translucent, don't change it.
		// Otherwise, make it 66% translucent.
		if (mo->translucency == FRACUNIT)
			mo->translucency = TRANSLUC66;

		mo->tics -= P_Random (pr_explodemissile) & 3;

		if (mo->tics < 1)
			mo->tics = 1;

		mo->flags &= ~MF_MISSILE;

		if (mo->info->deathsound)
			S_Sound (mo, CHAN_VOICE, mo->info->deathsound, 1, ATTN_NORM);

		mo->effects = 0;		// [RH]
	}
}

//----------------------------------------------------------------------------
//
// PROC P_FloorBounceMissile
//
//----------------------------------------------------------------------------

void P_FloorBounceMissile (AActor *mo)
{
/*
	if(P_HitFloor(mo) >= FLOOR_LIQUID)
	{
		switch(mo->type)
		{
			case MT_SORCFX1:
			case MT_SORCBALL1:
			case MT_SORCBALL2:
			case MT_SORCBALL3:
				break;
			default:
				delete mo;
				return;
		}
	}
*/
	switch(mo->type)
	{
/*
	case MT_SORCFX1:
		mo->momz = -mo->momz;		// no energy absorbed
		break;
	case MT_SGSHARD1:
	case MT_SGSHARD2:
	case MT_SGSHARD3:
	case MT_SGSHARD4:
	case MT_SGSHARD5:
	case MT_SGSHARD6:
	case MT_SGSHARD7:
	case MT_SGSHARD8:
	case MT_SGSHARD9:
	case MT_SGSHARD0:
		mo->momz = FixedMul(mo->momz, (fixed_t)(-0.3*FRACUNIT));
		if(abs(mo->momz) < (FRACUNIT/2))
		{
			P_SetMobjState(mo, S_NULL);
			return;
		}
		break;
*/
	default:
		mo->momz = FixedMul(mo->momz, (fixed_t)(-0.7*FRACUNIT));
		break;
	}
	mo->momx = 2*mo->momx/3;
	mo->momy = 2*mo->momy/3;
	if(mo->info->seesound)
	{
		S_Sound (mo, CHAN_VOICE, mo->info->seesound, 1, ATTN_IDLE);
	}
	if (!(mo->flags & MF_NOGRAVITY) && (mo->momz < 3*FRACUNIT))
		mo->flags2 &= ~MF2_FLOORBOUNCE;
//	P_SetMobjState(mo, mobjinfo[mo->type].deathstate);
}


//
// P_XYMovement  
//
extern int numspechit;
extern line_t **spechit;

#define STOPSPEED			0x1000
#define FRICTION			0xe800

void P_XYMovement (AActor *mo) 
{
	angle_t angle;
	fixed_t ptryx, ptryy;
	player_t *player;
	fixed_t xmove, ymove;

	if (!mo->momx && !mo->momy)
	{
		if (mo->flags & MF_SKULLFLY)
		{
			// the skull slammed into something
			mo->flags &= ~MF_SKULLFLY;
			mo->momx = mo->momy = mo->momz = 0;

			P_SetMobjState (mo, mo->info->spawnstate);
		}
		return;
	}

	player = mo->player;

	int maxmove = (mo->waterlevel < 2) || (mo->flags & MF_MISSILE) ? MAXMOVE : MAXMOVE/4;

	if (mo->momx > maxmove)
		mo->momx = maxmove;
	else if (mo->momx < -maxmove)
		mo->momx = -maxmove;

	if (mo->momy > maxmove)
		mo->momy = maxmove;
	else if (mo->momy < -maxmove)
		mo->momy = -maxmove;
	
	xmove = mo->momx;
	ymove = mo->momy;

	maxmove /= 2;

	do
	{
		if (xmove > maxmove || ymove > maxmove ||
			xmove < -maxmove || ymove < -maxmove)
		{
			ptryx = mo->x + xmove/2;
			ptryy = mo->y + ymove/2;
			xmove >>= 1;
			ymove >>= 1;
		}
		else
		{
			ptryx = mo->x + xmove;
			ptryy = mo->y + ymove;
			xmove = ymove = 0;
		}

		// killough 3/15/98: Allow objects to drop off
		if (!P_TryMove (mo, ptryx, ptryy, true))
		{
			// blocked move
			if (mo->flags2 & MF2_SLIDE)
			{
				// try to slide along it
				if (BlockingMobj == NULL)
				{ // slide against wall
					if (mo->player && mo->waterlevel && mo->waterlevel < 3
						&& (mo->player->cmd.ucmd.forwardmove | mo->player->cmd.ucmd.sidemove))
					{
						mo->momz = WATER_JUMP_SPEED;
					}
					P_SlideMove (mo);
				}
				else
				{ // slide against mobj
					if (P_TryMove (mo, mo->x, ptryy, true))
					{
						mo->momx = 0;
					}
					else if (P_TryMove (mo, ptryx, mo->y, true))
					{
						mo->momy = 0;
					}
					else
					{
						mo->momx = mo->momy = 0;
					}
					if (player && player->mo == mo)
					{
						if (mo->momx == 0)
							player->momx = 0;
						if (mo->momy == 0)
							player->momy = 0;
					}
				}
			}
			else if (mo->flags & MF_MISSILE)
			{
				if (mo->flags2 & MF2_FLOORBOUNCE)
				{
					if (BlockingMobj)
					{
						if ((BlockingMobj->flags2 & MF2_REFLECTIVE) ||
							((!BlockingMobj->player) &&
							(!(BlockingMobj->flags & MF_COUNTKILL))))
						{
							fixed_t speed;

							angle = R_PointToAngle2 (BlockingMobj->x,
								BlockingMobj->y, mo->x, mo->y)
								+ANGLE_1*((P_Random(pr_bounce)%16)-8);
							speed = P_AproxDistance (mo->momx, mo->momy);
							speed = FixedMul (speed, (fixed_t)(0.75*FRACUNIT));
							mo->angle = angle;
							angle >>= ANGLETOFINESHIFT;
							mo->momx = FixedMul (speed, finecosine[angle]);
							mo->momy = FixedMul (speed, finesine[angle]);
							if (mo->info->seesound) {
								S_Sound (mo, CHAN_VOICE, mo->info->seesound, 1, ATTN_IDLE);
							}
							return;
						}
						else
						{
							// Struck a player/creature
							P_ExplodeMissile (mo);
						}
					}
					else
					{
						// Struck a wall
						P_BounceWall (mo);
						if (mo->info->seesound) {
							S_Sound (mo, CHAN_VOICE, mo->info->seesound, 1, ATTN_IDLE);
						}
						return;
					}
				}
				if(BlockingMobj &&
					(BlockingMobj->flags2 & MF2_REFLECTIVE))
				{
					angle = R_PointToAngle2(BlockingMobj->x,
													BlockingMobj->y,
													mo->x, mo->y);

					// Change angle for delflection/reflection
					angle += ANGLE_1 * ((P_Random(pr_bounce)%16)-8);

					// Reflect the missile along angle
					mo->angle = angle;
					angle >>= ANGLETOFINESHIFT;
					mo->momx = FixedMul(mo->info->speed>>1, finecosine[angle]);
					mo->momy = FixedMul(mo->info->speed>>1, finesine[angle]);
//					mo->momz = -mo->momz;
					mo->target = BlockingMobj;
					return;
				}
				// explode a missile
				if (ceilingline &&
					ceilingline->backsector &&
					ceilingline->backsector->ceilingpic == skyflatnum &&
					mo->z >= ceilingline->backsector->ceilingheight) //killough
				{
					// Hack to prevent missiles exploding against the sky.
					// Does not handle sky floors.
					delete mo;
					return;
				}
				P_ExplodeMissile (mo);
			}
			else
			{
				mo->momx = mo->momy = 0;
			}
		}
	} while (xmove || ymove);
	
	// Friction

	if (player && player->mo == mo && player->cheats & CF_NOMOMENTUM)
	{
		// debug option for no sliding at all
		mo->momx = mo->momy = 0;
		player->momx = player->momy = 0;
		return;
	}

	if (mo->flags & (MF_MISSILE | MF_SKULLFLY))
		return; 		// no friction for missiles

	if (mo->z > mo->floorz && !(mo->flags2 & MF2_ONMOBJ) && !(mo->flags2 & MF2_FLY)
		&& !mo->waterlevel)
		return;			// no friction when falling

	if (mo->flags & MF_CORPSE)
	{
		// do not stop sliding
		//	if halfway off a step with some momentum
		if (mo->momx > FRACUNIT/4
			|| mo->momx < -FRACUNIT/4
			|| mo->momy > FRACUNIT/4
			|| mo->momy < -FRACUNIT/4)
		{
			if (mo->floorz != mo->subsector->sector->floorheight)
				return;
		}
	}

	// killough 11/98:
	// Stop voodoo dolls that have come to rest, despite any
	// moving corresponding player:
	if (mo->momx > -STOPSPEED && mo->momx < STOPSPEED
		&& mo->momy > -STOPSPEED && mo->momy < STOPSPEED
		&& (!player || (player->mo != mo)
			|| !(player->cmd.ucmd.forwardmove | player->cmd.ucmd.sidemove)))
	{
		// if in a walking frame, stop moving
		// killough 10/98:
		// Don't affect main player when voodoo dolls stop:
		if (player && (unsigned)((player->mo->state - states) - S_PLAY_RUN1) < 4 
			&& (player->mo == mo))
		{
			P_SetMobjState (player->mo, S_PLAY);
		}

		mo->momx = mo->momy = 0;

		// killough 10/98: kill any bobbing momentum too (except in voodoo dolls)
		if (player && player->mo == mo)
			player->momx = player->momy = 0; 
	}
	else
	{
		// phares 3/17/98
		// Friction will have been adjusted by friction thinkers for icy
		// or muddy floors. Otherwise it was never touched and
		// remained set at ORIG_FRICTION
		//
		// killough 8/28/98: removed inefficient thinker algorithm,
		// instead using touching_sectorlist in P_GetFriction() to
		// determine friction (and thus only when it is needed).
		//
		// killough 10/98: changed to work with new bobbing method.
		// Reducing player momentum is no longer needed to reduce
		// bobbing, so ice works much better now.

		fixed_t friction = P_GetFriction (mo, NULL);

		mo->momx = FixedMul (mo->momx, friction);
		mo->momy = FixedMul (mo->momy, friction);

		// killough 10/98: Always decrease player bobbing by ORIG_FRICTION.
		// This prevents problems with bobbing on ice, where it was not being
		// reduced fast enough, leading to all sorts of kludges being developed.

		if (player && player->mo == mo)		//  Not voodoo dolls
		{
			player->momx = FixedMul (player->momx, ORIG_FRICTION);
			player->momy = FixedMul (player->momy, ORIG_FRICTION);
		}
	}
}

//
// P_ZMovement
//
void P_ZMovement (AActor *mo)
{
	fixed_t 	dist;
	fixed_t 	delta;
	
	// check for smooth step up
	if (mo->player && mo->z < mo->floorz)
	{
		mo->player->viewheight -= mo->floorz - mo->z;
		mo->player->deltaviewheight = (VIEWHEIGHT - mo->player->viewheight)>>3;
	}

	// adjust height
	mo->z += mo->momz;

	if ((mo->flags & MF_FLOAT) && mo->target)
	{
		// float down towards target if too close
		if (!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
		{
			dist = P_AproxDistance (mo->x - mo->target->x, mo->y - mo->target->y);
			delta =(mo->target->z + (mo->height>>1)) - mo->z;

			if (delta<0 && dist < -(delta*3) )
				mo->z -= FLOATSPEED;
			else if (delta>0 && dist < (delta*3) )
				mo->z += FLOATSPEED;
		}
	}
	if (mo->player && (mo->flags2 & MF2_FLY) && !(mo->z <= mo->floorz))
	{
		mo->z += finesine[(FINEANGLES/80*level.time)&FINEMASK]/8;
		mo->momz = FixedMul (mo->momz, FRICTION_FLY);
	}
	if (mo->waterlevel && !(mo->flags & MF_NOGRAVITY))
	{
		mo->momz = FixedMul (mo->momz, mo->subsector->sector->friction);
	}

	// clip movement
	if (mo->z <= mo->floorz)
	{
		// hit the floor
		if ((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
		{
			mo->z = mo->floorz;
			if (mo->flags2 & MF2_FLOORBOUNCE)
			{
				P_FloorBounceMissile (mo);
				return;
			}
			else
			{
				if (mo->subsector->sector->floorpic == skyflatnum)
				{
					// [RH] Just remove the missile without exploding it
					//		if this is a sky floor.
					delete mo;
					return;
				}
				P_HitFloor (mo);
				P_ExplodeMissile (mo);
				return;
			}
		}
		/*
		if (mo->flags & MF_COUNTKILL)		// Blasted mobj falling
		{
			if (mo->momz < -(23*FRACUNIT))
			{
				P_MonsterFallingDamage (mo);
			}
		}
		*/
		if (mo->z-mo->momz > mo->floorz)
		{
			// Spawn splashes, etc.
			P_HitFloor (mo);
		}
		mo->z = mo->floorz;
		if (mo->momz < 0)
		{
			if (mo->player)
			{
				// [RH] avoid integer roundoff by doing comparisons with floats
				float minmom = sv_gravity.value * mo->subsector->sector->gravity * -655.36f;
				float mom = (float)mo->momz;

				mo->player->jumpTics = 7;	// delay any jumping for a short while
				if (mom < minmom && !(mo->flags2 & MF2_FLY))
				{
					// Squat down.
					// Decrease viewheight for a moment after hitting the ground (hard),
					// and utter appropriate sound.
					mo->player->deltaviewheight = mo->momz >> 3;
					S_Sound (mo, CHAN_AUTO, "*land1", 1, ATTN_NORM);
				}
				mo->momz = 0;
			}
		}
		if (mo->flags & MF_SKULLFLY)
		{
			// The skull slammed into something
			mo->momz = -mo->momz;
		}
	}
	else
	{
		fixed_t startmomz = mo->momz;

		if (!mo->waterlevel || (mo->player &&
			!(mo->player->cmd.ucmd.forwardmove | mo->player->cmd.ucmd.sidemove)))
		{
			if (mo->flags2 & MF2_LOGRAV)
			{
				if (mo->momz == 0)
					mo->momz = (fixed_t)(sv_gravity.value * mo->subsector->sector->gravity * -20.48);
				else
					mo->momz -= (fixed_t)(sv_gravity.value * mo->subsector->sector->gravity * 10.24);
			}
			else if (!(mo->flags & MF_NOGRAVITY))
			{
				if (mo->momz == 0)
					mo->momz = (fixed_t)(sv_gravity.value * mo->subsector->sector->gravity * -163.84);
				else
					mo->momz -= (fixed_t)(sv_gravity.value * mo->subsector->sector->gravity * 81.92);
			}
		}
		if (mo->waterlevel > 1)
		{
			if (mo->momz < -WATER_SINK_SPEED)
			{
				mo->momz = (startmomz < -WATER_SINK_SPEED) ? startmomz : -WATER_SINK_SPEED;
			}
			else
			{
				mo->momz = startmomz + ((mo->momz - startmomz) >>
					(mo->waterlevel == 1 ? WATER_SINK_SMALL_FACTOR : WATER_SINK_FACTOR));
			}
		}
	}

	if (mo->z + mo->height > mo->ceilingz)
	{
		// hit the ceiling
		mo->z = mo->ceilingz - mo->height;
		if (mo->flags2 & MF2_FLOORBOUNCE)
		{
			// reverse momentum here for ceiling bounce
			mo->momz = FixedMul (mo->momz, (fixed_t)(-0.75*FRACUNIT));
			if (mo->info->seesound)
			{
				S_Sound (mo, CHAN_BODY, mo->info->seesound, 1, ATTN_IDLE);
			}
			return;
		}
		if (mo->momz > 0)
			mo->momz = 0;
		if (mo->flags & MF_SKULLFLY)
		{
			// the skull slammed into something
			mo->momz = -mo->momz;
		}
		if (mo->flags & MF_MISSILE && !(mo->flags & MF_NOCLIP))
		{
			if (mo->subsector->sector->ceilingpic == skyflatnum)
			{
				delete mo;
				return;
			}
			P_ExplodeMissile (mo);
			return;
		}
	}
}

//===========================================================================
//
// PlayerLandedOnThing
//
//===========================================================================

static void PlayerLandedOnThing(AActor *mo, AActor *onmobj)
{
	mo->player->deltaviewheight = mo->momz>>3;
	S_Sound (mo, CHAN_AUTO, "*land1", 1, ATTN_IDLE);
//	mo->player->centering = true;
}




//
// P_NightmareRespawn
//
void P_NightmareRespawn (AActor *mobj)
{
	fixed_t x, y, z;
	subsector_t *ss;
	AActor *mo;
	mapthing2_t *mthing;

	x = mobj->spawnpoint.x << FRACBITS;
	y = mobj->spawnpoint.y << FRACBITS;
	// something is occupying it's position?
	if (!P_CheckPosition (mobj, x, y)) 
		return;		// no respawn

	// spawn a teleport fog at old spot
	// because of removal of the body?
	mo = new AActor (mobj->x, mobj->y, ONFLOORZ, MT_TFOG);
	// initiate teleport sound
	S_Sound (mo, CHAN_VOICE, "misc/teleport", 1, ATTN_NORM);

	ss = R_PointInSubsector (x,y);

	// spawn a teleport fog at the new spot
	mo = new AActor (x, y, ONFLOORZ, MT_TFOG);

	S_Sound (mo, CHAN_VOICE, "misc/teleport", 1, ATTN_NORM);

	// spawn the new monster
	mthing = &mobj->spawnpoint;

	if (mobj->info->flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else if (mobj->info->flags2 & MF2_SPAWNFLOAT)
		z = FLOATRANDZ;
	else if (mobj->info->flags2 & MF2_FLOATBOB)
		z = mthing->z << FRACBITS;
	else
		z = ONFLOORZ;

	// spawn it
	// inherit attributes from deceased one
	mo = new AActor (x, y, ONFLOORZ, mobj->type);
	mo->spawnpoint = mobj->spawnpoint;
	mo->angle = ANG45 * (mthing->angle/45);

	if (z == ONFLOORZ)
		mo->z += mthing->z << FRACBITS;
	else if (z == ONCEILINGZ)
		mo->z -= mthing->z << FRACBITS;

	if (mo->flags2 & MF2_FLOATBOB)
	{ // Seed random starting index for bobbing motion
		mo->health = M_Random();
		mo->special1 = mthing->z << FRACBITS;
	}

	if (mthing->flags & MTF_AMBUSH)
		mo->flags |= MF_AMBUSH;

	mo->reactiontime = 18;

	// remove the old monster,
	delete mobj;
}


//
// [RH] Some new functions to work with Thing IDs. ------->
//
AActor *AActor::TIDHash[128];

//
// P_ClearTidHashes
//
// Clears the tid hashtable.
//

void AActor::ClearTIDHashes ()
{
	int i;

	for (i = 0; i < 128; i++)
		TIDHash[i] = NULL;
}

//
// P_AddMobjToHash
//
// Inserts an mobj into the correct chain based on its tid.
// If its tid is 0, this function does nothing.
//
void AActor::AddToHash ()
{
	if (tid == 0)
	{
		inext = iprev = NULL;
		return;
	}
	else
	{
		int hash = TIDHASH (tid);

		inext = TIDHash[hash];
		iprev = NULL;
		TIDHash[hash] = this;
	}
}

//
// P_RemoveMobjFromHash
//
// Removes an mobj from its hash chain.
//
void AActor::RemoveFromHash ()
{
	if (tid == 0)
		return;
	else
	{
		if (iprev == NULL)
		{
			// First mobj in the chain (probably)
			int hash = TIDHASH(tid);

			if (TIDHash[hash] == this)
				TIDHash[hash] = inext;
			if (inext)
			{
				inext->iprev = NULL;
				inext = NULL;
			}
		}
		else
		{
			// Not the first mobj in the chain
			iprev->inext = inext;
			if (inext)
			{
				inext->iprev = iprev;
				inext = NULL;
			}
			iprev = NULL;
		}
	}
}

//
// P_FindMobjByTid
//
// Returns the next mobj with the tid after the one given,
// or the first with that tid if no mobj is passed. Returns
// NULL if there are no more.
//
AActor *AActor::FindByTID (int tid) const
{
	return FindByTID (this, tid);
}

AActor *AActor::FindByTID (const AActor *actor, int tid)
{
	// Mobjs without tid are never stored.
	if (tid == 0)
		return NULL;

	if (!actor)
		actor = TIDHash[TIDHASH(tid)];
	else
		actor = actor->inext;

	while (actor && actor->tid != tid)
		actor = actor->inext;

	return const_cast<AActor *>(actor);
}

//
// P_FindGoal
//
// Like FindByTID except it also matches on type.
//
AActor *AActor::FindGoal (int tid, int kind) const
{
	return FindGoal (this, tid, kind);
}

AActor *AActor::FindGoal (const AActor *actor, int tid, int kind)
{
	do
	{
		actor = FindByTID (actor, tid);
	} while (actor && actor->type != kind);

	return const_cast<AActor *>(actor);
}

// <------- [RH] End new functions


//
// P_MobjThinker
//
void AActor::RunThink ()
{
	AActor *onmo;
	int i;

	// [RH] Decrement targettic
	if (targettic)
		targettic--;

	// [RH] Pulse in and out of visibility
	if (effects & FX_VISIBILITYPULSE)
	{
		if (special2 > 0)
		{
			translucency += 0x800;
			if (translucency >= FRACUNIT)
			{
				translucency = FRACUNIT;
				special2 = -1;
			}
		}
		else
		{
			translucency -= 0x800;
			if (translucency <= TRANSLUC25)
			{
				translucency = TRANSLUC25;
				special2 = 1;
			}
		}
	}

	// [RH] Fade a stealth monster in and out of visibility
	if (visdir > 0)
	{
		translucency += 2*FRACUNIT/TICRATE;
		if (translucency > FRACUNIT)
		{
			translucency = FRACUNIT;
			visdir = 0;
		}
	}
	else if (visdir < 0)
	{
		translucency -= 3*FRACUNIT/TICRATE/2;
		if (translucency < 0)
		{
			translucency = 0;
			visdir = 0;
		}
	}

	//Added by MC: Freeze mode.
	if (bglobal.freeze && !(player && !player->isbot))
		return;

	if (!bglobal.itemsdone) //Added by MC: Initialize thing list
	{
		if (bglobal.thingnum >= MAXTHINGS)
		{
			bglobal.itemsdone = true;
			Printf (PRINT_HIGH, "Warning: Number of items known to the bot limited to %d.\n", MAXTHINGS);
		}
		else if (flags & MF_SPECIAL)
		{
			if (this == bglobal.firstthing)
				bglobal.itemsdone = true;
			bglobal.things[bglobal.thingnum] = this;
			bglobal.thingnum++;
			if (!bglobal.firstthing)
				bglobal.firstthing = this;
		}
	}

	if (bglobal.botnum && consoleplayer == Net_Arbitrator && !demoplayback &&
		flags & (MF_COUNTKILL|MF_SPECIAL|MF_MISSILE))
	{
		clock (BotSupportCycles);
		bglobal.m_Thinking = true;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || !players[i].isbot)
				continue;

			if ((flags & MF_COUNTKILL)
				&& health > 0
				&& !players[i].enemy
				&& player ? !IsTeammate (players[i].mo) : true
				&& P_AproxDistance (players[i].mo->x-x, players[i].mo->y-y) < MAX_MONSTER_TARGET_DIST
				&& P_CheckSight (players[i].mo, this))
			{ //Probably a monster, so go kill it.
				players[i].enemy = this;
			}
			else if (flags & MF_SPECIAL)
			{ //Item pickup time
				bglobal.WhatToGet (players[i].mo, this);
			}
			else if ((flags & MF_MISSILE)
				&& !players[i].missile
				&& type != MT_PLASMA
				&& type != MT_ARACHPLAZ)
			{ //warn for incoming missiles.
				if (target != players[i].mo && bglobal.Check_LOS (players[i].mo, this, ANGLE_90))
					players[i].missile = this;
			}
		}
		bglobal.m_Thinking = false;
		unclock (BotSupportCycles);
	}

	//End of MC

	// Handle X and Y momemtums
	BlockingMobj = NULL;
	if (momx || momy || (flags & MF_SKULLFLY))
	{
		P_XYMovement (this);

		if (!IsValid ())
			return; 			// mobj was removed
	}

	if (flags2&MF2_FLOATBOB)
	{ // Floating item bobbing motion (special1 is height)
		z = floorz + special1 + FloatBobOffsets[(health++)&63];
	}
	else if ((z != floorz) || momz || BlockingMobj)
	{
		// Handle Z momentum and gravity
		if (flags2 & MF2_PASSMOBJ)
		{
			if (!(onmo = P_CheckOnmobj (this)))
			{
				P_ZMovement (this);
				if (player && flags2 & MF2_ONMOBJ)
				{
					flags2 &= ~MF2_ONMOBJ;
				}
			}
			else
			{
				if (player)
				{
					if (momz < (fixed_t)(sv_gravity.value * subsector->sector->gravity * -655.36f)
						&& !(flags2&MF2_FLY))
					{
						PlayerLandedOnThing (this, onmo);
					}
					if (onmo->z + onmo->height - z <= 24 * FRACUNIT)
					{
						player->viewheight -= z + onmo->height - z;
						player->deltaviewheight =
							(VIEWHEIGHT - player->viewheight)>>3;
						z = onmo->z + onmo->height;
						flags2 |= MF2_ONMOBJ;
						momz = 0;
					}
					else
					{
						// hit the bottom of the blocking mobj
						momz = 0;
					}
				}
			}
		}
		else
		{
			P_ZMovement (this);
		}
		
		if (!IsValid ())
			return; 			// mobj was removed
	}

	byte lastwaterlevel = waterlevel;

	waterlevel = 0;
	if (subsector->sector->waterzone)
		waterlevel = 3;
	sector_t *hsec;
	if ( (hsec = subsector->sector->heightsec) )
	{
		if (hsec->waterzone && !subsector->sector->waterzone)
		{
			if (z < hsec->floorheight)
			{
				waterlevel = 1;
				if (z + height/2 < hsec->floorheight)
				{
					waterlevel = 2;
					if (z + height <= hsec->floorheight)
						waterlevel = 3;
				}
			}
			else if (z + height > hsec->ceilingheight)
			{
				waterlevel = 3;
			}
		}
	}

	if (flags2 & MF2_DORMANT)
		return;

	// cycle through states, calling action functions at transitions
	if (tics != -1)
	{
		tics--;
				
		// you can cycle through multiple states in a tic
		if (!tics)
			if (!P_SetMobjState (this, state->nextstate) )
				return; 		// freed itself
	}
	else
	{
		// check for nightmare respawn
		if (!(flags & MF_COUNTKILL) || !respawnmonsters)
			return;

		movecount++;

		if (movecount < 12*TICRATE)
			return;

		if (level.time & 31)
			return;

		if (P_Random (pr_mobjthinker) > 4)
			return;

		P_NightmareRespawn (this);
	}
}

//==========================================================================
//
// P_SpawnMobj
//
//==========================================================================

AActor::AActor (fixed_t ix, fixed_t iy, fixed_t iz, mobjtype_t itype)
{
	state_t *st;
	fixed_t space;
	pr_class_t rngclass = bglobal.m_Thinking ? pr_botspawnmobj : pr_spawnmobj;

	if ((unsigned int)itype >= NUMMOBJTYPES)
	{
		I_Error ("Tried to spawn actor type %d\n", itype);
	}

	memset (&x, 0, (byte *)&this[1] - (byte *)&x);
	info = &mobjinfo[itype];
	type = itype;
	x = ix;
	y = iy;
	radius = info->radius;
	height = info->height;
	flags = info->flags;
	flags2 = info->flags2;
	damage = info->damage;
	health = info->spawnhealth;
	translucency = info->translucency;

	// [RH] Ugly hack:
	switch (type)
	{
	case MT_INS:
		effects = FX_VISIBILITYPULSE;
		special2 = -1;
		break;
	case MT_ROCKET:
		effects = FX_ROCKET;
		break;
	}

	if (gameskill.value != sk_nightmare)
		reactiontime = info->reactiontime;
	
	lastlook = P_Random (rngclass) % MAXPLAYERS;

	// Set the state, but do not use P_SetMobjState, because action
	// routines can't be called yet.  If the spawnstate has an action
	// routine, it will not be called.
	st = &states[info->spawnstate];
	state = st;
	tics = st->tics;
	sprite = st->sprite;
	frame = st->frame;
	touching_sectorlist = NULL;	// NULL head of sector list // phares 3/13/98

	// set subsector and/or block links
	LinkToWorld ();
	floorz = subsector->sector->floorheight;
	ceilingz = subsector->sector->ceilingheight;

	if (iz == ONFLOORZ)
	{
		z = floorz;
	}
	else if (iz == ONCEILINGZ)
	{
		z = ceilingz - height;
	}
	else if (iz == FLOATRANDZ)
	{
		space = ceilingz - info->height - floorz;
		if (space > 48*FRACUNIT)
		{
			space -= 40*FRACUNIT;
			z = ((space * P_Random(rngclass))>>8) + floorz + 40*FRACUNIT;
		}
		else
		{
			z = floorz;
		}
	}
	else if (flags2 & MF2_FLOATBOB)
	{
		z = floorz + iz;		// artifact z passed in as height
	}
	else
	{
		z = iz;
	}
}


//
// P_RemoveMobj
//
mapthing2_t		itemrespawnque[ITEMQUESIZE];
int 			itemrespawntime[ITEMQUESIZE];
int 			iquehead;
int 			iquetail;

AActor::~AActor ()
{
	if ((flags & MF_SPECIAL) && !(flags & MF_DROPPED))
	{
		if ((type != MT_INV && type != MT_INS) || (dmflags & DF_RESPAWN_SUPER))
		{
			itemrespawnque[iquehead] = spawnpoint;
			itemrespawntime[iquehead] = level.time;
			iquehead = (iquehead+1)&(ITEMQUESIZE-1);
		}

		// lose one off the end?
		if (iquehead == iquetail)
			iquetail = (iquetail+1)&(ITEMQUESIZE-1);
	}
		
	// [RH] Unlink from tid chain
	RemoveFromHash ();

	// unlink from sector and block lists
	UnlinkFromWorld ();

	// Delete all nodes on the current sector_list			phares 3/16/98
	if (sector_list)
	{
		P_DelSeclist (sector_list);
		sector_list = NULL;
	}

	// stop any playing sound
	S_RelinkSound (this, NULL);
}




//
// P_RespawnSpecials
//
void P_RespawnSpecials (void)
{
	fixed_t 			x;
	fixed_t 			y;
	fixed_t 			z;
	
	subsector_t*		ss; 
	AActor* 			mo;
	mapthing2_t* 		mthing;
	
	int 				i;

	// only respawn items in deathmatch
	if ((!deathmatch.value && !alwaysapplydmflags.value) || !(dmflags & DF_ITEMS_RESPAWN))
		return;

	// nothing left to respawn?
	if (iquehead == iquetail)
		return; 		

	// wait at least 30 seconds
	if (level.time - itemrespawntime[iquetail] < 30*TICRATE)
		return; 				

	mthing = &itemrespawnque[iquetail];
		
	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	// find which type to spawn
	for (i=0 ; i< NUMMOBJTYPES ; i++)
	{
		if (mthing->type == mobjinfo[i].doomednum)
			break;
	}
	if (mobjinfo[i].flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else if (mobjinfo[i].flags2 & MF2_SPAWNFLOAT)
		z = FLOATRANDZ;
	else if (mobjinfo[i].flags2 & MF2_FLOATBOB)
		z = mthing->z << FRACBITS;
	else
		z = ONFLOORZ;
	
	// spawn a teleport fog at the new spot
	ss = R_PointInSubsector (x, y); 
	mo = new AActor (x, y, z, MT_IFOG);
	S_Sound (mo, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE);

	// spawn it
	mo = new AActor (x, y, z, (mobjtype_t)i);
	mo->spawnpoint = *mthing;	
	mo->angle = ANG45 * (mthing->angle/45);

	if (z == ONFLOORZ)
		mo->z += mthing->z << FRACBITS;
	else if (z == ONCEILINGZ)
		mo->z -= mthing->z << FRACBITS;

	if (mo->flags2 & MF2_FLOATBOB)
	{ // Seed random starting index for bobbing motion
		mo->health = M_Random();
		mo->special1 = mthing->z << FRACBITS;
	}

	// [RH] Set the thing's special
	// On second thought, don't.
	mo->special = 0;
	//mo->special = mthing->special;
	//memcpy (mo->args, mthing->args, sizeof(mo->args));

	// pull it from the que
	iquetail = (iquetail+1)&(ITEMQUESIZE-1);
}




//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//	between levels.
//
EXTERN_CVAR (chasedemo)
extern BOOL demonew;

void P_SpawnPlayer (mapthing2_t *mthing)
{
	int		  playernum;
	player_t *p;
	AActor	 *mobj;
	int 	  i;

	// [RH] Things 4001-? are also multiplayer starts. Just like 1-4.
	//		To make things simpler, figure out which player is being
	//		spawned here.
	playernum = (mthing->type > 4000) ? mthing->type - 4001 + 4 : mthing->type - 1;

	// not playing?
	if (playernum >= MAXPLAYERS || !playeringame[playernum])
		return;

	p = &players[playernum];
	if (p->playerstate == PST_REBORN)
		G_PlayerReborn (playernum);

	mobj = new AActor (mthing->x << FRACBITS, mthing->y << FRACBITS, ONFLOORZ, MT_PLAYER);

	// set color translations for player sprites
	// [RH] Different now: MF_TRANSLATION is not used.
	mobj->translation = translationtables + 256*playernum;

	mobj->angle = ANG45 * (mthing->angle/45);
	mobj->pitch = mobj->roll = 0;
	mobj->player = p;
	mobj->health = p->health;

	//Added by MC: Identification (number in the players[MAXPLAYERS] array)
    mobj->id = playernum;

	// [RH] Set player sprite based on skin
	mobj->sprite = skins[p->userinfo.skin].sprite;

	p->fov = 90.0f;
	p->mo = p->camera = mobj;
	p->playerstate = PST_LIVE;
	p->refire = 0;
	p->damagecount = 0;
	p->bonuscount = 0;
	p->extralight = 0;
	p->fixedcolormap = 0;
	p->viewheight = VIEWHEIGHT;
	p->inconsistant = 0;
	p->attacker = NULL;

	p->momx = p->momy = 0;		// killough 10/98: initialize bobbing to 0.

	players[consoleplayer].camera = players[displayplayer].mo;

	// [RH] Allow chasecam for demo watching
	if ((demoplayback || demonew) && chasedemo.value)
		p->cheats = CF_CHASECAM;

	// setup gun psprite
	P_SetupPsprites (p);

	// give all cards in death match mode
	if (deathmatch.value)
		for (i = 0; i < NUMCARDS; i++)
			p->cards[i] = true;

	if (players[consoleplayer].camera == p->mo)
	{
		// wake up the status bar
		ST_Start ();
	}

	// [RH] If someone is in the way, kill them
	P_TeleportMove (mobj, mobj->x, mobj->y, mobj->z, true);
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
// [RH] position is used to weed out unwanted start spots
void P_SpawnMapThing (mapthing2_t *mthing, int position)
{
	int i;
	int bit;
	AActor *mobj;
	fixed_t x, y, z;

	if (mthing->type == 0 || mthing->type == -1)
		return;

	// count deathmatch start positions
	if (mthing->type == 11)
	{
		if (deathmatch_p == &deathmatchstarts[MaxDeathmatchStarts])
		{
			// [RH] Get more deathmatchstarts
			int offset = MaxDeathmatchStarts;
			MaxDeathmatchStarts *= 2;
			deathmatchstarts = (mapthing2_t *)Realloc (deathmatchstarts, MaxDeathmatchStarts * sizeof(mapthing2_t));
			deathmatch_p = &deathmatchstarts[offset];
		}
		memcpy (deathmatch_p, mthing, sizeof(*mthing));
		deathmatch_p++;
		return;
	}

	// [RH] Record polyobject-related things
	if (HexenHack)
	{
		switch (mthing->type)
		{
		case PO_HEX_ANCHOR_TYPE:
			mthing->type = PO_ANCHOR_TYPE;
			break;
		case PO_HEX_SPAWN_TYPE:
			mthing->type = PO_SPAWN_TYPE;
			break;
		case PO_HEX_SPAWNCRUSH_TYPE:
			mthing->type = PO_SPAWNCRUSH_TYPE;
			break;
		}
	}

	if (mthing->type == PO_ANCHOR_TYPE ||
		mthing->type == PO_SPAWN_TYPE ||
		mthing->type == PO_SPAWNCRUSH_TYPE)
	{
		polyspawns_t *polyspawn = new polyspawns_t;
		polyspawn->next = polyspawns;
		polyspawn->x = mthing->x << FRACBITS;
		polyspawn->y = mthing->y << FRACBITS;
		polyspawn->angle = mthing->angle;
		polyspawn->type = mthing->type;
		polyspawns = polyspawn;
		if (mthing->type != PO_ANCHOR_TYPE)
			po_NumPolyobjs++;
		return;
	}

	// check for players specially
	if (mthing->type <= 4 && mthing->type > 0)
	{
		// [RH] Only spawn spots that match position.
		if (mthing->args[0] != position)
			return;

		// save spots for respawning in network games
		playerstarts[mthing->type-1] = *mthing;
		if (!deathmatch.value)
			P_SpawnPlayer (mthing);

		return;
	}
	else if (mthing->type >= 4001 && mthing->type <= 4001 + MAXPLAYERS - 4)
	{
		// [RH] Multiplayer starts for players 5-?

		// [RH] Only spawn spots that match position.
		if (mthing->args[0] != position)
			return;

		playerstarts[mthing->type - 4001 + 4] = *mthing;
		if (!deathmatch.value)
			P_SpawnPlayer (mthing);
		return;
	}

	// [RH] sound sequence overrides
	if (mthing->type >= 1400 && mthing->type < 1410)
	{
		R_PointInSubsector (mthing->x<<FRACBITS,
			mthing->y<<FRACBITS)->sector->seqType = mthing->type - 1400;
		return;
	}
	else if (mthing->type == 1411)
	{
		int type;

		if (mthing->args[0] == 255)
			type = -1;
		else
			type = mthing->args[0];

		if (type > 63)
		{
			Printf (PRINT_HIGH, "Sound sequence %d out of range\n", type);
		}
		else
		{
			R_PointInSubsector (mthing->x << FRACBITS,
				mthing->y << FRACBITS)->sector->seqType = type;
		}
		return;
	}

	if (deathmatch.value) 
	{
		if (!(mthing->flags & MTF_DEATHMATCH))
			return;
	}
	else if (multiplayer)
	{
		if (!(mthing->flags & MTF_COOPERATIVE))
			return;
	}
	else
	{
		if (!(mthing->flags & MTF_SINGLE))
			return;
	}

	// check for apropriate skill level
	if (gameskill.value == sk_baby)
		bit = 1;
	else if (gameskill.value == sk_nightmare)
		bit = 4;
	else
		bit = 1 << ((int)gameskill.value - 1);

	if (!(mthing->flags & bit))
		return;

	// [RH] Determine if it is an old ambient thing, and if so,
	//		map it to MT_AMBIENT with the proper parameter.
	if (mthing->type >= 14001 && mthing->type <= 14064)
	{
		mthing->args[0] = mthing->type - 14000;
		mthing->type = 14065;
		i = MT_AMBIENT;
	}
	// [RH] Check if it's a particle fountain
	else if (mthing->type >= 9027 && mthing->type <= 9033)
	{
		mthing->args[0] = mthing->type - 9026;
		i = MT_FOUNTAIN;
	}
	else
	{
		// find which type to spawn
		for (i = 0; i < NUMMOBJTYPES; i++)
			if (mthing->type == mobjinfo[i].doomednum)
				break;
	}

	if (i == NUMMOBJTYPES)
	{
		// [RH] Don't die if the map tries to spawn an unknown thing
		Printf (PRINT_HIGH, "Unknown type %i at (%i, %i)\n",
				 mthing->type,
				 mthing->x, mthing->y);
		i = MT_UNKNOWNTHING;
	}
	// [RH] If the thing's corresponding sprite has no frames, also map
	//		it to the unknown thing.
	else if (sprites[states[mobjinfo[i].spawnstate].sprite].numframes == 0)
	{
		Printf (PRINT_HIGH, "Type %i at (%i, %i) has no frames\n",
				mthing->type, mthing->x, mthing->y);
		i = MT_UNKNOWNTHING;
	}

	// don't spawn keycards and players in deathmatch
	if (deathmatch.value && mobjinfo[i].flags & MF_NOTDMATCH)
		return;

	// [RH] don't spawn extra weapons in coop
	if (multiplayer && !deathmatch.value)
	{
		switch (i)
		{
		case MT_CHAINGUN:
		case MT_SHOTGUN:
		case MT_SUPERSHOTGUN:
		case MT_MISC25:		// BFG
		case MT_MISC26:		// chainsaw
		case MT_MISC27:		// rocket launcher
		case MT_MISC28:		// plasma gun
			if ((mthing->flags & (MTF_DEATHMATCH|MTF_SINGLE)) == MTF_DEATHMATCH)
				return;
			break;
		default:
			break;
		}
	}
				
	// don't spawn any monsters if -nomonsters
	if (dmflags & DF_NO_MONSTERS
		&& ( i == MT_SKULL
			 || (mobjinfo[i].flags & MF_COUNTKILL)) )
	{
		return;
	}
	
	// [RH] Other things that shouldn't be spawned depending on dmflags
	if (deathmatch.value || alwaysapplydmflags.value)
	{
		spritenum_t sprite = states[mobjinfo[i].spawnstate].sprite;

		if (dmflags & DF_NO_HEALTH)
		{
			switch (sprite)
			{
			case SPR_STIM:
			case SPR_MEDI:
			case SPR_PSTR:
			case SPR_BON1:
			case SPR_SOUL:
			case SPR_MEGA:
				return;
			default:
				break;
			}
		}
		if (dmflags & DF_NO_ITEMS)
		{
			switch (sprite)
			{
			case SPR_PINV:
			case SPR_PSTR:
			case SPR_PINS:
			case SPR_SUIT:
			case SPR_PMAP:
			case SPR_PVIS:
				return;
			default:
				break;
			}
		}
		if (dmflags & DF_NO_ARMOR)
		{
			switch (sprite)
			{
			case SPR_ARM1:
			case SPR_ARM2:
			case SPR_BON2:
			case SPR_MEGA:
				return;
			default:
				break;
			}
		}
	}

	// spawn it
	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	if (i == MT_WATERZONE)
	{
		sector_t *sec = R_PointInSubsector (x, y)->sector;
		sec->waterzone = 1;
		return;
	}
	else if (i == MT_SECRETTRIGGER)
	{
		level.total_secrets++;
	}

	if (mobjinfo[i].flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else if (mobjinfo[i].flags2 & MF2_SPAWNFLOAT)
		z = FLOATRANDZ;
	else if (mobjinfo[i].flags2 & MF2_FLOATBOB)
		z = mthing->z << FRACBITS;
	else
		z = ONFLOORZ;
	
	mobj = new AActor (x, y, z, (mobjtype_t)i);

	if (z == ONFLOORZ)
		mobj->z += mthing->z << FRACBITS;
	else if (z == ONCEILINGZ)
		mobj->z -= mthing->z << FRACBITS;
	mobj->spawnpoint = *mthing;

	if (mobj->flags2 & MF2_FLOATBOB)
	{ // Seed random starting index for bobbing motion
		mobj->health = M_Random();
		mobj->special1 = mthing->z << FRACBITS;
	}

	// [RH] Set the thing's special
	mobj->special = mthing->special;
	memcpy (mobj->args, mthing->args, sizeof(mobj->args));

	// [RH] If it's an ambient sound, activate it
	if (i == MT_AMBIENT)
		S_ActivateAmbient (mobj, mobj->args[0]);

	// [RH] If a fountain and not dormant, start it
	if (i == MT_FOUNTAIN && !(mthing->flags & MTF_DORMANT))
		mobj->effects = mobj->args[0] << FX_FOUNTAINSHIFT;

	if (mobj->tics > 0)
		mobj->tics = 1 + (P_Random (pr_spawnmapthing) % mobj->tics);
	if (mobj->flags & MF_COUNTKILL)
		level.total_monsters++;
	if (mobj->flags & MF_COUNTITEM)
		level.total_items++;

	if (i != MT_SPARK)
		mobj->angle = ANG45 * (mthing->angle/45);

	if (mthing->flags & MTF_AMBUSH)
		mobj->flags |= MF_AMBUSH;

	// [RH] Add ThingID to mobj and link it in with the others
	mobj->tid = mthing->thingid;
	mobj->AddToHash ();

	// [RH] Go dormant as needed
	if (mthing->flags & MTF_DORMANT)
		P_DeactivateMobj (mobj);
}



//
// GAME SPAWN FUNCTIONS
//


//
// P_SpawnPuff
//
extern fixed_t attackrange;
CVAR (cl_pufftype, "0", CVAR_ARCHIVE)
CVAR (cl_bloodtype, "0", CVAR_ARCHIVE)

void P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z, angle_t dir, int updown)
{
	AActor *th;
	int t;

	if (!cl_pufftype.value || (updown == 3)) {
		t = P_Random (pr_spawnpuff);
		z += (t - P_Random (pr_spawnpuff)) << 10;

		th = new AActor (x, y, z, MT_PUFF);
		th->momz = FRACUNIT;
		th->tics -= P_Random (pr_spawnpuff) & 3;

		if (th->tics < 1)
			th->tics = 1;
			
		// don't make punches spark on the wall
		if (attackrange == MELEERANGE)
			P_SetMobjState (th, S_PUFF3);
	} else {
		if (attackrange != MELEERANGE)
			P_DrawSplash2 (32, x, y, z, dir, updown, 1);
	}
}



//
// P_SpawnBlood
// 
void P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, angle_t dir, int damage)
{
	AActor *th;
	int t;

	if (cl_bloodtype.value <= 1) {
		t = P_Random (pr_spawnblood);
		z += (t - P_Random (pr_spawnblood)) << 10;
		th = new AActor (x, y, z, MT_BLOOD);
		th->momz = FRACUNIT*2;
		th->tics -= P_Random (pr_spawnblood) & 3;

		if (th->tics < 1)
			th->tics = 1;
					
		if (damage <= 12 && damage >= 9)
			P_SetMobjState (th, S_BLOOD2);
		else if (damage < 9)
			P_SetMobjState (th, S_BLOOD3);
	}

	if (cl_bloodtype.value >= 1)
		P_DrawSplash2 (32, x, y, z, dir, 2, 0);
}


//---------------------------------------------------------------------------
//
// FUNC P_HitFloor
//
//---------------------------------------------------------------------------
#define SMALLSPLASHCLIP 12<<FRACBITS;

int P_HitFloor(AActor *thing)
{
/*
	AActor *mo;
	int smallsplash=false;

	if(thing->floorz != thing->subsector->sector->floorheight)
	{ // don't splash if landing on the edge above water/lava/etc....
		return(FLOOR_SOLID);
	}

	// Things that don't splash go here
	switch(thing->type)
	{
		case MT_LEAF1:
		case MT_LEAF2:
//		case MT_BLOOD:			// I set these to low mass -- pm
//		case MT_BLOODSPLATTER:
		case MT_SPLASH:
		case MT_SLUDGECHUNK:
			return(FLOOR_SOLID);
		default:
			break;
	}

	// Small splash for small masses
	if (thing->info->mass < 10) smallsplash = true;

	switch(P_GetThingFloorType(thing))
	{
		case FLOOR_WATER:
			if (smallsplash)
			{
				mo=new AActor(thing->x, thing->y, ONFLOORZ, MT_SPLASHBASE);
				if (mo) mo->floorclip += SMALLSPLASHCLIP;
				S_StartSound(mo, SFX_AMBIENT10);	// small drip
			}
			else
			{
				mo = new AActor(thing->x, thing->y, ONFLOORZ, MT_SPLASH);
				mo->target = thing;
				mo->momx = (P_Random()-P_Random())<<8;
				mo->momy = (P_Random()-P_Random())<<8;
				mo->momz = 2*FRACUNIT+(P_Random()<<8);
				mo = new AActor(thing->x, thing->y, ONFLOORZ, MT_SPLASHBASE);
				if (thing->player) P_NoiseAlert(thing, thing);
				S_StartSound(mo, SFX_WATER_SPLASH);
			}
			return(FLOOR_WATER);
		case FLOOR_LAVA:
			if (smallsplash)
			{
				mo=new AActor(thing->x, thing->y, ONFLOORZ, MT_LAVASPLASH);
				if (mo) mo->floorclip += SMALLSPLASHCLIP;
			}
			else
			{
				mo = new AActor(thing->x, thing->y, ONFLOORZ, MT_LAVASMOKE);
				mo->momz = FRACUNIT+(P_Random()<<7);
				mo = new AActor(thing->x, thing->y, ONFLOORZ, MT_LAVASPLASH);
				if (thing->player) P_NoiseAlert(thing, thing);
			}
			S_StartSound(mo, SFX_LAVA_SIZZLE);
			if(thing->player && leveltime&31)
			{
				P_DamageMobj(thing, &LavaInflictor, NULL, 5);
			}
			return(FLOOR_LAVA);
		case FLOOR_SLUDGE:
			if (smallsplash)
			{
				mo = new AActor(thing->x, thing->y, ONFLOORZ,
					MT_SLUDGESPLASH);
				if (mo) mo->floorclip += SMALLSPLASHCLIP;
			}
			else
			{
				mo = new AActor(thing->x, thing->y, ONFLOORZ, MT_SLUDGECHUNK);
				mo->target = thing;
				mo->momx = (P_Random()-P_Random())<<8;
				mo->momy = (P_Random()-P_Random())<<8;
				mo->momz = FRACUNIT+(P_Random()<<8);
				mo = new AActor(thing->x, thing->y, ONFLOORZ, 
					MT_SLUDGESPLASH);
				if (thing->player) P_NoiseAlert(thing, thing);
			}
			S_StartSound(mo, SFX_SLUDGE_GLOOP);
			return(FLOOR_SLUDGE);
	}
	return(FLOOR_SOLID);
*/
	return 0;
}

//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//	and possibly explodes it right there.
//
BOOL P_CheckMissileSpawn (AActor* th)
{
	th->tics -= P_Random (pr_checkmissilespawn) & 3;
	if (th->tics < 1)
		th->tics = 1;

	// [RH] Give the missile time to get away from the shooter
	th->targettic = 10;
	
	// move a little forward so an angle can
	// be computed if it immediately explodes
	th->x += th->momx>>1;
	th->y += th->momy>>1;
	th->z += th->momz>>1;

	// killough 3/15/98: no dropoff (really = don't care for missiles)

	if (!P_TryMove (th, th->x, th->y, false))
	{
		P_ExplodeMissile (th);
		return false;
	}
	return true;
}


//
// P_SpawnMissile
//
AActor *P_SpawnMissile (AActor *source, AActor *dest, mobjtype_t type)
{
	AActor *th = new AActor (source->x, source->y, source->z + 4*8*FRACUNIT, type);
	
	if (th->info->seesound)
		S_Sound (th, CHAN_VOICE, th->info->seesound, 1, ATTN_NORM);

	th->target = source;		// where it came from

	vec3_t velocity;
	float speed = FIXED2FLOAT (th->info->speed);

	velocity[0] = FIXED2FLOAT(dest->x - source->x);
	velocity[1] = FIXED2FLOAT(dest->y - source->y);
	velocity[2] = FIXED2FLOAT(dest->z - source->z);
	VectorNormalize (velocity);
	th->momx = FLOAT2FIXED(velocity[0] * speed);
	th->momy = FLOAT2FIXED(velocity[1] * speed);
	th->momz = FLOAT2FIXED(velocity[2] * speed);

	// fuzzy player: rotate velocity vector in 2D
	if (dest->flags & MF_SHADOW)
	{
		int t = P_Random (pr_spawnmissile);
		angle_t an = (t - P_Random (pr_spawnmissile)) << 20;
		an >>= ANGLETOFINESHIFT;
		
		fixed_t newx = FixedMul (th->momx, finecosine[an]) - FixedMul (th->momy, finesine[an]);
		fixed_t newy = FixedMul (th->momx, finesine[an]) + FixedMul (th->momy, finecosine[an]);
		th->momx = newx;
		th->momy = newy;
	}

	th->angle = R_PointToAngle2 (0, 0, th->momx, th->momy);

	P_CheckMissileSpawn (th);

	return th;
}


//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//
void P_SpawnPlayerMissile (AActor *source, mobjtype_t type)
{
	angle_t an;
	fixed_t slope;
	fixed_t pitchslope = finetangent[FINEANGLES/4-(source->pitch>>ANGLETOFINESHIFT)];

	// see which target is to be aimed at
	an = source->angle;

	if (source->player &&
		source->player->userinfo.aimdist == 0 &&
		!(dmflags & DF_NO_FREELOOK))
	{
		slope = pitchslope;
	}
	else
	{
		slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
		
		if (!linetarget)
		{
			an += 1<<26;
			slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);

			if (!linetarget)
			{
				an -= 2<<26;
				slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
			}

			if (!linetarget)
			{
				an = source->angle;
				// [RH] Use pitch to calculate slope instead of 0.
				slope = pitchslope;
			}
		}

		if (linetarget && source->player)
		{
			if (!(dmflags & DF_NO_FREELOOK)
				&& abs(slope - pitchslope) > source->player->userinfo.aimdist)
			{
				an = source->angle;
				slope = pitchslope;
			}
		}
	}

	AActor *th = new AActor (source->x, source->y, source->z + 4*8*FRACUNIT, type);

	if (th->info->seesound)
		S_Sound (th, CHAN_VOICE, th->info->seesound, 1, ATTN_NORM);

	th->target = source;
	th->angle = an;

	vec3_t velocity;
	float speed = FIXED2FLOAT (th->info->speed);

	velocity[0] = FIXED2FLOAT (finecosine[an>>ANGLETOFINESHIFT]);
	velocity[1] = FIXED2FLOAT (finesine[an>>ANGLETOFINESHIFT]);
	velocity[2] = FIXED2FLOAT (slope);

	VectorNormalize (velocity);

	th->momx = FLOAT2FIXED (velocity[0] * speed);
	th->momy = FLOAT2FIXED (velocity[1] * speed);
	th->momz = FLOAT2FIXED (velocity[2] * speed);

	P_CheckMissileSpawn (th);
}

// [RH] Throw gibs around (based on Q2 code)

#if 0	// Not used right now (Note that this code considers vectors to
		// be expressed as fixed_t's.)
void VelocityForDamage (int damage, vec3_t v)
{
	v[0] = 8 * crandom(pr_vel4dmg);
	v[1] = 8 * crandom(pr_vel4dmg);
	v[2] = 8 * FRACUNIT + (fixed_t)(P_Random(pr_vel4dmg) * (6553600/256));

	if (damage < 50)
		VectorScale (v, 0.7f, v);
	else 
		VectorScale (v, 1.2f, v);
}

void ClipGibVelocity (AActor *ent)
{
	if (ent->momx < -15 * FRACUNIT)
		ent->momx = -15 * FRACUNIT;
	else if (ent->momx > 15 * FRACUNIT)
		ent->momx = 15 * FRACUNIT;
	if (ent->momy < -15 * FRACUNIT)
		ent->momy = -15 * FRACUNIT;
	else if (ent->momy > 15 * FRACUNIT)
		ent->momy = 15 * FRACUNIT;
	if (ent->momz < 6 * FRACUNIT)
		ent->momz = 6 * FRACUNIT;	// always some upwards
	else if (ent->momz > 10 * FRACUNIT)
		ent->momz = 10 * FRACUNIT;
}

void ThrowGib (AActor *self, mobjtype_t gibtype, int damage)
{
	AActor *gib;
	vec3_t	vd;
	vec3_t	selfvel;
	vec3_t	gibvel;

	gib = new AActor (self->x + crandom(pr_throwgib) * (self->radius >> (FRACBITS + 1)),
					  self->y + crandom(pr_throwgib) * (self->radius >> (FRACBITS + 1)),
					  self->z + (self->height >> 1) + crandom(pr_throwgib) * (self->height >> (FRACBITS + 1)),
					  gibtype);

	VelocityForDamage (damage, vd);
	selfvel[0] = self->momx;
	selfvel[1] = self->momy;
	selfvel[2] = self->momz;
	VectorMA (selfvel, 1.0, vd, gibvel);
	gib->momx = gibvel[0];
	gib->momy = gibvel[1];
	gib->momz = gibvel[2];
	ClipGibVelocity (gib);
}
#else
void ThrowGib (AActor *self, mobjtype_t gibtype, int damage)
{
}
#endif

bool AActor::IsTeammate (AActor *other)
{
	if (!player || !other || !other->player)
		return false;
	if (!deathmatch.value)
		return true;
	if (teamplay.value && other->player->userinfo.team[0] &&
		!stricmp (player->userinfo.team, other->player->userinfo.team))
	{
		return true;
	}
	return false;
}
