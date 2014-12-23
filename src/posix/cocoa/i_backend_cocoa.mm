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
#include <dlfcn.h>

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

// Avoid collision between DObject class and Objective-C
#define Class ObjectClass

#include "bitmap.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "d_event.h"
#include "d_gui.h"
#include "d_main.h"
#include "dikeys.h"
#include "doomdef.h"
#include "doomerrors.h"
#include "doomstat.h"
#include "hardware.h"
#include "m_argv.h"
#include "r_renderer.h"
#include "r_swrenderer.h"
#include "s_sound.h"
#include "stats.h"
#include "textures.h"
#include "v_palette.h"
#include "v_pfx.h"
#include "v_text.h"
#include "v_video.h"
#include "version.h"
#include "i_rbopts.h"
#include "i_osversion.h"
#include "i_system.h"

#undef Class


#define ZD_UNUSED(VARIABLE) ((void)(VARIABLE))


// ---------------------------------------------------------------------------


// The following definitions are required to build with older OS X SDKs

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050

typedef unsigned int NSUInteger;
typedef          int NSInteger;

typedef float CGFloat;

// From HIToolbox/Events.h
enum 
{
	kVK_Return                    = 0x24,
	kVK_Tab                       = 0x30,
	kVK_Space                     = 0x31,
	kVK_Delete                    = 0x33,
	kVK_Escape                    = 0x35,
	kVK_Command                   = 0x37,
	kVK_Shift                     = 0x38,
	kVK_CapsLock                  = 0x39,
	kVK_Option                    = 0x3A,
	kVK_Control                   = 0x3B,
	kVK_RightShift                = 0x3C,
	kVK_RightOption               = 0x3D,
	kVK_RightControl              = 0x3E,
	kVK_Function                  = 0x3F,
	kVK_F17                       = 0x40,
	kVK_VolumeUp                  = 0x48,
	kVK_VolumeDown                = 0x49,
	kVK_Mute                      = 0x4A,
	kVK_F18                       = 0x4F,
	kVK_F19                       = 0x50,
	kVK_F20                       = 0x5A,
	kVK_F5                        = 0x60,
	kVK_F6                        = 0x61,
	kVK_F7                        = 0x62,
	kVK_F3                        = 0x63,
	kVK_F8                        = 0x64,
	kVK_F9                        = 0x65,
	kVK_F11                       = 0x67,
	kVK_F13                       = 0x69,
	kVK_F16                       = 0x6A,
	kVK_F14                       = 0x6B,
	kVK_F10                       = 0x6D,
	kVK_F12                       = 0x6F,
	kVK_F15                       = 0x71,
	kVK_Help                      = 0x72,
	kVK_Home                      = 0x73,
	kVK_PageUp                    = 0x74,
	kVK_ForwardDelete             = 0x75,
	kVK_F4                        = 0x76,
	kVK_End                       = 0x77,
	kVK_F2                        = 0x78,
	kVK_PageDown                  = 0x79,
	kVK_F1                        = 0x7A,
	kVK_LeftArrow                 = 0x7B,
	kVK_RightArrow                = 0x7C,
	kVK_DownArrow                 = 0x7D,
	kVK_UpArrow                   = 0x7E
};

@interface NSView(SupportOutdatedOSX)
- (NSPoint)convertPointFromBase:(NSPoint)aPoint;
@end

@implementation NSView(SupportOutdatedOSX)
- (NSPoint)convertPointFromBase:(NSPoint)aPoint
{
    return [self convertPoint:aPoint fromView:nil];
}
@end

#endif // prior to 10.5

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060

enum
{
	NSApplicationActivationPolicyRegular
};

typedef NSInteger NSApplicationActivationPolicy;

@interface NSApplication(ActivationPolicy)
- (BOOL)setActivationPolicy:(NSApplicationActivationPolicy)activationPolicy;
@end

@interface NSWindow(SetStyleMask)
- (void)setStyleMask:(NSUInteger)styleMask;
@end

#endif // prior to 10.6

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070

@interface NSView(HiDPIStubs)
- (NSPoint)convertPointToBacking:(NSPoint)aPoint;
- (NSSize)convertSizeToBacking:(NSSize)aSize;
- (NSSize)convertSizeFromBacking:(NSSize)aSize;

- (void)setWantsBestResolutionOpenGLSurface:(BOOL)flag;
@end

@interface NSScreen(HiDPIStubs)
- (NSRect)convertRectToBacking:(NSRect)aRect;
@end

#endif // prior to 10.7


// ---------------------------------------------------------------------------


RenderBufferOptions rbOpts;

EXTERN_CVAR(Bool, ticker       )
EXTERN_CVAR(Bool, vid_hidpi    )
EXTERN_CVAR(Bool, vid_vsync    )
EXTERN_CVAR(Int,  vid_defwidth )
EXTERN_CVAR(Int,  vid_defheight)

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

void I_StartupJoysticks();
void I_ShutdownJoysticks();


// ---------------------------------------------------------------------------


extern int NewWidth, NewHeight, NewBits, DisplayBits;


CUSTOM_CVAR(Bool, fullscreen, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	NewWidth = screen->GetWidth();
	NewHeight = screen->GetHeight();
	NewBits = DisplayBits;
	setmodeneeded = true;
}


// ---------------------------------------------------------------------------


DArgs *Args; // command line arguments

namespace
{

// The maximum number of functions that can be registered with atterm.
static const size_t MAX_TERMS = 64;

static void (*TermFuncs[MAX_TERMS])();
static const char *TermNames[MAX_TERMS];
static size_t NumTerms;

void call_terms()
{
	while (NumTerms > 0)
	{
		TermFuncs[--NumTerms]();
	}
}

} // unnamed namespace


void addterm(void (*func)(), const char *name)
{
	// Make sure this function wasn't already registered.

	for (size_t i = 0; i < NumTerms; ++i)
	{
		if (TermFuncs[i] == func)
		{
			return;
		}
	}

	if (NumTerms == MAX_TERMS)
	{
		func();
		I_FatalError("Too many exit functions registered.");
	}

	TermNames[NumTerms] = name;
	TermFuncs[NumTerms] = func;

	++NumTerms;
}

void popterm()
{
	if (NumTerms)
	{
		--NumTerms;
	}
}


void I_SetMainWindowVisible(bool);

void Mac_I_FatalError(const char* const message)
{
	I_SetMainWindowVisible(false);

	const CFStringRef errorString = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
		message, kCFStringEncodingASCII, kCFAllocatorNull);

	if (NULL != errorString)
	{
		CFOptionFlags dummy;

		CFUserNotificationDisplayAlert( 0, kCFUserNotificationStopAlertLevel, NULL, NULL, NULL,
			CFSTR("Fatal Error"), errorString, CFSTR("Exit"), NULL, NULL, &dummy);

		CFRelease(errorString);
	}
}


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


void NewFailure()
{
	I_FatalError("Failed to allocate memory from system heap");
}


int OriginalMain(int argc, char** argv)
{
	printf(GAMENAME" %s - %s - Cocoa version\nCompiled on %s\n\n",
		GetVersionString(), GetGitTime(), __DATE__);

	seteuid(getuid());
	std::set_new_handler(NewFailure);

	// Set LC_NUMERIC environment variable in case some library decides to
	// clear the setlocale call at least this will be correct.
	// Note that the LANG environment variable is overridden by LC_*
	setenv("LC_NUMERIC", "C", 1);
	setlocale(LC_ALL, "C");

	// Set reasonable default values for video settings
	const NSSize screenSize = [[NSScreen mainScreen] frame].size;
	vid_defwidth  = static_cast<int>(screenSize.width);
	vid_defheight = static_cast<int>(screenSize.height);
	vid_vsync     = true;
	fullscreen    = true;

	try
	{
		Args = new DArgs(argc, argv);

		/*
		 killough 1/98:

		 This fixes some problems with exit handling
		 during abnormal situations.

		 The old code called I_Quit() to end program,
		 while now I_Quit() is installed as an exit
		 handler and exit() is called to exit, either
		 normally or abnormally. Seg faults are caught
		 and the error handler is used, to prevent
		 being left in graphics mode or having very
		 loud SFX noise because the sound card is
		 left in an unstable state.
		 */

		atexit (call_terms);
		atterm (I_Quit);

		// Should we even be doing anything with progdir on Unix systems?
		char program[PATH_MAX];
		if (realpath (argv[0], program) == NULL)
			strcpy (program, argv[0]);
		char *slash = strrchr (program, '/');
		if (slash != NULL)
		{
			*(slash + 1) = '\0';
			progdir = program;
		}
		else
		{
			progdir = "./";
		}

		I_StartupJoysticks();
		atterm(I_ShutdownJoysticks);

		C_InitConsole(80 * 8, 25 * 8, false);
		D_DoomMain();
	}
	catch(const CDoomError& error)
	{
		const char* const message = error.GetMessage();

		if (NULL != message)
		{
			fprintf(stderr, "%s\n", message);
			Mac_I_FatalError(message);
		}

		exit(-1);
	}
	catch(...)
	{
		call_terms();
		throw;
	}

	return 0;
}


// ---------------------------------------------------------------------------


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


// see cocoa/i_joystick.cpp
void I_ProcessJoysticks();


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
	// The following value shoud be equal to NSAppKitVersionNumber10_7
	// and it's hard-coded in order to build on earlier SDKs
	return NSAppKitVersionNumber >= 1138; 
}

NSSize GetRealContentViewSize(const NSWindow* const window)
{
	const NSView* view = [window contentView];
	const NSSize frameSize = [view frame].size;

	// TODO: figure out why [NSView frame] returns different values in "fullscreen" and in window
	// In "fullscreen" the result is multiplied by [NSScreen backingScaleFactor], but not in window

	return (vid_hidpi && !fullscreen)
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
		: [view convertPoint:windowPos fromView:nil];

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


const size_t BYTES_PER_PIXEL = 4;

} // unnamed namespace


// ---------------------------------------------------------------------------


namespace
{
	const NSInteger LEVEL_FULLSCREEN = NSMainMenuWindowLevel + 1;
	const NSInteger LEVEL_WINDOWED   = NSNormalWindowLevel;

	const NSUInteger STYLE_MASK_FULLSCREEN = NSBorderlessWindowMask;
	const NSUInteger STYLE_MASK_WINDOWED   = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask;
}


// ---------------------------------------------------------------------------


@interface FullscreenWindow : NSWindow
{

}

- (bool)canBecomeKeyWindow;

- (void)setLevel:(NSInteger)level;
- (void)setStyleMask:(NSUInteger)styleMask;

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


@interface ApplicationController : NSResponder
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
	<NSFileManagerDelegate>
#endif
{
@private
	FullscreenWindow* m_window;

	uint8_t* m_softwareRenderingBuffer;
	GLuint   m_softwareRenderingTexture;

	int  m_multisample;

	int  m_width;
	int  m_height;
	bool m_fullscreen;
	bool m_hiDPI;

	bool m_openGLInitialized;
}

- (id)init;
- (void)dealloc;

- (void)keyDown:(NSEvent*)theEvent;
- (void)keyUp:(NSEvent*)theEvent;

- (void)applicationDidBecomeActive:(NSNotification*)aNotification;
- (void)applicationWillResignActive:(NSNotification*)aNotification;

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification;

- (BOOL)application:(NSApplication*)theApplication openFile:(NSString*)filename;

- (int)multisample;
- (void)setMultisample:(int)multisample;

- (void)changeVideoResolution:(bool)fullscreen width:(int)width height:(int)height useHiDPI:(bool)hiDPI;
- (void)useHiDPI:(bool)hiDPI;

- (void)setupSoftwareRenderingWithWidth:(int)width height:(int)height;
- (void*)softwareRenderingBuffer;

- (void)processEvents:(NSTimer*)timer;

- (void)invalidateCursorRects;

- (void)setMainWindowVisible:(bool)visible;

- (void)setWindowStyleMask:(NSUInteger)styleMask;

@end


static ApplicationController* appCtrl;


// ---------------------------------------------------------------------------


@implementation FullscreenWindow

static bool s_fullscreenNewAPI;

+ (void)initialize
{
	// The following value shoud be equal to NSAppKitVersionNumber10_6
	// and it's hard-coded in order to build on earlier SDKs
	s_fullscreenNewAPI = NSAppKitVersionNumber >= 1038;
}

- (bool)canBecomeKeyWindow
{
	return true;
}

- (void)setLevel:(NSInteger)level
{
	if (s_fullscreenNewAPI)
	{
		[super setLevel:level];
	}
	else
	{
		// Old Carbon-based way to make fullscreen window above dock and menu
		// It's supported on 64-bit, but on 10.6 and later the following is preferred:
		// [NSWindow setLevel:NSMainMenuWindowLevel + 1]

		const SystemUIMode mode = LEVEL_FULLSCREEN == level
			? kUIModeAllHidden
			: kUIModeNormal;
		SetSystemUIMode(mode, 0);
	}
}

- (void)setStyleMask:(NSUInteger)styleMask
{
	if (s_fullscreenNewAPI)
	{
		[super setStyleMask:styleMask];
	}
	else
	{
		[appCtrl setWindowStyleMask:styleMask];
	}
}

@end


// ---------------------------------------------------------------------------


@implementation ApplicationController

- (id)init
{
	self = [super init];

	m_window = nil;

	m_softwareRenderingBuffer  = NULL;
	m_softwareRenderingTexture = 0;

	m_multisample = 0;

	m_width      = -1;
	m_height     = -1;
	m_fullscreen = false;
	m_hiDPI      = false;

	m_openGLInitialized = false;
	
	return self;
}

- (void)dealloc
{
	delete[] m_softwareRenderingBuffer;

	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &m_softwareRenderingTexture);

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
	[[NSRunLoop currentRunLoop] addTimer:timer
								 forMode:NSDefaultRunLoopMode];

	exit(OriginalMain(s_argc, s_argv));
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


- (int)multisample
{
	return m_multisample;
}

- (void)setMultisample:(int)multisample
{
	m_multisample = multisample;
}


- (FullscreenWindow*)createWindow:(NSUInteger)styleMask
{
	FullscreenWindow* window = [[FullscreenWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 480)
																   styleMask:styleMask
																	 backing:NSBackingStoreBuffered
																	   defer:NO];
	[window setOpaque:YES];
	[window makeFirstResponder:self];
	[window setAcceptsMouseMovedEvents:YES];

	return window;
}

- (void)initializeOpenGL
{
	if (m_openGLInitialized)
	{
		return;
	}
	
	m_window = [self createWindow:STYLE_MASK_WINDOWED];
	
	// Create OpenGL context and view
	
	NSOpenGLPixelFormatAttribute attributes[16];
	size_t i = 0;
	
	attributes[i++] = NSOpenGLPFADoubleBuffer;
	attributes[i++] = NSOpenGLPFAColorSize;
	attributes[i++] = NSOpenGLPixelFormatAttribute(32);
	attributes[i++] = NSOpenGLPFADepthSize;
	attributes[i++] = NSOpenGLPixelFormatAttribute(24);
	attributes[i++] = NSOpenGLPFAStencilSize;
	attributes[i++] = NSOpenGLPixelFormatAttribute(8);
	
	if (m_multisample)
	{
		attributes[i++] = NSOpenGLPFAMultisample;
		attributes[i++] = NSOpenGLPFASampleBuffers;
		attributes[i++] = NSOpenGLPixelFormatAttribute(1);
		attributes[i++] = NSOpenGLPFASamples;
		attributes[i++] = NSOpenGLPixelFormatAttribute(m_multisample);
	}	
	
	attributes[i] = NSOpenGLPixelFormatAttribute(0);
	
	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	
	const NSRect contentRect = [m_window contentRectForFrameRect:[m_window frame]];
	NSOpenGLView* glView = [[FullscreenView alloc] initWithFrame:contentRect
													 pixelFormat:pixelFormat];
	[[glView openGLContext] makeCurrentContext];

	[m_window setContentView:glView];
	
	m_openGLInitialized = true;
}

- (void)setFullscreenModeWidth:(int)width height:(int)height
{
	NSScreen* screen = [m_window screen];

	const NSRect screenFrame = [screen frame];
	const NSRect displayRect = vid_hidpi
		? [screen convertRectToBacking:screenFrame]
		: screenFrame;

	const float  displayWidth  = displayRect.size.width;
	const float  displayHeight = displayRect.size.height;
	
	const float pixelScaleFactorX = displayWidth  / static_cast<float>(width );
	const float pixelScaleFactorY = displayHeight / static_cast<float>(height);
	
	rbOpts.pixelScale = std::min(pixelScaleFactorX, pixelScaleFactorY);
	
	rbOpts.width  = width  * rbOpts.pixelScale;
	rbOpts.height = height * rbOpts.pixelScale;
	
	rbOpts.shiftX = (displayWidth  - rbOpts.width ) / 2.0f;
	rbOpts.shiftY = (displayHeight - rbOpts.height) / 2.0f;

	if (!m_fullscreen)
	{
		[m_window setLevel:LEVEL_FULLSCREEN];
		[m_window setStyleMask:STYLE_MASK_FULLSCREEN];
		[m_window setHidesOnDeactivate:YES];
	}

	[m_window setFrame:displayRect display:YES];
	[m_window setFrameOrigin:NSMakePoint(0.0f, 0.0f)];
}

- (void)setWindowedModeWidth:(int)width height:(int)height
{
	rbOpts.pixelScale = 1.0f;
	
	rbOpts.width  = static_cast<float>(width );
	rbOpts.height = static_cast<float>(height);
	
	rbOpts.shiftX = 0.0f;
	rbOpts.shiftY = 0.0f;

	const NSSize windowPixelSize = NSMakeSize(width, height);
	const NSSize windowSize = vid_hidpi
		? [[m_window contentView] convertSizeFromBacking:windowPixelSize]
		: windowPixelSize;

	if (m_fullscreen)
	{
		[m_window setLevel:LEVEL_WINDOWED];
		[m_window setStyleMask:STYLE_MASK_WINDOWED];
		[m_window setHidesOnDeactivate:NO];
	}

	[m_window setContentSize:windowSize];
	[m_window center];

	NSButton* closeButton = [m_window standardWindowButton:NSWindowCloseButton];
	[closeButton setAction:@selector(terminate:)];
	[closeButton setTarget:NSApp];
}

- (void)changeVideoResolution:(bool)fullscreen width:(int)width height:(int)height useHiDPI:(bool)hiDPI
{
	if (fullscreen == m_fullscreen
		&& width   == m_width
		&& height  == m_height
		&& hiDPI   == m_hiDPI)
	{
		return;
	}

	[self initializeOpenGL];

	if (IsHiDPISupported())
	{
		NSOpenGLView* const glView = [m_window contentView];
		[glView setWantsBestResolutionOpenGLSurface:hiDPI];
	}

	if (fullscreen)
	{
		[self setFullscreenModeWidth:width height:height];
	}
	else
	{
		[self setWindowedModeWidth:width height:height];
	}

	rbOpts.dirty = true;

	const NSSize viewSize = GetRealContentViewSize(m_window);
	
	glViewport(0, 0, static_cast<GLsizei>(viewSize.width), static_cast<GLsizei>(viewSize.height));
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	CGLFlushDrawable(CGLGetCurrentContext());

	static NSString* const TITLE_STRING = 
		[NSString stringWithFormat:@"%s %s", GAMESIG, GetVersionString()];
	[m_window setTitle:TITLE_STRING];		
	
	if (![m_window isKeyWindow])
	{
		[m_window makeKeyAndOrderFront:nil];
	}

	m_fullscreen = fullscreen;
	m_width      = width;
	m_height     = height;
	m_hiDPI      = hiDPI;
}

- (void)useHiDPI:(bool)hiDPI
{
	if (!m_openGLInitialized)
	{
		return;
	}

	[self changeVideoResolution:m_fullscreen
						  width:m_width
						 height:m_height
					   useHiDPI:hiDPI];
}


- (void)setupSoftwareRenderingWithWidth:(int)width height:(int)height
{
	if (0 == m_softwareRenderingTexture)
	{
		glEnable(GL_TEXTURE_RECTANGLE_ARB);

		glGenTextures(1, &m_softwareRenderingTexture);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_softwareRenderingTexture);
		glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);

		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	delete[] m_softwareRenderingBuffer;
	m_softwareRenderingBuffer = new uint8_t[width * height * BYTES_PER_PIXEL];

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
}

- (void*)softwareRenderingBuffer
{
	return m_softwareRenderingBuffer;
}


- (void)processEvents:(NSTimer*)timer
{
	ZD_UNUSED(timer);

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

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
				
			default:
				break;
		}
		
		[NSApp sendEvent:event];
	}
    
    [NSApp updateWindows];

	[pool release];
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


- (void)setWindowStyleMask:(NSUInteger)styleMask
{
	// Before 10.6 it's impossible to change window's style mask
	// To workaround this new window should be created with required style mask
    // This method should not be called when building for Snow Leopard or newer

	FullscreenWindow* tempWindow = [self createWindow:styleMask];
	[tempWindow setContentView:[m_window contentView]];

	[m_window close];
	m_window = tempWindow;
}

@end


// ---------------------------------------------------------------------------


CUSTOM_CVAR(Bool, vid_hidpi, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (IsHiDPISupported())
	{
		[appCtrl useHiDPI:self];
	}
	else if (0 != self)
	{
		self = 0;
	}
}


// ---------------------------------------------------------------------------


void I_SetMainWindowVisible(bool visible)
{
	[appCtrl setMainWindowVisible:visible];

	SetNativeMouse(!visible);
}


// ---------------------------------------------------------------------------


bool I_SetCursor(FTexture* cursorpic)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

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
		memset(buffer, 0, imagePitch * imageHeight);

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
	
	[appCtrl invalidateCursorRects];

	[pool release];

	return true;
}


// ---------------------------------------------------------------------------


extern "C" 
{

typedef enum
{
	SDL_WINDOW_FULLSCREEN = 0x00000001,         /**< fullscreen window */
	SDL_WINDOW_OPENGL = 0x00000002,             /**< window usable with OpenGL context */
	SDL_WINDOW_FULLSCREEN_DESKTOP = ( SDL_WINDOW_FULLSCREEN | 0x00001000 ),
} SDL_WindowFlags;

struct SDL_Window
{
	uint32_t flags;
	int w, h;
	int pitch;
	void *pixels;
};

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int width, int height, uint32_t flags)
{
	[appCtrl changeVideoResolution:(SDL_WINDOW_FULLSCREEN_DESKTOP & flags)
							 width:width
							height:height
						  useHiDPI:vid_hidpi];

	static SDL_Window result;

	if (!(SDL_WINDOW_OPENGL & flags))
	{
		[appCtrl setupSoftwareRenderingWithWidth:width
										  height:height];
	}

	result.flags    = flags;
	result.w        = width;
	result.h        = height;
	result.pitch    = width * BYTES_PER_PIXEL;
	result.pixels   = [appCtrl softwareRenderingBuffer];

	return &result;
}
void SDL_DestroyWindow(SDL_Window *window)
{
	ZD_UNUSED(window);
}

uint32_t SDL_GetWindowFlags(SDL_Window *window)
{
	return window->flags;
}

int SDL_SetWindowFullscreen(SDL_Window* window, uint32_t flags)
{
	if ((window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == (flags & SDL_WINDOW_FULLSCREEN_DESKTOP))
		return 0;

	if (window->flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
	{
		window->flags &= ~SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	else
	{
		window->flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	[appCtrl changeVideoResolution:(SDL_WINDOW_FULLSCREEN_DESKTOP & flags)
							 width:window->w
							height:window->h
						  useHiDPI:vid_hidpi];

	return 0;
}

int SDL_UpdateWindowSurface(SDL_Window *screen)
{
	assert(NULL != screen);

	if (rbOpts.dirty)
	{
		glViewport(rbOpts.shiftX, rbOpts.shiftY, rbOpts.width, rbOpts.height);

		// TODO: Figure out why the following glClear() call is needed
		// to avoid drawing of garbage in fullscreen mode when
		// in-game's aspect ratio is different from display one
		glClear(GL_COLOR_BUFFER_BIT);

		rbOpts.dirty = false;
	}

	const int width  = screen->w;
	const int height = screen->h;

#ifdef __LITTLE_ENDIAN__
	static const GLenum format = GL_RGBA;
#else // __BIG_ENDIAN__
	static const GLenum format = GL_ABGR_EXT;
#endif // __LITTLE_ENDIAN__

	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8,
		width, height, 0, format, GL_UNSIGNED_BYTE, screen->pixels);

	glBegin(GL_QUADS);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, 0.0f);
	glTexCoord2f(width, 0.0f);
	glVertex2f(width, 0.0f);
	glTexCoord2f(width, height);
	glVertex2f(width, height);
	glTexCoord2f(0.0f, height);
	glVertex2f(0.0f, height);
	glEnd();

	glFlush();

	[[NSOpenGLContext currentContext] flushBuffer];

	return 0;
}

} // extern "C"


// ---------------------------------------------------------------------------


class CocoaVideo : public IVideo
{
public:
	explicit CocoaVideo(int dummy);

	virtual EDisplayType GetDisplayType() { return DISPLAY_Both; }
	virtual void SetWindowedScale(float scale);

	virtual DFrameBuffer* CreateFrameBuffer(int width, int height, bool fs, DFrameBuffer* old);

	virtual void StartModeIterator(int bits, bool fs);
	virtual bool NextMode(int* width, int* height, bool* letterbox);

private:
	size_t m_modeIterator;
};


class CocoaFrameBuffer : public DFrameBuffer
{
public:
	CocoaFrameBuffer(int width, int height, bool fullscreen);
	~CocoaFrameBuffer();

	virtual bool IsValid();

	virtual bool Lock(bool buffer);
	virtual void Unlock();
	virtual void Update();
	
	virtual PalEntry* GetPalette();
	virtual void GetFlashedPalette(PalEntry pal[256]);
	virtual void UpdatePalette();
	
	virtual bool SetGamma(float gamma);
	virtual bool SetFlash(PalEntry  rgb, int  amount);
	virtual void GetFlash(PalEntry &rgb, int &amount);

	virtual int GetPageCount();

	virtual bool IsFullscreen();

	virtual void SetVSync(bool vsync);

	void SetFullscreen(bool fullscreen);

private:
	PalEntry m_palette[256];
	bool     m_needPaletteUpdate;

	BYTE     m_gammaTable[3][256];
	float    m_gamma;
	bool     m_needGammaUpdate;

	PalEntry m_flashColor;
	int      m_flashAmount;

	bool     m_isUpdatePending;

	SDL_Window *Screen;

	void UpdateColors();
};


// ---------------------------------------------------------------------------


EXTERN_CVAR (Float, Gamma)

CUSTOM_CVAR (Float, rgamma, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (NULL != screen)
	{
		screen->SetGamma(Gamma);
	}
}

CUSTOM_CVAR (Float, ggamma, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (NULL != screen)
	{
		screen->SetGamma(Gamma);
	}
}

CUSTOM_CVAR (Float, bgamma, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (NULL != screen)
	{
		screen->SetGamma(Gamma);
	}
}


// ---------------------------------------------------------------------------


static const struct
{
	uint16_t width;
	uint16_t height;
}
VideoModes[] =
{
	{  320,  200 },
	{  320,  240 },
	{  400,  225 },	// 16:9
	{  400,  300 },
	{  480,  270 },	// 16:9
	{  480,  360 },
	{  512,  288 },	// 16:9
	{  512,  384 },
	{  640,  360 },	// 16:9
	{  640,  400 },
	{  640,  480 },
	{  720,  480 },	// 16:10
	{  720,  540 },
	{  800,  450 },	// 16:9
	{  800,  480 },
	{  800,  500 },	// 16:10
	{  800,  600 },
	{  848,  480 },	// 16:9
	{  960,  600 },	// 16:10
	{  960,  720 },
	{ 1024,  576 },	// 16:9
	{ 1024,  600 },	// 17:10
	{ 1024,  640 },	// 16:10
	{ 1024,  768 },
	{ 1088,  612 },	// 16:9
	{ 1152,  648 },	// 16:9
	{ 1152,  720 },	// 16:10
	{ 1152,  864 },
	{ 1280,  720 },	// 16:9
	{ 1280,  854 },
	{ 1280,  800 },	// 16:10
	{ 1280,  960 },
	{ 1280, 1024 },	// 5:4
	{ 1360,  768 },	// 16:9
	{ 1366,  768 },
	{ 1400,  787 },	// 16:9
	{ 1400,  875 },	// 16:10
	{ 1400, 1050 },
	{ 1440,  900 },
	{ 1440,  960 },
	{ 1440, 1080 },
	{ 1600,  900 },	// 16:9
	{ 1600, 1000 },	// 16:10
	{ 1600, 1200 },
	{ 1920, 1080 },
	{ 1920, 1200 },
	{ 2048, 1536 },
	{ 2560, 1440 },
	{ 2560, 1600 },
	{ 2560, 2048 },
	{ 2880, 1800 },
	{ 3200, 1800 },
	{ 3840, 2160 },
	{ 3840, 2400 },
	{ 4096, 2160 },
	{ 5120, 2880 }
};


static cycle_t BlitCycles;
static cycle_t FlipCycles;


// ---------------------------------------------------------------------------


CocoaVideo::CocoaVideo(int dummy)
: m_modeIterator(0)
{
	ZD_UNUSED(dummy);
}

void CocoaVideo::StartModeIterator(int bits, bool fs)
{
	ZD_UNUSED(bits);
	ZD_UNUSED(fs);

	m_modeIterator = 0;
}

bool CocoaVideo::NextMode(int* width, int* height, bool* letterbox)
{
	assert(NULL != width);
	assert(NULL != height);
	ZD_UNUSED(letterbox);

	if (m_modeIterator < sizeof(VideoModes) / sizeof(VideoModes[0]))
	{
		*width  = VideoModes[m_modeIterator].width;
		*height = VideoModes[m_modeIterator].height;

		++m_modeIterator;
		return true;
	}

	return false;
}

DFrameBuffer* CocoaVideo::CreateFrameBuffer(int width, int height, bool fullscreen, DFrameBuffer* old)
{
	static int retry = 0;
	static int owidth, oheight;

	PalEntry flashColor;
	int flashAmount;

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		CocoaFrameBuffer *fb = static_cast<CocoaFrameBuffer *> (old);
		if (fb->GetWidth() == width &&
			fb->GetHeight() == height)
		{
			if (fb->IsFullscreen() != fullscreen)
			{
				fb->SetFullscreen (fullscreen);
			}

			return old;
		}
		old->GetFlash (flashColor, flashAmount);
		old->ObjectFlags |= OF_YesReallyDelete;
		if (screen == old) screen = NULL;
		delete old;
	}
	else
	{
		flashColor = 0;
		flashAmount = 0;
	}

	CocoaFrameBuffer *fb = new CocoaFrameBuffer (width, height, fullscreen);
	retry = 0;

	// If we could not create the framebuffer, try again with slightly
	// different parameters in this order:
	// 1. Try with the closest size
	// 2. Try in the opposite screen mode with the original size
	// 3. Try in the opposite screen mode with the closest size
	// This is a somewhat confusing mass of recursion here.

	while (fb == NULL || !fb->IsValid ())
	{
		if (fb != NULL)
		{
			delete fb;
		}

		switch (retry)
		{
			case 0:
				owidth = width;
				oheight = height;
			case 2:
				// Try a different resolution. Hopefully that will work.
				I_ClosestResolution (&width, &height, 8);
				break;

			case 1:
				// Try changing fullscreen mode. Maybe that will work.
				width = owidth;
				height = oheight;
				fullscreen = !fullscreen;
				break;

			default:
				// I give up!
				I_FatalError ("Could not create new screen (%d x %d)", owidth, oheight);
		}

		++retry;
		fb = static_cast<CocoaFrameBuffer *>(CreateFrameBuffer (width, height, fullscreen, NULL));
	}

	fb->SetFlash (flashColor, flashAmount);

	return fb;
}

void CocoaVideo::SetWindowedScale (float scale)
{
}


CocoaFrameBuffer::CocoaFrameBuffer (int width, int height, bool fullscreen)
: DFrameBuffer(width, height)
, m_needPaletteUpdate(false)
, m_gamma(0.0f)
, m_needGammaUpdate(false)
, m_flashAmount(0)
, m_isUpdatePending(false)
{
	FString caption;
	caption.Format(GAMESIG " %s (%s)", GetVersionString(), GetGitTime());

	Screen = SDL_CreateWindow (caption, 0, 0,
		width, height, (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));

	if (Screen == NULL)
		return;

	GPfx.SetFormat(32, 0x000000FF, 0x0000FF00, 0x00FF0000);

	for (size_t i = 0; i < 256; ++i)
	{
		m_gammaTable[0][i] = m_gammaTable[1][i] = m_gammaTable[2][i] = i;
	}

	memcpy(m_palette, GPalette.BaseColors, sizeof(PalEntry) * 256);
	UpdateColors();

	SetVSync(vid_vsync);
}


CocoaFrameBuffer::~CocoaFrameBuffer ()
{
	if(Screen)
	{
		SDL_DestroyWindow (Screen);
	}
}

bool CocoaFrameBuffer::IsValid ()
{
	return DFrameBuffer::IsValid() && Screen != NULL;
}

int CocoaFrameBuffer::GetPageCount ()
{
	return 1;
}

bool CocoaFrameBuffer::Lock (bool buffered)
{
	return DSimpleCanvas::Lock ();
}

void CocoaFrameBuffer::Unlock ()
{
	if (m_isUpdatePending && LockCount == 1)
	{
		Update ();
	}
	else if (--LockCount <= 0)
	{
		Buffer = NULL;
		LockCount = 0;
	}
}

void CocoaFrameBuffer::Update ()
{
	if (LockCount != 1)
	{
		if (LockCount > 0)
		{
			m_isUpdatePending = true;
			--LockCount;
		}
		return;
	}

	DrawRateStuff ();

	Buffer = NULL;
	LockCount = 0;
	m_isUpdatePending = false;

	BlitCycles.Reset();
	FlipCycles.Reset();
	BlitCycles.Clock();

	GPfx.Convert(MemBuffer, Pitch, [appCtrl softwareRenderingBuffer], Width * BYTES_PER_PIXEL,
		Width, Height, FRACUNIT, FRACUNIT, 0, 0);

	FlipCycles.Clock();
	SDL_UpdateWindowSurface(Screen);
	FlipCycles.Unclock();

	BlitCycles.Unclock();

	if (m_needGammaUpdate)
	{
		bool Windowed = false;
		m_needGammaUpdate = false;
		CalcGamma((Windowed || rgamma == 0.f) ? m_gamma : (m_gamma * rgamma), m_gammaTable[0]);
		CalcGamma((Windowed || ggamma == 0.f) ? m_gamma : (m_gamma * ggamma), m_gammaTable[1]);
		CalcGamma((Windowed || bgamma == 0.f) ? m_gamma : (m_gamma * bgamma), m_gammaTable[2]);
		m_needPaletteUpdate = true;
	}

	if (m_needPaletteUpdate)
	{
		m_needPaletteUpdate = false;
		UpdateColors();
	}
}

void CocoaFrameBuffer::UpdateColors()
{
	PalEntry palette[256];

	for (size_t i = 0; i < 256; ++i)
	{
		palette[i].r = m_gammaTable[0][m_palette[i].r];
		palette[i].g = m_gammaTable[1][m_palette[i].g];
		palette[i].b = m_gammaTable[2][m_palette[i].b];
	}

	if (m_flashAmount)
	{
		DoBlending(palette, palette, 256,
			m_gammaTable[0][m_flashColor.r], m_gammaTable[1][m_flashColor.g], m_gammaTable[2][m_flashColor.b],
			m_flashAmount);
	}

	GPfx.SetPalette(palette);
}

PalEntry *CocoaFrameBuffer::GetPalette ()
{
	return m_palette;
}

void CocoaFrameBuffer::UpdatePalette()
{
	m_needPaletteUpdate = true;
}

bool CocoaFrameBuffer::SetGamma(float gamma)
{
	m_gamma           = gamma;
	m_needGammaUpdate = true;

	return true;
}

bool CocoaFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	m_flashColor        = rgb;
	m_flashAmount       = amount;
	m_needPaletteUpdate = true;

	return true;
}

void CocoaFrameBuffer::GetFlash(PalEntry &rgb, int &amount)
{
	rgb    = m_flashColor;
	amount = m_flashAmount;
}

void CocoaFrameBuffer::GetFlashedPalette(PalEntry pal[256])
{
	memcpy(pal, m_palette, sizeof m_palette);

	if (0 != m_flashAmount)
	{
		DoBlending (pal, pal, 256, m_flashColor.r, m_flashColor.g, m_flashColor.b, m_flashAmount);
	}
}

void CocoaFrameBuffer::SetFullscreen (bool fullscreen)
{
	SDL_SetWindowFullscreen (Screen, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

bool CocoaFrameBuffer::IsFullscreen ()
{
	return (SDL_GetWindowFlags (Screen) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

void CocoaFrameBuffer::SetVSync (bool vsync)
{
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
	const long value = vsync ? 1 : 0;
#else // 10.5 or newer
	const GLint value = vsync ? 1 : 0;
#endif // prior to 10.5

	[[NSOpenGLContext currentContext] setValues:&value
								   forParameter:NSOpenGLCPSwapInterval];
}


ADD_STAT(blit)
{
	FString result;
	result.Format("blit=%04.1f ms  flip=%04.1f ms", BlitCycles.TimeMS(), FlipCycles.TimeMS());
	return result;
}


IVideo *Video;


void I_ShutdownGraphics ()
{
	if (screen)
	{
		DFrameBuffer *s = screen;
		screen = NULL;
		s->ObjectFlags |= OF_YesReallyDelete;
		delete s;
	}
	if (Video)
		delete Video, Video = NULL;
}

void I_InitGraphics ()
{
	UCVarValue val;

	val.Bool = !!Args->CheckParm ("-devparm");
	ticker.SetGenericRepDefault (val, CVAR_Bool);

	Video = new CocoaVideo (0);
	if (Video == NULL)
		I_FatalError ("Failed to initialize display");

	atterm (I_ShutdownGraphics);
}

static void I_DeleteRenderer()
{
	if (Renderer != NULL) delete Renderer;
}

void I_CreateRenderer()
{
	if (Renderer == NULL)
	{
		Renderer = new FSoftwareRenderer;
		atterm(I_DeleteRenderer);
	}
}


DFrameBuffer *I_SetMode (int &width, int &height, DFrameBuffer *old)
{
	bool fs = false;
	switch (Video->GetDisplayType ())
	{
		case DISPLAY_WindowOnly:
			fs = false;
			break;
		case DISPLAY_FullscreenOnly:
			fs = true;
			break;
		case DISPLAY_Both:
			fs = fullscreen;
			break;
	}

	return Video->CreateFrameBuffer (width, height, fs, old);
}

bool I_CheckResolution (int width, int height, int bits)
{
	int twidth, theight;

	Video->StartModeIterator (bits, screen ? screen->IsFullscreen() : fullscreen);
	while (Video->NextMode (&twidth, &theight, NULL))
	{
		if (width == twidth && height == theight)
			return true;
	}
	return false;
}

void I_ClosestResolution (int *width, int *height, int bits)
{
	int twidth, theight;
	int cwidth = 0, cheight = 0;
	int iteration;
	DWORD closest = 4294967295u;

	for (iteration = 0; iteration < 2; iteration++)
	{
		Video->StartModeIterator (bits, screen ? screen->IsFullscreen() : fullscreen);
		while (Video->NextMode (&twidth, &theight, NULL))
		{
			if (twidth == *width && theight == *height)
				return;

			if (iteration == 0 && (twidth < *width || theight < *height))
				continue;

			DWORD dist = (twidth - *width) * (twidth - *width)
			+ (theight - *height) * (theight - *height);

			if (dist < closest)
			{
				closest = dist;
				cwidth = twidth;
				cheight = theight;
			}
		}
		if (closest != 4294967295u)
		{
			*width = cwidth;
			*height = cheight;
			return;
		}
	}
}


EXTERN_CVAR(Int, vid_maxfps);
EXTERN_CVAR(Bool, cl_capfps);

// So Apple doesn't support POSIX timers and I can't find a good substitute short of
// having Objective-C Cocoa events or something like that.
void I_SetFPSLimit(int limit)
{
}

CUSTOM_CVAR (Int, vid_maxfps, 200, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (vid_maxfps < TICRATE && vid_maxfps != 0)
	{
		vid_maxfps = TICRATE;
	}
	else if (vid_maxfps > 1000)
	{
		vid_maxfps = 1000;
	}
	else if (cl_capfps == 0)
	{
		I_SetFPSLimit(vid_maxfps);
	}
}


CCMD (vid_listmodes)
{
	static const char *ratios[5] = { "", " - 16:9", " - 16:10", "", " - 5:4" };
	int width, height, bits;
	bool letterbox;

	if (Video == NULL)
	{
		return;
	}
	for (bits = 1; bits <= 32; bits++)
	{
		Video->StartModeIterator (bits, screen->IsFullscreen());
		while (Video->NextMode (&width, &height, &letterbox))
		{
			bool thisMode = (width == DisplayWidth && height == DisplayHeight && bits == DisplayBits);
			int ratio = CheckRatio (width, height);
			Printf (thisMode ? PRINT_BOLD : PRINT_HIGH,
				"%s%4d x%5d x%3d%s%s\n",
				thisMode || !(ratio & 3) ? "" : TEXTCOLOR_GOLD,
				width, height, bits,
				ratios[ratio],
				thisMode || !letterbox ? "" : TEXTCOLOR_BROWN " LB");
		}
	}
}

CCMD (vid_currentmode)
{
	Printf ("%dx%dx%d\n", DisplayWidth, DisplayHeight, DisplayBits);
}


namespace
{

NSMenuItem* CreateApplicationMenu()
{
	NSMenu* menu = [NSMenu new];

	[menu addItemWithTitle:[@"About " stringByAppendingString:@GAMENAME]
					   action:@selector(orderFrontStandardAboutPanel:)
				keyEquivalent:@""];
	[menu addItem:[NSMenuItem separatorItem]];
	[menu addItemWithTitle:[@"Hide " stringByAppendingString:@GAMENAME]
					   action:@selector(hide:)
				keyEquivalent:@"h"];
	[[menu addItemWithTitle:@"Hide Others"
						action:@selector(hideOtherApplications:)
				 keyEquivalent:@"h"]
	 setKeyEquivalentModifierMask:NSAlternateKeyMask | NSCommandKeyMask];
	[menu addItemWithTitle:@"Show All"
					   action:@selector(unhideAllApplications:)
				keyEquivalent:@""];
	[menu addItem:[NSMenuItem separatorItem]];
	[menu addItemWithTitle:[@"Quit " stringByAppendingString:@GAMENAME]
					   action:@selector(terminate:)
				keyEquivalent:@"q"];

	NSMenuItem* menuItem = [NSMenuItem new];
	[menuItem setSubmenu:menu];

	if ([NSApp respondsToSelector:@selector(setAppleMenu:)])
	{
		[NSApp performSelector:@selector(setAppleMenu:) withObject:menu];
	}

	return menuItem;
}

NSMenuItem* CreateEditMenu()
{
	NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Edit"];

	[menu addItemWithTitle:@"Undo"
						action:@selector(undo:)
				 keyEquivalent:@"z"];
	[menu addItemWithTitle:@"Redo"
						action:@selector(redo:)
				 keyEquivalent:@"Z"];
	[menu addItem:[NSMenuItem separatorItem]];
	[menu addItemWithTitle:@"Cut"
						action:@selector(cut:)
				 keyEquivalent:@"x"];
	[menu addItemWithTitle:@"Copy"
						action:@selector(copy:)
				 keyEquivalent:@"c"];
	[menu addItemWithTitle:@"Paste"
						action:@selector(paste:)
				 keyEquivalent:@"v"];
	[menu addItemWithTitle:@"Delete"
						action:@selector(delete:)
				 keyEquivalent:@""];
	[menu addItemWithTitle:@"Select All"
						action:@selector(selectAll:)
				 keyEquivalent:@"a"];

	NSMenuItem* menuItem = [NSMenuItem new];
	[menuItem setSubmenu:menu];

	return menuItem;
}

NSMenuItem* CreateWindowMenu()
{
	NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Window"];
	[NSApp setWindowsMenu:menu];

	[menu addItemWithTitle:@"Minimize"
					action:@selector(performMiniaturize:)
			 keyEquivalent:@"m"];
	[menu addItemWithTitle:@"Zoom"
					action:@selector(performZoom:)
			 keyEquivalent:@""];
	[menu addItem:[NSMenuItem separatorItem]];
	[menu addItemWithTitle:@"Bring All to Front"
					action:@selector(arrangeInFront:)
			 keyEquivalent:@""];

	NSMenuItem* menuItem = [NSMenuItem new];
	[menuItem setSubmenu:menu];

	return menuItem;
}

void CreateMenu()
{
	NSMenu* menuBar = [NSMenu new];
	[menuBar addItem:CreateApplicationMenu()];
	[menuBar addItem:CreateEditMenu()];
	[menuBar addItem:CreateWindowMenu()];

	[NSApp setMainMenu:menuBar];
}

DarwinVersion GetDarwinVersion()
{
	DarwinVersion result = {};

	int mib[2] = { CTL_KERN, KERN_OSRELEASE };
	size_t size = 0;

	if (0 == sysctl(mib, 2, NULL, &size, NULL, 0))
	{
		char* version = static_cast<char*>(alloca(size));

		if (0 == sysctl(mib, 2, version, &size, NULL, 0))
		{
			sscanf(version, "%hu.%hu.%hu",
				&result.major, &result.minor, &result.bugfix);
		}
	}

	return result;
}

void ReleaseApplicationController()
{
	if (NULL != appCtrl)
	{
		[NSApp setDelegate:nil];
		[NSApp deactivate];

		[appCtrl release];
		appCtrl = NULL;
	}
}

} // unnamed namespace


const DarwinVersion darwinVersion = GetDarwinVersion();


int main(int argc, char** argv)
{
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

	// The following code isn't mandatory,
	// but it enables to run the application without a bundle
	if ([NSApp respondsToSelector:@selector(setActivationPolicy:)])
	{
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	}

	CreateMenu();

	atterm(ReleaseApplicationController);

	appCtrl = [ApplicationController new];
	[NSApp setDelegate:appCtrl];
	[NSApp run];

	[pool release];

	return EXIT_SUCCESS;
}
