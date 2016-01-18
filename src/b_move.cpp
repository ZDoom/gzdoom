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
#include "d_ticcmd.h"
#include "m_random.h"
#include "i_system.h"
#include "p_lnspec.h"
#include "gi.h"
#include "a_keys.h"
#include "d_event.h"
#include "p_enemy.h"

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
	int delta;

	if (Reachable(dest))
	{ // Straight towards it.
		angle = player->mo->AngleTo(dest);
	}
	else if (player->mo->movedir < 8) // turn towards movement direction if not there yet
	{
		angle &= (angle_t)(7<<29);
		delta = angle - (player->mo->movedir << 29);

		if (delta > 0)
			angle -= ANG45;
		else if (delta < 0)
			angle += ANG45;
	}

	// chase towards destination.
	if (--player->mo->movecount < 0 || !Move (cmd))
	{
		NewChaseDir (cmd);
	}
}

bool DBot::Move (ticcmd_t *cmd)
{
	fixed_t tryx, tryy;
	bool try_ok;
	int good;

	if (player->mo->movedir == DI_NODIR)
		return false;

	if ((unsigned)player->mo->movedir >= 8)
		I_Error ("Weird bot movedir!");

	tryx = player->mo->X() + 8*xspeed[player->mo->movedir];
	tryy = player->mo->Y() + 8*yspeed[player->mo->movedir];

	try_ok = bglobal.CleanAhead (player->mo, tryx, tryy, cmd);

	if (!try_ok) //Anything blocking that could be opened etc..
	{
		if (!spechit.Size ())
			return false;

		player->mo->movedir = DI_NODIR;

		good = 0;
		line_t *ld;

		while (spechit.Pop (ld))
		{
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

	fixedvec2 delta = player->mo->Vec2To(dest);

    if (delta.x > 10*FRACUNIT)
        d[1] = DI_EAST;
    else if (delta.x < -10*FRACUNIT)
        d[1] = DI_WEST;
    else
        d[1] = DI_NODIR;

    if (delta.y < -10*FRACUNIT)
        d[2] = DI_SOUTH;
    else if (delta.y > 10*FRACUNIT)
        d[2] = DI_NORTH;
    else
        d[2] = DI_NODIR;

    // try direct route
    if (d[1] != DI_NODIR && d[2] != DI_NODIR)
    {
        player->mo->movedir = diags[((delta.y<0)<<1)+(delta.x>0)];
        if (player->mo->movedir != turnaround && TryWalk(cmd))
            return;
    }

    // try other directions
    if (pr_botnewchasedir() > 200
        || abs(delta.y)>abs(delta.x))
    {
        tdir=d[1];
        d[1]=d[2];
        d[2]=(dirtype_t)tdir;
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
bool FCajunMaster::CleanAhead (AActor *thing, fixed_t x, fixed_t y, ticcmd_t *cmd)
{
	FCheckPosition tm;

    if (!SafeCheckPosition (thing, x, y, tm))
        return false;           // solid wall or thing

    if (!(thing->flags & MF_NOCLIP) )
    {
		fixed_t maxstep = thing->MaxStepHeight;
        if (tm.ceilingz - tm.floorz < thing->height)
            return false;       // doesn't fit

		if (!(thing->flags&MF_MISSILE))
		{
			if(tm.floorz > (thing->Sector->floorplane.ZatPoint (x, y)+MAXMOVEHEIGHT)) //Too high wall
				return false;

			//Jumpable
			if(tm.floorz>(thing->Sector->floorplane.ZatPoint (x, y)+thing->MaxStepHeight))
				cmd->ucmd.buttons |= BT_JUMP;


	        if ( !(thing->flags & MF_TELEPORT) &&
	             tm.ceilingz - thing->Z() < thing->height)
	            return false;       // mobj must lower itself to fit

	        // jump out of water
//	        if((thing->eflags & (MF_UNDERWATER|MF_TOUCHWATER))==(MF_UNDERWATER|MF_TOUCHWATER))
//	            maxstep=37*FRACUNIT;

	        if ( !(thing->flags & MF_TELEPORT) &&
	             (tm.floorz - thing->Z() > maxstep ) )
	            return false;       // too big a step up


			if ( !(thing->flags&(MF_DROPOFF|MF_FLOAT))
			&& tm.floorz - tm.dropoffz > thing->MaxDropOffHeight )
				return false;       // don't stand over a dropoff

		}
    }
    return true;
}

#define OKAYRANGE (5*ANGLE_1) //counts *2, when angle is in range, turning is not executed.
#define MAXTURN (15*ANGLE_1) //Max degrees turned in one tic. Lower is smother but may cause the bot not getting where it should = crash
#define TURNSENS 3 //Higher is smoother but slower turn.

void DBot::TurnToAng ()
{
    int maxturn = MAXTURN;

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
					if(Check_LOS(enemy, SHOOTFOV+5*ANGLE_1))
						maxturn = 3;
	}

	int distance = angle - player->mo->angle;

	if (abs (distance) < OKAYRANGE && !enemy)
		return;

	distance /= TURNSENS;
	if (abs (distance) > maxturn)
		distance = distance < 0 ? -maxturn : maxturn;

	player->mo->angle += distance;
}

void DBot::Pitch (AActor *target)
{
	double aim;
	double diff;

	diff = target->Z() - player->mo->Z();
	aim = atan(diff / (double)player->mo->AproxDistance(target));
	player->mo->pitch = -(int)(aim * ANGLE_180/M_PI);
}

//Checks if a sector is dangerous.
bool FCajunMaster::IsDangerous (sector_t *sec)
{
	return sec->damageamount > 0;
}
