#ifndef __M_MENU_MENU_H__
#define __M_MENU_MENU_H__




#include "dobject.h"
#include "d_player.h"
#include "r_data/r_translate.h"
#include "c_cvars.h"
#include "v_font.h"
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
	FString SaveTitle;
	FString Filename;
	bool bOldVersion = false;
	bool bMissingWads = false;
	bool bNoDelete = false;
};

struct FSavegameManager
{
private:
	TArray<FSaveGameNode*> SaveGames;
	FSaveGameNode NewSaveNode;
	int LastSaved = -1;
	int LastAccessed = -1;
	TArray<char> SavePicData;
	FTexture *SavePic = nullptr;
	FBrokenLines *SaveComment = nullptr;

public:
	int WindowSize = 0;
	FSaveGameNode *quickSaveSlot = nullptr;
	~FSavegameManager();

private:
	int InsertSaveNode(FSaveGameNode *node);
public:
	void NotifyNewSave(const FString &file, const FString &title, bool okForQuicksave);
	void ClearSaveGames();

	void ReadSaveStrings();
	void UnloadSaveData();

	int RemoveSaveSlot(int index);
	void LoadSavegame(int Selected);
	void DoSave(int Selected, const char *savegamestring);
	unsigned ExtractSaveData(int index);
	void ClearSaveStuff();
	bool DrawSavePic(int x, int y, int w, int h);
	void DrawSaveComment(FFont *font, int cr, int x, int y, int scalefactor);
	void SetFileInfo(int Selected);
	unsigned SavegameCount();
	FSaveGameNode *GetSavegame(int i);
	void InsertNewSaveNode();
	bool RemoveNewSaveNode();

};

extern FSavegameManager savegameManager;
class DMenu;
extern DMenu *CurrentMenu;
extern int MenuTime;
class DMenuItemBase;

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
	PClass *mClass = nullptr;
	bool mProtected = false;
	TArray<DMenuItemBase *> mItems;

	virtual size_t PropagateMark() { return 0;  }
};


class DListMenuDescriptor : public DMenuDescriptor
{
	DECLARE_CLASS(DListMenuDescriptor, DMenuDescriptor)

public:
	int mSelectedItem;
	double mSelectOfsX;
	double mSelectOfsY;
	FTextureID mSelector;
	int mDisplayTop;
	double mXpos, mYpos;
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



public:
	enum
	{
		MOUSE_Click,
		MOUSE_Move,
		MOUSE_Release
	};

	TObjPtr<DMenu*> mParentMenu;
	bool mMouseCapture;
	bool mBackbuttonSelected;
	bool DontDim;

	DMenu(DMenu *parent = NULL);
	bool TranslateKeyboardEvents();
	virtual void Close();

	bool CallResponder(event_t *ev);
	bool CallMenuEvent(int mkey, bool fromcontroller);
	void CallTicker();
	void CallDrawer();
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
	double mXpos, mYpos;
	FNameNoInit mAction;
	bool mEnabled;

	bool Activate();
	bool SetString(int i, const char *s);
	bool GetString(int i, char *s, int len);
	bool SetValue(int i, int value);
	bool GetValue(int i, int *pvalue);
	void OffsetPositionY(int ydelta) { mYpos += ydelta; }
	double GetY() { return mYpos; }
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
//
//
//=============================================================================

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
void M_StartControlPanel (bool makeSound);
void M_SetMenu(FName menu, int param = -1);
void M_StartMessage(const char *message, int messagemode, FName action = NAME_None);
DMenu *StartPickerMenu(DMenu *parent, const char *name, FColorCVar *cvar);
void M_RefreshModesList ();
void M_InitVideoModesMenu ();
void M_MarkMenus();


struct IJoystickConfig;
DMenuItemBase * CreateOptionMenuItemStaticText(const char *name, int v = -1);
DMenuItemBase * CreateOptionMenuItemSubmenu(const char *label, FName cmd, int center);
DMenuItemBase * CreateOptionMenuItemControl(const char *label, FName cmd, FKeyBindings *bindings);
DMenuItemBase * CreateOptionMenuItemJoyConfigMenu(const char *label, IJoystickConfig *joy);
DMenuItemBase * CreateListMenuItemPatch(double x, double y, int height, int hotkey, FTextureID tex, FName command, int param);
DMenuItemBase * CreateListMenuItemText(double x, double y, int height, int hotkey, const char *text, FFont *font, PalEntry color1, PalEntry color2, FName command, int param);
DMenuItemBase * CreateOptionMenuItemCommand(const char *label, FName cmd, bool centered = false);

#endif
