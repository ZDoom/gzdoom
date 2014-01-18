/*
 ** iwadpicker_cocoa.mm
 **
 ** Implements Mac OS X native IWAD Picker.
 **
 **---------------------------------------------------------------------------
 ** Copyright 2010 Braden Obrzut
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

#include "d_main.h"
#include "version.h"
#include <Cocoa/Cocoa.h>

enum
{
	COLUMN_IWAD,
	COLUMN_GAME,

	NUM_COLUMNS
};

static const char* const tableHeaders[NUM_COLUMNS] = { "IWAD", "Game" };

// Class to convert the IWAD data into a form that Cocoa can use.
@interface IWADTableData : NSObject
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	<NSTableViewDataSource>
#endif
{
	NSMutableArray *data;
}

- (void)dealloc;
- (IWADTableData *)init:(WadStuff *) wads num:(int) numwads;

- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
@end

@implementation IWADTableData

- (void)dealloc
{
	[data release];

	[super dealloc];
}

- (IWADTableData *)init:(WadStuff *) wads num:(int) numwads
{
	data = [[NSMutableArray alloc] initWithCapacity:numwads];

	for(int i = 0;i < numwads;i++)
	{
		NSMutableDictionary *record = [[NSMutableDictionary alloc] initWithCapacity:NUM_COLUMNS];
		const char* filename = strrchr(wads[i].Path, '/');
		if(filename == NULL)
			filename = wads[i].Path;
		else
			filename++;
		[record setObject:[NSString stringWithUTF8String:filename] forKey:[NSString stringWithUTF8String:tableHeaders[COLUMN_IWAD]]];
		[record setObject:[NSString stringWithUTF8String:wads[i].Name] forKey:[NSString stringWithUTF8String:tableHeaders[COLUMN_GAME]]];
		[data addObject:record];
		[record release];
	}

	return self;
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
	return [data count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
	NSParameterAssert(rowIndex >= 0 && (unsigned int) rowIndex < [data count]);
	NSMutableDictionary *record = [data objectAtIndex:rowIndex];
	return [record objectForKey:[aTableColumn identifier]];
}

@end

// So we can listen for button actions and such we need to have an Obj-C class.
@interface IWADPicker : NSObject
{
	NSApplication *app;
	NSWindow *window;
	NSButton *okButton;
	NSButton *cancelButton;
	bool cancelled;
}

- (void)buttonPressed:(id) sender;
- (void)doubleClicked:(id) sender;
- (void)makeLabel:(NSTextField *)label withString:(const char*) str;
- (int)pickIWad:(WadStuff *)wads num:(int) numwads showWindow:(bool) showwin defaultWad:(int) defaultiwad;
@end

@implementation IWADPicker

- (void)buttonPressed:(id) sender
{
	if(sender == cancelButton)
		cancelled = true;

	[window orderOut:self];
	[app stopModal];
}

- (void)doubleClicked:(id) sender
{
	if ([sender clickedRow] >= 0)
	{
		[window orderOut:self];
		[app stopModal];
	}
}

// Apparently labels in Cocoa are uneditable text fields, so lets make this a
// little more automated.
- (void)makeLabel:(NSTextField *)label withString:(const char*) str
{
	[label setStringValue:[NSString stringWithUTF8String:str]];
	[label setBezeled:NO];
	[label setDrawsBackground:NO];
	[label setEditable:NO];
	[label setSelectable:NO];
}

- (int)pickIWad:(WadStuff *)wads num:(int) numwads showWindow:(bool) showwin defaultWad:(int) defaultiwad
{
	cancelled = false;

	app = [NSApplication sharedApplication];
	id windowTitle = [NSString stringWithFormat:@"%s %s", GAMESIG, GetVersionString()];

	NSRect frame = NSMakeRect(0, 0, 440, 450);
	window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
	[window setTitle:windowTitle];

	NSTextField *description = [[NSTextField alloc] initWithFrame:NSMakeRect(22, 379, 412, 50)];
	[self makeLabel:description withString:"GZDoom found more than one IWAD\nSelect from the list below to determine which one to use:"];
	[[window contentView] addSubview:description];
	[description release];

	// Commented out version would account for an additional parameters box.
	//NSScrollView *iwadScroller = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 103, 412, 288)];
	NSScrollView *iwadScroller = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 50, 412, 341)];
	NSTableView *iwadTable = [[NSTableView alloc] initWithFrame:[iwadScroller bounds]];
	IWADTableData *tableData = [[IWADTableData alloc] init:wads num:numwads];
	for(int i = 0;i < NUM_COLUMNS;i++)
	{
		NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:[NSString stringWithUTF8String:tableHeaders[i]]];
		[[column headerCell] setStringValue:[column identifier]];
		if(i == 0)
			[column setMaxWidth:110];
		[column setEditable:NO];
		[column setResizingMask:NSTableColumnAutoresizingMask];
		[iwadTable addTableColumn:column];
		[column release];
	}
	[iwadScroller setDocumentView:iwadTable];
	[iwadScroller setHasVerticalScroller:YES];
	[iwadTable setDataSource:tableData];
	[iwadTable sizeToFit];
	[iwadTable setDoubleAction:@selector(doubleClicked:)];
	[iwadTable setTarget:self];
	NSIndexSet *selection = [[NSIndexSet alloc] initWithIndex:defaultiwad];
	[iwadTable selectRowIndexes:selection byExtendingSelection:NO];
	[selection release];
	[iwadTable scrollRowToVisible:defaultiwad];
	[[window contentView] addSubview:iwadScroller];
	[iwadTable release];
	[iwadScroller release];

	/*NSTextField *additionalParametersLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(17, 78, 144, 17)];
	[self makeLabel:additionalParametersLabel:"Additional Parameters"];
	[[window contentView] addSubview:additionalParametersLabel];
	NSTextField *additionalParameters = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 48, 360, 22)];
	[[window contentView] addSubview:additionalParameters];*/

	// Doesn't look like the SDL version implements this so lets not show it.
	/*NSButton *dontAsk = [[NSButton alloc] initWithFrame:NSMakeRect(18, 18, 178, 18)];
	[dontAsk setTitle:[NSString stringWithCString:"Don't ask me this again"]];
	[dontAsk setButtonType:NSSwitchButton];
	[dontAsk setState:(showwin ? NSOffState : NSOnState)];
	[[window contentView] addSubview:dontAsk];*/

	okButton = [[NSButton alloc] initWithFrame:NSMakeRect(236, 12, 96, 32)];
	[okButton setTitle:[NSString stringWithUTF8String:"OK"]];
	[okButton setBezelStyle:NSRoundedBezelStyle];
	[okButton setAction:@selector(buttonPressed:)];
	[okButton setTarget:self];
	[okButton setKeyEquivalent:@"\r"];
	[[window contentView] addSubview:okButton];

	cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(332, 12, 96, 32)];
	[cancelButton setTitle:[NSString stringWithUTF8String:"Cancel"]];
	[cancelButton setBezelStyle:NSRoundedBezelStyle];
	[cancelButton setAction:@selector(buttonPressed:)];
	[cancelButton setTarget:self];
	[cancelButton setKeyEquivalent:@"\033"];
	[[window contentView] addSubview:cancelButton];

	[window center];
	[app runModalForWindow:window];

	[window release];
	[okButton release];
	[cancelButton release];

	return cancelled ? -1 : [iwadTable selectedRow];
}

@end

// Simple wrapper so we can call this from outside.
int I_PickIWad_Cocoa (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	IWADPicker *picker = [IWADPicker alloc];
	int ret = [picker pickIWad:wads num:numwads showWindow:showwin defaultWad:defaultiwad];
	[picker release];
	return ret;
}
