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
		Printf (PRINT_HIGH, "Unknown stat: %s\n", name);
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
		Printf (PRINT_HIGH, "Unknown stat: %s\n", name);
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
		screen->DrawText (CR_GREEN, 5, SCREENHEIGHT -
			SmallFont->GetHeight(), stattext);
		SB_state = screen->GetPageCount ();
	}
}

void FStat::DumpRegisteredStats ()
{
	FStat *stat = m_FirstStat;

	Printf (PRINT_HIGH, "Available stats:\n");
	while (stat)
	{
		Printf (PRINT_HIGH, "  %s\n", stat->m_Name);
		stat = stat->m_Next;
	}
}

CCMD (stat)
{
	if (argc != 2)
	{
		Printf (PRINT_HIGH, "Usage: stat <statistics>\n");
		FStat::DumpRegisteredStats ();
	}
	else
	{
		FStat::ToggleStat (argv[1]);
	}
}
