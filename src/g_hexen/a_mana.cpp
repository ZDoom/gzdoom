#include "a_pickups.h"
#include "p_local.h"
#include "gstrings.h"

// Blue mana ----------------------------------------------------------------

class AMana1 : public AAmmo
{
	DECLARE_ACTOR (AMana1, AAmmo)
public:
	bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, MANA_1, 15);
	}
protected:
	const char *PickupMessage ()
	{
		return GStrings(TXT_MANA_1);
	}
};

FState AMana1::States[] =
{
	S_BRIGHT (MAN1, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (MAN1, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (MAN1, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (MAN1, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (MAN1, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (MAN1, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (MAN1, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (MAN1, 'H',	4, NULL					    , &States[8]),
	S_BRIGHT (MAN1, 'I',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AMana1, Hexen, 122, 11)
	PROP_SpawnHealth (10)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)

	PROP_SpawnState (0)
END_DEFAULTS

// Green mana ---------------------------------------------------------------

class AMana2 : public AAmmo
{
	DECLARE_ACTOR (AMana2, AAmmo)
public:
	bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, MANA_2, 15);
	}
protected:
	const char *PickupMessage ()
	{
		return GStrings(TXT_MANA_2);
	}
};

FState AMana2::States[] =
{
	S_BRIGHT (MAN2, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (MAN2, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (MAN2, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (MAN2, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (MAN2, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (MAN2, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (MAN2, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (MAN2, 'H',	4, NULL					    , &States[8]),
	S_BRIGHT (MAN2, 'I',	4, NULL					    , &States[9]),
	S_BRIGHT (MAN2, 'J',	4, NULL					    , &States[10]),
	S_BRIGHT (MAN2, 'K',	4, NULL					    , &States[11]),
	S_BRIGHT (MAN2, 'L',	4, NULL					    , &States[12]),
	S_BRIGHT (MAN2, 'M',	4, NULL					    , &States[13]),
	S_BRIGHT (MAN2, 'N',	4, NULL					    , &States[14]),
	S_BRIGHT (MAN2, 'O',	4, NULL					    , &States[15]),
	S_BRIGHT (MAN2, 'P',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AMana2, Hexen, 124, 12)
	PROP_SpawnHealth (10)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)

	PROP_SpawnState (0)
END_DEFAULTS

// Combined mana ------------------------------------------------------------

class AMana3 : public AAmmo
{
	DECLARE_ACTOR (AMana3, AAmmo)
public:
	bool TryPickup (AActor *toucher)
	{
		bool gotit;

		gotit = P_GiveAmmo (toucher->player, MANA_1, 20);
		gotit |= P_GiveAmmo (toucher->player, MANA_2, 20);

		return gotit;
	}
protected:
	const char *PickupMessage ()
	{
		return GStrings(TXT_MANA_BOTH);
	}
};

FState AMana3::States[] =
{
	S_BRIGHT (MAN3, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (MAN3, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (MAN3, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (MAN3, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (MAN3, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (MAN3, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (MAN3, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (MAN3, 'H',	4, NULL					    , &States[8]),
	S_BRIGHT (MAN3, 'I',	4, NULL					    , &States[9]),
	S_BRIGHT (MAN3, 'J',	4, NULL					    , &States[10]),
	S_BRIGHT (MAN3, 'K',	4, NULL					    , &States[11]),
	S_BRIGHT (MAN3, 'L',	4, NULL					    , &States[12]),
	S_BRIGHT (MAN3, 'M',	4, NULL					    , &States[13]),
	S_BRIGHT (MAN3, 'N',	4, NULL					    , &States[14]),
	S_BRIGHT (MAN3, 'O',	4, NULL					    , &States[15]),
	S_BRIGHT (MAN3, 'P',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AMana3, Hexen, 8004, 75)
	PROP_SpawnHealth (20)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)

	PROP_SpawnState (0)
END_DEFAULTS
