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


#include "m_alloc.h"

#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"
#include "sounds.h"

#include "st_stuff.h"
#include "hu_stuff.h"

#include "s_sound.h"

#include "doomstat.h"

#include "v_video.h"
#include "c_cvars.h"

cvar_t *sv_gravity;
cvar_t *sv_friction;


void G_PlayerReborn (int player);
void P_SpawnMapThing (mapthing2_t *mthing);


//
// P_SetMobjState
// Returns true if the mobj is still present.
//
BOOL P_SetMobjState (mobj_t *mobj, statenum_t state)
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
			P_RemoveMobj (mobj);
			ret = false;
			break;					// killough 4/9/98
		}

		st = &states[state];
		mobj->state = st;
		mobj->tics = st->tics;
		mobj->sprite = st->sprite;
		mobj->frame = st->frame;

		// Modified handling.
		// Call action functions when the state is set

		if (st->action.acp1)
			st->action.acp1(mobj);

		seenstate[state] = 1 + st->nextstate;		// killough 4/9/98

		state = st->nextstate;
	} while (!mobj->tics && !seenstate[state]);		// killough 4/9/98

	if (ret && !mobj->tics)  // killough 4/9/98: detect state cycles
		Printf ("Warning: State Cycle Detected\n");

	if (!--recursion)
		for (;(state=seenstate[i]);i=state-1)
			seenstate[i] = 0;  // killough 4/9/98: erase memory of states

	return ret;
}


//
// P_ExplodeMissile  
//
void P_ExplodeMissile (mobj_t* mo)
{
	mo->momx = mo->momy = mo->momz = 0;

	P_SetMobjState (mo, mobjinfo[mo->type].deathstate);
	// [RH] If the object isn't translucent, don't change it.
	// Otherwise, make it 50% translucent.
	if (TransTable && !(mo->flags & MF_TRANSLUCBITS))
		mo->flags |= MF_TRANSLUC50;

	mo->tics -= P_Random (pr_explodemissile) & 3;

	if (mo->tics < 1)
		mo->tics = 1;

	mo->flags &= ~MF_MISSILE;

	if (mo->info->deathsound)
		S_StartSound (mo, mo->info->deathsound);
}


//
// P_XYMovement  
//
#define STOPSPEED				0x1000
#define FRICTION				0xe800

void P_XYMovement (mobj_t* mo) 
{		
	fixed_t 	ptryx;
	fixed_t 	ptryy;
	player_t*	player;
	fixed_t 	xmove;
	fixed_t 	ymove;
						
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
				
	if (mo->momx > MAXMOVE)
		mo->momx = MAXMOVE;
	else if (mo->momx < -MAXMOVE)
		mo->momx = -MAXMOVE;

	if (mo->momy > MAXMOVE)
		mo->momy = MAXMOVE;
	else if (mo->momy < -MAXMOVE)
		mo->momy = -MAXMOVE;
				
	xmove = mo->momx;
	ymove = mo->momy;
		
	do
	{
		if (xmove > MAXMOVE/2 || ymove > MAXMOVE/2)
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
				
		if (!P_TryMove (mo, ptryx, ptryy))
		{
			// blocked move
			if (mo->player)
			{	// try to slide along it
				P_SlideMove (mo);
			}
			else if (mo->flags & MF_MISSILE)
			{
				// explode a missile
				if (ceilingline &&
					ceilingline->backsector &&
					ceilingline->backsector->ceilingpic == skyflatnum)
					if (olddemo ||
						mo->z > ceilingline->backsector->ceilingheight) //killough
					{
						// Hack to prevent missiles exploding against the sky.
						// Does not handle sky floors.
						P_RemoveMobj (mo);
						return;
					}
				P_ExplodeMissile (mo);
			}
			else
				mo->momx = mo->momy = 0;
		}
	} while (xmove || ymove);
	
	// slow down
	if (player && player->cheats & CF_NOMOMENTUM)
	{
		// debug option for no sliding at all
		mo->momx = mo->momy = 0;
		return;
	}

	if (mo->flags & (MF_MISSILE | MF_SKULLFLY) )
		return; 		// no friction for missiles ever
				
	if (!P_FindFloor (mo))	// [RH] Z-Check
		return; 		// no friction when airborne

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

	if (mo->momx > -STOPSPEED
		&& mo->momx < STOPSPEED
		&& mo->momy > -STOPSPEED
		&& mo->momy < STOPSPEED
		&& (!player
			|| (player->cmd.ucmd.forwardmove== 0
				&& player->cmd.ucmd.sidemove == 0 ) ) )
	{
		// if in a walking frame, stop moving
		if ( player&&(unsigned)((player->mo->state - states)- S_PLAY_RUN1) < 4)
			P_SetMobjState (player->mo, S_PLAY);
		
		mo->momx = 0;
		mo->momy = 0;
	}
	else
	{
		mo->momx = FixedMul (mo->momx, FRICTION);
		mo->momy = FixedMul (mo->momy, FRICTION);
	}
}

//
// P_ZMovement
//
void P_ZMovement (mobj_t* mo)
{
	fixed_t 	dist;
	fixed_t 	delta;
	mobj_t		*other;
	
	// check for smooth step up
	if (mo->player && mo->z < mo->floorz)
	{
		mo->player->viewheight -= mo->floorz-mo->z;

		mo->player->deltaviewheight
			= (VIEWHEIGHT - mo->player->viewheight)>>3;
	}

	// adjust height
	mo->z += mo->momz;
		
	if ( mo->flags & MF_FLOAT
		 && mo->target)
	{
		// float down towards target if too close
		if ( !(mo->flags & MF_SKULLFLY)
			 && !(mo->flags & MF_INFLOAT) )
		{
			dist = P_AproxDistance (mo->x - mo->target->x,
									mo->y - mo->target->y);
			
			delta =(mo->target->z + (mo->height>>1)) - mo->z;

			if (delta<0 && dist < -(delta*3) )
				mo->z -= FLOATSPEED;
			else if (delta>0 && dist < (delta*3) )
				mo->z += FLOATSPEED;					
		}
		
	}
	
	// clip movement
	other = P_FindFloor (mo);			// [RH] Z-Check
	if ((mo->flags & MF_MISSILE) && (other == mo->target) && (gametic < (signed)mo->targettic))
		other = NULL;

	if (other) {
		// hit the floor ([RH] or something else)

		// Note (id):
		//	somebody left this after the setting momz to 0,
		//	kinda useless there.
		if (mo->flags & MF_SKULLFLY)
		{
			// the skull slammed into something
			mo->momz = -mo->momz;
		}
		
		if (mo->momz < 0)
		{
			if (mo->player
				&& mo->momz < (fixed_t)(sv_gravity->value * -655.36))		
			{
				// [RH] Andy Kempling's fall damage mod.
				//		(adapted to work with ZDoom)
				// calculate the number of broken bones... (joke)
				// damage player according to the height fallen.
				//
				if (dmflags & (DF_YES_FALLING|DF_YES_FALLING_LOTS)) {
					fixed_t minvelocity;
					fixed_t damage;
					float divisor;

					if (dmflags & DF_YES_FALLING_LOTS) {
						// Original values
						minvelocity = -741455*4/3;
						divisor = -26214400.0f;
					} else {
						// Not-quite-so-damaging values
						minvelocity = -741455*5/3;
						divisor = -39321600.0f;
					}

					if (mo->momz < minvelocity) {
						damage = (fixed_t)((float)(mo->momz) * sv_gravity->value / divisor);
						if (other != (mobj_t *)-1) {
							// [RH] We landed on another thing. Give half the damage to it.
							damage >>= 1;
							P_DamageMobj (other, mo, mo, damage, MOD_FALLXFER);
						}
						if (mo->player)	{
							// Monsters don't take falling damage
							P_DamageMobj(mo, mo, mo, damage, MOD_FALLING);
						}
					}
				}

				// Squat down.
				// Decrease viewheight for a moment
				// after hitting the ground (hard),
				// and utter appropriate sound.
				mo->player->deltaviewheight = mo->momz>>3;
				S_StartSound (mo, sfx_oof);
			}
			mo->momz = 0;
		}
		if ( (mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP) )
		{
			if (other == (mobj_t *)-1) {
				if (!olddemo && mo->subsector->sector->floorpic == skyflatnum) {
					// [RH] Just remove the missile without exploding it
					//		if this is a sky floor.
					P_RemoveMobj (mo);
					return;
				}
				mo->z = mo->floorz;
			}
			P_ExplodeMissile (mo);
			return;
		}

		P_StandOnThing (mo, other);
	}
	else if (! (mo->flags & MF_NOGRAVITY) )
	{
		if (mo->momz == 0)
			mo->momz = (fixed_t)(sv_gravity->value * -163.84);
		else
			mo->momz -= (fixed_t)(sv_gravity->value * 81.92);
	}
		
	other = P_FindCeiling (mo);			// [RH] Z-Check
	if ((mo->flags & MF_MISSILE) && (other == mo->target) && (gametic < (signed)mo->targettic))
		other = NULL;

	if (other)
	{
		// hit the ceiling (or something else)
		if (mo->momz > 0)
			mo->momz = 0;

		if ( (mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP) )
		{
			if (other == (mobj_t *)-1) {
				if (!olddemo && mo->subsector->sector->ceilingpic == skyflatnum) {
					// [RH] Just remove the missile without exploding it
					//		if this is a sky ceiling.
					P_RemoveMobj (mo);
					return;
				}
				mo->z = mo->ceilingz - mo->height;
			}
			P_ExplodeMissile (mo);
			return;
		}

		{
			if (other == (mobj_t *)-1)
				mo->z = mo->ceilingz - mo->height;
			else
				mo->z = other->z - mo->height;
		}

		if (mo->flags & MF_SKULLFLY)
		{		// the skull slammed into something
			mo->momz = -mo->momz;
		}
		
	}
} 



//
// P_NightmareRespawn
//
void P_NightmareRespawn (mobj_t* mobj)
{
	fixed_t 			x;
	fixed_t 			y;
	fixed_t 			z;
	subsector_t*		ss;
	mobj_t* 			mo;
	mapthing2_t* 		mthing;
	int					onfloor;

	x = mobj->spawnpoint.x << FRACBITS;
	y = mobj->spawnpoint.y << FRACBITS;
	z = mobj->spawnpoint.z << FRACBITS;

	// something is occupying it's position?
	if (!P_CheckPosition (mobj, x, y) ) 
		return;		// no respawn

	// spawn a teleport fog at old spot
	// because of removal of the body?
	mo = P_SpawnMobj (mobj->x,
					  mobj->y,
					  0, MT_TFOG, ONFLOORZ);
	// initiate teleport sound
	S_StartSound (mo, sfx_telept);

	ss = R_PointInSubsector (x,y);

	// spawn a teleport fog at the new spot
	if (mobj->info->flags & MF_SPAWNCEILING)
		onfloor = ONCEILINGZ;
	else
		onfloor = ONFLOORZ;

	mo = P_SpawnMobj (x, y, z, MT_TFOG, onfloor);

	S_StartSound (mo, sfx_telept);

	// spawn the new monster
	mthing = &mobj->spawnpoint;

	// spawn it
	// inherit attributes from deceased one
	mo = P_SpawnMobj (x,y,z, mobj->type, onfloor);
	mo->spawnpoint = mobj->spawnpoint;
	mo->angle = ANG45 * (mthing->angle/45);

	if (mthing->flags & MTF_AMBUSH)
		mo->flags |= MF_AMBUSH;

	mo->reactiontime = 18;

	// remove the old monster,
	P_RemoveMobj (mobj);
}


//
// P_MobjThinker
//
void P_MobjThinker (mobj_t* mobj)
{
	// momentum movement
	if (mobj->momx
		|| mobj->momy
		|| (mobj->flags&MF_SKULLFLY) )
	{
		P_XYMovement (mobj);

		// FIXME: decent NOP/NULL/Nil function pointer please.
		if (mobj->thinker.function.acv == (actionf_v) (-1))
			return; 			// mobj was removed
	}
	if ( (mobj->z != mobj->floorz)
		 || mobj->momz )
	{
		P_ZMovement (mobj);
		
		// FIXME: decent NOP/NULL/Nil function pointer please.
		if (mobj->thinker.function.acv == (actionf_v) (-1))
			return; 			// mobj was removed
	}

	
	// cycle through states,
	// calling action functions at transitions
	if (mobj->tics != -1)
	{
		mobj->tics--;
				
		// you can cycle through multiple states in a tic
		if (!mobj->tics)
			if (!P_SetMobjState (mobj, mobj->state->nextstate) )
				return; 		// freed itself
	}
	else
	{
		// check for nightmare respawn
		if (! (mobj->flags & MF_COUNTKILL) )
			return;

		if (!respawnmonsters)
			return;

		mobj->movecount++;

		if (mobj->movecount < 12*TICRATE)
			return;

		if ( level.time&31 )
			return;

		if (P_Random (pr_mobjthinker) > 4)
			return;

		P_NightmareRespawn (mobj);
	}

}


//
// P_SpawnMobj
//	[RH] Since MapThings can now be stored with their own z-position,
//		 we now use a separate parameter to indicate if it should be
//		 spawned relative to the floor or ceiling.
//
mobj_t *P_SpawnMobj (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type, int onfloor)
{
	mobj_t* 	mobj;
	state_t*	st;
	mobjinfo_t* info;
		
	mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
	memset (mobj, 0, sizeof (*mobj));
	info = &mobjinfo[type];
		
	mobj->type = type;
	mobj->info = info;
	mobj->x = x;
	mobj->y = y;
	mobj->radius = info->radius;
	mobj->height = info->height;
	mobj->flags = info->flags;
	mobj->health = info->spawnhealth;

	if (gameskill->value != sk_nightmare)
		mobj->reactiontime = info->reactiontime;
	
	mobj->lastlook = P_Random (pr_spawnmobj) % MAXPLAYERS;
	// do not set the state with P_SetMobjState,
	// because action routines can not be called yet
	st = &states[info->spawnstate];

	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;

	// set subsector and/or block links
	P_SetThingPosition (mobj);
		
	mobj->floorz = mobj->subsector->sector->floorheight;
	mobj->ceilingz = mobj->subsector->sector->ceilingheight;

	if (onfloor == ONFLOORZ)
		mobj->z = mobj->floorz + z;
	else if (onfloor == ONCEILINGZ)
		mobj->z = mobj->ceilingz - mobj->height - z;
	else 
		mobj->z = z;

	mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker;
		
	P_AddThinker (&mobj->thinker);

	// [RH] If it's an ambient sound, activate it
	if (type >= MT_AMBIENT0 && type <= MT_AMBIENT63)
		S_ActivateAmbient (mobj, type - MT_AMBIENT0);

	return mobj;
}


//
// P_RemoveMobj
//
mapthing2_t		itemrespawnque[ITEMQUESIZE];
int 			itemrespawntime[ITEMQUESIZE];
int 			iquehead;
int 			iquetail;


void P_RemoveMobj (mobj_t* mobj)
{
	if ((mobj->flags & MF_SPECIAL)
		&& !(mobj->flags & MF_DROPPED)
		&& (mobj->type != MT_INV)
		&& (mobj->type != MT_INS))
	{
		itemrespawnque[iquehead] = mobj->spawnpoint;
		itemrespawntime[iquehead] = level.time;
		iquehead = (iquehead+1)&(ITEMQUESIZE-1);

		// lose one off the end?
		if (iquehead == iquetail)
			iquetail = (iquetail+1)&(ITEMQUESIZE-1);
	}
		
	// unlink from sector and block lists
	P_UnsetThingPosition (mobj);
	
	// stop any playing sound
	S_StopSound (mobj);
	
	// free block
	P_RemoveThinker ((thinker_t*)mobj);
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
	mobj_t* 			mo;
	mapthing2_t* 		mthing;
	int					onfloor;
	
	int 				i;

	// only respawn items in deathmatch
	if (!deathmatch->value || !(dmflags & DF_ITEMS_RESPAWN))
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
	z = mthing->z << FRACBITS;
		  
	// find which type to spawn
	for (i=0 ; i< NUMMOBJTYPES ; i++)
	{
		if (mthing->type == mobjinfo[i].doomednum)
			break;
	}
	
	if (mobjinfo[i].flags & MF_SPAWNCEILING)
		onfloor = ONCEILINGZ;
	else
		onfloor = ONFLOORZ;

	// spawn a teleport fog at the new spot
	ss = R_PointInSubsector (x,y); 
	mo = P_SpawnMobj (x, y, z, MT_IFOG, onfloor);
	S_StartSound (mo, sfx_itmbk);

	// spawn it
	mo = P_SpawnMobj (x,y,z, i, onfloor);
	mo->spawnpoint = *mthing;	
	mo->angle = ANG45 * (mthing->angle/45);

	// pull it from the que
	iquetail = (iquetail+1)&(ITEMQUESIZE-1);
}




//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//	between levels.
//
void P_SpawnPlayer (mapthing2_t *mthing)
{
	player_t *p;
	fixed_t   x;
	fixed_t   y;
	fixed_t   z;

	mobj_t	 *mobj;

	int 	  i;

	// not playing?
	// [RH] Things 4001-? are also multiplayer starts.
	if (mthing->type > 4000) {
		if (!playeringame[mthing->type - 4001 + 4])
			return;
		else {
			p = &players[mthing->type - 4001 + 4];
			if (p->playerstate == PST_REBORN)
				G_PlayerReborn (mthing->type - 4001 + 4);
		}
	} else {
		if (!playeringame[mthing->type-1])
			return;
		else {
			p = &players[mthing->type-1];
			if (p->playerstate == PST_REBORN)
				G_PlayerReborn (mthing->type - 1);
		}
	}

	x	 = mthing->x << FRACBITS;
	y	 = mthing->y << FRACBITS;
	z	 = mthing->z << FRACBITS;
	mobj = P_SpawnMobj (x,y,z, MT_PLAYER, ONFLOORZ);

	// set color translations for player sprites
	// [RH] Different now: MF_TRANSLATION is not used.
	mobj->palette = (struct palette_s *)(translationtables + (mthing->type-1)*256);

	mobj->angle = ANG45 * (mthing->angle/45);
	mobj->pitch = mobj->roll = 0;
	mobj->player = p;
	mobj->health = p->health;

	p->mo = mobj;
	p->playerstate = PST_LIVE;
	p->refire = 0;
	p->message = NULL;
	p->damagecount = 0;
	p->bonuscount = 0;
	p->extralight = 0;
	p->fixedcolormap = 0;
	p->viewheight = VIEWHEIGHT;

	// setup gun psprite
	P_SetupPsprites (p);

	// give all cards in death match mode
	if (deathmatch->value)
		for (i=0 ; i<NUMCARDS ; i++)
			p->cards[i] = true;

	if (consoleplayer ==
		(mthing->type > 4000 ? mthing->type - 4001 + 4 : mthing->type - 1))
	{
		// wake up the status bar
		ST_Start ();
		// wake up the heads up text
		HU_Start ();
	}
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
void P_SpawnMapThing (mapthing2_t *mthing)
{
	int 				i;
	int 				bit;
	mobj_t* 			mobj;
	fixed_t 			x;
	fixed_t 			y;
	fixed_t 			z;
	int					onfloor;

	// count deathmatch start positions
	if (mthing->type == 11)
	{
		if (deathmatch_p == &deathmatchstarts[MaxDeathmatchStarts]) {
			// [RH] Get more deathmatchstarts
			MaxDeathmatchStarts += 8;
			deathmatchstarts = Realloc (deathmatchstarts, MaxDeathmatchStarts * sizeof(mapthing2_t));
			deathmatch_p = &deathmatchstarts[MaxDeathmatchStarts - 8];
		}
		memcpy (deathmatch_p, mthing, sizeof(*mthing));
		deathmatch_p++;
		return;
	}
		
	// check for players specially
	if (mthing->type <= 4 && mthing->type > 0)
	{
		// save spots for respawning in network games
		playerstarts[mthing->type-1] = *mthing;
		if (!deathmatch->value)
			P_SpawnPlayer (mthing);

		return;
	} else if (mthing->type >= 4001 && mthing->type <= 4001 + MAXPLAYERS - 4) {
		// [RH] Multiplayer starts for players 5-?
		playerstarts[mthing->type - 4001 + 4] = *mthing;
		if (!deathmatch->value)
			P_SpawnPlayer (mthing);
		return;
	}

	if (netgame) {
		if (deathmatch->value) {
			if (!(mthing->flags & MTF_DEATHMATCH))
				return;
		} else {
			if (!(mthing->flags & MTF_COOPERATIVE))
				return;
		}
	} else if (!(mthing->flags & MTF_SINGLE))
		return;
				
	// check for apropriate skill level
	if (gameskill->value == sk_baby)
		bit = 1;
	else if (gameskill->value == sk_nightmare)
		bit = 4;
	else
		bit = 1 << ((int)gameskill->value - 1);

	if (!(mthing->flags & bit) )
		return;
		
	// find which type to spawn
	for (i=0 ; i< NUMMOBJTYPES ; i++)
		if (mthing->type == mobjinfo[i].doomednum)
			break;
		
	if (i==NUMMOBJTYPES) {
		// [RH] Don't die if the map tries to spawn an unknown thing
		Printf ("Unknown type %i at (%i, %i)\n",
				 mthing->type,
				 mthing->x, mthing->y);
		i = MT_UNKNOWNTHING;
	}
	// [RH] If the thing's corresponding sprite has no frames, also map
	//		it to the unknown thing.
	else if (sprites[states[mobjinfo[i].spawnstate].sprite].numframes == 0) {
		Printf ("Type %i at (%i, %i) has no frames\n",
				mthing->type, mthing->x, mthing->y);
		i = MT_UNKNOWNTHING;
	}

	// don't spawn keycards and players in deathmatch
	if (deathmatch->value && mobjinfo[i].flags & MF_NOTDMATCH)
		return;
				
	// don't spawn any monsters if -nomonsters
	if (dmflags & DF_NO_MONSTERS
		&& ( i == MT_SKULL
			 || (mobjinfo[i].flags & MF_COUNTKILL)) )
	{
		return;
	}
	
	// [RH] Other things that shouldn't be spawned depending on dmflags
	if (deathmatch->value) {
		spritenum_t sprite = states[mobjinfo[i].spawnstate].sprite;

		if (dmflags & DF_NO_HEALTH) {
			switch (sprite) {
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
		if (dmflags & DF_NO_ITEMS) {
			switch (sprite) {
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
		if (dmflags & DF_NO_ARMOR) {
			switch (sprite) {
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
	z = mthing->z << FRACBITS;

	if (mobjinfo[i].flags & MF_SPAWNCEILING)
		onfloor = ONCEILINGZ;
	else
		onfloor = ONFLOORZ;
	
	mobj = P_SpawnMobj (x,y,z, i, onfloor);
	mobj->spawnpoint = *mthing;

	if (mobj->tics > 0)
		mobj->tics = 1 + (P_Random (pr_spawnmapthing) % mobj->tics);
	if (mobj->flags & MF_COUNTKILL)
		level.total_monsters++;
	if (mobj->flags & MF_COUNTITEM)
		level.total_items++;
				
	mobj->angle = ANG45 * (mthing->angle/45);
	if (mthing->flags & MTF_AMBUSH)
		mobj->flags |= MF_AMBUSH;
}



//
// GAME SPAWN FUNCTIONS
//


//
// P_SpawnPuff
//
extern fixed_t attackrange;

void P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z)
{
	mobj_t *th;
	int t;

	t = P_Random (pr_spawnpuff);
	z += (t - P_Random (pr_spawnpuff)) << 10;

	th = P_SpawnMobj (x,y,z, MT_PUFF, 0);
	th->momz = FRACUNIT;
	th->tics -= P_Random (pr_spawnpuff) & 3;

	if (th->tics < 1)
		th->tics = 1;
		
	// don't make punches spark on the wall
	if (attackrange == MELEERANGE)
		P_SetMobjState (th, S_PUFF3);
}



//
// P_SpawnBlood
// 
void
P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage)
{
	mobj_t *th;
	int t;

	t = P_Random (pr_spawnblood);
	z += (t - P_Random (pr_spawnblood)) << 10;
	th = P_SpawnMobj (x,y,z, MT_BLOOD, 0);
	th->momz = FRACUNIT*2;
	th->tics -= P_Random (pr_spawnblood) & 3;

	if (th->tics < 1)
		th->tics = 1;
				
	if (damage <= 12 && damage >= 9)
		P_SetMobjState (th,S_BLOOD2);
	else if (damage < 9)
		P_SetMobjState (th,S_BLOOD3);
}



//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//	and possibly explodes it right there.
//
void P_CheckMissileSpawn (mobj_t* th)
{
	th->tics -= P_Random (pr_checkmissilespawn) & 3;
	if (th->tics < 1)
		th->tics = 1;

	// Give the missile time to get away from the shooter
	th->targettic = gametic + 10;
	
	// move a little forward so an angle can
	// be computed if it immediately explodes
	th->x += th->momx>>1;
	th->y += th->momy>>1;
	th->z += th->momz>>1;

	if (!P_TryMove (th, th->x, th->y)) {
		P_ExplodeMissile (th);
	}
}


//
// P_SpawnMissile
//
mobj_t *P_SpawnMissile (mobj_t *source, mobj_t *dest, mobjtype_t type)
{
	mobj_t* 	th;
	angle_t 	an;
	int 		dist;

	th = P_SpawnMobj (source->x,
					  source->y,
					  source->z + 4*8*FRACUNIT, type, 0);
	
	if (th->info->seesound)
		S_StartSound (th, th->info->seesound);

	th->target = source;		// where it came from
	an = R_PointToAngle2 (source->x, source->y, dest->x, dest->y);		

	// fuzzy player
	if (dest->flags & MF_SHADOW) {
		int t = P_Random (pr_spawnmissile);
		an += (t - P_Random (pr_spawnmissile)) << 20;
	}

	th->angle = an;
	an >>= ANGLETOFINESHIFT;
	th->momx = FixedMul (th->info->speed, finecosine[an]);
	th->momy = FixedMul (th->info->speed, finesine[an]);
		
	dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
	dist = dist / th->info->speed;

	if (dist < 1)
		dist = 1;

	th->momz = (dest->z - source->z) / dist;
	P_CheckMissileSpawn (th);
		
	return th;
}


//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//
void P_SpawnPlayerMissile (mobj_t *source, mobjtype_t type)
{
	mobj_t* 	th;
	angle_t 	an;
	
	fixed_t 	x;
	fixed_t 	y;
	fixed_t 	z;
	fixed_t 	slope;
	fixed_t		pitchslope;

	pitchslope = FixedMul (source->pitch, -40960);

	// see which target is to be aimed at
	an = source->angle;

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
		if (!(dmflags & DF_NO_FREELOOK)
			&& abs(slope - pitchslope) > source->player->userinfo->aimdist) {
			an = source->angle;
			slope = pitchslope;
		}

	x = source->x;
	y = source->y;
	z = source->z + 4*8*FRACUNIT;
		
	th = P_SpawnMobj (x,y,z, type, 0);

	if (th->info->seesound)
		S_StartSound (th, th->info->seesound);

	th->target = source;
	th->angle = an;
	th->momx = FixedMul( th->info->speed,
						 finecosine[an>>ANGLETOFINESHIFT]);
	th->momy = FixedMul( th->info->speed,
						 finesine[an>>ANGLETOFINESHIFT]);
	th->momz = FixedMul( th->info->speed, slope);

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

void ClipGibVelocity (mobj_t *ent)
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

void ThrowGib (mobj_t *self, mobjtype_t gibtype, int damage)
{
	mobj_t *gib;
	vec3_t	vd;
	vec3_t	selfvel;
	vec3_t	gibvel;

	gib = P_SpawnMobj (self->x + crandom(pr_throwgib) * (self->radius >> (FRACBITS + 1)),
					   self->y + crandom(pr_throwgib) * (self->radius >> (FRACBITS + 1)),
					   self->z + (self->height >> 1) + crandom(pr_throwgib) * (self->height >> (FRACBITS + 1)),
					   gibtype,
					   0);

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
void ThrowGib (mobj_t *self, mobjtype_t gibtype, int damage)
{
}
#endif