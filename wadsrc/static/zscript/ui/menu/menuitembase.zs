//=============================================================================
//
// base class for menu items
//
//=============================================================================

class MenuItemBase : Object native ui version("2.4")
{
	protected native double mXpos, mYpos;
	protected native Name mAction;
	native int mEnabled;

	void Init(double xpos = 0, double ypos = 0, Name actionname = 'None')
	{
		mXpos = xpos;
		mYpos = ypos;
		mAction = actionname;
		mEnabled = true;
	}

	virtual bool CheckCoordinate(int x, int y) { return false; }
	virtual void Ticker() {}
	virtual void Drawer(bool selected) {}
	virtual bool Selectable() {return false; }
	virtual bool Activate() { return false; }
	virtual Name, int GetAction() { return mAction, 0; }
	virtual bool SetString(int i, String s) { return false; }
	virtual bool, String GetString(int i) { return false, ""; }
	virtual bool SetValue(int i, int value) { return false; }
	virtual bool, int GetValue(int i)  { return false, 0; }
	virtual void Enable(bool on) { mEnabled = on; }
	virtual bool MenuEvent (int mkey, bool fromcontroller) { return false; }
	virtual bool MouseEvent(int type, int x, int y) { return false; }
	virtual bool CheckHotkey(int c) { return false; }
	virtual int GetWidth() { return 0; }
	virtual int GetIndent() { return 0; }
	virtual int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected) { return indent; }

	void OffsetPositionY(double ydelta) { mYpos += ydelta; }
	double GetY() { return mYpos; }
	double GetX() { return mXpos; }
	void SetX(double x) { mXpos = x; }
	virtual void OnMenuCreated() {}
}

// this is only used to parse font color ranges in MENUDEF
enum MenudefColorRange
{
	NO_COLOR = -1
}