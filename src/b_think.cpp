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
#include "p_inter.h"
#include "b_bot.h"
#include "g_game.h"
#include "m_random.h"
#include "r_main.h"
#include "stats.h"

//This function is called each tic for each bot,
//so this is what the bot does.
void DCajunMaster::Think (AActor *actor, ticcmd_t *cmd)
{
	memset (cmd, 0, sizeof(*cmd));

	if (actor->player->enemy && actor->player->enemy->health <= 0)
		actor->player->enemy = NULL;

	if (actor->health > 0) //Still alive
	{
		if (teamplay.value || !deathmatch.value)
			actor->player->mate = Choose_Mate (actor);

		angle_t oldyaw = actor->angle;
		int oldpitch = actor->pitch;

		Set_enemy (actor);
		ThinkForMove (actor, cmd);
		TurnToAng (actor);

		cmd->ucmd.yaw = (short)((actor->angle - oldyaw) >> 16) / ticdup;
		cmd->ucmd.pitch = (short)((oldpitch - actor->pitch) >> 16);
		if (cmd->ucmd.pitch == -37268)
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
		if (--actor->player->t_respawn == 1)
			cmd->ucmd.buttons |= BT_USE; //Press to respawn.
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
	else if (actor->pitch <= -60 || actor->pitch >= 60)
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
	else if (b->enemy && P_CheckSight (actor, b->enemy, false)) //Fight!
	{

		Pitch (actor, b->enemy);

		//Check if it's more important to get an item than fight.
		if (b->dest && (b->dest->flags&MF_SPECIAL)) //Must be an item, that is close enough.
		{
#define is b->dest->type==(mobjtype_t)
			if (((actor->health<b->skill.isp && (is Medikit || is Stimpack || is Soul || is Mega)) || (is Invul || is Invis || is Mega) || dist<(GETINCOMBAT/4) || (b->readyweapon==wp_pistol || b->readyweapon==wp_fist))
				&& (dist<GETINCOMBAT || (b->readyweapon==wp_pistol || b->readyweapon==wp_fist))
				&& Reachable(actor, b->dest))
#undef is
			{
				goto roam; //Pick it up, no matter the situation. All bonuses are nice close up.
			}
		}

		b->dest = NULL; //To let bot turn right

		if (b->readyweapon != wp_pistol && b->readyweapon != wp_fist)
			actor->flags &= ~MF_DROPOFF; //Don't jump off any ledges when fighting.

		if (!(b->enemy->flags & MF_COUNTKILL))
			b->t_fight = AFTERTICS;

		if ((P_AproxDistance(actor->x-b->oldx, actor->y-b->oldy)<50000
			|| ((P_Random(pr_botmove)%30)==10))
			&& b->t_strafe<=0)
		{
			stuck = true;
			b->t_strafe = 5;
			b->sleft = !b->sleft;
		}

		b->angle = R_PointToAngle2(actor->x, actor->y, b->enemy->x, b->enemy->y);

		if (P_AproxDistance(actor->x-b->enemy->x, actor->y-b->enemy->y) > bglobal.combatdst[b->readyweapon])
		{
			// If a monster, use lower speed (just for cooler apperance while strafing down doomed monster)
			cmd->ucmd.forwardmove = (b->enemy->flags & MF_COUNTKILL) ? FORWARDWALK : FORWARDRUN;
		}
		else if (!stuck) //Too close, so move away.
		{
			// If a monster, use lower speed (just for cooler apperance while strafing down doomed monster)
			cmd->ucmd.forwardmove = (b->enemy->flags & MF_COUNTKILL) ? -FORWARDWALK : -FORWARDRUN;
		}

		//Strafing.
		if (b->enemy->flags & MF_COUNTKILL) //It's just a monster so take it down cool.
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

		if (!Reachable(actor, b->mate))
			goto roam;

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
			b->dest = NULL; //Roaming after something dead.

		if (!b->dest)
		{
			if (b->t_fight && b->enemy) //Enemy has jumped around corner/bot has. So what to do.
			{
				if (b->enemy->player)
				{
					if ((b->enemy->player->readyweapon==wp_missile || (P_Random(pr_botmove)%100)>b->skill.isp) && !(b->readyweapon==wp_pistol || b->readyweapon==wp_fist))
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
				r = P_Random(pr_botmove);
				if ((r%100)<50 && bglobal.thingnum)
					b->dest = bglobal.things[(r%(bglobal.thingnum-1))];
				else if (b->mate && (P_CheckSight(actor, b->mate) || (r%100)<70))
					b->dest = b->mate;
				else if ((playeringame[(r%bglobal.playernumber)]) && players[(r%bglobal.playernumber)].mo->health >= 0)
					b->dest = players[(r%bglobal.playernumber)].mo; 
			}

			if(b->dest)
				b->t_roam = MAXROAM;
		}
		if (b->dest) //Bot has a target so roam after it.
			Roam (actor, cmd);

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

#define typeis item->type==(mobjtype_t)
    if (!item //Under respawn and away. (handled in P_Mobj.c)
		|| item == b->prev)
	{
		return;
	}
	//if(pos && !bglobal.thingvis[pos->id][item->id]) continue;
	if ((typeis Stimpack || typeis Medikit) && actor->health>=MAXHEALTH)
		return;
	else if((typeis Mega || typeis Soul || typeis Potion) && actor->health>=(MAXHEALTH*2))
		return;
	else if(((typeis Ssg || typeis Shotgun || typeis Shell || typeis ShellBox) && b->ammo[am_shell]==b->maxammo[am_shell]) || (deathmatch.value!=2 && ((typeis Shotgun && b->weaponowned[wp_shotgun]) || (typeis Ssg && b->weaponowned[wp_supershotgun]))))
		return;
	else if(((typeis Chain ||  typeis Clip|| typeis AmmoBox) && b->ammo[am_clip]==b->maxammo[am_clip]) || (deathmatch.value!=2 && (typeis Chain && b->weaponowned[wp_chaingun])))
		return;
	else if(((typeis Plasma || typeis Bfg || typeis Cell || typeis CellPack) && b->ammo[am_cell]==b->maxammo[am_cell]) || (deathmatch.value!=2 && ((typeis Bfg && b->weaponowned[wp_bfg]) || (typeis Plasma && b->weaponowned[wp_plasma]))))
		return;
	else if(((typeis Rl || typeis Rocket || typeis RoxBox) && b->ammo[am_misl]==b->maxammo[am_misl]) || (deathmatch.value!=2 && (typeis Rl && b->weaponowned[wp_missile])))
		return;
	else if(typeis Saw && b->weaponowned[wp_chainsaw])
		return;
	if (!Reachable(actor, item))
		return;

	if(!b->dest || !(b->dest && b->dest->flags&MF_SPECIAL) || (b->dest && !Reachable(actor, b->dest)))
	{
		b->prev = b->dest;
		b->dest = item;
		b->t_roam = MAXROAM;
	}
}

void DCajunMaster::Set_enemy (AActor *actor)
{
	AActor *oldenemy;

	if (actor->player->enemy
		&& actor->player->enemy->health > 0
		&& P_CheckSight (actor, actor->player->enemy))
	{
		oldenemy = actor->player->enemy;
	}
	else
	{
		oldenemy = NULL;
	}

	actor->player->allround = !!actor->player->enemy;
	actor->player->enemy = NULL;
	actor->player->enemy = Find_enemy(actor);
	if (!actor->player->enemy)
		actor->player->enemy = oldenemy; //Try go for last (it will be NULL if there wasn't anyone)
	//Verify that that enemy is really something alive that bot can kill.
	if (!actor->player->enemy || actor->player->enemy->health < 0 || !(actor->player->enemy->flags&MF_SHOOTABLE))
		actor->player->enemy = NULL;
}
