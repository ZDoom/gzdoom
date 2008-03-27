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
#include "r_main.h"
#include "stats.h"
#include "a_pickups.h"
#include "statnums.h"

static FRandom pr_botmove ("BotMove");

//This function is called each tic for each bot,
//so this is what the bot does.
void DCajunMaster::Think (AActor *actor, ticcmd_t *cmd)
{
	memset (cmd, 0, sizeof(*cmd));

	if (actor->player->enemy && actor->player->enemy->health <= 0)
		actor->player->enemy = NULL;

	if (actor->health > 0) //Still alive
	{
		if (teamplay || !deathmatch)
			actor->player->mate = Choose_Mate (actor);

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

	if (actor->player->t_active)	actor->player->t_active--;
	if (actor->player->t_strafe)	actor->player->t_strafe--;
	if (actor->player->t_react)		actor->player->t_react--;
	if (actor->player->t_fight)		actor->player->t_fight--;
	if (actor->player->t_rocket)	actor->player->t_rocket--;
	if (actor->player->t_roam)		actor->player->t_roam--;

	//Respawn ticker
	if (actor->player->t_respawn)
	{
		actor->player->t_respawn--;
	}
	else if (actor->health <= 0)
	{ // Time to respawn
		cmd->ucmd.buttons |= BT_USE;
	}
}

//how the bot moves.
//MAIN movement function.
void DCajunMaster::ThinkForMove (AActor *actor, ticcmd_t *cmd)
{
	player_t *b;
	fixed_t dist;
	bool stuck;
	int r;

	b = actor->player;
	if (!b->isbot)
		return;

	stuck = false;
	dist = b->dest ? P_AproxDistance(actor->x-b->dest->x, actor->y-b->dest->y) : 0;

	if (b->missile &&
		((!b->missile->momx || !b->missile->momy) || !Check_LOS(actor, b->missile, SHOOTFOV*3/2)))
	{
		b->sleft = !b->sleft;
		b->missile = NULL; //Probably ended its travel.
	}

	if (actor->pitch > 0)
		actor->pitch -= 80;
	else if (actor->pitch <= -60)
		actor->pitch += 80;

	//HOW TO MOVE:
	if (b->missile && (P_AproxDistance(actor->x-b->missile->x, actor->y-b->missile->y)<AVOID_DIST)) //try avoid missile got from P_Mobj.c thinking part.
	{
		Pitch (actor, b->missile);
		actor->player->angle = R_PointToAngle2(actor->x, actor->y, b->missile->x, b->missile->y);
		cmd->ucmd.sidemove = b->sleft ? -SIDERUN : SIDERUN;
		cmd->ucmd.forwardmove = -FORWARDRUN; //Back IS best.

		if ((P_AproxDistance(actor->x-b->oldx, actor->y-b->oldy)<50000)
			&& b->t_strafe<=0)
		{
			b->t_strafe = 5;
			b->sleft = !b->sleft;
		}

		//If able to see enemy while avoiding missile, still fire at enemy.
		if (b->enemy && Check_LOS (actor, b->enemy, SHOOTFOV)) 
			Dofire (actor, cmd); //Order bot to fire current weapon
	}
	else if (b->enemy && P_CheckSight (actor, b->enemy, 0)) //Fight!
	{
		Pitch (actor, b->enemy);

		//Check if it's more important to get an item than fight.
		if (b->dest && (b->dest->flags&MF_SPECIAL)) //Must be an item, that is close enough.
		{
#define is(x) b->dest->IsKindOf (PClass::FindClass (#x))
			if (
				(
				 (actor->health < b->skill.isp &&
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
				&& Reachable (actor, b->dest))
#undef is
			{
				goto roam; //Pick it up, no matter the situation. All bonuses are nice close up.
			}
		}

		b->dest = NULL; //To let bot turn right

		if (b->ReadyWeapon != NULL && !(b->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON))
			actor->flags &= ~MF_DROPOFF; //Don't jump off any ledges when fighting.

		if (!(b->enemy->flags3 & MF3_ISMONSTER))
			b->t_fight = AFTERTICS;

		if (b->t_strafe <= 0 &&
			(P_AproxDistance(actor->x-b->oldx, actor->y-b->oldy)<50000
			|| ((pr_botmove()%30)==10))
			)
		{
			stuck = true;
			b->t_strafe = 5;
			b->sleft = !b->sleft;
		}

		b->angle = R_PointToAngle2(actor->x, actor->y, b->enemy->x, b->enemy->y);

		if (b->ReadyWeapon == NULL ||
			P_AproxDistance(actor->x-b->enemy->x, actor->y-b->enemy->y) >
			b->ReadyWeapon->MoveCombatDist)
		{
			// If a monster, use lower speed (just for cooler apperance while strafing down doomed monster)
			cmd->ucmd.forwardmove = (b->enemy->flags3 & MF3_ISMONSTER) ? FORWARDWALK : FORWARDRUN;
		}
		else if (!stuck) //Too close, so move away.
		{
			// If a monster, use lower speed (just for cooler apperance while strafing down doomed monster)
			cmd->ucmd.forwardmove = (b->enemy->flags3 & MF3_ISMONSTER) ? -FORWARDWALK : -FORWARDRUN;
		}

		//Strafing.
		if (b->enemy->flags3 & MF3_ISMONSTER) //It's just a monster so take it down cool.
		{
			cmd->ucmd.sidemove = b->sleft ? -SIDEWALK : SIDEWALK;
		}
		else
		{
			cmd->ucmd.sidemove = b->sleft ? -SIDERUN : SIDERUN;
		}
		Dofire (actor, cmd); //Order bot to fire current weapon
	}
	else if (b->mate && !b->enemy && (!b->dest || b->dest==b->mate)) //Follow mate move.
	{
		fixed_t matedist;

		Pitch (actor, b->mate);

		if (!Reachable (actor, b->mate))
		{
			if (b->mate == b->dest && pr_botmove.Random() < 32)
			{ // [RH] If the mate is the dest, pick a new dest sometimes
				b->dest = NULL;
			}
			goto roam;
		}

		actor->player->angle = R_PointToAngle2(actor->x, actor->y, b->mate->x, b->mate->y);

		matedist = P_AproxDistance(actor->x - b->mate->x, actor->y - b->mate->y);
		if (matedist > (FRIEND_DIST*2))
			cmd->ucmd.forwardmove = FORWARDRUN;
		else if (matedist > FRIEND_DIST)
			cmd->ucmd.forwardmove = FORWARDWALK; //Walk, when starting to get close.
		else if (matedist < FRIEND_DIST-(FRIEND_DIST/3)) //Got too close, so move away.
			cmd->ucmd.forwardmove = -FORWARDWALK;
	}
	else //Roam after something.
	{
		b->first_shot = true;

	/////
	roam:
	/////
		if (b->enemy && Check_LOS (actor, b->enemy, SHOOTFOV*3/2)) //If able to see enemy while avoiding missile , still fire at it.
			Dofire (actor, cmd); //Order bot to fire current weapon

		if (b->dest && !(b->dest->flags&MF_SPECIAL) && b->dest->health < 0)
		{ //Roaming after something dead.
			b->dest = NULL;
		}

		if (b->dest == NULL)
		{
			if (b->t_fight && b->enemy) //Enemy/bot has jumped around corner. So what to do?
			{
				if (b->enemy->player)
				{
					if (((b->enemy->player->ReadyWeapon != NULL && b->enemy->player->ReadyWeapon->WeaponFlags & WIF_BOT_EXPLOSIVE) ||
						(pr_botmove()%100)>b->skill.isp) && b->ReadyWeapon != NULL && !(b->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON))
						b->dest = b->enemy;//Dont let enemy kill the bot by supressive fire. So charge enemy.
					else //hide while b->t_fight, but keep view at enemy.
						b->angle = R_PointToAngle2(actor->x, actor->y, b->enemy->x, b->enemy->y);
				} //Just a monster, so kill it.
				else
					b->dest = b->enemy;

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
						b->dest = item;
					}
				}
				else if (b->mate && (r < 179 || P_CheckSight(actor, b->mate)))
				{
					b->dest = b->mate;
				}
				else if ((playeringame[(r&(MAXPLAYERS-1))]) && players[(r&(MAXPLAYERS-1))].mo->health > 0)
				{
					b->dest = players[(r&(MAXPLAYERS-1))].mo; 
				}
			}

			if (b->dest)
			{
				b->t_roam = MAXROAM;
			}
		}
		if (b->dest)
		{ //Bot has a target so roam after it.
			Roam (actor, cmd);
		}

	} //End of movement main part.

	if (!b->t_roam && b->dest)
	{
		b->prev = b->dest;
		b->dest = NULL;
	}

	if (b->t_fight<(AFTERTICS/2))
		actor->flags |= MF_DROPOFF;

	b->oldx = actor->x;
	b->oldy = actor->y;
}

//BOT_WhatToGet
//
//Determines if the bot will roam after an item or not.
void DCajunMaster::WhatToGet (AActor *actor, AActor *item)
{
	player_t *b = actor->player;

	if (b == NULL)
	{
		return;
	}

#define typeis(x) item->IsKindOf (PClass::FindClass (#x))
	if ((item->renderflags & RF_INVISIBLE) //Under respawn and away.
		|| item == b->prev)
	{
		return;
	}
	int weapgiveammo = (alwaysapplydmflags || deathmatch) && !(dmflags & DF_WEAPONS_STAY);

	//if(pos && !bglobal->thingvis[pos->id][item->id]) continue;
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

	if ((b->dest == NULL ||
		!(b->dest->flags & MF_SPECIAL)/* ||
		!Reachable (actor, b->dest)*/)/* &&
		Reachable (actor, item)*/)	// Calling Reachable slows this down tremendously
	{
		b->prev = b->dest;
		b->dest = item;
		b->t_roam = MAXROAM;
	}
}

void DCajunMaster::Set_enemy (AActor *actor)
{
	AActor *oldenemy;
	AActor **enemy = &actor->player->enemy;

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
		actor->player->allround = !!*enemy;
		*enemy = NULL;
		*enemy = Find_enemy(actor);
		if (!*enemy)
			*enemy = oldenemy; //Try go for last (it will be NULL if there wasn't anyone)
	}
	//Verify that that enemy is really something alive that bot can kill.
	if (*enemy && (((*enemy)->health < 0 || !((*enemy)->flags&MF_SHOOTABLE)) || actor->IsFriend(*enemy)))
		*enemy = NULL;
}
