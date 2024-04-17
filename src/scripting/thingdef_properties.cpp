/*
** thingdef-properties.cpp
**
** Actor denitions - properties and flags handling
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
#include "d_player.h"
#include "filesystem.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "decallib.h"
#include "p_local.h"
#include "p_effect.h"
#include "v_text.h"
#include "thingdef.h"
#include "a_morph.h"
#include "teaminfo.h"
#include "vmbuilder.h"
#include "a_keys.h"
#include "g_levellocals.h"
#include "types.h"
#include "a_dynlight.h"
#include "v_video.h"
#include "texturemanager.h"

//==========================================================================
//
// Gets a class pointer and performs an error check for correct type
//
//==========================================================================
static PClassActor *FindClassTentative(const char *name, PClass *ancestor, bool optional = false)
{
	// "" and "none" mean 'no class'
	if (name == NULL || *name == 0 || !stricmp(name, "none"))
	{
		return NULL;
	}

	PClass *cls = ancestor->FindClassTentative(name);
	assert(cls != NULL);	// cls can not be NULL here
	if (!cls->IsDescendantOf(ancestor))
	{
		I_Error("%s does not inherit from %s\n", name, ancestor->TypeName.GetChars());
	}
	if (cls->Size == TentativeClass && optional)
	{
		cls->bOptional = true;
	}
	return static_cast<PClassActor *>(cls);
}

//==========================================================================
//
// Sets or clears a flag, taking field width into account.
//
//==========================================================================
void ModActorFlag(AActor *actor, FFlagDef *fd, bool set)
{
	// Little-Endian machines only need one case, because all field sizes
	// start at the same address. (Unless the machine has unaligned access
	// exceptions, in which case you'll need multiple cases for it too.)
#ifdef __BIG_ENDIAN__
	if (fd->fieldsize == 4)
#endif
	{
		uint32_t *flagvar = (uint32_t *)((char *)actor + fd->structoffset);
		if (set)
		{
			*flagvar |= fd->flagbit;
		}
		else
		{
			*flagvar &= ~fd->flagbit;
		}
	}
#ifdef __BIG_ENDIAN__
	else if (fd->fieldsize == 2)
	{
		uint16_t *flagvar = (uint16_t *)((char *)actor + fd->structoffset);
		if (set)
		{
			*flagvar |= fd->flagbit;
		}
		else
		{
			*flagvar &= ~fd->flagbit;
		}
	}
	else
	{
		assert(fd->fieldsize == 1);
		uint8_t *flagvar = (uint8_t *)((char *)actor + fd->structoffset);
		if (set)
		{
			*flagvar |= fd->flagbit;
		}
		else
		{
			*flagvar &= ~fd->flagbit;
		}
	}
#endif
}

//==========================================================================
//
// Finds a flag by name and sets or clears it
//
// Returns true if the flag was found for the actor; else returns false
//
//==========================================================================

bool ModActorFlag(AActor *actor, const FString &flagname, bool set, bool printerror)
{
	bool found = false;

	if (actor != NULL)
	{
		auto Level = actor->Level;
		const char *dot = strchr(flagname.GetChars(), '.');
		FFlagDef *fd;
		PClassActor *cls = actor->GetClass();

		if (dot != NULL)
		{
			FString part1(flagname.GetChars(), dot - flagname.GetChars());
			fd = FindFlag(cls, part1.GetChars(), dot + 1);
		}
		else
		{
			fd = FindFlag(cls, flagname.GetChars(), NULL);
		}

		if (fd != NULL)
		{
			found = true;

			if (actor->CountsAsKill() && actor->health > 0) --Level->total_monsters;
			if (actor->flags & MF_COUNTITEM) --Level->total_items;
			if (actor->flags5 & MF5_COUNTSECRET) --Level->total_secrets;

			if (fd->structoffset == -1)
			{
				HandleDeprecatedFlags(actor, cls, set, fd->flagbit);
			}
			else
			{
				ActorFlags *flagp = (ActorFlags*)(((char*)actor) + fd->structoffset);

				// If these 2 flags get changed we need to update the blockmap and sector links.
				bool linkchange = flagp == &actor->flags && (fd->flagbit == MF_NOBLOCKMAP || fd->flagbit == MF_NOSECTOR);

				FLinkContext ctx;
				if (linkchange) actor->UnlinkFromWorld(&ctx);
				ModActorFlag(actor, fd, set);
				if (linkchange) actor->LinkToWorld(&ctx);
			}

			if (actor->CountsAsKill() && actor->health > 0) ++Level->total_monsters;
			if (actor->flags & MF_COUNTITEM) ++Level->total_items;
			if (actor->flags5 & MF5_COUNTSECRET) ++Level->total_secrets;
		}
		else if (printerror)
		{
			DPrintf(DMSG_ERROR, "ACS/DECORATE: '%s' is not a flag in '%s'\n", flagname.GetChars(), cls->TypeName.GetChars());
		}
	}

	return found;
}

//==========================================================================
//
// Returns whether an actor flag is true or not.
//
//==========================================================================

INTBOOL CheckActorFlag(AActor *owner, FFlagDef *fd)
{
	if (fd->structoffset == -1)
	{
		return CheckDeprecatedFlags(owner, owner->GetClass(), fd->flagbit);
	}
	else
#ifdef __BIG_ENDIAN__
	if (fd->fieldsize == 4)
#endif
	{
		return fd->flagbit & *(uint32_t *)(((char*)owner) + fd->structoffset);
	}
#ifdef __BIG_ENDIAN__
	else if (fd->fieldsize == 2)
	{
		return fd->flagbit & *(uint16_t *)(((char*)owner) + fd->structoffset);
	}
	else
	{
		assert(fd->fieldsize == 1);
		return fd->flagbit & *(uint8_t *)(((char*)owner) + fd->structoffset);
	}
#endif
}

INTBOOL CheckActorFlag(AActor *owner, const char *flagname, bool printerror)
{
	const char *dot = strchr (flagname, '.');
	FFlagDef *fd;
	const PClass *cls = owner->GetClass();

	if (dot != NULL)
	{
		FString part1(flagname, dot-flagname);
		fd = FindFlag (cls, part1.GetChars(), dot+1);
	}
	else
	{
		fd = FindFlag (cls, flagname, NULL);
	}

	if (fd != NULL)
	{
		return CheckActorFlag(owner, fd);
	}
	else
	{
		if (printerror) Printf("Unknown flag '%s' in '%s'\n", flagname, cls->TypeName.GetChars());
		return false;
	}
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
void HandleDeprecatedFlags(AActor *defaults, PClassActor *info, bool set, int index)
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
		defaults->Gravity = set ? 1. / 8 : 1.;
		break;
	case DEPF_SHORTMISSILERANGE:
		defaults->maxtargetrange = set? 896. : 0.;
		break;
	case DEPF_LONGMELEERANGE:
		defaults->meleethreshold = set? 196. : 0.;
		break;
	case DEPF_QUARTERGRAVITY:
		defaults->Gravity = set ? 1. / 4 : 1.;
		break;
	case DEPF_FIRERESIST:
		info->SetDamageFactor(NAME_Fire, set ? 0.5 : 1.);
		break;
	// the bounce flags will set the compatibility bounce modes to remain compatible
	case DEPF_HERETICBOUNCE:
		defaults->BounceFlags &= ~(BOUNCE_TypeMask|BOUNCE_UseSeeSound);
		if (set) defaults->BounceFlags |= BOUNCE_HereticCompat;
		break;
	case DEPF_HEXENBOUNCE:
		defaults->BounceFlags &= ~(BOUNCE_TypeMask|BOUNCE_UseSeeSound);
		if (set) defaults->BounceFlags |= BOUNCE_HexenCompat;
		break;
	case DEPF_DOOMBOUNCE:
		defaults->BounceFlags &= ~(BOUNCE_TypeMask|BOUNCE_UseSeeSound);
		if (set) defaults->BounceFlags |= BOUNCE_DoomCompat;
		break;
	case DEPF_PICKUPFLASH:
		if (set)
		{
			defaults->PointerVar<PClass>(NAME_PickupFlash) = FindClassTentative("PickupFlash", RUNTIME_CLASS(AActor));
		}
		else
		{
			defaults->PointerVar<PClass>(NAME_PickupFlash) = nullptr;
		}
		break;
	case DEPF_INTERHUBSTRIP: // Old system was 0 or 1, so if the flag is cleared, assume 1.
		defaults->IntVar(NAME_InterHubAmount) = set ? 0 : 1;
		break;

	case DEPF_HIGHERMPROB:
		defaults->MinMissileChance = set ? 160 : 200;
		break;

	default:
		break;	// silence GCC
	}
}

//===========================================================================
//
// CheckDeprecatedFlags
//
// Checks properties related to deprecated flags, and returns true only
// if the relevant properties are configured exactly as they would have
// been by setting the flag in HandleDeprecatedFlags.
//
//===========================================================================

bool CheckDeprecatedFlags(AActor *actor, PClassActor *info, int index)
{
	// A deprecated flag is false if
	// a) it hasn't been added here
	// b) any property of the actor differs from what it would be after setting the flag using HandleDeprecatedFlags

	// Deprecated flags are normally replaced by something more flexible, which means a multitude of related configurations
	// will report "false".

	switch (index)
	{
	case DEPF_FIREDAMAGE:
		return actor->DamageType == NAME_Fire;
	case DEPF_ICEDAMAGE:
		return actor->DamageType == NAME_Ice;
	case DEPF_LOWGRAVITY:
		return actor->Gravity == 1./8;
	case DEPF_SHORTMISSILERANGE:
		return actor->maxtargetrange == 896.;
	case DEPF_LONGMELEERANGE:
		return actor->meleethreshold == 196.;
	case DEPF_QUARTERGRAVITY:
		return actor->Gravity == 1./4;
	case DEPF_FIRERESIST:
		for (auto &df : info->ActorInfo()->DamageFactors)
		{
			if (df.first == NAME_Fire) return df.second == 0.5;
		}
		return false;

	case DEPF_HERETICBOUNCE:
		return (actor->BounceFlags & (BOUNCE_TypeMask|BOUNCE_UseSeeSound)) == BOUNCE_HereticCompat;

	case DEPF_HEXENBOUNCE:
		return (actor->BounceFlags & (BOUNCE_TypeMask|BOUNCE_UseSeeSound)) == BOUNCE_HexenCompat;
	
	case DEPF_DOOMBOUNCE:
		return (actor->BounceFlags & (BOUNCE_TypeMask|BOUNCE_UseSeeSound)) == BOUNCE_DoomCompat;

	case DEPF_PICKUPFLASH:
		return actor->PointerVar<PClass>(NAME_PickupFlash) == PClass::FindClass(NAME_PickupFlash);

	case DEPF_INTERHUBSTRIP:
		return !(actor->IntVar(NAME_InterHubAmount));

	case DEPF_HIGHERMPROB:
		return actor->MinMissileChance <= 160;
	}

	return false; // Any entirely unknown flag is not set
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
// Get access to scripted pointers.
// They need a bit more work than other variables.
//
//==========================================================================

static bool PointerCheck(PType *symtype, PType *checktype)
{
	auto symptype = PType::toClassPointer(symtype);
	auto checkptype = PType::toClassPointer(checktype);
	return symptype != nullptr && checkptype != nullptr && symptype->ClassRestriction->IsDescendantOf(checkptype->ClassRestriction);
}

//==========================================================================
//
// Info Property handlers
//
//==========================================================================


//==========================================================================
//
//==========================================================================
DEFINE_INFO_PROPERTY(game, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	auto & GameFilter = info->ActorInfo()->GameFilter;
	if (!stricmp(str, "Doom"))
	{
		GameFilter |= GAME_Doom;
	}
	else if (!stricmp(str, "Heretic"))
	{
		GameFilter |= GAME_Heretic;
	}
	else if (!stricmp(str, "Hexen"))
	{
		GameFilter |= GAME_Hexen;
	}
	else if (!stricmp(str, "Raven"))
	{
		GameFilter |= GAME_Raven;
	}
	else if (!stricmp(str, "Strife"))
	{
		GameFilter |= GAME_Strife;
	}
	else if (!stricmp(str, "Chex"))
	{
		GameFilter |= GAME_Chex;
	}
	else if (!stricmp(str, "Any"))
	{
		GameFilter = GAME_Any;
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
	if (id<0 || id>65535)
	{
		I_Error ("SpawnID must be in the range [0,65535]");
	}
	else info->ActorInfo()->SpawnID=(uint16_t)id;
}

//==========================================================================
//
//==========================================================================
DEFINE_INFO_PROPERTY(conversationid, IiI, Actor)
{
	PROP_INT_PARM(convid, 0);
	PROP_INT_PARM(id1, 1);
	PROP_INT_PARM(id2, 2);

	if (convid <= 0 || convid > 65535) return;	// 0 is not usable because the dialogue scripts use it as 'no object'.
	else info->ActorInfo()->ConversationID=(uint16_t)convid;
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
	auto actorclass = RUNTIME_CLASS(AActor);
	if (info->Size != actorclass->Size)
	{
		bag.ScriptPosition.Message(MSG_OPTERROR,
			"'skip_super' is only allowed in subclasses of Actor with no additional fields and will be ignored in type %s.", info->TypeName.GetChars());
		return;
	}
	if (bag.StateSet)
	{
		bag.ScriptPosition.Message(MSG_OPTERROR,
			"'skip_super' must appear before any state definitions.");
		return;
	}

	// major hack job alert. This is only supposed to copy the parts that actually are defined by AActor itself.
	memcpy(&defaults->snext, &GetDefault<AActor>()->snext, (uint8_t*)&defaults[1] - (uint8_t*)&defaults->snext);

	ResetBaggage (&bag, RUNTIME_CLASS(AActor));
	static_cast<PClassActor*>(bag.Info)->ActorInfo()->SkipSuperSet = true;	// ZScript processes the states later so this property must be flagged for later handling.
}

//==========================================================================
// for internal use only - please do not document!
//==========================================================================
DEFINE_PROPERTY(defaultstateusage, I, Actor)
{
	PROP_INT_PARM(use, 0);
	static_cast<PClassActor*>(bag.Info)->ActorInfo()->DefaultStateUsage = use;

}
//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(tag, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->SetTag(str);
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
		FName painType = NAME_None;
		if (stricmp(str, "Normal")) painType = str;

		info->SetPainChance(painType, id);
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(defthreshold, I, Actor)
{
	PROP_INT_PARM(id, 0);
	if (id < 0)
		I_Error("DefThreshold cannot be negative.");
	defaults->DefThreshold = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(threshold, I, Actor)
{
	PROP_INT_PARM(id, 0);
	if (id < 0)
		I_Error("Threshold cannot be negative.");
	defaults->threshold = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(damage, X, Actor)
{
	PROP_INT_PARM(dmgval, 0);
	PROP_EXP_PARM(id, 1);

	// Damage can either be a single number, in which case it is subject
	// to the original damage calculation rules. Or, it can be an expression
	// and will be calculated as-is, ignoring the original rules. For
	// compatibility reasons, expressions must be enclosed within
	// parentheses.

	defaults->DamageVal = dmgval;
	// Only DECORATE can get here with a valid expression.
	CreateDamageFunction(bag.Namespace, bag.Version, bag.Info, defaults, id, true, bag.Lumpnum);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(scale, F, Actor)
{
	PROP_DOUBLE_PARM(id, 0);
	defaults->Scale.X = defaults->Scale.Y = float(id);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(floatbobphase, I, Actor)
{
	PROP_INT_PARM(id, 0);
	if (id < -1 || id >= 64) I_Error ("FloatBobPhase must be in range [-1,63]");
	defaults->FloatBobPhase = id;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(floatbobstrength, F, Actor)
{
	PROP_DOUBLE_PARM(id, 0);
	defaults->FloatBobStrength = id;
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
DEFINE_PROPERTY(dropitem, S_i_i, Actor)
{
	PROP_STRING_PARM(type, 0);

	// create a linked list of dropitems
	if (!bag.DropItemSet)
	{
		bag.DropItemSet = true;
		bag.DropItemList = NULL;
	}

	FDropItem *di = (FDropItem*)ClassDataAllocator.Alloc(sizeof(FDropItem));

	di->Name = type;
	di->Probability = 255;
	di->Amount = -1;

	if (PROP_PARM_COUNT > 1)
	{
		PROP_INT_PARM(prob, 1);
		di->Probability = prob;
		if (PROP_PARM_COUNT > 2)
		{
			PROP_INT_PARM(amt, 2);
			di->Amount = amt;
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
		"NONE", "NORMAL", "FUZZY", "SOULTRANS", "OPTFUZZY", "STENCIL", 
		"TRANSLUCENT", "ADD", "SHADED", "SHADOW", "SUBTRACT", "ADDSTENCIL", 
		"ADDSHADED", "COLORBLEND", "COLORADD", "MULTIPLY", NULL };

	static const int renderstyle_values[]={
		STYLE_None, STYLE_Normal, STYLE_Fuzzy, STYLE_SoulTrans, STYLE_OptFuzzy,
			STYLE_TranslucentStencil, STYLE_Translucent, STYLE_Add, STYLE_Shaded,
			STYLE_Shadow, STYLE_Subtract, STYLE_AddStencil, STYLE_AddShaded,
			STYLE_ColorBlend, STYLE_ColorAdd, STYLE_Multiply};

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
	defaults->Alpha = gameinfo.gametype == GAME_Heretic ? HR_SHADOW : HX_SHADOW;
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
		int max = 6;
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
			FTranslationID tnum;
			if (i== 1 && PROP_PARM_COUNT == 2 && (tnum = R_FindCustomTranslation(str)) != INVALID_TRANSLATION)
			{
				defaults->Translation = tnum;
				return;
			}
			else
			{
				// parse all ranges to get a complete list of errors, if more than one range fails.
				try
				{
					CurrentTranslation.AddToTranslation(str);
				}
				catch (CRecoverableError &err)
				{
					bag.ScriptPosition.Message(MSG_WARNING, "Error in translation '%s':\n" TEXTCOLOR_CYAN "%s\n", str, err.GetMessage());
				}
			}
		}
		defaults->Translation = GPalette.StoreTranslation (TRANSLATION_Decorate, &CurrentTranslation);
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(stencilcolor, C, Actor)
{
	PROP_COLOR_PARM(color, 0, &bag.ScriptPosition);

	defaults->fillcolor = color | (ColorMatcher.Pick (RPART(color), GPART(color), BPART(color)) << 24);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(bloodcolor, C, Actor)
{
	PROP_COLOR_PARM(color, 0, &bag.ScriptPosition);

	defaults->BloodColor = color;
	defaults->BloodColor.a = 255;	// a should not be 0.
	defaults->BloodTranslation = CreateBloodTranslation(color);
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
	defaults->NameVar("BloodType") = blood;

	if (PROP_PARM_COUNT > 1)
	{
		blood = str1;
	}
	// blood splatter
	defaults->NameVar("BloodType2") = blood;

	if (PROP_PARM_COUNT > 2)
	{
		blood = str2;
	}
	// axe blood
	defaults->NameVar("BloodType3") = blood;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(bouncetype, S, Actor)
{
	static const char *names[] = { "None", "Doom", "Heretic", "Hexen", "DoomCompat", "HereticCompat", "HexenCompat", "Grenade", "Classic", NULL };
	static const ActorBounceFlag flags[] = { BOUNCE_None,
		BOUNCE_Doom, BOUNCE_Heretic, BOUNCE_Hexen,
		BOUNCE_DoomCompat, BOUNCE_HereticCompat, BOUNCE_HexenCompat,
		BOUNCE_Grenade, BOUNCE_Classic, };
	PROP_STRING_PARM(id, 0);
	int match = MatchString(id, names);
	if (match < 0)
	{
		I_Error("Unknown bouncetype %s", id);
		match = 0;
	}
	defaults->BounceFlags &= ~(BOUNCE_TypeMask | BOUNCE_UseSeeSound);
	defaults->BounceFlags |= flags[match];
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(bouncefactor, F, Actor)
{
	PROP_DOUBLE_PARM(id, 0);
	defaults->bouncefactor = clamp<double>(id, 0, 1);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(wallbouncefactor, F, Actor)
{
	PROP_DOUBLE_PARM(id, 0);
	defaults->wallbouncefactor = clamp<double>(id, 0, 1);
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

//==========================================================================
DEFINE_PROPERTY(paintype, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	if (!stricmp(str, "Normal")) defaults->PainType = NAME_None;
	else defaults->PainType=str;
}

//==========================================================================

//==========================================================================
DEFINE_PROPERTY(deathtype, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	if (!stricmp(str, "Normal")) defaults->DeathType = NAME_None;
	else defaults->DeathType=str;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(damagefactor, ZF, Actor)
{
	PROP_STRING_PARM(str, 0);
	PROP_DOUBLE_PARM(id, 1);

	if (str == nullptr)
	{
		defaults->DamageFactor = id;
	}
	else
	{
		FName dmgType = NAME_None;
		if (stricmp(str, "Normal")) dmgType = str;

		info->SetDamageFactor(dmgType, id);
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(decal, S, Actor)
{
	PROP_STRING_PARM(str, 0);
	defaults->DecalGenerator = (FDecalBase *)(intptr_t)FName(str).GetIndex();
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(poisondamage, Iii, Actor)
{
	PROP_INT_PARM(poisondamage, 0);
	PROP_INT_PARM(poisonduration, 1);
	PROP_INT_PARM(poisonperiod, 2);

	defaults->PoisonDamage = poisondamage;
	if (PROP_PARM_COUNT == 1)
	{
		defaults->PoisonDuration = INT_MIN;
	}
	else
	{
		defaults->PoisonDuration = poisonduration;

		if (PROP_PARM_COUNT > 2)
			defaults->PoisonPeriod = poisonperiod;
		else
			defaults->PoisonPeriod = 0;
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(gravity, F, Actor)
{
	PROP_DOUBLE_PARM(i, 0);

	//if (i < 0) I_Error ("Gravity must not be negative.");
	defaults->Gravity = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(spriteangle, F, Actor)
{
	PROP_DOUBLE_PARM(i, 0);
	defaults->SpriteAngle = DAngle::fromDeg(i);
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(friction, F, Actor)
{
	PROP_DOUBLE_PARM(i, 0);

	if (i < 0) I_Error ("Friction must not be negative.");
	defaults->Friction = i;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(clearflags, 0, Actor)
{
	defaults->flags = 0;
	defaults->flags2 &= MF2_ARGSDEFINED;	// this flag must not be cleared
	defaults->flags3 = 0;
	defaults->flags4 = 0;
	defaults->flags5 = 0;
	defaults->flags6 = 0;
	defaults->flags7 = 0;
	defaults->flags8 = 0;
	defaults->flags9 = 0;
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
//==========================================================================
DEFINE_PROPERTY(activation, N, Actor)
{
	// How the thing behaves when activated by death, USESPECIAL or BUMPSPECIAL
	PROP_INT_PARM(val, 0);
	defaults->activationtype = val;
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(designatedteam, I, Actor)
{
	PROP_INT_PARM(val, 0);
	if(val < 0 || (val >= (signed) Teams.Size() && val != TEAM_NONE))
		I_Error("Invalid team designation.\n");
	defaults->DesignatedTeam = val;
}

//==========================================================================
// MBF21
//==========================================================================
DEFINE_PROPERTY(infightinggroup, I, Actor)
{
	PROP_INT_PARM(i, 0);
	if (i < 0)
	{
		I_Error("Infighting groups must be >= 0.");
	}
	info->ActorInfo()->infighting_group = i;
}

//==========================================================================
// MBF21
//==========================================================================
DEFINE_PROPERTY(projectilegroup, I, Actor)
{
	PROP_INT_PARM(i, 0);
	if (i < -1)
	{
		I_Error("Projectile groups must be >= -1.");
	}
	info->ActorInfo()->projectile_group = i;
}

//==========================================================================
// MBF21
//==========================================================================
DEFINE_PROPERTY(splashgroup, I, Actor)
{
	PROP_INT_PARM(i, 0);
	if (i < 0)
	{
		I_Error("Splash groups must be >= 0.");
	}
	info->ActorInfo()->splash_group = i;
}

//==========================================================================
// [BB]
//==========================================================================
DEFINE_PROPERTY(visibletoteam, I, Actor)
{
	PROP_INT_PARM(i, 0);
	defaults->VisibleToTeam=i+1;
}

//==========================================================================
// [BB]
//==========================================================================
DEFINE_PROPERTY(visibletoplayerclass, Ssssssssssssssssssss, Actor)
{
	info->ActorInfo()->VisibleToPlayerClass.Clear();
	for(int i = 0;i < PROP_PARM_COUNT;++i)
	{
		PROP_STRING_PARM(n, i);
		if (*n != 0)
			info->ActorInfo()->VisibleToPlayerClass.Push(FindClassTentative(n, RUNTIME_CLASS(AActor)));
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_PROPERTY(distancecheck, S, Actor)
{
	PROP_STRING_PARM(cvar, 0);
	FBaseCVar *scratch;
	FBaseCVar *cv = FindCVar(cvar, &scratch);
	if (cv == NULL)
	{
		I_Error("CVar %s not defined", cvar);
	}
	else if (cv->GetRealType() == CVAR_Int)
	{
		info->ActorInfo()->distancecheck = static_cast<FIntCVar *>(cv);
	}
	else
	{
		I_Error("CVar %s must of type Int", cvar);
	}
}


//==========================================================================
//
// Special inventory properties
//
//==========================================================================

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(restrictedto, Ssssssssssssssssssss, Inventory)
{
	auto restrictarray = (TArray<PClassActor*>*)defaults->ScriptVar(NAME_RestrictedToPlayerClass, nullptr);

	restrictarray->Clear();
	for(int i = 0;i < PROP_PARM_COUNT;++i)
	{
		PROP_STRING_PARM(n, i);
		if (*n != 0)
			restrictarray->Push(FindClassTentative(n, RUNTIME_CLASS(AActor)));
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(forbiddento, Ssssssssssssssssssss, Inventory)
{
	auto forbidarray = (TArray<PClassActor*>*)defaults->ScriptVar(NAME_ForbiddenToPlayerClass, nullptr);

	forbidarray->Clear();
	for(int i = 0;i < PROP_PARM_COUNT;++i)
	{
		PROP_STRING_PARM(n, i);
		if (*n != 0)
			forbidarray->Push(FindClassTentative(n, RUNTIME_CLASS(AActor)));
	}
}

//==========================================================================
//
//==========================================================================
static void SetIcon(FTextureID &icon, Baggage &bag, const char *i)
{
	if (i == NULL || i[0] == '\0')
	{
		icon.SetNull();
	}
	else
	{
		icon = TexMan.CheckForTexture(i, ETextureType::MiscPatch);
		if (!icon.isValid())
		{
			// Don't print warnings if the item is for another game or if this is a shareware IWAD. 
			// Strife's teaser doesn't contain all the icon graphics of the full game.
			if ((bag.Info->ActorInfo()->GameFilter == GAME_Any || bag.Info->ActorInfo()->GameFilter & gameinfo.gametype) &&
				!(gameinfo.flags&GI_SHAREWARE) && fileSystem.GetFileContainer(bag.Lumpnum) != 0)
			{
				bag.ScriptPosition.Message(MSG_WARNING,
					"Icon '%s' for '%s' not found\n", i, bag.Info->TypeName.GetChars());
			}
		}
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(icon, S, Inventory)
{
	PROP_STRING_PARM(i, 0);
	SetIcon(defaults->TextureIDVar(NAME_Icon), bag, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(althudicon, S, Inventory)
{
	PROP_STRING_PARM(i, 0);
	SetIcon(defaults->TextureIDVar(NAME_AltHUDIcon), bag, i);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(defmaxamount, 0, Inventory)
{
	defaults->IntVar(NAME_MaxAmount) = gameinfo.definventorymaxamount;
}

//==========================================================================
// Dummy for Skulltag compatibility...
//==========================================================================
DEFINE_CLASS_PROPERTY(pickupannouncerentry, S, Inventory)
{
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(defaultkickback, 0, Weapon)
{
	defaults->IntVar(NAME_Kickback) = gameinfo.defKickback;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(bobstyle, S, Weapon)
{
	static const char *names[] = { "Normal", "Inverse", "Alpha", "InverseAlpha", "Smooth", "InverseSmooth", NULL };
	static const EBobStyle styles[] = { EBobStyle::BobNormal,
		EBobStyle::BobInverse, EBobStyle::BobAlpha, EBobStyle::BobInverseAlpha,
		EBobStyle::BobSmooth, EBobStyle::BobInverseSmooth, };
	PROP_STRING_PARM(id, 0);
	int match = MatchString(id, names);
	if (match < 0)
	{
		I_Error("Unknown bobstyle %s", id);
		match = 0;
	}
	defaults->IntVar(NAME_BobStyle) = (int)styles[match];
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(preferredskin, S, Weapon)
{
	PROP_STRING_PARM(str, 0);
	// NoOp - only for Skulltag compatibility
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(powerup, color, C_f, Inventory)
{
	static const char *specialcolormapnames[] = {
		"INVERSEMAP", "GOLDMAP", "REDMAP", "GREENMAP", "BLUEMAP", NULL };

	int alpha;
	PalEntry *pBlendColor;
	bool isgiver = info->IsDescendantOf(NAME_PowerupGiver);

	if (info->IsDescendantOf(NAME_Powerup) || isgiver)
	{
		pBlendColor = &defaults->ColorVar(NAME_BlendColor);
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

		// We must check the old special colormap names for compatibility
		int v = MatchString(name, specialcolormapnames);
		if (v >= 0)
		{
			*pBlendColor = MakeSpecialColormap(v);
			return;
		}
		else if (!stricmp(name, "none") && isgiver)
		{
			*pBlendColor = MakeSpecialColormap(65535);
			return;
		}
		color = V_GetColor(name, &bag.ScriptPosition);
	}
	if (PROP_PARM_COUNT > 2)
	{
		PROP_DOUBLE_PARM(falpha, 2);
		alpha=int(falpha*255);
	}
	else alpha = 255/3;

	alpha=clamp<int>(alpha, 0, 255);
	if (alpha != 0) *pBlendColor = MAKEARGB(alpha, 0, 0, 0) | color;
	else *pBlendColor = 0;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(powerup, colormap, FFFfff, Inventory)
{
	PalEntry BlendColor;

	if (!info->IsDescendantOf(NAME_Powerup) && !info->IsDescendantOf(NAME_PowerupGiver))
	{
		I_Error("\"powerup.colormap\" requires an actor of type \"Powerup\"\n");
		return;
	}

	if (PROP_PARM_COUNT == 3)
	{
		PROP_FLOAT_PARM(r, 0);
		PROP_FLOAT_PARM(g, 1);
		PROP_FLOAT_PARM(b, 2);
		BlendColor = MakeSpecialColormap(AddSpecialColormap(GPalette.BaseColors, 0, 0, 0, r, g, b));
	}
	else if (PROP_PARM_COUNT == 6)
	{
		PROP_FLOAT_PARM(r1, 0);
		PROP_FLOAT_PARM(g1, 1);
		PROP_FLOAT_PARM(b1, 2);
		PROP_FLOAT_PARM(r2, 3);
		PROP_FLOAT_PARM(g2, 4);
		PROP_FLOAT_PARM(b2, 5);
		BlendColor = MakeSpecialColormap(AddSpecialColormap(GPalette.BaseColors, r1, g1, b1, r2, g2, b2));
	}
	else
	{
		I_Error("\"power.colormap\" must have either 3 or 6 parameters\n");
	}
	defaults->ColorVar(NAME_BlendColor) = BlendColor;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(powerup, duration, I, Inventory)
{
	if (!info->IsDescendantOf(NAME_Powerup) && !info->IsDescendantOf(NAME_PowerupGiver))
	{
		I_Error("\"powerup.duration\" requires an actor of type \"Powerup\"\n");
		return;
	}

	PROP_INT_PARM(i, 0);
	defaults->IntVar(NAME_EffectTics) = (i >= 0) ? i : -i * TICRATE;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(powerup, type, S, PowerupGiver)
{
	PROP_STRING_PARM(str, 0);

	// Yuck! What was I thinking when I decided to prepend "Power" to the name? 
	// Now it's too late to change it...
	PClassActor *cls = PClass::FindActor(str);
	auto pow = PClass::FindActor(NAME_Powerup);
	if (cls == nullptr || !cls->IsDescendantOf(pow))
	{
		if (bag.fromDecorate)
		{
			FString st;
			st.Format("%s%s", strnicmp(str, "power", 5) ? "Power" : "", str);
			cls = FindClassTentative(st.GetChars(), pow);
		}
		else
		{
			I_Error("Unknown powerup type %s", str);
		}
	}
	defaults->PointerVar<PClassActor>(NAME_PowerupType) = cls;
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
	info->ActorInfo()->DisplayName = str;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, soundclass, S, PlayerPawn)
{
	PROP_STRING_PARM(str, 0);

	FString tmp = str;
	tmp.ReplaceChars (' ', '_');
	defaults->NameVar(NAME_SoundClass) = tmp.IsNotEmpty()? FName(tmp) : NAME_None;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, face, S, PlayerPawn)
{
	PROP_STRING_PARM(str, 0);
	FString tmp = str;

	if (tmp.Len() == 0) defaults->NameVar(NAME_Face) = NAME_None;
	else
	{
		tmp.ToUpper();
		bool valid = (tmp.Len() == 3 &&
			(((tmp[0] >= 'A') && (tmp[0] <= 'Z')) || ((tmp[0] >= '0') && (tmp[0] <= '9'))) &&
			(((tmp[1] >= 'A') && (tmp[1] <= 'Z')) || ((tmp[1] >= '0') && (tmp[1] <= '9'))) &&
			(((tmp[2] >= 'A') && (tmp[2] <= 'Z')) || ((tmp[2] >= '0') && (tmp[2] <= '9')))
			);
		if (!valid)
		{
			bag.ScriptPosition.Message(MSG_OPTERROR,
				"Invalid face '%s' for '%s';\nSTF replacement codes must be 3 alphanumeric characters.\n",
				tmp.GetChars(), info->TypeName.GetChars());
		}
		defaults->NameVar(NAME_Face) = tmp;
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, colorrange, I_I, PlayerPawn)
{
	PROP_INT_PARM(start, 0);
	PROP_INT_PARM(end, 1);

	if (start > end)
		std::swap (start, end);

	defaults->IntVar(NAME_ColorRangeStart) = start;
	defaults->IntVar(NAME_ColorRangeEnd) = end;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, colorset, ISIIIiiiiiiiiiiiiiiiiiiiiiiii, PlayerPawn)
{
	PROP_INT_PARM(setnum, 0);
	PROP_STRING_PARM(setname, 1);
	PROP_INT_PARM(rangestart, 2);
	PROP_INT_PARM(rangeend, 3);
	PROP_INT_PARM(representative_color, 4);

	FPlayerColorSet color;
	color.Name = setname;
	color.Lump = -1;
	color.FirstColor = rangestart;
	color.LastColor = rangeend;
	color.RepresentativeColor = representative_color;
	color.NumExtraRanges = 0;

	if (PROP_PARM_COUNT > 5)
	{
		int count = PROP_PARM_COUNT - 5;
		int start = 5;

		while (count >= 4)
		{
			PROP_INT_PARM(range_start, start+0);
			PROP_INT_PARM(range_end, start+1);
			PROP_INT_PARM(first_color, start+2);
			PROP_INT_PARM(last_color, start+3);
			int extra = color.NumExtraRanges++;
			assert (extra < (int)countof(color.Extra));

			color.Extra[extra].RangeStart = range_start;
			color.Extra[extra].RangeEnd = range_end;
			color.Extra[extra].FirstColor = first_color;
			color.Extra[extra].LastColor = last_color;
			count -= 4;
			start += 4;
		}
		if (count != 0)
		{
			bag.ScriptPosition.Message(MSG_OPTERROR, "Extra ranges require 4 parameters each.\n");
		}
	}

	if (setnum < 0)
	{
		bag.ScriptPosition.Message(MSG_OPTERROR, "Color set number must not be negative.\n");
	}
	else
	{
		ColorSets.Push(std::make_tuple(info, setnum, color));
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, colorsetfile, ISSI, PlayerPawn)
{
	PROP_INT_PARM(setnum, 0);
	PROP_STRING_PARM(setname, 1);
	PROP_STRING_PARM(rangefile, 2);
	PROP_INT_PARM(representative_color, 3);

	FPlayerColorSet color;
	color.Name = setname;
	color.Lump = fileSystem.CheckNumForName(rangefile);
	color.RepresentativeColor = representative_color;
	color.NumExtraRanges = 0;

	if (setnum < 0)
	{
		bag.ScriptPosition.Message(MSG_OPTERROR, "Color set number must not be negative.\n");
	}
	else if (color.Lump >= 0)
	{
		ColorSets.Push(std::make_tuple(info, setnum, color));
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, clearcolorset, I, PlayerPawn)
{
	PROP_INT_PARM(setnum, 0);

	if (setnum < 0)
	{
		bag.ScriptPosition.Message(MSG_OPTERROR, "Color set number must not be negative.\n");
	}
	else
	{
		FPlayerColorSet color;
		memset(&color, 0, sizeof(color));
		ColorSets.Push(std::make_tuple(info, setnum, color));
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, spawnclass, L, PlayerPawn)
{
	PROP_INT_PARM(type, 0);

	int &SpawnMask = defaults->IntVar(NAME_SpawnMask);
	if (type == 0)
	{
		PROP_INT_PARM(val, 1);
		if (val > 0) SpawnMask |= 1<<(val-1);
	}
	else 
	{
		for(int i=1; i<PROP_PARM_COUNT; i++)
		{
			PROP_STRING_PARM(str, i);

			if (!stricmp(str, "Any"))
				SpawnMask = 0;
			else if (!stricmp(str, "Fighter"))
				SpawnMask |= 1;
			else if (!stricmp(str, "Cleric"))
				SpawnMask |= 2;
			else if (!stricmp(str, "Mage"))
				SpawnMask |= 4;

		}
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, forwardmove, F_f, PlayerPawn)
{
	PROP_DOUBLE_PARM(m, 0);
	defaults->FloatVar(NAME_ForwardMove1) = defaults->FloatVar(NAME_ForwardMove2) = m;
	if (PROP_PARM_COUNT > 1)
	{
		PROP_DOUBLE_PARM(m2, 1);
		defaults->FloatVar(NAME_ForwardMove2) = m2;
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, sidemove, F_f, PlayerPawn)
{
	PROP_DOUBLE_PARM(m, 0);
	defaults->FloatVar(NAME_SideMove1) = defaults->FloatVar(NAME_SideMove2) = m;
	if (PROP_PARM_COUNT > 1)
	{
		PROP_DOUBLE_PARM(m2, 1);
		defaults->FloatVar(NAME_SideMove2) = m2;
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, scoreicon, S, PlayerPawn)
{
	PROP_STRING_PARM(z, 0);
	auto icon = TexMan.CheckForTexture(z, ETextureType::MiscPatch);
	defaults->IntVar(NAME_ScoreIcon) = icon.GetIndex();
	if (!icon.isValid())
	{
		bag.ScriptPosition.Message(MSG_WARNING,
			"Icon '%s' for '%s' not found\n", z, info->TypeName.GetChars ());
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
		defaults->IntVar(NAME_crouchsprite) = GetSpriteIndex (z);
	}
	else if (*z == 0)
	{
		defaults->IntVar(NAME_crouchsprite) = 0;
	}
	else
	{
		I_Error("Sprite name must have exactly 4 characters");
	}
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, damagescreencolor, Cfs, PlayerPawn)
{
	PROP_COLOR_PARM(c, 0, &bag.ScriptPosition);

	PalEntry color = c;

	if (PROP_PARM_COUNT < 3)		// Because colors count as 2 parms
	{
		color.a = 255;
		defaults->IntVar(NAME_DamageFade) = color;
	}
	else if (PROP_PARM_COUNT < 4)
	{
		PROP_DOUBLE_PARM(a, 2);

		color.a = uint8_t(255 * clamp<double>(a, 0.f, 1.f));
		defaults->IntVar(NAME_DamageFade) = color;
	}
	else
	{
		PROP_DOUBLE_PARM(a, 2);
		PROP_STRING_PARM(type, 3);

		color.a = uint8_t(255 * clamp<double>(a, 0.f, 1.f));
		PainFlashes.Push(std::make_tuple(info, type, color));
	}
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

	FDropItem *di = (FDropItem*)ClassDataAllocator.Alloc(sizeof(FDropItem));

	di->Name = str;
	di->Probability = 255;
	di->Amount = 1;
	if (PROP_PARM_COUNT > 1)
	{
		PROP_INT_PARM(amt, 1);
		di->Amount = amt;
	}
	di->Next = bag.DropItemList;
	bag.DropItemList = di;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY_PREFIX(player, hexenarmor, FFFFF, PlayerPawn)
{
	for (int i = 0; i < 5; i++)
	{
		PROP_DOUBLE_PARM(val, i);
		auto hexarmor = &defaults->FloatVar(NAME_HexenArmor);
		hexarmor[i] = val;
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
		FName *slots = &defaults->NameVar(NAME_Slot);
		slots[slot] = weapons.IsEmpty()? NAME_None : FName(weapons);
	}
}

//==========================================================================
// (non-fatal with non-existent types only in DECORATE)
//==========================================================================
DEFINE_CLASS_PROPERTY(playerclass, S, MorphProjectile)
{
	PROP_STRING_PARM(str, 0);
	defaults->PointerVar<PClassActor>(NAME_PlayerClass) = FindClassTentative(str, RUNTIME_CLASS(AActor), bag.fromDecorate);
}

//==========================================================================
// (non-fatal with non-existent types only in DECORATE)
//==========================================================================
DEFINE_CLASS_PROPERTY(monsterclass, S, MorphProjectile)
{
	PROP_STRING_PARM(str, 0);
	defaults->PointerVar<PClassActor>(NAME_MonsterClass) = FindClassTentative(str, RUNTIME_CLASS(AActor), bag.fromDecorate);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(duration, I, MorphProjectile)
{
	PROP_INT_PARM(i, 0);
	defaults->IntVar(NAME_Duration) = i >= 0 ? i : -i*TICRATE;
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(morphstyle, M, MorphProjectile)
{
	PROP_INT_PARM(i, 0);
	defaults->IntVar(NAME_MorphStyle) = i;
}

//==========================================================================
// (non-fatal with non-existent types only in DECORATE)
//==========================================================================
DEFINE_CLASS_PROPERTY(morphflash, S, MorphProjectile)
{
	PROP_STRING_PARM(str, 0);
	defaults->PointerVar<PClassActor>(NAME_MorphFlash) = FindClassTentative(str, RUNTIME_CLASS(AActor), bag.fromDecorate);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(unmorphflash, S, MorphProjectile)
{
	PROP_STRING_PARM(str, 0);
	defaults->PointerVar<PClassActor>(NAME_UnMorphFlash) = FindClassTentative(str, RUNTIME_CLASS(AActor), bag.fromDecorate);
}

//==========================================================================
// (non-fatal with non-existent types only in DECORATE)
//==========================================================================
DEFINE_CLASS_PROPERTY(playerclass, S, PowerMorph)
{
	PROP_STRING_PARM(str, 0);
	defaults->PointerVar<PClassActor>(NAME_PlayerClass) = FindClassTentative(str, RUNTIME_CLASS(AActor), bag.fromDecorate);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(monsterclass, S, PowerMorph)
{
	PROP_STRING_PARM(str, 0);
	defaults->PointerVar<PClassActor>(NAME_MonsterClass) = FindClassTentative(str, RUNTIME_CLASS(AActor), bag.fromDecorate);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(morphstyle, M, PowerMorph)
{
	PROP_INT_PARM(i, 0);
	defaults->IntVar(NAME_MorphStyle) = i;
}

//==========================================================================
// (non-fatal with non-existent types only in DECORATE)
//==========================================================================
DEFINE_CLASS_PROPERTY(morphflash, S, PowerMorph)
{
	PROP_STRING_PARM(str, 0);
	defaults->PointerVar<PClassActor>(NAME_MorphFlash) = FindClassTentative(str, RUNTIME_CLASS(AActor), bag.fromDecorate);
}

//==========================================================================
// (non-fatal with non-existent types only in DECORATE)
//==========================================================================
DEFINE_CLASS_PROPERTY(unmorphflash, S, PowerMorph)
{
	PROP_STRING_PARM(str, 0);
	defaults->PointerVar<PClassActor>(NAME_UnMorphFlash) = FindClassTentative(str, RUNTIME_CLASS(AActor), bag.fromDecorate);
}

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(type, S, DynamicLight)
{
	PROP_STRING_PARM(str, 0);
	static const char * ltype_names[]={
		"Point","Pulse","Flicker","Sector","RandomFlicker", "ColorPulse", "ColorFlicker", "RandomColorFlicker", nullptr};
	
	static const int ltype_values[]={
		PointLight, PulseLight, FlickerLight, SectorLight, RandomFlickerLight, ColorPulseLight, ColorFlickerLight, RandomColorFlickerLight };
	
	int style = MatchString(str, ltype_names);
	if (style < 0) I_Error("Unknown light type '%s'", str);
	defaults->IntVar(NAME_lighttype) = ltype_values[style];
}


