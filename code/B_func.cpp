/*******************************
* B_spawn.c                    *
* Description:                 *
* various procedures that the  *
* bot need to work             *
*******************************/

#include <malloc.h>

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "p_inter.h"
#include "b_bot.h"
#include "g_game.h"
#include "m_random.h"
#include "r_sky.h"
#include "r_main.h"
#include "st_stuff.h"
#include "stats.h"
#include "i_system.h"
#include "s_sound.h"
#include "vectors.h"

//Used with Reachable().
static AActor *looker;
static AActor *rtarget;
static bool reachable;
static fixed_t last_z;
static sector_t *last_s;
static fixed_t estimated_dist;

static PTR_Reachable (intercept_t *in)
{
	fixed_t frac;
	line_t *line;
	AActor *thing;
	fixed_t dist;
	sector_t *s;

	frac = in->frac - FixedDiv (4*FRACUNIT, MAX_TRAVERSE_DIST);
	dist = FixedMul (frac, MAX_TRAVERSE_DIST);

	if (in->isaline)
	{
		line = in->d.line;

		if (!(line->flags & ML_TWOSIDED) || (line->flags & ML_BLOCKING))
		{
			return (reachable = false); //Cannot continue.
		}
		else
		{
			//Determine if going to use backsector/frontsector.
			if (line->backsector == last_s)
				s = line->frontsector;
			else s = line->backsector;

			if (s->floorheight <= (last_z+MAXMOVEHEIGHT)
				&& ((s->ceilingheight==s->floorheight && line->special) || (s->ceilingheight-s->floorheight)>=looker->info->height) //Does it fit?
				&& (!bglobal.IsDangerous(s))) //Any Nukage/lava?
			{
				last_z = s->floorheight;
				last_s = s;
				return true;
			}
			else
			{
				return (reachable = false);
			}
		}

		if (dist > estimated_dist)
		{
			reachable = true;
			return false; //Don't need to continue.
		}
	}

	thing = in->d.thing;
	if (thing == looker) //Can't reach self in this case.
		return true;
	if (thing == rtarget && (rtarget->subsector->sector->floorheight <= (last_z+MAXMOVEHEIGHT)))
	{
		reachable = true;
		return false;
	}

	reachable = false;
	return true;
}

//Checks TRUE reachability from
//one actor to another. First mobj (actor) is looker.
bool DCajunMaster::Reachable (AActor *actor, AActor *target)
{
	if (actor == target)
		return false;

	if ((target->subsector->sector->ceilingheight - target->subsector->sector->floorheight)
		< actor->info->height) //Where target is, looker can't be.
		return false;

	if (target->subsector->sector == actor->subsector->sector)
		return true;    

	looker = actor;
	rtarget = target;
	last_z = actor->subsector->sector->floorheight;
	last_s = actor->subsector->sector;
	reachable = true;
	estimated_dist = P_AproxDistance (actor->x - target->x, actor->y - target->y);
	P_PathTraverse (actor->x+actor->momx, actor->y+actor->momy, target->x, target->y, PT_ADDLINES|PT_ADDTHINGS, PTR_Reachable);
	return reachable;
}

//doesnt check LOS, checks visibility with a set view angle.
//B_Checksight checks LOS (straight line)
//----------------------------------------------------------------------
//Check if mo1 has free line to mo2
//and if mo2 is within mo1 viewangle (vangle) given with normal degrees.
//if these conditions are true, the function returns true.
//GOOD TO KNOW is that the players view angle
//in doom is 90 degrees infront.
bool DCajunMaster::Check_LOS (AActor *from, AActor *to, angle_t vangle)
{
	if (!P_CheckSight (from, to, false))
		return false; // out of sight
	if (vangle == ANGLE_MAX)
		return true;
	if (vangle == 0)
		return false; //Looker seems to be blind.

	return abs (R_PointToAngle2 (from->x, from->y, to->x, to->y) - from->angle) <= (int)vangle/2;
}

//Returns warrior's current weapon.
//if the bool is set to true, the warrior
//is a bot else it's a player.
//"Id" is the number of bot/player.
mobjtype_t DCajunMaster::Warrior_weapon_drop (AActor *actor)
{
	return weaponinfo[actor->player->readyweapon].droptype;
}

//-------------------------------------
//Bot_Dofire()
//-------------------------------------
//The bot will check if it's time to fire
//and do so if that is the case.
void DCajunMaster::Dofire (AActor *actor, ticcmd_t *cmd)
{
	bool no_fire; //used to prevent bot from pumping rockets into nearby walls.
	int aiming_penalty=0; //For shooting at shading target, if screen is red, MAKEME: When screen red.
	int aiming_value; //The final aiming value.
	fixed_t dist;
	angle_t an;
	int m;
	static bool inc[MAXPLAYERS];
	AActor *enemy = actor->player->enemy;

	if (!enemy || !(enemy->flags & MF_SHOOTABLE) || enemy->health <= 0)
		return;

	if (actor->player->damagecount > actor->player->skill.isp)
	{
		actor->player->first_shot = true;
		return;
	}

	//Reaction skill thing.
	if (actor->player->first_shot &&
		!(actor->player->readyweapon==wp_bfg || actor->player->readyweapon==wp_missile))
	{
		actor->player->t_react = (100-actor->player->skill.reaction+1)/((P_Random(pr_botdofire)%3)+3);
	}
	actor->player->first_shot = false;
	if (actor->player->t_react)
		return;

	//MAKEME: Decrease the rocket suicides even more.

	no_fire = true;
	//actor->player->angle = R_PointToAngle2(actor->x, actor->y, actor->player->enemy->x, actor->player->enemy->y);
	//Distance to enemy.
	dist = P_AproxDistance ((actor->x + actor->momx) - (enemy->x + enemy->momx),
		(actor->y + actor->momy) - (enemy->y + enemy->momy));

	//FIRE EACH TYPE OF WEAPON DIFFERENT: Here should all the ddf weapons go.
	switch (actor->player->readyweapon)
	{
	case wp_missile: //Special rules for RL
		an = FireRox (actor, enemy, cmd);
		if(an)
		{
			actor->player->angle = an;
			if (abs (actor->player->angle - actor->angle) < 12*ANGLE_1) //have to be somewhat precise. to avoid suicide.
			{
				actor->player->t_rocket = 9;
				no_fire = false;
			}
		}
		break;

	case wp_plasma: //Plasma (prediction aiming)
		//Here goes the prediction.
		dist = P_AproxDistance (actor->x - enemy->x, actor->y - enemy->y);
		m = (dist/FRACUNIT) / mobjinfo[MT_PLASMA].speed;
		SetBodyAt (enemy->x + FixedMul (enemy->momx, (m*2*FRACUNIT)),
				   enemy->y + FixedMul (enemy->momy, (m*2*FRACUNIT)), ONFLOORZ, 1);
		actor->player->angle = R_PointToAngle2 (actor->x, actor->y, body1->x, body1->y);
		if (Check_LOS (actor, enemy, SHOOTFOV))
			no_fire = false;
		break;

	case wp_chainsaw:
	case wp_fist:
		no_fire = (dist > (MELEERANGE*4)); //*4 is for atmosphere,  the chainsaws sounding and all..
		break;

	case wp_bfg:
		//MAKEME: This should be smarter.
		if ((P_Random(pr_botdofire)%200)<=actor->player->skill.reaction)
			if(Check_LOS(actor, actor->player->enemy, SHOOTFOV))
				no_fire = false;
		break;

	default: //Other weapons, mostly instant hit stuff.
		actor->player->angle = R_PointToAngle2 (actor->x, actor->y, enemy->x, enemy->y);
		aiming_penalty = 0;
		if (enemy->flags & MF_SHADOW)
			aiming_penalty += (P_Random (pr_botdofire)%25)+10;
		if (enemy->subsector->sector->lightlevel<WHATS_DARK && !actor->player->powers[pw_infrared])
			aiming_penalty += P_Random (pr_botdofire)%40;//Dark
		if (actor->player->damagecount)
			aiming_penalty += actor->player->damagecount; //Blood in face makes it hard to aim
		aiming_value = actor->player->skill.aiming - aiming_penalty;
		if (aiming_value <= 0)
			aiming_value = 1;
		m = ((SHOOTFOV/2)-(aiming_value*SHOOTFOV/200)); //Higher skill is more accurate
		if (m <= 0)
			m = 1; //Prevents lock.

		if (m)
		{
			if (inc[actor->id])
				actor->player->angle += m;
			else
				actor->player->angle -= m;
		}

		if (abs (actor->player->angle - actor->angle) < 4*ANGLE_1)
		{
			inc[actor->id] = !inc[actor->id];
		}

		if (Check_LOS (actor, enemy, (SHOOTFOV/2)))
			no_fire = false;
	}
	if (!no_fire) //If going to fire weapon
	{
		cmd->ucmd.buttons |= BT_ATTACK;
	}
	//Prevents bot from jerking, when firing automatic things with low skill.
	//actor->angle = R_PointToAngle2(actor->x, actor->y, actor->player->enemy->x, actor->player->enemy->y);
}


//This function is called every
//tick (for each bot) to set
//the mate (teammate coop mate).
AActor *DCajunMaster::Choose_Mate (AActor *bot)
{
	int count;
	int count2;
	fixed_t closest_dist, test;
	AActor *target;
	AActor *observer;
	bool p_leader[MAXPLAYERS];

	//is mate alive?
	if (bot->player->mate)
	{
		if (bot->player->mate->health <= 0)
			bot->player->mate = NULL;
		else
			bot->player->last_mate = bot->player->mate;
	}
	if (bot->player->mate) //Still is..
		return bot->player->mate;

	//Check old_mates status.
	if (bot->player->last_mate)
		if (bot->player->last_mate->health <= 0)
			bot->player->last_mate = NULL;

	for (count = 0; count < MAXPLAYERS; count++)
	{
		if (!playeringame[count])
			continue;
		p_leader[count] = false;
		for (count2 = 0; count2 < MAXPLAYERS; count2++)
		{
			if (players[count].isbot)
			{
				if (players[count2].mate == players[count].mo)
				{
					p_leader[count] = true;
					break;
				}
			}
		}
	}

	target = NULL;
	closest_dist = MAXINT;
	if (bot_observer.value)
		observer = players[consoleplayer].mo;
	else
		observer = NULL;

	//Check for player friends
	for (count = 0; count < MAXPLAYERS; count++)
	{
		if (playeringame[count]
			&& players[count].mo
			&& bot != players[count].mo
			&& (bot->IsTeammate (players[count].mo) || !deathmatch.value)
			&& players[count].mo->health > 0
			&& players[count].mo != observer
			&& ((bot->health/2) <= players[count].mo->health || !deathmatch.value)
			&& !p_leader[count]) //taken?
		{

			if (P_CheckSight (bot, players[count].mo, true))
			{
				test = P_AproxDistance (players[count].mo->x-bot->x,
										players[count].mo->y-bot->y);

				if (test < closest_dist)
				{
					closest_dist = test;
					target = players[count].mo;
				}
			}
		}
	}

/*
	//Make a introducing to mate.
	if(target && target!=bot->player->last_mate)
	{
		if((P_Random()%(200*bglobal.botnum))<3)
		{
			bot->player->chat = c_teamup;
			if(target->bot)
					strcpy(bot->player->c_target, botsingame[target->bot_id]);
						else if(target->player)
					strcpy(bot->player->c_target, player_names[target->play_id]);
		}
	}
*/

	return target;

}

//MAKEME: Make this a smart decision
AActor *DCajunMaster::Find_enemy (AActor *bot)
{
	int count;
	fixed_t closest_dist, temp; //To target.
	AActor *target;
	angle_t vangle;
	AActor *observer;

	//Allow monster killing. keep monster enemy.
	if (!deathmatch.value)
		return NULL;

	//Note: It's hard to ambush a bot who is not alone
	if (bot->player->allround || bot->player->mate)
		vangle = ANGLE_MAX;
	else
		vangle = ENEMY_SCAN_FOV;
	bot->player->allround = false;

	target = NULL;
	closest_dist = MAXINT;
	if (bot_observer.value)
		observer = players[consoleplayer].mo;
	else
		observer = NULL;

	for (count = 0; count < MAXPLAYERS; count++)
	{
		if (playeringame[count] && !bot->IsTeammate (players[count].mo)
			&& players[count].mo != observer
			&& players[count].mo->health > 0
			&& bot != players[count].mo)
		{
			if (Check_LOS (bot, players[count].mo, vangle)) //Here's a strange one, when bot is standing still, the P_CheckSight within Check_LOS almost always returns false. tought it should be the same checksight as below but.. (below works) something must be fuckin wierd screded up. 
			//if(P_CheckSight( bot, players[count].mo))
			{
				temp = P_AproxDistance (players[count].mo->x-bot->x,
										players[count].mo->y-bot->y);

				//Too dark?
				if (temp > DARK_DIST &&
					players[count].mo->subsector->sector->lightlevel < WHATS_DARK &&
					bot->player->powers[pw_infrared])
					continue;

				if (temp < closest_dist)
				{
					closest_dist = temp;
					target = players[count].mo;
				}
			}
		}
	}

	return target;
}

//Creates a temporary mobj (invisible) at the given location.
void DCajunMaster::SetBodyAt (fixed_t x, fixed_t y, fixed_t z, int hostnum)
{
	if (hostnum == 1)
	{
		if (body1)
			body1->SetOrigin (x, y, z);
		else
			body1 = new AActor (x, y, z, MT_NODE);
	}
	else if (hostnum == 2)
	{
		if (body2)
			body2->SetOrigin (x, y, z);
		else
			body2 = new AActor (x, y, z, MT_NODE);
	}
}

//------------------------------------------
//    FireRox()
//
//Returns NULL if shouldn't fire
//else an angle (in degrees) are given
//This function assumes actor->player->angle
//has been set an is the main aiming angle.

//Emulates missile travel. Returns position of explosion.
pos_t DCajunMaster::FakeFire (AActor *source, AActor *dest, ticcmd_t *cmd)
{
	pos_t pos;

	AActor *th = new AActor (source->x, source->y, source->z + 4*8*FRACUNIT, MT_BOTTRACE);
	
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

	while (1)
	{
		th->x += th->momx;
		th->y += th->momy;
		th->z += th->momz;

		if (!CleanAhead (th, th->x, th->y, cmd))
		{
			pos.x = th->x;
			pos.y = th->y;
			pos.z = th->z;
			delete th;
			return pos;
		}
	}
}

angle_t DCajunMaster::FireRox (AActor *bot, AActor *enemy, ticcmd_t *cmd)
{
	fixed_t dist;
	angle_t ang;
	pos_t pos;
	AActor *actor;
	int m;

	SetBodyAt(bot->x + FixedMul(bot->momx, 5*FRACUNIT), bot->y + FixedMul(bot->momy, 5*FRACUNIT), bot->z+(bot->info->height/2), 2);
		actor = bglobal.body2;

	dist = P_AproxDistance (actor->x-enemy->x, actor->y-enemy->y);
	if (dist < SAFE_SELF_MISDIST)
		return NULL;
	//Predict.
	m = (((dist+1)/FRACUNIT)/mobjinfo[MT_ROCKET].speed);

	SetBodyAt (enemy->x + FixedMul (enemy->momx, (m+2*FRACUNIT)),
			   enemy->y + FixedMul(enemy->momy, (m+2*FRACUNIT)), ONFLOORZ, 1);
	dist = P_AproxDistance(actor->x-bglobal.body1->x, actor->y-bglobal.body1->y);
	//try the predicted location
	if (P_CheckSight (actor, bglobal.body1, true)) //See the predicted location, so give a test missile
	{
		if (SafeCheckPosition (bot, actor->x, actor->y))
		{
			pos = FakeFire (actor, bglobal.body1, cmd);
			if (P_AproxDistance (actor->x-pos.x, actor->y-pos.y) > SAFE_SELF_MISDIST)
			{
				ang = R_PointToAngle2 (actor->x, actor->y, bglobal.body1->x, bglobal.body1->y);
				return ang;
			}
		}
	}
	//Try fire straight.
	if (P_CheckSight (actor, enemy, false))
	{
		pos = FakeFire (bot, enemy, cmd);
		if (P_AproxDistance(bot->x-pos.x, bot->y-pos.y)>SAFE_SELF_MISDIST)
		{
			ang = R_PointToAngle2(bot->x, bot->y, enemy->x, enemy->y);
			return ang;
		}
	}
	return NULL;
}

// [RH] We absolutely do not want to pick things up here. The bot code is
// executed apart from all the other simulation code, so we don't want it
// creating side-effects during gameplay.
bool DCajunMaster::SafeCheckPosition (AActor *actor, fixed_t x, fixed_t y)
{
	int savedFlags = actor->flags;
	actor->flags &= ~MF_PICKUP;
	bool res = P_CheckPosition (actor, x, y) ? true : false;
	actor->flags = savedFlags;
	return res;
}