#ifndef __M_MENU_H__
#define __M_MENU_H__




#include "dobject.h"
#include "textures/textures.h"

struct event_t;
class FTexture;
class FFont;
enum EColorRange;

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
	TObjPtr<DMenu> mParentMenu;

public:
	static DMenu *CurrentMenu;

	DMenu(DMenu *parent = NULL);
	virtual ~DMenu();
	virtual bool Responder (event_t *ev);
	virtual void Ticker ();
	virtual void Drawer ();
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
	FListMenuItem(int xpos = 0, int ypos = 0);
	virtual ~FListMenuItem();

	virtual bool CheckCoordinate(int x, int y);
	virtual void Ticker();
	virtual void Drawer();
	virtual bool Selectable();
	void DrawSelector(int xofs, int yofs, FTextureID tex);
};	

class FListMenuItemStaticPatch : public FListMenuItem
{
protected:
	FTextureID mTexture;

public:
	FListMenuItemStaticPatch(int x, int y, FTextureID patch);
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

class FListMenuItemSelectable : public FListMenuItem
{
protected:
	FName mChild;
	FMenuRect mHotspot;
	int mHotkey;

public:
	FListMenuItemSelectable(int x, int y, FName childmenu);
	void SetHotspot(int x, int y, int w, int h);
	bool CheckCoordinate(int x, int y);
	bool Selectable();
	bool CheckHotkey(int c) { return c == mHotkey; }
};

class FListMenuItemText : public FListMenuItemSelectable
{
	const char *mText;
	FFont *mFont;
	EColorRange mColor;
public:
	FListMenuItemText(int x, int y, int hotkey, const char *text, FFont *font, EColorRange color, FName child);
	~FListMenuItemText();
	void Drawer();
};

class FListMenuItemPatch : public FListMenuItemSelectable
{
	FTextureID mTexture;
public:
	FListMenuItemPatch(int x, int y, int hotkey, FTextureID patch, FName child);
	void Drawer();
};


//=============================================================================
//
// menu descriptor. This is created from the menu definition lump
// Items must be inserted in the order they are cycled through with the cursor
//
//=============================================================================

struct FListMenuDescriptor
{
	FName mMenuName;
	TDeletingArray<FListMenuItem *> mItems;
	int mSelectedItem;
	int mSelectOfsX;
	int mSelectOfsY;
	FTextureID mSelector;
	int mDisplayTop;
};

//=============================================================================
//
// list menu class runs a menu described by a FListMrnuDescriptor
//
//=============================================================================

class DListMenu : public DMenu
{
	DECLARE_CLASS(DListMenu, DMenu)

	FListMenuDescriptor *mDesc;

public:
	DListMenu(DMenu *parent = NULL, FListMenuDescriptor *desc = NULL);
	~DListMenu();
	bool Responder (event_t *ev);
	void Ticker ();
	void Drawer ();
};



#endif