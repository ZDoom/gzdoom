/*
 ** i_main.mm
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2018 Alexey Lysiuk
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
#include "s_soundinternal.h"

#include <sys/sysctl.h>
#include <sys/stat.h>

#include "c_console.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "i_system.h"
#include "m_argv.h"
#include "st_console.h"
#include "version.h"
#include "printf.h"
#include "s_music.h"
#include "engineerrors.h"


#define ZD_UNUSED(VARIABLE) ((void)(VARIABLE))


// ---------------------------------------------------------------------------


EXTERN_CVAR(Int,  vid_defwidth )
EXTERN_CVAR(Int,  vid_defheight)
EXTERN_CVAR(Bool, vid_vsync    )

int GameMain();
// ---------------------------------------------------------------------------


void Mac_I_FatalError(const char* const message)
{
	I_SetMainWindowVisible(false);
	S_StopMusic(true);

	FConsoleWindow::GetInstance().ShowFatalError(message);
}


static bool ReadSystemVersionFromPlist(NSOperatingSystemVersion& version)
{
#if MAC_OS_X_VERSION_MAX_ALLOWED < 110000
	// The version returned by macOS depends on the SDK which the software has been built against.
	// When built against the 10.15 SDK or earlier, Big Sur returns 10.16 for compatibility with previous numbering.
	// When built against the 11.0 SDK, it returns 11.0 for forward compatibility.
	// https://eclecticlight.co/2020/08/13/macos-version-numbering-isnt-so-simple/

	// It's impossible to load real SystemVersion.plist when linking with old SDK, i.e. when building for Intel CPU
	// Any attempt to read this file is redirected to SystemVersionCompat.plist silently
	// Workaround with the external process is needed in order to report correct macOS version

	const char *const plistPath = "/System/Library/CoreServices/SystemVersion.plist";
	struct stat dummy;

	if (stat(plistPath, &dummy) != 0)
		return false;

	char commandLine[1024] = {};
	snprintf(commandLine, sizeof commandLine, "defaults read %s ProductVersion", plistPath);

	FILE *const versionFile = popen(commandLine, "r");

	if (versionFile == nullptr)
		return false;

	NSOperatingSystemVersion plistVersion = {};
	char versionString[256] = {};

	if (fgets(versionString, sizeof versionString, versionFile))
	{
		plistVersion.majorVersion = atoi(versionString);

		if (const char *minorVersionString = strstr(versionString, "."))
		{
			minorVersionString++;
			plistVersion.minorVersion = atoi(minorVersionString);

			if (const char *patchVersionString = strstr(minorVersionString, "."))
			{
				patchVersionString++;
				plistVersion.patchVersion = atoi(patchVersionString);
			}
		}
	}

	fclose(versionFile);

	if (plistVersion.majorVersion != 0)
	{
		version = plistVersion;
		return true;
	}
#endif // MAC_OS_X_VERSION_MAX_ALLOWED < 110000

	return false;
}

void I_DetectOS()
{
	NSOperatingSystemVersion version = {};

	if (!ReadSystemVersionFromPlist(version))
	{
		NSProcessInfo *const processInfo = [NSProcessInfo processInfo];

		if ([processInfo respondsToSelector:@selector(operatingSystemVersion)])
		{
			version = [processInfo operatingSystemVersion];
		}
	}

	const char* name = "Unknown version";

	switch (version.majorVersion)
	{
	case 10:
		switch (version.minorVersion)
		{
			case 12: name = "Sierra";      break;
			case 13: name = "High Sierra"; break;
			case 14: name = "Mojave";      break;
			case 15: name = "Catalina";    break;
			case 16: name = "Big Sur";     break;
		}
		break;
	case 11:
		name = "Big Sur";
		break;
	case 12:
		name = "Monterey";
		break;
	case 13:
		name = "Ventura";
		break;
	case 14:
		name = "Sonoma";
		break;
	}

	char release[16] = "unknown";
	size_t size = sizeof release - 1;
	sysctlbyname("kern.osversion", release, &size, nullptr, 0);

	char model[64] = "Unknown Mac model";
	size = sizeof model - 1;
	sysctlbyname("hw.model", model, &size, nullptr, 0);

	const char* const architecture =
#ifdef __x86_64__
		"64-bit Intel";	
#elif defined __aarch64__
		"64-bit ARM";
#else
		"Unknown";
#endif

	Printf("%s running macOS %s %d.%d.%d (%s) %s\n", model, name,
		   int(version.majorVersion), int(version.minorVersion), int(version.patchVersion),
		   release, architecture);
}


FArgs* Args; // command line arguments


namespace
{

TArray<FString> s_argv;

int DoMain(int argc, char** argv)
{
	printf(GAMENAME" %s - %s - Cocoa version\nCompiled on %s\n\n",
		GetVersionString(), GetGitTime(), __DATE__);

	seteuid(getuid());

	// Set LC_NUMERIC environment variable in case some library decides to
	// clear the setlocale call at least this will be correct.
	// Note that the LANG environment variable is overridden by LC_*
	setenv("LC_NUMERIC", "C", 1);
	setlocale(LC_ALL, "C");

	Args = new FArgs(argc, argv);

	NSString* exePath = [[NSBundle mainBundle] executablePath];
	progdir = [[exePath stringByDeletingLastPathComponent] UTF8String];
	progdir += "/";

	auto ret = GameMain();
	FConsoleWindow::DeleteInstance();
	return ret;
}

} // unnamed namespace


// ---------------------------------------------------------------------------


@interface ApplicationController : NSResponder<NSApplicationDelegate>
{
}

- (void)keyDown:(NSEvent*)theEvent;
- (void)keyUp:(NSEvent*)theEvent;

- (void)applicationDidBecomeActive:(NSNotification*)aNotification;
- (void)applicationWillResignActive:(NSNotification*)aNotification;

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification;

- (BOOL)application:(NSApplication*)theApplication openFile:(NSString*)filename;

- (void)processEvents:(NSTimer*)timer;

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;

- (void)sendExitEvent:(id)sender;

@end


ApplicationController* appCtrl;


@implementation ApplicationController

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


extern bool AppActive;

- (void)applicationDidBecomeActive:(NSNotification*)aNotification
{
	ZD_UNUSED(aNotification);

	S_SetSoundPaused(1);

	AppActive = true;
}

- (void)applicationWillResignActive:(NSNotification*)aNotification
{
	ZD_UNUSED(aNotification);

	S_SetSoundPaused(0);

	AppActive = false;
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

	FConsoleWindow::CreateInstance();

	const size_t argc = s_argv.Size();
	TArray<char*> argv(argc + 1, true);

	for (size_t i = 0; i < argc; ++i)
	{
		argv[i] = s_argv[i].LockBuffer();
	}

	argv[argc] = nullptr;

	exit(DoMain(argc, &argv[0]));
}


- (BOOL)application:(NSApplication*)theApplication openFile:(NSString*)filename
{
	ZD_UNUSED(theApplication);

	// Some parameters from command line are passed to this function
	// These parameters need to be skipped to avoid duplication
	// Note: SDL has different approach to fix this issue, see the same method in SDLMain.m

	const char* const charFileName = [filename UTF8String];

	for (size_t i = 0, count = s_argv.Size(); i < count; ++i)
	{
		if (0 == strcmp(s_argv[i], charFileName))
		{
			return FALSE;
		}
	}

	bool iwad = false;

	if (const char* const extPos = strrchr(charFileName, '.'))
	{
		iwad = 0 == stricmp(extPos, ".iwad")
			|| 0 == stricmp(extPos, ".ipk3")
			|| 0 == stricmp(extPos, ".ipk7");
	}

	s_argv.Push(iwad ? "-iwad" : "-file");
	s_argv.Push(charFileName);

	return TRUE;
}


- (void)processEvents:(NSTimer*)timer
{
	ZD_UNUSED(timer);

	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	while (true)
	{
		NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
											untilDate:[NSDate dateWithTimeIntervalSinceNow:0]
											   inMode:NSDefaultRunLoopMode
											  dequeue:YES];
		if (nil == event)
		{
			break;
		}

		I_ProcessEvent(event);

		[NSApp sendEvent:event];
	}

	[NSApp updateWindows];

	[pool release];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	[self sendExitEvent:sender];
	return NSTerminateLater;
}

- (void)sendExitEvent:(id)sender
{
	throw CExitEvent(0);
}

@end


// ---------------------------------------------------------------------------


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
	 setKeyEquivalentModifierMask:NSEventModifierFlagOption | NSEventModifierFlagCommand];
	[menu addItemWithTitle:@"Show All"
					   action:@selector(unhideAllApplications:)
				keyEquivalent:@""];
	[menu addItem:[NSMenuItem separatorItem]];
	[menu addItemWithTitle:[@"Quit " stringByAppendingString:@GAMENAME]
					action:@selector(sendExitEvent:)
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


int main(int argc, char** argv)
{
	for (int i = 0; i < argc; ++i)
	{
		const char* const argument = argv[i];

#if _DEBUG
		if (0 == strcmp(argument, "-wait_for_debugger"))
		{
			NSAlert* alert = [[NSAlert alloc] init];
			[alert setMessageText:@GAMENAME];
			[alert setInformativeText:@"Waiting for debugger..."];
			[alert addButtonWithTitle:@"Continue"];
			[alert runModal];
		}
#endif // _DEBUG

		s_argv.Push(argument);
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

	atexit(ReleaseApplicationController);

	appCtrl = [ApplicationController new];
	[NSApp setDelegate:appCtrl];
	[NSApp run];

	[pool release];

	return EXIT_SUCCESS;
}
