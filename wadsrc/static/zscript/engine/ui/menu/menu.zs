/*
** menu.zs
** The menu engine core
**
**---------------------------------------------------------------------------
** Copyright 2010-2020 Christoph Oelckers
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


struct KeyBindings native version("2.4")
{
	native static String NameKeys(int k1, int k2);
	native static String NameAllKeys(array<int> list, bool colors = true);

	native int, int GetKeysForCommand(String cmd);
	native void GetAllKeysForCommand(out array<int> list, String cmd);
	native String GetBinding(int key);

	native void SetBind(int key, String cmd);
	native void UnbindACommand (String str);
}

struct OptionValues native version("2.4")
{
	native static int GetCount(Name group);
	native static String GetText(Name group, int index);
	native static double GetValue(Name group, int index);
	native static String GetTextValue(Name group, int index);
}

struct JoystickConfig native version("2.4")
{
	enum EJoyAxis
	{
		JOYAXIS_None = -1,
		JOYAXIS_Yaw,
		JOYAXIS_Pitch,
		JOYAXIS_Forward,
		JOYAXIS_Side,
		JOYAXIS_Up,
	//	JOYAXIS_Roll,		// Ha ha. No roll for you.
		NUM_JOYAXIS,
	};

	native float GetSensitivity();
	native void SetSensitivity(float scale);

	native float GetAxisScale(int axis);
	native void SetAxisScale(int axis, float scale);

	native float GetAxisDeadZone(int axis);
	native void SetAxisDeadZone(int axis, float zone);

	native int GetAxisMap(int axis);
	native void SetAxisMap(int axis, int gameaxis);

	native String GetName();
	native int GetNumAxes();
	native String GetAxisName(int axis);

	native bool GetEnabled();
	native void SetEnabled(bool enabled);

	native bool AllowsEnabledInBackground();
	native bool GetEnabledInBackground();
	native void SetEnabledInBackground(bool enabled);

}

class Menu : Object native ui version("2.4")
{
	enum EMenuKey
	{
		MKEY_Up,
		MKEY_Down,
		MKEY_Left,
		MKEY_Right,
		MKEY_PageUp,
		MKEY_PageDown,
		MKEY_Enter,
		MKEY_Back,
		MKEY_Clear,
		NUM_MKEYS,

		// These are not buttons but events sent from other menus 

		MKEY_Input,
		MKEY_Abort,
		MKEY_MBYes,
		MKEY_MBNo,
	}

	enum EMenuMouse
	{
		MOUSE_Click,
		MOUSE_Move,
		MOUSE_Release
	};

	enum EMenuState
	{
		Off,			// Menu is closed
		On,				// Menu is opened
		WaitKey,		// Menu is opened and waiting for a key in the controls menu
		OnNoPause,		// Menu is opened but does not pause the game
	};

	native Menu mParentMenu;
	native bool mMouseCapture;
	native bool mBackbuttonSelected;
	native bool DontDim;
	native bool DontBlur;
	native bool AnimatedTransition;
	native bool Animated;

	native static int MenuTime();
	native static Menu GetCurrentMenu();
	native static clearscope void SetMenu(Name mnu, int param = 0);	// This is not 100% safe but needs to be available - but always make sure to check that only the desired player opens it!
	native static void StartMessage(String msg, int mode = 0, Name command = 'none');
	native static void SetMouseCapture(bool on);
	native void Close();
	native void ActivateMenu();

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void Init(Menu parent)
	{
		mParentMenu = parent;
		mMouseCapture = false;
		mBackbuttonSelected = false;
		DontDim = false;
		DontBlur = false;
		AnimatedTransition = false;
		Animated = false;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual bool MenuEvent (int mkey, bool fromcontroller)
	{
		switch (mkey)
		{
		case MKEY_Back:
		{
			Close();
			let m = GetCurrentMenu();
			MenuSound(m != null ? "menu/backup" : "menu/clear");
			if (!m) menuDelegate.MenuDismissed();
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

	protected bool MouseEventBack(int type, int x, int y)
	{
		if (m_show_backbutton >= 0)
		{
			let tex = TexMan.CheckForTexture(gameinfo.mBackButton, TexMan.Type_MiscPatch);
			if (tex.IsValid())
			{
				Vector2 v = TexMan.GetScaledSize(tex);
				int w = int(v.X + 0.5) * CleanXfac;
				int h = int(v.Y + 0.5) * CleanYfac;
				if (m_show_backbutton&1) x -= screen.GetWidth() - w;
				if (m_show_backbutton&2) y -= screen.GetHeight() - h;
				mBackbuttonSelected = ( x >= 0 && x < w && y >= 0 && y < h);
				if (mBackbuttonSelected && type == MOUSE_Release)
				{
					if (m_use_mouse == 2) mBackbuttonSelected = false;
					MenuEvent(MKEY_Back, true);
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

	virtual bool OnUIEvent(UIEvent ev)
	{ 
		bool res = false;
		int y = ev.MouseY;
		if (ev.type == UIEvent.Type_LButtonDown)
		{
			res = MouseEventBack(MOUSE_Click, ev.MouseX, y);
			// make the menu's mouse handler believe that the current coordinate is outside the valid range
			if (res) y = -1;	
			res |= MouseEvent(MOUSE_Click, ev.MouseX, y);
			if (res)
			{
				SetCapture(true);
			}

		}
		else if (ev.type == UIEvent.Type_MouseMove)
		{
			BackbuttonTime = 4*GameTicRate;
			if (mMouseCapture || m_use_mouse == 1)
			{
				res = MouseEventBack(MOUSE_Move, ev.MouseX, y);
				if (res) y = -1;	
				res |= MouseEvent(MOUSE_Move, ev.MouseX, y);
			}
		}
		else if (ev.type == UIEvent.Type_LButtonUp)
		{
			if (mMouseCapture)
			{
				SetCapture(false);
				res = MouseEventBack(MOUSE_Release, ev.MouseX, y);
				if (res) y = -1;	
				res |= MouseEvent(MOUSE_Release, ev.MouseX, y);
			}
		}
		return false; 
	}

	virtual bool OnInputEvent(InputEvent ev)
	{ 
		return false;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual void Drawer () 
	{
		if (self == GetCurrentMenu() && BackbuttonAlpha > 0 && m_show_backbutton >= 0 && m_use_mouse)
		{
			let tex = TexMan.CheckForTexture(gameinfo.mBackButton, TexMan.Type_MiscPatch);
			if (tex.IsValid())
			{
				Vector2 v = TexMan.GetScaledSize(tex);
				int w = int(v.X + 0.5) * CleanXfac;
				int h = int(v.Y + 0.5) * CleanYfac;
				int x = (!(m_show_backbutton&1))? 0:screen.GetWidth() - w;
				int y = (!(m_show_backbutton&2))? 0:screen.GetHeight() - h;
				if (mBackbuttonSelected && (mMouseCapture || m_use_mouse == 1))
				{
					screen.DrawTexture(tex, true, x, y, DTA_CleanNoMove, true, DTA_ColorOverlay, Color(40, 255,255,255), DTA_NOOFFSET, true);
				}
				else
				{
					screen.DrawTexture(tex, true, x, y, DTA_CleanNoMove, true, DTA_Alpha, BackbuttonAlpha, DTA_NOOFFSET, true);
				}
			}
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void SetCapture(bool on)
	{
		if (mMouseCapture != on)
		{
			mMouseCapture = on;
			SetMouseCapture(on);
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual bool TranslateKeyboardEvents() { return true; }
	virtual void SetFocus(MenuItemBase fc) {}
	virtual bool CheckFocus(MenuItemBase fc) { return false;  }
	virtual void ReleaseFocus() {}
	virtual void ResetColor() {}
	virtual bool MouseEvent(int type, int mx, int my) { return true; }
	virtual void Ticker() {}
	virtual void OnReturn() {}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	static void MenuSound(Name snd)
	{
		menuDelegate.PlaySound(snd);
	}

	deprecated("4.0") static void DrawConText (int color, int x, int y, String str)
	{
		screen.DrawText (ConFont, color, x, y, str, DTA_CellX, 8 * CleanXfac, DTA_CellY, 8 * CleanYfac);
	}

	static Font OptionFont()
	{
		return NewSmallFont;
	}

	static int OptionHeight() 
	{
		return OptionFont().GetHeight();
	}

	static int OptionWidth(String s, bool localize = true)
	{
		return OptionFont().StringWidth(s, localize);
	}

	static void DrawOptionText(int x, int y, int color, String text, bool grayed = false, bool localize = true)
	{
		String label = localize ? Stringtable.Localize(text) : text;
		int overlay = grayed? Color(96,48,0,0) : 0;
		screen.DrawText (OptionFont(), color, x, y, text, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, DTA_Localize, localize);
	}


}

class MenuDescriptor : Object native ui version("2.4")
{
	native Name mMenuName;
	native String mNetgameMessage;
	native Class<Menu> mClass;

	native static MenuDescriptor GetDescriptor(Name n);
}

// This class is only needed to give it a virtual Init method that doesn't belong to Menu itself
class GenericMenu : Menu
{
	virtual void Init(Menu parent)
	{
		Super.Init(parent);
	}
}
