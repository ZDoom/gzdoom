#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "gi.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_lnspec.h"
#include "p_effect.h"
#include "a_artifacts.h"
#include "sbar.h"
#include "d_player.h"
#include "m_random.h"
#include "v_video.h"
#include "templates.h"
#include "a_morph.h"
#include "g_level.h"
#include "doomstat.h"
#include "v_palette.h"
#include "serializer.h"
#include "r_utility.h"
#include "virtual.h"
#include "g_levellocals.h"

#include "r_data/colormaps.h"

static FRandom pr_torch ("Torch");

/* Those are no longer needed, except maybe as reference?
 * They're not used anywhere in the code anymore, except
 * MAULATORTICS as redefined in a_minotaur.cpp...
#define	INVULNTICS (30*TICRATE)
#define	INVISTICS (60*TICRATE)
#define	INFRATICS (120*TICRATE)
#define	IRONTICS (60*TICRATE)
#define WPNLEV2TICS (40*TICRATE)
#define FLIGHTTICS (60*TICRATE)
#define SPEEDTICS (45*TICRATE)
#define MAULATORTICS (25*TICRATE)
#define	TIMEFREEZE_TICS	( 12 * TICRATE )
*/


IMPLEMENT_CLASS(APowerup, false, false)

// Powerup-Giver -------------------------------------------------------------


IMPLEMENT_CLASS(APowerupGiver, false, true)

IMPLEMENT_POINTERS_START(APowerupGiver)
IMPLEMENT_POINTER(PowerupType)
IMPLEMENT_POINTERS_END

DEFINE_FIELD(APowerupGiver, PowerupType)
DEFINE_FIELD(APowerupGiver, EffectTics)
DEFINE_FIELD(APowerupGiver, BlendColor)
DEFINE_FIELD(APowerupGiver, Mode)
DEFINE_FIELD(APowerupGiver, Strength)

//===========================================================================
//
// APowerupGiver :: Use
//
//===========================================================================

bool APowerupGiver::Use (bool pickup)
{
	if (PowerupType == NULL) return true;	// item is useless
	if (Owner == nullptr) return true;

	APowerup *power = static_cast<APowerup *> (Spawn (PowerupType));

	if (EffectTics != 0)
	{
		power->EffectTics = EffectTics;
	}
	if (BlendColor != 0)
	{
		if (BlendColor != MakeSpecialColormap(65535)) power->BlendColor = BlendColor;
		else power->BlendColor = 0;
	}
	if (Mode != NAME_None)
	{
		power->Mode = Mode;
	}
	if (Strength != 0)
	{
		power->Strength = Strength;
	}

	power->ItemFlags |= ItemFlags & (IF_ALWAYSPICKUP|IF_ADDITIVETIME|IF_NOTELEPORTFREEZE);
	if (power->CallTryPickup (Owner))
	{
		return true;
	}
	power->GoAwayAndDie ();
	return false;
}

//===========================================================================
//
// APowerupGiver :: Serialize
//
//===========================================================================

void APowerupGiver::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (APowerupGiver*)GetDefault();
	arc("poweruptype", PowerupType, def->PowerupType)
		("effecttics", EffectTics, def->EffectTics)
		("blendcolor", BlendColor, def->BlendColor)
		("mode", Mode, def->Mode)
		("strength", Strength, def->Strength);
}

// Powerup -------------------------------------------------------------------

DEFINE_FIELD(APowerup, EffectTics)
DEFINE_FIELD(APowerup, BlendColor)
DEFINE_FIELD(APowerup, Mode)
DEFINE_FIELD(APowerup, Strength)
DEFINE_FIELD(APowerup, Colormap)

//===========================================================================
//
// APowerup :: Serialize
//
//===========================================================================

void APowerup::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (APowerup*)GetDefault();
	arc("effecttics", EffectTics, def->EffectTics)
		("blendcolor", BlendColor, def->BlendColor)
		("mode", Mode, def->Mode)
		("strength", Strength, def->Strength)
		("colormap", Colormap, def->Colormap);
}

//===========================================================================
//
// APowerup :: OwnerDied
//
// Powerups don't last beyond death.
//
//===========================================================================

void APowerup::OwnerDied ()
{
	Destroy ();
}

// Morph powerup ------------------------------------------------------

IMPLEMENT_CLASS(APowerMorph, false, true)

IMPLEMENT_POINTERS_START(APowerMorph)
	IMPLEMENT_POINTER(PlayerClass)
	IMPLEMENT_POINTER(MorphFlash)
	IMPLEMENT_POINTER(UnMorphFlash)
IMPLEMENT_POINTERS_END


DEFINE_FIELD(APowerMorph, PlayerClass)
DEFINE_FIELD(APowerMorph, MorphFlash)
DEFINE_FIELD(APowerMorph, UnMorphFlash)
DEFINE_FIELD(APowerMorph, MorphStyle)
DEFINE_FIELD(APowerMorph, MorphedPlayer)

//===========================================================================
//
// APowerMorph :: Serialize
//
//===========================================================================

void APowerMorph::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("playerclass", PlayerClass)
		("morphstyle", MorphStyle)
		("morphflash", MorphFlash)
		("unmorphflash", UnMorphFlash)
		("morphedplayer", MorphedPlayer);
}

