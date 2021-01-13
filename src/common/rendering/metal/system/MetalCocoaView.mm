//
//---------------------------------------------------------------------------
//
// Copyright(C) 2020-2021 Eugene Grigorchuk
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "MetalCocoaView.h"

@implementation MetalCocoaView

- (void)drawRect:(NSRect)dirtyRect
{
    [NSColor.blackColor setFill];
    NSRectFill(dirtyRect);
}

- (void)resetCursorRects
{
    [super resetCursorRects];

    NSCursor* const cursor = nil == m_cursor ? [NSCursor arrowCursor] : m_cursor;

    [self addCursorRect:[self bounds]
                 cursor:cursor];
}

- (void)setCursor:(NSCursor*)cursor
{
    m_cursor = cursor;
}

-(id)initWithFrame:(NSRect)FrameRect device:(OBJC_ID(MTLDevice))device vsync:(bool)vsync Str:(NSString*)Str
{
    self = [super initWithFrame:FrameRect];
    self.wantsLayer = YES;
    
    metalLayer = [CAMetalLayer layer];
    metalLayer.device =  device;
    metalLayer.framebufferOnly = YES; //todo: optimized way
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.wantsExtendedDynamicRangeContent = false;
    //metalLayer.drawableSize = CGSizeMake(self.frame.size.width, self.frame.size.height);
    metalLayer.drawableSize = CGSizeMake(1440, 900);
    metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
    metalLayer.colorspace = CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3); // TEMPORARY
    if (@available(macOS 10.13, *)) {
        metalLayer.displaySyncEnabled = vsync;
    }
    self.layer = metalLayer;
    str = Str;
    NSUserDefaults *StandardDefaults = [NSUserDefaults standardUserDefaults];
    
    NSTrackingArea* trackingArea = [[[NSTrackingArea alloc] initWithRect: self.bounds
                                                                 options: ( NSTrackingCursorUpdate | NSTrackingActiveInKeyWindow )
                                                                   owner:self userInfo:nil] autorelease];
    [self addTrackingArea:trackingArea];
    [self setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    
    return self;
}

-(OBJC_ID(CAMetalDrawable))getDrawable
{
    return metalLayer.nextDrawable;
}

@end
