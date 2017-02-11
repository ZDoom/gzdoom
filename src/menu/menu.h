#ifndef __M_MENU_MENU_H__
#define __M_MENU_MENU_H__




#include "dobject.h"
#include "d_player.h"
#include "r_data/r_translate.h"
#include "c_cvars.h"
#include "v_font.h"
#include "version.h"
#include "textures/textures.h"

EXTERN_CVAR(Float, snd_menuvolume)
EXTERN_CVAR(Int, m_use_mouse);


struct event_t;
class FTexture;
class FFont;
enum EColorRange : int;
class FPlayerClass;
class FKeyBindings;
struct FBrokenLines;

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
	NUM_MKEYS,

	// These are not buttons but events sent from other menus 

	MKEY_Input,		// Sent when input is confirmed
	MKEY_Abort,		// Input aborted
	MKEY_MBYes,
	MKEY_MBNo,
};


struct FGameStartup
{
	const char *PlayerClass;
	int Episode;
	int Skill;
};

extern FGameStartup GameStartupInfo;

struct FSaveGameNode
{
	char Title[SAVESTRINGSIZE];
	FString Filename;
	bool bOldVersion;
	bool bMissingWads;
	bool bNoDelete;

	FSaveGameNode() { bNoDelete = false; }
};

struct SavegameManager
{
	TArray<FSaveGameNode*> SaveGames;
	int LastSaved = -1;
	int LastAccessed = -1;
	int WindowSize = 0;
	FSaveGameNode *quickSaveSlot = nullptr;

	FileReader *currentSavePic = nullptr;
	TArray<char> SavePicData;

	FTexture *SavePic = nullptr;
	FBrokenLines *SaveComment = nullptr;

	void ClearSaveGames();
	int InsertSaveNode(FSaveGameNode *node);
	int RemoveSaveSlot(int index);
	void ReadSaveStrings();
	void NotifyNewSave(const char *file, const char *title, bool okForQuicksave);
	void LoadSavegame(int Selected);
	void DoSave(int Selected, const char *savegamestring);
	void DeleteEntry(int Selected);
	void ExtractSaveData(int index);
	void UnloadSaveData();
	void ClearSaveStuff();
	bool DrawSavePic(int x, int y, int w, int h);
	void SetFileInfo(int Selected);

};

extern SavegameManager savegameManager;

//=============================================================================
//
// menu descriptor. This is created from the menu definition lump
// Items must be inserted in the order they are cycled through with the cursor
//
//=============================================================================

class DMenuDescriptor : public DObject
{
	DECLARE_CLASS(DMenuDescriptor, DObject)
public:
	FName mMenuName;
	FString mNetgameMessage;
	const PClass *mClass;

	virtual size_t PropagateMark() { return 0;  }
};

class DMenuItemBase;
class DOptionMenuItem;

class DListMenuDescriptor : public DMenuDescriptor
{
	DECLARE_CLASS(DListMenuDescriptor, DMenuDescriptor)

public:
	TArray<DMenuItemBase *> mItems;
	int mSelectedItem;
	int mSelectOfsX;
	int mSelectOfsY;
	FTextureID mSelector;
	int mDisplayTop;
	int mXpos, mYpos;
	int mWLeft, mWRight;
	int mLinespacing;	// needs to be stored for dynamically created menus
	int mAutoselect;	// this can only be set by internal menu creation functions
	FFont *mFont;
	EColorRange mFontColor;
	EColorRange mFontColor2;
	bool mCenter;

	void Reset()
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
	}
	
	size_t PropagateMark() override;
};

struct FOptionMenuSettings
{
	EColorRange mTitleColor;
	EColorRange mFontColor;
	EColorRange mFontColorValue;
	EColorRange mFontColorMore;
	EColorRange mFontColorHeader;
	EColorRange mFontColorHighlight;
	EColorRange mFontColorSelection;
	int mLinespacing;
};

class DOptionMenuDescriptor : public DMenuDescriptor
{
	DECLARE_CLASS(DOptionMenuDescriptor, DMenuDescriptor)

public:
	TArray<DOptionMenuItem *> mItems;
	FString mTitle;
	int mSelectedItem;
	int mDrawTop;
	int mScrollTop;
	int mScrollPos;
	int mIndent;
	int mPosition;
	bool mDontDim;

	void CalcIndent();
	DOptionMenuItem *GetItem(FName name);
	void Reset()
	{
		// Reset the default settings (ignore all other values in the struct)
		mPosition = 0;
		mScrollTop = 0;
		mIndent = 0;
		mDontDim = 0;
	}
	size_t PropagateMark() override;
	~DOptionMenuDescriptor()
	{
	}
};
						

typedef TMap<FName, DMenuDescriptor *> MenuDescriptorList;

extern FOptionMenuSettings OptionSettings;
extern MenuDescriptorList MenuDescriptors;

#define CURSORSPACE (14 * CleanXfac_1)

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
	bool mMouseCapture;
	bool mBackbuttonSelected;

public:
	enum
	{
		MOUSE_Click,
		MOUSE_Move,
		MOUSE_Release
	};

	enum
	{
		BACKBUTTON_TIME = 4*TICRATE
	};

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
	virtual void Close();
	virtual bool MouseEvent(int type, int x, int y);

	virtual void SetFocus(DMenuItemBase *fc) {}
	virtual bool CheckFocus(DMenuItemBase *fc) { return false;  }
	virtual void ReleaseFocus() {}


	bool CallMenuEvent(int mkey, bool fromcontroller);
	bool CallMouseEvent(int type, int x, int y);
	void CallDrawer();

	bool MouseEventBack(int type, int x, int y);
	void SetCapture();
	void ReleaseCapture();
	bool HasCapture()
	{
		return mMouseCapture;
	}
};

//=============================================================================
//
// base class for menu items
//
//=============================================================================

class DMenuItemBase : public DObject
{
	DECLARE_CLASS(DMenuItemBase, DObject)
public:
	int mXpos, mYpos;
	FName mAction;

	bool mEnabled;

	DMenuItemBase(int xpos = 0, int ypos = 0, FName action = NAME_None)
	{
		mXpos = xpos;
		mYpos = ypos;
		mAction = action;
		mEnabled = true;
	}

	virtual bool CheckCoordinate(int x, int y);
	virtual void Ticker();
	virtual void Drawer(bool selected);
	virtual bool Selectable();
	virtual bool Activate();
	virtual FName GetAction(int *pparam);
	virtual bool SetString(int i, const char *s);
	virtual bool GetString(int i, char *s, int len);
	virtual bool SetValue(int i, int value);
	virtual bool GetValue(int i, int *pvalue);
	virtual void Enable(bool on);
	virtual bool MenuEvent (int mkey, bool fromcontroller);
	virtual bool MouseEvent(int type, int x, int y);
	virtual bool CheckHotkey(int c);
	virtual int GetWidth();
	void DrawSelector(int xofs, int yofs, FTextureID tex);
	void OffsetPositionY(int ydelta) { mYpos += ydelta; }
	int GetY() { return mYpos; }
	int GetX() { return mXpos; }
	void SetX(int x) { mXpos = x; }
};	

class DListMenuItemStaticPatch_ : public DMenuItemBase
{
	DECLARE_CLASS(DListMenuItemStaticPatch_, DMenuItemBase)
protected:
	FTextureID mTexture;
	bool mCentered;

	DListMenuItemStaticPatch_() {}
public:
	DListMenuItemStaticPatch_(int x, int y, FTextureID patch, bool centered);
	void Drawer(bool selected);
};

class DListMenuItemStaticText_ : public DMenuItemBase
{
	DECLARE_CLASS(DListMenuItemStaticText_, DMenuItemBase)
protected:
	FString mText;
	FFont *mFont;
	EColorRange mColor;
	bool mCentered;

	DListMenuItemStaticText_() {}
public:
	DListMenuItemStaticText_(int x, int y, const char *text, FFont *font, EColorRange color, bool centered);
	void Drawer(bool selected);
};

//=============================================================================
//
// the player sprite window
//
//=============================================================================

class DListMenuItemPlayerDisplay_ : public DMenuItemBase
{
	DECLARE_CLASS(DListMenuItemPlayerDisplay_, DMenuItemBase)

	DListMenuDescriptor *mOwner;
	FTexture *mBackdrop;
	FRemapTable mRemap;
	FPlayerClass *mPlayerClass;
	FState *mPlayerState;
	int mPlayerTics;
	bool mNoportrait;
	BYTE mRotation;
	BYTE mMode;	// 0: automatic (used by class selection), 1: manual (used by player setup)
	BYTE mTranslate;
	int mSkin;
	int mRandomClass;
	int mRandomTimer;
	int mClassNum;

	void SetPlayerClass(int classnum, bool force = false);
	bool UpdatePlayerClass();
	void UpdateRandomClass();
	void UpdateTranslation();

	DListMenuItemPlayerDisplay_() {}
public:

	enum
	{
		PDF_ROTATION = 0x10001,
		PDF_SKIN = 0x10002,
		PDF_CLASS = 0x10003,
		PDF_MODE = 0x10004,
		PDF_TRANSLATE = 0x10005,
	};

	DListMenuItemPlayerDisplay_(DListMenuDescriptor *menu, int x, int y, PalEntry c1, PalEntry c2, bool np, FName action);
	void OnDestroy() override;
	virtual void Ticker();
	virtual void Drawer(bool selected);
	bool SetValue(int i, int value);
};


//=============================================================================
//
// selectable items
//
//=============================================================================

class DListMenuItemSelectable_ : public DMenuItemBase
{
	DECLARE_CLASS(DListMenuItemSelectable_, DMenuItemBase)
protected:
	int mHotkey;
	int mHeight;
	int mParam;

	DListMenuItemSelectable_() {}
public:
	DListMenuItemSelectable_(int x, int y, int height, FName childmenu, int mParam = -1);
	bool CheckCoordinate(int x, int y);
	bool Selectable();
	bool CheckHotkey(int c);
	bool Activate();
	bool MouseEvent(int type, int x, int y);
	FName GetAction(int *pparam);
};

class DListMenuItemText_ : public DListMenuItemSelectable_
{
	DECLARE_CLASS(DListMenuItemText_, DListMenuItemSelectable_)
	const char *mText;
	FFont *mFont;
	EColorRange mColor;
	EColorRange mColorSelected;

	DListMenuItemText_() {}
public:
	DListMenuItemText_(int x, int y, int height, int hotkey, const char *text, FFont *font, EColorRange color, EColorRange color2, FName child, int param = 0);
	void OnDestroy() override;
	void Drawer(bool selected);
	int GetWidth();
};

class DListMenuItemPatch_ : public DListMenuItemSelectable_
{
	DECLARE_CLASS(DListMenuItemPatch_, DListMenuItemSelectable_)
	FTextureID mTexture;

	DListMenuItemPatch_() {}
public:
	DListMenuItemPatch_(int x, int y, int height, int hotkey, FTextureID patch, FName child, int param = 0);
	void Drawer(bool selected);
	int GetWidth();
};

//=============================================================================
//
// items for the player menu
//
//=============================================================================

class DPlayerNameBox_ : public DListMenuItemSelectable_
{
	DECLARE_CLASS(DPlayerNameBox_, DListMenuItemSelectable_)
	FString mText;
	FFont *mFont;
	EColorRange mFontColor;
	int mFrameSize;
	char mPlayerName[MAXPLAYERNAME+1];
	char mEditName[MAXPLAYERNAME+2];
	bool mEntering;

	void DrawBorder (int x, int y, int len);

	DPlayerNameBox_() {}
public:

	DPlayerNameBox_(int x, int y, int height, int frameofs, const char *text, FFont *font, EColorRange color, FName action);
	bool SetString(int i, const char *s);
	bool GetString(int i, char *s, int len);
	void Drawer(bool selected);
	bool MenuEvent (int mkey, bool fromcontroller);
};

//=============================================================================
//
// items for the player menu
//
//=============================================================================

class DValueTextItem_ : public DListMenuItemSelectable_
{
	DECLARE_CLASS(DValueTextItem_, DListMenuItemSelectable_)
	TArray<FString> mSelections;
	FString mText;
	int mSelection;
	FFont *mFont;
	EColorRange mFontColor;
	EColorRange mFontColor2;

	DValueTextItem_() {}
public:

	DValueTextItem_(int x, int y, int height, const char *text, FFont *font, EColorRange color, EColorRange valuecolor, FName action, FName values);
	bool SetString(int i, const char *s);
	bool SetValue(int i, int value);
	bool GetValue(int i, int *pvalue);
	bool MenuEvent (int mkey, bool fromcontroller);
	void Drawer(bool selected);
};

//=============================================================================
//
// items for the player menu
//
//=============================================================================

class DSliderItem_ : public DListMenuItemSelectable_
{
	DECLARE_CLASS(DSliderItem_, DListMenuItemSelectable_)
	FString mText;
	FFont *mFont;
	EColorRange mFontColor;
	int mMinrange, mMaxrange;
	int mStep;
	int mSelection;

	void DrawSlider (int x, int y);

	DSliderItem_() {}
public:

	DSliderItem_(int x, int y, int height, const char *text, FFont *font, EColorRange color, FName action, int min, int max, int step);
	bool SetValue(int i, int value);
	bool GetValue(int i, int *pvalue);
	bool MenuEvent (int mkey, bool fromcontroller);
	void Drawer(bool selected);
	bool MouseEvent(int type, int x, int y);
};

//=============================================================================
//
// list menu class runs a menu described by a DListMenuDescriptor
//
//=============================================================================

class DListMenu : public DMenu
{
	DECLARE_CLASS(DListMenu, DMenu)
	HAS_OBJECT_POINTERS;

protected:
	DListMenuDescriptor *mDesc;
	DMenuItemBase *mFocusControl;

public:
	DListMenu(DMenu *parent = NULL, DListMenuDescriptor *desc = NULL);
	virtual void Init(DMenu *parent = NULL, DListMenuDescriptor *desc = NULL);
	DMenuItemBase *GetItem(FName name);
	bool Responder (event_t *ev);
	bool MenuEvent (int mkey, bool fromcontroller);
	bool MouseEvent(int type, int x, int y);
	void Ticker ();
	void Drawer ();
	void SetFocus(DMenuItemBase *fc)
	{
		mFocusControl = fc;
	}
	bool CheckFocus(DMenuItemBase *fc)
	{
		return mFocusControl == fc;
	}
	void ReleaseFocus()
	{
		mFocusControl = NULL;
	}
};


//=============================================================================
//
// base class for menu items
//
//=============================================================================

class DOptionMenuItem : public DMenuItemBase
{
	DECLARE_ABSTRACT_CLASS(DOptionMenuItem, DMenuItemBase)
public:
	FString mLabel;
	bool mCentered;

	void drawLabel(int indent, int y, EColorRange color, bool grayed = false);

	DOptionMenuItem(const char *text = nullptr, FName action = NAME_None, bool center = false)
		: DMenuItemBase(0, 0, action)
	{
		mLabel = text;
		mCentered = center;
	}

	virtual int Draw(DOptionMenuDescriptor *desc, int y, int indent, bool selected);
	virtual bool Selectable();
	virtual int GetIndent();
	virtual bool MouseEvent(int type, int x, int y);
};	

//=============================================================================
//
//
//
//=============================================================================
struct FOptionValues
{
	struct Pair
	{
		double Value;
		FString TextValue;
		FString Text;
	};

	TArray<Pair> mValues;
};

typedef TMap< FName, FOptionValues* > FOptionMap;

extern FOptionMap OptionValues;


//=============================================================================
//
// Option menu class runs a menu described by a DOptionMenuDescriptor
//
//=============================================================================

class DOptionMenu : public DMenu
{
	DECLARE_CLASS(DOptionMenu, DMenu)
	HAS_OBJECT_POINTERS;

public: // needs to be public for script access
	bool CanScrollUp;
	bool CanScrollDown;
	int VisBottom;
	DOptionMenuItem *mFocusControl;
	DOptionMenuDescriptor *mDesc;

//public:
	DOptionMenuItem *GetItem(FName name);
	DOptionMenu(DMenu *parent = NULL, DOptionMenuDescriptor *desc = NULL);
	virtual void Init(DMenu *parent = NULL, DOptionMenuDescriptor *desc = NULL);
	int FirstSelectable();
	bool Responder (event_t *ev);
	bool MenuEvent (int mkey, bool fromcontroller);
	bool MouseEvent(int type, int x, int y);
	void Ticker ();
	void Drawer ();
	const DOptionMenuDescriptor *GetDescriptor() const { return mDesc; }
	void SetFocus(DOptionMenuItem *fc)
	{
		mFocusControl = fc;
	}
	bool CheckFocus(DOptionMenuItem *fc)
	{
		return mFocusControl == fc;
	}
	void ReleaseFocus()
	{
		mFocusControl = NULL;
	}
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

	// [TP]
	bool AllowColors;

public:

	// [TP] Added allowcolors
	DTextEnterMenu(DMenu *parent, char *textbuffer, int maxlen, int sizemode, bool showgrid, bool allowcolors = false);

	void Drawer ();
	bool MenuEvent (int mkey, bool fromcontroller);
	bool Responder(event_t *ev);
	bool TranslateKeyboardEvents();
	bool MouseEvent(int type, int x, int y);

};




struct event_t;
void M_EnableMenu (bool on) ;
bool M_Responder (event_t *ev);
void M_Ticker (void);
void M_Drawer (void);
void M_Init (void);
void M_CreateMenus();
void M_ActivateMenu(DMenu *menu);
void M_ClearMenus ();
void M_ParseMenuDefs();
void M_StartupSkillMenu(FGameStartup *gs);
int M_GetDefaultSkill();
void M_StartControlPanel (bool makeSound);
void M_SetMenu(FName menu, int param = -1);
void M_StartMessage(const char *message, int messagemode, FName action = NAME_None);
DMenu *StartPickerMenu(DMenu *parent, const char *name, FColorCVar *cvar);
void M_RefreshModesList ();
void M_InitVideoModesMenu ();
void M_MarkMenus();


#endif
