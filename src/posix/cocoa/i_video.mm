/*
 ** i_video.mm
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

#include "gl_load/gl_load.h"

#include "i_common.h"

// Avoid collision between DObject class and Objective-C
#define Class ObjectClass

#include "bitmap.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "hardware.h"
#include "m_argv.h"
#include "m_png.h"
#include "r_renderer.h"
#include "swrenderer/r_swrenderer.h"
#include "st_console.h"
#include "v_text.h"
#include "version.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/textures/gl_samplers.h"

#undef Class


@implementation NSWindow(ExitAppOnClose)

- (void)exitAppOnClose
{
	NSButton* closeButton = [self standardWindowButton:NSWindowCloseButton];
	[closeButton setAction:@selector(terminate:)];
	[closeButton setTarget:NSApp];
}

@end

@interface NSWindow(EnterFullscreenOnZoom)
- (void)enterFullscreenOnZoom;
@end

@implementation NSWindow(EnterFullscreenOnZoom)

- (void)enterFullscreen:(id)sender
{
	ToggleFullscreen = true;
}

- (void)enterFullscreenOnZoom
{
	NSButton* zoomButton = [self standardWindowButton:NSWindowZoomButton];
	[zoomButton setEnabled:YES];
	[zoomButton setAction:@selector(enterFullscreen:)];
	[zoomButton setTarget:self];
}

@end

EXTERN_CVAR(Bool, vid_vsync)
EXTERN_CVAR(Bool, vid_hidpi)
EXTERN_CVAR(Int,  vid_defwidth)
EXTERN_CVAR(Int,  vid_defheight)
EXTERN_CVAR(Bool, fullscreen)

CVAR(Int, win_x, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_y, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_w, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_h, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Bool, vid_autoswitch, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("You must restart " GAMENAME " to apply graphics switching mode\n");
}


// ---------------------------------------------------------------------------


namespace
{
	const NSInteger LEVEL_FULLSCREEN = NSMainMenuWindowLevel + 1;
	const NSInteger LEVEL_WINDOWED   = NSNormalWindowLevel;

	const NSUInteger STYLE_MASK_FULLSCREEN = NSBorderlessWindowMask;
	const NSUInteger STYLE_MASK_WINDOWED   = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;
}


// ---------------------------------------------------------------------------


@interface CocoaWindow : NSWindow
{
	NSString* m_title;
}

- (BOOL)canBecomeKeyWindow;
- (void)setTitle:(NSString*)title;
- (void)updateTitle;

@end


@implementation CocoaWindow

- (BOOL)canBecomeKeyWindow
{
	return true;
}

- (void)setTitle:(NSString*)title
{
	m_title = title;

	[self updateTitle];
}

- (void)updateTitle
{
	if (nil == m_title)
	{
		m_title = [NSString stringWithFormat:@"%s %s", GAMESIG, GetVersionString()];
	}

	[super setTitle:m_title];
}

- (void)frameDidChange:(NSNotification*)notification
{
	const NSRect frame = [self frame];
	win_x = frame.origin.x;
	win_y = frame.origin.y;
	win_w = frame.size.width;
	win_h = frame.size.height;
}

@end


// ---------------------------------------------------------------------------


@interface CocoaView : NSOpenGLView
{
	NSCursor* m_cursor;
}

- (void)resetCursorRects;

- (void)setCursor:(NSCursor*)cursor;

@end


@implementation CocoaView

- (void)resetCursorRects
{
	[super resetCursorRects];

	NSCursor* const cursor = nil == m_cursor
		? [NSCursor arrowCursor]
		: m_cursor;

	[self addCursorRect:[self bounds]
				 cursor:cursor];
}

- (void)setCursor:(NSCursor*)cursor
{
	m_cursor = cursor;
}

@end


// ---------------------------------------------------------------------------


class CocoaVideo : public IVideo
{
public:
	virtual DFrameBuffer* CreateFrameBuffer() override
	{
		auto fb = new OpenGLFrameBuffer(nullptr, fullscreen);

		fb->SetMode(fullscreen, vid_hidpi);
		fb->SetSize(fb->GetClientWidth(), fb->GetClientHeight());

		return fb;
	}
};


// ---------------------------------------------------------------------------


EXTERN_CVAR(Float, Gamma)

CUSTOM_CVAR(Float, rgamma, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (NULL != screen)
	{
		screen->SetGamma();
	}
}

CUSTOM_CVAR(Float, ggamma, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (NULL != screen)
	{
		screen->SetGamma();
	}
}

CUSTOM_CVAR(Float, bgamma, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (NULL != screen)
	{
		screen->SetGamma();
	}
}


// ---------------------------------------------------------------------------


extern id appCtrl;


namespace
{

CocoaWindow* CreateWindow(const NSUInteger styleMask)
{
	CocoaWindow* const window = [CocoaWindow alloc];
	[window initWithContentRect:NSMakeRect(0, 0, vid_defwidth, vid_defheight)
					  styleMask:styleMask
						backing:NSBackingStoreBuffered
						  defer:NO];
	[window setOpaque:YES];
	[window makeFirstResponder:appCtrl];
	[window setAcceptsMouseMovedEvents:YES];

	NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
	[nc addObserver:window
		   selector:@selector(frameDidChange:)
			   name:NSWindowDidEndLiveResizeNotification
			 object:nil];
	[nc addObserver:window
		   selector:@selector(frameDidChange:)
			   name:NSWindowDidMoveNotification
			 object:nil];

	return window;
}

NSOpenGLPixelFormat* CreatePixelFormat(const NSOpenGLPixelFormatAttribute profile)
{
	NSOpenGLPixelFormatAttribute attributes[16];
	size_t i = 0;

	attributes[i++] = NSOpenGLPFADoubleBuffer;
	attributes[i++] = NSOpenGLPFAColorSize;
	attributes[i++] = NSOpenGLPixelFormatAttribute(32);
	attributes[i++] = NSOpenGLPFADepthSize;
	attributes[i++] = NSOpenGLPixelFormatAttribute(24);
	attributes[i++] = NSOpenGLPFAStencilSize;
	attributes[i++] = NSOpenGLPixelFormatAttribute(8);
	attributes[i++] = NSOpenGLPFAOpenGLProfile;
	attributes[i++] = profile;
	
	if (!vid_autoswitch)
	{
		attributes[i++] = NSOpenGLPFAAllowOfflineRenderers;
	}

	attributes[i] = NSOpenGLPixelFormatAttribute(0);

	return [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
}

} // unnamed namespace


// ---------------------------------------------------------------------------


static SystemGLFrameBuffer* frameBuffer;


SystemGLFrameBuffer::SystemGLFrameBuffer(void*, const bool fullscreen)
: DFrameBuffer(vid_defwidth, vid_defheight)
, m_window(CreateWindow(STYLE_MASK_WINDOWED))
, m_fullscreen(false)
, m_hiDPI(false)
{
	SetFlash(0, 0);

	// Create OpenGL pixel format
	NSOpenGLPixelFormatAttribute defaultProfile = NSOpenGLProfileVersion3_2Core;

	if (NSAppKitVersionNumber < AppKit10_9)
	{
		// There is no support for OpenGL 3.3 before Mavericks
		defaultProfile = NSOpenGLProfileVersionLegacy;
	}
	else if (const char* const glversion = Args->CheckValue("-glversion"))
	{
		// Check for explicit version specified in command line
		const double version = strtod(glversion, nullptr) + 0.01;
		if (version < 3.3)
		{
			defaultProfile = NSOpenGLProfileVersionLegacy;
		}
	}

	NSOpenGLPixelFormat* pixelFormat = CreatePixelFormat(defaultProfile);

	if (nil == pixelFormat && NSOpenGLProfileVersion3_2Core == defaultProfile)
	{
		pixelFormat = CreatePixelFormat(NSOpenGLProfileVersionLegacy);
	}

	if (nil == pixelFormat)
	{
		I_FatalError("Cannot create OpenGL pixel format, graphics hardware is not supported");
	}

	// Create OpenGL context and view

	const NSRect contentRect = [m_window contentRectForFrameRect:[m_window frame]];
	NSOpenGLView* glView = [[CocoaView alloc] initWithFrame:contentRect
												pixelFormat:pixelFormat];
	[[glView openGLContext] makeCurrentContext];

	[m_window setContentView:glView];

	// Create table for system-wide gamma correction

	CGGammaValue gammaTable[GAMMA_TABLE_SIZE];
	uint32_t actualChannelSize;

	const CGError result = CGGetDisplayTransferByTable(kCGDirectMainDisplay, GAMMA_CHANNEL_SIZE,
		gammaTable, &gammaTable[GAMMA_CHANNEL_SIZE], &gammaTable[GAMMA_CHANNEL_SIZE * 2], &actualChannelSize);
	m_supportsGamma = kCGErrorSuccess == result && GAMMA_CHANNEL_SIZE == actualChannelSize;

	if (m_supportsGamma)
	{
		for (uint32_t i = 0; i < GAMMA_TABLE_SIZE; ++i)
		{
			m_originalGamma[i] = static_cast<uint16_t>(gammaTable[i] * 65535.0f);
		}
	}

	frameBuffer = this;

	FConsoleWindow::GetInstance().Show(false);
}

SystemGLFrameBuffer::~SystemGLFrameBuffer()
{
	NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
	[nc removeObserver:m_window
				  name:NSWindowDidMoveNotification
				object:nil];
	[nc removeObserver:m_window
				  name:NSWindowDidEndLiveResizeNotification
				object:nil];
}

bool SystemGLFrameBuffer::IsFullscreen()
{
	return m_fullscreen;
}

void SystemGLFrameBuffer::ToggleFullscreen(bool yes)
{
	SetMode(yes, m_hiDPI);
}


void SystemGLFrameBuffer::SetVSync(bool vsync)
{
	const GLint value = vsync ? 1 : 0;

	[[NSOpenGLContext currentContext] setValues:&value
								   forParameter:NSOpenGLCPSwapInterval];
}


void SystemGLFrameBuffer::SwapBuffers()
{
	[[NSOpenGLContext currentContext] flushBuffer];
}

void SystemGLFrameBuffer::SetGammaTable(uint16_t* table)
{
	if (m_supportsGamma)
	{
		CGGammaValue gammaTable[GAMMA_TABLE_SIZE];

		for (uint32_t i = 0; i < GAMMA_TABLE_SIZE; ++i)
		{
			gammaTable[i] = static_cast<CGGammaValue>(table[i] / 65535.0f);
		}

		CGSetDisplayTransferByTable(kCGDirectMainDisplay, GAMMA_CHANNEL_SIZE,
			gammaTable, &gammaTable[GAMMA_CHANNEL_SIZE], &gammaTable[GAMMA_CHANNEL_SIZE * 2]);
	}
}

void SystemGLFrameBuffer::ResetGammaTable()
{
	if (m_supportsGamma)
	{
		SetGammaTable(m_originalGamma);
	}
}


int SystemGLFrameBuffer::GetClientWidth()
{
	const int clientWidth = I_GetContentViewSize(m_window).width;
	return clientWidth > 0 ? clientWidth : GetWidth();
}

int SystemGLFrameBuffer::GetClientHeight()
{
	const int clientHeight = I_GetContentViewSize(m_window).height;
	return clientHeight > 0 ? clientHeight : GetHeight();
}


void SystemGLFrameBuffer::SetFullscreenMode()
{
	if (!m_fullscreen)
	{
		[m_window setLevel:LEVEL_FULLSCREEN];
		[m_window setStyleMask:STYLE_MASK_FULLSCREEN];

		[m_window setHidesOnDeactivate:YES];
	}

	const NSRect screenFrame = [[m_window screen] frame];
	[m_window setFrame:screenFrame display:YES];
}

void SystemGLFrameBuffer::SetWindowedMode()
{
	if (m_fullscreen)
	{
		[m_window setLevel:LEVEL_WINDOWED];
		[m_window setStyleMask:STYLE_MASK_WINDOWED];

		[m_window setHidesOnDeactivate:NO];
	}

	const bool isFrameValid = win_x >= 0 && win_y >= 0 && win_w > 320 && win_h > 200;
	const NSRect frameSize = isFrameValid
		? NSMakeRect(win_x, win_y, win_w, win_h)
		: NSMakeRect(0, 0, vid_defwidth, vid_defheight);

	[m_window setFrame:frameSize display:YES];
	[m_window enterFullscreenOnZoom];
	[m_window exitAppOnClose];
}

void SystemGLFrameBuffer::SetMode(const bool fullscreen, const bool hiDPI)
{
	NSOpenGLView* const glView = [m_window contentView];
	[glView setWantsBestResolutionOpenGLSurface:hiDPI];

	if (fullscreen)
	{
		SetFullscreenMode();
	}
	else
	{
		SetWindowedMode();
	}

	const NSSize viewSize = I_GetContentViewSize(m_window);

	glViewport(0, 0, static_cast<GLsizei>(viewSize.width), static_cast<GLsizei>(viewSize.height));
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	[[NSOpenGLContext currentContext] flushBuffer];

	[m_window updateTitle];

	if (![m_window isKeyWindow])
	{
		[m_window makeKeyAndOrderFront:nil];
	}

	m_fullscreen = fullscreen;
	m_hiDPI      = hiDPI;
}


void SystemGLFrameBuffer::UseHiDPI(const bool hiDPI)
{
	if (frameBuffer != nullptr)
	{
		frameBuffer->SetMode(frameBuffer->m_fullscreen, hiDPI);
	}
}

void SystemGLFrameBuffer::SetCursor(NSCursor* cursor)
{
	if (frameBuffer != nullptr)
	{
		NSWindow*  const window = frameBuffer->m_window;
		CocoaView* const view   = [window contentView];

		[view setCursor:cursor];
		[window invalidateCursorRectsForView:view];
	}
}

void SystemGLFrameBuffer::SetWindowVisible(bool visible)
{
	if (frameBuffer != nullptr)
	{
		if (visible)
		{
			[frameBuffer->m_window orderFront:nil];
		}
		else
		{
			[frameBuffer->m_window orderOut:nil];
		}

		I_SetNativeMouse(!visible);
	}
}

void SystemGLFrameBuffer::SetWindowTitle(const char* title)
{
	if (frameBuffer != nullptr)
	{
		NSString* const nsTitle = nullptr == title ? nil :
			[NSString stringWithCString:title encoding:NSISOLatin1StringEncoding];
		[frameBuffer->m_window setTitle:nsTitle];
	}
}


// ---------------------------------------------------------------------------


IVideo* Video;


// ---------------------------------------------------------------------------


void I_ShutdownGraphics()
{
	if (NULL != screen)
	{
		delete screen;
		screen = NULL;
	}

	delete Video;
	Video = NULL;
}

void I_InitGraphics()
{
	Video = new CocoaVideo;
	atterm(I_ShutdownGraphics);
}


// ---------------------------------------------------------------------------


EXTERN_CVAR(Int, vid_maxfps);
EXTERN_CVAR(Bool, cl_capfps);

// So Apple doesn't support POSIX timers and I can't find a good substitute short of
// having Objective-C Cocoa events or something like that.
void I_SetFPSLimit(int limit)
{
}

CUSTOM_CVAR(Bool, vid_hidpi, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	SystemGLFrameBuffer::UseHiDPI(self);
}


// ---------------------------------------------------------------------------


bool I_SetCursor(FTexture* cursorpic)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	NSCursor* cursor = nil;

	if (NULL != cursorpic && ETextureType::Null != cursorpic->UseType)
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

		uint8_t* buffer = [bitmapImageRep bitmapData];
		memset(buffer, 0, imagePitch * imageHeight);

		FBitmap bitmap(buffer, imagePitch, imageWidth, imageHeight);
		cursorpic->CopyTrueColorPixels(&bitmap, 0, 0);

		// Swap red and blue components in each pixel

		for (size_t i = 0; i < size_t(imageWidth * imageHeight); ++i)
		{
			const size_t offset = i * 4;

			const uint8_t temp    = buffer[offset    ];
			buffer[offset    ] = buffer[offset + 2];
			buffer[offset + 2] = temp;
		}

		// Create image from representation and set it as cursor

		NSData* imageData = [bitmapImageRep representationUsingType:NSPNGFileType
														 properties:[NSDictionary dictionary]];
		NSImage* cursorImage = [[NSImage alloc] initWithData:imageData];

		cursor = [[NSCursor alloc] initWithImage:cursorImage
										 hotSpot:NSMakePoint(0.0f, 0.0f)];
	}
	
	SystemGLFrameBuffer::SetCursor(cursor);
	
	[pool release];
	
	return true;
}


NSSize I_GetContentViewSize(const NSWindow* const window)
{
	const NSView* const view = [window contentView];
	const NSSize frameSize   = [view frame].size;

	return (vid_hidpi)
		? [view convertSizeToBacking:frameSize]
		: frameSize;
}

void I_SetMainWindowVisible(bool visible)
{
	SystemGLFrameBuffer::SetWindowVisible(visible);
}

// each platform has its own specific version of this function.
void I_SetWindowTitle(const char* title)
{
	SystemGLFrameBuffer::SetWindowTitle(title);
}
