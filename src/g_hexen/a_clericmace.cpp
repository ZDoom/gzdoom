/*
#include "m_random.h"
#include "p_local.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

extern void AdjustPlayerAngle (AActor *pmo, AActor *linetarget);

static FRandom pr_maceatk ("CMaceAttack");

//===========================================================================
//
// A_CMaceAttack
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CMaceAttack)
{
	angle_t angle;
	int damage;
	int slope;
	int i;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = self->player))
	{
		return;
	}

	damage = 25+(pr_maceatk()&15);
	for (i = 0; i < 16; i++)
	{
		angle = player->mo->angle+i*(ANG45/16);
		slope = P_AimLineAttack (player->mo, angle, 2*MELEERANGE, &linetarget);
		if (linetarget)
		{
			P_LineAttack (player->mo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true, &linetarget);
			if (linetarget != NULL)
			{
				AdjustPlayerAngle (player->mo, linetarget);
				goto macedone;
			}
		}
		angle = player->mo->angle-i*(ANG45/16);
		slope = P_AimLineAttack (player->mo, angle, 2*MELEERANGE, &linetarget);
		if (linetarget)
		{
			P_LineAttack (player->mo, angle, 2*MELEERANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"), true, &linetarget);
			if (linetarget != NULL)
			{
				AdjustPlayerAngle (player->mo, linetarget);
				goto macedone;
			}
		}
	}
	// didn't find any creatures, so try to strike any walls
	player->mo->weaponspecial = 0;

	angle = player->mo->angle;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE, &linetarget);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, PClass::FindClass ("HammerPuff"));
macedone:
	return;		
}
