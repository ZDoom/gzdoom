/*******************************
* B_spawn.c                    *
* Description:                 *
* various procedures that the  *
* bot need to work             *
*******************************/

#include <malloc.h>

#include "doomtype.h"
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
#include "a_doomglobal.h"

static FRandom pr_botdofire ("BotDoFire");

//Used with Reachable().
static AActor *looker;
static AActor *rtarget;
static bool reachable;
static fixed_t last_z;
static sector_t *last_s;
static fixed_t estimated_dist;

static int PTR_Reachable (intercept_t *in)
{
	fixed_t hitx, hity;
	fixed_t frac;
	line_t *line;
	AActor *thing;
	fixed_t dist;
	sector_t *s;

	frac = in->frac - FixedDiv (4*FRACUNIT, MAX_TRAVERSE_DIST);
	dist = FixedMul (frac, MAX_TRAVERSE_DIST);

	hitx = trace.x + FixedMul (looker->momx, frac);
	hity = trace.y + FixedMul (looker->momy, frac);

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
				return true;
			}
			else
			{
				return (reachable = false);
			}
		}
	}

	if (dist > estimated_dist)
	{
		reachable = true;
		return false; //Don't need to continue.
	}

	thing = in->d.thing;
	if (thing == looker) //Can't reach self in this case.
		return true;
	if (thing == rtarget && (rtarget->Sector->floorplane.ZatPoint (rtarget->x, rtarget->y) <= (last_z+MAXMOVEHEIGHT)))
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

	if ((target->Sector->ceilingplane.ZatPoint (target->x, target->y) -
		 target->Sector->floorplane.ZatPoint (target->x, target->y))
		< actor->height) //Where target is, looker can't be.
		return false;

	looker = actor;
	rtarget = target;
	last_s = actor->Sector;
	last_z = last_s->floorplane.ZatPoint (actor->x, actor->y);
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
//GOOD TO KNOW is that the player's view angle
//in doom is 90 degrees infront.
bool DCajunMaster::Check_LOS (AActor *from, AActor *to, angle_t vangle)
{
	if (!P_CheckSight (from, to, false))
		return false; // out of sight
	if (vangle == ANGLE_MAX)
		return true;
	if (vangle == 0)
		return false; //Looker seems to be blind.

	return abs (R_PointToAngle2 (from->x, from->y, to->x, to->y) - from->angle) <= vangle/2;
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
		!(actor->player->readyweapon==wp_bfg ||
		  actor->player->readyweapon==wp_missile ||
		  actor->player->readyweapon==wp_phoenixrod ||
		  actor->player->readyweapon==wp_mace))
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
	dist = P_AproxDistance ((actor->x + actor->momx) - (enemy->x + enemy->momx),
		(actor->y + actor->momy) - (enemy->y + enemy->momy));

	//FIRE EACH TYPE OF WEAPON DIFFERENT: Here should all the different weapons go.
	switch (actor->player->readyweapon)
	{
	case wp_missile: //Special rules for RL
	case wp_phoenixrod:
	case wp_cfire:
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

	case wp_fhammer:
		if (actor->player->ammo[MANA_2] == 0)
		{
			goto fighternoammo;
		}
	case wp_faxe:
	case wp_plasma: //Plasma (prediction aiming)
	case wp_skullrod:
	case wp_crossbow:
	case wp_cstaff:
	case wp_mfrost:
	case wp_mlightning:
	case wp_fsword:
	case wp_choly:
	case wp_mstaff:
		//Here goes the prediction.
		dist = P_AproxDistance (actor->x - enemy->x, actor->y - enemy->y);
		m = dist;
		switch (actor->player->readyweapon)
		{
		case wp_plasma:
			m /= GetDefaultByName ("PlasmaBall")->Speed;
			break;
		case wp_skullrod:
			m /= GetDefaultByName ("HornRodFX1")->Speed;
			break;
		case wp_crossbow:
			m /= GetDefaultByName ("CrossbowFX1")->Speed;
			break;
		case wp_cstaff:
			m /= GetDefaultByName ("CStaffMissile")->Speed;
			break;
		case wp_mfrost:
			m /= GetDefaultByName ("FrostMissile")->Speed;
			break;
		case wp_mlightning:
			m /= GetDefaultByName ("LightningCeiling")->Speed;
			break;
		case wp_fsword:
			m /= GetDefaultByName ("FSwordMissile")->Speed;
			break;
		case wp_choly:
			m /= GetDefaultByName ("HolySpirit")->Speed;
			break;
		case wp_mstaff:
			m /= GetDefaultByName ("MageStaffFX2")->Speed;
			break;
		}
		SetBodyAt (enemy->x + enemy->momx*m*2, enemy->y + enemy->momy*m*2, ONFLOORZ, 1);
		actor->player->angle = R_PointToAngle2 (actor->x, actor->y, body1->x, body1->y);
		if (Check_LOS (actor, enemy, SHOOTFOV))
			no_fire = false;
		break;

	case wp_chainsaw:
	case wp_fist:
	case wp_staff:
	case wp_gauntlets:
	case wp_snout:
	case wp_beak:
	case wp_ffist:
	case wp_cmace:
fighternoammo:
		no_fire = (dist > (MELEERANGE*4)); //*4 is for atmosphere,  the chainsaws sounding and all..
		break;

	case wp_bfg:
		//MAKEME: This should be smarter.
		if ((pr_botdofire()%200)<=actor->player->skill.reaction)
			if(Check_LOS(actor, actor->player->enemy, SHOOTFOV))
				no_fire = false;
		break;

	default: //Other weapons, mostly instant hit stuff.
		actor->player->angle = R_PointToAngle2 (actor->x, actor->y, enemy->x, enemy->y);
		aiming_penalty = 0;
		if (enemy->flags & MF_SHADOW)
			aiming_penalty += (pr_botdofire()%25)+10;
		if (enemy->Sector->lightlevel<WHATS_DARK && !actor->player->powers[pw_infrared])
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

			if (P_CheckSight (bot, client->mo, true))
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
AActor *DCajunMaster::Find_enemy (AActor *bot)
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
					client->mo->Sector->lightlevel < WHATS_DARK &&
					bot->player->powers[pw_infrared])
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


class ACajunBodyNode : public AActor
{
	DECLARE_STATELESS_ACTOR (ACajunBodyNode, AActor)
};

IMPLEMENT_STATELESS_ACTOR (ACajunBodyNode, Any, -1, 0)
	PROP_Flags (MF_NOSECTOR | MF_NOGRAVITY)
	PROP_RenderFlags (RF_INVISIBLE)
END_DEFAULTS

//Creates a temporary mobj (invisible) at the given location.
void DCajunMaster::SetBodyAt (fixed_t x, fixed_t y, fixed_t z, int hostnum)
{
	if (hostnum == 1)
	{
		if (body1)
			body1->SetOrigin (x, y, z);
		else
			body1 = Spawn<ACajunBodyNode> (x, y, z);
	}
	else if (hostnum == 2)
	{
		if (body2)
			body2->SetOrigin (x, y, z);
		else
			body2 = Spawn<ACajunBodyNode> (x, y, z);
	}
}

//------------------------------------------
//    FireRox()
//
//Returns NULL if shouldn't fire
//else an angle (in degrees) are given
//This function assumes actor->player->angle
//has been set an is the main aiming angle.

class ACajunTrace : public AActor
{
	DECLARE_STATELESS_ACTOR (ACajunTrace, AActor)
};

IMPLEMENT_STATELESS_ACTOR (ACajunTrace, Any, -1, 0)
	PROP_SpeedFixed (12)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
END_DEFAULTS

//Emulates missile travel. Returns distance travelled.
fixed_t DCajunMaster::FakeFire (AActor *source, AActor *dest, ticcmd_t *cmd)
{
	AActor *th = Spawn<ACajunTrace> (source->x, source->y, source->z + 4*8*FRACUNIT);
	
	th->target = source;		// where it came from

	vec3_t velocity;
	float speed = (float)th->Speed;

	velocity[0] = FIXED2FLOAT(dest->x - source->x);
	velocity[1] = FIXED2FLOAT(dest->y - source->y);
	velocity[2] = FIXED2FLOAT(dest->z - source->z);
	VectorNormalize (velocity);
	th->momx = FLOAT2FIXED(velocity[0] * speed);
	th->momy = FLOAT2FIXED(velocity[1] * speed);
	th->momz = FLOAT2FIXED(velocity[2] * speed);

	fixed_t dist = 0;

	while (dist < SAFE_SELF_MISDIST)
	{
		dist += th->Speed;
		th->SetOrigin (th->x + th->momx, th->y + th->momy, th->z + th->momz);
		if (!CleanAhead (th, th->x, th->y, cmd))
			break;
	}
	th->Destroy ();
	return dist;
}

angle_t DCajunMaster::FireRox (AActor *bot, AActor *enemy, ticcmd_t *cmd)
{
	fixed_t dist;
	angle_t ang;
	AActor *actor;
	int m;

	SetBodyAt (bot->x + FixedMul(bot->momx, 5*FRACUNIT),
			   bot->y + FixedMul(bot->momy, 5*FRACUNIT),
			   bot->z + (bot->height / 2), 2);

	actor = bglobal.body2;

	dist = P_AproxDistance (actor->x-enemy->x, actor->y-enemy->y);
	if (dist < SAFE_SELF_MISDIST)
		return 0;
	//Predict.
	m = (((dist+1)/FRACUNIT) / GetDefault<ARocket>()->Speed);

	SetBodyAt (enemy->x + FixedMul (enemy->momx, (m+2*FRACUNIT)),
			   enemy->y + FixedMul(enemy->momy, (m+2*FRACUNIT)), ONFLOORZ, 1);
	dist = P_AproxDistance(actor->x-bglobal.body1->x, actor->y-bglobal.body1->y);
	//try the predicted location
	if (P_CheckSight (actor, bglobal.body1, true)) //See the predicted location, so give a test missile
	{
		if (SafeCheckPosition (bot, actor->x, actor->y))
		{
			if (FakeFire (actor, bglobal.body1, cmd) >= SAFE_SELF_MISDIST)
			{
				ang = R_PointToAngle2 (actor->x, actor->y, bglobal.body1->x, bglobal.body1->y);
				return ang;
			}
		}
	}
	//Try fire straight.
	if (P_CheckSight (actor, enemy, false))
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
bool DCajunMaster::SafeCheckPosition (AActor *actor, fixed_t x, fixed_t y)
{
	int savedFlags = actor->flags;
	actor->flags &= ~MF_PICKUP;
	bool res = P_CheckPosition (actor, x, y) ? true : false;
	actor->flags = savedFlags;
	return res;
}
