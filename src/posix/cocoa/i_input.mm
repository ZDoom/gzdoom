/*
 ** i_input.mm
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2015 Alexey Lysiuk
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
 */

#include "i_common.h"

#import <Carbon/Carbon.h>

// Avoid collision between DObject class and Objective-C
#define Class ObjectClass

#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "d_event.h"
#include "d_gui.h"
#include "dikeys.h"
#include "doomdef.h"
#include "doomstat.h"
#include "v_video.h"

#undef Class


EXTERN_CVAR(Int, m_use_mouse)

CVAR(Bool, use_mouse,    true,  CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, m_noprescale, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, m_filter,     false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR(Bool, k_allowfullscreentoggle, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Int, mouse_capturemode, 1, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 2)
	{
		self = 2;
	}
}


extern int paused, chatmodeon;
extern constate_e ConsoleState;

bool GUICapture;


namespace
{

// TODO: remove this magic!
size_t s_skipMouseMoves;


// ---------------------------------------------------------------------------


void CheckGUICapture()
{
	const bool wantCapture = (MENU_Off == menuactive)
		? (c_down == ConsoleState || c_falling == ConsoleState || chatmodeon)
		: (MENU_On == menuactive || MENU_OnNoPause == menuactive);

	if (wantCapture != GUICapture)
	{
		GUICapture = wantCapture;

		ResetButtonStates();
	}
}

void CenterCursor()
{
	NSWindow* window = [NSApp keyWindow];
	if (nil == window)
	{
		return;
	}

	const NSRect  displayRect = [[window screen] frame];
	const NSRect   windowRect = [window frame];
	const CGPoint centerPoint = CGPointMake(NSMidX(windowRect), displayRect.size.height - NSMidY(windowRect));

	CGEventSourceRef eventSource = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);

	if (NULL != eventSource)
	{
		CGEventRef mouseMoveEvent = CGEventCreateMouseEvent(eventSource,
			kCGEventMouseMoved, centerPoint, kCGMouseButtonLeft);

		if (NULL != mouseMoveEvent)
		{
			CGEventPost(kCGHIDEventTap, mouseMoveEvent);
			CFRelease(mouseMoveEvent);
		}

		CFRelease(eventSource);
	}

	// TODO: remove this magic!
	s_skipMouseMoves = 2;
}


bool IsInGame()
{
	switch (mouse_capturemode)
	{
		default:
		case 0:
			return gamestate == GS_LEVEL;

		case 1:
			return gamestate == GS_LEVEL
				|| gamestate == GS_INTERMISSION
				|| gamestate == GS_FINALE;

		case 2:
			return true;
	}
}

void CheckNativeMouse()
{
	const bool windowed = (NULL == screen) || !screen->IsFullscreen();
	bool wantNative;

	if (windowed)
	{
		if (![NSApp isActive] || !use_mouse)
		{
			wantNative = true;
		}
		else if (MENU_WaitKey == menuactive)
		{
			wantNative = false;
		}
		else
		{
			wantNative = (!m_use_mouse || MENU_WaitKey != menuactive)
				&& (!IsInGame() || GUICapture || paused || demoplayback);
		}
	}
	else
	{
		// ungrab mouse when in the menu with mouse control on.
		wantNative = m_use_mouse
			&& (MENU_On == menuactive || MENU_OnNoPause == menuactive);
	}

	I_SetNativeMouse(wantNative);
}

} // unnamed namespace


void I_GetEvent()
{
	[[NSRunLoop currentRunLoop] limitDateForMode:NSDefaultRunLoopMode];
}

void I_StartTic()
{
	CheckGUICapture();
	CheckNativeMouse();

	I_ProcessJoysticks();
	I_GetEvent();
}

void I_StartFrame()
{
}


void I_SetMouseCapture()
{
}

void I_ReleaseMouseCapture()
{
}

void I_SetNativeMouse(bool wantNative)
{
	static bool nativeMouse = true;

	if (wantNative != nativeMouse)
	{
		nativeMouse = wantNative;

		if (!wantNative)
		{
			CenterCursor();
		}

		CGAssociateMouseAndMouseCursorPosition(wantNative);

		if (wantNative)
		{
			[NSCursor unhide];
		}
		else
		{
			[NSCursor hide];
		}
	}
}


// ---------------------------------------------------------------------------


namespace
{

const size_t KEY_COUNT = 128;


// See Carbon -> HIToolbox -> Events.h for kVK_ constants

const uint8_t KEYCODE_TO_DIK[KEY_COUNT] =
{
	DIK_A,        DIK_S,             DIK_D,         DIK_F,        DIK_H,           DIK_G,       DIK_Z,        DIK_X,          // 0x00 - 0x07
	DIK_C,        DIK_V,             0,             DIK_B,        DIK_Q,           DIK_W,       DIK_E,        DIK_R,          // 0x08 - 0x0F
	DIK_Y,        DIK_T,             DIK_1,         DIK_2,        DIK_3,           DIK_4,       DIK_6,        DIK_5,          // 0x10 - 0x17
	DIK_EQUALS,   DIK_9,             DIK_7,         DIK_MINUS,    DIK_8,           DIK_0,       DIK_RBRACKET, DIK_O,          // 0x18 - 0x1F
	DIK_U,        DIK_LBRACKET,      DIK_I,         DIK_P,        DIK_RETURN,      DIK_L,       DIK_J,        DIK_APOSTROPHE, // 0x20 - 0x27
	DIK_K,        DIK_SEMICOLON,     DIK_BACKSLASH, DIK_COMMA,    DIK_SLASH,       DIK_N,       DIK_M,        DIK_PERIOD,     // 0x28 - 0x2F
	DIK_TAB,      DIK_SPACE,         DIK_GRAVE,     DIK_BACK,     0,               DIK_ESCAPE,  0,            DIK_LWIN,       // 0x30 - 0x37
	DIK_LSHIFT,   DIK_CAPITAL,       DIK_LMENU,     DIK_LCONTROL, DIK_RSHIFT,      DIK_RMENU,   DIK_RCONTROL, 0,              // 0x38 - 0x3F
	0,            DIK_DECIMAL,       0,             DIK_MULTIPLY, 0,               DIK_ADD,     0,            0,              // 0x40 - 0x47
	DIK_VOLUMEUP, DIK_VOLUMEDOWN,    DIK_MUTE,      DIK_SLASH,    DIK_NUMPADENTER, 0,           DIK_SUBTRACT, 0,              // 0x48 - 0x4F
	0,            DIK_NUMPAD_EQUALS, DIK_NUMPAD0,   DIK_NUMPAD1,  DIK_NUMPAD2,     DIK_NUMPAD3, DIK_NUMPAD4,  DIK_NUMPAD5,    // 0x50 - 0x57
	DIK_NUMPAD6,  DIK_NUMPAD7,       0,             DIK_NUMPAD8,  DIK_NUMPAD9,     0,           0,            0,              // 0x58 - 0x5F
	DIK_F5,       DIK_F6,            DIK_F7,        DIK_F3,       DIK_F8,          DIK_F9,      0,            DIK_F11,        // 0x60 - 0x67
	0,            DIK_F13,           0,             DIK_F14,      0,               DIK_F10,     0,            DIK_F12,        // 0x68 - 0x6F
	0,            DIK_F15,           0,             DIK_HOME,     0,               DIK_DELETE,  DIK_F4,       DIK_END,        // 0x70 - 0x77
	DIK_F2,       0,                 DIK_F1,        DIK_LEFT,     DIK_RIGHT,       DIK_DOWN,    DIK_UP,       0,              // 0x78 - 0x7F
};

const uint8_t KEYCODE_TO_ASCII[KEY_COUNT] =
{
	'a', 's',  'd', 'f', 'h', 'g', 'z',  'x', // 0x00 - 0x07
	'c', 'v',    0, 'b', 'q', 'w', 'e',  'r', // 0x08 - 0x0F
	'y', 't',  '1', '2', '3', '4', '6',  '5', // 0x10 - 0x17
	'=', '9',  '7', '-', '8', '0', ']',  'o', // 0x18 - 0x1F
	'u', '[',  'i', 'p',  13, 'l', 'j', '\'', // 0x20 - 0x27
	'k', ';', '\\', ',', '/', 'n', 'm',  '.', // 0x28 - 0x2F
	  9, ' ',  '`',  12,   0,  27,   0,    0, // 0x30 - 0x37
	  0,   0,    0,   0,   0,   0,   0,    0, // 0x38 - 0x3F
	  0,   0,    0,   0,   0,   0,   0,    0, // 0x40 - 0x47
	  0,   0,    0,   0,   0,   0,   0,    0, // 0x48 - 0x4F
	  0,   0,    0,   0,   0,   0,   0,    0, // 0x50 - 0x57
	  0,   0,    0,   0,   0,   0,   0,    0, // 0x58 - 0x5F
	  0,   0,    0,   0,   0,   0,   0,    0, // 0x60 - 0x67
	  0,   0,    0,   0,   0,   0,   0,    0, // 0x68 - 0x6F
	  0,   0,    0,   0,   0,   0,   0,    0, // 0x70 - 0x77
	  0,   0,    0,   0,   0,   0,   0,    0, // 0x78 - 0x7F
};


uint8_t ModifierToDIK(const uint32_t modifier)
{
	switch (modifier)
	{
		case NSAlphaShiftKeyMask: return DIK_CAPITAL;
		case NSShiftKeyMask:      return DIK_LSHIFT;
		case NSControlKeyMask:    return DIK_LCONTROL;
		case NSAlternateKeyMask:  return DIK_LMENU;
		case NSCommandKeyMask:    return DIK_LWIN;
	}

	return 0;
}

SWORD ModifierFlagsToGUIKeyModifiers(NSEvent* theEvent)
{
	const NSUInteger modifiers([theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask);
	return ((modifiers & NSShiftKeyMask    ) ? GKM_SHIFT : 0)
		 | ((modifiers & NSControlKeyMask  ) ? GKM_CTRL  : 0)
		 | ((modifiers & NSAlternateKeyMask) ? GKM_ALT   : 0)
		 | ((modifiers & NSCommandKeyMask  ) ? GKM_META  : 0);
}

bool ShouldGenerateGUICharEvent(NSEvent* theEvent)
{
	const NSUInteger modifiers([theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask);
	return !(modifiers & NSControlKeyMask)
		&& !(modifiers & NSAlternateKeyMask)
		&& !(modifiers & NSCommandKeyMask)
		&& !(modifiers & NSFunctionKeyMask);
}


NSStringEncoding GetEncodingForUnicodeCharacter(const unichar character)
{
	if (character >= L'\u0100' && character <= L'\u024F')
	{
		return NSWindowsCP1250StringEncoding; // Central and Eastern Europe
	}
	else if (character >= L'\u0370' && character <= L'\u03FF')
	{
		return NSWindowsCP1253StringEncoding; // Greek
	}
	else if (character >= L'\u0400' && character <= L'\u04FF')
	{
		return NSWindowsCP1251StringEncoding; // Cyrillic
	}

	// TODO: add handling for other characters
	// TODO: Turkish should use NSWindowsCP1254StringEncoding

	return NSWindowsCP1252StringEncoding;
}

unsigned char GetCharacterFromNSEvent(NSEvent* theEvent)
{
	const NSString* unicodeCharacters = [theEvent characters];

	if (0 == [unicodeCharacters length])
	{
		return '\0';
	}

	const unichar unicodeCharacter = [unicodeCharacters characterAtIndex:0];
	const NSStringEncoding encoding = GetEncodingForUnicodeCharacter(unicodeCharacter);

	unsigned char character = '\0';

	if (NSWindowsCP1252StringEncoding == encoding)
	{
		// TODO: make sure that the following is always correct
		character = unicodeCharacter & 0xFF;
	}
	else
	{
		const NSData* const characters =
			[[theEvent characters] dataUsingEncoding:encoding];

		character = [characters length] > 0
			? *static_cast<const unsigned char*>([characters bytes])
			: '\0';
	}

	return character;
}

void ProcessKeyboardEventInMenu(NSEvent* theEvent)
{
	event_t event = {};

	event.type    = EV_GUI_Event;
	event.subtype = NSKeyDown == [theEvent type] ? EV_GUI_KeyDown : EV_GUI_KeyUp;
	event.data2   = GetCharacterFromNSEvent(theEvent);
	event.data3   = ModifierFlagsToGUIKeyModifiers(theEvent);

	if (EV_GUI_KeyDown == event.subtype && [theEvent isARepeat])
	{
		event.subtype = EV_GUI_KeyRepeat;
	}

	const unsigned short keyCode = [theEvent keyCode];

	switch (keyCode)
	{
		case kVK_Return:        event.data1 = GK_RETURN;    break;
		case kVK_PageUp:        event.data1 = GK_PGUP;      break;
		case kVK_PageDown:      event.data1 = GK_PGDN;      break;
		case kVK_End:           event.data1 = GK_END;       break;
		case kVK_Home:          event.data1 = GK_HOME;      break;
		case kVK_LeftArrow:     event.data1 = GK_LEFT;      break;
		case kVK_RightArrow:    event.data1 = GK_RIGHT;     break;
		case kVK_UpArrow:       event.data1 = GK_UP;        break;
		case kVK_DownArrow:     event.data1 = GK_DOWN;      break;
		case kVK_Delete:        event.data1 = GK_BACKSPACE; break;
		case kVK_ForwardDelete: event.data1 = GK_DEL;       break;
		case kVK_Escape:        event.data1 = GK_ESCAPE;    break;
		case kVK_F1:            event.data1 = GK_F1;        break;
		case kVK_F2:            event.data1 = GK_F2;        break;
		case kVK_F3:            event.data1 = GK_F3;        break;
		case kVK_F4:            event.data1 = GK_F4;        break;
		case kVK_F5:            event.data1 = GK_F5;        break;
		case kVK_F6:            event.data1 = GK_F6;        break;
		case kVK_F7:            event.data1 = GK_F7;        break;
		case kVK_F8:            event.data1 = GK_F8;        break;
		case kVK_F9:            event.data1 = GK_F9;        break;
		case kVK_F10:           event.data1 = GK_F10;       break;
		case kVK_F11:           event.data1 = GK_F11;       break;
		case kVK_F12:           event.data1 = GK_F12;       break;
		default:
			event.data1 = KEYCODE_TO_ASCII[keyCode];
			break;
	}

	if (event.data1 < 128)
	{
		event.data1 = toupper(event.data1);

		D_PostEvent(&event);
	}

	if (!iscntrl(event.data2)
		&& EV_GUI_KeyUp != event.subtype
		&& ShouldGenerateGUICharEvent(theEvent))
	{
		event.subtype = EV_GUI_Char;
		event.data1   = event.data2;
		event.data2   = event.data3 & GKM_ALT;
		
		D_PostEvent(&event);
	}
}


void NSEventToGameMousePosition(NSEvent* inEvent, event_t* outEvent)
{
	const NSWindow* window = [inEvent window];
	const NSView*     view = [window contentView];

	const NSPoint screenPos = [NSEvent mouseLocation];
	const NSPoint windowPos = [window convertScreenToBase:screenPos];

	const NSPoint   viewPos = I_IsHiDPISupported()
		? [view convertPointToBacking:windowPos]
		: [view convertPoint:windowPos fromView:nil];

	const CGFloat frameHeight = I_GetContentViewSize(window).height;

	const CGFloat posX = (              viewPos.x - rbOpts.shiftX) / rbOpts.pixelScale;
	const CGFloat posY = (frameHeight - viewPos.y - rbOpts.shiftY) / rbOpts.pixelScale;

	outEvent->data1 = static_cast<int>(posX);
	outEvent->data2 = static_cast<int>(posY);
}

void ProcessMouseMoveInMenu(NSEvent* theEvent)
{
	event_t event = {};

	event.type    = EV_GUI_Event;
	event.subtype = EV_GUI_MouseMove;

	NSEventToGameMousePosition(theEvent, &event);

	D_PostEvent(&event);
}

void ProcessMouseMoveInGame(NSEvent* theEvent)
{
	if (!use_mouse)
	{
		return;
	}

	// TODO: remove this magic!

	if (s_skipMouseMoves > 0)
	{
		--s_skipMouseMoves;
		return;
	}

	int x([theEvent deltaX]);
	int y(-[theEvent deltaY]);

	if (0 == x && 0 == y)
	{
		return;
	}

	if (!m_noprescale)
	{
		x *= 3;
		y *= 2;
	}

	event_t event = {};

	static int lastX = 0, lastY = 0;

	if (m_filter)
	{
		event.x = (x + lastX) / 2;
		event.y = (y + lastY) / 2;
	}
	else
	{
		event.x = x;
		event.y = y;
	}

	lastX = x;
	lastY = y;

	if (0 != event.x | 0 != event.y)
	{
		event.type = EV_Mouse;
		
		D_PostEvent(&event);
	}
}


void ProcessKeyboardEvent(NSEvent* theEvent)
{
	const unsigned short keyCode = [theEvent keyCode];
	if (keyCode >= KEY_COUNT)
	{
		assert(!"Unknown keycode");
		return;
	}

	if (k_allowfullscreentoggle
		&& (kVK_ANSI_F == keyCode)
		&& (NSCommandKeyMask & [theEvent modifierFlags])
		&& (NSKeyDown == [theEvent type])
		&& ![theEvent isARepeat])
	{
		ToggleFullscreen = !ToggleFullscreen;
		return;
	}

	if (GUICapture)
	{
		ProcessKeyboardEventInMenu(theEvent);
	}
	else
	{
		event_t event = {};

		event.type  = NSKeyDown == [theEvent type] ? EV_KeyDown : EV_KeyUp;
		event.data1 = KEYCODE_TO_DIK[ keyCode ];

		if (0 != event.data1)
		{
			event.data2 = KEYCODE_TO_ASCII[ keyCode ];

			D_PostEvent(&event);
		}
	}
}

void ProcessKeyboardFlagsEvent(NSEvent* theEvent)
{
	static const uint32_t FLAGS_MASK =
		NSDeviceIndependentModifierFlagsMask & ~NSNumericPadKeyMask;

	const  uint32_t      modifiers = [theEvent modifierFlags] & FLAGS_MASK;
	static uint32_t   oldModifiers = 0;
	const  uint32_t deltaModifiers = modifiers ^ oldModifiers;

	if (0 == deltaModifiers)
	{
		return;
	}

	event_t event = {};

	event.type  = modifiers > oldModifiers ? EV_KeyDown : EV_KeyUp;
	event.data1 = ModifierToDIK(deltaModifiers);

	oldModifiers = modifiers;

	// Caps Lock is a modifier key which generates one event per state change
	// but not per actual key press or release. So treat any event as key down
	// Also its event should be not be posted in menu and console

	if (DIK_CAPITAL == event.data1)
	{
		if (GUICapture)
		{
			return;
		}

		event.type = EV_KeyDown;
	}

	D_PostEvent(&event);
}


void ProcessMouseMoveEvent(NSEvent* theEvent)
{
	if (GUICapture)
	{
		ProcessMouseMoveInMenu(theEvent);
	}
	else
	{
		ProcessMouseMoveInGame(theEvent);
	}
}

void ProcessMouseButtonEvent(NSEvent* theEvent)
{
	event_t event = {};

	const NSEventType cocoaEventType = [theEvent type];

	if (GUICapture)
	{
		event.type = EV_GUI_Event;

		switch (cocoaEventType)
		{
			case NSLeftMouseDown:  event.subtype = EV_GUI_LButtonDown; break;
			case NSRightMouseDown: event.subtype = EV_GUI_RButtonDown; break;
			case NSOtherMouseDown: event.subtype = EV_GUI_MButtonDown; break;
			case NSLeftMouseUp:    event.subtype = EV_GUI_LButtonUp;   break;
			case NSRightMouseUp:   event.subtype = EV_GUI_RButtonUp;   break;
			case NSOtherMouseUp:   event.subtype = EV_GUI_MButtonUp;   break;
			default: break;
		}

		NSEventToGameMousePosition(theEvent, &event);

		D_PostEvent(&event);
	}
	else
	{
		switch (cocoaEventType)
		{
			case NSLeftMouseDown:
			case NSRightMouseDown:
			case NSOtherMouseDown:
				event.type = EV_KeyDown;
				break;

			case NSLeftMouseUp:
			case NSRightMouseUp:
			case NSOtherMouseUp:
				event.type = EV_KeyUp;
				break;

			default:
				break;
		}

		event.data1 = MIN(KEY_MOUSE1 + [theEvent buttonNumber], NSInteger(KEY_MOUSE8));

		D_PostEvent(&event);
	}
}

void ProcessMouseWheelEvent(NSEvent* theEvent)
{
	const CGFloat delta    = [theEvent deltaY];
	const bool isZeroDelta = fabs(delta) < 1.0E-5;

	if (isZeroDelta && GUICapture)
	{
		return;
	}
	
	event_t event = {};
	
	if (GUICapture)
	{
		event.type    = EV_GUI_Event;
		event.subtype = delta > 0.0f ? EV_GUI_WheelUp : EV_GUI_WheelDown;
		event.data3   = delta;
		event.data3   = ModifierFlagsToGUIKeyModifiers(theEvent);
	}
	else
	{
		event.type  = isZeroDelta  ? EV_KeyUp     : EV_KeyDown;
		event.data1 = delta > 0.0f ? KEY_MWHEELUP : KEY_MWHEELDOWN;
	}
	
	D_PostEvent(&event);
}

} // unnamed namespace


void I_ProcessEvent(NSEvent* event)
{
	const NSEventType eventType = [event type];

	switch (eventType)
	{
		case NSMouseMoved:
			ProcessMouseMoveEvent(event);
			break;

		case NSLeftMouseDown:
		case NSLeftMouseUp:
		case NSRightMouseDown:
		case NSRightMouseUp:
		case NSOtherMouseDown:
		case NSOtherMouseUp:
			ProcessMouseButtonEvent(event);
			break;

		case NSLeftMouseDragged:
		case NSRightMouseDragged:
		case NSOtherMouseDragged:
			ProcessMouseButtonEvent(event);
			ProcessMouseMoveEvent(event);
			break;

		case NSScrollWheel:
			ProcessMouseWheelEvent(event);
			break;

		case NSKeyDown:
		case NSKeyUp:
			ProcessKeyboardEvent(event);
			break;

		case NSFlagsChanged:
			ProcessKeyboardFlagsEvent(event);
			break;

		default:
			break;
	}
}
