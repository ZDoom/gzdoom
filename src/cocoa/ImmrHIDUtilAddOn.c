//	    File: ImmrHIDUtilAddOn.c
//	Abstract: Glue code to convert IOHIDDeviceRef's to (FFB) io_object_t's
//	 Version: 2.0
//	
//	Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple
//	Inc. ("Apple") in consideration of your agreement to the following
//	terms, and your use, installation, modification or redistribution of
//	this Apple software constitutes acceptance of these terms.  If you do
//	not agree with these terms, please do not use, install, modify or
//	redistribute this Apple software.
//	
//	In consideration of your agreement to abide by the following terms, and
//	subject to these terms, Apple grants you a personal, non-exclusive
//	license, under Apple's copyrights in this original Apple software (the
//	"Apple Software"), to use, reproduce, modify and redistribute the Apple
//	Software, with or without modifications, in source and/or binary forms;
//	provided that if you redistribute the Apple Software in its entirety and
//	without modifications, you must retain this notice and the following
//	text and disclaimers in all such redistributions of the Apple Software.
//	Neither the name, trademarks, service marks or logos of Apple Inc. may
//	be used to endorse or promote products derived from the Apple Software
//	without specific prior written permission from Apple.  Except as
//	expressly stated in this notice, no other rights or licenses, express or
//	implied, are granted by Apple herein, including but not limited to any
//	patent rights that may be infringed by your derivative works or by other
//	works in which the Apple Software may be incorporated.
//	
//	The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
//	MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
//	THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
//	FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
//	OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
//	
//	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
//	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//	INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
//	MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
//	AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
//	STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
//	POSSIBILITY OF SUCH DAMAGE.
//	
//	Copyright (C) 2009 Apple Inc. All Rights Reserved.
//	
//*****************************************************
#include <mach/mach.h>
#include <mach/mach_error.h>

#include "ImmrHIDUtilAddOn.h"

//---------------------------------------------------------------------------------
//
// AllocateHIDObjectFromIOHIDDeviceRef( )
//
//	returns:
//		NULL, or acceptable io_object_t
//
//---------------------------------------------------------------------------------
io_service_t AllocateHIDObjectFromIOHIDDeviceRef(IOHIDDeviceRef inIOHIDDeviceRef) {
	io_service_t result = 0L;
	if ( inIOHIDDeviceRef ) {
		// Set up the matching criteria for the devices we're interested in.
		// We are interested in instances of class IOHIDDevice.
		// matchingDict is consumed below( in IOServiceGetMatchingService )
		// so we have no leak here.
		CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOHIDDeviceKey);
		if ( matchingDict ) {
			// Add a key for locationID to our matching dictionary.  This works for matching to
			// IOHIDDevices, so we will only look for a device attached to that particular port
			// on the machine.
			CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty( inIOHIDDeviceRef, CFSTR(kIOHIDLocationIDKey) );
			if ( tCFTypeRef ) {
				CFDictionaryAddValue(matchingDict, CFSTR(kIOHIDLocationIDKey), tCFTypeRef);
				// CFRelease( tCFTypeRef );	// don't release objects that we "Get".
				
				// IOServiceGetMatchingService assumes that we already know that there is only one device
				// that matches.  This way we don't have to do the whole iteration dance to look at each
				// device that matches.  This is a new API in 10.2
				result = IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
			}
			
			// Note: We're not leaking the matchingDict.
			// One reference is consumed by IOServiceGetMatchingServices
		}
	}
	
	return (result);
}   // AllocateHIDObjectFromIOHIDDeviceRef

//---------------------------------------------------------------------------------
//
// FreeHIDObject( )
//
//---------------------------------------------------------------------------------
bool FreeHIDObject(io_service_t inHIDObject) {
	kern_return_t kr;
	
	kr = IOObjectRelease(inHIDObject);
	
	return (kIOReturnSuccess == kr);
} // FreeHIDObject
