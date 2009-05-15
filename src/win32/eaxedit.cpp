#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500
#define _WIN32_IE 0x0500
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define USE_WINDOWS_DWORD
#include "resource.h"
#include "s_sound.h"
#include "templates.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "c_cvars.h"
#include "doomstat.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

// More w32api lackings
#ifndef TTM_SETTITLE
#define TTM_SETTITLEA (WM_USER+32)
#define TTM_SETTITLEW (WM_USER+33)
#ifdef UNICODE
#define TTM_SETTITLE TTM_SETTITLEW
#else
#define TTM_SETTITLE TTM_SETTITLEA
#endif
#endif
#ifndef TTF_TRACK
#define TTF_TRACK 0x0020
#endif
#ifndef TTF_TRANSPARENT
#define TTF_TRANSPARENT 0x0100
#endif

#ifndef OFN_ENABLESIZING
#define OFN_ENABLESIZING 0x00800000
typedef struct w32apiFixedOFNA {
	DWORD lStructSize;
	HWND hwndOwner;
	HINSTANCE hInstance;
	LPCSTR lpstrFilter;
	LPSTR lpstrCustomFilter;
	DWORD nMaxCustFilter;
	DWORD nFilterIndex;
	LPSTR lpstrFile;
	DWORD nMaxFile;
	LPSTR lpstrFileTitle;
	DWORD nMaxFileTitle;
	LPCSTR lpstrInitialDir;
	LPCSTR lpstrTitle;
	DWORD Flags;
	WORD nFileOffset;
	WORD nFileExtension;
	LPCSTR lpstrDefExt;
	LPARAM lCustData;
	LPOFNHOOKPROC lpfnHook;
	LPCSTR lpTemplateName;
	void *pvReserved;
	DWORD dwReserved;
	DWORD FlagsEx;
} FIXEDOPENFILENAMEA;
#define OPENFILENAME FIXEDOPENFILENAMEA
#endif

extern HINSTANCE g_hInst;
extern HWND Window;
extern bool ForceWindowed;
EXTERN_CVAR (Bool, fullscreen)
extern ReverbContainer *ForcedEnvironment;

HWND EAXEditWindow;
HWND hPropList;
POINT PropListMaxSize;
LONG PropListHeightDiff;
POINT EditWindowSize;
POINT DoneLocation;
WNDPROC StdEditProc;
LONG NewLeft;
LONG SaveLeft;
LONG RevertLeft;
POINT TestLocation;

bool SpawnEAXWindow;

REVERB_PROPERTIES SavedProperties;
ReverbContainer *CurrentEnv;

CUSTOM_CVAR (Bool, eaxedit_test, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (EAXEditWindow != 0)
	{
		CheckDlgButton (EAXEditWindow, IDC_TESTEAX, self ? BST_CHECKED : BST_UNCHECKED);
		if (self)
		{
			ForcedEnvironment = CurrentEnv;
		}
		else
		{
			ForcedEnvironment = NULL;
		}
	}
}

struct MySaveData
{
	const ReverbContainer *Default;
	const ReverbContainer **Saves;
	UINT NumSaves;

	MySaveData (const ReverbContainer *def)
		: Default (def), Saves (NULL), NumSaves (0) {}
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
	HWND CheckboxHWND;
};

void PopulateEnvDropDown (HWND hCtl);
void SetupEnvControls (HWND hDlg);
void UpdateControls (const ReverbContainer *env, HWND hDlg);
void UpdateControl (const EnvControl *control, int value, bool slider);

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

LRESULT AddEnvToDropDown (HWND hCtl, bool showID, const ReverbContainer *env)
{
	char buff[128];
	LRESULT i;

	if (showID)
	{
		mysnprintf (buff, countof(buff), "(%3d,%3d) %s", HIBYTE(env->ID), LOBYTE(env->ID), env->Name);
		i = SendMessage (hCtl, CB_ADDSTRING, 0, (LPARAM)buff);
	}
	else
	{
		i = SendMessage (hCtl, CB_ADDSTRING, 0, (LPARAM)env->Name);
	}
	SendMessage (hCtl, CB_SETITEMDATA, i, (LPARAM)env);
	return i;
}

void PopulateEnvDropDown (HWND hCtl, bool showIDs, const ReverbContainer *defEnv)
{
	const ReverbContainer *env;
	WPARAM envCount = 0;
	LPARAM strCount = 0;
	LRESULT i;
	
	for (env = Environments; env != NULL; env = env->Next)
	{
		envCount++;
		strCount += strlen (env->Name) + 1;
	}

	SendMessage (hCtl, WM_SETREDRAW, FALSE, 0);
	SendMessage (hCtl, CB_RESETCONTENT, 0, 0);
	SendMessage (hCtl, CB_INITSTORAGE, envCount, showIDs ?
		strCount + envCount * 10 : strCount);

	for (env = Environments; env != NULL; env = env->Next)
	{
		AddEnvToDropDown (hCtl, showIDs, env);
	}

	SendMessage (hCtl, WM_SETREDRAW, TRUE, 0);

	if (defEnv == NULL)
	{
		SendMessage (hCtl, CB_SETCURSEL, 0, 0);
		SetFocus (hCtl);
	}
	else
	{
		for (i = 0; i < (LPARAM)envCount; ++i)
		{
			if ((const ReverbContainer  *)SendMessage (hCtl, CB_GETITEMDATA, i, 0)
				== defEnv)
			{
				SendMessage (hCtl, CB_SETCURSEL, i, 0);
				break;
			}
		}
	}
}

void SetIDEdits (HWND hDlg, WORD id)
{
	char text[4];

	mysnprintf (text, countof(text), "%d", HIBYTE(id));
	SendMessage (GetDlgItem (hDlg, IDC_EDITID1), WM_SETTEXT, 0, (LPARAM)text);
	mysnprintf (text, countof(text), "%d", LOBYTE(id));
	SendMessage (GetDlgItem (hDlg, IDC_EDITID2), WM_SETTEXT, 0, (LPARAM)text);
}

void PopulateEnvList (HWND hCtl, bool show, const ReverbContainer *defEnv)
{
	const ReverbContainer *env;
	int i;
	LVITEM item;
	
	SendMessage (hCtl, WM_SETREDRAW, FALSE, 0);

	item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
	item.stateMask = LVIS_SELECTED|LVIS_FOCUSED;
	item.iSubItem = 0;

	for (env = Environments, i = 0; env != NULL; env = env->Next, ++i)
	{
		if (!env->Builtin || show)
		{
			item.iItem = i;
			item.pszText = (LPSTR)env->Name;
			item.lParam = (LPARAM)env;
			item.state = env == defEnv ? LVIS_SELECTED|LVIS_FOCUSED : 0;
			SendMessage (hCtl, LVM_INSERTITEM, 0, (LPARAM)&item);
		}
	}

	SendMessage (hCtl, WM_SETREDRAW, TRUE, 0);
}

WORD FirstFreeID (WORD base, bool builtin)
{
	int tryCount = 0;
	int priID = HIBYTE(base);

	// If the original sound is built-in, start searching for a new
	// primary ID at 30.
	if (builtin)
	{
		for (priID = 30; priID < 256; ++priID)
		{
			if (S_FindEnvironment (priID << 8) == NULL)
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
		WORD lastID = Environments->ID;
		const ReverbContainer *env = Environments->Next;

		// Find the lowest-numbered free ID with the same primary ID as base
		// If none are available, add 100 to base's primary ID and try again.
		// If that fails, then the primary ID gets incremented
		// by 1 until a match is found. If all the IDs searchable by this
		// algorithm are in use, then you're in trouble.

		while (env != NULL)
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

LRESULT CALLBACK EditControlProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char buff[16];
	double val;
	const EnvControl *env;
	MSG *msg;

	switch (uMsg)
	{
	case WM_GETDLGCODE:
		msg = (MSG *)lParam;
		if (msg && LOWORD(msg->message) == WM_KEYDOWN)
		{
			// Do not close the dialog if Enter was pressed
			// inside an edit control.
			if (msg->wParam == VK_RETURN || msg->wParam == VK_ESCAPE)
			{
				return DLGC_WANTALLKEYS;
			}
		}
		break;

	case WM_CHAR:
		// Only allow numeric symbols.
		if (wParam >= ' ' && wParam <= 127 &&
			(wParam < '0' || wParam > '9') &&
			wParam != '.' &&
			wParam != '-' &&
			wParam != '+')
		{
			return 0;
		}
		break;

	case WM_KEYDOWN:
		if (wParam != VK_RETURN)
		{
			if (wParam == VK_ESCAPE)
			{ // Put the original value back in the edit control.
				env = (const EnvControl *)(LONG_PTR)GetWindowLongPtr (hWnd, GWLP_USERDATA);
				int vali = SendMessage (env->SliderHWND, TBM_GETPOS, 0, 0);
				if (env->Float)
				{
					mysnprintf (buff, countof(buff), "%d.%03d", vali/1000, abs(vali%1000));
				}
				else
				{
					mysnprintf (buff, countof(buff), "%d", vali);
				}
				CallWindowProc (StdEditProc, hWnd, WM_SETTEXT, 0, (LPARAM)buff);
				CallWindowProc (StdEditProc, hWnd, EM_SETSEL, 0, -1);
				return 0;
			}
			break;
		}

		// intentional fallthrough

	case WM_KILLFOCUS:
		// Validate the new value and update the corresponding slider.
		env = (const EnvControl *)(LONG_PTR)GetWindowLongPtr (hWnd, GWLP_USERDATA);
		val = 0.0;
		if (CallWindowProc (StdEditProc, hWnd, WM_GETTEXT, 16, (LPARAM)buff) > 0)
		{
			val = atof (buff);
		}
		if (env->Float)
		{
			val *= 1000.0;
		}
		if (val < env->Min) val = env->Min;
		else if (val > env->Max) val = env->Max;
		UpdateControl (env, int(val), true);
		break;
	}
	return CallWindowProc (StdEditProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK EditControlProcNoDeci (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// This is just like the above WndProc, except it only accepts integers.
	if (uMsg == WM_CHAR && wParam == '.')
	{
		return 0;
	}
	return EditControlProc (hWnd, uMsg, wParam, lParam);
}

void SetupEnvControls (HWND hDlg)
{
	size_t i;

	for (i = 0; i < countof(EnvControls); ++i)
	{
		if (EnvControls[i].EditControl == 0)
			continue;

		EnvControls[i].EditHWND = GetDlgItem (hDlg, EnvControls[i].EditControl);
		EnvControls[i].SliderHWND = GetDlgItem (hDlg, EnvControls[i].SliderControl);
		SendMessage (EnvControls[i].SliderHWND, TBM_SETRANGEMIN, FALSE, EnvControls[i].Min);
		SendMessage (EnvControls[i].SliderHWND, TBM_SETRANGEMAX, TRUE, EnvControls[i].Max);
		SendMessage (EnvControls[i].EditHWND, EM_LIMITTEXT, 10, 0);
		StdEditProc = (WNDPROC)(LONG_PTR)SetWindowLongPtr (EnvControls[i].EditHWND, GWLP_WNDPROC,
			(LONG_PTR)(EnvControls[i].Float ? EditControlProc : EditControlProcNoDeci));
		SetWindowLongPtr (EnvControls[i].EditHWND, GWLP_USERDATA, (LONG_PTR)&EnvControls[i]);
		SetWindowLongPtr (EnvControls[i].SliderHWND, GWLP_USERDATA, (LONG_PTR)&EnvControls[i]);
	}
	for (i = 0; i < countof(EnvFlags); ++i)
	{
		EnvFlags[i].CheckboxHWND = GetDlgItem (hDlg, EnvFlags[i].CheckboxControl);
		SetWindowLongPtr (EnvFlags[i].CheckboxHWND, GWLP_USERDATA, (LONG_PTR)&EnvFlags[i]);
	}
}

void UpdateControl (const EnvControl *control, int value, bool slider)
{
	char buff[16];

	if (slider)
	{
		SendMessage (control->SliderHWND, TBM_SETPOS, TRUE, value);
	}
	if (control->Float)
	{
		mysnprintf (buff, countof(buff), "%d.%03d", value/1000, abs(value%1000));
		if (CurrentEnv != NULL)
		{
			CurrentEnv->Properties.*control->Float = float(value) / 1000.0;
		}
	}
	else
	{
		mysnprintf (buff, countof(buff), "%d", value);
		if (CurrentEnv != NULL)
		{
			CurrentEnv->Properties.*control->Int = value;
		}
	}
	SendMessage (control->EditHWND, WM_SETTEXT, 0, (LPARAM)buff);
	if (CurrentEnv != NULL)
	{
		CurrentEnv->Modified = true;
	}
}

void UpdateControls (ReverbContainer *env, HWND hDlg)
{
	char buff[4];
	size_t i;

	if (env == NULL)
		return;

	CurrentEnv = NULL;

	for (i = 0; i < countof(EnvControls); ++i)
	{
		EnvControl *ctrl = &EnvControls[i];

		if (ctrl->Float)
		{
			float v = env->Properties.*ctrl->Float * 1000;
			UpdateControl (ctrl, int(v >= 0.0 ? v + 0.5 : v - 0.5), true);
		}
		else
		{
			UpdateControl (ctrl, env->Properties.*ctrl->Int, true);
		}
		EnableWindow (ctrl->EditHWND, !env->Builtin);
		EnableWindow (ctrl->SliderHWND, !env->Builtin);
	}
	for (i = 0; i < countof(EnvFlags); ++i)
	{
		SendMessage (EnvFlags[i].CheckboxHWND, BM_SETCHECK,
			(env->Properties.Flags & EnvFlags[i].Flag) ? BST_CHECKED : BST_UNCHECKED, 0);
		EnableWindow (EnvFlags[i].CheckboxHWND, !env->Builtin);
	}
	EnableWindow (GetDlgItem (hDlg, IDC_REVERT), !env->Builtin);

	mysnprintf (buff, countof(buff), "%d", HIBYTE(env->ID));
	SendMessage (GetDlgItem (hDlg, IDC_ID1), WM_SETTEXT, 0, (LPARAM)buff);

	mysnprintf (buff, countof(buff), "%d", LOBYTE(env->ID));
	SendMessage (GetDlgItem (hDlg, IDC_ID2), WM_SETTEXT, 0, (LPARAM)buff);

	SavedProperties = env->Properties;
	CurrentEnv = env;

	if (SendMessage (GetDlgItem (hDlg, IDC_TESTEAX), BM_GETCHECK, 0,0) == BST_CHECKED)
	{
		ForcedEnvironment = env;
	}
}

INT_PTR CALLBACK EAXProp (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	const EnvControl *env;
	SCROLLINFO si;
	int yPos;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		SetupEnvControls (hDlg);
		return FALSE;

	case WM_VSCROLL:
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		GetScrollInfo (hDlg, SB_VERT, &si);
		yPos = si.nPos;

		switch (LOWORD(wParam))
		{
		case SB_TOP:		si.nPos = si.nMin;		break;
		case SB_BOTTOM:		si.nPos = si.nMax;		break;
		case SB_LINEUP:		si.nPos -= 16;			break;
		case SB_LINEDOWN:	si.nPos += 16;			break;
		case SB_PAGEUP:		si.nPos -= si.nPage;	break;
		case SB_PAGEDOWN:	si.nPos += si.nPage;	break;
		case SB_THUMBTRACK:	si.nPos = si.nTrackPos;	break;
		}
		si.fMask = SIF_POS;
		SetScrollInfo (hDlg, SB_VERT, &si, TRUE);
		GetScrollInfo (hDlg, SB_VERT, &si);
		if (si.nPos != yPos)
		{
			ScrollWindow (hDlg, 0, yPos - si.nPos, NULL, NULL);
			UpdateWindow (hDlg);
		}
		return TRUE;

	case WM_HSCROLL:
		env = (const EnvControl *)(LONG_PTR)GetWindowLongPtr ((HWND)lParam, GWLP_USERDATA);
		UpdateControl (env, SendMessage ((HWND)lParam, TBM_GETPOS, 0, 0), false);
		return TRUE;

	case WM_SIZE:
		if (wParam != SIZE_MAXSHOW && wParam != SIZE_MAXHIDE)
		{
			si.cbSize = sizeof(si);
			si.fMask = SIF_POS;
			GetScrollInfo (hDlg, SB_VERT, &si);
			yPos = si.nPos;

			// If we let the scroll bar disappear, then when that happens,
			// it will resize the window before SetScrollInfo returns.
			// Rather than checking for recursive WM_SIZE messages, I choose
			// to force the scroll bar to always be present. The dialog is
			// really designed with the appearance of the scroll bar in
			// mind anyway, since it does nothing to resize the controls
			// horizontally.

			si.fMask = SIF_POS|SIF_PAGE|SIF_DISABLENOSCROLL;
			si.nPage = HIWORD(lParam);

			SetScrollInfo (hDlg, SB_VERT, &si, TRUE);
			GetScrollInfo (hDlg, SB_VERT, &si);
			if (si.nPos != yPos)
			{
				ScrollWindow (hDlg, 0, yPos - si.nPos, NULL, NULL);
				UpdateWindow (hDlg);
			}
		}
		return TRUE;

	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED && CurrentEnv != NULL)
		{
			for (size_t i = 0; i < countof(EnvFlags); ++i)
			{
				if ((HWND)lParam == EnvFlags[i].CheckboxHWND)
				{
					if (SendMessage ((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED)
					{
						CurrentEnv->Properties.Flags |= EnvFlags[i].Flag;
					}
					else
					{
						CurrentEnv->Properties.Flags &= ~EnvFlags[i].Flag;
					}
					return TRUE;;
				}
			}
		}
		return FALSE;
	}
	return FALSE;
}

void SuggestNewName (const ReverbContainer *env, HWND hEdit)
{
	const ReverbContainer *probe = NULL;
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
		while (probe != NULL)
		{
			if (stricmp (probe->Name, text) == 0)
				break;
			probe = probe->Next;
		}
		if (probe == NULL)
		{
			break;
		}
	}

	if (probe == NULL)
	{
		SetWindowText (hEdit, text);
	}
}

void ShowErrorTip (HWND ToolTip, TOOLINFO &ti, HWND hDlg, const char *title)
{
	SendMessage (ToolTip, TTM_SETTITLE, 3, (LPARAM)title);
	
	ti.cbSize = sizeof(ti);
	ti.hinst = g_hInst;
	SendMessage (ToolTip, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
	GetClientRect (ti.hwnd, &ti.rect);
	POINT pt = { (ti.rect.left + ti.rect.right) / 2, ti.rect.bottom - 4 };
	ClientToScreen (ti.hwnd, &pt);
	SendMessage (ToolTip, TTM_TRACKACTIVATE, FALSE, 0);
	SendMessage (ToolTip, TTM_TRACKPOSITION, 0, MAKELONG(pt.x, pt.y));
	SendMessage (ToolTip, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);
	SetFocus (ti.hwnd);
	SendMessage (ti.hwnd, EM_SETSEL, 0, -1);
	MessageBeep (MB_ICONEXCLAMATION);
	SetTimer (hDlg, 11223, 10000, NULL);
}

INT_PTR CALLBACK NewEAXProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	const ReverbContainer *rev;
	TOOLINFO ti;
	char buff[33], buff4[4];
	int id1, id2;
	static HWND ToolTip;
	HWND hWnd;
	int i;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		hWnd = GetDlgItem (hDlg, IDC_ENVIRONMENTLIST);
		PopulateEnvList (hWnd, true, (const ReverbContainer *)lParam);
		SendMessage (GetDlgItem (hDlg, IDC_SPINID1), UDM_SETRANGE, 0, MAKELONG(255,0));
		SendMessage (GetDlgItem (hDlg, IDC_SPINID2), UDM_SETRANGE, 0, MAKELONG(255,0));
		SendMessage (GetDlgItem (hDlg, IDC_EDITID1), EM_LIMITTEXT, 3, 0);
		SendMessage (GetDlgItem (hDlg, IDC_EDITID2), EM_LIMITTEXT, 3, 0);

		hWnd = GetDlgItem (hDlg, IDC_NEWENVNAME);
		SendMessage (hWnd, EM_LIMITTEXT, 31, 0);
		SuggestNewName ((const ReverbContainer *)lParam, hWnd);
		SetIDEdits (hDlg, FirstFreeID (((const ReverbContainer *)lParam)->ID,
			((const ReverbContainer *)lParam)->Builtin));

		ToolTip = CreateWindowEx (WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
			WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP|TTS_BALLOON|TTS_CLOSE,
			0, 0, 0, 0,
			hDlg, NULL, g_hInst, NULL);
		if (ToolTip)
		{
			char zero = '\0';
			ti.cbSize = sizeof(ti);
			ti.uFlags = TTF_TRACK | TTF_TRANSPARENT;
			ti.hinst = g_hInst;
			ti.lpszText = &zero;
			for (i = 0; i < 3; ++i)
			{
				ti.uId = i;
				ti.hwnd = GetDlgItem (hDlg, IDC_NEWENVNAME+i);
				GetClientRect (ti.hwnd, &ti.rect);
				SendMessage (ToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
			}
		}

		return 1;

	case WM_TIMER:
		if (wParam == 11223)
		{
			SendMessage (ToolTip, TTM_TRACKACTIVATE, FALSE, 0);
			KillTimer (hDlg, 11223);
		}
		return 1;

	case WM_MOVE:
		// Just hide the tool tip if the user moves the window.
		SendMessage (ToolTip, TTM_TRACKACTIVATE, FALSE, 0);
		return 1;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			id1 = 256; id2 = 256;
			buff[0] = 0;
			if (0 == GetWindowText (GetDlgItem (hDlg, IDC_NEWENVNAME), buff, 32) ||
				S_FindEnvironment (buff) != NULL)
			{
				static CHAR text0[] = "That name is already used.";
				static CHAR text1[] = "Please enter a name.";
				ti.uId = 0;
				ti.hwnd = GetDlgItem (hDlg, IDC_NEWENVNAME);
				ti.lpszText = buff[0] ? text0 : text1;
				ShowErrorTip (ToolTip, ti, hDlg, "Bad Name");
				return 0;
			}
			if (0 < GetWindowText (GetDlgItem (hDlg, IDC_EDITID1), buff4, 4))
			{
				id1 = atoi (buff4);
			}
			if (0 < GetWindowText (GetDlgItem (hDlg, IDC_EDITID2), buff4, 4))
			{
				id2 = atoi (buff4);
			}
			if (id1 > 255 || id2 > 255)
			{
				static CHAR text[] = "Please enter a number between 0 and 255.";
				ti.uId = id1 > 255 ? 1 : 2;
				ti.hwnd = GetDlgItem (hDlg, IDC_EDITID1 + ti.uId - 1);
				ti.lpszText = text;
				ShowErrorTip (ToolTip, ti, hDlg, "Bad Value");
			}
			else if (NULL != (rev = S_FindEnvironment (MAKEWORD (id2, id1))))
			{
				static char foo[80];
				ti.uId = 2;
				ti.hwnd = GetDlgItem (hDlg, IDC_EDITID2);
				mysnprintf (foo, countof(foo), "This ID is already used by \"%s\".", rev->Name);
				ti.lpszText = foo;
				ShowErrorTip (ToolTip, ti, hDlg, "Bad ID");
			}
			else
			{
				LVITEM item;

				hWnd = GetDlgItem (hDlg, IDC_ENVIRONMENTLIST);
				i = SendMessage (hWnd, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_ALL|LVNI_SELECTED);

				if (i == -1)
				{
					MessageBeep (MB_ICONEXCLAMATION);
					return 0;
				}

				item.iItem = i;
				item.iSubItem = 0;
				item.mask = LVIF_PARAM;
				if (!SendMessage (hWnd, LVM_GETITEM, 0, (LPARAM)&item))
				{
					MessageBeep (MB_ICONEXCLAMATION);
					return 0;
				}

				ReverbContainer *env = new ReverbContainer;
				rev = (ReverbContainer *)item.lParam;

				env->Builtin = false;
				env->ID = MAKEWORD (id2, id1);
				env->Name = copystring (buff);
				env->Next = NULL;
				env->Properties = rev->Properties;
				S_AddEnvironment (env);

				EndDialog (hDlg, (INT_PTR)env);
			}
			return 0;

		case IDCANCEL:
			EndDialog (hDlg, 0);
			break;
		}
		return 0;

	case WM_NOTIFY:
		if (wParam == IDC_ENVIRONMENTLIST && ((LPNMHDR)lParam)->code == LVN_ITEMCHANGED)
		{
			LPNMLISTVIEW nmlv = (LPNMLISTVIEW)lParam;

			if (nmlv->iItem != -1 &&
				(nmlv->uNewState & LVIS_SELECTED) &&
				!(nmlv->uOldState & LVIS_SELECTED))
			{
				SuggestNewName ((const ReverbContainer *)nmlv->lParam, GetDlgItem (hDlg, IDC_NEWENVNAME));
				SetIDEdits (hDlg, FirstFreeID (((const ReverbContainer *)nmlv->lParam)->ID,
					((const ReverbContainer *)lParam)->Builtin));
			}
		}
		return 0;
	}
	return FALSE;
}

UINT_PTR CALLBACK SaveHookProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static LONG ButtonsLeftOffset;
	static LONG ListRightOffset;
	static LONG GroupRightOffset;

	MySaveData *msd;
	HWND hWnd;
	LVITEM item;
	int i, count;
	RECT rect1, rect2;
	UINT ns;

	switch (msg)
	{
	case WM_INITDIALOG:
		GetClientRect (hDlg, &rect1);
		GetWindowRect (GetDlgItem (hDlg, IDC_SELECTALL), &rect2);
		ScreenToClient (hDlg, (LPPOINT)&rect2.left);
		ButtonsLeftOffset = rect2.left - rect1.right;
		GetWindowRect (GetDlgItem (hDlg, IDC_ENVLIST), &rect2);
		ListRightOffset = (rect2.right - rect2.left) - rect1.right;
		GetWindowRect (GetDlgItem (hDlg, IDC_SAVEGROUP), &rect2);
		GroupRightOffset = (rect2.right - rect2.left) - rect1.right;

		hWnd = GetDlgItem (hDlg, IDC_ENVLIST);
		PopulateEnvList (hWnd, false,
			((MySaveData *)(((LPOPENFILENAME)lParam)->lCustData))->Default);
		SetWindowLongPtr (hDlg, DWLP_USER, (LONG_PTR)((LPOPENFILENAME)lParam)->lCustData);
		return 1;

	case WM_ERASEBKGND:
		SetWindowLongPtr (hDlg, DWLP_MSGRESULT, 1);
		return 1;

	case WM_SIZE:
		if (wParam != SIZE_MAXHIDE && wParam != SIZE_MAXSHOW)
		{
			GetWindowRect (hWnd = GetDlgItem (hDlg, IDC_SAVEGROUP), &rect1);
			SetWindowPos (hWnd, NULL, 0, 0, LOWORD(lParam) + GroupRightOffset, rect1.bottom-rect1.top, SWP_NOMOVE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_DEFERERASE);

			GetWindowRect (hWnd = GetDlgItem (hDlg, IDC_SELECTALL), &rect1);
			ScreenToClient (hDlg, (LPPOINT)&rect1.left);
			SetWindowPos (hWnd, NULL, LOWORD(lParam)+ButtonsLeftOffset, rect1.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER);

			GetWindowRect (hWnd = GetDlgItem (hDlg, IDC_SELECTNONE), &rect1);
			ScreenToClient (hDlg, (LPPOINT)&rect1.left);
			SetWindowPos (hWnd, NULL, LOWORD(lParam)+ButtonsLeftOffset, rect1.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER);

			GetWindowRect (hWnd = GetDlgItem (hDlg, IDC_ENVLIST), &rect1);
			SetWindowPos (hWnd, NULL, 0, 0, LOWORD(lParam) + ListRightOffset, rect1.bottom-rect1.top, SWP_DEFERERASE|SWP_NOMOVE|SWP_NOZORDER|SWP_NOOWNERZORDER);
		}
		return 1;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_SELECTALL:
		case IDC_SELECTNONE:
			hWnd = GetDlgItem (hDlg, IDC_ENVLIST);
			SendMessage (hWnd, WM_SETREDRAW, FALSE, 0);
			count = ListView_GetItemCount (hWnd);
			item.iSubItem = 0;
			item.mask = LVIF_STATE;
			item.stateMask = LVIS_SELECTED;
			item.state = LOWORD(wParam)==IDC_SELECTALL ? LVIS_SELECTED : 0;
			for (i = 0; i < count; ++i)
			{
				item.iItem = i;
				ListView_SetItem (hWnd, &item);
			}
			if (LOWORD(wParam) == IDC_SELECTALL)
			{
				SetFocus (hWnd);
			}
			SendMessage (hWnd, WM_SETREDRAW, TRUE, 0);
			return 1;
		}
		return 0;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case CDN_INITDONE:
			hWnd = GetParent (hDlg);
			GetWindowRect (hWnd, &rect1);
			GetWindowRect (hDlg, &rect2);
			SetWindowPos (hDlg, NULL, 0, 0, rect1.right-rect1.left, rect2.bottom-rect2.top, SWP_NOMOVE|SWP_NOZORDER|SWP_NOOWNERZORDER);
			return 1;

		case CDN_FILEOK:
			msd = (MySaveData *)(LONG_PTR)GetWindowLongPtr (hDlg, DWLP_USER);
			hWnd = GetDlgItem (hDlg, IDC_ENVLIST);
			ns = ListView_GetSelectedCount (hWnd);
			if (ns == 0)
			{
				msd->NumSaves = 0;
				if (IDNO == MessageBox (hDlg,
					"You have not selected any EAX environments to save.\n"
					"Do you want to cancel the save operation?",
					"Nothing Selected",
					MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2))
				{
					SetWindowLongPtr (hDlg, DWLP_MSGRESULT, 1);
					return 1;
				}
			}
			else
			{
				UINT x;
				msd->Saves = new const ReverbContainer *[ns];
				item.iItem = -1;
				item.iSubItem = 0;
				item.mask = LVIF_PARAM;

				for (x = 0; x != ns; )
				{
					item.iItem = ListView_GetNextItem (hWnd, item.iItem, LVNI_SELECTED);
					if (item.iItem != -1 && ListView_GetItem (hWnd, &item))
					{
						msd->Saves[x++] = (const ReverbContainer *)item.lParam;
					}
				}
				msd->NumSaves = x;
			}
			return 0;
		}
		return 0;

	default:
		return 0;
	}
}

void ExportEnvironments (const char *filename, UINT count, const ReverbContainer **envs)
{
	FILE *f;

retry:
	f = fopen (filename, "w");

	if (f != NULL)
	{
		for (UINT i = 0; i < count; ++i)
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
				base = NULL;
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
			NULL,
			GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0,
			NULL);
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
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = sizeof(filename);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
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

INT_PTR CALLBACK EAXProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
			(LPSTR)NULL,
			WS_CHILD | WS_VISIBLE | SBS_SIZEGRIP | SBS_SIZEBOXBOTTOMRIGHTALIGN | WS_CLIPSIBLINGS,
			0,0,rect.right,rect.bottom,
			hDlg,
			(HMENU)IDC_SIZEBOX,
			g_hInst,
			NULL);

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
		PopulateEnvDropDown (hWnd, IsDlgButtonChecked (hDlg, IDC_SHOWIDS)==BST_CHECKED, NULL);
		EAXProc (hDlg, WM_COMMAND, MAKELONG(IDC_CURRENTENVIRONMENT,CBN_SELENDOK), (LPARAM)hWnd);

		CheckDlgButton (hDlg, IDC_TESTEAX, eaxedit_test ? BST_CHECKED : BST_UNCHECKED);
		return 0;

	case WM_SIZE:
		if (wParam != SIZE_MAXHIDE && wParam != SIZE_MAXSHOW)
		{
			GetClientRect (hWnd = GetDlgItem (hDlg, IDC_SIZEBOX), &rect);
			SetWindowPos (hWnd, HWND_BOTTOM, LOWORD(lParam)-rect.right, HIWORD(lParam)-rect.bottom, 0, 0, SWP_NOSIZE);

			SetWindowPos (hPropList, NULL, 0, 0, PropListMaxSize.x, HIWORD(lParam)-PropListHeightDiff, SWP_NOMOVE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_DEFERERASE);
			SetWindowPos (GetDlgItem (hDlg, IDOK), NULL, LOWORD(lParam)-DoneLocation.x, HIWORD(lParam)-DoneLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
			SetWindowPos (GetDlgItem (hDlg, IDC_NEW), NULL, NewLeft, HIWORD(lParam)-DoneLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
			SetWindowPos (GetDlgItem (hDlg, IDC_SAVE), NULL, SaveLeft, HIWORD(lParam)-DoneLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
			SetWindowPos (GetDlgItem (hDlg, IDC_REVERT), NULL, RevertLeft, HIWORD(lParam)-DoneLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
			SetWindowPos (GetDlgItem (hDlg, IDC_TESTEAX), NULL, LOWORD(lParam)-TestLocation.x, HIWORD(lParam)-TestLocation.y, 0, 0, SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOSIZE);
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
		ForcedEnvironment = NULL;
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
			if (env != NULL)
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

CCMD (reverbedit)
{
	if (EAXEditWindow != 0)
	{
		SetForegroundWindow (EAXEditWindow);
	}
	else
	{
		ForceWindowed = true;
		if (fullscreen)
		{
			setmodeneeded = true;
			SpawnEAXWindow = true;
		}
		else
		{
			ShowEAXEditor ();
		}
	}
}
