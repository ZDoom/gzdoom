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

//===========================================================================
//
// APowerup :: Tick
//
//===========================================================================

void APowerup::Tick ()
{
	// Powerups cannot exist outside an inventory
	if (Owner == NULL)
	{
		Destroy ();
	}
	if (EffectTics > 0 && --EffectTics == 0)
	{
		Destroy ();
	}
}

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
		("strength", Strength, def->Strength);
}

//===========================================================================
//
// APowerup :: GetBlend
//
//===========================================================================

PalEntry APowerup::GetBlend ()
{
	if (isBlinking())
		return 0;

	if (IsSpecialColormap(BlendColor)) return 0;
	return BlendColor;
}

//===========================================================================
//
// APowerup :: InitEffect
//
//===========================================================================

void APowerup::InitEffect ()
{
}

DEFINE_ACTION_FUNCTION(APowerup, InitEffect)
{
	PARAM_SELF_PROLOGUE(APowerup);
	self->InitEffect();
	return 0;
}

void APowerup::CallInitEffect()
{
	IFVIRTUAL(APowerup, InitEffect)
	{
		VMValue params[1] = { (DObject*)this };
		VMFrameStack stack;
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
	else InitEffect();
}

//===========================================================================
//
// APowerup :: isBlinking (todo: make this virtual so that child classes can configure their blinking)
//
//===========================================================================

bool APowerup::isBlinking() const
{
	return (EffectTics <= BLINKTHRESHOLD && (EffectTics & 8) && !(ItemFlags & IF_NOSCREENBLINK));
}

DEFINE_ACTION_FUNCTION(APowerup, isBlinking)
{
	PARAM_SELF_PROLOGUE(APowerup);
	ACTION_RETURN_BOOL(self->isBlinking());
}

//===========================================================================
//
// APowerup :: DoEffect
//
//===========================================================================

void APowerup::DoEffect ()
{
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}

	if (EffectTics > 0)
	{
		int Colormap = GetSpecialColormap(BlendColor);

		if (Colormap != NOFIXEDCOLORMAP)
		{
			if (!isBlinking())
			{
				Owner->player->fixedcolormap = Colormap;
			}
			else if (Owner->player->fixedcolormap == Colormap)	
			{
				// only unset if the fixed colormap comes from this item
				Owner->player->fixedcolormap = NOFIXEDCOLORMAP;
			}
		}
	}
}

//===========================================================================
//
// APowerup :: EndEffect
//
//===========================================================================

void APowerup::EndEffect ()
{
	int colormap = GetSpecialColormap(BlendColor);

	if (colormap != NOFIXEDCOLORMAP && Owner && Owner->player && Owner->player->fixedcolormap == colormap)
	{ // only unset if the fixed colormap comes from this item
		Owner->player->fixedcolormap = NOFIXEDCOLORMAP;
	}
}

DEFINE_ACTION_FUNCTION(APowerup, EndEffect)
{
	PARAM_SELF_PROLOGUE(APowerup);
	self->EndEffect();
	return 0;
}

void APowerup::CallEndEffect()
{
	IFVIRTUAL(APowerup, EndEffect)
	{
		VMValue params[1] = { (DObject*)this };
		VMFrameStack stack;
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
	else EndEffect();
}


//===========================================================================
//
// APowerup :: Destroy
//
//===========================================================================

void APowerup::OnDestroy ()
{
	CallEndEffect ();
	Super::OnDestroy();
}

//===========================================================================
//
// APowerup :: DrawPowerup
//
//===========================================================================

bool APowerup::DrawPowerup (int x, int y)
{
	if (!Icon.isValid())
	{
		return false;
	}
	if (!isBlinking())
	{
		FTexture *pic = TexMan(Icon);
		screen->DrawTexture (pic, x, y,
			DTA_HUDRules, HUD_Normal,
//			DTA_TopOffset, pic->GetHeight()/2,
//			DTA_LeftOffset, pic->GetWidth()/2,
			TAG_DONE);
	}
	return true;
}

//===========================================================================
//
// APowerup :: HandlePickup
//
//===========================================================================

bool APowerup::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{
		APowerup *power = static_cast<APowerup*>(item);
		if (power->EffectTics == 0)
		{
			power->ItemFlags |= IF_PICKUPGOOD;
			return true;
		}
		// Color gets transferred if the new item has an effect.

		// Increase the effect's duration.
		if (power->ItemFlags & IF_ADDITIVETIME) 
		{
			EffectTics += power->EffectTics;
			BlendColor = power->BlendColor;
		}
		// If it's not blinking yet, you can't replenish the power unless the
		// powerup is required to be picked up.
		else if (EffectTics > BLINKTHRESHOLD && !(power->ItemFlags & IF_ALWAYSPICKUP))
		{
			return true;
		}
		// Reset the effect duration.
		else if (power->EffectTics > EffectTics)
		{
			EffectTics = power->EffectTics;
			BlendColor = power->BlendColor;
		}
		power->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	return false;
}

//===========================================================================
//
// APowerup :: CreateCopy
//
//===========================================================================

AInventory *APowerup::CreateCopy (AActor *other)
{
	// Get the effective effect time.
	EffectTics = abs (EffectTics);
	// Abuse the Owner field to tell the
	// InitEffect method who started it;
	// this should be cleared afterwards,
	// as this powerup instance is not
	// properly attached to anything yet.
	Owner = other;
	// Actually activate the powerup.
	CallInitEffect ();
	// Clear the Owner field, unless it was
	// changed by the activation, for example,
	// if this instance is a morph powerup;
	// the flag tells the caller that the
	// ownership has changed so that they
	// can properly handle the situation.
	if (!(ItemFlags & IF_CREATECOPYMOVED))
	{
		Owner = NULL;
	}
	// All done.
	return this;
}

//===========================================================================
//
// APowerup :: CreateTossable
//
// Powerups are never droppable, even without IF_UNDROPPABLE set.
//
//===========================================================================

AInventory *APowerup::CreateTossable ()
{
	return NULL;
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

// Invulnerability Powerup ---------------------------------------------------

IMPLEMENT_CLASS(APowerInvulnerable, false, false)

//===========================================================================
//
// APowerInvulnerable :: InitEffect
//
//===========================================================================

void APowerInvulnerable::InitEffect ()
{
	Super::InitEffect();
	Owner->effects &= ~FX_RESPAWNINVUL;
	Owner->flags2 |= MF2_INVULNERABLE;
	if (Mode == NAME_None && Owner->IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{
		Mode = static_cast<PClassPlayerPawn *>(Owner->GetClass())->InvulMode;
	}
	if (Mode == NAME_Reflective)
	{
		Owner->flags2 |= MF2_REFLECTIVE;
	}
}

//===========================================================================
//
// APowerInvulnerable :: DoEffect
//
//===========================================================================

void APowerInvulnerable::DoEffect ()
{
	Super::DoEffect ();

	if (Owner == NULL)
	{
		return;
	}

	if (Mode == NAME_Ghost)
	{
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Translucent;
			if (!(level.time & 7) && Owner->Alpha > 0 && Owner->Alpha < 1)
			{
				if (Owner->Alpha == HX_SHADOW)
				{
					Owner->Alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->Alpha = 0;
					Owner->flags2 |= MF2_NONSHOOTABLE;
				}
			}
			if (!(level.time & 31))
			{
				if (Owner->Alpha == 0)
				{
					Owner->flags2 &= ~MF2_NONSHOOTABLE;
					Owner->Alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->Alpha = HX_SHADOW;
				}
			}
		}
		else
		{
			Owner->flags2 &= ~MF2_NONSHOOTABLE;
		}
	}
}

//===========================================================================
//
// APowerInvulnerable :: EndEffect
//
//===========================================================================

void APowerInvulnerable::EndEffect ()
{
	Super::EndEffect();

	if (Owner == NULL)
	{
		return;
	}

	Owner->flags2 &= ~MF2_INVULNERABLE;
	Owner->effects &= ~FX_RESPAWNINVUL;
	if (Mode == NAME_Ghost)
	{
		Owner->flags2 &= ~MF2_NONSHOOTABLE;
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Normal;
			Owner->Alpha = 1.;
		}
	}
	else if (Mode == NAME_Reflective)
	{
		Owner->flags2 &= ~MF2_REFLECTIVE;
	}

	if (Owner->player != NULL)
	{
		Owner->player->fixedcolormap = NOFIXEDCOLORMAP;
	}
}

// Speed Powerup -------------------------------------------------------------

IMPLEMENT_CLASS(APowerSpeed, false, false)

DEFINE_FIELD(APowerSpeed, SpeedFlags)

//===========================================================================
//
// APowerSpeed :: Serialize
//
//===========================================================================

void APowerSpeed::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("speedflags", SpeedFlags);
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

