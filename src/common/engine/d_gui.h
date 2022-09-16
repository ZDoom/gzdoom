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

// include integer logarithm based on lzcnt32 to reverse power of 2 enum constants
#include "math/ilogxi.h"

// For a GUIEvent, x and y specify absolute location of mouse pointer
enum EGUIEvent
{
	EV_GUI_None             = 1 <<  0,
	EV_GUI_KeyDown          = 1 <<  1,	// data1: unshifted ASCII, data2: shifted ASCII, data3: modifiers
	EV_GUI_KeyRepeat        = 1 <<  2,	// same
	EV_GUI_KeyUp            = 1 <<  3,	// same
	EV_GUI_Char             = 1 <<  4,	// data1: translated character (for user text input), data2: alt down?
	EV_GUI_FirstMouseEvent  = 1 <<  5,
	EV_GUI_MouseMove        = 1 <<  6,
	EV_GUI_LButtonDown      = 1 <<  7,
	EV_GUI_LButtonUp        = 1 <<  8,
	EV_GUI_LButtonDblClick  = 1 <<  9,
	EV_GUI_MButtonDown      = 1 << 10,
	EV_GUI_MButtonUp        = 1 << 11,  
	EV_GUI_MButtonDblClick  = 1 << 12,
	EV_GUI_RButtonDown      = 1 << 13,
	EV_GUI_RButtonUp        = 1 << 14,
	EV_GUI_RButtonDblClick  = 1 << 15,
	EV_GUI_WheelEvent       = 1 << 16,
	EV_GUI_WheelUp          = 1 << 17,	// data3: shift/ctrl/alt
	EV_GUI_WheelDown        = 1 << 18,	// "
	EV_GUI_WheelRight       = 1 << 19,	// "
	EV_GUI_WheelLeft        = 1 << 20,	// "
	EV_GUI_BackButtonDown   = 1 << 21,
	EV_GUI_BackButtonUp     = 1 << 22,
	EV_GUI_FwdButtonDown    = 1 << 23,
	EV_GUI_FwdButtonUp      = 1 << 24,
	EV_GUI_LastMouseEvent   = 1 << 25,
};

enum GUIKeyModifiers
{
	GKM_SHIFT               = 1 << 0,
	GKM_CTRL                = 1 << 1,
	GKM_ALT                 = 1 << 2,
	GKM_META                = 1 << 3,
	GKM_LBUTTON             = 1 << 4,
};

// Special codes for some GUI keys, including a few real ASCII codes.
enum ESpecialGUIKeys
{
	GK_PGDN	                = 1,
	GK_PGUP                 = 2,
	GK_HOME                 = 3,
	GK_END                  = 4,
	GK_LEFT                 = 5,
	GK_RIGHT                = 6,
	GK_ALERT                = 7,		// ASCII bell
	GK_BACKSPACE            = 8,		// ASCII
	GK_TAB                  = 9,		// ASCII
	GK_LINEFEED             = 10,		// ASCII
	GK_DOWN                 = 10,
	GK_VTAB                 = 11,		// ASCII
	GK_UP                   = 11,
	GK_FORMFEED             = 12,		// ASCII
	GK_RETURN               = 13,		// ASCII
	GK_F1                   = 14,
	GK_F2                   = 15,
	GK_F3                   = 16,
	GK_F4                   = 17,
	GK_F5                   = 18,
	GK_F6                   = 19,
	GK_F7                   = 20,
 	GK_F8                   = 21,
	GK_F9                   = 22,
	GK_F10                  = 23,
	GK_F11                  = 24,
	GK_F12                  = 25,
	GK_DEL                  = 26,
	GK_ESCAPE               = 27,		// ASCII
	GK_FREE1                = 28,
	GK_FREE2                = 29,
	GK_BACK                 = 30,		// browser back key
	GK_CESCAPE              = 31		// color escape
};

#endif //__D_GUI_H__
