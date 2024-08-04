/*
** g_skill.cpp
** Skill level handling
**
**---------------------------------------------------------------------------
** Copyright 2008-2009 Christoph Oelckers
** Copyright 2008-2009 Randy Heit
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

#include <ctype.h>
#include "doomstat.h"
#include "d_player.h"
#include "g_game.h"
#include "gi.h"
#include "v_font.h"
#include "gstrings.h"
#include "g_levellocals.h"
#include "vm.h"

TArray<FSkillInfo> AllSkills;
int DefaultSkill = -1;

//==========================================================================
//
// ParseSkill
//
//==========================================================================

void FMapInfoParser::ParseSkill ()
{
	FSkillInfo skill;
	bool thisisdefault = false;
	bool acsreturnisset = false;

	skill.NoMenu = false;
	skill.AmmoFactor = 1.;
	skill.DoubleAmmoFactor = 2.;
	skill.DropAmmoFactor = -1.;
	skill.DamageFactor = 1.;
	skill.ArmorFactor = 1.;
	skill.HealthFactor = 1.;
	skill.KickbackFactor = 1.;
	skill.FastMonsters = false;
	skill.SlowMonsters = false;
	skill.DisableCheats = false;
	skill.EasyBossBrain = false;
	skill.EasyKey = false;
	skill.AutoUseHealth = false;
	skill.RespawnCounter = 0;
	skill.RespawnLimit = 0;
	skill.Aggressiveness = 1.;
	skill.SpawnFilter = 0;
	skill.SpawnMulti = false;
	skill.InstantReaction = false;
	skill.ACSReturn = 0;
	skill.MustConfirm = false;
	skill.Shortcut = 0;
	skill.TextColor = "";
	skill.Replace.Clear();
	skill.Replaced.Clear();
	skill.MonsterHealth = 1.;
	skill.FriendlyHealth = 1.;
	skill.NoPain = false;
	skill.Infighting = 0;
	skill.PlayerRespawn = false;

	sc.MustGetString();
	skill.Name = sc.String;

	ParseOpenBrace();

	while (sc.GetString ())
	{
		if (sc.Compare ("ammofactor"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.AmmoFactor = sc.Float;
		}
		else if (sc.Compare ("doubleammofactor"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.DoubleAmmoFactor = sc.Float;
		}
		else if (sc.Compare ("dropammofactor"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.DropAmmoFactor = sc.Float;
		}
		else if (sc.Compare ("damagefactor"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.DamageFactor = sc.Float;
		}
		else if (sc.Compare("kickbackfactor"))
		{
			ParseAssign();
			sc.MustGetFloat();
			skill.KickbackFactor = sc.Float;
		}
		else if (sc.Compare ("fastmonsters"))
		{
			skill.FastMonsters = true;
		}
		else if (sc.Compare ("slowmonsters"))
		{
			skill.SlowMonsters = true;
		}
		else if (sc.Compare ("disablecheats"))
		{
			skill.DisableCheats = true;
		}
		else if (sc.Compare ("easybossbrain"))
		{
			skill.EasyBossBrain = true;
		}
		else if (sc.Compare ("easykey"))
		{
			skill.EasyKey = true;
		}
		else if (sc.Compare("autousehealth"))
		{
			skill.AutoUseHealth = true;
		}
		else if (sc.Compare("nomenu"))
		{
			skill.NoMenu = true;
		}
		else if (sc.Compare ("playerrespawn"))
		{
			skill.PlayerRespawn = true;
		}
		else if (sc.Compare("respawntime"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.RespawnCounter = int(sc.Float*TICRATE);
		}
		else if (sc.Compare("respawnlimit"))
		{
			ParseAssign();
			sc.MustGetNumber ();
			skill.RespawnLimit = sc.Number;
		}
		else if (sc.Compare("Aggressiveness"))
		{
			ParseAssign();
			sc.MustGetFloat ();
			skill.Aggressiveness = 1. - clamp(sc.Float, 0.,1.);
		}
		else if (sc.Compare("SpawnFilter"))
		{
			ParseAssign();
			if (sc.CheckNumber())
			{
				if (sc.Number > 0) skill.SpawnFilter |= (1<<(sc.Number-1));
			}
			else
			{
				sc.MustGetString ();
				if (sc.Compare("baby")) skill.SpawnFilter |= 1;
				else if (sc.Compare("easy")) skill.SpawnFilter |= 2;
				else if (sc.Compare("normal")) skill.SpawnFilter |= 4;
				else if (sc.Compare("hard")) skill.SpawnFilter |= 8;
				else if (sc.Compare("nightmare")) skill.SpawnFilter |= 16;
			}
		}
		else if (sc.Compare ("spawnmulti"))
		{
			skill.SpawnMulti = true;
		}
		else if (sc.Compare ("InstantReaction"))
		{
			skill.InstantReaction = true;
		}
		else if (sc.Compare("ACSReturn"))
		{
			ParseAssign();
			sc.MustGetNumber ();
			skill.ACSReturn = sc.Number;
			acsreturnisset = true;
		}
		else if (sc.Compare("ReplaceActor"))
		{
			ParseAssign();
			sc.MustGetString();
			FName replaced = sc.String;
			ParseComma();
			sc.MustGetString();
			FName replacer = sc.String;
			skill.SetReplacement(replaced, replacer);
			skill.SetReplacedBy(replacer, replaced);
		}
		else if (sc.Compare("Name"))
		{
			ParseAssign();
			sc.MustGetString ();
			skill.MenuName = strbin1(sc.String);
		}
		else if (sc.Compare("PlayerClassName"))
		{
			ParseAssign();
			sc.MustGetString ();
			FName pc = sc.String;
			ParseComma();
			sc.MustGetString ();
			skill.MenuNamesForPlayerClass[pc]=sc.String;
		}
		else if (sc.Compare("PicName"))
		{
			ParseAssign();
			sc.MustGetString ();
			skill.PicName = sc.String;
		}
		else if (sc.Compare("MustConfirm"))
		{
			skill.MustConfirm = true;
			if (format_type == FMT_New) 
			{
				if (CheckAssign())
				{
					sc.MustGetString();
					skill.MustConfirmText = sc.String;
				}
			}
			else
			{
				if (sc.CheckToken(TK_StringConst))
				{
					skill.MustConfirmText = sc.String;
				}
			}
		}
		else if (sc.Compare("Key"))
		{
			ParseAssign();
			sc.MustGetString();
			skill.Shortcut = tolower(sc.String[0]);
		}
		else if (sc.Compare("TextColor"))
		{
			ParseAssign();
			sc.MustGetString();
			skill.TextColor.Format("[%s]", sc.String);
		}
		else if (sc.Compare("MonsterHealth"))
		{
			ParseAssign();
			sc.MustGetFloat();
			skill.MonsterHealth = sc.Float;	
		}
		else if (sc.Compare("FriendlyHealth"))
		{
			ParseAssign();
			sc.MustGetFloat();
			skill.FriendlyHealth = sc.Float;
		}
		else if (sc.Compare("NoPain"))
		{
			skill.NoPain = true;
		}
		else if (sc.Compare("ArmorFactor"))
		{
			ParseAssign();
			sc.MustGetFloat();
			skill.ArmorFactor = sc.Float;
		}
		else if (sc.Compare("HealthFactor"))
		{
			ParseAssign();
			sc.MustGetFloat();
			skill.HealthFactor = sc.Float;
		}
		else if (sc.Compare("NoInfighting"))
		{
			skill.Infighting = LEVEL2_NOINFIGHTING;
		}
		else if (sc.Compare("TotalInfighting"))
		{
			skill.Infighting = LEVEL2_TOTALINFIGHTING;
		}
		else if (sc.Compare("DefaultSkill"))
		{
			thisisdefault = true;
		}
		else if (!ParseCloseBrace())
		{
			// Unknown
			sc.ScriptMessage("Unknown property '%s' found in skill definition\n", sc.String);
			SkipToNext();
		}
		else
		{
			break;
		}
	}
	CheckEndOfFile("skill");
	for(unsigned int i = 0; i < AllSkills.Size(); i++)
	{
		if (AllSkills[i].Name == skill.Name)
		{
			if (!acsreturnisset)
			{ // Use the ACS return for the skill we are overwriting.
				skill.ACSReturn = AllSkills[i].ACSReturn;
			}
			AllSkills[i] = skill;
			if (thisisdefault)
			{
				DefaultSkill = i;
			}
			return;
		}
	}
	if (!acsreturnisset)
	{
		skill.ACSReturn = AllSkills.Size();
	}
	if (thisisdefault)
	{
		DefaultSkill = AllSkills.Size();
	}
	AllSkills.Push(skill);
}

//==========================================================================
//
//
//
//==========================================================================

int G_SkillProperty(ESkillProperty prop)
{
	if (AllSkills.Size() > 0)
	{
		switch(prop)
		{
		case SKILLP_FastMonsters:
			return AllSkills[gameskill].FastMonsters  || (dmflags & DF_FAST_MONSTERS);

		case SKILLP_SlowMonsters:
			return AllSkills[gameskill].SlowMonsters;

		case SKILLP_Respawn:
			if (dmflags & DF_MONSTERS_RESPAWN && AllSkills[gameskill].RespawnCounter==0) 
				return TICRATE * gameinfo.defaultrespawntime;
			return AllSkills[gameskill].RespawnCounter;

		case SKILLP_RespawnLimit:
			return AllSkills[gameskill].RespawnLimit;

		case SKILLP_DisableCheats:
			return AllSkills[gameskill].DisableCheats;

		case SKILLP_AutoUseHealth:
			return AllSkills[gameskill].AutoUseHealth;

		case SKILLP_EasyBossBrain:
			return AllSkills[gameskill].EasyBossBrain;

		case SKILLP_EasyKey:
			return AllSkills[gameskill].EasyKey;

		case SKILLP_SpawnFilter:
			return AllSkills[gameskill].SpawnFilter;

		case SKILLP_ACSReturn:
			return AllSkills[gameskill].ACSReturn;
		
		case SKILLP_NoPain:			
			return AllSkills[gameskill].NoPain;	

		case SKILLP_Infight:
			if (AllSkills[gameskill].Infighting == LEVEL2_TOTALINFIGHTING) return 1;
			if (AllSkills[gameskill].Infighting == LEVEL2_NOINFIGHTING) return -1;
			return infighting;

		case SKILLP_PlayerRespawn:
			return AllSkills[gameskill].PlayerRespawn;

		case SKILLP_SpawnMulti:
			return AllSkills[gameskill].SpawnMulti;

		case SKILLP_InstantReaction:
			return AllSkills[gameskill].InstantReaction;
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DObject, G_SkillPropertyInt)
{
	PARAM_PROLOGUE;
	PARAM_INT(which);
	ACTION_RETURN_INT(G_SkillProperty((ESkillProperty)which));
}

//==========================================================================
//
//
//
//==========================================================================

double G_SkillProperty(EFSkillProperty prop)
{
	if (AllSkills.Size() > 0)
	{
		switch (prop)
		{
		case SKILLP_AmmoFactor:
			if (dmflags2 & DF2_YES_DOUBLEAMMO)
			{
				return AllSkills[gameskill].DoubleAmmoFactor;
			}
			return AllSkills[gameskill].AmmoFactor;

		case SKILLP_DropAmmoFactor:
			return AllSkills[gameskill].DropAmmoFactor;

		case SKILLP_ArmorFactor:
			return AllSkills[gameskill].ArmorFactor;

		case SKILLP_HealthFactor:
			return AllSkills[gameskill].HealthFactor;

		case SKILLP_DamageFactor:
			return AllSkills[gameskill].DamageFactor;

		case SKILLP_Aggressiveness:
			return AllSkills[gameskill].Aggressiveness;

		case SKILLP_MonsterHealth:
			return AllSkills[gameskill].MonsterHealth;

		case SKILLP_FriendlyHealth:
			return AllSkills[gameskill].FriendlyHealth;

		case SKILLP_KickbackFactor:
			return AllSkills[gameskill].KickbackFactor;

		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DObject, G_SkillPropertyFloat)
{
	PARAM_PROLOGUE;
	PARAM_INT(which);
	ACTION_RETURN_FLOAT(G_SkillProperty((EFSkillProperty)which));
}



//==========================================================================
//
//
//
//==========================================================================

const char * G_SkillName()
{
	const char *name = AllSkills[gameskill].MenuName.GetChars();

	player_t *player = &players[consoleplayer];
	const char *playerclass = player->mo->GetInfo()->DisplayName.GetChars();

	if (playerclass != NULL)
	{
		FString * pmnm = AllSkills[gameskill].MenuNamesForPlayerClass.CheckKey(playerclass);
		if (pmnm != NULL) name = pmnm->GetChars();
	}

	if (*name == '$') name = GStrings.GetString(name+1);
	return name;
}

DEFINE_ACTION_FUNCTION(DObject, G_SkillName)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_STRING(G_SkillName());
}

//==========================================================================
//
//
//
//==========================================================================

void G_VerifySkill()
{
	if (gameskill >= (int)AllSkills.Size())
		gameskill = AllSkills.Size()-1;
	else if (gameskill < 0)
		gameskill = 0;
}

//==========================================================================
//
//
//
//==========================================================================

FSkillInfo &FSkillInfo::operator=(const FSkillInfo &other)
{
	Name = other.Name;
	AmmoFactor = other.AmmoFactor;
	NoMenu = other.NoMenu;
	DoubleAmmoFactor = other.DoubleAmmoFactor;
	DropAmmoFactor = other.DropAmmoFactor;
	DamageFactor = other.DamageFactor;
	KickbackFactor = other.KickbackFactor;
	FastMonsters = other.FastMonsters;
	SlowMonsters = other.SlowMonsters;
	DisableCheats = other.DisableCheats;
	AutoUseHealth = other.AutoUseHealth;
	EasyBossBrain = other.EasyBossBrain;
	EasyKey = other.EasyKey;
	RespawnCounter= other.RespawnCounter;
	RespawnLimit= other.RespawnLimit;
	Aggressiveness= other.Aggressiveness;
	SpawnFilter = other.SpawnFilter;
	SpawnMulti = other.SpawnMulti;
	InstantReaction = other.InstantReaction;
	ACSReturn = other.ACSReturn;
	MenuName = other.MenuName;
	PicName = other.PicName;
	MenuNamesForPlayerClass = other.MenuNamesForPlayerClass;
	MustConfirm = other.MustConfirm;
	MustConfirmText = other.MustConfirmText;
	Shortcut = other.Shortcut;
	TextColor = other.TextColor;
	Replace = other.Replace;
	Replaced = other.Replaced;
	MonsterHealth = other.MonsterHealth;
	FriendlyHealth = other.FriendlyHealth;
	NoPain = other.NoPain;
	Infighting = other.Infighting;
	ArmorFactor = other.ArmorFactor;
	HealthFactor = other.HealthFactor;
	PlayerRespawn = other.PlayerRespawn;
	return *this;
}

//==========================================================================
//
//
//
//==========================================================================

int FSkillInfo::GetTextColor() const
{
	if (TextColor.IsEmpty())
	{
		return CR_UNTRANSLATED;
	}
	const uint8_t *cp = (const uint8_t *)TextColor.GetChars();
	int color = V_ParseFontColor(cp, 0, 0);
	if (color == CR_UNDEFINED)
	{
		Printf("Undefined color '%s' in definition of skill %s\n", TextColor.GetChars(), Name.GetChars());
		color = CR_UNTRANSLATED;
	}
	return color;
}

//==========================================================================
//
// FSkillInfo::SetReplacement
//
//==========================================================================

void FSkillInfo::SetReplacement(FName a, FName b)
{
	Replace[a] = b;
}

//==========================================================================
//
// FSkillInfo::GetReplacement
//
//==========================================================================

FName FSkillInfo::GetReplacement(FName a)
{
	if (Replace.CheckKey(a)) return Replace[a];
	else return NAME_None;
}

//==========================================================================
//
// FSkillInfo::SetReplaced
//
//==========================================================================

void FSkillInfo::SetReplacedBy(FName b, FName a)
{
	Replaced[b] = a;
}

//==========================================================================
//
// FSkillInfo::GetReplaced
//
//==========================================================================

FName FSkillInfo::GetReplacedBy(FName b)
{
	if (Replaced.CheckKey(b)) return Replaced[b];
	else return NAME_None;
}
