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

//Called while the bot moves after its player->dest mobj
//which can be a weapon/enemy/item whatever.
void FCajunMaster::Roam (AActor *actor, ticcmd_t *cmd)
{
	int delta;

	if (Reachable(actor, actor->player->dest))
	{ // Straight towards it.
		actor->player->angle = R_PointToAngle2(actor->x, actor->y, actor->player->dest->x, actor->player->dest->y);
	}
	else if (actor->movedir < 8) // turn towards movement direction if not there yet
	{
		actor->player->angle &= (angle_t)(7<<29);
		delta = actor->player->angle - (actor->movedir << 29);

		if (delta > 0)
			actor->player->angle -= ANG45;
		else if (delta < 0)
			actor->player->angle += ANG45;
	}

	// chase towards destination.
	if (--actor->movecount < 0 || !Move (actor, cmd))
	{
		NewChaseDir (actor, cmd);
	}
}

bool FCajunMaster::Move (AActor *actor, ticcmd_t *cmd)
{
	fixed_t tryx, tryy;
	bool try_ok;
	int good;

	if (actor->movedir == DI_NODIR)
		return false;

	if ((unsigned)actor->movedir >= 8)
		I_Error ("Weird bot movedir!");

	tryx = actor->x + 8*xspeed[actor->movedir];
	tryy = actor->y + 8*yspeed[actor->movedir];

	try_ok = CleanAhead (actor, tryx, tryy, cmd);

	if (!try_ok) //Anything blocking that could be opened etc..
	{
		if (!spechit.Size ())
			return false;

		actor->movedir = DI_NODIR;

		good = 0;
		line_t *ld;

		while (spechit.Pop (ld))
		{
			bool tryit = true;

			if (ld->special == Door_LockedRaise && !P_CheckKeys (actor, ld->args[3], false))
				tryit = false;
			else if (ld->special == Generic_Door && !P_CheckKeys (actor, ld->args[4], false))
				tryit = false;

			if (tryit &&
				(P_TestActivateLine (ld, actor, 0, SPAC_Use) ||
				 P_TestActivateLine (ld, actor, 0, SPAC_Push)))
			{
				good |= ld == actor->BlockingLine ? 1 : 2;
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

bool FCajunMaster::TryWalk (AActor *actor, ticcmd_t *cmd)
{
    if (!Move (actor, cmd))
        return false;

    actor->movecount = pr_bottrywalk() & 60;
    return true;
}

void FCajunMaster::NewChaseDir (AActor *actor, ticcmd_t *cmd)
{
    fixed_t     deltax;
    fixed_t     deltay;

    dirtype_t   d[3];

    int         tdir;
    dirtype_t   olddir;

    dirtype_t   turnaround;

    if (!actor->player->dest)
	{
#ifndef BOT_RELEASE_COMPILE
        Printf ("Bot tried move without destination\n");
#endif
		return;
	}

    olddir = (dirtype_t)actor->movedir;
    turnaround = opposite[olddir];

    deltax = actor->player->dest->x - actor->x;
    deltay = actor->player->dest->y - actor->y;

    if (deltax > 10*FRACUNIT)
        d[1] = DI_EAST;
    else if (deltax < -10*FRACUNIT)
        d[1] = DI_WEST;
    else
        d[1] = DI_NODIR;

    if (deltay < -10*FRACUNIT)
        d[2] = DI_SOUTH;
    else if (deltay > 10*FRACUNIT)
        d[2] = DI_NORTH;
    else
        d[2] = DI_NODIR;

    // try direct route
    if (d[1] != DI_NODIR && d[2] != DI_NODIR)
    {
        actor->movedir = diags[((deltay<0)<<1)+(deltax>0)];
        if (actor->movedir != turnaround && TryWalk(actor, cmd))
            return;
    }

    // try other directions
    if (pr_botnewchasedir() > 200
        || abs(deltay)>abs(deltax))
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
        actor->movedir = d[1];
        if (TryWalk (actor, cmd))
            return;
    }

    if (d[2]!=DI_NODIR)
    {
        actor->movedir = d[2];

        if (TryWalk(actor, cmd))
            return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir!=DI_NODIR)
    {
        actor->movedir = olddir;

        if (TryWalk(actor, cmd))
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
                actor->movedir = tdir;

                if (TryWalk(actor, cmd))
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
                actor->movedir = tdir;

                if (TryWalk(actor, cmd))
                    return;
            }
        }
    }

    if (turnaround !=  DI_NODIR)
    {
        actor->movedir = turnaround;
        if (TryWalk(actor, cmd))
            return;
    }

    actor->movedir = DI_NODIR;  // can not move
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
	             tm.ceilingz - thing->z < thing->height)
	            return false;       // mobj must lower itself to fit

	        // jump out of water
//	        if((thing->eflags & (MF_UNDERWATER|MF_TOUCHWATER))==(MF_UNDERWATER|MF_TOUCHWATER))
//	            maxstep=37*FRACUNIT;

	        if ( !(thing->flags & MF_TELEPORT) &&
	             (tm.floorz - thing->z > maxstep ) )
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

void FCajunMaster::TurnToAng (AActor *actor)
{
    int maxturn = MAXTURN;

	if (actor->player->ReadyWeapon != NULL)
	{
		if (actor->player->ReadyWeapon->WeaponFlags & WIF_BOT_EXPLOSIVE)
		{
			if (actor->player->t_roam && !actor->player->missile)
			{ //Keep angle that where when shot where decided.
				return;
			}
		}


		if(actor->player->enemy)
			if(!actor->player->dest) //happens when running after item in combat situations, or normal, prevents weak turns
				if(actor->player->ReadyWeapon->ProjectileType == NULL && !(actor->player->ReadyWeapon->WeaponFlags & WIF_MELEEWEAPON))
					if(Check_LOS(actor, actor->player->enemy, SHOOTFOV+5*ANGLE_1))
						maxturn = 3;
	}

	int distance = actor->player->angle - actor->angle;

	if (abs (distance) < OKAYRANGE && !actor->player->enemy)
		return;

	distance /= TURNSENS;
	if (abs (distance) > maxturn)
		distance = distance < 0 ? -maxturn : maxturn;

	actor->angle += distance;
}

void FCajunMaster::Pitch (AActor *actor, AActor *target)
{
	double aim;
	double diff;

	diff = target->z - actor->z;
	aim = atan (diff / (double)P_AproxDistance (actor->x - target->x, actor->y - target->y));
	actor->pitch = -(int)(aim * ANGLE_180/M_PI);
}

//Checks if a sector is dangerous.
bool FCajunMaster::IsDangerous (sector_t *sec)
{
	int special;

	return
		   sec->damage
		|| sec->special & DAMAGE_MASK
		|| (special = sec->special & 0xff, special == dLight_Strobe_Hurt)
		|| special == dDamage_Hellslime
		|| special == dDamage_Nukage
		|| special == dDamage_End
		|| special == dDamage_SuperHellslime
		|| special == dDamage_LavaWimpy
		|| special == dDamage_LavaHefty
		|| special == dScroll_EastLavaDamage
		|| special == sLight_Strobe_Hurt
		|| special == Damage_InstantDeath
		|| special == sDamage_SuperHellslime;
}

