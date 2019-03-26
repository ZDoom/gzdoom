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

#ifdef HAVE_VULKAN
#define VK_USE_PLATFORM_MACOS_MVK
#include "volk/volk.h"
#endif

#include "i_common.h"

#include "bitmap.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "hardware.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_png.h"
#include "r_renderer.h"
#include "swrenderer/r_swrenderer.h"
#include "st_console.h"
#include "v_text.h"
#include "version.h"
#include "doomerrors.h"

#include "gl/system/gl_framebuffer.h"
#include "vulkan/system/vk_framebuffer.h"


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
EXTERN_CVAR(Int,  vid_backend)
EXTERN_CVAR(Bool, vk_debug)

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


@interface OpenGLCocoaView : NSOpenGLView
{
	NSCursor* m_cursor;
}

- (void)setCursor:(NSCursor*)cursor;

@end


@implementation OpenGLCocoaView

- (void)drawRect:(NSRect)dirtyRect
{
	[NSColor.blackColor setFill];
	NSRectFill(dirtyRect);
}

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


@interface VulkanCocoaView : NSView
{
	NSCursor* m_cursor;
}

- (void)setCursor:(NSCursor*)cursor;

@end


@implementation VulkanCocoaView

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

+(Class) layerClass
{
	return NSClassFromString(@"CAMetalLayer");
}

-(CALayer*) makeBackingLayer
{
	return [self.class.layerClass layer];
}

@end


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

NSOpenGLPixelFormat* CreatePixelFormat()
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
	attributes[i++] = NSOpenGLProfileVersion3_2Core;
	
	if (!vid_autoswitch)
	{
		attributes[i++] = NSOpenGLPFAAllowOfflineRenderers;
	}

	attributes[i] = NSOpenGLPixelFormatAttribute(0);

	return [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
}

void SetupOpenGLView(CocoaWindow* window)
{
	NSOpenGLPixelFormat* pixelFormat = CreatePixelFormat();

	if (nil == pixelFormat)
	{
		I_FatalError("Cannot create OpenGL pixel format, graphics hardware is not supported");
	}

	// Create OpenGL context and view

	const NSRect contentRect = [window contentRectForFrameRect:[window frame]];
	OpenGLCocoaView* glView = [[OpenGLCocoaView alloc] initWithFrame:contentRect
														 pixelFormat:pixelFormat];
	[[glView openGLContext] makeCurrentContext];

	[window setContentView:glView];
}

} // unnamed namespace


// ---------------------------------------------------------------------------


class CocoaVideo : public IVideo
{
public:
	CocoaVideo()
	{
		ms_isVulkanEnabled = vid_backend == 0 && NSAppKitVersionNumber >= 1404; // NSAppKitVersionNumber10_11
	}

	~CocoaVideo()
	{
		delete m_vulkanDevice;

		ms_window = nil;
	}

	virtual DFrameBuffer* CreateFrameBuffer() override
	{
		assert(ms_window == nil);
		ms_window = CreateWindow(STYLE_MASK_WINDOWED);

		SystemBaseFrameBuffer *fb = nullptr;

		if (ms_isVulkanEnabled)
		{
			const NSRect contentRect = [ms_window contentRectForFrameRect:[ms_window frame]];

			NSView* vulkanView = [[VulkanCocoaView alloc] initWithFrame:contentRect];
			vulkanView.wantsLayer = YES;
			vulkanView.layer.backgroundColor = NSColor.blackColor.CGColor;

			[ms_window setContentView:vulkanView];

			if (!vk_debug)
			{
				// Limit MoltenVK logging to errors only
				setenv("MVK_CONFIG_LOG_LEVEL", "1", 0);
			}

			if (!vid_autoswitch)
			{
				// CVAR from pre-Vulkan era has a priority over vk_device selection
				setenv("MVK_CONFIG_FORCE_LOW_POWER_GPU", "1", 0);
			}

			try
			{
				m_vulkanDevice = new VulkanDevice();
				fb = new VulkanFrameBuffer(nullptr, fullscreen, m_vulkanDevice);
			}
			catch (std::exception const&)
			{
				ms_isVulkanEnabled = false;

				SetupOpenGLView(ms_window);
			}
		}
		else
		{
			SetupOpenGLView(ms_window);
		}

		if (fb == nullptr)
		{
			fb = new OpenGLRenderer::OpenGLFrameBuffer(0, fullscreen);
		}

		fb->SetWindow(ms_window);
		fb->SetMode(fullscreen, vid_hidpi);
		fb->SetSize(fb->GetClientWidth(), fb->GetClientHeight());

		// This lame hack is a temporary workaround for strange performance issues
		// with fullscreen window and Core Animation's Metal layer
		// It is somehow related to initial window level and flags
		// Toggling fullscreen -> window -> fullscreen mysteriously solves the problem
		if (ms_isVulkanEnabled && fullscreen)
		{
			fb->SetMode(false, vid_hidpi);
			fb->SetMode(true, vid_hidpi);
		}

		return fb;
	}

	static CocoaWindow* GetWindow()
	{
		return ms_window;
	}

private:
	VulkanDevice *m_vulkanDevice = nullptr;

	static CocoaWindow* ms_window;

	static bool ms_isVulkanEnabled;
};


CocoaWindow* CocoaVideo::ms_window;

bool CocoaVideo::ms_isVulkanEnabled;


// ---------------------------------------------------------------------------


static SystemBaseFrameBuffer* frameBuffer;


SystemBaseFrameBuffer::SystemBaseFrameBuffer(void*, const bool fullscreen)
: DFrameBuffer(vid_defwidth, vid_defheight)
, m_fullscreen(false)
, m_hiDPI(false)
, m_window(nullptr)
{
	assert(frameBuffer == nullptr);
	frameBuffer = this;

	FConsoleWindow::GetInstance().Show(false);
}

SystemBaseFrameBuffer::~SystemBaseFrameBuffer()
{
	assert(frameBuffer == this);
	frameBuffer = nullptr;

	NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
	[nc removeObserver:m_window
				  name:NSWindowDidMoveNotification
				object:nil];
	[nc removeObserver:m_window
				  name:NSWindowDidEndLiveResizeNotification
				object:nil];
}

bool SystemBaseFrameBuffer::IsFullscreen()
{
	return m_fullscreen;
}

void SystemBaseFrameBuffer::ToggleFullscreen(bool yes)
{
	SetMode(yes, m_hiDPI);
}

void SystemBaseFrameBuffer::SetWindowSize(int width, int height)
{
	if (width < MINIMUM_WIDTH || height < MINIMUM_HEIGHT)
	{
		return;
	}

	if (fullscreen)
	{
		// Enter windowed mode in order to calculate title bar height
		fullscreen = false;
		SetMode(false, m_hiDPI);
	}

	win_w = width;
	win_h = height + GetTitleBarHeight();

	SetMode(false, m_hiDPI);

	[m_window center];
}

int SystemBaseFrameBuffer::GetTitleBarHeight() const
{
	const NSRect windowFrame = [m_window frame];
	const NSRect contentFrame = [m_window contentRectForFrameRect:windowFrame];
	const int titleBarHeight = windowFrame.size.height - contentFrame.size.height;

	return titleBarHeight;
}


int SystemBaseFrameBuffer::GetClientWidth()
{
	const int clientWidth = I_GetContentViewSize(m_window).width;
	return clientWidth > 0 ? clientWidth : GetWidth();
}

int SystemBaseFrameBuffer::GetClientHeight()
{
	const int clientHeight = I_GetContentViewSize(m_window).height;
	return clientHeight > 0 ? clientHeight : GetHeight();
}


void SystemBaseFrameBuffer::SetFullscreenMode()
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

void SystemBaseFrameBuffer::SetWindowedMode()
{
	if (m_fullscreen)
	{
		[m_window setLevel:LEVEL_WINDOWED];
		[m_window setStyleMask:STYLE_MASK_WINDOWED];

		[m_window setHidesOnDeactivate:NO];
	}

	const int minimumFrameWidth  = MINIMUM_WIDTH;
	const int minimumFrameHeight = MINIMUM_HEIGHT + GetTitleBarHeight();
	const NSSize minimumFrameSize = NSMakeSize(minimumFrameWidth, minimumFrameHeight);
	[m_window setMinSize:minimumFrameSize];

	const bool isFrameValid = win_x != -1 && win_y != -1
		&& win_w >= minimumFrameWidth && win_h >= minimumFrameHeight;

	if (!isFrameValid)
	{
		const NSRect screenSize = [[NSScreen mainScreen] frame];
		win_x = screenSize.origin.x + screenSize.size.width  / 10;
		win_y = screenSize.origin.y + screenSize.size.height / 10;
		win_w = screenSize.size.width  * 8 / 10;
		win_h = screenSize.size.height * 8 / 10 + GetTitleBarHeight();
	}

	const NSRect frameSize = NSMakeRect(win_x, win_y, win_w, win_h);
	[m_window setFrame:frameSize display:YES];
	[m_window enterFullscreenOnZoom];
	[m_window exitAppOnClose];
}

void SystemBaseFrameBuffer::SetMode(const bool fullscreen, const bool hiDPI)
{
	assert(m_window.screen != nil);
	assert(m_window.contentView.layer != nil);
	[m_window.contentView layer].contentsScale = hiDPI ? m_window.screen.backingScaleFactor : 1.0;

	if (fullscreen)
	{
		SetFullscreenMode();
	}
	else
	{
		SetWindowedMode();
	}

	[m_window updateTitle];

	if (![m_window isKeyWindow])
	{
		[m_window makeKeyAndOrderFront:nil];
	}

	m_fullscreen = fullscreen;
	m_hiDPI      = hiDPI;
}


void SystemBaseFrameBuffer::UseHiDPI(const bool hiDPI)
{
	if (frameBuffer != nullptr)
	{
		frameBuffer->SetMode(frameBuffer->m_fullscreen, hiDPI);
	}
}

void SystemBaseFrameBuffer::SetCursor(NSCursor* cursor)
{
	if (frameBuffer != nullptr)
	{
		NSWindow* const window = frameBuffer->m_window;
		id view = [window contentView];

		[view setCursor:cursor];
		[window invalidateCursorRectsForView:view];
	}
}

void SystemBaseFrameBuffer::SetWindowVisible(bool visible)
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

void SystemBaseFrameBuffer::SetWindowTitle(const char* title)
{
	if (frameBuffer != nullptr)
	{
		NSString* const nsTitle = nullptr == title ? nil :
			[NSString stringWithCString:title encoding:NSISOLatin1StringEncoding];
		[frameBuffer->m_window setTitle:nsTitle];
	}
}


// ---------------------------------------------------------------------------


SystemGLFrameBuffer::SystemGLFrameBuffer(void *hMonitor, bool fullscreen)
: SystemBaseFrameBuffer(hMonitor, fullscreen)
{
}


void SystemGLFrameBuffer::SetVSync(bool vsync)
{
	const GLint value = vsync ? 1 : 0;

	[[NSOpenGLContext currentContext] setValues:&value
								   forParameter:NSOpenGLCPSwapInterval];
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

	[m_window updateTitle];

	if (![m_window isKeyWindow])
	{
		[m_window makeKeyAndOrderFront:nil];
	}

	m_fullscreen = fullscreen;
	m_hiDPI      = hiDPI;
}


void SystemGLFrameBuffer::SwapBuffers()
{
	[[NSOpenGLContext currentContext] flushBuffer];
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

CUSTOM_CVAR(Bool, vid_hidpi, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	SystemBaseFrameBuffer::UseHiDPI(self);
}


// ---------------------------------------------------------------------------


bool I_SetCursor(FTexture *cursorpic)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	NSCursor* cursor = nil;

	if (NULL != cursorpic && cursorpic->isValid())
	{
		// Create bitmap image representation
		
		auto sbuffer = cursorpic->CreateTexBuffer(0);

		const NSInteger imageWidth  = sbuffer.mWidth;
		const NSInteger imageHeight = sbuffer.mHeight;
		const NSInteger imagePitch  = sbuffer.mWidth * 4;

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
		memcpy(buffer, sbuffer.mBuffer, imagePitch * imageHeight);

		// Swap red and blue components in each pixel

		for (size_t i = 0; i < size_t(imageWidth * imageHeight); ++i)
		{
			const size_t offset = i * 4;
			std::swap(buffer[offset    ], buffer[offset + 2]);
		}

		// Create image from representation and set it as cursor

		NSData* imageData = [bitmapImageRep representationUsingType:NSPNGFileType
														 properties:[NSDictionary dictionary]];
		NSImage* cursorImage = [[NSImage alloc] initWithData:imageData];

		cursor = [[NSCursor alloc] initWithImage:cursorImage
										 hotSpot:NSMakePoint(0.0f, 0.0f)];
	}
	
	SystemBaseFrameBuffer::SetCursor(cursor);
	
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
	SystemBaseFrameBuffer::SetWindowVisible(visible);
}

// each platform has its own specific version of this function.
void I_SetWindowTitle(const char* title)
{
	SystemBaseFrameBuffer::SetWindowTitle(title);
}


#ifdef HAVE_VULKAN
void I_GetVulkanDrawableSize(int *width, int *height)
{
	NSWindow* const window = CocoaVideo::GetWindow();
	assert(window != nil);

	const NSSize size = I_GetContentViewSize(window);

	if (width != nullptr)
	{
		*width = int(size.width);
	}

	if (height != nullptr)
	{
		*height = int(size.height);
	}
}

bool I_GetVulkanPlatformExtensions(unsigned int *count, const char **names)
{
	static const char* extensions[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_MVK_MACOS_SURFACE_EXTENSION_NAME
	};
	static const unsigned int extensionCount = static_cast<unsigned int>(sizeof extensions / sizeof extensions[0]);

	if (count == nullptr && names == nullptr)
	{
		return false;
	}
	else if (names == nullptr)
	{
		*count = extensionCount;
		return true;
	}
	else
	{
		const bool result = *count >= extensionCount;
		*count = std::min(*count, extensionCount);

		for (unsigned int i = 0; i < *count; ++i)
		{
			names[i] = extensions[i];
		}

		return result;
	}
}

bool I_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR *surface)
{
	VkMacOSSurfaceCreateInfoMVK windowCreateInfo;
	windowCreateInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
	windowCreateInfo.pNext = nullptr;
	windowCreateInfo.flags = 0;
	windowCreateInfo.pView = [[CocoaVideo::GetWindow() contentView] layer];

	const VkResult result = vkCreateMacOSSurfaceMVK(instance, &windowCreateInfo, nullptr, surface);
	return result == VK_SUCCESS;
}
#endif
