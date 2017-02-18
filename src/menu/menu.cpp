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

#include "doomdef.h"
#include "doomstat.h"
#include "c_dispatch.h"
#include "d_gui.h"
#include "d_player.h"
#include "g_level.h"
#include "c_console.h"
#include "c_bind.h"
#include "s_sound.h"
#include "p_tick.h"
#include "g_game.h"
#include "c_cvars.h"
#include "d_event.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "gi.h"
#include "v_palette.h"
#include "i_input.h"
#include "gameconfigfile.h"
#include "gstrings.h"
#include "r_utility.h"
#include "menu/menu.h"
#include "textures/textures.h"
#include "virtual.h"

//
// Todo: Move these elsewhere
//
CVAR (Float, mouse_sensitivity, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, show_messages, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, show_obituaries, true, CVAR_ARCHIVE)


CVAR (Float, snd_menuvolume, 0.6f, CVAR_ARCHIVE)
CVAR(Int, m_use_mouse, 2, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Int, m_show_backbutton, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

DMenu *DMenu::CurrentMenu;

DEFINE_ACTION_FUNCTION(DMenu, GetCurrentMenu)
{
	ACTION_RETURN_OBJECT(DMenu::CurrentMenu);
}

int DMenu::MenuTime;

DEFINE_ACTION_FUNCTION(DMenu, MenuTime)
{
	ACTION_RETURN_INT(DMenu::MenuTime);
}

FGameStartup GameStartupInfo;
EMenuState		menuactive;
bool			M_DemoNoPlay;
FButtonStatus	MenuButtons[NUM_MKEYS];
int				MenuButtonTickers[NUM_MKEYS];
bool			MenuButtonOrigin[NUM_MKEYS];
int				BackbuttonTime;
float			BackbuttonAlpha;
static bool		MenuEnabled = true;

void M_InitVideoModes();
extern PClass *DefaultListMenuClass;
extern PClass *DefaultOptionMenuClass;


#define KEY_REPEAT_DELAY	(TICRATE*5/12)
#define KEY_REPEAT_RATE		(3)

//============================================================================
//
//
//
//============================================================================

IMPLEMENT_CLASS(DMenuDescriptor, false, false)
IMPLEMENT_CLASS(DListMenuDescriptor, false, false)
IMPLEMENT_CLASS(DOptionMenuDescriptor, false, false)

DEFINE_ACTION_FUNCTION(DMenuDescriptor, GetDescriptor)
{
	PARAM_PROLOGUE;
	PARAM_NAME(name);
	DMenuDescriptor **desc = MenuDescriptors.CheckKey(name);
	auto retn = desc ? *desc : nullptr;
	ACTION_RETURN_OBJECT(retn);
}

size_t DListMenuDescriptor::PropagateMark()
{
	for (auto item : mItems) GC::Mark(item);
	return 0;
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
	GC::Mark(DMenu::CurrentMenu);
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
	GC::WriteBarrier(this, parent);
}
	
bool DMenu::Responder (event_t *ev) 
{ 
	bool res = false;
	if (ev->type == EV_GUI_Event)
	{
		if (ev->subtype == EV_GUI_LButtonDown)
		{
			res = MouseEventBack(MOUSE_Click, ev->data1, ev->data2);
			// make the menu's mouse handler believe that the current coordinate is outside the valid range
			if (res) ev->data2 = -1;	
			res |= CallMouseEvent(MOUSE_Click, ev->data1, ev->data2);
			if (res)
			{
				SetCapture();
			}
			
		}
		else if (ev->subtype == EV_GUI_MouseMove)
		{
			BackbuttonTime = BACKBUTTON_TIME;
			if (mMouseCapture || m_use_mouse == 1)
			{
				res = MouseEventBack(MOUSE_Move, ev->data1, ev->data2);
				if (res) ev->data2 = -1;	
				res |= CallMouseEvent(MOUSE_Move, ev->data1, ev->data2);
			}
		}
		else if (ev->subtype == EV_GUI_LButtonUp)
		{
			if (mMouseCapture)
			{
				ReleaseCapture();
				res = MouseEventBack(MOUSE_Release, ev->data1, ev->data2);
				if (res) ev->data2 = -1;	
				res |= CallMouseEvent(MOUSE_Release, ev->data1, ev->data2);
			}
		}
	}
	return false; 
}

DEFINE_ACTION_FUNCTION(DMenu, Responder)
{
	PARAM_SELF_PROLOGUE(DMenu);
	PARAM_POINTER(ev, event_t);
	ACTION_RETURN_BOOL(self->Responder(ev));
}

bool DMenu::CallResponder(event_t *ev)
{
	IFVIRTUAL(DMenu, Responder)
	{
		VMValue params[] = { (DObject*)this, ev};
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, 2, &ret, 1, nullptr);
		return !!retval;
	}
	else return Responder(ev);
}

//=============================================================================
//
//
//
//=============================================================================

bool DMenu::MenuEvent (int mkey, bool fromcontroller)
{
	switch (mkey)
	{
	case MKEY_Back:
	{
		Close();
		S_Sound (CHAN_VOICE | CHAN_UI, 
			DMenu::CurrentMenu != nullptr? "menu/backup" : "menu/clear", snd_menuvolume, ATTN_NONE);
		return true;
	}
	}
	return false;
}

DEFINE_ACTION_FUNCTION(DMenu, MenuEvent)
{
	PARAM_SELF_PROLOGUE(DMenu);
	PARAM_INT(key);
	PARAM_BOOL(fromcontroller);
	ACTION_RETURN_BOOL(self->MenuEvent(key, fromcontroller));
}

bool DMenu::CallMenuEvent(int mkey, bool fromcontroller)
{
	IFVIRTUAL(DMenu, MenuEvent)
	{
		VMValue params[] = { (DObject*)this, mkey, fromcontroller };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, 3, &ret, 1, nullptr);
		return !!retval;
	}
	else return MenuEvent(mkey, fromcontroller);
}
//=============================================================================
//
//
//
//=============================================================================

void DMenu::Close ()
{
	if (DMenu::CurrentMenu == nullptr) return;	// double closing can happen in the save menu.
	assert(DMenu::CurrentMenu == this);
	DMenu::CurrentMenu = mParentMenu;
	Destroy();
	if (DMenu::CurrentMenu != nullptr)
	{
		GC::WriteBarrier(DMenu::CurrentMenu);
	}
	else
	{
		M_ClearMenus ();
	}
}

//=============================================================================
//
//
//
//=============================================================================

bool DMenu::MouseEvent(int type, int x, int y)
{
	return true;
}

DEFINE_ACTION_FUNCTION(DMenu, MouseEvent)
{
	PARAM_SELF_PROLOGUE(DMenu);
	PARAM_INT(type);
	PARAM_INT(x);
	PARAM_INT(y);
	ACTION_RETURN_BOOL(self->MouseEvent(type, x, y));
}

bool DMenu::CallMouseEvent(int type, int x, int y)
{
	IFVIRTUAL(DMenu, MouseEvent)
	{
		VMValue params[] = { (DObject*)this, type, x, y };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, 4, &ret, 1, nullptr);
		return !!retval;
	}
	else return MouseEvent (type, x, y);
}

//=============================================================================
//
//
//
//=============================================================================

bool DMenu::MouseEventBack(int type, int x, int y)
{
	if (m_show_backbutton >= 0)
	{
		FTexture *tex = TexMan(gameinfo.mBackButton);
		if (tex != nullptr)
		{
			if (m_show_backbutton&1) x -= screen->GetWidth() - tex->GetScaledWidth() * CleanXfac;
			if (m_show_backbutton&2) y -= screen->GetHeight() - tex->GetScaledHeight() * CleanYfac;
			mBackbuttonSelected = ( x >= 0 && x < tex->GetScaledWidth() * CleanXfac && 
									y >= 0 && y < tex->GetScaledHeight() * CleanYfac);
			if (mBackbuttonSelected && type == MOUSE_Release)
			{
				if (m_use_mouse == 2) mBackbuttonSelected = false;
				CallMenuEvent(MKEY_Back, true);
			}
			return mBackbuttonSelected;
		}
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

void DMenu::SetCapture()
{
	if (!mMouseCapture)
	{
		mMouseCapture = true;
		I_SetMouseCapture();
	}
}

void DMenu::ReleaseCapture()
{
	if (mMouseCapture)
	{
		mMouseCapture = false;
		I_ReleaseMouseCapture();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DMenu::Ticker () 
{
}

DEFINE_ACTION_FUNCTION(DMenu, Ticker)
{
	PARAM_SELF_PROLOGUE(DMenu);
	self->Ticker();
	return 0;
}

void DMenu::CallTicker()
{
	IFVIRTUAL(DMenu, Ticker)
	{
		VMValue params[] = { (DObject*)this };
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
	else Ticker();
}


void DMenu::Drawer () 
{
	if (this == DMenu::CurrentMenu && BackbuttonAlpha > 0 && m_show_backbutton >= 0 && m_use_mouse)
	{
		FTexture *tex = TexMan(gameinfo.mBackButton);
		int w = tex->GetScaledWidth() * CleanXfac;
		int h = tex->GetScaledHeight() * CleanYfac;
		int x = (!(m_show_backbutton&1))? 0:screen->GetWidth() - w;
		int y = (!(m_show_backbutton&2))? 0:screen->GetHeight() - h;
		if (mBackbuttonSelected && (mMouseCapture || m_use_mouse == 1))
		{
			screen->DrawTexture(tex, x, y, DTA_CleanNoMove, true, DTA_ColorOverlay, MAKEARGB(40, 255,255,255), TAG_DONE);
		}
		else
		{
			screen->DrawTexture(tex, x, y, DTA_CleanNoMove, true, DTA_Alpha, BackbuttonAlpha, TAG_DONE);
		}
	}
}


DEFINE_ACTION_FUNCTION(DMenu, Drawer)
{
	PARAM_SELF_PROLOGUE(DMenu);
	self->Drawer();
	return 0;
}

void DMenu::CallDrawer()
{
	IFVIRTUAL(DMenu, Drawer)
	{
		VMValue params[] = { (DObject*)this };
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
	else Drawer();
}

DEFINE_ACTION_FUNCTION(DMenu, Close)
{
	PARAM_SELF_PROLOGUE(DMenu);
	self->Close();
	return 0;
}

DEFINE_ACTION_FUNCTION(DMenu, GetItem)
{
	PARAM_SELF_PROLOGUE(DMenu);
	PARAM_NAME(name);
	ACTION_RETURN_OBJECT(self->GetItem(name));
}

DEFINE_ACTION_FUNCTION(DOptionMenuDescriptor, GetItem)
{
	PARAM_SELF_PROLOGUE(DOptionMenuDescriptor);
	PARAM_NAME(name);
	ACTION_RETURN_OBJECT(self->GetItem(name));
}


bool DMenu::DimAllowed()
{
	return true;
}

bool DMenu::TranslateKeyboardEvents()
{
	IFVIRTUAL(DMenu, TranslateKeyboardEvents)
	{
		VMValue params[] = { (DObject*)this };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return !!retval;
	}
	return true;
}

//=============================================================================
//
//
//
//=============================================================================

void M_StartControlPanel (bool makeSound)
{
	// intro might call this repeatedly
	if (DMenu::CurrentMenu != nullptr)
		return;

	ResetButtonStates ();
	for (int i = 0; i < NUM_MKEYS; ++i)
	{
		MenuButtons[i].ReleaseKey(0);
	}

	C_HideConsole ();				// [RH] Make sure console goes bye bye.
	menuactive = MENU_On;
	// Pause sound effects before we play the menu switch sound.
	// That way, it won't be paused.
	P_CheckTickerPaused ();

	if (makeSound)
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", snd_menuvolume, ATTN_NONE);
	}
	BackbuttonTime = 0;
	BackbuttonAlpha = 0;
}

//=============================================================================
//
//
//
//=============================================================================

void M_ActivateMenu(DMenu *menu)
{
	if (menuactive == MENU_Off) menuactive = MENU_On;
	if (DMenu::CurrentMenu != nullptr) DMenu::CurrentMenu->ReleaseCapture();
	DMenu::CurrentMenu = menu;
	GC::WriteBarrier(DMenu::CurrentMenu);
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

void M_SetMenu(FName menu, int param)
{
	// some menus need some special treatment
	switch (menu)
	{
	case NAME_Episodemenu:
		// sent from the player class menu
		GameStartupInfo.Skill = -1;
		GameStartupInfo.Episode = -1;
		GameStartupInfo.PlayerClass = 
			param == -1000? nullptr :
			param == -1? "Random" : GetPrintableDisplayName(PlayerClasses[param].Type).GetChars();
		break;

	case NAME_Skillmenu:
		// sent from the episode menu

		if ((gameinfo.flags & GI_SHAREWARE) && param > 0)
		{
			// Only Doom and Heretic have multi-episode shareware versions.
			M_StartMessage(GStrings("SWSTRING"), 1);
			return;
		}

		GameStartupInfo.Episode = param;
		M_StartupSkillMenu(&GameStartupInfo);	// needs player class name from class menu (later)
		break;

	case NAME_StartgameConfirm:
	{
		// sent from the skill menu for a skill that needs to be confirmed
		GameStartupInfo.Skill = param;

		const char *msg = AllSkills[param].MustConfirmText;
		if (*msg==0) msg = GStrings("NIGHTMARE");
		M_StartMessage (msg, 0, NAME_StartgameConfirmed);
		return;
	}

	case NAME_Startgame:
		// sent either from skill menu or confirmation screen. Skill gets only set if sent from skill menu
		// Now we can finally start the game. Ugh...
		GameStartupInfo.Skill = param;
	case NAME_StartgameConfirmed:

		G_DeferedInitNew (&GameStartupInfo);
		if (gamestate == GS_FULLCONSOLE)
		{
			gamestate = GS_HIDECONSOLE;
			gameaction = ga_newgame;
		}
		M_ClearMenus ();
		return;

	case NAME_Savegamemenu:
		if (!usergame || (players[consoleplayer].health <= 0 && !multiplayer) || gamestate != GS_LEVEL)
		{
			// cannot save outside the game.
			M_StartMessage (GStrings("SAVEDEAD"), 1);
			return;
		}

	case NAME_VideoModeMenu:
		M_InitVideoModes();
		break;

	}

	// End of special checks

	DMenuDescriptor **desc = MenuDescriptors.CheckKey(menu);
	if (desc != nullptr)
	{
		if ((*desc)->mNetgameMessage.IsNotEmpty() && netgame && !demoplayback)
		{
			M_StartMessage((*desc)->mNetgameMessage, 1);
			return;
		}

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

				DListMenu *newmenu = (DListMenu *)cls->CreateNew();
				IFVIRTUALPTRNAME(newmenu, "OptionMenu", Init)
				{
					VMValue params[3] = { newmenu, DMenu::CurrentMenu, ld };
					GlobalVMStack.Call(func, params, 3, nullptr, 0);
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
				VMValue params[3] = { newmenu, DMenu::CurrentMenu, ld };
				GlobalVMStack.Call(func, params, 3, nullptr, 0);
			}
			M_ActivateMenu(newmenu);
		}
		return;
	}
	else
	{
		const PClass *menuclass = PClass::FindClass(menu);
		if (menuclass != nullptr)
		{
			if (menuclass->IsDescendantOf(RUNTIME_CLASS(DMenu)))
			{
				DMenu *newmenu = (DMenu*)menuclass->CreateNew();
				newmenu->mParentMenu = DMenu::CurrentMenu;
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

	if (DMenu::CurrentMenu != nullptr && menuactive != MENU_Off) 
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
				return DMenu::CurrentMenu->CallResponder(ev);
			}
			else if (DMenu::CurrentMenu->TranslateKeyboardEvents())
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
						return DMenu::CurrentMenu->CallResponder(ev);
					}
					break;
				}
			}
		}
		else if (menuactive != MENU_WaitKey && (ev->type == EV_KeyDown || ev->type == EV_KeyUp))
		{
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
				DMenu::CurrentMenu->CallMenuEvent(mkey, fromcontroller);
				return true;
			}
		}
		return DMenu::CurrentMenu->CallResponder(ev) || !keyup;
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
			// If devparm is set, pressing F1 always takes a screenshot no matter
			// what it's bound to. (for those who don't bother to read the docs)
			if (devparm && ev->data1 == KEY_F1)
			{
				G_ScreenShot(nullptr);
				return true;
			}
			return false;
		}
		else if (ev->type == EV_GUI_Event && ev->subtype == EV_GUI_LButtonDown && 
				 ConsoleState != c_down && m_use_mouse)
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
	DMenu::MenuTime++;
	if (DMenu::CurrentMenu != nullptr && menuactive != MENU_Off) 
	{
		DMenu::CurrentMenu->CallTicker();

		for (int i = 0; i < NUM_MKEYS; ++i)
		{
			if (MenuButtons[i].bDown)
			{
				if (MenuButtonTickers[i] > 0 &&	--MenuButtonTickers[i] <= 0)
				{
					MenuButtonTickers[i] = KEY_REPEAT_RATE;
					DMenu::CurrentMenu->CallMenuEvent(i, MenuButtonOrigin[i]);
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
	player_t *player = &players[consoleplayer];
	AActor *camera = player->camera;
	PalEntry fade = 0;

	if (!screen->Accel2D && camera != nullptr && (gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL))
	{
		if (camera->player != nullptr)
		{
			player = camera->player;
		}
		fade = PalEntry (BYTE(player->BlendA*255), BYTE(player->BlendR*255), BYTE(player->BlendG*255), BYTE(player->BlendB*255));
	}


	if (DMenu::CurrentMenu != nullptr && menuactive != MENU_Off) 
	{
		if (DMenu::CurrentMenu->DimAllowed())
		{
			screen->Dim(fade);
			V_SetBorderNeedRefresh();
		}
		DMenu::CurrentMenu->CallDrawer();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void M_ClearMenus ()
{
	M_DemoNoPlay = false;
	if (DMenu::CurrentMenu != nullptr)
	{
		DMenu::CurrentMenu->Destroy();
		DMenu::CurrentMenu = nullptr;
	}
	V_SetBorderNeedRefresh();
	menuactive = MENU_Off;
}

//=============================================================================
//
//
//
//=============================================================================

void M_Init (void) 
{
	M_ParseMenuDefs();
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
		Printf("Usage: openmenu \"menu_name\"");
		return;
	}
	M_StartControlPanel (true);
	M_SetMenu(argv[1], -1);
}

CCMD (closemenu)
{
	M_ClearMenus();
}

//
//		Toggle messages on/off
//
CCMD (togglemessages)
{
	if (show_messages)
	{
		Printf (128, "%s\n", GStrings("MSGOFF"));
		show_messages = false;
	}
	else
	{
		Printf (128, "%s\n", GStrings("MSGON"));
		show_messages = true;
	}
}

EXTERN_CVAR (Int, screenblocks)

CCMD (sizedown)
{
	screenblocks = screenblocks - 1;
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
}

CCMD (sizeup)
{
	screenblocks = screenblocks + 1;
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", snd_menuvolume, ATTN_NONE);
}

CCMD(menuconsole)
{
	M_ClearMenus();
	C_ToggleConsole();
}

CCMD(reset2defaults)
{
	C_SetDefaultBindings ();
	C_SetCVarsToDefaults ();
	R_SetViewSize (screenblocks);
}

CCMD(reset2saved)
{
	GameConfig->DoGlobalSetup ();
	GameConfig->DoGameSetup (gameinfo.ConfigName);
	GameConfig->DoModSetup (gameinfo.ConfigName);
	R_SetViewSize (screenblocks);
}


// This really should be in the script but we can't do scripted CCMDs yet.
CCMD(undocolorpic)
{
	if (DMenu::CurrentMenu != NULL)
	{
		IFVIRTUALPTR(DMenu::CurrentMenu, DMenu, ResetColor)
		{
			VMValue params[] = { (DObject*)DMenu::CurrentMenu };
			GlobalVMStack.Call(func, params, countof(params), nullptr, 0, nullptr);
		}
	}
}




DEFINE_FIELD(DMenu, mParentMenu)

DEFINE_FIELD(DMenuDescriptor, mMenuName)
DEFINE_FIELD(DMenuDescriptor, mNetgameMessage)
DEFINE_FIELD(DMenuDescriptor, mClass)

DEFINE_FIELD(DMenuItemBase, mXpos)
DEFINE_FIELD(DMenuItemBase, mYpos)
DEFINE_FIELD(DMenuItemBase, mAction)
DEFINE_FIELD(DMenuItemBase, mEnabled)

DEFINE_FIELD(DListMenu, mDesc)
DEFINE_FIELD(DListMenu, mFocusControl)

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
DMenuItemBase * CreateOptionMenuItemStaticText(const char *name, bool v)
{
	auto c = PClass::FindClass("OptionMenuItemStaticText");
	auto p = c->CreateNew();
	VMValue params[] = { p, FString(name), v };
	auto f = dyn_cast<PFunction>(c->Symbols.FindSymbol("Init", false));
	GlobalVMStack.Call(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateOptionMenuItemJoyConfigMenu(const char *label, IJoystickConfig *joy)
{
	auto c = PClass::FindClass("OptionMenuItemJoyConfigMenu");
	auto p = c->CreateNew();
	VMValue params[] = { p, FString(label), joy };
	auto f = dyn_cast<PFunction>(c->Symbols.FindSymbol("Init", false));
	GlobalVMStack.Call(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateOptionMenuItemSubmenu(const char *label, FName cmd, int center)
{
	auto c = PClass::FindClass("OptionMenuItemSubmenu");
	auto p = c->CreateNew();
	VMValue params[] = { p, FString(label), cmd.GetIndex(), center };
	auto f = dyn_cast<PFunction>(c->Symbols.FindSymbol("Init", false));
	GlobalVMStack.Call(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateOptionMenuItemControl(const char *label, FName cmd, FKeyBindings *bindings)
{
	auto c = PClass::FindClass("OptionMenuItemControlBase");
	auto p = c->CreateNew();
	VMValue params[] = { p, FString(label), cmd.GetIndex(), bindings };
	auto f = dyn_cast<PFunction>(c->Symbols.FindSymbol("Init", false));
	GlobalVMStack.Call(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateListMenuItemPatch(int x, int y, int height, int hotkey, FTextureID tex, FName command, int param)
{
	auto c = PClass::FindClass("ListMenuItemPatchItem");
	auto p = c->CreateNew();
	VMValue params[] = { p, x, y, height, tex.GetIndex(), FString(char(hotkey)), command.GetIndex(), param };
	auto f = dyn_cast<PFunction>(c->Symbols.FindSymbol("InitDirect", false));
	GlobalVMStack.Call(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

DMenuItemBase * CreateListMenuItemText(int x, int y, int height, int hotkey, const char *text, FFont *font, PalEntry color1, PalEntry color2, FName command, int param)
{
	auto c = PClass::FindClass("ListMenuItemTextItem");
	auto p = c->CreateNew();
	VMValue params[] = { p, x, y, height, FString(char(hotkey)), text, font, int(color1.d), int(color2.d), command.GetIndex(), param };
	auto f = dyn_cast<PFunction>(c->Symbols.FindSymbol("InitDirect", false));
	GlobalVMStack.Call(f->Variants[0].Implementation, params, countof(params), nullptr, 0);
	return (DMenuItemBase*)p;
}

bool DMenuItemBase::CheckCoordinate(int x, int y)
{
	IFVIRTUAL(DMenuItemBase, CheckCoordinate)
	{
		VMValue params[] = { (DObject*)this, x, y };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return !!retval;
	}
	return false;
}

void DMenuItemBase::Ticker()
{
	IFVIRTUAL(DMenuItemBase, Ticker)
	{
		VMValue params[] = { (DObject*)this };
		GlobalVMStack.Call(func, params, countof(params), nullptr, 0, nullptr);
	}
}

void DMenuItemBase::Drawer(bool selected)
{
	IFVIRTUAL(DMenuItemBase, Drawer)
	{
		VMValue params[] = { (DObject*)this, selected };
		GlobalVMStack.Call(func, params, countof(params), nullptr, 0, nullptr);
	}
}

bool DMenuItemBase::Selectable()
{
	IFVIRTUAL(DMenuItemBase, Selectable)
	{
		VMValue params[] = { (DObject*)this };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return !!retval;
	}
	return false;
}

bool DMenuItemBase::Activate()
{
	IFVIRTUAL(DMenuItemBase, Activate)
	{
		VMValue params[] = { (DObject*)this };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return !!retval;
	}
	return false;
}
FName DMenuItemBase::GetAction(int *pparam)
{
	IFVIRTUAL(DMenuItemBase, GetAction)
	{
		VMValue params[] = { (DObject*)this };
		int retval[2];
		VMReturn ret[2]; ret[0].IntAt(&retval[0]); ret[1].IntAt(&retval[1]);
		GlobalVMStack.Call(func, params, countof(params), ret, 2, nullptr);
		return ENamedName(retval[0]);
	}
	return NAME_None;
}

bool DMenuItemBase::SetString(int i, const char *s)
{
	IFVIRTUAL(DMenuItemBase, SetString)
	{
		VMValue params[] = { (DObject*)this, i, FString(s) };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
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
		GlobalVMStack.Call(func, params, countof(params), ret, 2, nullptr);
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
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
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
		GlobalVMStack.Call(func, params, countof(params), ret, 2, nullptr);
		*pvalue = retval[1];
		return !!retval[0];
	}
	return false;
}


void DMenuItemBase::Enable(bool on)
{
	IFVIRTUAL(DMenuItemBase, Enable)
	{
		VMValue params[] = { (DObject*)this, on };
		GlobalVMStack.Call(func, params, countof(params), nullptr, 0, nullptr);
	}
}

bool DMenuItemBase::MenuEvent(int mkey, bool fromcontroller)
{
	IFVIRTUAL(DMenuItemBase, MenuEvent)
	{
		VMValue params[] = { (DObject*)this, mkey, fromcontroller };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return !!retval;
	}
	return false;
}

bool DMenuItemBase::MouseEvent(int type, int x, int y)
{
	IFVIRTUAL(DMenuItemBase, MouseEvent)
	{
		VMValue params[] = { (DObject*)this, type, x, y };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return !!retval;
	}
	return false;
}

bool DMenuItemBase::CheckHotkey(int c)
{
	IFVIRTUAL(DMenuItemBase, CheckHotkey)
	{
		VMValue params[] = { (DObject*)this, c };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return !!retval;
	}
	return false;
}

int DMenuItemBase::GetWidth()
{
	IFVIRTUAL(DMenuItemBase, GetWidth)
	{
		VMValue params[] = { (DObject*)this };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return retval;
	}
	return false;
}

int DMenuItemBase::GetIndent()
{
	IFVIRTUAL(DMenuItemBase, GetIndent)
	{
		VMValue params[] = { (DObject*)this };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return retval;
	}
	return false;
}

int DMenuItemBase::Draw(DOptionMenuDescriptor *desc, int y, int indent, bool selected)
{
	IFVIRTUAL(DMenuItemBase, Draw)
	{
		VMValue params[] = { (DObject*)this, desc, y, indent, selected };
		int retval;
		VMReturn ret(&retval);
		GlobalVMStack.Call(func, params, countof(params), &ret, 1, nullptr);
		return retval;
	}
	return false;
}

void DMenuItemBase::DrawSelector(int xofs, int yofs, FTextureID tex)
{
	if (tex.isNull())
	{
		if ((DMenu::MenuTime % 8) < 6)
		{
			screen->DrawText(ConFont, OptionSettings.mFontColorSelection,
				(mXpos + xofs - 160) * CleanXfac + screen->GetWidth() / 2,
				(mYpos + yofs - 100) * CleanYfac + screen->GetHeight() / 2,
				"\xd",
				DTA_CellX, 8 * CleanXfac,
				DTA_CellY, 8 * CleanYfac,
				TAG_DONE);
		}
	}
	else
	{
		screen->DrawTexture(TexMan(tex), mXpos + xofs, mYpos + yofs, DTA_Clean, true, TAG_DONE);
	}
}

