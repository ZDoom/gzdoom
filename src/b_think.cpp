/********************************
* B_Think.c                     *
* Description:                  *
* Functions for the different   *
* states that the bot           *
* uses. These functions are     *
* the main AI                   *
*                               *
*********************************/

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "b_bot.h"
#include "g_game.h"
#include "m_random.h"
#include "stats.h"
#include "a_pickups.h"
#include "statnums.h"
#include "d_net.h"
#include "d_event.h"

static FRandom pr_botmove ("BotMove");

//This function is called each tic for each bot,
//so this is what the bot does.
void FCajunMaster::Think (AActor *actor, ticcmd_t *cmd)
{
	memset (cmd, 0, sizeof(*cmd));

	if (actor->player->Bot->enemy && actor->player->Bot->enemy->health <= 0)
		actor->player->Bot->enemy = NULL;

	if (actor->health > 0) //Still alive
	{
		if (teamplay || !deathmatch)
			actor->player->Bot->mate = Choose_Mate (actor);

		angle_t oldyaw = actor->angle;
		int oldpitch = actor->pitch;

		Set_enemy (actor);
		ThinkForMove (actor, cmd);
		TurnToAng (actor);

		cmd->ucmd.yaw = (short)((actor->angle - oldyaw) >> 16) / ticdup;
		cmd->ucmd.pitch = (short)((oldpitch - actor->pitch) >> 16);
		if (cmd->ucmd.pitch == -32768)
			cmd->ucmd.pitch = -32767;
		cmd->ucmd.pitch /= ticdup;
		actor->angle = oldyaw + (cmd->ucmd.yaw << 16) * ticdup;
		actor->pitch = oldpitch - (cmd->ucmd.pitch << 16) * ticdup;
	}

	if (actor->player->Bot->t_active)	actor->player->Bot->t_active--;
	if (actor->player->Bot->t_strafe)	actor->player->Bot->t_strafe--;
	if (actor->player->Bot->t_react)	actor->player->Bot->t_react--;
	if (actor->player->Bot->t_fight)	actor->player->Bot->t_fight--;
	if (actor->player->Bot->t_rocket)	actor->player->Bot->t_rocket--;
	if (actor->player->Bot->t_roam)		actor->player->Bot->t_roam--;

	//Respawn ticker
	if (actor->player->Bot->t_respawn)
	{
		actor->player->Bot->t_respawn--;
	}
	else if (actor->health <= 0)
	{ // Time to respawn
		cmd->ucmd.buttons |= BT_USE;
	}
}

//how the bot moves.
//MAIN movement function.
void FCajunMaster::ThinkForMove (AActor *actor, ticcmd_t *cmd)
{
	player_t *b;
	fixed_t dist;
	bool stuck;
	int r;

	b = actor->player;
	if (b->Bot == NULL)
		return;

	stuck = false;
	dist = b->Bot->dest ? P_AproxDistance(actor->x-b->Bot->dest->x, actor->y-b->Bot->dest->y) : 0;

	if (b->Bot->missile &&
		((!b->Bot->missile->velx || !b->Bot->missile->vely) || !Check_LOS(actor, b->Bot->missile, SHOOTFOV*3/2)))
	{
		b->Bot->sleft = !b->Bot->sleft;
		b->Bot->missile = NULL; //Probably ended its travel.
	}

	if (actor->pitch > 0)
		actor->pitch -= 80;
	else if (actor->pitch <= -60)
		actor->pitch += 80;

	//HOW TO MOVE:
	if (b->Bot->missile && (P_AproxDistance(actor->x-b->Bot->missile->x, actor->y-b->Bot->missile->y)<AVOID_DIST)) //try avoid missile got from P_Mobj.c thinking part.
	{
		Pitch (actor, b->Bot->missile);
		actor->player->Bot->angle = R_PointToAngle2(actor->x, actor->y, b->Bot->missile->x, b->Bot->missile->y);
		cmd->ucmd.sidemove = b->Bot->sleft ? -SIDERUN : SIDERUN;
		cmd->ucmd.forwardmove = -FORWARDRUN; //Back IS best.

		if ((P_AproxDistance(actor->x-b->Bot->oldx, actor->y-b->Bot->oldy)<50000)
			&& b->Bot->t_strafe<=0)
		{
			b->Bot->t_strafe = 5;
			b->Bot->sleft = !b->Bot->sleft;
		}

		//If able to see enemy while avoiding missile, still fire at enemy.
		if (b->Bot->enemy && Check_LOS (actor, b->Bot->enemy, SHOOTFOV)) 
			Dofire (actor, cmd); //Order bot to fire current weapon
	}
	else if (b->Bot->enemy && P_CheckSight (actor, b->Bot->enemy, 0)) //Fight!
	{
		Pitch (actor, b->Bot->enemy);

		//Check if it's more important to get an item than fight.
		if (b->Bot->dest && (b->Bot->dest->flags&MF_SPECIAL)) //Must be an item, that is close enough.
		{
#define is(x) b->Bot->dest->IsKindOf (PClass::FindClass (#x))
			if (
				(
				 (actor->health < b->Bot->skill.isp &&
				  (is (Medikit) ||
				   is (Stimpack) ||
				   is (Soulsphere) ||
				   is (Megasphere) ||
				   is (CrystalVial)
				  )
				 ) || (
				  is (Invulnerability) ||
				  is (Invisibility) ||
				  is (Megasphere)
				 ) || 
				 dist < (GETINCOMBAT/4) ||
				 (b->ReadyWeapon == NULL || b->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON)
				)
				&& (dist < GETINCOMBAT || (b->ReadyWeapon == NULL || b->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON))
				&& Reachable (actor, b->Bot->dest))
#undef is
			{
				goto roam; //Pick it up, no matter the situation. All bonuses are nice close up.
			}
		}

		b->Bot->dest = NULL; //To let bot turn right

		if (b->ReadyWeapon != NULL && !(b->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON))
			actor->flags &= ~MF_DROPOFF; //Don't jump off any ledges when fighting.

		if (!(b->Bot->enemy->flags3 & MF3_ISMONSTER))
			b->Bot->t_fight = AFTERTICS;

		if (b->Bot->t_strafe <= 0 &&
			(P_AproxDistance(actor->x-b->Bot->oldx, actor->y-b->Bot->oldy)<50000
			|| ((pr_botmove()%30)==10))
			)
		{
			stuck = true;
			b->Bot->t_strafe = 5;
			b->Bot->sleft = !b->Bot->sleft;
		}

		b->Bot->angle = R_PointToAngle2(actor->x, actor->y, b->Bot->enemy->x, b->Bot->enemy->y);

		if (b->ReadyWeapon == NULL ||
			P_AproxDistance(actor->x-b->Bot->enemy->x, actor->y-b->Bot->enemy->y) >
			b->ReadyWeapon->MoveCombatDist)
		{
			// If a monster, use lower speed (just for cooler apperance while strafing down doomed monster)
			cmd->ucmd.forwardmove = (b->Bot->enemy->flags3 & MF3_ISMONSTER) ? FORWARDWALK : FORWARDRUN;
		}
		else if (!stuck) //Too close, so move away.
		{
			// If a monster, use lower speed (just for cooler apperance while strafing down doomed monster)
			cmd->ucmd.forwardmove = (b->Bot->enemy->flags3 & MF3_ISMONSTER) ? -FORWARDWALK : -FORWARDRUN;
		}

		//Strafing.
		if (b->Bot->enemy->flags3 & MF3_ISMONSTER) //It's just a monster so take it down cool.
		{
			cmd->ucmd.sidemove = b->Bot->sleft ? -SIDEWALK : SIDEWALK;
		}
		else
		{
			cmd->ucmd.sidemove = b->Bot->sleft ? -SIDERUN : SIDERUN;
		}
		Dofire (actor, cmd); //Order bot to fire current weapon
	}
	else if (b->Bot->mate && !b->Bot->enemy && (!b->Bot->dest || b->Bot->dest==b->Bot->mate)) //Follow mate move.
	{
		fixed_t matedist;

		Pitch (actor, b->Bot->mate);

		if (!Reachable (actor, b->Bot->mate))
		{
			if (b->Bot->mate == b->Bot->dest && pr_botmove.Random() < 32)
			{ // [RH] If the mate is the dest, pick a new dest sometimes
				b->Bot->dest = NULL;
			}
			goto roam;
		}

		actor->player->Bot->angle = R_PointToAngle2(actor->x, actor->y, b->Bot->mate->x, b->Bot->mate->y);

		matedist = P_AproxDistance(actor->x - b->Bot->mate->x, actor->y - b->Bot->mate->y);
		if (matedist > (FRIEND_DIST*2))
			cmd->ucmd.forwardmove = FORWARDRUN;
		else if (matedist > FRIEND_DIST)
			cmd->ucmd.forwardmove = FORWARDWALK; //Walk, when starting to get close.
		else if (matedist < FRIEND_DIST-(FRIEND_DIST/3)) //Got too close, so move away.
			cmd->ucmd.forwardmove = -FORWARDWALK;
	}
	else //Roam after something.
	{
		b->Bot->first_shot = true;

	/////
	roam:
	/////
		if (b->Bot->enemy && Check_LOS (actor, b->Bot->enemy, SHOOTFOV*3/2)) //If able to see enemy while avoiding missile , still fire at it.
			Dofire (actor, cmd); //Order bot to fire current weapon

		if (b->Bot->dest && !(b->Bot->dest->flags&MF_SPECIAL) && b->Bot->dest->health < 0)
		{ //Roaming after something dead.
			b->Bot->dest = NULL;
		}

		if (b->Bot->dest == NULL)
		{
			if (b->Bot->t_fight && b->Bot->enemy) //Enemy/bot has jumped around corner. So what to do?
			{
				if (b->Bot->enemy->player)
				{
					if (((b->Bot->enemy->player->ReadyWeapon != NULL && b->Bot->enemy->player->ReadyWeapon->WeaponFlags & WIF_BOT_EXPLOSIVE) ||
						(pr_botmove()%100)>b->Bot->skill.isp) && b->ReadyWeapon != NULL && !(b->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON))
						b->Bot->dest = b->Bot->enemy;//Dont let enemy kill the bot by supressive fire. So charge enemy.
					else //hide while b->t_fight, but keep view at enemy.
						b->Bot->angle = R_PointToAngle2(actor->x, actor->y, b->Bot->enemy->x, b->Bot->enemy->y);
				} //Just a monster, so kill it.
				else
					b->Bot->dest = b->Bot->enemy;

				//VerifFavoritWeapon(actor->player); //Dont know why here.., but it must be here, i know the reason, but not why at this spot, uh.
			}
			else //Choose a distant target. to get things going.
			{
				r = pr_botmove();
				if (r < 128)
				{
					TThinkerIterator<AInventory> it (STAT_INVENTORY, firstthing);
					AInventory *item = it.Next();

					if (item != NULL || (item = it.Next()) != NULL)
					{
						r &= 63;	// Only scan up to 64 entries at a time
						while (r)
						{
							--r;
							item = it.Next();
						}
						if (item == NULL)
						{
							item = it.Next();
						}
						firstthing = item;
						b->Bot->dest = item;
					}
				}
				else if (b->Bot->mate && (r < 179 || P_CheckSight(actor, b->Bot->mate)))
				{
					b->Bot->dest = b->Bot->mate;
				}
				else if ((playeringame[(r&(MAXPLAYERS-1))]) && players[(r&(MAXPLAYERS-1))].mo->health > 0)
				{
					b->Bot->dest = players[(r&(MAXPLAYERS-1))].mo; 
				}
			}

			if (b->Bot->dest)
			{
				b->Bot->t_roam = MAXROAM;
			}
		}
		if (b->Bot->dest)
		{ //Bot has a target so roam after it.
			Roam (actor, cmd);
		}

	} //End of movement main part.

	if (!b->Bot->t_roam && b->Bot->dest)
	{
		b->Bot->prev = b->Bot->dest;
		b->Bot->dest = NULL;
	}

	if (b->Bot->t_fight<(AFTERTICS/2))
		actor->flags |= MF_DROPOFF;

	b->Bot->oldx = actor->x;
	b->Bot->oldy = actor->y;
}

//BOT_WhatToGet
//
//Determines if the bot will roam after an item or not.
void FCajunMaster::WhatToGet (AActor *actor, AActor *item)
{
	player_t *b = actor->player;

	if (b == NULL)
	{
		return;
	}

#define typeis(x) item->IsKindOf (PClass::FindClass (#x))
	if ((item->renderflags & RF_INVISIBLE) //Under respawn and away.
		|| item == b->Bot->prev)
	{
		return;
	}
	int weapgiveammo = (alwaysapplydmflags || deathmatch) && !(dmflags & DF_WEAPONS_STAY);

	//if(pos && !bglobal.thingvis[pos->id][item->id]) continue;
//	if (item->IsKindOf (RUNTIME_CLASS(AArtifact)))
//		return;	// don't know how to use artifacts
	if (item->IsKindOf (RUNTIME_CLASS(AWeapon)))
	{
		// FIXME
		AWeapon *heldWeapon;

		heldWeapon = static_cast<AWeapon *> (b->mo->FindInventory (item->GetClass()));
		if (heldWeapon != NULL)
		{
			if (!weapgiveammo)
				return;
			if ((heldWeapon->Ammo1 == NULL || heldWeapon->Ammo1->Amount >= heldWeapon->Ammo1->MaxAmount) &&
				(heldWeapon->Ammo2 == NULL || heldWeapon->Ammo2->Amount >= heldWeapon->Ammo2->MaxAmount))
			{
				return;
			}
		}
	}
	else if (item->IsKindOf (RUNTIME_CLASS(AAmmo)))
	{
		AAmmo *ammo = static_cast<AAmmo *> (item);
		const PClass *parent = ammo->GetParentAmmo ();
		AInventory *holdingammo = b->mo->FindInventory (parent);

		if (holdingammo != NULL && holdingammo->Amount >= holdingammo->MaxAmount)
		{
			return;
		}
	}
	else if ((typeis (Megasphere) || typeis (Soulsphere) || typeis (HealthBonus)) && actor->health >= deh.MaxSoulsphere)
		return;
	else if (item->IsKindOf (RUNTIME_CLASS(AHealth)) && actor->health >= deh.MaxHealth /*MAXHEALTH*/)
		return;

	if ((b->Bot->dest == NULL ||
		!(b->Bot->dest->flags & MF_SPECIAL)/* ||
		!Reachable (actor, b->dest)*/)/* &&
		Reachable (actor, item)*/)	// Calling Reachable slows this down tremendously
	{
		b->Bot->prev = b->Bot->dest;
		b->Bot->dest = item;
		b->Bot->t_roam = MAXROAM;
	}
}

void FCajunMaster::Set_enemy (AActor *actor)
{
	AActor *oldenemy;
	AActor **enemy = &actor->player->Bot->enemy;

	if (*enemy
		&& (*enemy)->health > 0
		&& P_CheckSight (actor, *enemy))
	{
		oldenemy = *enemy;
	}
	else
	{
		oldenemy = NULL;
	}

	// [RH] Don't even bother looking for a different enemy if this is not deathmatch
	// and we already have an existing enemy.
	if (deathmatch || !*enemy)
	{
		actor->player->Bot->allround = !!*enemy;
		*enemy = NULL;
		*enemy = Find_enemy(actor);
		if (!*enemy)
			*enemy = oldenemy; //Try go for last (it will be NULL if there wasn't anyone)
	}
	//Verify that that enemy is really something alive that bot can kill.
	if (*enemy && (((*enemy)->health < 0 || !((*enemy)->flags&MF_SHOOTABLE)) || actor->IsFriend(*enemy)))
		*enemy = NULL;
}
