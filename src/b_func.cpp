/*******************************
* B_spawn.c                    *
* Description:                 *
* various procedures that the  *
* bot need to work             *
*******************************/

#include <stdlib.h>

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "b_bot.h"
#include "g_game.h"
#include "m_random.h"
#include "r_sky.h"
#include "st_stuff.h"
#include "stats.h"
#include "i_system.h"
#include "s_sound.h"
#include "d_event.h"

static FRandom pr_botdofire ("BotDoFire");


//Checks TRUE reachability from
//one looker to another. First mobj (looker) is looker.
bool FCajunMaster::Reachable (AActor *looker, AActor *rtarget)
{
	if (looker == rtarget)
		return false;

	if ((rtarget->Sector->ceilingplane.ZatPoint (rtarget->x, rtarget->y) -
		 rtarget->Sector->floorplane.ZatPoint (rtarget->x, rtarget->y))
		< looker->height) //Where rtarget is, looker can't be.
		return false;

	sector_t *last_s = looker->Sector;
	fixed_t last_z = last_s->floorplane.ZatPoint (looker->x, looker->y);
	fixed_t estimated_dist = P_AproxDistance (looker->x - rtarget->x, looker->y - rtarget->y);
	bool reachable = true;

	FPathTraverse it(looker->x+looker->velx, looker->y+looker->vely, rtarget->x, rtarget->y, PT_ADDLINES|PT_ADDTHINGS);
	intercept_t *in;
	while ((in = it.Next()))
	{
		fixed_t hitx, hity;
		fixed_t frac;
		line_t *line;
		AActor *thing;
		fixed_t dist;
		sector_t *s;

		frac = in->frac - FixedDiv (4*FRACUNIT, MAX_TRAVERSE_DIST);
		dist = FixedMul (frac, MAX_TRAVERSE_DIST);

		hitx = it.Trace().x + FixedMul (looker->velx, frac);
		hity = it.Trace().y + FixedMul (looker->vely, frac);

		if (in->isaline)
		{
			line = in->d.line;

			if (!(line->flags & ML_TWOSIDED) || (line->flags & (ML_BLOCKING|ML_BLOCKEVERYTHING|ML_BLOCK_PLAYERS)))
			{
				return false; //Cannot continue.
			}
			else
			{
				//Determine if going to use backsector/frontsector.
				s = (line->backsector == last_s) ? line->frontsector : line->backsector;
				fixed_t ceilingheight = s->ceilingplane.ZatPoint (hitx, hity);
				fixed_t floorheight = s->floorplane.ZatPoint (hitx, hity);

				if (!bglobal.IsDangerous (s) &&		//Any nukage/lava?
					(floorheight <= (last_z+MAXMOVEHEIGHT)
					&& ((ceilingheight == floorheight && line->special)
						|| (ceilingheight - floorheight) >= looker->height))) //Does it fit?
				{
					last_z = floorheight;
					last_s = s;
					continue;
				}
				else
				{
					return false;
				}
			}
		}

		if (dist > estimated_dist)
		{
			return true;
		}

		thing = in->d.thing;
		if (thing == looker) //Can't reach self in this case.
			continue;
		if (thing == rtarget && (rtarget->Sector->floorplane.ZatPoint (rtarget->x, rtarget->y) <= (last_z+MAXMOVEHEIGHT)))
		{
			return true;
		}

		reachable = false;
	}
	return reachable;
}

//doesnt check LOS, checks visibility with a set view angle.
//B_Checksight checks LOS (straight line)
//----------------------------------------------------------------------
//Check if mo1 has free line to mo2
//and if mo2 is within mo1 viewangle (vangle) given with normal degrees.
//if these conditions are true, the function returns true.
//GOOD TO KNOW is that the player's view angle
//in doom is 90 degrees infront.
bool FCajunMaster::Check_LOS (AActor *from, AActor *to, angle_t vangle)
{
	if (!P_CheckSight (from, to, SF_SEEPASTBLOCKEVERYTHING))
		return false; // out of sight
	if (vangle == ANGLE_MAX)
		return true;
	if (vangle == 0)
		return false; //Looker seems to be blind.

	return (angle_t)abs (R_PointToAngle2 (from->x, from->y, to->x, to->y) - from->angle) <= vangle/2;
}

//-------------------------------------
//Bot_Dofire()
//-------------------------------------
//The bot will check if it's time to fire
//and do so if that is the case.
void FCajunMaster::Dofire (AActor *actor, ticcmd_t *cmd)
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

	if (actor->player->ReadyWeapon == NULL)
		return;

	if (actor->player->damagecount > actor->player->skill.isp)
	{
		actor->player->first_shot = true;
		return;
	}

	//Reaction skill thing.
	if (actor->player->first_shot &&
		!(actor->player->ReadyWeapon->WeaponFlags & WIF_BOT_REACTION_SKILL_THING))
	{
		actor->player->t_react = (100-actor->player->skill.reaction+1)/((pr_botdofire()%3)+3);
	}
	actor->player->first_shot = false;
	if (actor->player->t_react)
		return;

	//MAKEME: Decrease the rocket suicides even more.

	no_fire = true;
	//actor->player->angle = R_PointToAngle2(actor->x, actor->y, actor->player->enemy->x, actor->player->enemy->y);
	//Distance to enemy.
	dist = P_AproxDistance ((actor->x + actor->velx) - (enemy->x + enemy->velx),
		(actor->y + actor->vely) - (enemy->y + enemy->vely));

	//FIRE EACH TYPE OF WEAPON DIFFERENT: Here should all the different weapons go.
	if (actor->player->ReadyWeapon->WeaponFlags & WIF_MELEEWEAPON)
	{
		if ((actor->player->ReadyWeapon->ProjectileType != NULL))
		{
			if (actor->player->ReadyWeapon->CheckAmmo (AWeapon::PrimaryFire, false, true))
			{
				// This weapon can fire a projectile and has enough ammo to do so
				goto shootmissile;
			}
			else if (!(actor->player->ReadyWeapon->WeaponFlags & WIF_AMMO_OPTIONAL))
			{
				// Ammo is required, so don't shoot. This is for weapons that shoot
				// missiles that die at close range, such as the powered-up Phoneix Rod.
				return;
			}
		}
		else
		{
			//*4 is for atmosphere,  the chainsaws sounding and all..
			no_fire = (dist > (MELEERANGE*4));
		}
	}
	else if (actor->player->ReadyWeapon->WeaponFlags & WIF_BOT_BFG)
	{
		//MAKEME: This should be smarter.
		if ((pr_botdofire()%200)<=actor->player->skill.reaction)
			if(Check_LOS(actor, actor->player->enemy, SHOOTFOV))
				no_fire = false;
	}
	else if (actor->player->ReadyWeapon->ProjectileType != NULL)
	{
		if (actor->player->ReadyWeapon->WeaponFlags & WIF_BOT_EXPLOSIVE)
		{
			//Special rules for RL
			an = FireRox (actor, enemy, cmd);
			if(an)
			{
				actor->player->angle = an;
				//have to be somewhat precise. to avoid suicide.
				if (abs (actor->player->angle - actor->angle) < 12*ANGLE_1)
				{
					actor->player->t_rocket = 9;
					no_fire = false;
				}
			}
		}
		// prediction aiming
shootmissile:
		dist = P_AproxDistance (actor->x - enemy->x, actor->y - enemy->y);
		m = dist / GetDefaultByType (actor->player->ReadyWeapon->ProjectileType)->Speed;
		SetBodyAt (enemy->x + enemy->velx*m*2, enemy->y + enemy->vely*m*2, enemy->z, 1);
		actor->player->angle = R_PointToAngle2 (actor->x, actor->y, body1->x, body1->y);
		if (Check_LOS (actor, enemy, SHOOTFOV))
			no_fire = false;
	}
	else
	{
		//Other weapons, mostly instant hit stuff.
		actor->player->angle = R_PointToAngle2 (actor->x, actor->y, enemy->x, enemy->y);
		aiming_penalty = 0;
		if (enemy->flags & MF_SHADOW)
			aiming_penalty += (pr_botdofire()%25)+10;
		if (enemy->Sector->lightlevel<WHATS_DARK/* && !(actor->player->powers & PW_INFRARED)*/)
			aiming_penalty += pr_botdofire()%40;//Dark
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
			if (inc[actor->player - players])
				actor->player->angle += m;
			else
				actor->player->angle -= m;
		}

		if (abs (actor->player->angle - actor->angle) < 4*ANGLE_1)
		{
			inc[actor->player - players] = !inc[actor->player - players];
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
AActor *FCajunMaster::Choose_Mate (AActor *bot)
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
			if (players[count].isbot
				&& players[count2].mate == players[count].mo)
			{
				p_leader[count] = true;
				break;
			}
		}
	}

	target = NULL;
	closest_dist = FIXED_MAX;
	if (bot_observer)
		observer = players[consoleplayer].mo;
	else
		observer = NULL;

	//Check for player friends
	for (count = 0; count < MAXPLAYERS; count++)
	{
		player_t *client = &players[count];

		if (playeringame[count]
			&& client->mo
			&& bot != client->mo
			&& (bot->IsTeammate (client->mo) || !deathmatch)
			&& client->mo->health > 0
			&& client->mo != observer
			&& ((bot->health/2) <= client->mo->health || !deathmatch)
			&& !p_leader[count]) //taken?
		{

			if (P_CheckSight (bot, client->mo, SF_IGNOREVISIBILITY))
			{
				test = P_AproxDistance (client->mo->x - bot->x,
										client->mo->y - bot->y);

				if (test < closest_dist)
				{
					closest_dist = test;
					target = client->mo;
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
AActor *FCajunMaster::Find_enemy (AActor *bot)
{
	int count;
	fixed_t closest_dist, temp; //To target.
	AActor *target;
	angle_t vangle;
	AActor *observer;

	if (!deathmatch)
	{ // [RH] Take advantage of the Heretic/Hexen code to be a little smarter
		return P_RoughMonsterSearch (bot, 20);
	}

	//Note: It's hard to ambush a bot who is not alone
	if (bot->player->allround || bot->player->mate)
		vangle = ANGLE_MAX;
	else
		vangle = ENEMY_SCAN_FOV;
	bot->player->allround = false;

	target = NULL;
	closest_dist = FIXED_MAX;
	if (bot_observer)
		observer = players[consoleplayer].mo;
	else
		observer = NULL;

	for (count = 0; count < MAXPLAYERS; count++)
	{
		player_t *client = &players[count];
		if (playeringame[count]
			&& !bot->IsTeammate (client->mo)
			&& client->mo != observer
			&& client->mo->health > 0
			&& bot != client->mo)
		{
			if (Check_LOS (bot, client->mo, vangle)) //Here's a strange one, when bot is standing still, the P_CheckSight within Check_LOS almost always returns false. tought it should be the same checksight as below but.. (below works) something must be fuckin wierd screded up. 
			//if(P_CheckSight( bot, players[count].mo))
			{
				temp = P_AproxDistance (client->mo->x - bot->x,
										client->mo->y - bot->y);

				//Too dark?
				if (temp > DARK_DIST &&
					client->mo->Sector->lightlevel < WHATS_DARK /*&&
					bot->player->Powers & PW_INFRARED*/)
					continue;

				if (temp < closest_dist)
				{
					closest_dist = temp;
					target = client->mo;
				}
			}
		}
	}

	return target;
}



//Creates a temporary mobj (invisible) at the given location.
void FCajunMaster::SetBodyAt (fixed_t x, fixed_t y, fixed_t z, int hostnum)
{
	if (hostnum == 1)
	{
		if (body1)
		{
			body1->SetOrigin (x, y, z);
		}
		else
		{
			body1 = Spawn ("CajunBodyNode", x, y, z, NO_REPLACE);
		}
	}
	else if (hostnum == 2)
	{
		if (body2)
		{
			body2->SetOrigin (x, y, z);
		}
		else
		{
			body2 = Spawn ("CajunBodyNode", x, y, z, NO_REPLACE);
		}
	}
}

//------------------------------------------
//    FireRox()
//
//Returns NULL if shouldn't fire
//else an angle (in degrees) are given
//This function assumes actor->player->angle
//has been set an is the main aiming angle.


//Emulates missile travel. Returns distance travelled.
fixed_t FCajunMaster::FakeFire (AActor *source, AActor *dest, ticcmd_t *cmd)
{
	AActor *th = Spawn ("CajunTrace", source->x, source->y, source->z + 4*8*FRACUNIT, NO_REPLACE);
	
	th->target = source;		// where it came from

	float speed = (float)th->Speed;

	FVector3 velocity;
	velocity[0] = FIXED2FLOAT(dest->x - source->x);
	velocity[1] = FIXED2FLOAT(dest->y - source->y);
	velocity[2] = FIXED2FLOAT(dest->z - source->z);
	velocity.MakeUnit();
	th->velx = FLOAT2FIXED(velocity[0] * speed);
	th->vely = FLOAT2FIXED(velocity[1] * speed);
	th->velz = FLOAT2FIXED(velocity[2] * speed);

	fixed_t dist = 0;

	while (dist < SAFE_SELF_MISDIST)
	{
		dist += th->Speed;
		th->SetOrigin (th->x + th->velx, th->y + th->vely, th->z + th->velz);
		if (!CleanAhead (th, th->x, th->y, cmd))
			break;
	}
	th->Destroy ();
	return dist;
}

angle_t FCajunMaster::FireRox (AActor *bot, AActor *enemy, ticcmd_t *cmd)
{
	fixed_t dist;
	angle_t ang;
	AActor *actor;
	int m;

	SetBodyAt (bot->x + FixedMul(bot->velx, 5*FRACUNIT),
			   bot->y + FixedMul(bot->vely, 5*FRACUNIT),
			   bot->z + (bot->height / 2), 2);

	actor = bglobal.body2;

	dist = P_AproxDistance (actor->x-enemy->x, actor->y-enemy->y);
	if (dist < SAFE_SELF_MISDIST)
		return 0;
	//Predict.
	m = (((dist+1)/FRACUNIT) / GetDefaultByName("Rocket")->Speed);

	SetBodyAt (enemy->x + FixedMul(enemy->velx, (m+2*FRACUNIT)),
			   enemy->y + FixedMul(enemy->vely, (m+2*FRACUNIT)), ONFLOORZ, 1);
	dist = P_AproxDistance(actor->x-bglobal.body1->x, actor->y-bglobal.body1->y);
	//try the predicted location
	if (P_CheckSight (actor, bglobal.body1, SF_IGNOREVISIBILITY)) //See the predicted location, so give a test missile
	{
		FCheckPosition tm;
		if (SafeCheckPosition (bot, actor->x, actor->y, tm))
		{
			if (FakeFire (actor, bglobal.body1, cmd) >= SAFE_SELF_MISDIST)
			{
				ang = R_PointToAngle2 (actor->x, actor->y, bglobal.body1->x, bglobal.body1->y);
				return ang;
			}
		}
	}
	//Try fire straight.
	if (P_CheckSight (actor, enemy, 0))
	{
		if (FakeFire (bot, enemy, cmd) >= SAFE_SELF_MISDIST)
		{
			ang = R_PointToAngle2(bot->x, bot->y, enemy->x, enemy->y);
			return ang;
		}
	}
	return 0;
}

// [RH] We absolutely do not want to pick things up here. The bot code is
// executed apart from all the other simulation code, so we don't want it
// creating side-effects during gameplay.
bool FCajunMaster::SafeCheckPosition (AActor *actor, fixed_t x, fixed_t y, FCheckPosition &tm)
{
	int savedFlags = actor->flags;
	actor->flags &= ~MF_PICKUP;
	bool res = P_CheckPosition (actor, x, y, tm);
	actor->flags = savedFlags;
	return res;
}
