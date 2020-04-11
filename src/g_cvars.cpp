/*
 ** g_cvars.cpp
 ** collects all the CVARs that were scattered all across the code before
 **
 **---------------------------------------------------------------------------
 ** Copyright 1998-2016 Randy Heit
 ** Copyright 2003-2018 Christoph Oelckers
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

#include "c_cvars.h"
#include "g_levellocals.h"
#include "g_game.h"
#include "gstrings.h"
#include "i_system.h"
#include "v_font.h"
#include "utf8.h"
#include "gi.h"

void I_UpdateWindowTitle();

CVAR (Bool, cl_spreaddecals, true, CVAR_ARCHIVE)
CVAR(Bool, var_pushers, true, CVAR_SERVERINFO);
CVAR(Bool, gl_cachenodes, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Float, gl_cachetime, 0.6f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, alwaysapplydmflags, false, CVAR_SERVERINFO);

// Show developer messages if true.
CUSTOM_CVAR(Int, developer, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	FScriptPosition::Developer = self;
}

// [RH] Feature control cvars
CVAR(Bool, var_friction, true, CVAR_SERVERINFO);

// Option Search
CVAR(Bool, os_isanyof, true, CVAR_ARCHIVE);

CUSTOM_CVAR (Int, turnspeedwalkfast, 640, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self <= 0) self = 1;
}
CUSTOM_CVAR (Int, turnspeedsprintfast, 1280, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self <= 0) self = 1;
}
CUSTOM_CVAR (Int, turnspeedwalkslow, 320, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self <= 0) self = 1;
}
CUSTOM_CVAR (Int, turnspeedsprintslow, 320, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self <= 0) self = 1;
}



CUSTOM_CVAR (Bool, gl_lights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	for (auto Level : AllLevels())
	{
		if (self) Level->RecreateAllAttachedLights();
		else Level->DeleteAllAttachedLights();
	}
}

CUSTOM_CVAR(Int, sv_corpsequeuesize, 64, CVAR_ARCHIVE|CVAR_SERVERINFO|CVAR_NOINITCALL)
{
	if (self > 0)
	{
		for (auto Level : AllLevels())
		{
			auto &corpsequeue = Level->CorpseQueue;
			while (corpsequeue.Size() > (unsigned)self)
			{
				AActor *corpse = corpsequeue[0];
				if (corpse) corpse->Destroy();
				corpsequeue.Delete(0);
			}
		}
	}
}

CUSTOM_CVAR (Int, cl_maxdecals, 1024, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = 0;
	}
	else for (auto Level : AllLevels())
	{
		while (Level->ImpactDecalCount > self)
		{
			DThinker *thinker = Level->FirstThinker(STAT_AUTODECAL);
			if (thinker != NULL)
			{
				thinker->Destroy();
				Level->ImpactDecalCount--;
			}
		}
	}
}


// [BC] Allow the maximum number of particles to be specified by a cvar (so people
// with lots of nice hardware can have lots of particles!).
CUSTOM_CVAR(Int, r_maxparticles, 4000, CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	if (self == 0)
		self = 4000;
	else if (self > 65535)
		self = 65535;
	else if (self < 100)
		self = 100;

	if (gamestate != GS_STARTUP)
	{
		for (auto Level : AllLevels())
		{
			P_InitParticles(Level);
		}
	}
}

CUSTOM_CVAR(Float, teamdamage, 0.f, CVAR_SERVERINFO | CVAR_NOINITCALL)
{
	for (auto Level : AllLevels())
	{
		Level->teamdamage = self;
	}
}

bool generic_ui;
EXTERN_CVAR(String, language)

bool CheckFontComplete(FFont *font)
{
	// Also check if the SmallFont contains all characters this language needs.
	// If not, switch back to the original one.
	return font->CanPrint(GStrings["REQUIRED_CHARACTERS"]);
}

void UpdateGenericUI(bool cvar)
{
	auto switchstr = GStrings["USE_GENERIC_FONT"];
	generic_ui = (cvar || (switchstr && strtoll(switchstr, nullptr, 0)) || ((gameinfo.gametype & GAME_Raven) && !strnicmp(language, "el", 2)));
	if (!generic_ui)
	{
		// Use the mod's SmallFont if it is complete.
		// Otherwise use the stock Smallfont if it is complete.
		// If none is complete, fall back to the VGA font.
		// The font being set here will be used in 3 places: Notifications, centered messages and menu confirmations.
		if (CheckFontComplete(SmallFont))
		{
			AlternativeSmallFont = SmallFont;
		}
		else if (OriginalSmallFont && CheckFontComplete(OriginalSmallFont))
		{
			AlternativeSmallFont = OriginalSmallFont;
		}
		else
		{
			AlternativeSmallFont = NewSmallFont;
		}

		// Todo: Do the same for the BigFont
	}
}

CUSTOM_CVAR(Bool, ui_generic, false, CVAR_NOINITCALL) // This is for allowing to test the generic font system with all languages
{
	UpdateGenericUI(self);
}


CUSTOM_CVAR(String, language, "auto", CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
{
	GStrings.UpdateLanguage(self);
	for (auto Level : AllLevels())
	{
		// does this even make sense on secondary levels...?
		if (Level->info != nullptr) Level->LevelName = Level->info->LookupLevelName();
	}
	UpdateGenericUI(ui_generic);
	I_UpdateWindowTitle();
}
