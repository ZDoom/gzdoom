/*
** stats.cpp
** Performance-monitoring statistics
**
**---------------------------------------------------------------------------
** Copyright 1998-2005 Randy Heit
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
#include "stats.h"
#include "v_video.h"
#include "v_text.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "c_dispatch.h"
#include "m_swap.h"

FStat *FStat::m_FirstStat;
FStat *FStat::m_CurrStat;

FStat::FStat (const char *name)
{
	m_Name = name;
	m_Next = m_FirstStat;
	m_FirstStat = this;
}

FStat::~FStat ()
{
	FStat **prev = &m_FirstStat;

	while (*prev && *prev != this)
		prev = &((*prev)->m_Next)->m_Next;

	if (*prev == this)
		*prev = m_Next;
}

FStat *FStat::FindStat (const char *name)
{
	FStat *stat = m_FirstStat;

	while (stat && stricmp (name, stat->m_Name))
		stat = stat->m_Next;

	return stat;
}

void FStat::SelectStat (const char *name)
{
	FStat *stat = FindStat (name);
	if (stat)
		SelectStat (stat);
	else
		Printf ("Unknown stat: %s\n", name);
}

void FStat::SelectStat (FStat *stat)
{
	m_CurrStat = stat;
	SB_state = screen->GetPageCount ();
}

void FStat::ToggleStat (const char *name)
{
	FStat *stat = FindStat (name);
	if (stat)
		ToggleStat (stat);
	else
		Printf ("Unknown stat: %s\n", name);
}

void FStat::ToggleStat (FStat *stat)
{
	if (m_CurrStat == stat)
		m_CurrStat = NULL;
	else
		m_CurrStat = stat;
	SB_state = screen->GetPageCount ();
}

void FStat::PrintStat ()
{
	if (m_CurrStat)
	{
		char stattext[256];

		m_CurrStat->GetStats (stattext);
		screen->SetFont (ConFont);
		screen->DrawText (CR_GREEN, 5, SCREENHEIGHT -
			SmallFont->GetHeight(), stattext, TAG_DONE);
		screen->SetFont (SmallFont);
		SB_state = screen->GetPageCount ();
	}
}

void FStat::DumpRegisteredStats ()
{
	FStat *stat = m_FirstStat;

	Printf ("Available stats:\n");
	while (stat)
	{
		Printf ("  %s\n", stat->m_Name);
		stat = stat->m_Next;
	}
}

CCMD (stat)
{
	if (argv.argc() != 2)
	{
		Printf ("Usage: stat <statistics>\n");
		FStat::DumpRegisteredStats ();
	}
	else
	{
		FStat::ToggleStat (argv[1]);
	}
}
