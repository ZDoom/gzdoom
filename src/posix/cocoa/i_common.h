/*
 ** i_common.h
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

#ifndef COCOA_I_COMMON_INCLUDED
#define COCOA_I_COMMON_INCLUDED

#import <AppKit/AppKit.h>


struct RenderBufferOptions
{
	float pixelScale;

	float shiftX;
	float shiftY;

	float width;
	float height;

	bool dirty;
};

extern RenderBufferOptions rbOpts;


// Version of AppKit framework we are interested in
// The following values are needed to build with earlier SDKs

#define AppKit10_4  824
#define AppKit10_5  949
#define AppKit10_6 1038
#define AppKit10_7 1138


@interface NSWindow(ExitAppOnClose)
- (void)exitAppOnClose;
@end


inline bool I_IsHiDPISupported()
{
	return NSAppKitVersionNumber >= AppKit10_7;
}

void I_ProcessEvent(NSEvent* event);

void I_ProcessJoysticks();

NSSize I_GetContentViewSize(const NSWindow* window);
void I_SetMainWindowVisible(bool visible);
void I_SetNativeMouse(bool wantNative);


// The following definitions are required to build with older OS X SDKs

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050

typedef unsigned int NSUInteger;
typedef          int NSInteger;

typedef float CGFloat;

// From HIToolbox/Events.h
enum
{
	kVK_ANSI_F        = 0x03,
	kVK_Return        = 0x24,
	kVK_Tab           = 0x30,
	kVK_Space         = 0x31,
	kVK_Delete        = 0x33,
	kVK_Escape        = 0x35,
	kVK_Command       = 0x37,
	kVK_Shift         = 0x38,
	kVK_CapsLock      = 0x39,
	kVK_Option        = 0x3A,
	kVK_Control       = 0x3B,
	kVK_RightShift    = 0x3C,
	kVK_RightOption   = 0x3D,
	kVK_RightControl  = 0x3E,
	kVK_Function      = 0x3F,
	kVK_F17           = 0x40,
	kVK_VolumeUp      = 0x48,
	kVK_VolumeDown    = 0x49,
	kVK_Mute          = 0x4A,
	kVK_F18           = 0x4F,
	kVK_F19           = 0x50,
	kVK_F20           = 0x5A,
	kVK_F5            = 0x60,
	kVK_F6            = 0x61,
	kVK_F7            = 0x62,
	kVK_F3            = 0x63,
	kVK_F8            = 0x64,
	kVK_F9            = 0x65,
	kVK_F11           = 0x67,
	kVK_F13           = 0x69,
	kVK_F16           = 0x6A,
	kVK_F14           = 0x6B,
	kVK_F10           = 0x6D,
	kVK_F12           = 0x6F,
	kVK_F15           = 0x71,
	kVK_Help          = 0x72,
	kVK_Home          = 0x73,
	kVK_PageUp        = 0x74,
	kVK_ForwardDelete = 0x75,
	kVK_F4            = 0x76,
	kVK_End           = 0x77,
	kVK_F2            = 0x78,
	kVK_PageDown      = 0x79,
	kVK_F1            = 0x7A,
	kVK_LeftArrow     = 0x7B,
	kVK_RightArrow    = 0x7C,
	kVK_DownArrow     = 0x7D,
	kVK_UpArrow       = 0x7E
};

static const NSOpenGLPixelFormatAttribute NSOpenGLPFAAllowOfflineRenderers = NSOpenGLPixelFormatAttribute(96);

@interface NSWindow(SetCollectionBehavior)
- (void)setCollectionBehavior:(NSUInteger)collectionBehavior;
@end

typedef NSUInteger NSWindowCollectionBehavior;

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

static const NSWindowCollectionBehavior NSWindowCollectionBehaviorFullScreenAuxiliary = NSWindowCollectionBehavior(1 << 8);

#endif // prior to 10.7

#endif // COCOA_I_COMMON_INCLUDED
