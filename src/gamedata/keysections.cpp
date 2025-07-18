/*
** keysections.cpp
** Custom key bindings
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright 2010 Christoph Oelckers
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


#include "menu.h"
#include "gi.h"
#include "c_bind.h"
#include "c_dispatch.h"
#include "gameconfigfile.h"
#include "filesystem.h"
#include "gi.h"
#include "d_player.h"
#include "c_dispatch.h"

TArray<FKeySection> KeySections;
extern TArray<FString> KeyConfWeapons;

static void LoadKeys (const char *modname, bool dbl)
{
	char section[64];

	mysnprintf (section, countof(section), "%s.%s%sBindings", gameinfo.ConfigName.GetChars(), modname,
		dbl ? ".Double" : ".");

	FKeyBindings *bindings = dbl? &DoubleBindings : &Bindings;
	if (GameConfig->SetSection (section))
	{
		const char *key, *value;
		while (GameConfig->NextInSection (key, value))
		{
			bindings->DoBind (key, value);
		}
	}
}

static void DoSaveKeys (FConfigFile *config, const char *section, FKeySection *keysection, bool dbl)
{
	config->SetSection (section, true);
	config->ClearCurrentSection ();
	FKeyBindings *bindings = dbl? &DoubleBindings : &Bindings;
	for (unsigned i = 0; i < keysection->mActions.Size(); ++i)
	{
		bindings->ArchiveBindings (config, keysection->mActions[i].mAction.GetChars());
	}
}

void M_SaveCustomKeys (FConfigFile *config, char *section, char *subsection, size_t sublen)
{
	for (unsigned i=0; i<KeySections.Size(); i++)
	{
		mysnprintf (subsection, sublen, "%s.Bindings", KeySections[i].mSection.GetChars());
		DoSaveKeys (config, section, &KeySections[i], false);
		mysnprintf (subsection, sublen, "%s.DoubleBindings", KeySections[i].mSection.GetChars());
		DoSaveKeys (config, section, &KeySections[i], true);
	}
}

static int CurrentKeySection = -1;

CCMD (addkeysection)
{
	if (ParsingKeyConf)
	{
		if (argv.argc() != 3)
		{
			Printf ("Usage: addkeysection <menu section name> <ini name>\n");
			return;
		}

		FString name(argv[2]);
		// Limit the ini name to 32 chars
		if (name.Len() > 32)
		{
			DPrintf(DMSG_ERROR, "WARNING: %s is too long as an ini name! The ini name should be 32 bytes or less.\n", &name[0]);
			name.Truncate(32);
		}

		for (unsigned i = 0; i < KeySections.Size(); i++)
		{
			if (KeySections[i].mTitle.CompareNoCase(name) == 0)
			{
				CurrentKeySection = i;
				return;
			}
		}

		CurrentKeySection = KeySections.Reserve(1);
		KeySections[CurrentKeySection].mTitle = argv[1];
		KeySections[CurrentKeySection].mSection = argv[2];
		// Load bindings for this section from the ini
		LoadKeys (argv[2], 0);
		LoadKeys (argv[2], 1);
	}
}

CCMD (addmenukey)
{
	if (ParsingKeyConf)
	{
		if (argv.argc() != 3)
		{
			Printf ("Usage: addmenukey <description> <command>\n");
			return;
		}
		if (CurrentKeySection == -1 || CurrentKeySection >= (int)KeySections.Size())
		{
			Printf ("You must use addkeysection first.\n");
			return;
		}

		FKeySection *sect = &KeySections[CurrentKeySection];

		FKeyAction *act = &sect->mActions[sect->mActions.Reserve(1)];
		act->mTitle = argv[1];
		act->mAction = argv[2];
	}
}

//==========================================================================
//
// D_LoadWadSettings
//
// Parses any loaded KEYCONF lumps. These are restricted console scripts
// that can only execute the alias, defaultbind, addkeysection,
// addmenukey, weaponsection, and addslotdefault commands.
//
//==========================================================================

void D_LoadWadSettings ()
{
	TArray<char> command;
	int lump, lastlump = 0;

	ParsingKeyConf = true;
	KeySections.Clear();
	KeyConfWeapons.Clear();

	while ((lump = fileSystem.FindLump ("KEYCONF", &lastlump)) != -1)
	{
		auto data = fileSystem.ReadFile (lump);
		const char* conf = data.string();
		const char *eof = conf + data.size();

		while (conf < eof)
		{
			ptrdiff_t linePos = 0;

			// Fetch a line to execute
			command.Clear();
			for (linePos = 0; (conf + linePos) < eof && conf[linePos] != '\n' && conf[linePos] != '\r'; ++linePos)
			{
				command.Push(conf[linePos]);
			}
			// Increment 'conf' pointer to next line
			conf += linePos;
			while (conf < eof && (*conf == '\n' || *conf == '\r'))
			{
				conf++;
			}

			// Does 'command' have a comment? If so, remove it.
			// Comments begin with //
			char *stop = &command[linePos];
			char *comment = &command[0];
			char prevChar = 0;
			bool inQuote = false;

			while (comment < stop)
			{
				// if (prevChar != '\\' && *comment == '\"')
				if (*comment == '\"')
				{
					inQuote = !inQuote;
				}
				else if (!inQuote && prevChar == '/' && *comment == '/')
				{
					comment--; // 'comment' is on the second slash
					break;
				}
				prevChar = *comment;
				comment++;
			}
			// 'comment' will either be the end of the string, or the starting
			// position of an inline comment.
			if ((comment - &command[0]) < linePos)
			{
				*comment = 0;
			}
			else
			{
				// Just in case 'comment' is at EOF
				command.Push(0);
			}
			// DPrintf(DMSG_ERROR, "command: %s\n", &command[0]);
			AddCommandString (&command[0]);
		}
	}
	ParsingKeyConf = false;
}

// Strict handling of SetSlot and ClearPlayerClasses in KEYCONF (see a_weapons.cpp)
EXTERN_CVAR (Bool, setslotstrict)

// Specifically hunt for and remove IWAD playerclasses
void ClearIWADPlayerClasses (PClassActor *ti)
{
	for(unsigned i=0; i < PlayerClasses.Size(); i++)
	{
		if(PlayerClasses[i].Type==ti)
		{
			for(unsigned j = i; j < PlayerClasses.Size()-1; j++)
			{
				PlayerClasses[j] = PlayerClasses[j+1];
			}
			PlayerClasses.Pop();
		}
	}
}

CCMD(clearplayerclasses)
{
	if (ParsingKeyConf)
	{	
		// Only clear the playerclasses first if setslotstrict is true
		// If not, we'll only remove the IWAD playerclasses
		if(setslotstrict)
			PlayerClasses.Clear();
		else
		{
			// I wish I had a better way to pick out IWAD playerclasses
			// without having to explicitly name them here...
			ClearIWADPlayerClasses(PClass::FindActor("DoomPlayer"));
			ClearIWADPlayerClasses(PClass::FindActor("HereticPlayer"));
			ClearIWADPlayerClasses(PClass::FindActor("StrifePlayer"));
			ClearIWADPlayerClasses(PClass::FindActor("FighterPlayer"));
			ClearIWADPlayerClasses(PClass::FindActor("ClericPlayer"));
			ClearIWADPlayerClasses(PClass::FindActor("MagePlayer"));
			ClearIWADPlayerClasses(PClass::FindActor("ChexPlayer"));
		}
	}
}

bool ValidatePlayerClass(PClassActor *ti, const char *name);

CCMD(addplayerclass)
{
	if (ParsingKeyConf && argv.argc() > 1)
	{
		PClassActor *ti = PClass::FindActor(argv[1]);

		if (ValidatePlayerClass(ti, argv[1]))
		{
			FPlayerClass newclass;

			newclass.Type = ti;
			newclass.Flags = 0;
			
			// If this class was already added, don't add it again			
			for(unsigned i = 0; i < PlayerClasses.Size(); i++)
			{
				if(PlayerClasses[i].Type == ti)
				{
					return;
				}
			}
			

			int arg = 2;
			while (arg < argv.argc())
			{
				if (!stricmp(argv[arg], "nomenu"))
				{
					newclass.Flags |= PCF_NOMENU;
				}
				else
				{
					Printf("Unknown flag '%s' for player class '%s'\n", argv[arg], argv[1]);
				}

				arg++;
			}
			PlayerClasses.Push(newclass);
		}
	}
}

