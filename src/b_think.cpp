/*
**
**
**---------------------------------------------------------------------------
** Copyright 1999 Martin Colberg
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
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
#include "d_net.h"
#include "d_event.h"
#include "d_player.h"
#include "actorinlines.h"

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

		AActor *actor = player->mo;
		DAngle oldyaw = actor->Angles.Yaw;
		DAngle oldpitch = actor->Angles.Pitch;

		Set_enemy ();
		ThinkForMove (cmd);
		TurnToAng ();

		cmd->ucmd.yaw = (short)((actor->Angles.Yaw - oldyaw).Degrees * (65536 / 360.f)) / ticdup;
		cmd->ucmd.pitch = (short)((oldpitch - actor->Angles.Pitch).Degrees * (65536 / 360.f));
		if (cmd->ucmd.pitch == -32768)
			cmd->ucmd.pitch = -32767;
		cmd->ucmd.pitch /= ticdup;
		actor->Angles.Yaw = oldyaw + DAngle(cmd->ucmd.yaw * ticdup * (360 / 65536.f));
		actor->Angles.Pitch = oldpitch - DAngle(cmd->ucmd.pitch * ticdup * (360 / 65536.f));
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

#define THINKDISTSQ (50000.*50000./(65536.*65536.))
//how the bot moves.
//MAIN movement function.
void DBot::ThinkForMove (ticcmd_t *cmd)
{
	double dist;
	bool stuck;
	int r;

	stuck = false;
	dist = dest ? player->mo->Distance2D(dest) : 0;

	if (missile &&
		(!missile->Vel.X || !missile->Vel.Y || !Check_LOS(missile, SHOOTFOV*3/2)))
	{
		sleft = !sleft;
		missile = NULL; //Probably ended its travel.
	}

#if 0	// this has always been broken and without any reference it cannot be fixed.
	if (player->mo->Angles.Pitch > 0)
		player->mo->Angles.Pitch -= 80;
	else if (player->mo->Angles.Pitch <= -60)
		player->mo->Angles.Pitch += 80;
#endif

	//HOW TO MOVE:
	if (missile && (player->mo->Distance2D(missile)<AVOID_DIST)) //try avoid missile got from P_Mobj.c thinking part.
	{
		Pitch (missile);
		Angle = player->mo->AngleTo(missile);
		cmd->ucmd.sidemove = sleft ? -SIDERUN : SIDERUN;
		cmd->ucmd.forwardmove = -FORWARDRUN; //Back IS best.

		if ((player->mo->Pos() - old).LengthSquared() < THINKDISTSQ
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
			((player->mo->Pos() - old).LengthSquared() < THINKDISTSQ
			|| ((pr_botmove()%30)==10))
			)
		{
			stuck = true;
			t_strafe = 5;
			sleft = !sleft;
		}

		Angle = player->mo->AngleTo(enemy);

		if (player->ReadyWeapon == NULL ||
			player->mo->Distance2D(enemy) >
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
		double matedist;

		Pitch (mate);

		if (!Reachable (mate))
		{
			if (mate == dest && pr_botmove.Random() < 32)
			{ // [RH] If the mate is the dest, pick a new dest sometimes
				dest = NULL;
			}
			goto roam;
		}

		Angle = player->mo->AngleTo(mate);

		matedist = player->mo->Distance2D(mate);
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
						Angle = player->mo->AngleTo(enemy);
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
					TThinkerIterator<AInventory> it (MAX_STATNUM+1, bglobal.firstthing);
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

	old = player->mo->Pos();
}

int P_GetRealMaxHealth(APlayerPawn *actor, int max);

//BOT_WhatToGet
//
//Determines if the bot will roam after an item or not.
void DBot::WhatToGet (AActor *item)
{
	if ((item->renderflags & RF_INVISIBLE) //Under respawn and away.
		|| item == prev)
	{
		return;
	}
	int weapgiveammo = (alwaysapplydmflags || deathmatch) && !(dmflags & DF_WEAPONS_STAY);

	//if(pos && !bglobal.thingvis[pos->id][item->id]) continue;
//	if (item->IsKindOf (RUNTIME_CLASS(AArtifact)))
//		return;	// don't know how to use artifacts
	if (item->IsKindOf(NAME_Weapon))
	{
		// FIXME
		AWeapon *heldWeapon;

		heldWeapon = dyn_cast<AWeapon>(player->mo->FindInventory(item->GetClass()));
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
	else if (item->IsKindOf (PClass::FindActor(NAME_Ammo)))
	{
		auto ac = PClass::FindActor(NAME_Ammo);
		auto parent = item->GetClass();
		while (parent->ParentClass != ac) parent = static_cast<PClassActor*>(parent->ParentClass);
		AInventory *holdingammo = player->mo->FindInventory(parent);
		if (holdingammo != NULL && holdingammo->Amount >= holdingammo->MaxAmount)
		{
			return;
		}
	}
	else if (item->GetClass()->TypeName == NAME_Megasphere || item->IsKindOf(NAME_Health))
	{
		// do the test with the health item that's actually given.
		AActor* const testItem = NAME_Megasphere == item->GetClass()->TypeName
			? GetDefaultByName("MegasphereHealth")
			: item;
		if (nullptr != testItem)
		{
			const int maxhealth = P_GetRealMaxHealth(player->mo, testItem->IntVar(NAME_MaxAmount));
			if (player->mo->health >= maxhealth)
				return;
		}
	}

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
