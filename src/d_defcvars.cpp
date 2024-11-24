//-----------------------------------------------------------------------------
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
// Copyright 2019-2021 Rachael Alexanderson
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//
// DESCRIPTION:
//		defcvars loader split from d_main.cpp
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "doomstat.h"
#include "gstrings.h"
#include "filesystem.h"
#include "menu.h"
#include "doommenu.h"
#include "c_console.h"
#include "d_main.h"
#include "version.h"
#include "d_defcvars.h"


void D_GrabCVarDefaults()
{
	int lump, lastlump = 0;
	int lumpversion, gamelastrunversion;
	gamelastrunversion = atoi(LASTRUNVERSION);

	while ((lump = fileSystem.FindLump("DEFCVARS", &lastlump)) != -1)
	{
		// don't parse from wads
		if (lastlump > fileSystem.GetLastEntry(fileSystem.GetMaxBaseNum()))
		{
			// would rather put this in a modal of some sort, but this will have to do.
			Printf(TEXTCOLOR_RED "Cannot load DEFCVARS from a wadfile!\n");
			break;
		}

		FScanner sc(lump);

		sc.MustGetString();
		if (!sc.Compare("version"))
			sc.ScriptError("Must declare version for defcvars! (currently: %i)", gamelastrunversion);
		sc.MustGetNumber();
		lumpversion = sc.Number;
		if (lumpversion > gamelastrunversion)
			sc.ScriptError("Unsupported version %i (%i supported)", lumpversion, gamelastrunversion);
		if (lumpversion < 219)
			sc.ScriptError("Version must be at least 219 (current version %i)", gamelastrunversion);

		FBaseCVar* var;
		FString CurrentFindCVar;

		while (sc.GetString())
		{
			if (sc.Compare("set"))
			{
				sc.MustGetString();
			}

			CurrentFindCVar = sc.String;
			if (lumpversion < 220)
			{
				CurrentFindCVar.ToLower();

				// these two got renamed
				if (CurrentFindCVar.Compare("gamma") == 0)
				{
					CurrentFindCVar = "vid_gamma";
				}
				if (CurrentFindCVar.Compare("fullscreen") == 0)
				{
					CurrentFindCVar = "vid_fullscreen";
				}

				// this was removed
				if (CurrentFindCVar.Compare("cd_drive") == 0)
					break;
			}
			if (lumpversion < 221)
			{
				// removed cvar
				// this one doesn't matter as much, since it depended on platform-specific values,
				// and is something the user should change anyhow, so, let's just throw this value
				// out.
				if (CurrentFindCVar.Compare("mouse_sensitivity") == 0)
					break;
				if (CurrentFindCVar.Compare("m_noprescale") == 0)
					break;
			}

			bool blacklisted = false;
			SHOULD_BLACKLIST(disablecrashlog)
			SHOULD_BLACKLIST(gl_control_tear)
			SHOULD_BLACKLIST(in_mouse)
			SHOULD_BLACKLIST(joy_dinput)
			SHOULD_BLACKLIST(joy_ps2raw)
			SHOULD_BLACKLIST(joy_xinput)
			SHOULD_BLACKLIST(k_allowfullscreentoggle)
			SHOULD_BLACKLIST(k_mergekeys)
			SHOULD_BLACKLIST(m_swapbuttons)
			SHOULD_BLACKLIST(queryiwad_key)
			SHOULD_BLACKLIST(vid_gpuswitch)
			SHOULD_BLACKLIST(vr_enable_quadbuffered)
			SHOULD_BLACKLIST(m_filter)
			SHOULD_BLACKLIST(gl_debug)
			SHOULD_BLACKLIST(vid_adapter)
			SHOULD_BLACKLIST(sys_statsenabled47)
			SHOULD_BLACKLIST(sys_statsenabled49)
			SHOULD_BLACKLIST(anonstats_statsenabled411)
			SHOULD_BLACKLIST(save_dir)
			SHOULD_BLACKLIST(sys_statsport)
			SHOULD_BLACKLIST(sys_statshost)
			SHOULD_BLACKLIST(anonstats_port)
			SHOULD_BLACKLIST(anonstats_host)
			SHOULD_BLACKLIST(sentstats_hwr_done)

			var = FindCVar(CurrentFindCVar.GetChars(), NULL);

			if (blacklisted)
			{
				sc.ScriptMessage("Cannot set cvar default for blacklisted cvar '%s'", sc.String);
				sc.MustGetString(); // to ignore the value of the cvar
			}
			else if (var != NULL)
			{

				if (var->GetFlags() & CVAR_ARCHIVE)
				{
					UCVarValue val;

					sc.MustGetString();
					val.String = const_cast<char*>(sc.String);
					var->SetGenericRepDefault(val, CVAR_String);
				}
				else 
				{
					sc.ScriptMessage("Cannot set cvar default for non-config cvar '%s'", sc.String);
					sc.MustGetString(); // to ignore the value of the cvar
				}
			}
			else
			{
				sc.ScriptMessage("Unknown cvar '%s' in defcvars", sc.String);
				sc.MustGetString(); // to ignore the value of the cvar
			}
		}
	}
}

