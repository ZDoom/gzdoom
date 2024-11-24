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
#include "p_maputl.h"
#include "b_bot.h"
#include "g_game.h"
#include "d_event.h"
#include "d_player.h"
#include "p_spec.h"
#include "p_checkposition.h"
#include "actorinlines.h"

static FRandom pr_botdofire ("BotDoFire");


//Checks TRUE reachability from bot to a looker.
bool DBot::Reachable (AActor *rtarget)
{
	if (player->mo == rtarget)
		return false;

	if ((rtarget->Sector->ceilingplane.ZatPoint (rtarget) -
		 rtarget->Sector->floorplane.ZatPoint (rtarget))
		< player->mo->Height) //Where rtarget is, player->mo can't be.
		return false;

	sector_t *last_s = player->mo->Sector;
	double last_z = last_s->floorplane.ZatPoint (player->mo);
	double estimated_dist = player->mo->Distance2D(rtarget);
	bool reachable = true;

	FPathTraverse it(Level, player->mo->X()+player->mo->Vel.X, player->mo->Y()+player->mo->Vel.Y, rtarget->X(), rtarget->Y(), PT_ADDLINES|PT_ADDTHINGS);
	intercept_t *in;
	while ((in = it.Next()))
	{
		double hitx, hity;
		double frac;
		line_t *line;
		AActor *thing;
		double dist;
		sector_t *s;

		frac = in->frac - 4 /MAX_TRAVERSE_DIST;
		dist = frac * MAX_TRAVERSE_DIST;

		hitx = it.Trace().x + player->mo->Vel.X * frac;
		hity = it.Trace().y + player->mo->Vel.Y * frac;

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
				double ceilingheight = s->ceilingplane.ZatPoint (hitx, hity);
				double floorheight = s->floorplane.ZatPoint (hitx, hity);

				if (!Level->BotInfo.IsDangerous (s) &&		//Any nukage/lava?
					(floorheight <= (last_z+MAXMOVEHEIGHT)
					&& ((ceilingheight == floorheight && line->special)
						|| (ceilingheight - floorheight) >= player->mo->Height))) //Does it fit?
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
		if (thing == player->mo) //Can't reach self in this case.
			continue;
		if (thing == rtarget && (rtarget->Sector->floorplane.ZatPoint (rtarget) <= (last_z+MAXMOVEHEIGHT)))
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
bool DBot::Check_LOS (AActor *to, DAngle vangle)
{
	if (!P_CheckSight (player->mo, to, SF_SEEPASTBLOCKEVERYTHING))
		return false; // out of sight
	if (vangle >= DAngle::fromDeg(360.))
		return true;
	if (vangle == nullAngle)
		return false; //Looker seems to be blind.

	return absangle(player->mo->AngleTo(to), player->mo->Angles.Yaw) <= (vangle/2);
}

//-------------------------------------
//Bot_Dofire()
//-------------------------------------
//The bot will check if it's time to fire
//and do so if that is the case.
void DBot::Dofire (usercmd_t *cmd)
{
	bool no_fire; //used to prevent bot from pumping rockets into nearby walls.
	int aiming_penalty=0; //For shooting at shading target, if screen is red, MAKEME: When screen red.
	int aiming_value; //The final aiming value.
	double Dist;
	DAngle an;
	DAngle m;
	double fm;

	if (!enemy || !(enemy->flags & MF_SHOOTABLE) || enemy->health <= 0)
		return;

	if (player->ReadyWeapon == NULL)
		return;

	if (player->damagecount > (unsigned)skill.isp)
	{
		first_shot = true;
		return;
	}

	//Reaction skill thing.
	if (first_shot &&
		!(GetBotInfo(player->ReadyWeapon).flags & BIF_BOT_REACTION_SKILL_THING))
	{
		t_react = (100-skill.reaction+1)/((pr_botdofire()%3)+3);
	}
	first_shot = false;
	if (t_react)
		return;

	//MAKEME: Decrease the rocket suicides even more.

	no_fire = true;
	//Distance to enemy.
	Dist = player->mo->Distance2D(enemy, player->mo->Vel.X - enemy->Vel.X, player->mo->Vel.Y - enemy->Vel.Y);

	//FIRE EACH TYPE OF WEAPON DIFFERENT: Here should all the different weapons go.
	if (GetBotInfo(player->ReadyWeapon).MoveCombatDist == 0)
	{
		//*4 is for atmosphere,  the chainsaws sounding and all..
		no_fire = (Dist > DEFMELEERANGE*4);
	}
	else if (GetBotInfo(player->ReadyWeapon).flags & BIF_BOT_BFG)
	{
		//MAKEME: This should be smarter.
		if ((pr_botdofire()%200)<=skill.reaction)
			if(Check_LOS(enemy, DAngle::fromDeg(SHOOTFOV)))
				no_fire = false;
	}
	else if (GetBotInfo(player->ReadyWeapon).projectileType != NULL)
	{
		if (GetBotInfo(player->ReadyWeapon).flags & BIF_BOT_EXPLOSIVE)
		{
			//Special rules for RL
			an = FireRox (enemy, cmd);
			if(an != nullAngle)
			{
				Angle = an;
				//have to be somewhat precise. to avoid suicide.
				if (absangle(an, player->mo->Angles.Yaw) < DAngle::fromDeg(12.))
				{
					t_rocket = 9;
					no_fire = false;
				}
			}
		}
		// prediction aiming
		Dist = player->mo->Distance2D(enemy);
		fm = Dist / GetDefaultByType (GetBotInfo(player->ReadyWeapon).projectileType)->Speed;
		Level->BotInfo.SetBodyAt(Level, enemy->Pos() + enemy->Vel.XY() * fm * 2, 1);
		Angle = player->mo->AngleTo(Level->BotInfo.body1);
		if (Check_LOS (enemy, DAngle::fromDeg(SHOOTFOV)))
			no_fire = false;
	}
	else
	{
		//Other weapons, mostly instant hit stuff.
		Angle = player->mo->AngleTo(enemy);
		aiming_penalty = 0;
		if (enemy->flags & MF_SHADOW)
			aiming_penalty += (pr_botdofire()%25)+10;
		if (enemy->Sector->lightlevel<WHATS_DARK/* && !(player->powers & PW_INFRARED)*/)
			aiming_penalty += pr_botdofire()%40;//Dark
		if (player->damagecount)
			aiming_penalty += player->damagecount; //Blood in face makes it hard to aim
		aiming_value = skill.aiming - aiming_penalty;
		if (aiming_value <= 0)
			aiming_value = 1;
		m = DAngle::fromDeg(((SHOOTFOV/2)-(aiming_value*SHOOTFOV/200))); //Higher skill is more accurate
		if (m <= nullAngle)
			m = DAngle::fromDeg(1.); //Prevents lock.

		if (m != nullAngle)
		{
			if (increase)
				Angle += m;
			else
				Angle -= m;
		}

		if (absangle(Angle, player->mo->Angles.Yaw) < DAngle::fromDeg(4.))
		{
			increase = !increase;
		}

		if (Check_LOS (enemy, DAngle::fromDeg(SHOOTFOV/2)))
			no_fire = false;
	}
	if (!no_fire) //If going to fire weapon
	{
		cmd->buttons |= BT_ATTACK;
	}
	//Prevents bot from jerking, when firing automatic things with low skill.
}

bool FCajunMaster::IsLeader (player_t *player)
{
	for (int count = 0; count < MAXPLAYERS; count++)
	{
		if (players[count].Bot != NULL
			&& players[count].Bot->mate == player->mo)
		{
			return true;
		}
	}

	return false;
}

extern int BotWTG;

void FCajunMaster::BotTick(AActor *mo)
{
	BotSupportCycles.Clock();
	m_Thinking = true;
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].Bot == NULL)
			continue;

		if (mo->flags3 & MF3_ISMONSTER)
		{
			if (mo->health > 0
				&& !players[i].Bot->enemy
				&& mo->player ? !mo->IsTeammate(players[i].mo) : true
				&& mo->Distance2D(players[i].mo) < MAX_MONSTER_TARGET_DIST
				&& P_CheckSight(players[i].mo, mo, SF_SEEPASTBLOCKEVERYTHING))
			{ //Probably a monster, so go kill it.
				players[i].Bot->enemy = mo;
			}
		}
		else if (mo->flags & MF_SPECIAL)
		{ //Item pickup time
		  //clock (BotWTG);
			players[i].Bot->WhatToGet(mo);
			//unclock (BotWTG);
			BotWTG++;
		}
		else if (mo->flags & MF_MISSILE)
		{
			if (!players[i].Bot->missile && (mo->flags3 & MF3_WARNBOT))
			{ //warn for incoming missiles.
				if (mo->target != players[i].mo && players[i].Bot->Check_LOS(mo, DAngle::fromDeg(90.)))
					players[i].Bot->missile = mo;
			}
		}
	}
	m_Thinking = false;
	BotSupportCycles.Unclock();
}

//This function is called every
//tick (for each bot) to set
//the mate (teammate coop mate).
AActor *DBot::Choose_Mate ()
{
	int count;
	double closest_dist, test;
	AActor *target;

	//is mate alive?
	if (mate)
	{
		if (mate->health <= 0)
			mate = nullptr;
		else
			last_mate = mate;
	}
	if (mate) //Still is..
		return mate;

	//Check old_mates status.
	if (last_mate)
		if (last_mate->health <= 0)
			last_mate = nullptr;

	target = NULL;
	closest_dist = FLT_MAX;

	//Check for player friends
	for (count = 0; count < MAXPLAYERS; count++)
	{
		player_t *client = &players[count];

		if (playeringame[count]
			&& client->mo
			&& player->mo != client->mo
			&& (player->mo->IsTeammate (client->mo) || !deathmatch)
			&& client->mo->health > 0
			&& ((player->mo->health/2) <= client->mo->health || !deathmatch)
			&& !Level->BotInfo.IsLeader(client)) //taken?
		{
			if (P_CheckSight (player->mo, client->mo, SF_IGNOREVISIBILITY))
			{
				test = client->mo->Distance2D(player->mo);

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
	if(target && target!=last_mate)
	{
		if((P_Random()%(200*Level->BotInfo.botnum))<3)
		{
			chat = c_teamup;
			if(target->bot)
					strcpy(c_target, botsingame[target->bot_id]);
						else if(target->player)
					strcpy(c_target, player_names[target->play_id]);
		}
	}
*/

	return target;

}

//MAKEME: Make this a smart decision
AActor *DBot::Find_enemy ()
{
	int count;
	double closest_dist, temp; //To target.
	AActor *target;
	DAngle vangle;

	if (!deathmatch)
	{ // [RH] Take advantage of the Heretic/Hexen code to be a little smarter
		return P_RoughMonsterSearch (player->mo, 20);
	}

	//Note: It's hard to ambush a bot who is not alone
	if (allround || mate)
		vangle = DAngle::fromDeg(360.);
	else
		vangle = DAngle::fromDeg(ENEMY_SCAN_FOV);
	allround = false;

	target = NULL;
	closest_dist = FLT_MAX;

	for (count = 0; count < MAXPLAYERS; count++)
	{
		player_t *client = &players[count];
		if (playeringame[count]
			&& !player->mo->IsTeammate (client->mo)
			&& client->mo->health > 0
			&& player->mo != client->mo)
		{
			if (Check_LOS (client->mo, vangle)) //Here's a strange one, when bot is standing still, the P_CheckSight within Check_LOS almost always returns false. tought it should be the same checksight as below but.. (below works) something must be fuckin wierd screded up. 
			//if(P_CheckSight(player->mo, players[count].mo))
			{
				temp = client->mo->Distance2D(player->mo);

				//Too dark?
				if (temp > DARK_DIST &&
					client->mo->Sector->lightlevel < WHATS_DARK /*&&
					player->Powers & PW_INFRARED*/)
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
void FCajunMaster::SetBodyAt (FLevelLocals *Level, const DVector3 &pos, int hostnum)
{
	if (hostnum == 1)
	{
		if (body1)
		{
			body1->SetOrigin (pos, false);
		}
		else
		{
			body1 = Spawn (Level, "CajunBodyNode", pos, NO_REPLACE);
		}
	}
	else if (hostnum == 2)
	{
		if (body2)
		{
			body2->SetOrigin (pos, false);
		}
		else
		{
			body2 = Spawn (Level, "CajunBodyNode", pos, NO_REPLACE);
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
double FCajunMaster::FakeFire (AActor *source, AActor *dest, usercmd_t *cmd)
{
	AActor *th = Spawn (source->Level, "CajunTrace", source->PosPlusZ(4*8.), NO_REPLACE);
	
	th->target = source;		// where it came from


	th->Vel = source->Vec3To(dest).Resized(th->Speed);

	double dist = 0;

	while (dist < SAFE_SELF_MISDIST)
	{
		dist += th->Speed;
		th->Move(th->Vel);
		if (!CleanAhead (th, th->X(), th->Y(), cmd))
			break;
	}
	th->Destroy ();
	return dist;
}

DAngle DBot::FireRox (AActor *enemy, usercmd_t *cmd)
{
	double dist;
	AActor *actor;
	double m;

	Level->BotInfo.SetBodyAt(Level, player->mo->PosPlusZ(player->mo->Height / 2) + player->mo->Vel.XY() * 5, 2);

	actor = Level->BotInfo.body2;

	dist = actor->Distance2D (enemy);
	if (dist < SAFE_SELF_MISDIST)
		return nullAngle;
	//Predict.
	m = ((dist+1) / GetDefaultByName("Rocket")->Speed);

	Level->BotInfo.SetBodyAt(Level, DVector3((enemy->Pos().XY() + enemy->Vel * (m + 2)), ONFLOORZ), 1);
	
	//try the predicted location
	if (P_CheckSight (actor, Level->BotInfo.body1, SF_IGNOREVISIBILITY)) //See the predicted location, so give a test missile
	{
		FCheckPosition tm;
		if (Level->BotInfo.SafeCheckPosition (player->mo, actor->X(), actor->Y(), tm))
		{
			if (Level->BotInfo.FakeFire (actor, Level->BotInfo.body1, cmd) >= SAFE_SELF_MISDIST)
			{
				return actor->AngleTo(Level->BotInfo.body1);
			}
		}
	}
	//Try fire straight.
	if (P_CheckSight (actor, enemy, 0))
	{
		if (Level->BotInfo.FakeFire (player->mo, enemy, cmd) >= SAFE_SELF_MISDIST)
		{
			return player->mo->AngleTo(enemy);
		}
	}
	return nullAngle;
}

// [RH] We absolutely do not want to pick things up here. The bot code is
// executed apart from all the other simulation code, so we don't want it
// creating side-effects during gameplay.
bool FCajunMaster::SafeCheckPosition (AActor *actor, double x, double y, FCheckPosition &tm)
{
	ActorFlags savedFlags = actor->flags;
	actor->flags &= ~MF_PICKUP;
	bool res = P_CheckPosition (actor, DVector2(x, y), tm);
	actor->flags = savedFlags;
	return res;
}

void FCajunMaster::StartTravel ()
{
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (players[i].Bot != NULL)
		{
			players[i].Bot->ChangeStatNum (STAT_TRAVELLING);
		}
	}
}

void FCajunMaster::FinishTravel ()
{
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (players[i].Bot != NULL)
		{
			players[i].Bot->ChangeStatNum (STAT_BOT);
		}
	}
}
