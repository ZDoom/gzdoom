/*
 ** st_console.mm
 **
 **---------------------------------------------------------------------------
 ** Copyright 2015 Alexey Lysiuk
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

#include "d_main.h"
#include "i_system.h"
#include "st_console.h"
#include "v_text.h"
#include "version.h"


static NSColor* RGB(const BYTE red, const BYTE green, const BYTE blue)
{
	return [NSColor colorWithCalibratedRed:red   / 255.0f
									 green:green / 255.0f
									  blue:blue  / 255.0f
									 alpha:1.0f];
}

static NSColor* RGB(const PalEntry& color)
{
	return RGB(color.r, color.g, color.b);
}

static NSColor* RGB(const DWORD color)
{
	return RGB(PalEntry(color));
}


static const CGFloat PROGRESS_BAR_HEIGHT = 18.0f;
static const CGFloat NET_VIEW_HEIGHT     = 88.0f;


FConsoleWindow::FConsoleWindow()
: m_window([NSWindow alloc])
, m_textView([NSTextView alloc])
, m_scrollView([NSScrollView alloc])
, m_progressBar(nil)
, m_netView(nil)
, m_netMessageText(nil)
, m_netCountText(nil)
, m_netProgressBar(nil)
, m_netAbortButton(nil)
, m_characterCount(0)
, m_netCurPos(0)
, m_netMaxPos(0)
{
	const CGFloat initialWidth  = 512.0f;
	const CGFloat initialHeight = 384.0f;
	const NSRect initialRect = NSMakeRect(0.0f, 0.0f, initialWidth, initialHeight);

	[m_textView initWithFrame:initialRect];
	[m_textView setEditable:NO];
	[m_textView setBackgroundColor:RGB(70, 70, 70)];
	[m_textView setMinSize:NSMakeSize(0.0f, initialHeight)];
	[m_textView setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
	[m_textView setVerticallyResizable:YES];
	[m_textView setHorizontallyResizable:NO];
	[m_textView setAutoresizingMask:NSViewWidthSizable];

	NSTextContainer* const textContainer = [m_textView textContainer];
	[textContainer setContainerSize:NSMakeSize(initialWidth, FLT_MAX)];
	[textContainer setWidthTracksTextView:YES];

	[m_scrollView initWithFrame:NSMakeRect(0.0f, 0.0f, initialWidth, initialHeight)];
	[m_scrollView setBorderType:NSNoBorder];
	[m_scrollView setHasVerticalScroller:YES];
	[m_scrollView setHasHorizontalScroller:NO];
	[m_scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[m_scrollView setDocumentView:m_textView];

	NSString* const title = [NSString stringWithFormat:@"%s %s - Console", GAMESIG, GetVersionString()];

	[m_window initWithContentRect:initialRect
						styleMask:NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask
						  backing:NSBackingStoreBuffered
							defer:NO];
	[m_window setMinSize:[m_window frame].size];
	[m_window setShowsResizeIndicator:NO];
	[m_window setTitle:title];
	[m_window center];
	[m_window exitAppOnClose];

	[[m_window contentView] addSubview:m_scrollView];

	[m_window makeKeyAndOrderFront:nil];
}


static FConsoleWindow* s_instance;


void FConsoleWindow::CreateInstance()
{
	assert(NULL == s_instance);
	s_instance = new FConsoleWindow;
}

void FConsoleWindow::DeleteInstance()
{
	assert(NULL != s_instance);
	delete s_instance;
	s_instance = NULL;
}

FConsoleWindow& FConsoleWindow::GetInstance()
{
	assert(NULL != s_instance);
	return *s_instance;
}


void FConsoleWindow::Show(const bool visible)
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

void FConsoleWindow::ShowFatalError(const char* const message)
{
	SetProgressBar(false);
	NetDone();

	const CGFloat textViewWidth = [m_scrollView frame].size.width;

	ExpandTextView(-32.0f);

	NSButton* quitButton = [[NSButton alloc] initWithFrame:NSMakeRect(textViewWidth - 76.0f, 0.0f, 72.0f, 30.0f)];
	[quitButton setAutoresizingMask:NSViewMinXMargin];
	[quitButton setBezelStyle:NSRoundedBezelStyle];
	[quitButton setTitle:@"Quit"];
	[quitButton setKeyEquivalent:@"\r"];
	[quitButton setTarget:NSApp];
	[quitButton setAction:@selector(terminate:)];

	NSView* quitPanel = [[NSView alloc] initWithFrame:NSMakeRect(0.0f, 0.0f, textViewWidth, 32.0f)];
	[quitPanel setAutoresizingMask:NSViewWidthSizable];
	[quitPanel addSubview:quitButton];

	[[m_window contentView] addSubview:quitPanel];
	[m_window orderFront:nil];

	AddText(PalEntry(255,   0,   0), "\nExecution could not continue.\n");
	AddText(PalEntry(255, 255, 170), message);
	AddText("\n");

	[NSApp runModalForWindow:m_window];
}


void FConsoleWindow::AddText(const char* message)
{
	PalEntry color(223, 223, 223);

	char buffer[1024] = {};
	size_t pos = 0;
	bool reset = false;

	while (*message != '\0')
	{
		if ((TEXTCOLOR_ESCAPE == *message && 0 != pos)
			|| (pos == sizeof buffer - 1)
			|| reset)
		{
			buffer[pos] = '\0';
			pos = 0;
			reset = false;

			AddText(color, buffer);
		}

#define CHECK_BUFFER_SPACE \
	if (pos >= sizeof buffer - 3) { reset = true; continue; }

		if (TEXTCOLOR_ESCAPE == *message)
		{
			const BYTE* colorID = reinterpret_cast<const BYTE*>(message) + 1;
			if ('\0' == colorID)
			{
				break;
			}

			const EColorRange range = V_ParseFontColor(colorID, CR_UNTRANSLATED, CR_YELLOW);

			if (range != CR_UNDEFINED)
			{
				color = V_LogColorFromColorRange(range);
			}

			message += 2;
		}
		else if (0x1d == *message) // Opening bar character
		{
			CHECK_BUFFER_SPACE;

			// Insert BOX DRAWINGS LIGHT LEFT AND HEAVY RIGHT
			buffer[pos++] = '\xe2';
			buffer[pos++] = '\x95';
			buffer[pos++] = '\xbc';
			++message;
		}
		else if (0x1e == *message) // Middle bar character
		{
			CHECK_BUFFER_SPACE;

			// Insert BOX DRAWINGS HEAVY HORIZONTAL
			buffer[pos++] = '\xe2';
			buffer[pos++] = '\x94';
			buffer[pos++] = '\x81';
			++message;
		}
		else if (0x1f == *message) // Closing bar character
		{
			CHECK_BUFFER_SPACE;

			// Insert BOX DRAWINGS HEAVY LEFT AND LIGHT RIGHT
			buffer[pos++] = '\xe2';
			buffer[pos++] = '\x95';
			buffer[pos++] = '\xbe';
			++message;
		}
		else
		{
			buffer[pos++] = *message++;
		}

#undef CHECK_BUFFER_SPACE
	}

	if (0 != pos)
	{
		buffer[pos] = '\0';

		AddText(color, buffer);
	}

	if ([m_window isVisible])
	{
		[m_textView scrollRangeToVisible:NSMakeRange(m_characterCount, 0)];

		[[NSRunLoop currentRunLoop] limitDateForMode:NSDefaultRunLoopMode];
	}
}

void FConsoleWindow::AddText(const PalEntry& color, const char* const message)
{
	NSString* const text = [NSString stringWithUTF8String:message];

	NSDictionary* const attributes = [NSDictionary dictionaryWithObjectsAndKeys:
									  [NSFont systemFontOfSize:14.0f], NSFontAttributeName,
									  RGB(color), NSForegroundColorAttributeName,
									  nil];

	NSAttributedString* const formattedText =
	[[NSAttributedString alloc] initWithString:text
									attributes:attributes];
	[[m_textView textStorage] appendAttributedString:formattedText];

	m_characterCount += [text length];
}


void FConsoleWindow::SetTitleText()
{
	static const CGFloat TITLE_TEXT_HEIGHT = 32.0f;

	NSRect textViewFrame = [m_scrollView frame];
	textViewFrame.size.height -= TITLE_TEXT_HEIGHT;
	[m_scrollView setFrame:textViewFrame];

	const NSRect titleTextRect = NSMakeRect(
		0.0f,
		textViewFrame.origin.y + textViewFrame.size.height,
		textViewFrame.size.width,
		TITLE_TEXT_HEIGHT);

	NSTextField* titleText = [[NSTextField alloc] initWithFrame:titleTextRect];
	[titleText setStringValue:[NSString stringWithUTF8String:DoomStartupInfo.Name]];
	[titleText setAlignment:NSCenterTextAlignment];
	[titleText setTextColor:RGB(DoomStartupInfo.FgColor)];
	[titleText setBackgroundColor:RGB(DoomStartupInfo.BkColor)];
	[titleText setFont:[NSFont fontWithName:@"Trebuchet MS Bold" size:18.0f]];
	[titleText setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
	[titleText setSelectable:NO];
	[titleText setBordered:NO];

	[[m_window contentView] addSubview:titleText];
}

void FConsoleWindow::SetProgressBar(const bool visible)
{
	if (  (!visible && nil == m_progressBar)
		|| (visible && nil != m_progressBar))
	{
		return;
	}

	if (visible)
	{
		ExpandTextView(-PROGRESS_BAR_HEIGHT);

		m_progressBar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(2.0f, 0.0f, 508.0f, 16.0f)];
		[m_progressBar setIndeterminate:NO];
		[m_progressBar setAutoresizingMask:NSViewWidthSizable];

		[[m_window contentView] addSubview:m_progressBar];
	}
	else
	{
		ExpandTextView(PROGRESS_BAR_HEIGHT);

		[m_progressBar removeFromSuperview];
		[m_progressBar release];
		m_progressBar = nil;
	}
}


void FConsoleWindow::ExpandTextView(const float height)
{
	NSRect textFrame = [m_scrollView frame];
	textFrame.origin.y    -= height;
	textFrame.size.height += height;
	[m_scrollView setFrame:textFrame];
}


void FConsoleWindow::Progress(const int current, const int maximum)
{
	if (nil == m_progressBar)
	{
		return;
	}

	static unsigned int previousTime = I_MSTime();
	unsigned int currentTime = I_MSTime();

	if (currentTime - previousTime > 33) // approx. 30 FPS
	{
		previousTime = currentTime;

		[m_progressBar setMaxValue:maximum];
		[m_progressBar setDoubleValue:current];

		[[NSRunLoop currentRunLoop] limitDateForMode:NSDefaultRunLoopMode];
	}
}


void FConsoleWindow::NetInit(const char* const message, const int playerCount)
{
	if (nil == m_netView)
	{
		SetProgressBar(false);
		ExpandTextView(-NET_VIEW_HEIGHT);

		// Message like 'Waiting for players' or 'Contacting host'
		m_netMessageText = [[NSTextField alloc] initWithFrame:NSMakeRect(12.0f, 64.0f, 400.0f, 16.0f)];
		[m_netMessageText setAutoresizingMask:NSViewWidthSizable];
		[m_netMessageText setDrawsBackground:NO];
		[m_netMessageText setSelectable:NO];
		[m_netMessageText setBordered:NO];

		// Text with connected/total players count
		m_netCountText = [[NSTextField alloc] initWithFrame:NSMakeRect(428.0f, 64.0f, 72.0f, 16.0f)];
		[m_netCountText setAutoresizingMask:NSViewMinXMargin];
		[m_netCountText setAlignment:NSRightTextAlignment];
		[m_netCountText setDrawsBackground:NO];
		[m_netCountText setSelectable:NO];
		[m_netCountText setBordered:NO];

		// Connection progress
		m_netProgressBar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(12.0f, 40.0f, 488.0f, 16.0f)];
		[m_netProgressBar setAutoresizingMask:NSViewWidthSizable];
		[m_netProgressBar setMaxValue:playerCount];

		if (0 == playerCount)
		{
			// Joining game
			[m_netProgressBar setIndeterminate:YES];
			[m_netProgressBar startAnimation:nil];
		}
		else
		{
			// Hosting game
			[m_netProgressBar setIndeterminate:NO];
		}

		// Cancel network game button
		m_netAbortButton = [[NSButton alloc] initWithFrame:NSMakeRect(432.0f, 8.0f, 72.0f, 28.0f)];
		[m_netAbortButton setAutoresizingMask:NSViewMinXMargin];
		[m_netAbortButton setBezelStyle:NSRoundedBezelStyle];
		[m_netAbortButton setTitle:@"Cancel"];
		[m_netAbortButton setKeyEquivalent:@"\r"];
		[m_netAbortButton setTarget:NSApp];
		[m_netAbortButton setAction:@selector(terminate:)];

		// Panel for controls above
		m_netView = [[NSView alloc] initWithFrame:NSMakeRect(0.0f, 0.0f, 512.0f, NET_VIEW_HEIGHT)];
		[m_netView setAutoresizingMask:NSViewWidthSizable];
		[m_netView addSubview:m_netMessageText];
		[m_netView addSubview:m_netCountText];
		[m_netView addSubview:m_netProgressBar];
		[m_netView addSubview:m_netAbortButton];

		NSRect windowRect = [m_window frame];
		windowRect.origin.y    -= NET_VIEW_HEIGHT;
		windowRect.size.height += NET_VIEW_HEIGHT;

		[m_window setFrame:windowRect display:YES];
		[[m_window contentView] addSubview:m_netView];
	}

	[m_netMessageText setStringValue:[NSString stringWithUTF8String:message]];

	m_netCurPos = 0;
	m_netMaxPos = playerCount;

	NetProgress(1); // You always know about yourself
}

void FConsoleWindow::NetProgress(const int count)
{
	if (0 == count)
	{
		++m_netCurPos;
	}
	else
	{
		m_netCurPos = count;
	}

	if (nil == m_netView)
	{
		return;
	}

	if (m_netMaxPos > 1)
	{
		[m_netCountText setStringValue:[NSString stringWithFormat:@"%d / %d", m_netCurPos, m_netMaxPos]];
		[m_netProgressBar setDoubleValue:MIN(m_netCurPos, m_netMaxPos)];
	}
}

void FConsoleWindow::NetDone()
{
	if (nil != m_netView)
	{
		ExpandTextView(NET_VIEW_HEIGHT);

		[m_netView removeFromSuperview];
		[m_netView release];
		m_netView = nil;

		// Released by m_netView
		m_netMessageText = nil;
		m_netCountText = nil;
		m_netProgressBar = nil;
		m_netAbortButton = nil;
	}
}
