#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_action.h"
#include "a_hexenglobal.h"
#include "gi.h"

#define HEAL_RADIUS_DIST	255*FRACUNIT

static FRandom pr_healradius ("HealRadius");

// Healing Radius Artifact --------------------------------------------------

class AArtiHealingRadius : public AInventory
{
	DECLARE_ACTOR (AArtiHealingRadius, AInventory)
public:
	bool Use (bool pickup);
};

FState AArtiHealingRadius::States[] =
{
	S_BRIGHT (HRAD, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (HRAD, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (HRAD, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (HRAD, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (HRAD, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (HRAD, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (HRAD, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (HRAD, 'H',	4, NULL					    , &States[8]),
	S_BRIGHT (HRAD, 'I',	4, NULL					    , &States[9]),
	S_BRIGHT (HRAD, 'J',	4, NULL					    , &States[10]),
	S_BRIGHT (HRAD, 'K',	4, NULL					    , &States[11]),
	S_BRIGHT (HRAD, 'L',	4, NULL					    , &States[12]),
	S_BRIGHT (HRAD, 'M',	4, NULL					    , &States[13]),
	S_BRIGHT (HRAD, 'N',	4, NULL					    , &States[14]),
	S_BRIGHT (HRAD, 'O',	4, NULL					    , &States[15]),
	S_BRIGHT (HRAD, 'P',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AArtiHealingRadius, Hexen, 10120, 0)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_PICKUPFLASH|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTIHRAD")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTIHEALINGRADIUS")
END_DEFAULTS

bool AArtiHealingRadius::Use (bool pickup)
{
	bool effective = false;
	int mode = Owner->GetClass()->Meta.GetMetaInt(APMETA_HealingRadius);

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] &&
			players[i].mo != NULL &&
			players[i].mo->health > 0 &&
			P_AproxDistance (players[i].mo->x - Owner->x, players[i].mo->y - Owner->y) <= HEAL_RADIUS_DIST)
		{
			// Q: Is it worth it to make this selectable as a player property?
			// A: Probably not - but it sure doesn't hurt.
			bool gotsome=false;
			switch (mode)
			{
			case NAME_Armor:
				for (int j = 0; j < 4; ++j)
				{
					AHexenArmor *armor = Spawn<AHexenArmor> (0,0,0, NO_REPLACE);
					armor->health = j;
					armor->Amount = 1;
					if (!armor->TryPickup (players[i].mo))
					{
						armor->Destroy ();
					}
					else
					{
						gotsome = true;
					}
				}
				break;

			case NAME_Mana:
			{
				int amount = 50 + (pr_healradius() % 50);

				if (players[i].mo->GiveAmmo (PClass::FindClass(NAME_Mana1), amount) ||
					players[i].mo->GiveAmmo (PClass::FindClass(NAME_Mana2), amount))
				{
					gotsome = true;
				}
				break;
			}

			default:
			//case NAME_Health:
				gotsome = P_GiveBody (players[i].mo, 50 + (pr_healradius()%50));
				break;
			}
			if (gotsome)
			{
				S_Sound (players[i].mo, CHAN_AUTO, "MysticIncant", 1, ATTN_NORM);
				effective=true;
			}
		}
	}
	return effective;

}
