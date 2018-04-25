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
* Movement/Roaming code for     *
* the bot's					    *
*********************************/

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "b_bot.h"
#include "g_game.h"
#include "p_lnspec.h"
#include "a_keys.h"
#include "d_event.h"
#include "p_enemy.h"
#include "d_player.h"
#include "p_spec.h"
#include "p_checkposition.h"
#include "actorinlines.h"

static FRandom pr_botopendoor ("BotOpenDoor");
static FRandom pr_bottrywalk ("BotTryWalk");
static FRandom pr_botnewchasedir ("BotNewChaseDir");

// borrow some tables from p_enemy.cpp
extern dirtype_t opposite[9];
extern dirtype_t diags[4];

//Called while the bot moves after its dest mobj
//which can be a weapon/enemy/item whatever.
void DBot::Roam (ticcmd_t *cmd)
{

	if (Reachable(dest))
	{ // Straight towards it.
		Angle = player->mo->AngleTo(dest);
	}
	else if (player->mo->movedir < 8) // turn towards movement direction if not there yet
	{
		// no point doing this with floating point angles...
		unsigned angle = Angle.BAMs() & (unsigned)(7 << 29);
		int delta = angle - (player->mo->movedir << 29);

		if (delta > 0)
			Angle -= 45;
		else if (delta < 0)
			Angle += 45;
	}

	// chase towards destination.
	if (--player->mo->movecount < 0 || !Move (cmd))
	{
		NewChaseDir (cmd);
	}
}

bool DBot::Move (ticcmd_t *cmd)
{
	double tryx, tryy;
	bool try_ok;
	int good;

	if (player->mo->movedir >= DI_NODIR)
	{
		player->mo->movedir = DI_NODIR;	// make sure it's valid.
		return false;
	}

	tryx = player->mo->X() + 8*xspeed[player->mo->movedir];
	tryy = player->mo->Y() + 8*yspeed[player->mo->movedir];

	try_ok = bglobal.CleanAhead (player->mo, tryx, tryy, cmd);

	if (!try_ok) //Anything blocking that could be opened etc..
	{
		if (!spechit.Size ())
			return false;

		player->mo->movedir = DI_NODIR;

		good = 0;
		spechit_t spechit1;
		line_t *ld;

		while (spechit.Pop (spechit1))
		{
			ld = spechit1.line;
			bool tryit = true;

			if (ld->special == Door_LockedRaise && !P_CheckKeys (player->mo, ld->args[3], false))
				tryit = false;
			else if (ld->special == Generic_Door && !P_CheckKeys (player->mo, ld->args[4], false))
				tryit = false;

			if (tryit &&
				(P_TestActivateLine (ld, player->mo, 0, SPAC_Use) ||
				 P_TestActivateLine (ld, player->mo, 0, SPAC_Push)))
			{
				good |= ld == player->mo->BlockingLine ? 1 : 2;
			}
		}
		if (good && ((pr_botopendoor() >= 203) ^ (good & 1)))
		{
			cmd->ucmd.buttons |= BT_USE;
			cmd->ucmd.forwardmove = FORWARDRUN;
			return true;
		}
		else
			return false;
	}
	else //Move forward.
		cmd->ucmd.forwardmove = FORWARDRUN;

	return true;
}

bool DBot::TryWalk (ticcmd_t *cmd)
{
    if (!Move (cmd))
        return false;

    player->mo->movecount = pr_bottrywalk() & 60;
    return true;
}

void DBot::NewChaseDir (ticcmd_t *cmd)
{
    dirtype_t   d[3];

    int         tdir;
    dirtype_t   olddir;

    dirtype_t   turnaround;

    if (!dest)
	{
#ifndef BOT_RELEASE_COMPILE
        Printf ("Bot tried move without destination\n");
#endif
		return;
	}

    olddir = (dirtype_t)player->mo->movedir;
    turnaround = opposite[olddir];

	DVector2 delta = player->mo->Vec2To(dest);

    if (delta.X > 10)
        d[1] = DI_EAST;
    else if (delta.X < -10)
        d[1] = DI_WEST;
    else
        d[1] = DI_NODIR;

    if (delta.Y < -10)
        d[2] = DI_SOUTH;
    else if (delta.Y > 10)
        d[2] = DI_NORTH;
    else
        d[2] = DI_NODIR;

    // try direct route
    if (d[1] != DI_NODIR && d[2] != DI_NODIR)
    {
		player->mo->movedir = diags[((delta.Y < 0) << 1) + (delta.X > 0)];
        if (player->mo->movedir != turnaround && TryWalk(cmd))
            return;
    }

    // try other directions
	if (pr_botnewchasedir() > 200
		|| fabs(delta.Y) > fabs(delta.X))
	{
		tdir = d[1];
		d[1] = d[2];
		d[2] = (dirtype_t)tdir;
	}

    if (d[1]==turnaround)
        d[1]=DI_NODIR;
    if (d[2]==turnaround)
        d[2]=DI_NODIR;

    if (d[1]!=DI_NODIR)
    {
        player->mo->movedir = d[1];
        if (TryWalk (cmd))
            return;
    }

    if (d[2]!=DI_NODIR)
    {
        player->mo->movedir = d[2];

        if (TryWalk(cmd))
            return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir!=DI_NODIR)
    {
        player->mo->movedir = olddir;

        if (TryWalk(cmd))
            return;
    }

    // randomly determine direction of search
    if (pr_botnewchasedir()&1)
    {
        for ( tdir=DI_EAST;
              tdir<=DI_SOUTHEAST;
              tdir++ )
        {
            if (tdir!=turnaround)
            {
                player->mo->movedir = tdir;

                if (TryWalk(cmd))
                    return;
            }
        }
    }
    else
    {
        for ( tdir=DI_SOUTHEAST;
              tdir != (DI_EAST-1);
              tdir-- )
        {
            if (tdir!=turnaround)
            {
                player->mo->movedir = tdir;

                if (TryWalk(cmd))
                    return;
            }
        }
    }

    if (turnaround !=  DI_NODIR)
    {
        player->mo->movedir = turnaround;
        if (TryWalk(cmd))
            return;
    }

    player->mo->movedir = DI_NODIR;  // can not move
}


//
// B_CleanAhead
// Check if a place is ok to move towards.
// This is also a traverse function for
// bots pre-rocket fire (preventing suicide)
//
bool FCajunMaster::CleanAhead (AActor *thing, double x, double y, ticcmd_t *cmd)
{
	FCheckPosition tm;

    if (!SafeCheckPosition (thing, x, y, tm))
        return false;           // solid wall or thing

    if (!(thing->flags & MF_NOCLIP) )
    {
        if (tm.ceilingz - tm.floorz < thing->Height)
            return false;       // doesn't fit

		double maxmove = MAXMOVEHEIGHT;
		if (!(thing->flags&MF_MISSILE))
		{
			if(tm.floorz > (thing->Sector->floorplane.ZatPoint(x, y)+maxmove)) //Too high wall
				return false;

			//Jumpable
			if(tm.floorz > (thing->Sector->floorplane.ZatPoint(x, y)+thing->MaxStepHeight))
				cmd->ucmd.buttons |= BT_JUMP;


	        if ( !(thing->flags & MF_TELEPORT) &&
	             tm.ceilingz < thing->Top())
	            return false;       // mobj must lower itself to fit

	        // jump out of water
//	        if((thing->eflags & (MF_UNDERWATER|MF_TOUCHWATER))==(MF_UNDERWATER|MF_TOUCHWATER))
//	            maxstep=37;

	        if ( !(thing->flags & MF_TELEPORT) &&
	             (tm.floorz - thing->Z() > thing->MaxStepHeight) )
	            return false;       // too big a step up


			if ( !(thing->flags&(MF_DROPOFF|MF_FLOAT))
			&& tm.floorz - tm.dropoffz > thing->MaxDropOffHeight )
				return false;       // don't stand over a dropoff

		}
    }
    return true;
}

#define OKAYRANGE (5) //counts *2, when angle is in range, turning is not executed.
#define MAXTURN (15) //Max degrees turned in one tic. Lower is smother but may cause the bot not getting where it should = crash
#define TURNSENS 3 //Higher is smoother but slower turn.

void DBot::TurnToAng ()
{
    double maxturn = MAXTURN;

	if (player->ReadyWeapon != NULL)
	{
		if (player->ReadyWeapon->WeaponFlags & WIF_BOT_EXPLOSIVE)
		{
			if (t_roam && !missile)
			{ //Keep angle that where when shot where decided.
				return;
			}
		}


		if(enemy)
			if(!dest) //happens when running after item in combat situations, or normal, prevents weak turns
				if(player->ReadyWeapon->ProjectileType == NULL && !(player->ReadyWeapon->WeaponFlags & WIF_MELEEWEAPON))
					if(Check_LOS(enemy, SHOOTFOV+5))
						maxturn = 3;
	}

	DAngle distance = deltaangle(player->mo->Angles.Yaw, Angle);

	if (fabs (distance) < OKAYRANGE && !enemy)
		return;

	distance /= TURNSENS;
	if (fabs (distance) > maxturn)
		distance = distance < 0 ? -maxturn : maxturn;

	player->mo->Angles.Yaw += distance;
}

void DBot::Pitch (AActor *target)
{
	double aim;
	double diff;

	diff = target->Z() - player->mo->Z();
	aim = g_atan(diff / player->mo->Distance2D(target));
	player->mo->Angles.Pitch = DAngle::ToDegrees(aim);
}

//Checks if a sector is dangerous.
bool FCajunMaster::IsDangerous (sector_t *sec)
{
	return sec->damageamount > 0;
}
