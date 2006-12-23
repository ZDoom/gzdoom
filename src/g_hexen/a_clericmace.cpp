#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"

extern void AdjustPlayerAngle (AActor *pmo);

static FRandom pr_atk ("CMaceAttack");

void A_CMaceAttack (AActor *actor);

// The Cleric's Mace --------------------------------------------------------

class ACWeapMace : public AClericWeapon
{
	DECLARE_ACTOR (ACWeapMace, AClericWeapon)
};

FState ACWeapMace::States[] =
{
#define S_CMACEREADY 0
	S_NORMAL (CMCE, 'A',	1, A_WeaponReady		    , &States[S_CMACEREADY]),

#define S_CMACEDOWN (S_CMACEREADY+1)
	S_NORMAL (CMCE, 'A',	1, A_Lower				    , &States[S_CMACEDOWN]),

#define S_CMACEUP (S_CMACEDOWN+1)
	S_NORMAL (CMCE, 'A',	1, A_Raise				    , &States[S_CMACEUP]),

#define S_CMACEATK (S_CMACEUP+1)
	S_NORMAL2 (CMCE, 'B',	2, NULL					    , &States[S_CMACEATK+1], 60, 20),
	S_NORMAL2 (CMCE, 'B',	1, NULL					    , &States[S_CMACEATK+2], 30, 33),
	S_NORMAL2 (CMCE, 'B',	2, NULL					    , &States[S_CMACEATK+3], 8, 45),
	S_NORMAL2 (CMCE, 'C',	1, NULL					    , &States[S_CMACEATK+4], 8, 45),
	S_NORMAL2 (CMCE, 'D',	1, NULL					    , &States[S_CMACEATK+5], 8, 45),
	S_NORMAL2 (CMCE, 'E',	1, NULL					    , &States[S_CMACEATK+6], 8, 45),
	S_NORMAL2 (CMCE, 'E',	1, A_CMaceAttack		    , &States[S_CMACEATK+7], -11, 58),
	S_NORMAL2 (CMCE, 'F',	1, NULL					    , &States[S_CMACEATK+8], 8, 45),
	S_NORMAL2 (CMCE, 'F',	2, NULL					    , &States[S_CMACEATK+9], -8, 74),
	S_NORMAL2 (CMCE, 'F',	1, NULL					    , &States[S_CMACEATK+10], -20, 96),
	S_NORMAL2 (CMCE, 'F',	8, NULL					    , &States[S_CMACEATK+11], -33, 160),
	S_NORMAL2 (CMCE, 'A',	2, A_ReFire				    , &States[S_CMACEATK+12], 8, 75),
	S_NORMAL2 (CMCE, 'A',	1, NULL					    , &States[S_CMACEATK+13], 8, 65),
	S_NORMAL2 (CMCE, 'A',	2, NULL					    , &States[S_CMACEATK+14], 8, 60),
	S_NORMAL2 (CMCE, 'A',	1, NULL					    , &States[S_CMACEATK+15], 8, 55),
	S_NORMAL2 (CMCE, 'A',	2, NULL					    , &States[S_CMACEATK+16], 8, 50),
	S_NORMAL2 (CMCE, 'A',	1, NULL					    , &States[S_CMACEREADY], 8, 45),
};


IMPLEMENT_ACTOR (ACWeapMace, Hexen, -1, 0)
	PROP_Weapon_SelectionOrder (3500)
	PROP_Flags5 (MF5_BLOODSPLATTER)
	PROP_Weapon_Flags (WIF_BOT_MELEE)
	PROP_Weapon_UpState (S_CMACEUP)
	PROP_Weapon_DownState (S_CMACEDOWN)
	PROP_Weapon_ReadyState (S_CMACEREADY)
	PROP_Weapon_AtkState (S_CMACEATK)
	PROP_Weapon_Kickback (150)
	PROP_Weapon_YAdjust (0-8)
END_DEFAULTS

//===========================================================================
//
// A_CMaceAttack
//
//===========================================================================

void A_CMaceAttack (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
	int i;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	damage = 25+(pr_atk()&15);
	for (i = 0; i < 16; i++)
	{
		angle = player->mo->angle+i*(ANG45/16);
		slope = P_AimLineAttack (player->mo, angle, 2*MELEERANGE);
		if (linetarget)
		{
			P_LineAttack (player->mo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, RUNTIME_CLASS(AHammerPuff));
			AdjustPlayerAngle (player->mo);
//			player->mo->angle = R_PointToAngle2(player->mo->x,
//				player->mo->y, linetarget->x, linetarget->y);
			goto macedone;
		}
		angle = player->mo->angle-i*(ANG45/16);
		slope = P_AimLineAttack (player->mo, angle, 2*MELEERANGE);
		if (linetarget)
		{
			P_LineAttack (player->mo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, RUNTIME_CLASS(AHammerPuff));
			AdjustPlayerAngle (player->mo);
//			player->mo->angle = R_PointToAngle2(player->mo->x,
//				player->mo->y, linetarget->x, linetarget->y);
			goto macedone;
		}
	}
	// didn't find any creatures, so try to strike any walls
	player->mo->special1 = 0;

	angle = player->mo->angle;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, RUNTIME_CLASS(AHammerPuff));
macedone:
	return;		
}
