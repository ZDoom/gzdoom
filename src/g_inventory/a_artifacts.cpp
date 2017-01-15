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

//===========================================================================
//
// APowerInvulnerable :: AlterWeaponSprite
//
//===========================================================================

int APowerInvulnerable::AlterWeaponSprite (visstyle_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);
	if (Owner != NULL)
	{
		if (Mode == NAME_Ghost && !(Owner->flags & MF_SHADOW))
		{
			vis->Alpha = MIN<float>(0.25f + (float)Owner->Alpha*0.75f, 1.f);
		}
	}
	return changed;
}

// Strength (aka Berserk) Powerup --------------------------------------------

IMPLEMENT_CLASS(APowerStrength, false, false)

//===========================================================================
//
// APowerStrength :: HandlePickup
//
//===========================================================================

bool APowerStrength::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{ // Setting EffectTics to 0 will force Powerup's HandlePickup()
	  // method to reset the tic count so you get the red flash again.
		EffectTics = 0;
	}
	return Super::HandlePickup (item);
}

//===========================================================================
//
// APowerStrength :: InitEffect
//
//===========================================================================

void APowerStrength::InitEffect ()
{
	Super::InitEffect();
}

//===========================================================================
//
// APowerStrength :: DoEffect
//
//===========================================================================

void APowerStrength::Tick ()
{
	// Strength counts up to diminish the fade.
	assert(EffectTics < (INT_MAX - 1)); // I can't see a game lasting nearly two years, but...
	EffectTics += 2;
	Super::Tick();
}

//===========================================================================
//
// APowerStrength :: GetBlend
//
//===========================================================================

PalEntry APowerStrength::GetBlend ()
{
	// slowly fade the berserk out
	int cnt = 12 - (EffectTics >> 6);

	if (cnt > 0)
	{
		cnt = (cnt + 7) >> 3;
		return PalEntry (BlendColor.a*cnt*255/9,
			BlendColor.r, BlendColor.g, BlendColor.b);
	}
	return 0;
}

// Invisibility Powerup ------------------------------------------------------

IMPLEMENT_CLASS(APowerInvisibility, false, false)

// Invisibility flag combos
#define INVISIBILITY_FLAGS1	(MF_SHADOW)
#define INVISIBILITY_FLAGS3	(MF3_GHOST)
#define INVISIBILITY_FLAGS5	(MF5_CANTSEEK)

//===========================================================================
//
// APowerInvisibility :: InitEffect
//
//===========================================================================

void APowerInvisibility::InitEffect ()
{
	Super::InitEffect();
	// This used to call CommonInit(), which used to contain all the code that's repeated every
	// tic, plus the following code that needs to happen once and only once.
	// The CommonInit() code has been moved to DoEffect(), so this now ends with a call to DoEffect(),
	// and DoEffect() no longer needs to call InitEffect(). CommonInit() has been removed for being redundant.
	if (Owner != NULL)
	{
		flags &= ~(Owner->flags  & INVISIBILITY_FLAGS1);
		Owner->flags  |= flags & INVISIBILITY_FLAGS1;
		flags3 &= ~(Owner->flags3 & INVISIBILITY_FLAGS3);
		Owner->flags3 |= flags3 & INVISIBILITY_FLAGS3;
		flags5 &= ~(Owner->flags5 & INVISIBILITY_FLAGS5);
		Owner->flags5 |= flags5 & INVISIBILITY_FLAGS5;

		CallDoEffect();
	}
}

//===========================================================================
//
// APowerInvisibility :: DoEffect
//
//===========================================================================
void APowerInvisibility::DoEffect ()
{
	Super::DoEffect();
	// Due to potential interference with other PowerInvisibility items
	// the effect has to be refreshed each tic.
	double ts = (Strength / 100) * (special1 + 1);
	
	if (ts > 1.) ts = 1.;
	Owner->Alpha = clamp((1. - ts), 0., 1.);
	switch (Mode)
	{
	case (NAME_Fuzzy):
		Owner->RenderStyle = STYLE_OptFuzzy;
		break;
	case (NAME_Opaque):
		Owner->RenderStyle = STYLE_Normal;
		break;
	case (NAME_Additive):
		Owner->RenderStyle = STYLE_Add;
		break;
	case (NAME_Stencil):
		Owner->RenderStyle = STYLE_Stencil;
		break;
	case (NAME_AddStencil) :
		Owner->RenderStyle = STYLE_AddStencil;
		break;
	case (NAME_TranslucentStencil) :
		Owner->RenderStyle = STYLE_TranslucentStencil;
		break;
	case (NAME_None) :
	case (NAME_Cumulative):
	case (NAME_Translucent):
		Owner->RenderStyle = STYLE_Translucent;
		break;
	default: // Something's wrong
		Owner->RenderStyle = STYLE_Normal;
		Owner->Alpha = 1.;
		break;
	}
}

//===========================================================================
//
// APowerInvisibility :: EndEffect
//
//===========================================================================

void APowerInvisibility::EndEffect ()
{
	Super::EndEffect();
	if (Owner != NULL)
	{
		Owner->flags  &= ~(flags  & INVISIBILITY_FLAGS1);
		Owner->flags3 &= ~(flags3 & INVISIBILITY_FLAGS3);
		Owner->flags5 &= ~(flags5 & INVISIBILITY_FLAGS5);

		Owner->RenderStyle = STYLE_Normal;
		Owner->Alpha = 1.;

		// Check whether there are other invisibility items and refresh their effect.
		// If this isn't done there will be one incorrectly drawn frame when this
		// item expires.
		AInventory *item = Owner->Inventory;
		while (item != NULL)
		{
			if (item->IsKindOf(RUNTIME_CLASS(APowerInvisibility)) && item != this)
			{
				static_cast<APowerInvisibility*>(item)->DoEffect();
			}
			item = item->Inventory;
		}
	}
}

//===========================================================================
//
// APowerInvisibility :: AlterWeaponSprite
//
//===========================================================================

int APowerInvisibility::AlterWeaponSprite (visstyle_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);
	// Blink if the powerup is wearing off
	if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
		vis->Alpha = 1.f;
		return 1;
	}
	else if (changed == 1)
	{
		// something else set the weapon sprite back to opaque but this item is still active.
		float ts = float((Strength / 100) * (special1 + 1));
		vis->Alpha = clamp<>((1.f - ts), 0.f, 1.f);
		switch (Mode)
		{
		case (NAME_Fuzzy):
			vis->RenderStyle = STYLE_OptFuzzy;
			break;
		case (NAME_Opaque):
			vis->RenderStyle = STYLE_Normal;
			break;
		case (NAME_Additive):
			vis->RenderStyle = STYLE_Add;
			break;
		case (NAME_Stencil):
			vis->RenderStyle = STYLE_Stencil;
			break;
		case (NAME_TranslucentStencil) :
			vis->RenderStyle = STYLE_TranslucentStencil;
			break;
		case (NAME_AddStencil) :
			vis->RenderStyle = STYLE_AddStencil;
			break;
		case (NAME_None) :
		case (NAME_Cumulative):
		case (NAME_Translucent):
		default:
			vis->RenderStyle = STYLE_Translucent;
			break;
		}
	}
	// Handling of Strife-like cumulative invisibility powerups, the weapon itself shouldn't become invisible
	if ((vis->Alpha < 0.25f && special1 > 0) || (vis->Alpha == 0))
	{
		vis->Alpha = clamp((1.f - float(Strength/100)), 0.f, 1.f);
		vis->colormap = SpecialColormaps[INVERSECOLORMAP].Colormap;
	}
	return -1;	// This item is valid so another one shouldn't reset the translucency
}

//===========================================================================
//
// APowerInvisibility :: HandlePickup
//
// If the player already has the first stage of a cumulative powerup, getting 
// it again increases the player's alpha. (But shouldn't this be in Use()?)
//
//===========================================================================

bool APowerInvisibility::HandlePickup (AInventory *item)
{
	if (Mode == NAME_Cumulative && ((Strength * special1) < 1.) && item->GetClass() == GetClass())
	{
		APowerup *power = static_cast<APowerup *>(item);
		if (power->EffectTics == 0)
		{
			power->ItemFlags |= IF_PICKUPGOOD;
			return true;
		}
		// Only increase the EffectTics, not decrease it.
		// Color also gets transferred only when the new item has an effect.
		if (power->EffectTics > EffectTics)
		{
			EffectTics = power->EffectTics;
			BlendColor = power->BlendColor;
		}
		special1++;	// increases power
		power->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	return Super::HandlePickup (item);
}

// Ironfeet Powerup ----------------------------------------------------------

IMPLEMENT_CLASS(APowerIronFeet, false, false)

//===========================================================================
//
// APowerIronFeet :: AbsorbDamage
//
//===========================================================================

void APowerIronFeet::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType == NAME_Drowning)
	{
		newdamage = 0;
	}
	else if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// APowerIronFeet :: DoEffect
//
//===========================================================================

void APowerIronFeet::DoEffect ()
{
	if (Owner->player != NULL)
	{
		Owner->player->mo->ResetAirSupply ();
	}
}


// Strife Environment Suit Powerup -------------------------------------------

IMPLEMENT_CLASS(APowerMask, false, false)

//===========================================================================
//
// APowerMask :: AbsorbDamage
//
//===========================================================================

void APowerMask::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType == NAME_Fire)
	{
		newdamage = 0;
	}
	else
	{
		Super::AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// APowerMask :: DoEffect
//
//===========================================================================

void APowerMask::DoEffect ()
{
	Super::DoEffect ();
	if (!(level.time & 0x3f))
	{
		S_Sound (Owner, CHAN_AUTO, "misc/mask", 1, ATTN_STATIC);
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

