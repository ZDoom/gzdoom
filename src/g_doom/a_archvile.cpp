#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_doomglobal.h"
#include "gstrings.h"
#include "a_action.h"

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
void A_Fire (AActor *self);

static AActor *corpsehit;
static AActor *vileobj;
static fixed_t viletryx;
static fixed_t viletryy;
static FState *raisestate;

bool PIT_VileCheck (AActor *thing)
{
	int maxdist;
	bool check;
		
	if (!(thing->flags & MF_CORPSE) )
		return true;	// not a monster
	
	if (thing->tics != -1)
		return true;	// not lying still yet
	
	raisestate = thing->FindState(NAME_Raise);
	if (raisestate == NULL)
		return true;	// monster doesn't have a raise state
	
  	// This may be a potential problem if this is used by something other
	// than an Arch Vile.	
	//maxdist = thing->GetDefault()->radius + GetDefault<AArchvile>()->radius;
	
	// use the current actor's radius instead of the Arch Vile's default.
	maxdist = thing->GetDefault()->radius + vileobj->radius; 
		
	if ( abs(thing->x - viletryx) > maxdist
		 || abs(thing->y - viletryy) > maxdist )
		return true;			// not actually touching
				
	corpsehit = thing;
	corpsehit->momx = corpsehit->momy = 0;
	// [RH] Check against real height and radius

	fixed_t oldheight = corpsehit->height;
	fixed_t oldradius = corpsehit->radius;
	int oldflags = corpsehit->flags;

	corpsehit->flags |= MF_SOLID;
	corpsehit->height = corpsehit->GetDefault()->height;
	check = P_CheckPosition (corpsehit, corpsehit->x, corpsehit->y);
	corpsehit->flags = oldflags;
	corpsehit->radius = oldradius;
	corpsehit->height = oldheight;

	return !check;
}



//
// A_VileChase
// Check for ressurecting a body
//
void A_VileChase (AActor *self)
{
	static TArray<AActor *> vilebt;
	int xl, xh, yl, yh;
	int bx, by;

	const AActor *info;
	AActor *temp;
		
	if (self->movedir != DI_NODIR)
	{
		const fixed_t absSpeed = abs (self->Speed);

		// check for corpses to raise
		viletryx = self->x + FixedMul (absSpeed, xspeed[self->movedir]);
		viletryy = self->y + FixedMul (absSpeed, yspeed[self->movedir]);

		xl = (viletryx - bmaporgx - MAXRADIUS*2)>>MAPBLOCKSHIFT;
		xh = (viletryx - bmaporgx + MAXRADIUS*2)>>MAPBLOCKSHIFT;
		yl = (viletryy - bmaporgy - MAXRADIUS*2)>>MAPBLOCKSHIFT;
		yh = (viletryy - bmaporgy + MAXRADIUS*2)>>MAPBLOCKSHIFT;
		
		vileobj = self;
		validcount++;
		vilebt.Clear();

		for (bx = xl; bx <= xh; bx++)
		{
			for (by = yl; by <= yh; by++)
			{
				// Call PIT_VileCheck to check
				// whether object is a corpse
				// that can be raised.
				if (!P_BlockThingsIterator (bx, by, PIT_VileCheck, vilebt))
				{
					// got one!
					temp = self->target;
					self->target = corpsehit;
					A_FaceTarget (self);
					if (self->flags & MF_FRIENDLY)
					{
						// If this is a friendly Arch-Vile (which is turning the resurrected monster into its friend)
						// and the Arch-Vile is currently targetting the resurrected monster the target must be cleared.
						if (self->lastenemy == temp) self->lastenemy = NULL;
						if (temp == self->target) temp = NULL;
						
					}
					self->target = temp;
										
					// Make the state the monster enters customizable.
					FState * state = self->FindState(NAME_Heal);
					if (state != NULL)
					{
						self->SetState (state);
					}
					else
					{
						// For Dehacked compatibility this has to use the Arch Vile's
						// heal state as a default if the actor doesn't define one itself.
						const PClass *archvile = PClass::FindClass("Archvile");
						if (archvile != NULL)
						{
							self->SetState (archvile->ActorInfo->FindState(NAME_Heal));
						}
					}
					S_Sound (corpsehit, CHAN_BODY, "vile/raise", 1, ATTN_IDLE);
					info = corpsehit->GetDefault ();
					
					corpsehit->SetState (raisestate);
					corpsehit->height = info->height;	// [RH] Use real mobj height
					corpsehit->radius = info->radius;	// [RH] Use real radius
					/*
					// Make raised corpses look ghostly
					if (corpsehit->alpha > TRANSLUC50)
						corpsehit->alpha /= 2;
					*/
					corpsehit->flags = info->flags;
					corpsehit->flags2 = info->flags2;
					corpsehit->flags3 = info->flags3;
					corpsehit->flags4 = info->flags4;
					corpsehit->health = info->health;
					corpsehit->target = NULL;
					corpsehit->lastenemy = NULL;

					// [RH] If it's a monster, it gets to count as another kill
					if (corpsehit->CountsAsKill())
					{
						level.total_monsters++;
					}

					// You are the Archvile's minion now, so hate what it hates
					corpsehit->CopyFriendliness (self, false);


					return;
				}
			}
		}
	}

	// Return to normal attack.
	A_Chase (self);
}


//
// A_VileStart
//
void A_VileStart (AActor *self)
{
	S_Sound (self, CHAN_VOICE, "vile/start", 1, ATTN_NORM);
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
void A_StartFire (AActor *self)
{
	S_Sound (self, CHAN_BODY, "vile/firestrt", 1, ATTN_NORM);
	A_Fire (self);
}

void A_FireCrackle (AActor *self)
{
	S_Sound (self, CHAN_BODY, "vile/firecrkl", 1, ATTN_NORM);
	A_Fire (self);
}

void A_Fire (AActor *self)
{
	AActor *dest;
	angle_t an;
				
	dest = self->tracer;
	if (!dest)
		return;
				
	// don't move it if the vile lost sight
	if (!P_CheckSight (self->target, dest, 0) )
		return;

	an = dest->angle >> ANGLETOFINESHIFT;

	self->SetOrigin (dest->x + FixedMul (24*FRACUNIT, finecosine[an]),
					 dest->y + FixedMul (24*FRACUNIT, finesine[an]),
					 dest->z);
}



//
// A_VileTarget
// Spawn the hellfire
//
void A_VileTarget (AActor *actor)
{
	AActor *fog;
		
	if (!actor->target)
		return;

	A_FaceTarget (actor);

	fog = Spawn ("ArchvileFire", actor->target->x, actor->target->x,
		actor->target->z, ALLOW_REPLACE);
	
	actor->tracer = fog;
	fog->target = actor;
	fog->tracer = actor->target;
	A_Fire (fog);
}




//
// A_VileAttack
//
void A_VileAttack (AActor *actor)
{		
	AActor *fire;
	int an;
		
	if (!actor->target)
		return;
	
	A_FaceTarget (actor);

	if (!P_CheckSight (actor, actor->target, 0) )
		return;

	S_Sound (actor, CHAN_WEAPON, "vile/stop", 1, ATTN_NORM);
	P_DamageMobj (actor->target, actor, actor, 20, NAME_None);
	P_TraceBleed (20, actor->target);
	actor->target->momz = 1000 * FRACUNIT / actor->target->Mass;
		
	an = actor->angle >> ANGLETOFINESHIFT;

	fire = actor->tracer;

	if (!fire)
		return;
				
	// move the fire between the vile and the player
	fire->x = actor->target->x - FixedMul (24*FRACUNIT, finecosine[an]);
	fire->y = actor->target->y - FixedMul (24*FRACUNIT, finesine[an]);	
	P_RadiusAttack (fire, actor, 70, 70, NAME_Fire, false);
}
