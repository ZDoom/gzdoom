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
	TArray<DMenuItemBase *> mItems;
	FString mTitle;
	int mSelectedItem;
	int mDrawTop;
	int mScrollTop;
	int mScrollPos;
	int mIndent;
	int mPosition;
	bool mDontDim;

	void CalcIndent();
	DMenuItemBase *GetItem(FName name);
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
	FNameNoInit mAction;
	bool mEnabled;

	bool CheckCoordinate(int x, int y);
	void Ticker();
	void Drawer(bool selected);
	bool Selectable();
	bool Activate();
	FName GetAction(int *pparam);
	bool SetString(int i, const char *s);
	bool GetString(int i, char *s, int len);
	bool SetValue(int i, int value);
	bool GetValue(int i, int *pvalue);
	void Enable(bool on);
	bool MenuEvent (int mkey, bool fromcontroller);
	bool MouseEvent(int type, int x, int y);
	bool CheckHotkey(int c);
	int GetWidth();
	int GetIndent();
	int Draw(DOptionMenuDescriptor *desc, int y, int indent, bool selected);
	void OffsetPositionY(int ydelta) { mYpos += ydelta; }
	int GetY() { return mYpos; }
	int GetX() { return mXpos; }
	void SetX(int x) { mXpos = x; }

	void DrawSelector(int xofs, int yofs, FTextureID tex);

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
	DMenuItemBase *mFocusControl;
	DOptionMenuDescriptor *mDesc;

//public:
	DMenuItemBase *GetItem(FName name);
	DOptionMenu(DMenu *parent = NULL, DOptionMenuDescriptor *desc = NULL);
	virtual void Init(DMenu *parent = NULL, DOptionMenuDescriptor *desc = NULL);
	int FirstSelectable();
	bool Responder (event_t *ev);
	bool MenuEvent (int mkey, bool fromcontroller);
	bool MouseEvent(int type, int x, int y);
	void Ticker ();
	void Drawer ();
	const DOptionMenuDescriptor *GetDescriptor() const { return mDesc; }
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


struct IJoystickConfig;
DMenuItemBase * CreateOptionMenuItemStaticText(const char *name, bool v);
DMenuItemBase * CreateOptionMenuSliderVar(const char *name, int index, double min, double max, double step, int showval);
DMenuItemBase * CreateOptionMenuItemCommand(const char * label, FName cmd);
DMenuItemBase * CreateOptionMenuItemOption(const char * label, FName cmd, FName values, FBaseCVar *graycheck, bool center);
DMenuItemBase * CreateOptionMenuItemSubmenu(const char *label, FName cmd, int center);
DMenuItemBase * CreateOptionMenuItemControl(const char *label, FName cmd, FKeyBindings *bindings);
DMenuItemBase * CreateOptionMenuItemJoyConfigMenu(const char *label, IJoystickConfig *joy);
DMenuItemBase * CreateOptionMenuItemJoyMap(const char *label, int axis, FName values, bool center);
DMenuItemBase * CreateOptionMenuSliderJoySensitivity(const char * label, double min, double max, double step, int showval);
DMenuItemBase * CreateOptionMenuSliderJoyScale(const char *label, int axis, double min, double max, double step, int showval);
DMenuItemBase * CreateOptionMenuItemInverter(const char *label, int axis, int center);
DMenuItemBase * CreateOptionMenuSliderJoyDeadZone(const char *label, int axis, double min, double max, double step, int showval);
DMenuItemBase * CreateListMenuItemPatch(int x, int y, int height, int hotkey, FTextureID tex, FName command, int param);
DMenuItemBase * CreateListMenuItemText(int x, int y, int height, int hotkey, const char *text, FFont *font, PalEntry color1, PalEntry color2, FName command, int param);

#endif
