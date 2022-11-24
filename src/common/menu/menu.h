#ifndef __M_MENU_MENU_H__
#define __M_MENU_MENU_H__




#include "dobject.h"
#include "c_cvars.h"
#include "v_font.h"
#include "textures.h"

EXTERN_CVAR(Float, snd_menuvolume)
EXTERN_CVAR(Int, m_use_mouse);


struct event_t;
class FTexture;
class FFont;
enum EColorRange : int;
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


class DMenu;
extern DMenu *CurrentMenu;
extern int MenuTime;
class DMenuItemBase;
extern DObject* menuDelegate;

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
	FName mMenuName = NAME_None;
	FString mNetgameMessage;
	PClass *mClass = nullptr;
	bool mProtected = false;
	TArray<DMenuItemBase *> mItems;

	size_t PropagateMark() override;
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
	bool mFromEngine;
	bool mAnimated;
	bool mAnimatedTransition;
	bool mDontDim;
	bool mDontBlur;
	int mVirtWidth;
	int mVirtHeight;
	bool mCustomSizeSet;
	bool mForceList;

	void Reset();
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
	bool mDontBlur;
	bool mAnimatedTransition;
	bool mAnimated;
	FFont *mFont;

	void CalcIndent();
	DMenuItemBase *GetItem(FName name);
	void Reset();
	~DOptionMenuDescriptor() = default;
};

class DImageScrollerDescriptor : public DMenuDescriptor
{
	DECLARE_CLASS(DOptionMenuDescriptor, DMenuDescriptor)
public:
	FTextureID textBackground;
	PalEntry textBackgroundBrightness;

	FFont *textFont;
	double textScale;
	bool mAnimatedTransition;
	bool mAnimated;
	bool mDontDim;
	bool mDontBlur;
	int virtWidth, virtHeight;

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


enum MenuTransitionType
{ // Note: This enum is for logical categories, not visual types.
	MA_None,
	MA_Return,
	MA_Advance,
};

class DMenu;

struct MenuTransition
{
	DMenu* previous;
	DMenu* current;

	double start;
	int32_t length;
	int8_t dir;
	bool destroyprev;

	bool StartTransition(DMenu* from, DMenu* to, MenuTransitionType animtype);
	bool Draw();

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
	bool DontBlur;
	bool Animated;
	bool AnimatedTransition;
	static int InMenu;

	DMenu(DMenu *parent = NULL);
	bool TranslateKeyboardEvents();
	virtual void Close();

	bool CallResponder(event_t *ev);
	bool CallMenuEvent(int mkey, bool fromcontroller);
	void CallTicker();
	void CallDrawer();
	bool canAnimate() { return AnimatedTransition; }
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
	FName mAction;
	int mEnabled;

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
void M_PreviousMenu ();
void M_ParseMenuDefs();
void M_StartControlPanel(bool makeSound, bool scaleoverride = false);
void M_SetMenu(FName menu, int param = -1);
void M_StartMessage(const char *message, int messagemode, FName action = NAME_None);
DMenu *StartPickerMenu(DMenu *parent, const char *name, FColorCVar *cvar);
void M_MarkMenus();
FTextureID GetMenuTexture(const char* const name);
void DeinitMenus();
bool M_Active();
bool M_IsAnimated();


struct IJoystickConfig;
DMenuItemBase * CreateOptionMenuItemStaticText(const char *name, int v = -1);
DMenuItemBase * CreateOptionMenuItemSubmenu(const char *label, FName cmd, int center);
DMenuItemBase * CreateOptionMenuItemControl(const char *label, FName cmd, FKeyBindings *bindings);
DMenuItemBase * CreateOptionMenuItemJoyConfigMenu(const char *label, IJoystickConfig *joy);
DMenuItemBase * CreateListMenuItemPatch(double x, double y, int height, int hotkey, FTextureID tex, FName command, int param);
DMenuItemBase * CreateListMenuItemText(double x, double y, int height, int hotkey, const char *text, FFont *font, PalEntry color1, PalEntry color2, FName command, int param);
DMenuItemBase * CreateOptionMenuItemCommand(const char *label, FName cmd, bool centered = false);
DMenuItemBase* CreateListMenuItemStaticText(double x, double y, const char* text, FFont* font, PalEntry color, bool centered = false);

void UpdateVRModes(bool considerQuadBuffered=true);

#endif
