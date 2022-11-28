/*
** d_gui.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
** So when do I get a real UT-like windowing system?
*/

#ifndef __D_GUI_H__
#define __D_GUI_H__

// For a GUIEvent, x and y specify absolute location of mouse pointer
enum EGUIEvent
{
	EV_GUI_None,
	EV_GUI_KeyDown,			// data1: unshifted ASCII, data2: shifted ASCII, data3: modifiers
	EV_GUI_KeyRepeat,		// same
	EV_GUI_KeyUp,			// same
	EV_GUI_Char,			// data1: translated character (for user text input), data2: alt down?
	EV_GUI_FirstMouseEvent,
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
		EV_GUI_WheelUp,			// data3: shift/ctrl/alt
		EV_GUI_WheelDown,		// "
		EV_GUI_WheelRight,		// "
		EV_GUI_WheelLeft,		// "
		EV_GUI_BackButtonDown,
		EV_GUI_BackButtonUp,
		EV_GUI_FwdButtonDown,
		EV_GUI_FwdButtonUp,
	EV_GUI_LastMouseEvent,
};

enum GUIKeyModifiers
{
	GKM_SHIFT	= 1,
	GKM_CTRL	= 2,
	GKM_ALT		= 4,
	GKM_META	= 8,
	GKM_LBUTTON = 16
};

// Special codes for some GUI keys, including a few real ASCII codes.
enum ESpecialGUIKeys
{
	GK_PGDN		= 1,
	GK_PGUP		= 2,
	GK_HOME		= 3,
	GK_END		= 4,
	GK_LEFT		= 5,
	GK_RIGHT	= 6,
	GK_ALERT	= 7,		// ASCII bell
	GK_BACKSPACE= 8,		// ASCII
	GK_TAB		= 9,		// ASCII
	GK_LINEFEED	= 10,		// ASCII
	GK_DOWN		= 10,
	GK_VTAB		= 11,		// ASCII
	GK_UP		= 11,
	GK_FORMFEED	= 12,		// ASCII
	GK_RETURN	= 13,		// ASCII
	GK_F1		= 14,
	GK_F2		= 15,
	GK_F3		= 16,
	GK_F4		= 17,
	GK_F5		= 18,
	GK_F6		= 19,
	GK_F7		= 20,
	GK_F8		= 21,
	GK_F9		= 22,
	GK_F10		= 23,
	GK_F11		= 24,
	GK_F12		= 25,
	GK_DEL		= 26,
	GK_ESCAPE	= 27,		// ASCII
	GK_FREE1	= 28,
	GK_FREE2	= 29,
	GK_BACK		= 30,		// browser back key
	GK_CESCAPE	= 31		// color escape
};

#endif //__D_GUI_H__
