#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"

// Speed Boots Artifact -----------------------------------------------------

class AArtiSpeedBoots : public APowerupGiver
{
	DECLARE_ACTOR (AArtiSpeedBoots, APowerupGiver)
public:
	const char *PickupMessage ();
};

FState AArtiSpeedBoots::States[] =
{
	S_BRIGHT (SPED, 'A',	3, NULL					    , &States[1]),
	S_BRIGHT (SPED, 'B',	3, NULL					    , &States[2]),
	S_BRIGHT (SPED, 'C',	3, NULL					    , &States[3]),
	S_BRIGHT (SPED, 'D',	3, NULL					    , &States[4]),
	S_BRIGHT (SPED, 'E',	3, NULL					    , &States[5]),
	S_BRIGHT (SPED, 'F',	3, NULL					    , &States[6]),
	S_BRIGHT (SPED, 'G',	3, NULL					    , &States[7]),
	S_BRIGHT (SPED, 'H',	3, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AArtiSpeedBoots, Hexen, 8002, 13)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
	PROP_Inventory_FlagsSet (IF_PICKUPFLASH)
	PROP_Inventory_Icon ("ARTISPED")
	PROP_PowerupGiver_Powerup ("PowerSpeed")
END_DEFAULTS

const char *AArtiSpeedBoots::PickupMessage ()
{
	return GStrings("TXT_ARTISPEED");
}
