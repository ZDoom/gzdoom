/********************************
* B_Think.c                     *
* Description:                  *
* Movement/Roaming code for     *
* the bot's					    *
*********************************/

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "p_inter.h"
#include "b_bot.h"
#include "g_game.h"
#include "d_ticcmd.h"
#include "m_random.h"
#include "r_main.h"
#include "i_system.h"
#include "p_lnspec.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

enum dirtype_t
{
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
};

// borrow some tables from p_enemy.cpp
extern dirtype_t opposite[9];
extern dirtype_t diags[4];
extern fixed_t xspeed[8];
extern fixed_t yspeed[8];

extern line_t **spechit;
extern int numspechit;


//Called while the bot moves after it's player->dest mobj
//which can be a weapon/enemy/item whatever.
void DCajunMaster::Roam (AActor *actor, ticcmd_t *cmd)
{
	int delta;

	if (Reachable(actor, actor->player->dest)) //Straight towards it.
		actor->player->angle = R_PointToAngle2(actor->x, actor->y, actor->player->dest->x, actor->player->dest->y);
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

BOOL DCajunMaster::Move (AActor *actor, ticcmd_t *cmd)
{
	fixed_t tryx, tryy;
	bool try_ok;
	BOOL good;

	if (actor->movedir == DI_NODIR)
		return false;

	if ((unsigned)actor->movedir >= 8)
		I_Error ("Weird bot movedir!");

	tryx = actor->x + 8*xspeed[actor->movedir];
	tryy = actor->y + 8*yspeed[actor->movedir];

	try_ok = CleanAhead (actor, tryx, tryy, cmd);

	if (!try_ok) //Anything blocking that could be opened etc..
	{
		if (!numspechit)
			return false;

		actor->movedir = DI_NODIR;

		for (good = 0; numspechit > 0; )
		{
			line_t *ld = spechit[--numspechit];
			bool tryit = true;

			if (ld->special == Door_LockedRaise && !P_CheckKeys (actor->player, (keytype_t)ld->args[3], false))
				tryit = false;
			else if (ld->special == Generic_Door && !P_CheckKeys (actor->player, (keytype_t)ld->args[4], false))
				tryit = false;

			if (tryit &&
				(P_TestActivateLine (ld, actor, 0, SPAC_USE) ||
				 P_TestActivateLine (ld, actor, 0, SPAC_PUSH)))
			{
				good |= ld == BlockingLine ? 1 : 2;
			}
		}
		if (good && ((P_Random (pr_botopendoor) >= 203) ^ (good & 1)))
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

bool DCajunMaster::TryWalk (AActor *actor, ticcmd_t *cmd)
{
    if (!Move (actor, cmd))
        return false;

    actor->movecount = P_Random(pr_bottrywalk) & 60;
    return true;
}

void DCajunMaster::NewChaseDir (AActor *actor, ticcmd_t *cmd)
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
        Printf (PRINT_HIGH, "Bot tried move without destination\n");
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
    if (P_Random(pr_botnewchasedir) > 200
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
    if (P_Random(pr_botnewchasedir)&1)
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
bool DCajunMaster::CleanAhead (AActor *thing, fixed_t x, fixed_t y, ticcmd_t *cmd)
{
    if (!SafeCheckPosition (thing, x, y))
        return false;           // solid wall or thing

    if ( !(thing->flags & MF_NOCLIP) )
    {
        fixed_t maxstep = 24*FRACUNIT /*MAXSTEPMOVE*/;
        if (tmceilingz - tmfloorz < thing->height)
            return false;       // doesn't fit

		if(!(thing->flags&MF_MISSILE))
		{
			if(tmfloorz>(thing->subsector->sector->floorheight+MAXMOVEHEIGHT)) //Too high wall
				return false;

			//Jumpable
			if(tmfloorz>(thing->subsector->sector->floorheight+24*FRACUNIT /*MAXSTEPMOVE*/))
				cmd->ucmd.buttons |= BT_JUMP;


	        if ( !(thing->flags & MF_TELEPORT) &&
	             tmceilingz - thing->z < thing->height)
	            return false;       // mobj must lower itself to fit

	        // jump out of water
//	        if((thing->eflags & (MF_UNDERWATER|MF_TOUCHWATER))==(MF_UNDERWATER|MF_TOUCHWATER))
//	            maxstep=37*FRACUNIT;

	        if ( !(thing->flags & MF_TELEPORT) &&
	             (tmfloorz - thing->z > maxstep ) )
	            return false;       // too big a step up


			if ( !(thing->flags&(MF_DROPOFF|MF_FLOAT))
			&& tmfloorz - tmdropoffz > 24*FRACUNIT /*MAXSTEPMOVE*/ )
				return false;       // don't stand over a dropoff

		}
    }
    return true;
}

#define OKAYRANGE (5*ANGLE_1) //counts *2, when angle is in range, turning is not executed.
#define MAXTURN (15*ANGLE_1) //Max degrees turned in one tic. Lower is smother but may cause the bot not getting where it should = crash
#define TURNSENS 3 //Higher is smoother but slower turn.

void DCajunMaster::TurnToAng (AActor *actor)
{
	if (actor->player->readyweapon == wp_missile && actor->player->t_rocket && !actor->player->missile)
		return; //Keep angle that where when shot where decided.

    int maxturn = MAXTURN;

	if(actor->player->enemy)
	if(!actor->player->dest) //happens when running after item in combat situations, or normal, prevent's weak turns
	if(actor->player->readyweapon != wp_missile && actor->player->readyweapon != wp_bfg && actor->player->readyweapon != wp_plasma && actor->player->readyweapon != wp_fist && actor->player->readyweapon != wp_chainsaw)
	if(Check_LOS(actor, actor->player->enemy, SHOOTFOV+5*ANGLE_1))
		maxturn = 3;

	int distance = actor->player->angle - actor->angle;

	if (abs (distance) < OKAYRANGE && !actor->player->enemy)
		return;

	distance /= TURNSENS;
	if (abs (distance) > maxturn)
		distance = distance < 0 ? -maxturn : maxturn;

	actor->angle += distance;
}

void DCajunMaster::Pitch (AActor *actor, AActor *target)
{
	double aim;
	double diff;

	diff = target->z - actor->z;
	aim = atan (diff / (double)P_AproxDistance (actor->x - target->x, actor->y - target->y));
	actor->pitch = -(int)(aim * ANGLE_180/M_PI);
}

//Checks if a sector is dangerous.
bool DCajunMaster::IsDangerous (sector_t *sec)
{
	int special;

	return
		   sec->damage
		|| sec->special & DAMAGE_MASK
		|| (special = sec->special & 0xff, special == dLight_Strobe_Hurt)
		|| special == dDamage_Hellslime
		|| special == dDamage_Nukage
		|| special == dDamage_End
		|| special == dDamage_SuperHellslime;
}

