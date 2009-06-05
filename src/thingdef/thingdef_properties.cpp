/*
** thingdef-properties.cpp
**
** Actor definitions - properties and flags handling
**
**---------------------------------------------------------------------------
** Copyright 2002-2007 Christoph Oelckers
** Copyright 2004-2007 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gi.h"
#include "actor.h"
#include "info.h"
#include "tarray.h"
#include "w_wad.h"
#include "templates.h"
#include "r_defs.h"
#include "r_draw.h"
#include "a_pickups.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "i_system.h"
#include "p_local.h"
#include "p_effect.h"
#include "v_palette.h"
#include "doomerrors.h"
#include "a_hexenglobal.h"
#include "a_weaponpiece.h"
#include "p_conversation.h"
#include "v_text.h"
#include "thingdef.h"
#include "a_sharedglobal.h"
#include "r_translate.h"
#include "a_morph.h"
#include "colormatcher.h"
#include "autosegs.h"


//==========================================================================
//
// Gets a class pointer and performs an error check for correct type
//
//==========================================================================
static const PClass *FindClassTentative(const char *name, const char *ancestor)
{
	// "" and "none" mean 'no class'
	if (name == NULL || *name == 0 || !stricmp(name, "none"))
	{
		return NULL;
	}

	const PClass *anc = PClass::FindClass(ancestor);
	assert(anc != NULL);	// parent classes used here should always be natively defined	
	const PClass *cls = const_cast<PClass*>(anc)->FindClassTentative(name);
	assert (cls != NULL);	// cls can not ne NULL here
	if (!cls->IsDescendantOf(anc))
	{
		I_Error("%s does not inherit from %s\n", name, ancestor);
	}
	return cls;
}

//===========================================================================
//
// HandleDeprecatedFlags
//
// Handles the deprecated flags and sets the respective properties
// to appropriate values. This is solely intended for backwards
// compatibility so mixing this with code that is aware of the real
// properties is not recommended
//
//===========================================================================
void HandleDeprecatedFlags(AActor *defaults, FActorInfo *info, bool set, int index)
{
	switch (index)
	{
	case DEPF_FIREDAMAGE:
		defaults->DamageType = set? NAME_Fire : NAME_None;
		break;
	case DEPF_ICEDAMAGE:
		defaults->DamageType = set? NAME_Ice : NAME_None;
		break;
	case DEPF_LOWGRAVITY:
		defaults->gravity = set? FRACUNIT/8 : FRACUNIT;
		break;
	case DEPF_SHORTMISSILERANGE:
		defaults->maxtargetrange = set? 896*FRACUNIT : 0;
		break;
	case DEPF_LONGMELEERANGE:
		defaults->meleethreshold = set? 196*FRACUNIT : 0;
		break;
	case DEPF_QUARTERGRAVITY:
		defaults->gravity = set? FRACUNIT/4 : FRACUNIT;
		break;
	case DEPF_FIRERESIST:
		info->SetDamageFactor(NAME_Fire, set? FRACUNIT/2 : FRACUNIT);
		break;
	// the bounce flags will set the compatibility bounce modes to remain compatible
	case DEPF_HERETICBOUNCE:	
		defaults->bouncetype = set? BOUNCE_HereticCompat : 0;
		break;
	case DEPF_HEXENBOUNCE:
		defaults->bouncetype = set? BOUNCE_HexenCompat : 0;
		break;
	case DEPF_DOOMBOUNCE:
		defaults->bouncetype = set? BOUNCE_DoomCompat : 0;
		break;
	case DEPF_PICKUPFLASH:
		if (set)
		{
			static_cast<AInventory*>(defaults)->PickupFlash = FindClassTentative("PickupFlash", "Actor");
		}
		else
		{
			static_cast<AInventory*>(defaults)->PickupFlash = NULL;
		}
		break;
	default:
		break;	// silence GCC
	}
}

//==========================================================================
//
// 
//
//==========================================================================
int MatchString (const char *in, const char **strings)
{
	int i;

	for (i = 0; *strings != NULL; i++)
	{
		if (!stricmp(in, *strings++))
		{
			return i;
		}
	}
	return -1;
}

//==========================================================================
//
// Info Property handlers
//
//==========================================================================

//==========================================================================
//
//==========================================================================
DEFINE_INFO_PROPERTY(game, T, Actor)
{
	PROP_STRING_PARM(str, 0);
	if (!stricmp(str, "Doom"))
	{
		info->GameFilter |= GAME_Doom;
	}
	else if (!stricmp(str, "Heretic"))
	{
		info->GameFilter |= GAME_Heretic;
	}
	else if (!stricmp(str, "Hexen"))
	{
		info->GameFilter |= GAME_Hexen;
	}
	else if (!stricmp(str, "Raven"))
	{
		info->GameFilter |= GAME_Raven;
	}
	else if (!stricmp(str, "Strife"))
	{
		info->GameFilter |= GAME_Strife;
	}
	else if (!stricmp(str, "Chex"))
	{
		info->GameFilter |= GAME_Chex;
	}
	else if (!stricmp(str, "Any"))
	{
		info->GameFilter = GAME_Any;
	}
	else
	{
		I_Error ("Unknown game type %s", str);
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_INFO_PROPERTY(spawnid, I, Actor)
{
	PROP_INT_PARM(id, 0);
	if (id<0 || id>255)
	{
		I_Error ("SpawnID must be in the range [0,255]");
	}
	else info->SpawnID=(BYTE)id;
}

//==========================================================================
//
//==========================================================================
DEFINE_INFO_PROPERTY(conversationid, IiI, Actor)
{
	PROP_INT_PARM(convid, 0);
	PROP_INT_PARM(id1, 1);
	PROP_INT_PARM(id2, 2);

	// Handling for Strife teaser IDs - only of meaning for the standard items
	// as PWADs cannot be loaded with the teasers.
	if (PROP_PARM_COUNT > 1)
	{
		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE))
			convid=id1;

		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE|GI_TEASER2))
			convid=id2;

		if (convid==-1) return;
	}
	if (convid<0 || convid>1000)
	{
		I_Error ("ConversationID must be in the range [0,1000]");
	}
	else StrifeTypes[convid] = info->Class;
}

//==========================================================================
//
// Property handlers
//
//==========================================================================

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(skip_super, 0, Actor)
{
	if (info->Class->IsDescendantOf(RUNTIME_CLASS(AInventory)))
	{
		bag.ScriptPosition.Message(MSG_WARNING,
			"'skip_super' in definition of inventory item '%s' ignored.", info->Class->TypeName.GetChars() );
		return;
	}
	if (bag.StateSet)
	{
		bag.ScriptPosition.Message(MSG_WARNING,
			"'skip_super' must appear before any state definitions.");
		return;
	}

	memcpy (defaults, GetDefault<AActor>(), sizeof(AActor));
	if (bag.DropItemList != NULL)
	{
		FreeDropItemChain (bag.DropItemList);
	}
	ResetBaggage (&bag, RUNTIME_CLASS(AActor));
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(tag, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaString(AMETA_StrifeName, str);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(health, I, Actor)
{
	PROP_INT_PARM(id, 0);
	defaults->health=id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(gibhealth, I, Actor)
{
	PROP_INT_PARM(id, 0);
	info->Class->Meta.SetMetaInt (AMETA_GibHealth, id);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(woundhealth, I, Actor)
{
	PROP_INT_PARM(id, 0);
	info->Class->Meta.SetMetaInt (AMETA_WoundHealth, id);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(reactiontime, I, Actor)
{
	PROP_INT_PARM(id, 0);
	defaults->reactiontime=id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(painchance, ZI, Actor)
{
	PROP_STRING_PARM(str, 0);
	PROP_INT_PARM(id, 1);
	if (str == NULL)
	{
		defaults->PainChance=id;
	}
	else
	{
		FName painType;
		if (!stricmp(str, "Normal")) painType = NAME_None;
		else painType=str;

		if (info->PainChances == NULL) info->PainChances=new PainChanceList;
		(*info->PainChances)[painType] = (BYTE)id;
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(damage, X, Actor)
{
	PROP_INT_PARM(id, 0);

	// Damage can either be a single number, in which case it is subject
	// to the original damage calculation rules. Or, it can be an expression
	// and will be calculated as-is, ignoring the original rules. For
	// compatibility reasons, expressions must be enclosed within
	// parentheses.

	defaults->Damage = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(speed, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->Speed = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(floatspeed, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->FloatSpeed=id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(radius, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->radius=id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(height, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->height=id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(projectilepassheight, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->projectilepassheight=id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(mass, I, Actor)
{
	PROP_INT_PARM(id, 0);
	defaults->Mass=id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(xscale, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->scaleX = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(yscale, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->scaleY = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(scale, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->scaleX = defaults->scaleY = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(args, Iiiii, Actor)
{
	for (int i = 0; i < PROP_PARM_COUNT; i++)
	{
		PROP_INT_PARM(id, i);
		defaults->args[i] = id;
	}
	defaults->flags2|=MF2_ARGSDEFINED;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(seesound, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->SeeSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(attacksound, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->AttackSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(bouncesound, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->BounceSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(wallbouncesound, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->WallBounceSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(painsound, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->PainSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(deathsound, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->DeathSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(activesound, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->ActiveSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(howlsound, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaInt (AMETA_HowlSound, S_FindSound(str));
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(dropitem, S_i_i, Actor)
{
	PROP_STRING_PARM(type, 0);

	// create a linked list of dropitems
	if (!bag.DropItemSet)
	{
		bag.DropItemSet = true;
		bag.DropItemList = NULL;
	}

	FDropItem *di = new FDropItem;

	di->Name =type;
	di->probability=255;
	di->amount=-1;

	if (PROP_PARM_COUNT > 1)
	{
		PROP_INT_PARM(prob, 1);
		di->probability = prob;
		if (PROP_PARM_COUNT > 2)
		{
			PROP_INT_PARM(amt, 2);
			di->amount = amt;
		}
	}
	di->Next = bag.DropItemList;
	bag.DropItemList = di;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(renderstyle, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	static const char * renderstyles[]={
		"NONE","NORMAL","FUZZY","SOULTRANS","OPTFUZZY","STENCIL","TRANSLUCENT", "ADD","SHADED", NULL};

	static const int renderstyle_values[]={
		STYLE_None, STYLE_Normal, STYLE_Fuzzy, STYLE_SoulTrans, STYLE_OptFuzzy,
			STYLE_TranslucentStencil, STYLE_Translucent, STYLE_Add, STYLE_Shaded};

	// make this work for old style decorations, too.
	if (!strnicmp(str, "style_", 6)) str+=6;

	int style = MatchString(str, renderstyles);
	if (style < 0) I_Error("Unknown render style '%s'", str);
	defaults->RenderStyle = LegacyRenderStyles[renderstyle_values[style]];
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(defaultalpha, 0, Actor)
{
	defaults->alpha = gameinfo.gametype == GAME_Heretic ? HR_SHADOW : HX_SHADOW;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(alpha, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->alpha = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(obituary, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaString (AMETA_Obituary, str);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(hitobituary, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaString (AMETA_HitObituary, str);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(donthurtshooter, 0, Actor)
{
	info->Class->Meta.SetMetaInt (ACMETA_DontHurtShooter, true);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(explosionradius, I, Actor)
{
	PROP_INT_PARM(id, 0);
	info->Class->Meta.SetMetaInt (ACMETA_ExplosionRadius, id);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(explosiondamage, I, Actor)
{
	PROP_INT_PARM(id, 0);
	info->Class->Meta.SetMetaInt (ACMETA_ExplosionDamage, id);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(deathheight, F, Actor)
{
	PROP_FIXED_PARM(h, 0);
	// AActor::Die() uses a height of 0 to mean "cut the height to 1/4",
	// so if a height of 0 is desired, store it as -1.
	info->Class->Meta.SetMetaFixed (AMETA_DeathHeight, h <= 0 ? -1 : h);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(burnheight, F, Actor)
{
	PROP_FIXED_PARM(h, 0);
	// The note above for AMETA_DeathHeight also applies here.
	info->Class->Meta.SetMetaFixed (AMETA_BurnHeight, h <= 0 ? -1 : h);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(maxtargetrange, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->maxtargetrange = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(meleethreshold, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->meleethreshold = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(meleedamage, I, Actor)
{
	PROP_INT_PARM(id, 0);
	info->Class->Meta.SetMetaInt (ACMETA_MeleeDamage, id);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(meleerange, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->meleerange = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(meleesound, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaInt (ACMETA_MeleeSound, S_FindSound(str));
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(missiletype, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaInt (ACMETA_MissileName, FName(str));
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(missileheight, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	info->Class->Meta.SetMetaFixed (ACMETA_MissileHeight, id);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(pushfactor, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->pushfactor = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(translation, L, Actor)
{
	PROP_INT_PARM(type, 0);

	if (type == 0)
	{
		PROP_INT_PARM(trans, 1);
		int max = (gameinfo.gametype==GAME_Strife || (info->GameFilter&GAME_Strife)) ? 6:2;
		if (trans < 0 || trans > max)
		{
			I_Error ("Translation must be in the range [0,%d]", max);
		}
		defaults->Translation = TRANSLATION(TRANSLATION_Standard, trans);
	}
	else 
	{
		FRemapTable CurrentTranslation;

		CurrentTranslation.MakeIdentity();
		for(int i = 1; i < PROP_PARM_COUNT; i++)
		{
			PROP_STRING_PARM(str, i);
			if (i== 1 && PROP_PARM_COUNT == 2 && !stricmp(str, "Ice"))
			{
				defaults->Translation = TRANSLATION(TRANSLATION_Standard, 7);
				return;
			}
			else
			{
				CurrentTranslation.AddToTranslation(str);
			}
		}
		defaults->Translation = CurrentTranslation.StoreTranslation ();
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(stencilcolor, C, Actor)
{
	PROP_COLOR_PARM(color, 0);

	defaults->fillcolor = color | (ColorMatcher.Pick (RPART(color), GPART(color), BPART(color)) << 24);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(bloodcolor, C, Actor)
{
	PROP_COLOR_PARM(color, 0);

	PalEntry pe = color;
	pe.a = CreateBloodTranslation(pe);
	info->Class->Meta.SetMetaInt (AMETA_BloodColor, pe);
}


//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(bloodtype, Sss, Actor)
{
	PROP_STRING_PARM(str, 0)
	PROP_STRING_PARM(str1, 1)
	PROP_STRING_PARM(str2, 2)

	FName blood = str;
	// normal blood
	info->Class->Meta.SetMetaInt (AMETA_BloodType, blood);

	if (PROP_PARM_COUNT > 1)
	{
		blood = str1;
	}
	// blood splatter
	info->Class->Meta.SetMetaInt (AMETA_BloodType2, blood);

	if (PROP_PARM_COUNT > 2)
	{
		blood = str2;
	}
	// axe blood
	info->Class->Meta.SetMetaInt (AMETA_BloodType3, blood);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(bouncetype, S, Actor)
{
	const char *names[] = { "None", "Doom", "Heretic", "Hexen", "*", "DoomCompat", "HereticCompat", "HexenCompat", NULL };
	PROP_STRING_PARM(id, 0);
	defaults->bouncetype = MatchString(id, names);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(bouncefactor, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->bouncefactor = clamp<fixed_t>(id, 0, FRACUNIT);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(wallbouncefactor, F, Actor)
{
	PROP_FIXED_PARM(id, 0);
	defaults->wallbouncefactor = clamp<fixed_t>(id, 0, FRACUNIT);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(bouncecount, I, Actor)
{
	PROP_INT_PARM(id, 0);
	defaults->bouncecount = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(minmissilechance, I, Actor)
{
	PROP_INT_PARM(id, 0);
	defaults->MinMissileChance=id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(damagetype, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	if (!stricmp(str, "Normal")) defaults->DamageType = NAME_None;
	else defaults->DamageType=str;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(damagefactor, SF, Actor)
{
	PROP_STRING_PARM(str, 0);
	PROP_FIXED_PARM(id, 1);

	if (info->DamageFactors == NULL) info->DamageFactors=new DmgFactors;

	FName dmgType;
	if (!stricmp(str, "Normal")) dmgType = NAME_None;
	else dmgType=str;

	(*info->DamageFactors)[dmgType]=id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(decal, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->DecalGenerator = (FDecalBase *)intptr_t(int(FName(str)));
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(maxstepheight, F, Actor)
{
	PROP_FIXED_PARM(i, 0);
	defaults->MaxStepHeight = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(maxdropoffheight, F, Actor)
{
	PROP_FIXED_PARM(i, 0);
	defaults->MaxDropOffHeight = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(poisondamage, I, Actor)
{
	PROP_INT_PARM(i, 0);
	info->Class->Meta.SetMetaInt (AMETA_PoisonDamage, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(fastspeed, F, Actor)
{
	PROP_FIXED_PARM(i, 0);
	info->Class->Meta.SetMetaFixed (AMETA_FastSpeed, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(radiusdamagefactor, F, Actor)
{
	PROP_FIXED_PARM(i, 0);
	info->Class->Meta.SetMetaFixed (AMETA_RDFactor, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(cameraheight, F, Actor)
{
	PROP_FIXED_PARM(i, 0);
	info->Class->Meta.SetMetaFixed (AMETA_CameraHeight, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(vspeed, F, Actor)
{
	PROP_FIXED_PARM(i, 0);
	defaults->momz = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(gravity, F, Actor)
{
	PROP_FIXED_PARM(i, 0);

	if (i < 0) I_Error ("Gravity must not be negative.");
	defaults->gravity = i;
	if (i == 0) defaults->flags |= MF_NOGRAVITY;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(species, S, Actor)
{
	PROP_STRING_PARM(n, 0);
	defaults->Species = n;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(clearflags, 0, Actor)
{
	defaults->flags=defaults->flags3=defaults->flags4=defaults->flags5=0;
	defaults->flags2&=MF2_ARGSDEFINED;	// this flag must not be cleared
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(monster, 0, Actor)
{
	// sets the standard flags for a monster
	defaults->flags|=MF_SHOOTABLE|MF_COUNTKILL|MF_SOLID; 
	defaults->flags2|=MF2_PUSHWALL|MF2_MCROSS|MF2_PASSMOBJ;
	defaults->flags3|=MF3_ISMONSTER;
	defaults->flags4|=MF4_CANUSEWALLS;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(projectile, 0, Actor)
{
	// sets the standard flags for a projectile
	defaults->flags|=MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE; 
	defaults->flags2|=MF2_IMPACT|MF2_PCROSS|MF2_NOTELEPORT;
	if (gameinfo.gametype&GAME_Raven) defaults->flags5|=MF5_BLOODSPLATTER;
}

//==========================================================================
//
// Special inventory properties
//
//==========================================================================

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(backpackamount, I, Ammo)
{
	PROP_INT_PARM(i, 0);
	defaults->BackpackAmount = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(backpackmaxamount, I, Ammo)
{
	PROP_INT_PARM(i, 0);
	defaults->BackpackMaxAmount = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(dropamount, I, Ammo)
{
	PROP_INT_PARM(i, 0);
	info->Class->Meta.SetMetaInt (AIMETA_DropAmount, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(armor, maxsaveamount, I, BasicArmorBonus)
{
	PROP_INT_PARM(i, 0);
	defaults->MaxSaveAmount = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(armor, maxbonus, I, BasicArmorBonus)
{
	PROP_INT_PARM(i, 0);
	defaults->BonusCount = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(armor, maxbonusmax, I, BasicArmorBonus)
{
	PROP_INT_PARM(i, 0);
	defaults->BonusMax = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(saveamount, I, Armor)
{
	PROP_INT_PARM(i, 0);

	// Special case here because this property has to work for 2 unrelated classes
	if (info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->SaveAmount=i;
	}
	else if (info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->SaveAmount=i;
	}
	else
	{
		I_Error("\"Armor.SaveAmount\" requires an actor of type \"Armor\"");
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(savepercent, F, Armor)
{
	PROP_FIXED_PARM(i, 0);

	i = clamp(i, 0, 100*FRACUNIT)/100;
	// Special case here because this property has to work for 2 unrelated classes
	if (info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->SavePercent = i;
	}
	else if (info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->SavePercent = i;
	}
	else
	{
		I_Error("\"Armor.SavePercent\" requires an actor of type \"Armor\"\n");
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(maxabsorb, I, Armor)
{
	PROP_INT_PARM(i, 0);

	// Special case here because this property has to work for 2 unrelated classes
	if (info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->MaxAbsorb = i;
	}
	else if (info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->MaxAbsorb = i;
	}
	else
	{
		I_Error("\"Armor.MaxAbsorb\" requires an actor of type \"Armor\"\n");
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(maxfullabsorb, I, Armor)
{
	PROP_INT_PARM(i, 0);

	// Special case here because this property has to work for 2 unrelated classes
	if (info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->MaxFullAbsorb = i;
	}
	else if (info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->MaxFullAbsorb = i;
	}
	else
	{
		I_Error("\"Armor.MaxFullAbsorb\" requires an actor of type \"Armor\"\n");
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(amount, I, Inventory)
{
	PROP_INT_PARM(i, 0);
	defaults->Amount = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(icon, S, Inventory)
{
	PROP_STRING_PARM(i, 0);

	defaults->Icon = TexMan.CheckForTexture(i, FTexture::TEX_MiscPatch);
	if (!defaults->Icon.isValid())
	{
		// Don't print warnings if the item is for another game or if this is a shareware IWAD. 
		// Strife's teaser doesn't contain all the icon graphics of the full game.
		if ((info->GameFilter == GAME_Any || info->GameFilter & gameinfo.gametype) &&
			!(gameinfo.flags&GI_SHAREWARE) && Wads.GetLumpFile(bag.Lumpnum) != 0)
		{
			bag.ScriptPosition.Message(MSG_WARNING,
				"Icon '%s' for '%s' not found\n", i, info->Class->TypeName.GetChars());
		}
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(maxamount, I, Inventory)
{
	PROP_INT_PARM(i, 0);
	defaults->MaxAmount = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(defmaxamount, 0, Inventory)
{
	defaults->MaxAmount = gameinfo.gametype == GAME_Heretic ? 16 : 25;
}


//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(pickupflash, S, Inventory)
{
	PROP_STRING_PARM(str, 0);
	defaults->PickupFlash = FindClassTentative(str, "Actor");
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(pickupmessage, S, Inventory)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaString(AIMETA_PickupMessage, str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(pickupsound, S, Inventory)
{
	PROP_STRING_PARM(str, 0);
	defaults->PickupSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(respawntics, I, Inventory)
{
	PROP_INT_PARM(i, 0);
	defaults->RespawnTics = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(usesound, S, Inventory)
{
	PROP_STRING_PARM(str, 0);
	defaults->UseSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(givequest, I, Inventory)
{
	PROP_INT_PARM(i, 0);
	info->Class->Meta.SetMetaInt(AIMETA_GiveQuest, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(lowmessage, IS, Health)
{
	PROP_INT_PARM(i, 0);
	PROP_STRING_PARM(str, 1);
	info->Class->Meta.SetMetaInt(AIMETA_LowHealth, i);
	info->Class->Meta.SetMetaString(AIMETA_LowHealthMessage, str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(autouse, I, HealthPickup)
{
	PROP_INT_PARM(i, 0);
	defaults->autousemode = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(number, I, PuzzleItem)
{
	PROP_INT_PARM(i, 0);
	defaults->PuzzleItemNumber = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(failmessage, S, PuzzleItem)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaString(AIMETA_PuzzFailMessage, str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(ammogive, I, Weapon)
{
	PROP_INT_PARM(i, 0);
	defaults->AmmoGive1 = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(ammogive1, I, Weapon)
{
	PROP_INT_PARM(i, 0);
	defaults->AmmoGive1 = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(ammogive2, I, Weapon)
{
	PROP_INT_PARM(i, 0);
	defaults->AmmoGive2 = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(ammotype, S, Weapon)
{
	PROP_STRING_PARM(str, 0);
	defaults->AmmoType1 = FindClassTentative(str, "Ammo");
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(ammotype1, S, Weapon)
{
	PROP_STRING_PARM(str, 0);
	defaults->AmmoType1 = FindClassTentative(str, "Ammo");
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(ammotype2, S, Weapon)
{
	PROP_STRING_PARM(str, 0);
	defaults->AmmoType2 = FindClassTentative(str, "Ammo");
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(ammouse, I, Weapon)
{
	PROP_INT_PARM(i, 0);
	defaults->AmmoUse1 = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(ammouse1, I, Weapon)
{
	PROP_INT_PARM(i, 0);
	defaults->AmmoUse1 = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(ammouse2, I, Weapon)
{
	PROP_INT_PARM(i, 0);
	defaults->AmmoUse2 = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(kickback, I, Weapon)
{
	PROP_INT_PARM(i, 0);
	defaults->Kickback = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(defaultkickback, 0, Weapon)
{
	defaults->Kickback = gameinfo.defKickback;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(readysound, S, Weapon)
{
	PROP_STRING_PARM(str, 0);
	defaults->ReadySound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(selectionorder, I, Weapon)
{
	PROP_INT_PARM(i, 0);
	defaults->SelectionOrder = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(sisterweapon, S, Weapon)
{
	PROP_STRING_PARM(str, 0);
	defaults->SisterWeaponType = FindClassTentative(str, "Weapon");
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(upsound, S, Weapon)
{
	PROP_STRING_PARM(str, 0);
	defaults->UpSound = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(yadjust, F, Weapon)
{
	PROP_FIXED_PARM(i, 0);
	defaults->YAdjust = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(slotnumber, I, Weapon)
{
	PROP_INT_PARM(i, 0);
	info->Class->Meta.SetMetaInt(AWMETA_SlotNumber, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(slotpriority, F, Weapon)
{
	PROP_FIXED_PARM(i, 0);
	info->Class->Meta.SetMetaFixed(AWMETA_SlotPriority, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(number, I, WeaponPiece)
{
	PROP_INT_PARM(i, 0);
	defaults->PieceValue = 1 << (i-1);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(weapon, S, WeaponPiece)
{
	PROP_STRING_PARM(str, 0);
	defaults->WeaponClass = FindClassTentative(str, "Weapon");
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(powerup, color, C_f, Inventory)
{
	PROP_INT_PARM(i, 0);

	int alpha;
	PalEntry * pBlendColor;

	if (info->Class->IsDescendantOf(RUNTIME_CLASS(APowerup)))
	{
		pBlendColor = &((APowerup*)defaults)->BlendColor;
	}
	else if (info->Class->IsDescendantOf(RUNTIME_CLASS(APowerupGiver)))
	{
		pBlendColor = &((APowerupGiver*)defaults)->BlendColor;
	}
	else
	{
		I_Error("\"powerup.color\" requires an actor of type \"Powerup\"\n");
		return;
	}

	PROP_INT_PARM(mode, 0);
	PROP_INT_PARM(color, 1);

	if (mode == 1)
	{
		PROP_STRING_PARM(name, 1);

		if (!stricmp(name, "INVERSEMAP"))
		{
			*pBlendColor = INVERSECOLOR;
			return;
		}
		else if (!stricmp(name, "GOLDMAP"))
		{
			*pBlendColor = GOLDCOLOR;
			return;
		}
		// [BC] Yay, more hacks.
		else if (!stricmp(name, "REDMAP" ))
		{
			*pBlendColor = REDCOLOR;
			return;
		}
		else if (!stricmp(name, "GREENMAP" ))
		{
			*pBlendColor = GREENCOLOR;
			return;
		}

		color = V_GetColor(NULL, name);
	}
	if (PROP_PARM_COUNT > 2)
	{
		PROP_FLOAT_PARM(falpha, 2);
		alpha=int(falpha*255);
	}
	else alpha = 255/3;

	alpha=clamp<int>(alpha, 0, 255);
	if (alpha!=0) *pBlendColor = MAKEARGB(alpha, 0, 0, 0) | color;
	else *pBlendColor = 0;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(powerup, duration, I, Inventory)
{
	int *pEffectTics;

	if (info->Class->IsDescendantOf(RUNTIME_CLASS(APowerup)))
	{
		pEffectTics = &((APowerup*)defaults)->EffectTics;
	}
	else if (info->Class->IsDescendantOf(RUNTIME_CLASS(APowerupGiver)))
	{
		pEffectTics = &((APowerupGiver*)defaults)->EffectTics;
	}
	else
	{
		I_Error("\"powerup.duration\" requires an actor of type \"Powerup\"\n");
		return;
	}

	PROP_INT_PARM(i, 0);
	*pEffectTics = (i >= 0) ? i : -i * TICRATE;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(powerup, mode, S, PowerupGiver)
{
	PROP_STRING_PARM(str, 0);
	defaults->mode = (FName)str;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(powerup, type, S, PowerupGiver)
{
	PROP_STRING_PARM(str, 0);

	// Yuck! What was I thinking when I decided to prepend "Power" to the name? 
	// Now it's too late to change it...
	const PClass *cls = PClass::FindClass(str);
	if (cls == NULL || !cls->IsDescendantOf(RUNTIME_CLASS(APowerup)))
	{
		FString st;
		st.Format("%s%s", strnicmp(str, "power", 5)? "Power" : "", str);
		cls = FindClassTentative(st, "Powerup");
	}

	defaults->PowerupType = cls;
}

//==========================================================================
//
// [GRB] Special player properties
//
//==========================================================================

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, displayname, S, PlayerPawn)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaString (APMETA_DisplayName, str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, soundclass, S, PlayerPawn)
{
	PROP_STRING_PARM(str, 0);

	FString tmp = str;
	tmp.ReplaceChars (' ', '_');
	info->Class->Meta.SetMetaString (APMETA_SoundClass, tmp);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, face, S, PlayerPawn)
{
	PROP_STRING_PARM(str, 0);
	FString tmp = str;

	tmp.ToUpper();
	if (tmp.Len() != 3)
	{
		bag.ScriptPosition.Message(MSG_WARNING,
			"Invalid face '%s' for '%s';\nSTF replacement codes must be 3 characters.\n",
			tmp.GetChars(), info->Class->TypeName.GetChars ());
	}

	bool valid = (
		(((tmp[0] >= 'A') && (tmp[0] <= 'Z')) || ((tmp[0] >= '0') && (tmp[0] <= '9'))) &&
		(((tmp[1] >= 'A') && (tmp[1] <= 'Z')) || ((tmp[1] >= '0') && (tmp[1] <= '9'))) &&
		(((tmp[2] >= 'A') && (tmp[2] <= 'Z')) || ((tmp[2] >= '0') && (tmp[2] <= '9')))
		);
	if (!valid)
	{
		bag.ScriptPosition.Message(MSG_WARNING,
			"Invalid face '%s' for '%s';\nSTF replacement codes must be alphanumeric.\n",
			tmp.GetChars(), info->Class->TypeName.GetChars ());
	}
	
	info->Class->Meta.SetMetaString (APMETA_Face, tmp);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, colorrange, I_I, PlayerPawn)
{
	PROP_INT_PARM(start, 0);
	PROP_INT_PARM(end, 1);

	if (start > end)
		swap (start, end);

	info->Class->Meta.SetMetaInt (APMETA_ColorRange, (start & 255) | ((end & 255) << 8));
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, attackzoffset, F, PlayerPawn)
{
	PROP_FIXED_PARM(z, 0);
	defaults->AttackZOffset = z;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, jumpz, F, PlayerPawn)
{
	PROP_FIXED_PARM(z, 0);
	defaults->JumpZ = z;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, spawnclass, L, PlayerPawn)
{
	PROP_INT_PARM(type, 0);

	if (type == 0)
	{
		PROP_INT_PARM(val, 1);
		if (val > 0) defaults->SpawnMask |= 1<<(val-1);
	}
	else 
	{
		for(int i=1; i<PROP_PARM_COUNT; i++)
		{
			PROP_STRING_PARM(str, i);

			if (!stricmp(str, "Any"))
				defaults->SpawnMask = 0;
			else if (!stricmp(str, "Fighter"))
				defaults->SpawnMask |= 1;
			else if (!stricmp(str, "Cleric"))
				defaults->SpawnMask |= 2;
			else if (!stricmp(str, "Mage"))
				defaults->SpawnMask |= 4;

		}
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, viewheight, F, PlayerPawn)
{
	PROP_FIXED_PARM(z, 0);
	defaults->ViewHeight = z;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, forwardmove, F_f, PlayerPawn)
{
	PROP_FIXED_PARM(m, 0);
	defaults->ForwardMove1 = defaults->ForwardMove2 = m;
	if (PROP_PARM_COUNT > 1)
	{
		PROP_FIXED_PARM(m2, 1);
		defaults->ForwardMove2 = m2;
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, sidemove, F_f, PlayerPawn)
{
	PROP_FIXED_PARM(m, 0);
	defaults->SideMove1 = defaults->SideMove2 = m;
	if (PROP_PARM_COUNT > 1)
	{
		PROP_FIXED_PARM(m2, 1);
		defaults->SideMove2 = m2;
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, maxhealth, I, PlayerPawn)
{
	PROP_INT_PARM(z, 0);
	defaults->MaxHealth = z;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, runhealth, I, PlayerPawn)
{
	PROP_INT_PARM(z, 0);
	defaults->RunHealth = z;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, morphweapon, S, PlayerPawn)
{
	PROP_STRING_PARM(z, 0);
	defaults->MorphWeapon = FName(z);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, scoreicon, S, PlayerPawn)
{
	PROP_STRING_PARM(z, 0);
	defaults->ScoreIcon = TexMan.CheckForTexture(z, FTexture::TEX_MiscPatch);
	if (!defaults->ScoreIcon.isValid())
	{
		bag.ScriptPosition.Message(MSG_WARNING,
			"Icon '%s' for '%s' not found\n", z, info->Class->TypeName.GetChars ());
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, crouchsprite, S, PlayerPawn)
{
	PROP_STRING_PARM(z, 0);
	if (strlen(z) == 4)
	{
		defaults->crouchsprite = GetSpriteIndex (z);
	}
	else if (*z == 0)
	{
		defaults->crouchsprite = 0;
	}
	else
	{
		I_Error("Sprite name must have exactly 4 characters");
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, damagescreencolor, C, PlayerPawn)
{
	PROP_COLOR_PARM(c, 0);
	defaults->DamageFade = c;
}

//==========================================================================
//
// [GRB] Store start items in drop item list
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, startitem, S_i, PlayerPawn)
{
	PROP_STRING_PARM(str, 0);

	// create a linked list of startitems
	if (!bag.DropItemSet)
	{
		bag.DropItemSet = true;
		bag.DropItemList = NULL;
	}

	FDropItem * di=new FDropItem;

	di->Name = str;
	di->probability = 255;
	di->amount = 1;
	if (PROP_PARM_COUNT > 1)
	{
		PROP_INT_PARM(amt, 1);
		di->amount = amt;
	}
	di->Next = bag.DropItemList;
	bag.DropItemList = di;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, invulnerabilitymode, S, PlayerPawn)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaInt (APMETA_InvulMode, (FName)str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, healradiustype, S, PlayerPawn)
{
	PROP_STRING_PARM(str, 0);
	info->Class->Meta.SetMetaInt (APMETA_HealingRadius, (FName)str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, hexenarmor, FFFFF, PlayerPawn)
{
	for (int i=0;i<5;i++)
	{
		PROP_FIXED_PARM(val, i);
		info->Class->Meta.SetMetaFixed (APMETA_Hexenarmor0+i, val);
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, weaponslot, ISsssssssssssssssssssssssssssssssssssssssssss, PlayerPawn)
{
	PROP_INT_PARM(slot, 0);

	if (slot < 0 || slot > 9)
	{
		I_Error("Slot must be between 0 and 9.");
	}
	else
	{
		FString weapons;

		for(int i = 1; i < PROP_PARM_COUNT; ++i)
		{
			PROP_STRING_PARM(str, i);
			weapons << ' ' << str;
		}
		info->Class->Meta.SetMetaString(APMETA_Slot0 + slot, &weapons[1]);
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(playerclass, S, MorphProjectile)
{
	PROP_STRING_PARM(str, 0);
	defaults->PlayerClass = FName(str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(monsterclass, S, MorphProjectile)
{
	PROP_STRING_PARM(str, 0);
	defaults->MonsterClass = FName(str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(duration, I, MorphProjectile)
{
	PROP_INT_PARM(i, 0);
	defaults->Duration = i >= 0 ? i : -i*TICRATE;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(morphstyle, M, MorphProjectile)
{
	PROP_INT_PARM(i, 0);
	defaults->MorphStyle = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(morphflash, S, MorphProjectile)
{
	PROP_STRING_PARM(str, 0);
	defaults->MorphFlash = FName(str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(unmorphflash, S, MorphProjectile)
{
	PROP_STRING_PARM(str, 0);
	defaults->UnMorphFlash = FName(str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(playerclass, S, PowerMorph)
{
	PROP_STRING_PARM(str, 0);
	defaults->PlayerClass = FName(str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(morphstyle, M, PowerMorph)
{
	PROP_INT_PARM(i, 0);
	defaults->MorphStyle = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(morphflash, S, PowerMorph)
{
	PROP_STRING_PARM(str, 0);
	defaults->MorphFlash = FName(str);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(unmorphflash, S, PowerMorph)
{
	PROP_STRING_PARM(str, 0);
	defaults->UnMorphFlash = FName(str);
}


