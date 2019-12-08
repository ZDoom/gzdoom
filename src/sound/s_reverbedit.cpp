/*
**
** reverb editor
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2017 Christoph Oelckers
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

#include "doomtype.h"
#include "s_sound.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "templates.h"
#include "w_wad.h"
#include "i_system.h"
#include "m_misc.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "vm.h"
#include "dobject.h"
#include "menu/menu.h"

void S_ReadReverbDef (FScanner &sc);

extern ReverbContainer *ForcedEnvironment;
ReverbContainer *CurrentEnv;
REVERB_PROPERTIES SavedProperties;

extern FReverbField ReverbFields[];
extern const char* ReverbFieldNames[];
extern int NumReverbs;


// These are for internal use only and not supposed to be user-settable
CVAR(String, reverbedit_name, "", CVAR_NOSET);
CVAR(Int, reverbedit_id1, 0, CVAR_NOSET);
CVAR(Int, reverbedit_id2, 0, CVAR_NOSET);
CVAR(String, reverbsavename, "", 0);


CUSTOM_CVAR(Bool, eaxedit_test, false, CVAR_NOINITCALL)
{
	if (self)
	{
		ForcedEnvironment = CurrentEnv;
	}
	else
	{
		ForcedEnvironment = nullptr;
	}
}

struct EnvFlag
{
	const char *Name;
	int CheckboxControl;
	unsigned int Flag;
};

inline int HIBYTE(int i)
{
	return (i >> 8) & 255;
}

inline int LOBYTE(int i)
{
	return i & 255;
}

uint16_t FirstFreeID(uint16_t base, bool builtin)
{
	int tryCount = 0;
	int priID = HIBYTE(base);

	// If the original sound is built-in, start searching for a new
	// primary ID at 30.
	if (builtin)
	{
		for (priID = 30; priID < 256; ++priID)
		{
			if (S_FindEnvironment(priID << 8) == nullptr)
			{
				break;
			}
		}
		if (priID == 256)
		{ // Oh well.
			priID = 30;
		}
	}

	for (;;)
	{
		uint16_t lastID = Environments->ID;
		const ReverbContainer *env = Environments->Next;

		// Find the lowest-numbered free ID with the same primary ID as base
		// If none are available, add 100 to base's primary ID and try again.
		// If that fails, then the primary ID gets incremented
		// by 1 until a match is found. If all the IDs searchable by this
		// algorithm are in use, then you're in trouble.

		while (env != nullptr)
		{
			if (HIBYTE(env->ID) > priID)
			{
				break;
			}
			if (HIBYTE(env->ID) == priID)
			{
				if (HIBYTE(lastID) == priID)
				{
					if (LOBYTE(env->ID) - LOBYTE(lastID) > 1)
					{
						return lastID + 1;
					}
				}
				lastID = env->ID;
			}
			env = env->Next;
		}
		if (LOBYTE(lastID) == 255)
		{
			if (tryCount == 0)
			{
				base += 100 * 256;
				tryCount = 1;
			}
			else
			{
				base += 256;
			}
		}
		else if (builtin && lastID == 0)
		{
			return priID << 8;
		}
		else
		{
			return lastID + 1;
		}
	}
}

FString SuggestNewName(const ReverbContainer *env)
{
	const ReverbContainer *probe = nullptr;
	char text[32];
	size_t len;
	int number, numdigits;

	strncpy(text, env->Name, 31);
	text[31] = 0;

	len = strlen(text);
	while (text[len - 1] >= '0' && text[len - 1] <= '9')
	{
		len--;
	}
	number = atoi(text + len);
	if (number < 1)
	{
		number = 1;
	}

	if (text[len - 1] != ' ' && len < 31)
	{
		text[len++] = ' ';
	}

	for (; number < 100000; ++number)
	{
		if (number < 10)	numdigits = 1;
		else if (number < 100)	numdigits = 2;
		else if (number < 1000)	numdigits = 3;
		else if (number < 10000)numdigits = 4;
		else					numdigits = 5;
		if (len + numdigits > 31)
		{
			len = 31 - numdigits;
		}
		mysnprintf(text + len, countof(text) - len, "%d", number);

		probe = Environments;
		while (probe != nullptr)
		{
			if (stricmp(probe->Name, text) == 0)
				break;
			probe = probe->Next;
		}
		if (probe == nullptr)
		{
			break;
		}
	}
	return text;
}

void ExportEnvironments(const char *filename, uint32_t count, const ReverbContainer **envs)
{
	FString dest = M_GetDocumentsPath() + filename;

	FileWriter *f = FileWriter::Open(dest);

	if (f != nullptr)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			const ReverbContainer *env = envs[i];
			const ReverbContainer *base;
			size_t j;

			if ((unsigned int)env->Properties.Environment < 26)
			{
				base = DefaultEnvironments[env->Properties.Environment];
			}
			else
			{
				base = nullptr;
			}
			f->Printf("\"%s\" %u %u\n{\n", env->Name, HIBYTE(env->ID), LOBYTE(env->ID));
			for (j = 0; j < NumReverbs; ++j)
			{
				const FReverbField *ctl = &ReverbFields[j];
				const char *ctlName = ReverbFieldNames[j];
				if (ctlName)
				{
					if (j == 0 ||
						(ctl->Float && base->Properties.*ctl->Float != env->Properties.*ctl->Float) ||
						(ctl->Int && base->Properties.*ctl->Int != env->Properties.*ctl->Int))
					{
						f->Printf("\t%s ", ctlName);
						if (ctl->Float)
						{
							float v = env->Properties.*ctl->Float * 1000;
							int vi = int(v >= 0.0 ? v + 0.5 : v - 0.5);
							f->Printf("%d.%03d\n", vi / 1000, abs(vi % 1000));
						}
						else
						{
							f->Printf("%d\n", env->Properties.*ctl->Int);
						}
					}
					else
					{
						if ((1 << ctl->Flag) & (env->Properties.Flags ^ base->Properties.Flags))
						{
							f->Printf("\t%s %s\n", ctlName, ctl->Flag & env->Properties.Flags ? "true" : "false");
						}
					}
				}
			}
			f->Printf("}\n\n");
		}
		delete f;
	}
	else
	{
		M_StartMessage("Save failed", 1);
	}
}

DEFINE_ACTION_FUNCTION(DReverbEdit, GetValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	float v = 0;

	if (index >= 0 && index < NumReverbs)
	{
		auto rev = &ReverbFields[index];
		if (rev->Int != nullptr)
		{
			v = float(CurrentEnv->Properties.*(rev->Int));
		}
		else if (rev->Float != nullptr)
		{
			v = CurrentEnv->Properties.*(rev->Float);
		}
		else
		{
			v = !!(CurrentEnv->Properties.Flags & (1 << int(rev->Flag)));
		}
	}
	ACTION_RETURN_FLOAT(v);
	return 1;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, SetValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	PARAM_FLOAT(v);

	if (index >= 0 && index < NumReverbs)
	{
		auto rev = &ReverbFields[index];
		if (rev->Int != nullptr)
		{
			v = CurrentEnv->Properties.*(rev->Int) = clamp<int>(int(v), rev->Min, rev->Max);
		}
		else if (rev->Float != nullptr)
		{
			v = CurrentEnv->Properties.*(rev->Float) = clamp<float>(float(v), rev->Min / 1000.f, rev->Max / 1000.f);
		}
		else
		{
			if (v == 0) CurrentEnv->Properties.Flags &= ~(1 << int(rev->Flag));
			else CurrentEnv->Properties.Flags |= (1 << int(rev->Flag));
		}
	}

	ACTION_RETURN_FLOAT(v);
	return 1;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, GrayCheck)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_BOOL(CurrentEnv->Builtin);
	return 1;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, GetSelectedEnvironment)
{
	PARAM_PROLOGUE;
	if (numret > 1)
	{
		numret = 2;
		ret[1].SetInt(CurrentEnv ? CurrentEnv->ID : -1);
	}
	if (numret > 0)
	{
		ret[0].SetString(CurrentEnv ? CurrentEnv->Name : "");
	}
	return numret;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, FillSelectMenu)
{
	PARAM_PROLOGUE;
	PARAM_STRING(ccmd);
	PARAM_OBJECT(desc, DOptionMenuDescriptor);
	desc->mItems.Clear();
	for (auto env = Environments; env != nullptr; env = env->Next)
	{
		FStringf text("(%d, %d) %s", HIBYTE(env->ID), LOBYTE(env->ID), env->Name);
		FStringf cmd("%s \"%s\"", ccmd.GetChars(), env->Name);
		PClass *cls = PClass::FindClass("OptionMenuItemCommand");
		if (cls != nullptr && cls->IsDescendantOf("OptionMenuItem"))
		{
			auto func = dyn_cast<PFunction>(cls->FindSymbol("Init", true));
			if (func != nullptr)
			{
				DMenuItemBase *item = (DMenuItemBase*)cls->CreateNew();
				VMValue params[] = { item, &text, FName(cmd).GetIndex(), false, true };
				VMCall(func->Variants[0].Implementation, params, 5, nullptr, 0);
				desc->mItems.Push((DMenuItemBase*)item);
			}
		}
	}
	return 0;
}

static TArray<std::pair<ReverbContainer*, bool>> SaveState;

DEFINE_ACTION_FUNCTION(DReverbEdit, FillSaveMenu)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(desc, DOptionMenuDescriptor);
	desc->mItems.Resize(4);
	SaveState.Clear();
	for (auto env = Environments; env != nullptr; env = env->Next)
	{
		if (!env->Builtin)
		{
			int index = (int)SaveState.Push(std::make_pair(env, false));

			FStringf text("(%d, %d) %s", HIBYTE(env->ID), LOBYTE(env->ID), env->Name);
			PClass *cls = PClass::FindClass("OptionMenuItemReverbSaveSelect");
			if (cls != nullptr && cls->IsDescendantOf("OptionMenuItem"))
			{
				auto func = dyn_cast<PFunction>(cls->FindSymbol("Init", true));
				if (func != nullptr)
				{
					DMenuItemBase *item = (DMenuItemBase*)cls->CreateNew();
					VMValue params[] = { item, &text, index, FName("OnOff").GetIndex() };
					VMCall(func->Variants[0].Implementation, params, 4, nullptr, 0);
					desc->mItems.Push((DMenuItemBase*)item);
				}
			}
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, GetSaveSelection)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	bool res = false;
	if ((unsigned)index <= SaveState.Size())
	{
		res = SaveState[index].second;
	}
	ACTION_RETURN_BOOL(res);
}

DEFINE_ACTION_FUNCTION(DReverbEdit, ToggleSaveSelection)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	if ((unsigned)index <= SaveState.Size())
	{
		SaveState[index].second = !SaveState[index].second;
	}
	return 0;
}


CCMD(savereverbs)
{
	if (SaveState.Size() == 0) return;

	TArray<const ReverbContainer*> toSave;

	for (auto &p : SaveState)
	{
		if (p.second) toSave.Push(p.first);
	}
	ExportEnvironments(reverbsavename, toSave.Size(), &toSave[0]);
	SaveState.Clear();
}

static void SelectEnvironment(const char *envname)
{
	for (auto env = Environments; env != nullptr; env = env->Next)
	{
		if (!strcmp(env->Name, envname))
		{
			CurrentEnv = env;
			SavedProperties = env->Properties;
			if (eaxedit_test) ForcedEnvironment = env;

			// Set up defaults for a new environment based on this one.
			int newid = FirstFreeID(env->ID, env->Builtin);
			UCVarValue cv;
			cv.Int = HIBYTE(newid);
			reverbedit_id1.ForceSet(cv, CVAR_Int);
			cv.Int = LOBYTE(newid);
			reverbedit_id2.ForceSet(cv, CVAR_Int);
			FString selectname = SuggestNewName(env);
			cv.String = selectname.GetChars();
			reverbedit_name.ForceSet(cv, CVAR_String);
			return;
		}
	}
}

void InitReverbMenu()
{
	// Make sure that the editor's variables are properly initialized.
	SelectEnvironment("Off");
}

CCMD(selectenvironment)
{
	if (argv.argc() > 1)
	{
		auto str = argv[1];
		SelectEnvironment(str);
	}
	else
		InitReverbMenu();
}

CCMD(revertenvironment)
{
	if (CurrentEnv != nullptr)
	{
		CurrentEnv->Properties = SavedProperties;
	}
}

CCMD(createenvironment)
{
	if (S_FindEnvironment(reverbedit_name))
	{
		M_StartMessage(FStringf("An environment with the name '%s' already exists", *reverbedit_name), 1);
		return;
	}
	int id = (reverbedit_id1 << 8) + reverbedit_id2;
	if (S_FindEnvironment(id))
	{
		M_StartMessage(FStringf("An environment with the ID (%d, %d) already exists", *reverbedit_id1, *reverbedit_id2), 1);
		return;
	}

	auto newenv = new ReverbContainer;
	newenv->Builtin = false;
	newenv->ID = id;
	newenv->Name = copystring(reverbedit_name);
	newenv->Next = nullptr;
	newenv->Properties = CurrentEnv->Properties;
	S_AddEnvironment(newenv);
	SelectEnvironment(newenv->Name);
}

CCMD(reverbedit)
{
	C_DoCommand("openmenu reverbedit");
}

// This is here because it depends on Doom's resource management and is not universal.
void S_ParseReverbDef ()
{
	int lump, lastlump = 0;

	while ((lump = Wads.FindLump ("REVERBS", &lastlump)) != -1)
	{
		FScanner sc;
		sc.OpenLumpNum(lump);
		S_ReadReverbDef (sc);;
	}
	InitReverbMenu();
}

