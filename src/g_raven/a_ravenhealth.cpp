#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"

// Crystal vial -------------------------------------------------------------

class ACrystalVial : public AHealth
{
	DECLARE_ACTOR (ACrystalVial, AHealth)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveBody (toucher->player, 10);
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(TXT_ITEMHEALTH);
	}
};

FState ACrystalVial::States[] =
{
	S_NORMAL (PTN1, 'A',	3, NULL 				, &States[1]),
	S_NORMAL (PTN1, 'B',	3, NULL 				, &States[2]),
	S_NORMAL (PTN1, 'C',	3, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (ACrystalVial, Raven, 81, 23)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS
