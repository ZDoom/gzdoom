#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "c_console.h"

#define PUZZLE(name,type,msg,pic) \
	class APuzz##name : public APuzzleItem { \
		DECLARE_ACTOR (APuzz##name, APuzzleItem) public: \
		AT_GAME_SET_FRIEND (name) \
		bool TryPickup (AActor *toucher) { \
			return P_GiveArtifact (toucher->player, type); } protected: \
	const char *PickupMessage () { return GStrings(msg); } }; \
	AT_GAME_SET (name) { \
		ArtiDispatch[type] = APuzzleItem::ActivateArti; \
		ArtiPics[type] = #pic; }

class APuzzleItem : public AArtifact
{
	DECLARE_STATELESS_ACTOR (APuzzleItem, AArtifact)
public:
	static bool ActivateArti (player_t *player, artitype_t arti);

	void PlayPickupSound (AActor *toucher);
	void SetDormant ();
	bool ShouldStay ();
};

IMPLEMENT_STATELESS_ACTOR (APuzzleItem, Any, -1, 0)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
END_DEFAULTS

bool APuzzleItem::ActivateArti (player_t *player, artitype_t arti)

{
	if (P_UsePuzzleItem (player, arti - arti_firstpuzzitem))
	{
		return true;
	}
	// [RH] Always play the sound if the use fails.
	S_Sound (player->mo, CHAN_VOICE, "*puzzfail", 1, ATTN_IDLE);
	C_MidPrintBold (GStrings(TXT_USEPUZZLEFAILED));
	return false;
}

void APuzzleItem::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, "misc/i_pkup", 1, ATTN_NORM);
}

void APuzzleItem::SetDormant ()
{
	if (ShouldRespawn())
	{
		Hide ();
	}
	else
	{
		Destroy ();
	}
}

bool APuzzleItem::ShouldStay ()
{
	return !!multiplayer;
}

// Yorick's Skull -----------------------------------------------------------

PUZZLE(Skull, arti_puzzskull, TXT_ARTIPUZZSKULL, ARTISKLL)

FState APuzzSkull::States[] =
{
	S_NORMAL (ASKU, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzSkull, Hexen, 9002, 76)
	PROP_SpawnState (0)
END_DEFAULTS

// Heart of D'Sparil --------------------------------------------------------

PUZZLE(GemBig, arti_puzzgembig, TXT_ARTIPUZZGEMBIG, ARTIBGEM)

FState APuzzGemBig::States[] =
{
	S_NORMAL (ABGM, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemBig, Hexen, 9003, 77)
	PROP_SpawnState (0)
END_DEFAULTS

// Red Gem (Ruby Planet) ----------------------------------------------------

PUZZLE(GemRed, arti_puzzgemred, TXT_ARTIPUZZGEMRED, ARTIGEMR)

FState APuzzGemRed::States[] =
{
	S_NORMAL (AGMR, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemRed, Hexen, 9004, 78)
	PROP_SpawnState (0)
END_DEFAULTS

// Green Gem 1 (Emerald Planet) ---------------------------------------------

PUZZLE(GemGreen1, arti_puzzgemgreen1, TXT_ARTIPUZZGEMGREEN1, ARTIGEMG)

FState APuzzGemGreen1::States[] =
{
	S_NORMAL (AGMG, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemGreen1, Hexen, 9005, 79)
	PROP_SpawnState (0)
END_DEFAULTS

// Green Gem 2 (Emerald Planet) ---------------------------------------------

PUZZLE(GemGreen2, arti_puzzgemgreen2, TXT_ARTIPUZZGEMGREEN2, ARTIGMG2)

FState APuzzGemGreen2::States[] =
{
	S_NORMAL (AGG2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemGreen2, Hexen, 9009, 80)
	PROP_SpawnState (0)
END_DEFAULTS

// Blue Gem 1 (Sapphire Planet) ---------------------------------------------

PUZZLE(GemBlue1, arti_puzzgemblue1, TXT_ARTIPUZZGEMBLUE1, ARTIGEMB)

FState APuzzGemBlue1::States[] =
{
	S_NORMAL (AGMB, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemBlue1, Hexen, 9006, 81)
	PROP_SpawnState (0)
END_DEFAULTS

// Blue Gem 2 (Sapphire Planet) ---------------------------------------------

PUZZLE(GemBlue2, arti_puzzgemblue2, TXT_ARTIPUZZGEMBLUE2, ARTIGMB2)

FState APuzzGemBlue2::States[] =
{
	S_NORMAL (AGB2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzGemBlue2, Hexen, 9010, 82)
	PROP_SpawnState (0)
END_DEFAULTS

// Book 1 (Daemon Codex) ----------------------------------------------------

PUZZLE(Book1, arti_puzzbook1, TXT_ARTIPUZZBOOK1, ARTIBOK1)

FState APuzzBook1::States[] =
{
	S_NORMAL (ABK1, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzBook1, Hexen, 9007, 83)
	PROP_SpawnState (0)
END_DEFAULTS

// Book 2 (Liber Oscura) ----------------------------------------------------

PUZZLE(Book2, arti_puzzbook2, TXT_ARTIPUZZBOOK2, ARTIBOK2)

FState APuzzBook2::States[] =
{
	S_NORMAL (ABK2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzBook2, Hexen, 9008, 84)
	PROP_SpawnState (0)
END_DEFAULTS

// Flame Mask ---------------------------------------------------------------

PUZZLE(FlameMask, arti_puzzskull2, TXT_ARTIPUZZSKULL2, ARTISKL2)

FState APuzzFlameMask::States[] =
{
	S_NORMAL (ASK2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzFlameMask, Hexen, 9014, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Fighter Weapon (Glaive Seal) ---------------------------------------------

PUZZLE(FWeapon, arti_puzzfweapon, TXT_ARTIPUZZFWEAPON, ARTIFWEP)

FState APuzzFWeapon::States[] =
{
	S_NORMAL (AFWP, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzFWeapon, Hexen, 9015, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Cleric Weapon (Holy Relic) -----------------------------------------------

PUZZLE(CWeapon, arti_puzzcweapon, TXT_ARTIPUZZCWEAPON, ARTICWEP)

FState APuzzCWeapon::States[] =
{
	S_NORMAL (ACWP, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzCWeapon, Hexen, 9016, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Mage Weapon (Sigil of the Magus) -----------------------------------------

PUZZLE(MWeapon, arti_puzzmweapon, TXT_ARTIPUZZMWEAPON, ARTIMWEP)

FState APuzzMWeapon::States[] =
{
	S_NORMAL (AMWP, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (APuzzMWeapon, Hexen, 9017, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Clock Gear 1 -------------------------------------------------------------

PUZZLE(Gear1, arti_puzzgear1, TXT_ARTIPUZZGEAR, ARTIGEAR)

FState APuzzGear1::States[] =
{
	S_BRIGHT (AGER, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (AGER, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (AGER, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (AGER, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (AGER, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (AGER, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (AGER, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (AGER, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (APuzzGear1, Hexen, 9018, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Clock Gear 2 -------------------------------------------------------------

PUZZLE(Gear2, arti_puzzgear2, TXT_ARTIPUZZGEAR, ARTIGER2)

FState APuzzGear2::States[] =
{
	S_BRIGHT (AGR2, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (AGR2, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (AGR2, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (AGR2, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (AGR2, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (AGR2, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (AGR2, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (AGR2, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (APuzzGear2, Hexen, 9019, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Clock Gear 3 -------------------------------------------------------------

PUZZLE(Gear3, arti_puzzgear3, TXT_ARTIPUZZGEAR, ARTIGER3)

FState APuzzGear3::States[] =
{
	S_BRIGHT (AGR3, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (AGR3, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (AGR3, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (AGR3, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (AGR3, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (AGR3, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (AGR3, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (AGR3, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (APuzzGear3, Hexen, 9020, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Clock Gear 4 -------------------------------------------------------------

PUZZLE(Gear4, arti_puzzgear4, TXT_ARTIPUZZGEAR, ARTIGER4)

FState APuzzGear4::States[] =
{
	S_BRIGHT (AGR4, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (AGR4, 'B',	4, NULL					    , &States[2]),
	S_BRIGHT (AGR4, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (AGR4, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (AGR4, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (AGR4, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (AGR4, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (AGR4, 'H',	4, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (APuzzGear4, Hexen, 9021, 0)
	PROP_SpawnState (0)
END_DEFAULTS
