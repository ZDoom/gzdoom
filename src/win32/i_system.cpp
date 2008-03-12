// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <direct.h>
#include <string.h>
#include <process.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/timeb.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <richedit.h>

#define USE_WINDOWS_DWORD
#include "hardware.h"
#include "doomerrors.h"
#include <math.h>

#include "doomtype.h"
#include "version.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "m_argv.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_music.h"
#include "resource.h"

#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "i_input.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "templates.h"
#include "gameconfigfile.h"
#include "v_font.h"

#include "stats.h"

EXTERN_CVAR (String, language)

#ifdef USEASM
extern "C" void STACK_ARGS CheckMMX (CPUInfo *cpu);
#endif

extern "C"
{
	double		SecondsPerCycle = 1e-8;
	double		CyclesPerSecond = 1e8;		// 100 MHz
	CPUInfo		CPU;
}

extern HWND Window, ConWindow, GameTitleWindow;
extern HINSTANCE g_hInst;

UINT TimerPeriod;
UINT TimerEventID;
UINT MillisecondsPerTic;
HANDLE NewTicArrived;
uint32 LanguageIDs[4];
void CalculateCPUSpeed ();

const IWADInfo *DoomStartupInfo;

int (*I_GetTime) (bool saveMS);
int (*I_WaitForTic) (int);

os_t OSPlatform;

void I_Tactile (int on, int off, int total)
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t emptycmd;
ticcmd_t *I_BaseTiccmd(void)
{
	return &emptycmd;
}

static DWORD basetime = 0;

// [RH] Returns time in milliseconds
unsigned int I_MSTime (void)
{
	DWORD tm;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	return tm - basetime;
}

static DWORD TicStart;
static DWORD TicNext;

//
// I_GetTime
// returns time in 1/35th second tics
//
int I_GetTimePolled (bool saveMS)
{
	DWORD tm;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	if (saveMS)
	{
		TicStart = tm;
		TicNext = (tm * TICRATE / 1000 + 1) * 1000 / TICRATE;
	}

	return ((tm-basetime)*TICRATE)/1000;
}

int I_WaitForTicPolled (int prevtic)
{
	int time;

	while ((time = I_GetTimePolled(false)) <= prevtic)
		;

	return time;
}


static int tics;
static DWORD ted_start, ted_next;

int I_GetTimeEventDriven (bool saveMS)
{
	if (saveMS)
	{
		TicStart = ted_start;
		TicNext = ted_next;
	}
	return tics;
}

int I_WaitForTicEvent (int prevtic)
{
	while (prevtic >= tics)
	{
		WaitForSingleObject(NewTicArrived, 1000/TICRATE);
	}

	return tics;
}

void CALLBACK TimerTicked (UINT id, UINT msg, DWORD user, DWORD dw1, DWORD dw2)
{
	tics++;
	ted_start = timeGetTime ();
	ted_next = ted_start + MillisecondsPerTic;
	SetEvent (NewTicArrived);
}

// Returns the fractional amount of a tic passed since the most recent tic
fixed_t I_GetTimeFrac (uint32 *ms)
{
	DWORD now = timeGetTime();
	if (ms) *ms = TicNext;
	DWORD step = TicNext - TicStart;
	if (step == 0)
	{
		return FRACUNIT;
	}
	else
	{
		fixed_t frac = clamp<fixed_t> ((now - TicStart)*FRACUNIT/step, 0, FRACUNIT);
		return frac;
	}
}

void I_WaitVBL (int count)
{
	// I_WaitVBL is never used to actually synchronize to the
	// vertical blank. Instead, it's used for delay purposes.
	Sleep (1000 * count / 70);
}

// [RH] Detect the OS the game is running under
void I_DetectOS (void)
{
	OSVERSIONINFO info;
	const char *osname;

	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx (&info);

	switch (info.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_WINDOWS:
		OSPlatform = os_Win95;
		if (info.dwMinorVersion < 10)
		{
			osname = "95";
		}
		else if (info.dwMinorVersion < 90)
		{
			osname = "98";
		}
		else
		{
			osname = "Me";
		}
		break;

	case VER_PLATFORM_WIN32_NT:
		OSPlatform = info.dwMajorVersion < 5 ? os_WinNT4 : os_Win2k;
		osname = "NT";
		if (info.dwMajorVersion == 5)
		{
			if (info.dwMinorVersion == 0)
			{
				osname = "2000";
			}
			if (info.dwMinorVersion == 1)
			{
				osname = "XP";
			}
			else if (info.dwMinorVersion == 2)
			{
				osname = "Server 2003";
			}
		}
		else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 0)
		{
			osname = "Vista";
		}
		break;

	default:
		OSPlatform = os_unknown;
		osname = "Unknown OS";
		break;
	}

	if (OSPlatform == os_Win95)
	{
		Printf ("OS: Windows %s %lu.%lu.%lu %s\n",
				osname,
				info.dwMajorVersion, info.dwMinorVersion,
				info.dwBuildNumber & 0xffff, info.szCSDVersion);
	}
	else
	{
		Printf ("OS: Windows %s %lu.%lu (Build %lu)\n    %s\n",
				osname,
				info.dwMajorVersion, info.dwMinorVersion,
				info.dwBuildNumber, info.szCSDVersion);
	}

	if (OSPlatform == os_unknown)
	{
		Printf ("(Assuming Windows 95)\n");
		OSPlatform = os_Win95;
	}
}

//
// SubsetLanguageIDs
//
static void SubsetLanguageIDs (LCID id, LCTYPE type, int idx)
{
	char buf[8];
	LCID langid;
	char *idp;

	if (!GetLocaleInfo (id, type, buf, 8))
		return;
	langid = MAKELCID (strtoul(buf, NULL, 16), SORT_DEFAULT);
	if (!GetLocaleInfo (langid, LOCALE_SABBREVLANGNAME, buf, 8))
		return;
	idp = (char *)(&LanguageIDs[idx]);
	memset (idp, 0, 4);
	idp[0] = tolower(buf[0]);
	idp[1] = tolower(buf[1]);
	idp[2] = tolower(buf[2]);
	idp[3] = 0;
}

//
// SetLanguageIDs
//
void SetLanguageIDs ()
{
	size_t langlen = strlen (language);

	if (langlen < 2 || langlen > 3)
	{
		memset (LanguageIDs, 0, sizeof(LanguageIDs));
		SubsetLanguageIDs (LOCALE_USER_DEFAULT, LOCALE_ILANGUAGE, 0);
		SubsetLanguageIDs (LOCALE_USER_DEFAULT, LOCALE_IDEFAULTLANGUAGE, 1);
		SubsetLanguageIDs (LOCALE_SYSTEM_DEFAULT, LOCALE_ILANGUAGE, 2);
		SubsetLanguageIDs (LOCALE_SYSTEM_DEFAULT, LOCALE_IDEFAULTLANGUAGE, 3);
	}
	else
	{
		DWORD lang = 0;

		((BYTE *)&lang)[0] = (language)[0];
		((BYTE *)&lang)[1] = (language)[1];
		((BYTE *)&lang)[2] = (language)[2];
		LanguageIDs[0] = lang;
		LanguageIDs[1] = lang;
		LanguageIDs[2] = lang;
		LanguageIDs[3] = lang;
	}
}

//
// I_Init
//
void I_Init (void)
{
#ifndef USEASM
	memset (&CPU, 0, sizeof(CPU));
#else
	CheckMMX (&CPU);
	CalculateCPUSpeed ();

	// Why does Intel right-justify this string?
	char *f = CPU.CPUString, *t = f;

	while (*f == ' ')
	{
		++f;
	}
	if (f != t)
	{
		while (*f != 0)
		{
			*t++ = *f++;
		}
	}

#endif
	if (CPU.VendorID[0])
	{
		Printf ("CPU Vendor ID: %s\n", CPU.VendorID);
		if (CPU.CPUString[0])
		{
			Printf ("  Name: %s\n", CPU.CPUString);
		}
		if (CPU.bIsAMD)
		{
			Printf ("  Family %d (%d), Model %d, Stepping %d\n",
				CPU.Family, CPU.AMDFamily, CPU.AMDModel, CPU.AMDStepping);
		}
		else
		{
			Printf ("  Family %d, Model %d, Stepping %d\n",
				CPU.Family, CPU.Model, CPU.Stepping);
		}
		Printf ("  Features:");
		if (CPU.bMMX)		Printf (" MMX");
		if (CPU.bMMXPlus)	Printf (" MMX+");
		if (CPU.bSSE)		Printf (" SSE");
		if (CPU.bSSE2)		Printf (" SSE2");
		if (CPU.bSSE3)		Printf (" SSE3");
		if (CPU.b3DNow)		Printf (" 3DNow!");
		if (CPU.b3DNowPlus)	Printf (" 3DNow!+");
		Printf ("\n");
	}


	// Use a timer event if possible
	NewTicArrived = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (NewTicArrived)
	{
		UINT delay;
		char *cmdDelay;

		cmdDelay = Args->CheckValue ("-timerdelay");
		delay = 0;
		if (cmdDelay != 0)
		{
			delay = atoi (cmdDelay);
		}
		if (delay == 0)
		{
			delay = 1000/TICRATE;
		}
		TimerEventID = timeSetEvent
			(
				delay,
				0,
				TimerTicked,
				0,
				TIME_PERIODIC
			);
		MillisecondsPerTic = delay;
	}
	if (TimerEventID != 0)
	{
		I_GetTime = I_GetTimeEventDriven;
		I_WaitForTic = I_WaitForTicEvent;
	}
	else
	{	// If no timer event, busy-loop with timeGetTime
		I_GetTime = I_GetTimePolled;
		I_WaitForTic = I_WaitForTicPolled;
	}

	atterm (I_ShutdownSound);
	I_InitSound ();
}

void CalculateCPUSpeed ()
{
	LARGE_INTEGER freq;

	QueryPerformanceFrequency (&freq);

	if (freq.QuadPart != 0 && CPU.bRDTSC)
	{
		LARGE_INTEGER count1, count2;
		DWORD minDiff;
		cycle_t ClockCalibration = 0;

		// Count cycles for at least 55 milliseconds.
		// The performance counter is very low resolution compared to CPU
		// speeds today, so the longer we count, the more accurate our estimate.
		// On the other hand, we don't want to count too long, because we don't
		// want the user to notice us spend time here, since most users will
		// probably never use the performance statistics.
		minDiff = freq.LowPart * 11 / 200;

		// Minimize the chance of task switching during the testing by going very
		// high priority. This is another reason to avoid timing for too long.
		SetPriorityClass (GetCurrentProcess (), REALTIME_PRIORITY_CLASS);
		SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL);
		clock (ClockCalibration);
		QueryPerformanceCounter (&count1);
		do
		{
			QueryPerformanceCounter (&count2);
		} while ((DWORD)((unsigned __int64)count2.QuadPart - (unsigned __int64)count1.QuadPart) < minDiff);
		unclock (ClockCalibration);
		QueryPerformanceCounter (&count2);
		SetPriorityClass (GetCurrentProcess (), NORMAL_PRIORITY_CLASS);
		SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_NORMAL);

		CyclesPerSecond = (double)ClockCalibration *
			(double)freq.QuadPart /
			(double)((__int64)count2.QuadPart - (__int64)count1.QuadPart);
		SecondsPerCycle = 1.0 / CyclesPerSecond;
	}
	else
	{
		Printf ("Can't determine CPU speed, so pretending.\n");
	}

	Printf ("CPU Speed: %f MHz\n", CyclesPerSecond / 1e6);
}

//
// I_Quit
//
static int has_exited;

void I_Quit (void)
{
	has_exited = 1;		/* Prevent infinitely recursive exits -- killough */

	if (TimerEventID)
		timeKillEvent (TimerEventID);
	if (NewTicArrived)
		CloseHandle (NewTicArrived);

	timeEndPeriod (TimerPeriod);

	if (demorecording)
		G_CheckDemoStatus();
	G_ClearSnapshots ();
}


//
// I_Error
//
extern FILE *Logfile;
bool gameisdead;

void STACK_ARGS I_FatalError (const char *error, ...)
{
	static BOOL alreadyThrown = false;
	gameisdead = true;

	if (!alreadyThrown)		// ignore all but the first message -- killough
	{
		alreadyThrown = true;
		char errortext[MAX_ERRORTEXT];
		int index;
		va_list argptr;
		va_start (argptr, error);
		index = vsprintf (errortext, error, argptr);
		va_end (argptr);

		// Record error to log (if logging)
		if (Logfile)
			fprintf (Logfile, "\n**** DIED WITH FATAL ERROR:\n%s\n", errortext);

		throw CFatalError (errortext);
	}

	if (!has_exited)	// If it hasn't exited yet, exit now -- killough
	{
		has_exited = 1;	// Prevent infinitely recursive exits -- killough
		exit(-1);
	}
}

void STACK_ARGS I_Error (const char *error, ...)
{
	va_list argptr;
	char errortext[MAX_ERRORTEXT];

	va_start (argptr, error);
	vsprintf (errortext, error, argptr);
	va_end (argptr);

	throw CRecoverableError (errortext);
}

extern void LayoutMainWindow (HWND hWnd, HWND pane);

void I_SetIWADInfo (const IWADInfo *info)
{
	DoomStartupInfo = info;

	// Make the startup banner show itself
	LayoutMainWindow (Window, NULL);
}

void I_PrintStr (const char *cp)
{
	if (ConWindow == NULL)
		return;

	static bool newLine = true;
	HWND edit = ConWindow;
	char buf[256];
	int bpos = 0;
	CHARRANGE selection;
	CHARRANGE endselection;
	LONG lines_before, lines_after;
	CHARFORMAT format;

	// Store the current selection and set it to the end so we can append text.
	SendMessage (edit, EM_EXGETSEL, 0, (LPARAM)&selection);
	endselection.cpMax = endselection.cpMin = GetWindowTextLength (edit);
	SendMessage (edit, EM_EXSETSEL, 0, (LPARAM)&endselection);

	// GetWindowTextLength and EM_EXSETSEL can disagree on where the end of
	// the text is. Find out what EM_EXSETSEL thought it was and use that later.
	SendMessage (edit, EM_EXGETSEL, 0, (LPARAM)&endselection);

	// Remember how many lines there were before we added text.
	lines_before = SendMessage (edit, EM_GETLINECOUNT, 0, 0);

	while (*cp != 0)
	{
		// 28 is the escape code for a color change.
		if ((*cp == 28 && bpos != 0) || bpos == 255)
		{
			buf[bpos] = 0;
			SendMessage (edit, EM_REPLACESEL, FALSE, (LPARAM)buf);
			newLine = buf[bpos-1] == '\n';
			bpos = 0;
		}
		if (*cp != 28)
		{
			buf[bpos++] = *cp++;
		}
		else
		{
			const BYTE *color_id = (const BYTE *)cp + 1;
			EColorRange range = V_ParseFontColor (color_id, CR_UNTRANSLATED, CR_YELLOW);
			cp = (const char *)color_id;

			if (range != CR_UNDEFINED)
			{
				// Change the color of future text added to the control.
				PalEntry color = V_LogColorFromColorRange (range);
				// GDI uses BGR colors, but color is RGB, so swap the R and the B.
				swap (color.r, color.b);
				// Change the color.
				format.cbSize = sizeof(format);
				format.dwMask = CFM_COLOR;
				format.crTextColor = color;
				SendMessage (edit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
			}
		}
	}
	if (bpos != 0)
	{
		buf[bpos] = 0;
		SendMessage (edit, EM_REPLACESEL, FALSE, (LPARAM)buf); 
		newLine = buf[bpos-1] == '\n';
	}

	// If the old selection was at the end of the text, keep it at the end and
	// scroll. Don't scroll if the selection is anywhere else.
	if (selection.cpMin == endselection.cpMin && selection.cpMax == endselection.cpMax)
	{
		selection.cpMax = selection.cpMin = GetWindowTextLength (edit);
		lines_after = SendMessage (edit, EM_GETLINECOUNT, 0, 0);
		if (lines_after > lines_before)
		{
			SendMessage (edit, EM_LINESCROLL, 0, lines_after - lines_before);
		}
	}
	// Restore the previous selection.
	SendMessage (edit, EM_EXSETSEL, 0, (LPARAM)&selection);
	// Give the edit control a chance to redraw itself.
	I_GetEvent ();
}

EXTERN_CVAR (Bool, queryiwad);
CVAR (String, queryiwad_key, "shift", CVAR_GLOBALCONFIG|CVAR_ARCHIVE);
static WadStuff *WadList;
static int NumWads;
static int DefaultWad;

static void SetQueryIWad (HWND dialog)
{
	HWND checkbox = GetDlgItem (dialog, IDC_DONTASKIWAD);
	int state = SendMessage (checkbox, BM_GETCHECK, 0, 0);
	bool query = (state != BST_CHECKED);

	if (!query && queryiwad)
	{
		MessageBox (dialog,
			"You have chosen not to show this dialog box in the future.\n"
			"If you wish to see it again, hold down SHIFT while starting " GAMENAME ".",
			"Don't ask me this again",
			MB_OK | MB_ICONINFORMATION);
	}

	queryiwad = query;
}

BOOL CALLBACK IWADBoxCallback (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND ctrl;
	int i;

	switch (message)
	{
	case WM_INITDIALOG:
		// Add our program name to the window title
		{
			TCHAR label[256];
			FString newlabel;

			GetWindowText (hDlg, label, countof(label));
			newlabel.Format (GAMESIG " " DOTVERSIONSTR_NOREV ": %s", label);
			SetWindowText (hDlg, newlabel.GetChars());
		}
		// Populate the list with all the IWADs found
		ctrl = GetDlgItem (hDlg, IDC_IWADLIST);
		for (i = 0; i < NumWads; i++)
		{
			FString work;
			const char *filepart = strrchr (WadList[i].Path, '/');
			if (filepart == NULL)
				filepart = WadList[i].Path;
			else
				filepart++;
			work.Format ("%s (%s)", IWADInfos[WadList[i].Type].Name, filepart);
			SendMessage (ctrl, LB_ADDSTRING, 0, (LPARAM)work.GetChars());
			SendMessage (ctrl, LB_SETITEMDATA, i, (LPARAM)i);
		}
		SendMessage (ctrl, LB_SETCURSEL, DefaultWad, 0);
		SetFocus (ctrl);
		// Set the state of the "Don't ask me again" checkbox
		ctrl = GetDlgItem (hDlg, IDC_DONTASKIWAD);
		SendMessage (ctrl, BM_SETCHECK, queryiwad ? BST_UNCHECKED : BST_CHECKED, 0);
		// Make sure the dialog is in front. If SHIFT was pressed to force it visible,
		// then the other window will normally be on top.
		SetForegroundWindow (hDlg);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog (hDlg, -1);
		}
		else if (LOWORD(wParam) == IDOK ||
			(LOWORD(wParam) == IDC_IWADLIST && HIWORD(wParam) == LBN_DBLCLK))
		{
			SetQueryIWad (hDlg);
			ctrl = GetDlgItem (hDlg, IDC_IWADLIST);
			EndDialog (hDlg, SendMessage (ctrl, LB_GETCURSEL, 0, 0));
		}
		break;
	}
	return FALSE;
}

int I_PickIWad (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	int vkey;

	if (stricmp (queryiwad_key, "shift") == 0)
	{
		vkey = VK_SHIFT;
	}
	else if (stricmp (queryiwad_key, "control") == 0 || stricmp (queryiwad_key, "ctrl") == 0)
	{
		vkey = VK_CONTROL;
	}
	else
	{
		vkey = 0;
	}
	if (showwin || (vkey != 0 && GetAsyncKeyState(vkey)))
	{
		WadList = wads;
		NumWads = numwads;
		DefaultWad = defaultiwad;

		return DialogBox (g_hInst, MAKEINTRESOURCE(IDD_IWADDIALOG),
			(HWND)Window, (DLGPROC)IWADBoxCallback);
	}
	return defaultiwad;
}

bool I_WriteIniFailed ()
{
	char *lpMsgBuf;
	FString errortext;

	FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER | 
					FORMAT_MESSAGE_FROM_SYSTEM | 
					FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPSTR)&lpMsgBuf,
		0,
		NULL 
	);
	errortext.Format ("The config file %s could not be written:\n%s", GameConfig->GetPathName(), lpMsgBuf);
	LocalFree (lpMsgBuf);
	return MessageBox (Window, errortext.GetChars(), GAMENAME " configuration not saved", MB_ICONEXCLAMATION | MB_RETRYCANCEL) == IDRETRY;
}

void *I_FindFirst (const char *filespec, findstate_t *fileinfo)
{
	return FindFirstFileA (filespec, (LPWIN32_FIND_DATAA)fileinfo);
}
int I_FindNext (void *handle, findstate_t *fileinfo)
{
	return !FindNextFileA ((HANDLE)handle, (LPWIN32_FIND_DATAA)fileinfo);
}

int I_FindClose (void *handle)
{
	return FindClose ((HANDLE)handle);
}

static bool QueryPathKey(HKEY key, const char *keypath, const char *valname, FString &value)
{
	HKEY steamkey;
	DWORD pathtype;
	DWORD pathlen;
	LONG res;

	if(ERROR_SUCCESS == RegOpenKeyEx(key, keypath, 0, KEY_QUERY_VALUE, &steamkey))
	{
		if (ERROR_SUCCESS == RegQueryValueEx(steamkey, valname, 0, &pathtype, NULL, &pathlen) &&
			pathtype == REG_SZ && pathlen != 0)
		{
			// Don't include terminating null in count
			char *chars = value.LockNewBuffer(pathlen - 1);
			res = RegQueryValueEx(steamkey, valname, 0, NULL, (LPBYTE)chars, &pathlen);
			value.UnlockBuffer();
			if (res != ERROR_SUCCESS)
			{
				value = "";
			}
		}
		RegCloseKey(steamkey);
	}
	return value.IsNotEmpty();
}

FString I_GetSteamPath()
{
	FString path;

	if (QueryPathKey(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", path))
	{
		return path;
	}
	if (QueryPathKey(HKEY_LOCAL_MACHINE, "Software\\Valve\\Steam", "InstallPath", path))
	{
		return path;
	}
	path = "";
	return path;
}
