
struct KeyBindings native version("2.4")
{
	native static String NameKeys(int k1, int k2);

	native int, int GetKeysForCommand(String cmd);
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

	native static int MenuTime();
	native static Menu GetCurrentMenu();
	native static clearscope void SetMenu(Name mnu, int param = 0);	// This is not 100% safe but needs to be available - but always make sure to check that only the desired player opens it!
	native static void StartMessage(String msg, int mode = 0, Name command = 'none');
	native static void SetMouseCapture(bool on);
	native void Close();
	native void ActivateMenu();
	native static void UpdateColorsets(PlayerClass cls);
	native static void UpdateSkinOptions(PlayerClass cls);

	private native static void MakeScreenShot();
	
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
			Close();
			MenuSound (GetCurrentMenu() != null? "menu/backup" : "menu/clear");
			return true;
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
			BackbuttonTime = 4*Thinker.TICRATE;
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
		else if (ev.type == UIEvent.Type_KeyDown)
		{
			checkPrintScreen(ev);
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
					screen.DrawTexture(tex, true, x, y, DTA_CleanNoMove, true, DTA_ColorOverlay, Color(40, 255,255,255));
				}
				else
				{
					screen.DrawTexture(tex, true, x, y, DTA_CleanNoMove, true, DTA_Alpha, BackbuttonAlpha);
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

	static void MenuSound(Sound snd)
	{
		S_Sound (snd, CHAN_VOICE | CHAN_UI, snd_menuvolume, ATTN_NONE);
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
	
	static int OptionWidth(String s)
	{
		return OptionFont().StringWidth(s);
	}
	
	static void DrawOptionText(int x, int y, int color, String text, bool grayed = false)
	{
		String label = Stringtable.Localize(text);
		int overlay = grayed? Color(96,48,0,0) : 0;
		screen.DrawText (OptionFont(), color, x, y, text, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay);
	}
	
	private static int uiKeyToInputKey(UiEvent ev)
	{
		int uiKey = ev.KeyChar;

		switch (uiKey)
		{
		case UiEvent.Key_PgDn      : return 0; // used by menu
		case UiEvent.Key_PgUp      : return 0; // used by menu
		case UiEvent.Key_Home      : return InputEvent.Key_Home;
		case UiEvent.Key_End       : return InputEvent.Key_End;

		case UiEvent.Key_Left      : return 0; // used by menu
		case UiEvent.Key_Right     : return 0; // used by menu
		case UiEvent.Key_Alert     : return 0; // ASCII bell, not present in EDoomInputKeys
		case UiEvent.Key_Backspace : return 0; // used by menu
		case UiEvent.Key_Tab       : return InputEvent.Key_Tab;
		case UiEvent.Key_LineFeed  : return 0; // ASCII, not present in EDoomInputKeys
		case UiEvent.Key_Down      : return 0; // used by menu
		case UiEvent.Key_VTab      : return 0; // not present in EDoomInputKeys
		case UiEvent.Key_Up        : return 0; // used by menu
		case UiEvent.Key_FormFeed  : return 0; // ASCII, not present in EDoomInputKeys
		case UiEvent.Key_Return    : return 0; // used by menu

		case UiEvent.Key_F1        : return InputEvent.Key_F1;
		case UiEvent.Key_F2        : return InputEvent.Key_F2;
		case UiEvent.Key_F3        : return InputEvent.Key_F3;
		case UiEvent.Key_F4        : return InputEvent.Key_F4;
		case UiEvent.Key_F5        : return InputEvent.Key_F5;
		case UiEvent.Key_F6        : return InputEvent.Key_F6;
		case UiEvent.Key_F7        : return InputEvent.Key_F7;
		case UiEvent.Key_F8        : return InputEvent.Key_F8;
		case UiEvent.Key_F9        : return InputEvent.Key_F9;
		case UiEvent.Key_F10       : return InputEvent.Key_F10;
		case UiEvent.Key_F11       : return InputEvent.Key_F11;
		case UiEvent.Key_F12       : return InputEvent.Key_F12;

		case UiEvent.Key_Del       : return InputEvent.Key_Del;
		case UiEvent.Key_Escape    : return 0; // used by menu
		case UiEvent.Key_Free1     : return 0; // ?
		case UiEvent.Key_Free2     : return 0; // ?
		case UiEvent.Key_Back      : return 0; // browser back key, ?
		case UiEvent.Key_CEscape   : return 0; // color escape, not present in EDoomInputKeys
		}

		if (ev.IsShift) { return InputEvent.Key_LShift; }
		if (ev.IsCtrl)  { return InputEvent.Key_LCtrl; }
		if (ev.IsAlt)   { return InputEvent.Key_LAlt; }

		return 0;
	}

	private static bool uiKeyIsInputKey(UiEvent ev, int inputKey)
	{
		int convertedKey = uiKeyToInputKey(ev);
		if (convertedKey)
		{
			return (convertedKey == inputKey);
		}

		String inputKeyName = KeyBindings.NameKeys(inputKey, 0);
		if (inputKeyName.length() == 1)
		{
			int byte  = inputKeyName.ByteAt(0);
			int uiKey = ev.KeyChar;
			return (byte == uiKey);
		}

		return false;
	}

	private void checkPrintScreen(UiEvent ev)
	{
		if (self is "TextEnterMenu") { return; }

		int screenshotKey1, screenshotKey2;
		[screenshotKey1, screenshotKey2] = Bindings.GetKeysForCommand("screenshot");

		if (uiKeyIsInputKey(ev, screenshotKey1) || uiKeyIsInputKey(ev, screenshotKey2))
		{
			MakeScreenshot();
		}
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