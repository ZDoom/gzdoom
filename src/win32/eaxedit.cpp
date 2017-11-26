/*
**
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "resource.h"
#include "s_sound.h"
#include "templates.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "c_cvars.h"
#include "doomstat.h"
#include "v_video.h"
#include "c_cvars.h"
#include "vm.h"
#include "symbols.h"
#include "menu/menu.h"


REVERB_PROPERTIES SavedProperties;
ReverbContainer *CurrentEnv;
extern ReverbContainer *ForcedEnvironment;

CUSTOM_CVAR (Bool, eaxedit_test, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
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

struct MySaveData
{
	const ReverbContainer *Default;
	const ReverbContainer **Saves;
	uint32_t NumSaves;

	MySaveData (const ReverbContainer *def)
		: Default (def), Saves (nullptr), NumSaves (0) {}
	~MySaveData ()
	{
		if (Saves) delete[] Saves;
	}
};

struct EnvControl
{
	const char *Name;
	int EditControl;
	int SliderControl;
	int Min;
	int Max;
	float REVERB_PROPERTIES::*Float;
	int REVERB_PROPERTIES::*Int;
	HWND EditHWND;
	HWND SliderHWND;
};

struct EnvFlag
{
	const char *Name;
	int CheckboxControl;
	unsigned int Flag;
};

EnvControl EnvControls[] =
{
	{ "Environment",				0,							0,							0, 25, 0, &REVERB_PROPERTIES::Environment },
	{ "EnvironmentSize",			IDCE_ENVIRONMENTSIZE,		IDCS_ENVIRONMENTSIZE,		1000, 100000, &REVERB_PROPERTIES::EnvSize, 0 },
	{ "EnvironmentDiffusion",		IDCE_ENVIRONMENTDIFFUSION,	IDCS_ENVIRONMENTDIFFUSION,	0, 1000, &REVERB_PROPERTIES::EnvDiffusion, 0 },
	{ "Room",						IDCE_ROOM,					IDCS_ROOM,					-10000, 0, 0, &REVERB_PROPERTIES::Room },
	{ "RoomHF",						IDCE_ROOMHF,				IDCS_ROOMHF,				-10000, 0, 0, &REVERB_PROPERTIES::RoomHF },
	{ "RoomLF",						IDCE_ROOMLF,				IDCS_ROOMLF,				-10000, 0, 0, &REVERB_PROPERTIES::RoomLF },
	{ "DecayTime",					IDCE_DECAYTIME,				IDCS_DECAYTIME,				100, 20000, &REVERB_PROPERTIES::DecayTime, 0 },
	{ "DecayHFRatio",				IDCE_DECAYHFRATIO,			IDCS_DECAYHFRATIO,			100, 2000, &REVERB_PROPERTIES::DecayHFRatio, 0 },
	{ "DecayLFRatio",				IDCE_DECAYLFRATIO,			IDCS_DECAYLFRATIO,			100, 2000, &REVERB_PROPERTIES::DecayLFRatio, 0 },
	{ "Reflections",				IDCE_REFLECTIONS,			IDCS_REFLECTIONS,			-10000, 1000, 0, &REVERB_PROPERTIES::Reflections },
	{ "ReflectionsDelay",			IDCE_REFLECTIONSDELAY,		IDCS_REFLECTIONSDELAY,		0, 300, &REVERB_PROPERTIES::ReflectionsDelay, 0 },
	{ "ReflectionsPanX",			IDCE_REFLECTIONSPANX,		IDCS_REFLECTIONSPANX,		-2000000, 2000000, &REVERB_PROPERTIES::ReflectionsPan0, 0 },
	{ "ReflectionsPanY",			IDCE_REFLECTIONSPANY,		IDCS_REFLECTIONSPANY,		-2000000, 2000000, &REVERB_PROPERTIES::ReflectionsPan1, 0 },
	{ "ReflectionsPanZ",			IDCE_REFLECTIONSPANZ,		IDCS_REFLECTIONSPANZ,		-2000000, 2000000, &REVERB_PROPERTIES::ReflectionsPan2, 0 },
	{ "Reverb",						IDCE_REVERB,				IDCS_REVERB,				-10000, 2000, 0, &REVERB_PROPERTIES::Reverb },
	{ "ReverbDelay",				IDCE_REVERBDELAY,			IDCS_REVERBDELAY,			0, 100, &REVERB_PROPERTIES::ReverbDelay, 0 },
	{ "ReverbPanX",					IDCE_REVERBPANX,			IDCS_REVERBPANX,			-2000000, 2000000, &REVERB_PROPERTIES::ReverbPan0, 0 },
	{ "ReverbPanY",					IDCE_REVERBPANY,			IDCS_REVERBPANY,			-2000000, 2000000, &REVERB_PROPERTIES::ReverbPan1, 0 },
	{ "ReverbPanZ",					IDCE_REVERBPANZ,			IDCS_REVERBPANZ,			-2000000, 2000000, &REVERB_PROPERTIES::ReverbPan2, 0 },
	{ "EchoTime",					IDCE_ECHOTIME,				IDCS_ECHOTIME,				75, 250, &REVERB_PROPERTIES::EchoTime, 0 },
	{ "EchoDepth",					IDCE_ECHODEPTH,				IDCS_ECHODEPTH,				0, 1000, &REVERB_PROPERTIES::EchoDepth, 0 },
	{ "ModulationTime",				IDCE_MODULATIONTIME,		IDCS_MODULATIONTIME,		40, 4000, &REVERB_PROPERTIES::ModulationTime, 0 },
	{ "ModulationDepth",			IDCE_MODULATIONDEPTH,		IDCS_MODULATIONDEPTH,		0, 1000, &REVERB_PROPERTIES::ModulationDepth, 0 },
	{ "AirAbsorptionHF",			IDCE_AIRABSORPTIONHF,		IDCS_AIRABSORPTIONHF,		-100000, 0, &REVERB_PROPERTIES::AirAbsorptionHF, 0 },
	{ "HFReference",				IDCE_HFREFERENCE,			IDCS_HFREFERENCE,			1000000, 20000000, &REVERB_PROPERTIES::HFReference, 0 },
	{ "LFReference",				IDCE_LFREFERENCE,			IDCS_LFREFERENCE,			20000, 1000000, &REVERB_PROPERTIES::LFReference, 0 },
	{ "RoomRolloffFactor",			IDCE_ROOMROLLOFFFACTOR,		IDCS_ROOMROLLOFFFACTOR,		0, 10000, &REVERB_PROPERTIES::RoomRolloffFactor, 0 },
	{ "Diffusion",					0,							0,							0, 100000, &REVERB_PROPERTIES::Diffusion, 0 },
	{ "Density",					0,							0,							0, 100000, &REVERB_PROPERTIES::Density, 0 },
};

EnvFlag EnvFlags[] =
{
	{ "bReflectionsScale",		IDC_REFLECTIONSSCALE, REVERB_FLAGS_REFLECTIONSSCALE },
	{ "bReflectionsDelayScale",	IDC_REFLECTIONSDELAYSCALE, REVERB_FLAGS_REFLECTIONSDELAYSCALE },
	{ "bDecayTimeScale",		IDC_DECAYTIMESCALE, REVERB_FLAGS_DECAYTIMESCALE },
	{ "bDecayHFLimit",			IDC_DECAYHFLIMIT, REVERB_FLAGS_DECAYHFLIMIT },
	{ "bReverbScale",			IDC_REVERBSCALE, REVERB_FLAGS_REVERBSCALE },
	{ "bReverbDelayScale",		IDC_REVERBDELAYSCALE, REVERB_FLAGS_REVERBDELAYSCALE },
	{ "bEchoTimeScale",			IDC_ECHOTIMESCALE, REVERB_FLAGS_ECHOTIMESCALE },
	{ "bModulationTimeScale",	IDC_MODULATIONTIMESCALE, REVERB_FLAGS_MODULATIONTIMESCALE }
};

inline int HIBYTE(int i)
{
	return (i >> 8) & 255;
}

inline int LOBYTE(int i)
{
	return i & 255;
}

uint16_t FirstFreeID (uint16_t base, bool builtin)
{
	int tryCount = 0;
	int priID = HIBYTE(base);

	// If the original sound is built-in, start searching for a new
	// primary ID at 30.
	if (builtin)
	{
		for (priID = 30; priID < 256; ++priID)
		{
			if (S_FindEnvironment (priID << 8) == nullptr)
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
						return lastID+1;
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
				base += 100*256;
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
			return lastID+1;
		}
	}
}

FString SuggestNewName (const ReverbContainer *env)
{
	const ReverbContainer *probe = nullptr;
	char text[32];
	size_t len;
	int number, numdigits;

	strncpy (text, env->Name, 31);
	text[31] = 0;

	len = strlen (text);
	while (text[len-1] >= '0' && text[len-1] <= '9')
	{
		len--;
	}
	number = atoi (text + len);
	if (number < 1)
	{
		number = 1;
	}

	if (text[len-1] != ' ' && len < 31)
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
		mysnprintf (text + len, countof(text) - len, "%d", number);

		probe = Environments;
		while (probe != nullptr)
		{
			if (stricmp (probe->Name, text) == 0)
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

void ExportEnvironments (const char *filename, uint32_t count, const ReverbContainer **envs)
{
	FILE *f;

retry:
	f = fopen (filename, "w");

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
			fprintf (f, "\"%s\" %u %u\n{\n", env->Name, HIBYTE(env->ID), LOBYTE(env->ID));
			for (j = 0; j < countof(EnvControls); ++j)
			{
				const EnvControl *ctl = &EnvControls[j];
				if (ctl->Name &&
					(j == 0 ||
					(ctl->Float && base->Properties.*ctl->Float != env->Properties.*ctl->Float) ||
					(ctl->Int && base->Properties.*ctl->Int != env->Properties.*ctl->Int))
					)
				{
					fprintf (f, "\t%s ", ctl->Name);
					if (ctl->Float)
					{
						float v = env->Properties.*ctl->Float * 1000;
						int vi = int(v >= 0.0 ? v + 0.5 : v - 0.5);
						fprintf (f, "%d.%03d\n", vi/1000, abs(vi%1000));
					}
					else
					{
						fprintf (f, "%d\n", env->Properties.*ctl->Int);
					}
				}
			}
			for (j = 0; j < countof(EnvFlags); ++j)
			{
				if (EnvFlags[j].Flag & (env->Properties.Flags ^ base->Properties.Flags))
				{
					fprintf (f, "\t%s %s\n", EnvFlags[j].Name,
						EnvFlags[j].Flag & env->Properties.Flags ? "true" : "false");
				}
			}
			fprintf (f, "}\n\n");
		}
		fclose (f);
	}
	else
	{
		LPVOID lpMsgBuf;
		FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0,
			nullptr);
		if (IDRETRY == MessageBox (EAXEditWindow, (LPCTSTR)lpMsgBuf, "Save Failed!", MB_RETRYCANCEL|MB_ICONERROR))
		{
			LocalFree (lpMsgBuf);
			goto retry;
		}
		LocalFree (lpMsgBuf);
	}
}

void SaveEnvironments (HWND owner, const ReverbContainer *defEnv)
{
	MySaveData msd (defEnv);
	OPENFILENAME ofn = { sizeof(ofn) };
	char filename[PATH_MAX];

	ofn.hwndOwner = owner;
	ofn.hInstance = g_hInst;
	ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
	ofn.lpstrCustomFilter = nullptr;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = sizeof(filename);
	ofn.lpstrFileTitle = nullptr;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = nullptr;
	ofn.lpstrTitle = "Save EAX Environments As...";
	ofn.Flags = OFN_ENABLEHOOK|OFN_ENABLESIZING|OFN_ENABLETEMPLATE|OFN_EXPLORER|
		OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = "txt";
	ofn.lCustData = (LPARAM)&msd;
	ofn.lpfnHook = SaveHookProc;
	ofn.lpTemplateName = MAKEINTRESOURCE(IDD_SAVEEAX);

	filename[0] = 0;

	if (GetSaveFileName ((tagOFNA *)&ofn) && msd.NumSaves != 0)
	{
		ExportEnvironments (filename, msd.NumSaves, msd.Saves);
	}
}

INT_PTR CALLBACK EAXProc (HWND hDlg, uint32_t uMsg, WPARAM wParam, LPARAM lParam)
{
	SCROLLINFO scrollInfo;
	HWND hWnd;
	RECT rect;
	POINT ul;
	ReverbContainer *env;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		hPropList = CreateDialog (g_hInst, MAKEINTRESOURCE(IDD_EAXPROPERTYLIST), hDlg, EAXProp);
		hWnd = GetDlgItem (hDlg, IDC_DUMMY);
		GetWindowRect (hPropList, &rect);
		PropListMaxSize.x = rect.right - rect.left;
		PropListMaxSize.y = rect.bottom - rect.top;
		GetWindowRect (hWnd, &rect);
		DestroyWindow (hWnd);
		ul.x = rect.left;
		ul.y = rect.top;
		ScreenToClient (hDlg, &ul);
		PropListMaxSize.x = rect.right - rect.left;

		scrollInfo.cbSize = sizeof(scrollInfo);
		scrollInfo.fMask = SIF_RANGE|SIF_POS|SIF_PAGE|SIF_DISABLENOSCROLL;
		scrollInfo.nMin = 0;
		scrollInfo.nMax = PropListMaxSize.y;
		scrollInfo.nPos = 0;
		scrollInfo.nPage = rect.bottom - rect.top;
		SetScrollInfo (hPropList, SB_VERT, &scrollInfo, TRUE);

		MoveWindow (hPropList, ul.x, ul.y, PropListMaxSize.x, rect.bottom - rect.top, FALSE);
		ShowWindow (hPropList, SW_SHOW);

		// Windows should have a layout control like MUI so that I don't have
		// to do any extra work to make the dialog resizable.

		GetClientRect (hDlg, &rect);
		PropListHeightDiff = (rect.bottom - rect.top) - scrollInfo.nPage;

		// Using a scroll bar to create a size box seems like an odd way of
		// doing things.

		hWnd = CreateWindow(
			"Scrollbar", 
			(LPSTR)nullptr,
			WS_CHILD | WS_VISIBLE | SBS_SIZEGRIP | SBS_SIZEBOXBOTTOMRIGHTALIGN | WS_CLIPSIBLINGS,
			0,0,rect.right,rect.bottom,
			hDlg,
			(HMENU)IDC_SIZEBOX,
			g_hInst,
			nullptr);

		TestLocation.x = DoneLocation.x = rect.right;
		TestLocation.y = DoneLocation.y = rect.bottom;
		GetWindowRect (GetDlgItem (hDlg, IDOK), &rect);
		ScreenToClient (hDlg, (LPPOINT)&rect.left);
		DoneLocation.x -= rect.left;
		DoneLocation.y -= rect.top;

		GetWindowRect (GetDlgItem (hDlg, IDC_TESTEAX), &rect);
		ScreenToClient (hDlg, (LPPOINT)&rect.left);
		TestLocation.x -= rect.left;
		TestLocation.y -= rect.top;

		GetWindowRect (hDlg, &rect);
		EditWindowSize.x = rect.right - rect.left;
		EditWindowSize.y = (rect.bottom - rect.top) - scrollInfo.nPage;

		GetWindowRect (GetDlgItem (hDlg, IDC_NEW), &rect);
		ScreenToClient (hDlg, (LPPOINT)&rect.left);
		NewLeft = rect.left;

		GetWindowRect (GetDlgItem (hDlg, IDC_SAVE), &rect);
		ScreenToClient (hDlg, (LPPOINT)&rect.left);
		SaveLeft = rect.left;

		GetWindowRect (GetDlgItem (hDlg, IDC_REVERT), &rect);
		ScreenToClient (hDlg, (LPPOINT)&rect.left);
		RevertLeft = rect.left;

		hWnd = GetDlgItem (hDlg, IDC_CURRENTENVIRONMENT);
		PopulateEnvDropDown (hWnd, IsDlgButtonChecked (hDlg, IDC_SHOWIDS)==BST_CHECKED, nullptr);
		EAXProc (hDlg, WM_COMMAND, MAKELONG(IDC_CURRENTENVIRONMENT,CBN_SELENDOK), (LPARAM)hWnd);

		CheckDlgButton (hDlg, IDC_TESTEAX, eaxedit_test ? BST_CHECKED : BST_UNCHECKED);
		return 0;

	case WM_SIZE:
		if (wParam != SIZE_MAXHIDE && wParam != SIZE_MAXSHOW)
		{
			GetClientRect (hWnd = GetDlgItem (hDlg, IDC_SIZEBOX), &rect);
			SetWindowPos (hWnd, HWND_BOTTOM, LOWORD(lParam)-rect.right, HIWORD(lParam)-rect.bottom, 0, 0, SWP_NOSIZE);

			SetWindowPos (hPropList, nullptr, 0, 0, PropListMaxSize.x, HIWORD(lParam)-PropListHeightDiff, SWP_NOMOVE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_DEFERERASE);
			SetWindowPos (GetDlgItem (hDlg, IDOK), nullptr, LOWORD(lParam)-DoneLocation.x, HIWORD(lParam)-DoneLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
			SetWindowPos (GetDlgItem (hDlg, IDC_NEW), nullptr, NewLeft, HIWORD(lParam)-DoneLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
			SetWindowPos (GetDlgItem (hDlg, IDC_SAVE), nullptr, SaveLeft, HIWORD(lParam)-DoneLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
			SetWindowPos (GetDlgItem (hDlg, IDC_REVERT), nullptr, RevertLeft, HIWORD(lParam)-DoneLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
			SetWindowPos (GetDlgItem (hDlg, IDC_TESTEAX), nullptr, LOWORD(lParam)-TestLocation.x, HIWORD(lParam)-TestLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
		}
		return 0;

	case WM_NCDESTROY:
		EAXEditWindow = 0;
		//new FS_Switcher;
		ForceWindowed = false;
		if (fullscreen)
		{
			setmodeneeded = true;
		}
		ForcedEnvironment = nullptr;
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			DestroyWindow (hPropList);
			DestroyWindow (hDlg);
            return 1;

		case IDC_NEW:
			hWnd = GetDlgItem (hDlg, IDC_CURRENTENVIRONMENT);
			env = (ReverbContainer *)DialogBoxParam (g_hInst,
				MAKEINTRESOURCE(IDD_NEWEAX), hDlg, NewEAXProc,
				SendMessage (hWnd, CB_GETITEMDATA,
					SendMessage (hWnd, CB_GETCURSEL, 0, 0), 0));
			if (env != nullptr)
			{
				LRESULT i = AddEnvToDropDown (hWnd,
					SendMessage (GetDlgItem (hDlg, IDC_SHOWIDS), BM_GETCHECK, 0, 0)==BST_CHECKED,
					env);
				SendMessage (hWnd, CB_SETCURSEL, i, 0);
				UpdateControls (env, hDlg);

				hWnd = GetDlgItem (hPropList, IDCE_ENVIRONMENTSIZE);
				SetFocus (hWnd);
				SendMessage (hWnd, EM_SETSEL, 0, -1);
			}
			return 0;

		case IDC_REVERT:
			hWnd = GetDlgItem (hDlg, IDC_CURRENTENVIRONMENT);
			env = (ReverbContainer *)SendMessage (hWnd, CB_GETITEMDATA,
				SendMessage (hWnd, CB_GETCURSEL, 0, 0), 0);
			env->Properties = SavedProperties;
			UpdateControls (env, hDlg);
			return 0;

		case IDC_SAVE:
			hWnd = GetDlgItem (hDlg, IDC_CURRENTENVIRONMENT);
			SaveEnvironments (hDlg,
				(ReverbContainer *)SendMessage (hWnd, CB_GETITEMDATA,
					SendMessage (hWnd, CB_GETCURSEL, 0, 0), 0));
			return 0;

		case IDC_TESTEAX:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				eaxedit_test = (SendMessage ((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);
			}
			break;
			
		case IDC_SHOWIDS:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				hWnd = GetDlgItem (hDlg, IDC_CURRENTENVIRONMENT);
				PopulateEnvDropDown (hWnd,
					SendMessage ((HWND)lParam, BM_GETCHECK, 0, 0)==BST_CHECKED,
					(ReverbContainer *)SendMessage (hWnd, CB_GETITEMDATA,
						SendMessage (hWnd, CB_GETCURSEL, 0, 0), 0));
				EAXProc (hDlg, WM_COMMAND, MAKELONG(IDC_CURRENTENVIRONMENT,CBN_SELENDOK),
					(LPARAM)hWnd);
				return 0;
			}
			break;

		case IDC_CURRENTENVIRONMENT:
			if (HIWORD(wParam) == CBN_SELENDOK)
			{
				env = (ReverbContainer *)SendMessage ((HWND)lParam, CB_GETITEMDATA,
					SendMessage ((HWND)lParam, CB_GETCURSEL, 0, 0), 0);
				UpdateControls (env, hDlg);
			}
			return 0;
		}
		return 1;

	case WM_GETMINMAXINFO:
		((MINMAXINFO *)lParam)->ptMaxTrackSize.x =
		((MINMAXINFO *)lParam)->ptMinTrackSize.x = EditWindowSize.x;
		((MINMAXINFO *)lParam)->ptMaxTrackSize.y = EditWindowSize.y + PropListMaxSize.y + 5;
		GetClientRect (GetDlgItem (hDlg, IDOK), &rect);
		((MINMAXINFO *)lParam)->ptMinTrackSize.y = rect.bottom * 10;
		return 0;
	}
	return FALSE;
}

void ShowEAXEditor ()
{
	EAXEditWindow = CreateDialog (g_hInst, MAKEINTRESOURCE(IDD_EAXEDIT), Window, EAXProc);
}

extern int NewWidth, NewHeight, NewBits, DisplayBits;



DEFINE_ACTION_FUNCTION(DReverbEdit, GetValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	ACTION_RETURN_FLOAT(0);
	return 1;
}

DEFINE_ACTION_FUNCTION(DReverbEdit, SetValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	PARAM_FLOAT(value);
	ACTION_RETURN_FLOAT(value);
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

// These are for internal use only and not supposed to be user-settable
CVAR(String, reverbedit_name, "", CVAR_NOSET);
CVAR(Int, reverbedit_id1, 0, CVAR_NOSET);
CVAR(Int, reverbedit_id2, 0, CVAR_NOSET);

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
	int id = (reverbedit_id1 * 255) + reverbedit_id2;
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

