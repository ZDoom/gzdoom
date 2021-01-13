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

#ifndef MetalCocoaView_h
#define MetalCocoaView_h
#include "metal/system/MetalCore.h"
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

@interface MetalCocoaView : NSView <NSWindowDelegate>
{
    NSCursor* m_cursor;
    CAMetalLayer *metalLayer;
    NSString *str;
}

- (void)setCursor:(NSCursor*)cursor;
- (OBJC_ID(CAMetalDrawable))getDrawable;
- (id)initWithFrame:(NSRect)FrameRect device:(OBJC_ID(MTLDevice))device vsync:(bool)vsync Str:(NSString*)Str;
@end

#endif /* MetalCocoaView_h */
