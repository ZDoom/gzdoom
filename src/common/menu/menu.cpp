/*
** menu.cpp
** Menu base class and global interface
**
**---------------------------------------------------------------------------
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

#include "c_dispatch.h"
#include "d_gui.h"
#include "c_buttons.h"
#include "c_console.h"
#include "c_bind.h"
#include "d_eventbase.h"
#include "g_input.h"
#include "configfile.h"
#include "gstrings.h"
#include "menu.h"
#include "vm.h"
#include "v_video.h"
#include "i_system.h"
#include "types.h"
#include "texturemanager.h"
#include "v_draw.h"
#include "vm.h"
#include "gamestate.h"
#include "i_interface.h"
#include "menustate.h"
#include "i_time.h"
#include "printf.h"

void M_StartControlPanel(bool makeSound, bool scaleoverride = false);

int DMenu::InMenu;
static ScaleOverrider *CurrentScaleOverrider;
extern int chatmodeon;
//
// Todo: Move these elsewhere
//
CVAR (Int, m_showinputgrid, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, m_blockcontrollers, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR (Float, snd_menuvolume, 0.6f, CVAR_ARCHIVE)
CVAR(Int, m_use_mouse, 2, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Int, m_show_backbutton, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

static DMenu *GetCurrentMenu()
{
	return CurrentMenu;
}

DEFINE_ACTION_FUNCTION_NATIVE(DMenu, GetCurrentMenu, GetCurrentMenu)
{
	ACTION_RETURN_OBJECT(CurrentMenu);
}

static int GetMenuTime()
{
	return MenuTime;
}

DEFINE_ACTION_FUNCTION_NATIVE(DMenu, MenuTime, GetMenuTime)
{
	ACTION_RETURN_INT(MenuTime);
}

EMenuState		menuactive;
FButtonStatus	MenuButtons[NUM_MKEYS];
int				MenuButtonTickers[NUM_MKEYS];
bool			MenuButtonOrigin[NUM_MKEYS];
int				BackbuttonTime;
float			BackbuttonAlpha;
static bool		MenuEnabled = true;
DMenu			*CurrentMenu;
int				MenuTime;

extern PClass *DefaultListMenuClass;
extern PClass *DefaultOptionMenuClass;


#define KEY_REPEAT_DELAY	(GameTicRate*5/12)
#define KEY_REPEAT_RATE		(3)

bool OkForLocalization(FTextureID texnum, const char* substitute);

//============================================================================
//
//
//
//============================================================================

IMPLEMENT_CLASS(DMenuDescriptor, false, false)
IMPLEMENT_CLASS(DListMenuDescriptor, false, false)
IMPLEMENT_CLASS(DOptionMenuDescriptor, false, false)

DMenuDescriptor *GetMenuDescriptor(int name)
{
	DMenuDescriptor **desc = MenuDescriptors.CheckKey(ENamedName(name));
	return desc ? *desc : nullptr;
}

DEFINE_ACTION_FUNCTION_NATIVE(DMenuDescriptor, GetDescriptor, GetMenuDescriptor)
{
	PARAM_PROLOGUE;
	PARAM_NAME(name);
	ACTION_RETURN_OBJECT(GetMenuDescriptor(name.GetIndex()));
}

void DListMenuDescriptor::Reset()
{
	// Reset the default settings (ignore all other values in the struct)
	mSelectOfsX = 0;
	mSelectOfsY = 0;
	mSelector.SetInvalid();
	mDisplayTop = 0;
	mXpos = 0;
	mYpos = 0;
	mLinespacing = 0;
	mNetgameMessage = "";
	mFont = NULL;
	mFontColor = CR_UNTRANSLATED;
	mFontColor2 = CR_UNTRANSLATED;
	mFromEngine = false;
}


size_t DListMenuDescriptor::PropagateMark()
{
	for (auto item : mItems) GC::Mark(item);
	return 0;
}

void DOptionMenuDescriptor::Reset()
{
	// Reset the default settings (ignore all other values in the struct)
	mPosition = 0;
	mScrollTop = 0;
	mIndent = 0;
	mDontDim = 0;
	mFont = BigUpper;
}

size_t DOptionMenuDescriptor::PropagateMark()
{
	for (auto item : mItems) GC::Mark(item);
	return 0;
}

void M_MarkMenus()
{
	MenuDescriptorList::Iterator it(MenuDescriptors);
	MenuDescriptorList::Pair *pair;
	while (it.NextPair(pair))
	{
		GC::Mark(pair->Value);
	}
	GC::Mark(CurrentMenu);
}
//============================================================================
//
// DMenu base class
//
//============================================================================

IMPLEMENT_CLASS(DMenu, false, true)

IMPLEMENT_POINTERS_START(DMenu)
	IMPLEMENT_POINTER(mParentMenu)
IMPLEMENT_POINTERS_END

DMenu::DMenu(DMenu *parent) 
{
	mParentMenu = parent;
	mMouseCapture = false;
	mBackbuttonSelected = false;
	DontDim = false;
	GC::WriteBarrier(this, parent);
}
	
//=============================================================================
//
//
//
//=============================================================================

bool DMenu::CallResponder(event_t *ev)
{
	if (ev->type == EV_GUI_Event)
	{
		IFVIRTUAL(DMenu, OnUIEvent)
		{
			FUiEvent e = ev;
			VMValue params[] = { (DObject*)this, &e };
			int retval;
			VMReturn ret(&retval);
			InMenu++;
			VMCall(func, params, 2, &ret, 1);
			InMenu--;
			return !!retval;
		}
	}
	else
	{
		IFVIRTUAL(DMenu, OnInputEvent)
		{
			FInputEvent e = ev;
			VMValue params[] = { (DObject*)this, &e };
			int retval;
			VMReturn ret(&retval);
			InMenu++;
			VMCall(func, params, 2, &ret, 1);
			InMenu--;
			return !!retval;
		}
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

bool DMenu::CallMenuEvent(int mkey, bool fromcontroller)
{
	IFVIRTUAL(DMenu, MenuEvent)
	{
		VMValue params[] = { (DObject*)this, mkey, fromcontroller };
		int retval;
		VMReturn ret(&retval);
		InMenu++;
		VMCall(func, params, 3, &ret, 1);
		InMenu--;
		return !!retval;
	}
	else return false;
}
//=============================================================================
//
//
//
//=============================================================================

static void SetMouseCapture(bool on)
{
	if (on) I_SetMouseCapture();
	else I_ReleaseMouseCapture();
}
DEFINE_ACTION_FUNCTION_NATIVE(DMenu, SetMouseCapture, SetMouseCapture)
{
	PARAM_PROLOGUE;
	PARAM_BOOL(on);
	SetMouseCapture(on);
	return 0;
}

void DMenu::Close ()
{
	if (CurrentMenu == nullptr) return;	// double closing can happen in the save menu.
	assert(CurrentMenu == this);
	CurrentMenu = mParentMenu;
	Destroy();
	if (CurrentMenu != nullptr)
	{
		GC::WriteBarrier(CurrentMenu);
		IFVIRTUALPTR(CurrentMenu, DMenu, OnReturn)
		{
			VMValue params[] = { CurrentMenu };
			VMCall(func, params, 1, nullptr, 0);
		}

	}
	else
	{
		M_ClearMenus ();
	}
}


static void Close(DMenu *menu)
{
	menu->Close();
}

DEFINE_ACTION_FUNCTION_NATIVE(DMenu, Close, Close)
{
	PARAM_SELF_PROLOGUE(DMenu);
	self->Close();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

void DMenu::CallTicker()
{
	IFVIRTUAL(DMenu, Ticker)
	{
		VMValue params[] = { (DObject*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
}


void DMenu::CallDrawer()
{
	IFVIRTUAL(DMenu, Drawer)
	{
		VMValue params[] = { (DObject*)this };
		VMCall(func, params, 1, nullptr, 0);
		twod->ClearClipRect();	// make sure the scripts don't leave a valid clipping rect behind.
	}
}

bool DMenu::TranslateKeyboardEvents()
{
	IFVIRTUAL(DMenu, TranslateKeyboardEvents)
	{
		VMValue params[] = { (DObject*)this };
		int retval;
		VMReturn ret(&retval);
		VMCall(func, params, countof(params), &ret, 1);
		return !!retval;
	}
	return true;
}

//=============================================================================
//
//
//
//=============================================================================

void M_DoStartControlPanel (bool scaleoverride)
{
	// intro might call this repeatedly
	if (CurrentMenu != nullptr)
		return;

	buttonMap.ResetButtonStates ();
	for (int i = 0; i < NUM_MKEYS; ++i)
	{
		MenuButtons[i].ReleaseKey(0);
	}

	C_HideConsole ();				// [RH] Make sure console goes bye bye.
	menuactive = MENU_On;
	BackbuttonTime = 0;
	BackbuttonAlpha = 0;
	if (scaleoverride && !CurrentScaleOverrider) CurrentScaleOverrider = new ScaleOverrider(twod);
	else if (!scaleoverride && CurrentScaleOverrider)
	{
		delete CurrentScaleOverrider;
		CurrentScaleOverrider = nullptr;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void M_ActivateMenu(DMenu *menu)
{
	if (menuactive == MENU_Off) menuactive = MENU_On;
	if (CurrentMenu != nullptr && CurrentMenu->mMouseCapture)
	{
		CurrentMenu->mMouseCapture = false;
		I_ReleaseMouseCapture();
	}
	CurrentMenu = menu;
	GC::WriteBarrier(CurrentMenu);
}

DEFINE_ACTION_FUNCTION(DMenu, ActivateMenu)
{
	PARAM_SELF_PROLOGUE(DMenu);
	M_ActivateMenu(self);
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

bool M_SetSpecialMenu(FName menu, int param);	// game specific checks

void M_SetMenu(FName menu, int param)
{
	if (!M_SetSpecialMenu(menu, param)) return;

	DMenuDescriptor **desc = MenuDescriptors.CheckKey(menu);
	if (desc != nullptr)
	{

		if ((*desc)->IsKindOf(RUNTIME_CLASS(DListMenuDescriptor)))
		{
			DListMenuDescriptor *ld = static_cast<DListMenuDescriptor*>(*desc);
			if (ld->mAutoselect >= 0 && ld->mAutoselect < (int)ld->mItems.Size())
			{
				// recursively activate the autoselected item without ever creating this menu.
				ld->mItems[ld->mAutoselect]->Activate();
			}
			else
			{
				PClass *cls = ld->mClass;
				if (cls == nullptr) cls = DefaultListMenuClass;
				if (cls == nullptr) cls = PClass::FindClass("ListMenu");

				DMenu *newmenu = (DMenu *)cls->CreateNew();
				IFVIRTUALPTRNAME(newmenu, "ListMenu", Init)
				{
					VMValue params[3] = { newmenu, CurrentMenu, ld };
					VMCall(func, params, 3, nullptr, 0);
				}
				M_ActivateMenu(newmenu);
			}
		}
		else if ((*desc)->IsKindOf(RUNTIME_CLASS(DOptionMenuDescriptor)))
		{
			DOptionMenuDescriptor *ld = static_cast<DOptionMenuDescriptor*>(*desc);
			PClass *cls = ld->mClass;
			if (cls == nullptr) cls = DefaultOptionMenuClass;
			if (cls == nullptr) cls = PClass::FindClass("OptionMenu");

			DMenu *newmenu = (DMenu*)cls->CreateNew();
			IFVIRTUALPTRNAME(newmenu, "OptionMenu", Init)
			{
				VMValue params[3] = { newmenu, CurrentMenu, ld };
				VMCall(func, params, 3, nullptr, 0);
			}
			M_ActivateMenu(newmenu);
		}
		return;
	}
	else
	{
		PClass *menuclass = PClass::FindClass(menu);
		if (menuclass != nullptr)
		{
			if (menuclass->IsDescendantOf("GenericMenu"))
			{
				DMenu *newmenu = (DMenu*)menuclass->CreateNew();

				IFVIRTUALPTRNAME(newmenu, "GenericMenu", Init)
				{
					VMValue params[3] = { newmenu, CurrentMenu };
					VMCall(func, params, 2, nullptr, 0);
				}
				M_ActivateMenu(newmenu);
				return;
			}
		}
	}
	Printf("Attempting to open menu of unknown type '%s'\n", menu.GetChars());
	M_ClearMenus();
}

DEFINE_ACTION_FUNCTION(DMenu, SetMenu)
{
	PARAM_PROLOGUE;
	PARAM_NAME(menu);
	PARAM_INT(mparam);
	M_SetMenu(menu, mparam);
	return 0;
}
//=============================================================================
//
//
//
//=============================================================================

bool M_Responder (event_t *ev) 
{ 
	int ch = 0;
	bool keyup = false;
	int mkey = NUM_MKEYS;
	bool fromcontroller = true;

	if (chatmodeon)
	{
		return false;
	}

	if (CurrentMenu != nullptr && menuactive != MENU_Off) 
	{
		// There are a few input sources we are interested in:
		//
		// EV_KeyDown / EV_KeyUp : joysticks/gamepads/controllers
		// EV_GUI_KeyDown / EV_GUI_KeyUp : the keyboard
		// EV_GUI_Char : printable characters, which we want in string input mode
		//
		// This code previously listened for EV_GUI_KeyRepeat to handle repeating
		// in the menus, but that doesn't work with gamepads, so now we combine
		// the multiple inputs into buttons and handle the repetition manually.
		if (ev->type == EV_GUI_Event)
		{
			fromcontroller = false;
			if (ev->subtype == EV_GUI_KeyRepeat)
			{
				// We do our own key repeat handling but still want to eat the
				// OS's repeated keys.
				return true;
			}
			else if (ev->subtype == EV_GUI_BackButtonDown || ev->subtype == EV_GUI_BackButtonUp)
			{
				mkey = MKEY_Back;
				keyup = ev->subtype == EV_GUI_BackButtonUp;
			}
			else if (ev->subtype != EV_GUI_KeyDown && ev->subtype != EV_GUI_KeyUp)
			{
				// do we want mouse input?
				if (ev->subtype >= EV_GUI_FirstMouseEvent && ev->subtype <= EV_GUI_LastMouseEvent)
				{
						if (!m_use_mouse)
							return true;
				}

				// pass everything else on to the current menu
				return CurrentMenu->CallResponder(ev);
			}
			else if (CurrentMenu->TranslateKeyboardEvents())
			{
				ch = ev->data1;
				keyup = ev->subtype == EV_GUI_KeyUp;
				switch (ch)
				{
				case GK_BACK:			mkey = MKEY_Back;		break;
				case GK_ESCAPE:			mkey = MKEY_Back;		break;
				case GK_RETURN:			mkey = MKEY_Enter;		break;
				case GK_UP:				mkey = MKEY_Up;			break;
				case GK_DOWN:			mkey = MKEY_Down;		break;
				case GK_LEFT:			mkey = MKEY_Left;		break;
				case GK_RIGHT:			mkey = MKEY_Right;		break;
				case GK_BACKSPACE:		mkey = MKEY_Clear;		break;
				case GK_PGUP:			mkey = MKEY_PageUp;		break;
				case GK_PGDN:			mkey = MKEY_PageDown;	break;
				default:
					if (!keyup)
					{
						return CurrentMenu->CallResponder(ev);
					}
					break;
				}
			}
		}
		else if (menuactive != MENU_WaitKey && (ev->type == EV_KeyDown || ev->type == EV_KeyUp))
		{
			// eat blocked controller events without dispatching them.
			if (ev->data1 >= KEY_FIRSTJOYBUTTON && m_blockcontrollers) return true;

			keyup = ev->type == EV_KeyUp;

			ch = ev->data1;
			switch (ch)
			{
			case KEY_JOY1:
			case KEY_PAD_A:
				mkey = MKEY_Enter;
				break;

			case KEY_JOY2:
			case KEY_PAD_B:
				mkey = MKEY_Back;
				break;

			case KEY_JOY3:
			case KEY_PAD_X:
				mkey = MKEY_Clear;
				break;

			case KEY_JOY5:
			case KEY_PAD_LSHOULDER:
				mkey = MKEY_PageUp;
				break;

			case KEY_JOY6:
			case KEY_PAD_RSHOULDER:
				mkey = MKEY_PageDown;
				break;

			case KEY_PAD_DPAD_UP:
			case KEY_PAD_LTHUMB_UP:
			case KEY_JOYAXIS1MINUS:
			case KEY_JOYPOV1_UP:
				mkey = MKEY_Up;
				break;

			case KEY_PAD_DPAD_DOWN:
			case KEY_PAD_LTHUMB_DOWN:
			case KEY_JOYAXIS1PLUS:
			case KEY_JOYPOV1_DOWN:
				mkey = MKEY_Down;
				break;

			case KEY_PAD_DPAD_LEFT:
			case KEY_PAD_LTHUMB_LEFT:
			case KEY_JOYAXIS2MINUS:
			case KEY_JOYPOV1_LEFT:
				mkey = MKEY_Left;
				break;

			case KEY_PAD_DPAD_RIGHT:
			case KEY_PAD_LTHUMB_RIGHT:
			case KEY_JOYAXIS2PLUS:
			case KEY_JOYPOV1_RIGHT:
				mkey = MKEY_Right;
				break;
			}
		}

		if (mkey != NUM_MKEYS)
		{
			if (keyup)
			{
				MenuButtons[mkey].ReleaseKey(ch);
				return false;
			}
			else
			{
				MenuButtons[mkey].PressKey(ch);
				MenuButtonOrigin[mkey] = fromcontroller;
				if (mkey <= MKEY_PageDown)
				{
					MenuButtonTickers[mkey] = KEY_REPEAT_DELAY;
				}
				CurrentMenu->CallMenuEvent(mkey, fromcontroller);
				return true;
			}
		}
		return CurrentMenu->CallResponder(ev) || !keyup;
	}
	else if (MenuEnabled)
	{
		if (ev->type == EV_KeyDown)
		{
			// Pop-up menu?
			if (ev->data1 == KEY_ESCAPE)
			{
				M_StartControlPanel(true);
				M_SetMenu(NAME_Mainmenu, -1);
				return true;
			}
			return false;
		}
		else if (ev->type == EV_GUI_Event && ev->subtype == EV_GUI_LButtonDown && 
				 ConsoleState != c_down && gamestate != GS_LEVEL && m_use_mouse)
		{
			M_StartControlPanel(true);
			M_SetMenu(NAME_Mainmenu, -1);
			return true;
		}
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

void M_Ticker (void) 
{
	MenuTime++;
	if (CurrentMenu != nullptr && menuactive != MENU_Off) 
	{
		CurrentMenu->CallTicker();
	}

	// Check again because menu could be closed from Ticker()
	if (CurrentMenu != nullptr && menuactive != MENU_Off)
	{
		for (int i = 0; i < NUM_MKEYS; ++i)
		{
			if (MenuButtons[i].bDown)
			{
				if (MenuButtonTickers[i] > 0 &&	--MenuButtonTickers[i] <= 0)
				{
					MenuButtonTickers[i] = KEY_REPEAT_RATE;
					CurrentMenu->CallMenuEvent(i, MenuButtonOrigin[i]);
				}
			}
		}
		if (BackbuttonTime > 0)
		{
			if (BackbuttonAlpha < 1.f) BackbuttonAlpha += .1f;
			if (BackbuttonAlpha > 1.f) BackbuttonAlpha = 1.f;
			BackbuttonTime--;
		}
		else
		{
			if (BackbuttonAlpha > 0) BackbuttonAlpha -= .1f;
			if (BackbuttonAlpha < 0) BackbuttonAlpha = 0;
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void M_Drawer (void) 
{
	PalEntry fade = 0;

	if (CurrentMenu != nullptr && menuactive != MENU_Off) 
	{
		if (!CurrentMenu->DontBlur) screen->BlurScene(0);
		if (!CurrentMenu->DontDim)
		{
			if (sysCallbacks && sysCallbacks->MenuDim) sysCallbacks->MenuDim();
		}
		CurrentMenu->CallDrawer();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void M_ClearMenus()
{
	while (CurrentMenu != nullptr)
	{
		DMenu* parent = CurrentMenu->mParentMenu;
		CurrentMenu->Destroy();
		CurrentMenu = parent;
	}
	menuactive = MENU_Off;
	if (CurrentScaleOverrider)  delete CurrentScaleOverrider;
	CurrentScaleOverrider = nullptr;
}

//=============================================================================
//
//
//
//=============================================================================

void M_PreviousMenu()
{
	if (CurrentMenu != nullptr)
	{
		DMenu* parent = CurrentMenu->mParentMenu;
		CurrentMenu->Destroy();
		CurrentMenu = parent;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void M_Init (void) 
{
	try
	{
		M_ParseMenuDefs();
		GC::AddMarkerFunc(M_MarkMenus);
	}
	catch (CVMAbortException &err)
	{
		err.MaybePrintMessage();
		Printf("%s", err.stacktrace.GetChars());
		I_FatalError("Failed to initialize menus");
	}
	M_CreateMenus();
}


//=============================================================================
//
//
//
//=============================================================================

void M_EnableMenu (bool on) 
{
	MenuEnabled = on;
}


//=============================================================================
//
// [RH] Most menus can now be accessed directly
// through console commands.
//
//=============================================================================

CCMD (menu_main)
{
	M_StartControlPanel(true);
	M_SetMenu(NAME_Mainmenu, -1);
}

CCMD (menu_load)
{	// F3
	M_StartControlPanel (true);
	M_SetMenu(NAME_Loadgamemenu, -1);
}

CCMD (menu_save)
{	// F2
	M_StartControlPanel (true);
	M_SetMenu(NAME_Savegamemenu, -1);
}

CCMD (menu_help)
{	// F1
	M_StartControlPanel (true);
	M_SetMenu(NAME_Readthismenu, -1);
}

CCMD (menu_game)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_Playerclassmenu, -1);	// The playerclass menu is the first in the 'start game' chain
}
								
CCMD (menu_options)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_Optionsmenu, -1);
}

CCMD (menu_player)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_Playermenu, -1);
}

CCMD (menu_messages)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_MessageOptions, -1);
}

CCMD (menu_automap)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_AutomapOptions, -1);
}

CCMD (menu_scoreboard)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_ScoreboardOptions, -1);
}

CCMD (menu_mapcolors)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_MapColorMenu, -1);
}

CCMD (menu_keys)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_CustomizeControls, -1);
}

CCMD (menu_gameplay)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_GameplayOptions, -1);
}

CCMD (menu_compatibility)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_CompatibilityOptions, -1);
}

CCMD (menu_mouse)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_MouseOptions, -1);
}

CCMD (menu_joystick)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_JoystickOptions, -1);
}

CCMD (menu_sound)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_SoundOptions, -1);
}

CCMD (menu_advsound)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_AdvSoundOptions, -1);
}

CCMD (menu_modreplayer)
{
	M_StartControlPanel(true);
	M_SetMenu(NAME_ModReplayerOptions, -1);
}

CCMD (menu_display)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_VideoOptions, -1);
}

CCMD (menu_video)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_VideoModeMenu, -1);
}



CCMD (openmenu)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: openmenu \"menu_name\"\n");
		return;
	}
	M_StartControlPanel (true);
	M_SetMenu(argv[1], -1);
}

CCMD (closemenu)
{
	M_ClearMenus();
}

CCMD (prevmenu)
{
	M_PreviousMenu();
}

CCMD(menuconsole)
{
	M_ClearMenus();
	C_ToggleConsole();
}

// This really should be in the script but we can't do scripted CCMDs yet.
CCMD(undocolorpic)
{
	if (CurrentMenu != NULL)
	{
		IFVIRTUALPTR(CurrentMenu, DMenu, ResetColor)
		{
			VMValue params[] = { (DObject*)CurrentMenu };
			VMCall(func, params, countof(params), nullptr, 0);
		}
	}
}


DEFINE_GLOBAL(menuactive)
DEFINE_GLOBAL(BackbuttonTime)
DEFINE_GLOBAL(BackbuttonAlpha)

DEFINE_FIELD(DMenu, mParentMenu)
DEFINE_FIELD(DMenu, mMouseCapture);
DEFINE_FIELD(DMenu, mBackbuttonSelected);
DEFINE_FIELD(DMenu, DontDim);
DEFINE_FIELD(DMenu, DontBlur);

DEFINE_FIELD(DMenuDescriptor, mMenuName)
DEFINE_FIELD(DMenuDescriptor, mNetgameMessage)
DEFINE_FIELD(DMenuDescriptor, mClass)

DEFINE_FIELD(DMenuItemBase, mXpos)
DEFINE_FIELD(DMenuItemBase, mYpos)
DEFINE_FIELD(DMenuItemBase, mAction)
DEFINE_FIELD(DMenuItemBase, mEnabled)

DEFINE_FIELD(DListMenuDescriptor, mItems)
DEFINE_FIELD(DListMenuDescriptor, mSelectedItem)
DEFINE_FIELD(DListMenuDescriptor, mSelectOfsX)
DEFINE_FIELD(DListMenuDescriptor, mSelectOfsY)
DEFINE_FIELD(DListMenuDescriptor, mSelector)
DEFINE_FIELD(DListMenuDescriptor, mDisplayTop)
DEFINE_FIELD(DListMenuDescriptor, mXpos)
DEFINE_FIELD(DListMenuDescriptor, mYpos)
DEFINE_FIELD(DListMenuDescriptor, mWLeft)
DEFINE_FIELD(DListMenuDescriptor, mWRight)
DEFINE_FIELD(DListMenuDescriptor, mLinespacing)
DEFINE_FIELD(DListMenuDescriptor, mAutoselect)
DEFINE_FIELD(DListMenuDescriptor, mFont)
DEFINE_FIELD(DListMenuDescriptor, mFontColor)
DEFINE_FIELD(DListMenuDescriptor, mFontColor2)
DEFINE_FIELD(DListMenuDescriptor, mCenter)

DEFINE_FIELD(DOptionMenuDescriptor, mItems)
DEFINE_FIELD(DOptionMenuDescriptor, mTitle)
DEFINE_FIELD(DOptionMenuDescriptor, mSelectedItem)
DEFINE_FIELD(DOptionMenuDescriptor, mDrawTop)
DEFINE_FIELD(DOptionMenuDescriptor, mScrollTop)
DEFINE_FIELD(DOptionMenuDescriptor, mScrollPos)
DEFINE_FIELD(DOptionMenuDescriptor, mIndent)
DEFINE_FIELD(DOptionMenuDescriptor, mPosition)
DEFINE_FIELD(DOptionMenuDescriptor, mDontDim)
DEFINE_FIELD(DOptionMenuDescriptor, mFont)

DEFINE_FIELD(FOptionMenuSettings, mTitleColor)
DEFINE_FIELD(FOptionMenuSettings, mFontColor)
DEFINE_FIELD(FOptionMenuSettings, mFontColorValue)
DEFINE_FIELD(FOptionMenuSettings, mFontColorMore)
DEFINE_FIELD(FOptionMenuSettings, mFontColorHeader)
DEFINE_FIELD(FOptionMenuSettings, mFontColorHighlight)
DEFINE_FIELD(FOptionMenuSettings, mFontColorSelection)
DEFINE_FIELD(FOptionMenuSettings, mLinespacing)


struct IJoystickConfig;
// These functions are used by dynamic menu creation.
DMenuItemBase * CreateOptionMenuItemStaticText(const char *name, int v)
{
	auto c = PClass::FindClass("OptionMenuItemStaticText");
	auto p = c->CreateNew();
	FString namestr = name;
	VMValue params[] = { p, &namestr, v };
	auto f = dyn_cast<PFunction>(c->FindSymbol("Init", false));
	VMCall(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateOptionMenuItemJoyConfigMenu(const char *label, IJoystickConfig *joy)
{
	auto c = PClass::FindClass("OptionMenuItemJoyConfigMenu");
	auto p = c->CreateNew();
	FString namestr = label;
	VMValue params[] = { p, &namestr, joy };
	auto f = dyn_cast<PFunction>(c->FindSymbol("Init", false));
	VMCall(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateOptionMenuItemSubmenu(const char *label, FName cmd, int center)
{
	auto c = PClass::FindClass("OptionMenuItemSubmenu");
	auto p = c->CreateNew();
	FString namestr = label;
	VMValue params[] = { p, &namestr, cmd.GetIndex(), center, false };
	auto f = dyn_cast<PFunction>(c->FindSymbol("Init", false));
	VMCall(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateOptionMenuItemControl(const char *label, FName cmd, FKeyBindings *bindings)
{
	auto c = PClass::FindClass("OptionMenuItemControlBase");
	auto p = c->CreateNew();
	FString namestr = label;
	VMValue params[] = { p, &namestr, cmd.GetIndex(), bindings };
	auto f = dyn_cast<PFunction>(c->FindSymbol("Init", false));
	VMCall(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateOptionMenuItemCommand(const char *label, FName cmd, bool centered)
{
	auto c = PClass::FindClass("OptionMenuItemCommand");
	auto p = c->CreateNew();
	FString namestr = label;
	VMValue params[] = { p, &namestr, cmd.GetIndex(), centered, false };
	auto f = dyn_cast<PFunction>(c->FindSymbol("Init", false));
	VMCall(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	auto unsafe = dyn_cast<PField>(c->FindSymbol("mUnsafe", false));
	unsafe->Type->SetValue(reinterpret_cast<uint8_t*>(p) + unsafe->Offset, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateListMenuItemPatch(double x, double y, int height, int hotkey, FTextureID tex, FName command, int param)
{
	auto c = PClass::FindClass("ListMenuItemPatchItem");
	auto p = c->CreateNew();
	FString keystr = FString(char(hotkey));
	VMValue params[] = { p, x, y, height, tex.GetIndex(), &keystr, command.GetIndex(), param };
	auto f = dyn_cast<PFunction>(c->FindSymbol("InitDirect", false));
	VMCall(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateListMenuItemText(double x, double y, int height, int hotkey, const char *text, FFont *font, PalEntry color1, PalEntry color2, FName command, int param)
{
	auto c = PClass::FindClass("ListMenuItemTextItem");
	auto p = c->CreateNew();
	FString keystr = FString(char(hotkey));
	FString textstr = text;
	VMValue params[] = { p, x, y, height, &keystr, &textstr, font, int(color1.d), int(color2.d), command.GetIndex(), param };
	auto f = dyn_cast<PFunction>(c->FindSymbol("InitDirect", false));
	VMCall(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

bool DMenuItemBase::Activate()
{
	IFVIRTUAL(DMenuItemBase, Activate)
	{
		VMValue params[] = { (DObject*)this };
		int retval;
		VMReturn ret(&retval);
		VMCall(func, params, countof(params), &ret, 1);
		return !!retval;
	}
	return false;
}

bool DMenuItemBase::SetString(int i, const char *s)
{
	IFVIRTUAL(DMenuItemBase, SetString)
	{
		FString namestr = s;
		VMValue params[] = { (DObject*)this, i, &namestr };
		int retval;
		VMReturn ret(&retval);
		VMCall(func, params, countof(params), &ret, 1);
		return !!retval;
	}
	return false;
}

bool DMenuItemBase::GetString(int i, char *s, int len)
{
	IFVIRTUAL(DMenuItemBase, GetString)
	{
		VMValue params[] = { (DObject*)this, i };
		int retval;
		FString retstr;
		VMReturn ret[2]; ret[0].IntAt(&retval); ret[1].StringAt(&retstr);
		VMCall(func, params, countof(params), ret, 2);
		strncpy(s, retstr, len);
		return !!retval;
	}
	return false;
}


bool DMenuItemBase::SetValue(int i, int value)
{
	IFVIRTUAL(DMenuItemBase, SetValue)
	{
		VMValue params[] = { (DObject*)this, i, value };
		int retval;
		VMReturn ret(&retval);
		VMCall(func, params, countof(params), &ret, 1);
		return !!retval;
	}
	return false;
}

bool DMenuItemBase::GetValue(int i, int *pvalue)
{
	IFVIRTUAL(DMenuItemBase, GetValue)
	{
		VMValue params[] = { (DObject*)this, i };
		int retval[2];
		VMReturn ret[2]; ret[0].IntAt(&retval[0]); ret[1].IntAt(&retval[1]);
		VMCall(func, params, countof(params), ret, 2);
		*pvalue = retval[1];
		return !!retval[0];
	}
	return false;
}

IMPLEMENT_CLASS(DMenuItemBase, false, false)
