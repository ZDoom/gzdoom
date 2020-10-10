/*
** c_tabcomplete.cpp
** Tab completion code
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "name.h"
#include "tarray.h"
#include "zstring.h"
#include "c_commandbuffer.h"
#include "c_cvars.h"
#include "c_tabcomplete.h"
#include "printf.h"
#include "c_dispatch.h"
#include "v_draw.h"
#include <string.h>



struct TabData
{
	int UseCount;
	FName TabName;

	TabData()
	: UseCount(0), TabName(NAME_None)
	{
	}

	TabData(const char *name)
	: UseCount(1), TabName(name)
	{
	}

	TabData(const TabData &other) = default;
};

static TArray<TabData> TabCommands (TArray<TabData>::NoInit);
static int TabPos;				// Last TabCommand tabbed to
static int TabStart;			// First char in CmdLine to use for tab completion
static int TabSize;				// Size of tab string

bool TabbedLast;		// True if last key pressed was tab
bool TabbedList;		// True if tab list was shown
CVAR(Bool, con_notablist, false, CVAR_ARCHIVE)

static bool FindTabCommand (const char *name, int *stoppos, int len)
{
	FName aname(name);
	unsigned int i;
	int cval = 1;

	for (i = 0; i < TabCommands.Size(); i++)
	{
		if (TabCommands[i].TabName == aname)
		{
			*stoppos = i;
			return true;
		}
		cval = strnicmp (TabCommands[i].TabName.GetChars(), name, len);
		if (cval >= 0)
			break;
	}

	*stoppos = i;

	return (cval == 0);
}

void C_AddTabCommand (const char *name)
{
	int pos;

	if (FindTabCommand (name, &pos, INT_MAX))
	{
		TabCommands[pos].UseCount++;
	}
	else
	{
		TabData tab(name);
		TabCommands.Insert (pos, tab);
	}
}

void C_RemoveTabCommand (const char *name)
{
	if (TabCommands.Size() == 0)
	{
		// There are no tab commands that can be removed.
		// This is important to skip construction of aname 
		// in case the NameManager has already been destroyed.
		return;
	}

	FName aname(name, true);

	if (aname == NAME_None)
	{
		return;
	}
	for (unsigned int i = 0; i < TabCommands.Size(); ++i)
	{
		if (TabCommands[i].TabName == aname)
		{
			if (--TabCommands[i].UseCount == 0)
			{
				TabCommands.Delete(i);
			}
			break;
		}
	}
}

void C_ClearTabCommands ()
{
	TabCommands.Clear();
}

static int FindDiffPoint (FName name1, const char *str2)
{
	const char *str1 = name1.GetChars();
	int i;

	for (i = 0; tolower(str1[i]) == tolower(str2[i]); i++)
		if (str1[i] == 0 || str2[i] == 0)
			break;

	return i;
}

void C_TabComplete (bool goForward)
{
	unsigned i;
	int diffpoint;

	auto CmdLineText = CmdLine.GetText();
	if (!TabbedLast)
	{
		bool cancomplete;


		// Skip any spaces at beginning of command line
		for (i = 0; i < CmdLineText.Len(); ++i)
		{
			if (CmdLineText[i] != ' ')
				break;
		}
		if (i == CmdLineText.Len())
		{ // Line was nothing but spaces
			return;
		}
		TabStart = i;

		TabSize = (int)CmdLineText.Len() - TabStart;

		if (!FindTabCommand(&CmdLineText[TabStart], &TabPos, TabSize))
			return;		// No initial matches

		// Show a list of possible completions, if more than one.
		if (TabbedList || con_notablist)
		{
			cancomplete = true;
		}
		else
		{
			cancomplete = C_TabCompleteList ();
			TabbedList = true;
		}

		if (goForward)
		{ // Position just before the list of completions so that when TabPos
		  // gets advanced below, it will be at the first one.
			--TabPos;
		}
		else
		{ // Find the last matching tab, then go one past it.
			while (++TabPos < (int)TabCommands.Size())
			{
				if (FindDiffPoint(TabCommands[TabPos].TabName, &CmdLineText[TabStart]) < TabSize)
				{
					break;
				}
			}
		}
		TabbedLast = true;
		if (!cancomplete)
		{
			return;
		}
	}

	if ((goForward && ++TabPos == (int)TabCommands.Size()) ||
		(!goForward && --TabPos < 0))
	{
		TabbedLast = false;
		CmdLineText.Truncate(TabSize);
	}
	else
	{
		diffpoint = FindDiffPoint(TabCommands[TabPos].TabName, &CmdLineText[TabStart]);

		if (diffpoint < TabSize)
		{
			// No more matches
			TabbedLast = false;
			CmdLineText.Truncate(TabSize - TabStart);
		}
		else
		{
			CmdLineText.Truncate(TabStart);
			CmdLineText << TabCommands[TabPos].TabName.GetChars() << ' ';
		}
	}
	CmdLine.SetString(CmdLineText);
	CmdLine.MakeStartPosGood();
}

bool C_TabCompleteList ()
{
	int nummatches, i;
	size_t maxwidth;
	int commonsize = INT_MAX;

	nummatches = 0;
	maxwidth = 0;

	auto CmdLineText = CmdLine.GetText();
	for (i = TabPos; i < (int)TabCommands.Size(); ++i)
	{
		if (FindDiffPoint (TabCommands[i].TabName, &CmdLineText[TabStart]) < TabSize)
		{
			break;
		}
		else
		{
			if (i > TabPos)
			{
				// This keeps track of the longest common prefix for all the possible
				// completions, so we can fill in part of the command for the user if
				// the longest common prefix is longer than what the user already typed.
				int diffpt = FindDiffPoint (TabCommands[i-1].TabName, TabCommands[i].TabName.GetChars());
				if (diffpt < commonsize)
				{
					commonsize = diffpt;
				}
			}
			nummatches++;
			maxwidth = std::max (maxwidth, strlen (TabCommands[i].TabName.GetChars()));
		}
	}
	if (nummatches > 1)
	{
		size_t x = 0;
		maxwidth += 3;
		Printf (TEXTCOLOR_BLUE "Completions for %s:\n", CmdLineText.GetChars());
		for (i = TabPos; nummatches > 0; ++i, --nummatches)
		{
			// [Dusk] Print console commands blue, CVars green, aliases red.
			const char* colorcode = "";
			FConsoleCommand* ccmd;
			if (FindCVar (TabCommands[i].TabName.GetChars(), NULL))
				colorcode = TEXTCOLOR_GREEN;
			else if ((ccmd = FConsoleCommand::FindByName (TabCommands[i].TabName.GetChars())) != NULL)
			{
				if (ccmd->IsAlias())
					colorcode = TEXTCOLOR_RED;
				else
					colorcode = TEXTCOLOR_LIGHTBLUE;
			}

			Printf ("%s%-*s", colorcode, int(maxwidth), TabCommands[i].TabName.GetChars());
			x += maxwidth;
			if (x > CmdLine.ConCols / active_con_scale(twod) - maxwidth)
			{
				x = 0;
				Printf ("\n");
			}
		}
		if (x != 0)
		{
			Printf ("\n");
		}
		// Fill in the longest common prefix, if it's longer than what was typed.
		if (TabSize != commonsize)
		{
			TabSize = commonsize;
			CmdLineText.Truncate(TabStart);
			CmdLineText.AppendCStrPart(TabCommands[TabPos].TabName.GetChars(), commonsize);
			CmdLine.SetString(CmdLineText);
		}
		return false;
	}
	return true;
}
