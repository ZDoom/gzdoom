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
void DBot::Think ()
{
	ticcmd_t *cmd = &netcmds[player - players][((gametic + 1)/ticdup)%BACKUPTICS];

	memset (cmd, 0, sizeof(*cmd));

	if (enemy && enemy->health <= 0)
		enemy = NULL;

	if (player->mo->health > 0) //Still alive
	{
		if (teamplay || !deathmatch)
			mate = Choose_Mate ();

		angle_t oldyaw = player->mo->angle;
		int oldpitch = player->mo->pitch;

		Set_enemy ();
		ThinkForMove (cmd);
		TurnToAng ();

		cmd->ucmd.yaw = (short)((player->mo->angle - oldyaw) >> 16) / ticdup;
		cmd->ucmd.pitch = (short)((oldpitch - player->mo->pitch) >> 16);
		if (cmd->ucmd.pitch == -32768)
			cmd->ucmd.pitch = -32767;
		cmd->ucmd.pitch /= ticdup;
		player->mo->angle = oldyaw + (cmd->ucmd.yaw << 16) * ticdup;
		player->mo->pitch = oldpitch - (cmd->ucmd.pitch << 16) * ticdup;
	}

	if (t_active)	t_active--;
	if (t_strafe)	t_strafe--;
	if (t_react)	t_react--;
	if (t_fight)	t_fight--;
	if (t_rocket)	t_rocket--;
	if (t_roam)		t_roam--;

	//Respawn ticker
	if (t_respawn)
	{
		t_respawn--;
	}
	else if (player->mo->health <= 0)
	{ // Time to respawn
		cmd->ucmd.buttons |= BT_USE;
	}
}

//how the bot moves.
//MAIN movement function.
void DBot::ThinkForMove (ticcmd_t *cmd)
{
	fixed_t dist;
	bool stuck;
	int r;

	stuck = false;
	dist = dest ? player->mo->AproxDistance(dest) : 0;

	if (missile &&
		((!missile->velx || !missile->vely) || !Check_LOS(missile, SHOOTFOV*3/2)))
	{
		sleft = !sleft;
		missile = NULL; //Probably ended its travel.
	}

	if (player->mo->pitch > 0)
		player->mo->pitch -= 80;
	else if (player->mo->pitch <= -60)
		player->mo->pitch += 80;

	//HOW TO MOVE:
	if (missile && (player->mo->AproxDistance(missile)<AVOID_DIST)) //try avoid missile got from P_Mobj.c thinking part.
	{
		Pitch (missile);
		angle = player->mo->AngleTo(missile);
		cmd->ucmd.sidemove = sleft ? -SIDERUN : SIDERUN;
		cmd->ucmd.forwardmove = -FORWARDRUN; //Back IS best.

		if ((player->mo->AproxDistance(oldx, oldy)<50000)
			&& t_strafe<=0)
		{
			t_strafe = 5;
			sleft = !sleft;
		}

		//If able to see enemy while avoiding missile, still fire at enemy.
		if (enemy && Check_LOS (enemy, SHOOTFOV)) 
			Dofire (cmd); //Order bot to fire current weapon
	}
	else if (enemy && P_CheckSight (player->mo, enemy, 0)) //Fight!
	{
		Pitch (enemy);

		//Check if it's more important to get an item than fight.
		if (dest && (dest->flags&MF_SPECIAL)) //Must be an item, that is close enough.
		{
#define is(x) dest->IsKindOf (PClass::FindClass (#x))
			if (
				(
				 (player->mo->health < skill.isp &&
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
				 (player->ReadyWeapon == NULL || player->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON)
				)
				&& (dist < GETINCOMBAT || (player->ReadyWeapon == NULL || player->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON))
				&& Reachable (dest))
#undef is
			{
				goto roam; //Pick it up, no matter the situation. All bonuses are nice close up.
			}
		}

		dest = NULL; //To let bot turn right

		if (player->ReadyWeapon != NULL && !(player->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON))
			player->mo->flags &= ~MF_DROPOFF; //Don't jump off any ledges when fighting.

		if (!(enemy->flags3 & MF3_ISMONSTER))
			t_fight = AFTERTICS;

		if (t_strafe <= 0 &&
			(player->mo->AproxDistance(oldx, oldy)<50000
			|| ((pr_botmove()%30)==10))
			)
		{
			stuck = true;
			t_strafe = 5;
			sleft = !sleft;
		}

		angle = player->mo->AngleTo(enemy);

		if (player->ReadyWeapon == NULL ||
			player->mo->AproxDistance(enemy) >
			player->ReadyWeapon->MoveCombatDist)
		{
			// If a monster, use lower speed (just for cooler apperance while strafing down doomed monster)
			cmd->ucmd.forwardmove = (enemy->flags3 & MF3_ISMONSTER) ? FORWARDWALK : FORWARDRUN;
		}
		else if (!stuck) //Too close, so move away.
		{
			// If a monster, use lower speed (just for cooler apperance while strafing down doomed monster)
			cmd->ucmd.forwardmove = (enemy->flags3 & MF3_ISMONSTER) ? -FORWARDWALK : -FORWARDRUN;
		}

		//Strafing.
		if (enemy->flags3 & MF3_ISMONSTER) //It's just a monster so take it down cool.
		{
			cmd->ucmd.sidemove = sleft ? -SIDEWALK : SIDEWALK;
		}
		else
		{
			cmd->ucmd.sidemove = sleft ? -SIDERUN : SIDERUN;
		}
		Dofire (cmd); //Order bot to fire current weapon
	}
	else if (mate && !enemy && (!dest || dest==mate)) //Follow mate move.
	{
		fixed_t matedist;

		Pitch (mate);

		if (!Reachable (mate))
		{
			if (mate == dest && pr_botmove.Random() < 32)
			{ // [RH] If the mate is the dest, pick a new dest sometimes
				dest = NULL;
			}
			goto roam;
		}

		angle = player->mo->AngleTo(mate);

		matedist = player->mo->AproxDistance(mate);
		if (matedist > (FRIEND_DIST*2))
			cmd->ucmd.forwardmove = FORWARDRUN;
		else if (matedist > FRIEND_DIST)
			cmd->ucmd.forwardmove = FORWARDWALK; //Walk, when starting to get close.
		else if (matedist < FRIEND_DIST-(FRIEND_DIST/3)) //Got too close, so move away.
			cmd->ucmd.forwardmove = -FORWARDWALK;
	}
	else //Roam after something.
	{
		first_shot = true;

	/////
	roam:
	/////
		if (enemy && Check_LOS (enemy, SHOOTFOV*3/2)) //If able to see enemy while avoiding missile , still fire at it.
			Dofire (cmd); //Order bot to fire current weapon

		if (dest && !(dest->flags&MF_SPECIAL) && dest->health < 0)
		{ //Roaming after something dead.
			dest = NULL;
		}

		if (dest == NULL)
		{
			if (t_fight && enemy) //Enemy/bot has jumped around corner. So what to do?
			{
				if (enemy->player)
				{
					if (((enemy->player->ReadyWeapon != NULL && enemy->player->ReadyWeapon->WeaponFlags & WIF_BOT_EXPLOSIVE) ||
						(pr_botmove()%100)>skill.isp) && player->ReadyWeapon != NULL && !(player->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON))
						dest = enemy;//Dont let enemy kill the bot by supressive fire. So charge enemy.
					else //hide while t_fight, but keep view at enemy.
						angle = player->mo->AngleTo(enemy);
				} //Just a monster, so kill it.
				else
					dest = enemy;

				//VerifFavoritWeapon(player); //Dont know why here.., but it must be here, i know the reason, but not why at this spot, uh.
			}
			else //Choose a distant target. to get things going.
			{
				r = pr_botmove();
				if (r < 128)
				{
					TThinkerIterator<AInventory> it (STAT_INVENTORY, bglobal.firstthing);
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
						bglobal.firstthing = item;
						dest = item;
					}
				}
				else if (mate && (r < 179 || P_CheckSight(player->mo, mate)))
				{
					dest = mate;
				}
				else if ((playeringame[(r&(MAXPLAYERS-1))]) && players[(r&(MAXPLAYERS-1))].mo->health > 0)
				{
					dest = players[(r&(MAXPLAYERS-1))].mo; 
				}
			}

			if (dest)
			{
				t_roam = MAXROAM;
			}
		}
		if (dest)
		{ //Bot has a target so roam after it.
			Roam (cmd);
		}

	} //End of movement main part.

	if (!t_roam && dest)
	{
		prev = dest;
		dest = NULL;
	}

	if (t_fight<(AFTERTICS/2))
		player->mo->flags |= MF_DROPOFF;

	oldx = player->mo->X();
	oldy = player->mo->Y();
}

//BOT_WhatToGet
//
//Determines if the bot will roam after an item or not.
void DBot::WhatToGet (AActor *item)
{
#define typeis(x) item->IsKindOf (PClass::FindClass (#x))
	if ((item->renderflags & RF_INVISIBLE) //Under respawn and away.
		|| item == prev)
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

		heldWeapon = static_cast<AWeapon *> (player->mo->FindInventory (item->GetClass()));
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
		AInventory *holdingammo = player->mo->FindInventory (parent);

		if (holdingammo != NULL && holdingammo->Amount >= holdingammo->MaxAmount)
		{
			return;
		}
	}
	else if ((typeis (Megasphere) || typeis (Soulsphere) || typeis (HealthBonus)) && player->mo->health >= deh.MaxSoulsphere)
		return;
	else if (item->IsKindOf (RUNTIME_CLASS(AHealth)) && player->mo->health >= deh.MaxHealth /*MAXHEALTH*/)
		return;

	if ((dest == NULL ||
		!(dest->flags & MF_SPECIAL)/* ||
		!Reachable (dest)*/)/* &&
		Reachable (item)*/)	// Calling Reachable slows this down tremendously
	{
		prev = dest;
		dest = item;
		t_roam = MAXROAM;
	}
}

void DBot::Set_enemy ()
{
	AActor *oldenemy;

	if (enemy
		&& enemy->health > 0
		&& P_CheckSight (player->mo, enemy))
	{
		oldenemy = enemy;
	}
	else
	{
		oldenemy = NULL;
	}

	// [RH] Don't even bother looking for a different enemy if this is not deathmatch
	// and we already have an existing enemy.
	if (deathmatch || !enemy)
	{
		allround = !!enemy;
		enemy = Find_enemy();
		if (!enemy)
			enemy = oldenemy; //Try go for last (it will be NULL if there wasn't anyone)
	}
	//Verify that that enemy is really something alive that bot can kill.
	if (enemy && ((enemy->health < 0 || !(enemy->flags&MF_SHOOTABLE)) || player->mo->IsFriend(enemy)))
		enemy = NULL;
}
