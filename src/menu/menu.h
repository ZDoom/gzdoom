#ifndef __M_MENU_MENU_H__
#define __M_MENU_MENU_H__




#include "dobject.h"
#include "d_player.h"
#include "r_translate.h"
#include "textures/textures.h"

struct event_t;
class FTexture;
class FFont;
enum EColorRange;
class FPlayerClass;

enum EMenuKey
{
	MKEY_Up,
	MKEY_Down,
	MKEY_Left,
	MKEY_Right,
	MKEY_PageUp,
	MKEY_PageDown,
	//----------------- Keys past here do not repeat.
	MKEY_Enter,
	MKEY_Back,		// Back to previous menu
	MKEY_Clear,		// Clear keybinding/flip player sprite preview
	MKEY_Input,		// Sent when input is confirmed
	MKEY_Abort,		// Input aborted

	NUM_MKEYS
};


struct FGameStartup
{
	const char *PlayerClass;
	int Episode;
	int Skill;
};

extern FGameStartup GameStartupInfo;


//=============================================================================
//
// menu descriptor. This is created from the menu definition lump
// Items must be inserted in the order they are cycled through with the cursor
//
//=============================================================================

enum EMenuDescriptorType
{
	MDESC_ListMenu,
	MDESC_OptionsMenu,
};

struct FMenuDescriptor
{
	FName mMenuName;
	int mType;
};

class FListMenuItem;

struct FListMenuDescriptor : public FMenuDescriptor
{
	TDeletingArray<FListMenuItem *> mItems;
	int mSelectedItem;
	int mSelectOfsX;
	int mSelectOfsY;
	FTextureID mSelector;
	int mDisplayTop;
	int mXpos, mYpos;
	int mLinespacing;	// needs to be stored for dynamically created menus
	FString mNetgameMessage;
	int mAutoselect;	// this can only be set by internal menu creation functions
	FFont *mFont;
	EColorRange mFontColor;
	EColorRange mFontColor2;
	const PClass *mClass;
};

typedef TMap<FName, FMenuDescriptor *> MenuDescriptorList;

extern MenuDescriptorList MenuDescriptors;


//=============================================================================
//
//
//
//=============================================================================

struct FMenuRect
{
	int x, y;
	int width, height;

	void set(int _x, int _y, int _w, int _h)
	{
		x = _x;
		y = _y;
		width = _w;
		height = _h;
	}

	bool inside(int _x, int _y)
	{
		return _x >= x && _x < x+width && _y >= y && _y < y+height;
	}

};


class DMenu : public DObject
{
	DECLARE_CLASS (DMenu, DObject)
	HAS_OBJECT_POINTERS

protected:

public:
	static DMenu *CurrentMenu;
	static int MenuTime;

	TObjPtr<DMenu> mParentMenu;

	DMenu(DMenu *parent = NULL);
	virtual bool Responder (event_t *ev);
	virtual bool MenuEvent (int mkey, bool fromcontroller);
	virtual void Ticker ();
	virtual void Drawer ();
	virtual bool DimAllowed ();
	virtual bool TranslateKeyboardEvents();
};

//=============================================================================
//
// base class for menu items
//
//=============================================================================

class FListMenuItem
{
protected:
	int mXpos, mYpos;

public:
	bool mEnabled;

	FListMenuItem(int xpos = 0, int ypos = 0);
	virtual ~FListMenuItem();

	virtual bool CheckCoordinate(int x, int y);
	virtual void Ticker();
	virtual void Drawer();
	virtual bool Selectable();
	virtual bool Activate();
	virtual FName GetAction(int *pparam);
	virtual bool SetString(int i, const char *s);
	virtual bool GetString(int i, char *s, int len);
	virtual bool SetValue(int i, int value);
	virtual bool GetValue(int i, int *pvalue);
	virtual void Enable(bool on);
	virtual bool MenuEvent (int mkey, bool fromcontroller);
	void DrawSelector(int xofs, int yofs, FTextureID tex);
};	

class FListMenuItemStaticPatch : public FListMenuItem
{
protected:
	FTextureID mTexture;
	bool mCentered;

public:
	FListMenuItemStaticPatch(int x, int y, FTextureID patch, bool centered);
	void Drawer();
};

class FListMenuItemStaticAnimation : public FListMenuItemStaticPatch
{
	TArray<FTextureID> mFrames;
	int mFrameTime;
	int mFrameCount;
	unsigned int mFrame;

public:
	FListMenuItemStaticAnimation(int x, int y, int frametime);
	void AddTexture(FTextureID tex);
	void Ticker();
};

class FListMenuItemStaticText : public FListMenuItem
{
protected:
	const char *mText;
	FFont *mFont;
	EColorRange mColor;
	bool mCentered;

public:
	FListMenuItemStaticText(int x, int y, const char *text, FFont *font, EColorRange color, bool centered);
	void Drawer();
};

class FListMenuItemPlayerDisplay : public FListMenuItem
{
	FListMenuDescriptor *mOwner;
	FTexture *mBackdrop;
	FRemapTable mRemap;
	FPlayerClass *mPlayerClass;
	FState *mPlayerState;
	int mPlayerTics;
	bool mNoportrait;

	bool UpdatePlayerClass();

public:

	FListMenuItemPlayerDisplay(FListMenuDescriptor *menu, int x, int y, PalEntry c1, PalEntry c2, bool np);
	~FListMenuItemPlayerDisplay();
	virtual void Ticker();
	virtual void Drawer();
};


class FListMenuItemSelectable : public FListMenuItem
{
protected:
	FName mAction;
	FMenuRect mHotspot;
	int mHotkey;
	int mParam;

public:
	FListMenuItemSelectable(int x, int y, FName childmenu, int mParam = -1);
	void SetHotspot(int x, int y, int w, int h);
	bool CheckCoordinate(int x, int y);
	bool Selectable();
	bool CheckHotkey(int c) { return c == mHotkey; }
	bool Activate();
	FName GetAction(int *pparam);
};

class FListMenuItemText : public FListMenuItemSelectable
{
	const char *mText;
	FFont *mFont;
	EColorRange mColor;
public:
	FListMenuItemText(int x, int y, int hotkey, const char *text, FFont *font, EColorRange color, FName child, int param = 0);
	~FListMenuItemText();
	void Drawer();
};

class FListMenuItemPatch : public FListMenuItemSelectable
{
	FTextureID mTexture;
public:
	FListMenuItemPatch(int x, int y, int hotkey, FTextureID patch, FName child, int param = 0);
	void Drawer();
};

//=============================================================================
//
// items for the player menu
//
//=============================================================================

class FPlayerNameBox : public FListMenuItemSelectable
{
	const char *mText;
	FFont *mFont;
	EColorRange mFontColor;
	int mFrameSize;
	char mPlayerName[MAXPLAYERNAME+1];
	char mEditName[MAXPLAYERNAME+2];
	bool mEntering;

	void DrawBorder (int x, int y, int len);

public:

	FPlayerNameBox(int x, int y, int frameofs, const char *text, FFont *font, EColorRange color, FName action);
	~FPlayerNameBox();
	bool SetString(int i, const char *s);
	bool GetString(int i, char *s, int len);
	void Drawer();
	bool MenuEvent (int mkey, bool fromcontroller);
};

//=============================================================================
//
// items for the player menu
//
//=============================================================================

class FValueTextItem : public FListMenuItemSelectable
{
	TArray<FString> mSelections;
	const char *mText;
	int mSelection;
	FFont *mFont;
	EColorRange mFontColor;
	EColorRange mFontColor2;

public:

	FValueTextItem(int x, int y, const char *text, FFont *font, EColorRange color, EColorRange valuecolor, FName action);
	~FValueTextItem();
	bool SetString(int i, const char *s);
	bool SetValue(int i, int value);
	bool GetValue(int i, int *pvalue);
	bool MenuEvent (int mkey, bool fromcontroller);
	void Drawer();
};

//=============================================================================
//
// items for the player menu
//
//=============================================================================

class FSliderItem : public FListMenuItemSelectable
{
	const char *mText;
	FFont *mFont;
	EColorRange mFontColor;
	int mMinrange, mMaxrange;
	int mStep;
	int mSelection;

	void DrawSlider (int x, int y);

public:

	FSliderItem(int x, int y, const char *text, FFont *font, EColorRange color, FName action, int min, int max, int step);
	~FSliderItem();
	bool SetValue(int i, int value);
	bool GetValue(int i, int *pvalue);
	bool MenuEvent (int mkey, bool fromcontroller);
	void Drawer();
};

//=============================================================================
//
// list menu class runs a menu described by a FListMrnuDescriptor
//
//=============================================================================

class DListMenu : public DMenu
{
	DECLARE_CLASS(DListMenu, DMenu)

protected:
	FListMenuDescriptor *mDesc;

public:
	DListMenu(DMenu *parent = NULL, FListMenuDescriptor *desc = NULL);
	virtual void Init(DMenu *parent = NULL, FListMenuDescriptor *desc = NULL);
	FListMenuItem *GetItem(FName name);
	bool Responder (event_t *ev);
	bool MenuEvent (int mkey, bool fromcontroller);
	void Ticker ();
	void Drawer ();
};


//=============================================================================
//
// Input some text
//
//=============================================================================

class DTextEnterMenu : public DMenu
{
	DECLARE_ABSTRACT_CLASS(DTextEnterMenu, DMenu)

	char *mEnterString;
	unsigned int mEnterSize;
	unsigned int mEnterPos;
	int mSizeMode; // 1: size is length in chars. 2: also check string width
	bool mInputGridOkay;

	int InputGridX;
	int InputGridY;

public:

	DTextEnterMenu(DMenu *parent, char *textbuffer, int maxlen, int sizemode, bool showgrid);

	void Drawer ();
	bool MenuEvent (int mkey, bool fromcontroller);
	bool Responder(event_t *ev);
	bool TranslateKeyboardEvents();

};



void M_ActivateMenu(DMenu *menu);
void M_ClearMenus ();
void M_ParseMenuDefs();
void M_StartupSkillMenu(FGameStartup *gs);



#endif