/*
#include "m_random.h"
#include "p_local.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_maceatk ("CMaceAttack");

//===========================================================================
//
// A_CMaceAttack
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CMaceAttack)
{
	PARAM_ACTION_PROLOGUE;

	DAngle angle;
	int damage;
	DAngle slope;
	int i;
	player_t *player;
	FTranslatedLineTarget t;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	PClassActor *hammertime = PClass::FindActor("HammerPuff");

	damage = 25+(pr_maceatk()&15);
	for (i = 0; i < 16; i++)
	{
		for (int j = 1; j >= -1; j -= 2)
		{
			angle = player->mo->Angles.Yaw + j*i*(45. / 16);
			slope = P_AimLineAttack(player->mo, angle, 2 * MELEERANGE, &t);
			if (t.linetarget)
			{
				P_LineAttack(player->mo, angle, 2 * MELEERANGE, slope, damage, NAME_Melee, hammertime, true, &t);
				if (t.linetarget != NULL)
				{
					AdjustPlayerAngle(player->mo, &t);
					return 0;
				}
			}
		}
	}
	// didn't find any creatures, so try to strike any walls
	player->mo->weaponspecial = 0;

	angle = player->mo->Angles.Yaw;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, hammertime);
	return 0;		
}
