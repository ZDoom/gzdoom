#ifndef __D_GUI_H__
#define __D_GUI_H__

// For a GUIEvent, x and y specify absolute location of mouse pointer
enum EGUIEvent
{
	EV_GUI_None,
	EV_GUI_KeyDown,			// data1: unshifted ASCII, data2: shifted ASCII, data3: modifiers
	EV_GUI_KeyRepeat,		// same
	EV_GUI_KeyUp,			// same
	EV_GUI_MouseMove,
	EV_GUI_LButtonDown,
	EV_GUI_LButtonUp,
	EV_GUI_LButtonDblClick,
	EV_GUI_MButtonDown,
	EV_GUI_MButtonUp,
	EV_GUI_MButtonDblClick,
	EV_GUI_RButtonDown,
	EV_GUI_RButtonUp,
	EV_GUI_RButtonDblClick,
	EV_GUI_WheelUp,
	EV_GUI_WheelDown
};

enum GUIKeyModifiers
{
	GKM_SHIFT	= 1,
	GKM_CTRL	= 2,
	GKM_ALT		= 4
};

// Special "ASCII" codes for some GUI keys
enum ESpecialGUIKeys
{
	GK_PGUP		= 1,
	GK_PGDN		= 2,
	GK_HOME		= 3,
	GK_END		= 4,
	GK_LEFT		= 5,
	GK_RIGHT	= 6,
	GK_UP		= 11,
	GK_DOWN		= 10,
	GK_DEL		= 14,
	GK_ESCAPE	= 27
};

#endif //__D_GUI_H__