/*
 ** i_backend_cocoa.mm
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2014 Alexey Lysiuk
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

#include <algorithm>
#include <vector>

#include <sys/time.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include <dlfcn.h>

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <OpenGL/gl.h>

#include <SDL.h>

// Avoid collision between DObject class and Objective-C
#define Class ObjectClass

#include "basictypes.h"
#include "basicinlines.h"
#include "bitmap.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "d_event.h"
#include "d_gui.h"
#include "dikeys.h"
#include "doomdef.h"
#include "doomstat.h"
#include "s_sound.h"
#include "textures.h"
#include "v_video.h"
#include "version.h"
#include "i_rbopts.h"

#undef Class


#define ZD_UNUSED(VARIABLE) ((void)(VARIABLE))


// ---------------------------------------------------------------------------


#ifndef NSAppKitVersionNumber10_7

@interface NSView(HiDPIStubs)
- (NSPoint)convertPointToBacking:(NSPoint)aPoint;
- (NSSize)convertSizeToBacking:(NSSize)aSize;
- (NSSize)convertSizeFromBacking:(NSSize)aSize;

- (void)setWantsBestResolutionOpenGLSurface:(BOOL)flag;
@end

@interface NSScreen(HiDPIStubs)
- (NSRect)convertRectToBacking:(NSRect)aRect;
@end

#endif // NSAppKitVersionNumber10_7


// ---------------------------------------------------------------------------

RenderBufferOptions rbOpts;

CVAR(Bool, use_mouse,    true,  CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, m_noprescale, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, m_filter,     false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

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

bool GUICapture;


extern int paused, chatmodeon;
extern constate_e ConsoleState;

EXTERN_CVAR(Int, m_use_mouse);

void I_ShutdownJoysticks();


namespace
{

const int ARGC_MAX = 64;

int   s_argc;
char* s_argv[ARGC_MAX];

TArray<FString> s_argvStorage;

bool s_restartedFromWADPicker;


bool s_nativeMouse = true;
	
// TODO: remove this magic!
size_t s_skipMouseMoves;

NSCursor* s_cursor;


void CheckGUICapture()
{
	const bool wantCapture = (MENU_Off == menuactive)
		? (c_down == ConsoleState || c_falling == ConsoleState || chatmodeon)
		: (menuactive == MENU_On || menuactive == MENU_OnNoPause);
	
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
			return gamestate == GS_LEVEL || gamestate == GS_INTERMISSION || gamestate == GS_FINALE;
			
		case 2:
			return true;
	}
}

void SetNativeMouse(bool wantNative)
{
	if (wantNative != s_nativeMouse)
	{
		s_nativeMouse = wantNative;
		
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

void CheckNativeMouse()
{
	bool windowed = (NULL == screen) || !screen->IsFullscreen();
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
	
	SetNativeMouse(wantNative);
}

} // unnamed namespace


// from iokit_joystick.cpp
void I_ProcessJoysticks();


void I_GetEvent()
{
	[[NSRunLoop mainRunLoop] limitDateForMode:NSDefaultRunLoopMode];
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

void ProcessKeyboardEvent(NSEvent* theEvent)
{
	const unsigned short keyCode = [theEvent keyCode];
	if (keyCode >= KEY_COUNT)
	{
		assert(!"Unknown keycode");
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


bool IsHiDPISupported()
{
#ifdef NSAppKitVersionNumber10_7
	return NSAppKitVersionNumber >= NSAppKitVersionNumber10_7;
#else // !NSAppKitVersionNumber10_7
	return false;
#endif // NSAppKitVersionNumber10_7
}

NSSize GetRealContentViewSize(const NSWindow* const window)
{
	const NSView* view = [window contentView];
	const NSSize frameSize = [view frame].size;

	// TODO: figure out why [NSView frame] returns different values in "fullscreen" and in window
	// In "fullscreen" the result is multiplied by [NSScreen backingScaleFactor], but not in window

	return (IsHiDPISupported() && NSNormalWindowLevel == [window level])
		? [view convertSizeToBacking:frameSize]
		: frameSize;
}


void NSEventToGameMousePosition(NSEvent* inEvent, event_t* outEvent)
{
	const NSWindow* window = [inEvent window];
	const NSView*     view = [window contentView];
	
	const NSPoint screenPos = [NSEvent mouseLocation];
	const NSPoint windowPos = [window convertScreenToBase:screenPos];

	const NSPoint   viewPos = IsHiDPISupported()
		? [view convertPointToBacking:windowPos]
		: [view convertPointFromBase:windowPos];

	const CGFloat frameHeight = GetRealContentViewSize(window).height;

	const CGFloat posX = (              viewPos.x - rbOpts.shiftX) / rbOpts.pixelScale;
	const CGFloat posY = (frameHeight - viewPos.y - rbOpts.shiftY) / rbOpts.pixelScale;

	outEvent->data1 = static_cast< int >(posX);
	outEvent->data2 = static_cast< int >(posY);
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
		}
		
		event.data1 = std::min(KEY_MOUSE1 + [theEvent buttonNumber], NSInteger(KEY_MOUSE8));
		
		D_PostEvent(&event);
	}
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


// ---------------------------------------------------------------------------


@interface FullscreenWindow : NSWindow
{

}

- (bool)canBecomeKeyWindow;

- (void)close;

@end


@implementation FullscreenWindow

- (bool)canBecomeKeyWindow
{
	return true;
}


- (void)close
{
	[super close];
	
	I_ShutdownJoysticks();

	[NSApp terminate:self];
}

@end


// ---------------------------------------------------------------------------


@interface FullscreenView : NSOpenGLView
{

}

- (void)resetCursorRects;

@end


@implementation FullscreenView

- (void)resetCursorRects
{
	[super resetCursorRects];
    [self addCursorRect: [self bounds]
				 cursor: s_cursor];
}

@end


// ---------------------------------------------------------------------------


@interface ApplicationDelegate : NSResponder
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
	<NSFileManagerDelegate>
#endif
{
@private
    FullscreenWindow* m_window;
	
	bool m_openGLInitialized;
	int  m_multisample;
}

- (id)init;
- (void)dealloc;

- (void)keyDown:(NSEvent*)theEvent;
- (void)keyUp:(NSEvent*)theEvent;

- (void)applicationDidBecomeActive:(NSNotification*)aNotification;
- (void)applicationWillResignActive:(NSNotification*)aNotification;

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification;

- (void)applicationWillTerminate:(NSNotification*)aNotification;

- (BOOL)application:(NSApplication*)theApplication openFile:(NSString*)filename;

- (int)multisample;
- (void)setMultisample:(int)multisample;

- (void)changeVideoResolution:(bool)fullscreen width:(int)width height:(int)height;

- (void)processEvents:(NSTimer*)timer;

- (void)invalidateCursorRects;

- (void)setMainWindowVisible:(bool)visible;

@end


static ApplicationDelegate* s_applicationDelegate;


@implementation ApplicationDelegate

- (id)init
{
	self = [super init];
	
	m_openGLInitialized = false;
	m_multisample       = 0;
	
	return self;
}

- (void)dealloc
{
	[m_window release];
	
	[super dealloc];
}


- (void)keyDown:(NSEvent*)theEvent
{
	// Empty but present to avoid playing of 'beep' alert sound
	
	ZD_UNUSED(theEvent);
}

- (void)keyUp:(NSEvent*)theEvent
{
	// Empty but present to avoid playing of 'beep' alert sound
	
	ZD_UNUSED(theEvent);
}


- (void)applicationDidBecomeActive:(NSNotification*)aNotification
{
	ZD_UNUSED(aNotification);
	
	S_SetSoundPaused(1);
}

- (void)applicationWillResignActive:(NSNotification*)aNotification
{
	ZD_UNUSED(aNotification);
	
	S_SetSoundPaused(0);
}


- (void)applicationDidFinishLaunching:(NSNotification*)aNotification
{
	// When starting from command line with real executable path, e.g. ZDoom.app/Contents/MacOS/ZDoom
	// application remains deactivated for an unknown reason.
	// The following call resolves this issue
	[NSApp activateIgnoringOtherApps:YES];

	// Setup timer for custom event loop

	NSTimer* timer = [NSTimer timerWithTimeInterval:0
											 target:self
										   selector:@selector(processEvents:)
										   userInfo:nil
											repeats:YES];
	[[NSRunLoop mainRunLoop] addTimer:timer
							  forMode:NSDefaultRunLoopMode];

	exit(SDL_main(s_argc, s_argv));
}


- (BOOL)application:(NSApplication*)theApplication openFile:(NSString*)filename
{
	ZD_UNUSED(theApplication);

	if (s_restartedFromWADPicker
		|| 0 == [filename length]
		|| s_argc + 2 >= ARGC_MAX)
	{
		return FALSE;
	}

	// Some parameters from command line are passed to this function
	// These parameters need to be skipped to avoid duplication
	// Note: SDL has different approach to fix this issue, see the same method in SDLMain.m

	const char* const charFileName = [filename UTF8String];

	for (int i = 0; i < s_argc; ++i)
	{
		if (0 == strcmp(s_argv[i], charFileName))
		{
			return FALSE;
		}
	}

	s_argvStorage.Push("-file");
	s_argv[s_argc++] = s_argvStorage.Last().LockBuffer();

	s_argvStorage.Push([filename UTF8String]);
	s_argv[s_argc++] = s_argvStorage.Last().LockBuffer();

	return TRUE;
}


- (void)applicationWillTerminate:(NSNotification*)aNotification
{
	ZD_UNUSED(aNotification);
	
	// Hide window as nothing will be rendered at this point
	[m_window orderOut:nil];
}


- (int)multisample
{
	return m_multisample;
}

- (void)setMultisample:(int)multisample
{
	m_multisample = multisample;
}


- (void)initializeOpenGL
{
	if (m_openGLInitialized)
	{
		return;
	}
	
	// Create window
	
	m_window = [[FullscreenWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 480)
												   styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask
													 backing:NSBackingStoreBuffered
													   defer:NO];
	[m_window setOpaque:YES];
	[m_window makeFirstResponder:self];
	[m_window setAcceptsMouseMovedEvents:YES];
	
	// Create OpenGL context and view
	
	NSOpenGLPixelFormatAttribute attributes[16];
	size_t i = 0;
	
	attributes[i++] = NSOpenGLPFADoubleBuffer;
	attributes[i++] = NSOpenGLPFAColorSize;
	attributes[i++] = 32;
	attributes[i++] = NSOpenGLPFADepthSize;
	attributes[i++] = 24;
	attributes[i++] = NSOpenGLPFAStencilSize;
	attributes[i++] = 8;
	
	if (m_multisample)
	{
		attributes[i++] = NSOpenGLPFAMultisample;
		attributes[i++] = NSOpenGLPFASampleBuffers;
		attributes[i++] = 1;
		attributes[i++] = NSOpenGLPFASamples;
		attributes[i++] = m_multisample;
	}	
	
	attributes[i] = 0;
	
	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	
	const NSRect contentRect = [m_window contentRectForFrameRect:[m_window frame]];
	NSOpenGLView* glView = [[FullscreenView alloc] initWithFrame:contentRect
													 pixelFormat:pixelFormat];
	[[glView openGLContext] makeCurrentContext];

	if (IsHiDPISupported())
	{
		[glView setWantsBestResolutionOpenGLSurface:YES];
	}
	
	[m_window setContentView:glView];
	
	m_openGLInitialized = true;
}

- (void)fullscreenWithWidth:(int)width height:(int)height
{
	NSScreen* screen = [m_window screen];

	const NSRect screenFrame = [screen frame];
	const NSRect displayRect = IsHiDPISupported()
		? [screen convertRectToBacking:screenFrame]
		: screenFrame;

	const float  displayWidth  = displayRect.size.width;
	const float  displayHeight = displayRect.size.height;
	
	const float pixelScaleFactorX = displayWidth  / static_cast< float >(width );
	const float pixelScaleFactorY = displayHeight / static_cast< float >(height);
	
	rbOpts.pixelScale = std::min(pixelScaleFactorX, pixelScaleFactorY);
	
	rbOpts.width  = width  * rbOpts.pixelScale;
	rbOpts.height = height * rbOpts.pixelScale;
	
	rbOpts.shiftX = (displayWidth  - rbOpts.width ) / 2.0f;
	rbOpts.shiftY = (displayHeight - rbOpts.height) / 2.0f;

	[m_window setLevel:NSMainMenuWindowLevel + 1];
	[m_window setStyleMask:NSBorderlessWindowMask];
	[m_window setHidesOnDeactivate:YES];
	[m_window setFrame:displayRect display:YES];
	[m_window setFrameOrigin:NSMakePoint(0.0f, 0.0f)];
}

- (void)windowedWithWidth:(int)width height:(int)height
{
	rbOpts.pixelScale = 1.0f;
	
	rbOpts.width  = static_cast< float >(width );
	rbOpts.height = static_cast< float >(height);
	
	rbOpts.shiftX = 0.0f;
	rbOpts.shiftY = 0.0f;

	const NSSize windowPixelSize = NSMakeSize(width, height);
	const NSSize windowSize = IsHiDPISupported()
		? [[m_window contentView] convertSizeFromBacking:windowPixelSize]
		: windowPixelSize;

	[m_window setLevel:NSNormalWindowLevel];
	[m_window setStyleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask];
	[m_window setHidesOnDeactivate:NO];
	[m_window setContentSize:windowSize];
	[m_window center];
}

- (void)changeVideoResolution:(bool)fullscreen width:(int)width height:(int)height
{
	[self initializeOpenGL];
	
	if (fullscreen)
	{
		[self fullscreenWithWidth:width height:height];
	}
	else
	{
		[self windowedWithWidth:width height:height];
	}

	const NSSize viewSize = GetRealContentViewSize(m_window);
	
	glViewport(0, 0, viewSize.width, viewSize.height);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	CGLFlushDrawable(CGLGetCurrentContext());

	static NSString* const TITLE_STRING = 
		[NSString stringWithFormat:@"%s %s", GAMESIG, GetVersionString()];
	[m_window setTitle:TITLE_STRING];		
	
	if (![m_window isKeyWindow])
	{
		[m_window makeKeyAndOrderFront:nil];
	}	
}


- (void)processEvents:(NSTimer*)timer
{
	ZD_UNUSED(timer);
	
    while (true)
    {
        NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask
											untilDate:[NSDate dateWithTimeIntervalSinceNow:0]
											   inMode:NSDefaultRunLoopMode
											  dequeue:YES];
        if (nil == event)
        {
            break;
        }
		
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
		}
		
		[NSApp sendEvent:event];
	}
    
    [NSApp updateWindows];
}


- (void)invalidateCursorRects
{
	[m_window invalidateCursorRectsForView:[m_window contentView]];
}


- (void)setMainWindowVisible:(bool)visible
{
	if (visible)
	{
		[m_window orderFront:nil];
	}
	else
	{
		[m_window orderOut:nil];
	}
}

@end


// ---------------------------------------------------------------------------


void I_SetMainWindowVisible(bool visible)
{
	[s_applicationDelegate setMainWindowVisible:visible];
	
	SetNativeMouse(!visible);
}


// ---------------------------------------------------------------------------


bool I_SetCursor(FTexture* cursorpic)
{
	if (NULL == cursorpic || FTexture::TEX_Null == cursorpic->UseType)
	{
		s_cursor = [NSCursor arrowCursor];
	}
	else
	{
		// Create bitmap image representation
		
		const NSInteger imageWidth  = cursorpic->GetWidth();
		const NSInteger imageHeight = cursorpic->GetHeight();
		const NSInteger imagePitch  = imageWidth * 4;
		
		NSBitmapImageRep* bitmapImageRep = [NSBitmapImageRep alloc];
		[bitmapImageRep initWithBitmapDataPlanes:NULL
									  pixelsWide:imageWidth
									  pixelsHigh:imageHeight
								   bitsPerSample:8
								 samplesPerPixel:4
										hasAlpha:YES
										isPlanar:NO
								  colorSpaceName:NSDeviceRGBColorSpace
									 bytesPerRow:imagePitch
									bitsPerPixel:0];
		
		// Load bitmap data to representation
		
		BYTE* buffer = [bitmapImageRep bitmapData];
		FBitmap bitmap(buffer, imagePitch, imageWidth, imageHeight);
		cursorpic->CopyTrueColorPixels(&bitmap, 0, 0);
		
		// Swap red and blue components in each pixel
		
		for (size_t i = 0; i < size_t(imageWidth * imageHeight); ++i)
		{
			const size_t offset = i * 4;
			
			const BYTE temp    = buffer[offset    ];
			buffer[offset    ] = buffer[offset + 2];
			buffer[offset + 2] = temp;
		}
		
		// Create image from representation and set it as cursor
		
		NSData* imageData = [bitmapImageRep representationUsingType:NSPNGFileType
														 properties:nil];		
		NSImage* cursorImage = [[NSImage alloc] initWithData:imageData];
		
		s_cursor = [[NSCursor alloc] initWithImage:cursorImage
										   hotSpot:NSMakePoint(0.0f, 0.0f)];
	}
	
	[s_applicationDelegate invalidateCursorRects];
	
	return true;
}


// ---------------------------------------------------------------------------


const char* I_GetBackEndName()
{
	return "Native Cocoa";
}


// ---------------------------------------------------------------------------


extern "C" 
{

struct SDL_mutex
{
	pthread_mutex_t mutex;
};


SDL_mutex* SDL_CreateMutex()
{
	pthread_mutexattr_t attributes;
	pthread_mutexattr_init(&attributes);
	pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
	
	SDL_mutex* result = new SDL_mutex;
	
	if (0 != pthread_mutex_init(&result->mutex, &attributes))
	{
		delete result;
		result = NULL;
	}
	
	pthread_mutexattr_destroy(&attributes);
	
	return result;
}

int SDL_mutexP(SDL_mutex* mutex)
{
	return pthread_mutex_lock(&mutex->mutex);
}

int SDL_mutexV(SDL_mutex* mutex)
{
	return pthread_mutex_unlock(&mutex->mutex);
}

void SDL_DestroyMutex(SDL_mutex* mutex)
{
	pthread_mutex_destroy(&mutex->mutex);
	delete mutex;
}


static timeval s_startTicks;

uint32_t SDL_GetTicks()
{
	timeval now;
	gettimeofday(&now, NULL);
	
	const uint32_t ticks = 
		  (now.tv_sec  - s_startTicks.tv_sec ) * 1000
		+ (now.tv_usec - s_startTicks.tv_usec) / 1000;
	
	return ticks;
}


int SDL_Init(Uint32 flags)
{
	ZD_UNUSED(flags);

	return 0;
}

void SDL_Quit()
{
	if (NULL != s_applicationDelegate)
	{
		[NSApp setDelegate:nil];
		[NSApp deactivate];

		[s_applicationDelegate release];
		s_applicationDelegate = NULL;
	}
}


char* SDL_GetError()
{
	static char empty[] = {0};
	return empty;
}


char* SDL_VideoDriverName(char* namebuf, int maxlen)
{
	return strncpy(namebuf, "Native OpenGL", maxlen);
}

const SDL_VideoInfo* SDL_GetVideoInfo()
{
	// NOTE: Only required fields are assigned
	
	static SDL_PixelFormat pixelFormat;
	memset(&pixelFormat, 0, sizeof(pixelFormat));
	
	pixelFormat.BitsPerPixel = 32;
	
	static SDL_VideoInfo videoInfo;
	memset(&videoInfo, 0, sizeof(videoInfo));
	
	const NSRect displayRect = [[NSScreen mainScreen] frame];
	
	videoInfo.current_w = displayRect.size.width;
	videoInfo.current_h = displayRect.size.height;
	videoInfo.vfmt      = &pixelFormat;
	
	return &videoInfo;
}

SDL_Rect** SDL_ListModes(SDL_PixelFormat* format, Uint32 flags)
{
	ZD_UNUSED(format);
	ZD_UNUSED(flags);
	
	static std::vector< SDL_Rect* > resolutions;
	
	if (resolutions.empty())
	{
#define DEFINE_RESOLUTION(WIDTH, HEIGHT)                                   \
	static SDL_Rect resolution_##WIDTH##_##HEIGHT = { 0, 0, WIDTH, HEIGHT }; \
	resolutions.push_back(&resolution_##WIDTH##_##HEIGHT);
		
		DEFINE_RESOLUTION(640,  480);
		DEFINE_RESOLUTION(720,  480);
		DEFINE_RESOLUTION(800,  600);
		DEFINE_RESOLUTION(1024,  640);
		DEFINE_RESOLUTION(1024,  768);
		DEFINE_RESOLUTION(1152,  720);
		DEFINE_RESOLUTION(1280,  720);
		DEFINE_RESOLUTION(1280,  800);
		DEFINE_RESOLUTION(1280,  960);
		DEFINE_RESOLUTION(1280, 1024);
		DEFINE_RESOLUTION(1366,  768);
		DEFINE_RESOLUTION(1400, 1050);
		DEFINE_RESOLUTION(1440,  900);
		DEFINE_RESOLUTION(1600,  900);
		DEFINE_RESOLUTION(1600, 1200);
		DEFINE_RESOLUTION(1680, 1050);
		DEFINE_RESOLUTION(1920, 1080);
		DEFINE_RESOLUTION(1920, 1200);
		DEFINE_RESOLUTION(2048, 1536);
		DEFINE_RESOLUTION(2560, 1440);
		DEFINE_RESOLUTION(2560, 1600);
		DEFINE_RESOLUTION(2880, 1800);
		
#undef DEFINE_RESOLUTION
		
		resolutions.push_back(NULL);
	}
	
	return &resolutions[0];
}

int SDL_ShowCursor(int)
{
	// Does nothing
	return 0;
}


static GLuint s_frameBufferTexture = 0;

static const Uint16 BYTES_PER_PIXEL = 4;

static SDL_PixelFormat* GetPixelFormat()
{
	static SDL_PixelFormat result;
	
	result.palette       = NULL;
	result.BitsPerPixel  = BYTES_PER_PIXEL * 8;
	result.BytesPerPixel = BYTES_PER_PIXEL;
	result.Rloss         = 0;
	result.Gloss         = 0;
	result.Bloss         = 0;
	result.Aloss         = 8;
	result.Rshift        = 8;
	result.Gshift        = 16;
	result.Bshift        = 24;
	result.Ashift        = 0;
	result.Rmask         = 0x000000FF;
	result.Gmask         = 0x0000FF00;
	result.Bmask         = 0x00FF0000;
	result.Amask         = 0xFF000000;	
	result.colorkey      = 0;
	result.alpha         = 0xFF;
	
	return &result;
}


SDL_Surface* SDL_SetVideoMode(int width, int height, int, Uint32 flags)
{
	[s_applicationDelegate changeVideoResolution:(SDL_FULLSCREEN & flags) width:width height:height];
	
	static SDL_Surface result;
	
	const bool isSoftwareRenderer = !(SDL_OPENGL & flags);
	
	if (isSoftwareRenderer)
	{
		if (NULL != result.pixels)
		{
			free(result.pixels);
		}
		
		if (0 != s_frameBufferTexture)
		{
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &s_frameBufferTexture);
			s_frameBufferTexture = 0;
		}
	}
	
	result.flags    = flags;
	result.format   = GetPixelFormat();
	result.w        = width;
	result.h        = height;
	result.pitch    = width * BYTES_PER_PIXEL;
	result.pixels   = isSoftwareRenderer ? malloc(width * height * BYTES_PER_PIXEL) : NULL;
	result.refcount = 1;
	
	result.clip_rect.x = 0;
	result.clip_rect.y = 0;
	result.clip_rect.w = width;
	result.clip_rect.h = height;
	
	return &result;
}


void SDL_WM_SetCaption(const char* title, const char* icon)
{
	ZD_UNUSED(title);
	ZD_UNUSED(icon);
	
	// Window title is set in SDL_SetVideoMode()
}

static void ResetSoftwareViewport()
{
	// For an unknown reason the following call to glClear() is needed
	// to avoid drawing of garbage in fullscreen mode
	// when game video resolution's aspect ratio is different from display one

	GLint viewport[2];
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport);

	glViewport(0, 0, viewport[0], viewport[1]);
	glClear(GL_COLOR_BUFFER_BIT);

	glViewport(rbOpts.shiftX, rbOpts.shiftY, rbOpts.width, rbOpts.height);
}

int SDL_WM_ToggleFullScreen(SDL_Surface* surface)
{
	if (surface->flags & SDL_FULLSCREEN)
	{
		surface->flags &= ~SDL_FULLSCREEN;
	}
	else
	{
		surface->flags |= SDL_FULLSCREEN;
	}

	[s_applicationDelegate changeVideoResolution:(SDL_FULLSCREEN & surface->flags)
										   width:surface->w
										  height:surface->h];
	ResetSoftwareViewport();

	return 1;
}


void SDL_GL_SwapBuffers()
{
	[[NSOpenGLContext currentContext] flushBuffer];
}

int SDL_GL_SetAttribute(SDL_GLattr attr, int value)
{
	if (SDL_GL_MULTISAMPLESAMPLES == attr)
	{
		[s_applicationDelegate setMultisample:value];
	}

	// Not interested in other attributes

	return 0;
}


int SDL_LockSurface(SDL_Surface* surface)
{
	ZD_UNUSED(surface);
	
	return 0;
}

void SDL_UnlockSurface(SDL_Surface* surface)
{
	ZD_UNUSED(surface);
}

int SDL_BlitSurface(SDL_Surface* src, SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect)
{
	ZD_UNUSED(src);
	ZD_UNUSED(srcrect);
	ZD_UNUSED(dst);
	ZD_UNUSED(dstrect);
	
	return 0;
}


static void SetupSoftwareRendering(SDL_Surface* screen)
{
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, screen->w, screen->h, 0.0, -1.0, 1.0);
	
	ResetSoftwareViewport();
	
	glEnable(GL_TEXTURE_2D);
	
	glGenTextures(1, &s_frameBufferTexture);
	glBindTexture(GL_TEXTURE_2D, s_frameBufferTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

int SDL_Flip(SDL_Surface* screen)
{
	assert(NULL != screen);
	
	if (0 == s_frameBufferTexture)
	{
		SetupSoftwareRendering(screen);
	}
	
	const int width  = screen->w;
	const int height = screen->h;
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
				 width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, screen->pixels);

	glBegin(GL_QUADS);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, 0.0f);
	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(width, 0.0f);
	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(width, height);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, height);
	glEnd();

	glFlush();
	
	SDL_GL_SwapBuffers();
	
	return 0;	
}

int SDL_SetPalette(SDL_Surface* surface, int flags, SDL_Color* colors, int firstcolor, int ncolors)
{
	ZD_UNUSED(surface);
	ZD_UNUSED(flags);
	ZD_UNUSED(colors);
	ZD_UNUSED(firstcolor);
	ZD_UNUSED(ncolors);
	
	return 0;
}
	
} // extern "C"

#ifdef main
#undef main
#endif // main

static void CheckOSVersion()
{
	static const char* const PARAMETER_NAME = "kern.osrelease";

	size_t size = 0;

    if (-1 == sysctlbyname(PARAMETER_NAME, NULL, &size, NULL, 0))
	{
		return;
	}

    char* version = static_cast<char* >(alloca(size));

    if (-1 == sysctlbyname(PARAMETER_NAME, version, &size, NULL, 0))
	{
		return;
	}

	if (strcmp(version, "10.0") < 0)
	{
		CFOptionFlags responseFlags;
		CFUserNotificationDisplayAlert(0, kCFUserNotificationStopAlertLevel, NULL, NULL, NULL,
			CFSTR("Unsupported version of OS X"), CFSTR("You need OS X 10.6 or higher running on Intel platform in order to play."),
			NULL, NULL, NULL, &responseFlags);

		exit(EXIT_FAILURE);
	}
}

int main(int argc, char** argv)
{
	CheckOSVersion();

	gettimeofday(&s_startTicks, NULL);

	for (int i = 0; i <= argc; ++i)
	{
		const char* const argument = argv[i];

		if (NULL == argument || '\0' == argument[0])
		{
			continue;
		}

		if (0 == strcmp(argument, "-wad_picker_restart"))
		{
			s_restartedFromWADPicker = true;
		}
		else
		{
			s_argvStorage.Push(argument);
			s_argv[s_argc++] = s_argvStorage.Last().LockBuffer();
		}
	}

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	[NSApplication sharedApplication];
	[NSBundle loadNibNamed:@"zdoom" owner:NSApp];

	s_applicationDelegate = [ApplicationDelegate new];
	[NSApp setDelegate:s_applicationDelegate];

	[NSApp run];

	[pool release];

	return EXIT_SUCCESS;
}
