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
{
	NSMutableArray *data;
}

- (IWADTableData *)init:(WadStuff *) wads:(int) numwads;

- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
@end

@implementation IWADTableData

- (IWADTableData *)init:(WadStuff *) wads:(int) numwads
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
		[record setObject:[NSString stringWithCString:filename] forKey:[NSString stringWithCString:tableHeaders[COLUMN_IWAD]]];
		[record setObject:[NSString stringWithCString:IWADInfos[wads[i].Type].Name] forKey:[NSString stringWithCString:tableHeaders[COLUMN_GAME]]];
		[data addObject:record];
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
@interface IWADPicker : NSResponder
{
	NSApplication *app;
	NSWindow *window;
	NSButton *okButton;
	NSButton *cancelButton;
	bool cancelled;
}

- (void)buttonPressed:(id) sender;
- (void)makeLabel:(NSTextField *)label:(const char*) str;
- (int)pickIWad:(WadStuff *)wads:(int) numwads:(bool) showwin:(int) defaultiwad;
@end

@implementation IWADPicker

- (void)buttonPressed:(id) sender;
{
	if(sender == cancelButton)
		cancelled = true;

	[window orderOut:self];
	[app stopModal];
}

// Apparently labels in Cocoa are uneditable text fields, so lets make this a
// little more automated.
- (void)makeLabel:(NSTextField *)label:(const char*) str
{
	[label setStringValue:[NSString stringWithCString:str]];
	[label setBezeled:NO];
	[label setDrawsBackground:NO];
	[label setEditable:NO];
	[label setSelectable:NO];
}

- (int)pickIWad:(WadStuff *)wads:(int) numwads:(bool) showwin:(int) defaultiwad
{
	cancelled = false;

	app = [NSApplication sharedApplication];
	id windowTitle = [NSString stringWithCString:GAMESIG " " DOTVERSIONSTR ": Select an IWAD to use"];

	NSRect frame = NSMakeRect(0, 0, 400, 450);
	window = [[NSWindow alloc] initWithContentRect:frame styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
	[window setTitle:windowTitle];

	NSTextField *description = [[NSTextField alloc] initWithFrame:NSMakeRect(22, 379, 372, 50)];
	[self makeLabel:description:"ZDoom found more than one IWAD\nSelect from the list below to determine which one to use:"];
	[[window contentView] addSubview:description];

	// Commented out version would account for an additional parameters box.
	//NSScrollView *iwadScroller = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 103, 362, 288)];
	NSScrollView *iwadScroller = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 50, 362, 341)];
	NSTableView *iwadTable = [[NSTableView alloc] initWithFrame:[iwadScroller bounds]];
	IWADTableData *tableData = [[IWADTableData alloc] init:wads:numwads];
	for(int i = 0;i < NUM_COLUMNS;i++)
	{
		NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:[NSString stringWithCString:tableHeaders[i]]];
		[[column headerCell] setStringValue:[column identifier]];
		if(i == 0)
			[column setMaxWidth:110];
		[column setEditable:NO];
		[column setResizingMask:NSTableColumnAutoresizingMask];
		[iwadTable addTableColumn:column];
	}
	[iwadScroller setDocumentView:iwadTable];
	[iwadScroller setHasVerticalScroller:YES];
	[iwadTable setDataSource:tableData];
	[iwadTable sizeToFit];
	[iwadTable selectRowIndexes:[[NSIndexSet alloc] initWithIndex:defaultiwad] byExtendingSelection:NO];
	[iwadTable scrollRowToVisible:defaultiwad];
	[[window contentView] addSubview:iwadScroller];

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

	okButton = [[NSButton alloc] initWithFrame:NSMakeRect(196, 12, 96, 32)];
	[okButton setTitle:[NSString stringWithCString:"OK"]];
	[okButton setBezelStyle:NSRoundedBezelStyle];
	[okButton setAction:@selector(buttonPressed:)];
	[okButton setTarget:self];
	[[window contentView] addSubview:okButton];

	cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(292, 12, 96, 32)];
	[cancelButton setTitle:[NSString stringWithCString:"Cancel"]];
	[cancelButton setBezelStyle:NSRoundedBezelStyle];
	[cancelButton setAction:@selector(buttonPressed:)];
	[cancelButton setTarget:self];
	[[window contentView] addSubview:cancelButton];

	[window center];
	[app runModalForWindow:window];

	return cancelled ? -1 : [iwadTable selectedRow];
}

@end

// Simple wrapper so we can call this from outside.
int I_PickIWad_Cocoa (WadStuff *wads, int numwads, bool showwin, int defaultiwad)
{
	return [[IWADPicker alloc] pickIWad:wads:numwads:showwin:defaultiwad];
}
