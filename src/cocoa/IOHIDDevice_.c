//	    File: IOHIDDevice_.c
//	Abstract: convieance functions for IOHIDDeviceGetProperty
//	 Version: 2.0 + 5.3
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

#include <AvailabilityMacros.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1050

#pragma mark - includes & imports
//-----------------------------------------------------

#include "IOHIDDevice_.h"

//*****************************************************
#pragma mark - typedef's, struct's, enums, defines, etc.
//-----------------------------------------------------

#define kIOHIDDevice_TransactionKey "DeviceTransactionRef"
#define kIOHIDDevice_QueueKey "DeviceQueueRef"

//*****************************************************
#pragma mark - local (static) function prototypes
//-----------------------------------------------------

static Boolean IOHIDDevice_GetUInt32Property(IOHIDDeviceRef inIOHIDDeviceRef,
                                             CFStringRef	inKey,
                                             uint32_t *		outValue);
// static void IOHIDDevice_SetUInt32Property(IOHIDDeviceRef inIOHIDDeviceRef, CFStringRef inKey, uint32_t inValue);

static Boolean IOHIDDevice_GetPtrProperty(IOHIDDeviceRef	inIOHIDDeviceRef,
                                          CFStringRef		inKey,
                                          void **			outValue);
static void IOHIDDevice_SetPtrProperty(IOHIDDeviceRef	inIOHIDDeviceRef,
                                       CFStringRef		inKey,
                                       void *			inValue);

//*****************************************************
#pragma mark - exported globals
//-----------------------------------------------------

//*****************************************************
#pragma mark - local (static) globals
//-----------------------------------------------------

//*****************************************************
#pragma mark - exported function implementations
//-----------------------------------------------------

//*************************************************************************
//
// HIDIsValidDevice( inIOHIDDeviceRef )
//
// Purpose:	validate this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	Boolean			- TRUE if we find the device in our( internal ) device list
//

Boolean HIDIsValidDevice(IOHIDDeviceRef inIOHIDDeviceRef) {
	Boolean result = FALSE; // assume failure (pessimist!)
	if ( inIOHIDDeviceRef ) {
		if (                                  CFGetTypeID(inIOHIDDeviceRef) ==IOHIDDeviceGetTypeID() ) {
			result = TRUE;
		}
	}
	
	return (result);
} // HIDIsValidDevice

//*************************************************************************
//
// IOHIDDevice_GetTransport( inIOHIDDeviceRef )
//
// Purpose:	get the Transport CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	CFStringRef - the Transport for this device
//

CFStringRef IOHIDDevice_GetTransport(IOHIDDeviceRef inIOHIDDeviceRef) {
	assert( IOHIDDeviceGetTypeID() == CFGetTypeID(inIOHIDDeviceRef) );
	return ( IOHIDDeviceGetProperty( inIOHIDDeviceRef, CFSTR(kIOHIDTransportKey) ) );
}

//*************************************************************************
//
// IOHIDDevice_GetVendorID( inIOHIDDeviceRef )
//
// Purpose:	get the vendor ID for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the vendor ID for this device
//

uint32_t IOHIDDevice_GetVendorID(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDVendorIDKey), &result);
	return (result);
} // IOHIDDevice_GetVendorID

//*************************************************************************
//
// IOHIDDevice_GetVendorIDSource( inIOHIDDeviceRef )
//
// Purpose:	get the VendorIDSource for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the VendorIDSource for this device
//

uint32_t IOHIDDevice_GetVendorIDSource(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDVendorIDSourceKey), &result);
	return (result);
} // IOHIDDevice_GetVendorIDSource

//*************************************************************************
//
// IOHIDDevice_GetProductID( inIOHIDDeviceRef )
//
// Purpose:	get the product ID for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the product ID for this device
//

uint32_t IOHIDDevice_GetProductID(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDProductIDKey), &result);
	return (result);
} // IOHIDDevice_GetProductID

//*************************************************************************
//
// IOHIDDevice_GetVersionNumber( inIOHIDDeviceRef )
//
// Purpose:	get the VersionNumber CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the VersionNumber for this device
//

uint32_t IOHIDDevice_GetVersionNumber(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDVersionNumberKey), &result);
	return (result);
} // IOHIDDevice_GetVersionNumber

//*************************************************************************
//
// IOHIDDevice_GetManufacturer( inIOHIDDeviceRef )
//
// Purpose:	get the Manufacturer CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	CFStringRef - the Manufacturer for this device
//

CFStringRef IOHIDDevice_GetManufacturer(IOHIDDeviceRef inIOHIDDeviceRef) {
	assert( IOHIDDeviceGetTypeID() == CFGetTypeID(inIOHIDDeviceRef) );
	return ( IOHIDDeviceGetProperty( inIOHIDDeviceRef, CFSTR(kIOHIDManufacturerKey) ) );
} // IOHIDDevice_GetManufacturer

//*************************************************************************
//
// IOHIDDevice_GetProduct( inIOHIDDeviceRef )
//
// Purpose:	get the Product CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	CFStringRef - the Product for this device
//

CFStringRef IOHIDDevice_GetProduct(IOHIDDeviceRef inIOHIDDeviceRef) {
	assert( IOHIDDeviceGetTypeID() == CFGetTypeID(inIOHIDDeviceRef) );
	return ( IOHIDDeviceGetProperty( inIOHIDDeviceRef, CFSTR(kIOHIDProductKey) ) );
} // IOHIDDevice_GetProduct

//*************************************************************************
//
// IOHIDDevice_GetSerialNumber( inIOHIDDeviceRef )
//
// Purpose:	get the SerialNumber CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	CFStringRef - the SerialNumber for this device
//

CFStringRef IOHIDDevice_GetSerialNumber(IOHIDDeviceRef inIOHIDDeviceRef) {
	assert( IOHIDDeviceGetTypeID() == CFGetTypeID(inIOHIDDeviceRef) );
	return ( IOHIDDeviceGetProperty( inIOHIDDeviceRef, CFSTR(kIOHIDSerialNumberKey) ) );
}

//*************************************************************************
//
// IOHIDDevice_GetCountryCode( inIOHIDDeviceRef )
//
// Purpose:	get the CountryCode CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the CountryCode for this device
//

uint32_t IOHIDDevice_GetCountryCode(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDCountryCodeKey), &result);
	return (result);
} // IOHIDDevice_GetCountryCode

//*************************************************************************
//
// IOHIDDevice_GetLocationID( inIOHIDDeviceRef )
//
// Purpose:	get the location ID for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the location ID for this device
//

uint32_t IOHIDDevice_GetLocationID(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDLocationIDKey), &result);
	return (result);
}   // IOHIDDevice_GetLocationID

//*************************************************************************
//
// IOHIDDevice_GetUsage( inIOHIDDeviceRef )
//
// Purpose:	get the usage for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the usage for this device
//

uint32_t IOHIDDevice_GetUsage(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDDeviceUsageKey), &result);
	return (result);
} // IOHIDDevice_GetUsage

//*************************************************************************
//
// IOHIDDevice_GetUsagePage( inIOHIDDeviceRef )
//
// Purpose:	get the usage page for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the usage page for this device
//

uint32_t IOHIDDevice_GetUsagePage(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDDeviceUsagePageKey), &result);
	return (result);
}   // IOHIDDevice_GetUsagePage

//*************************************************************************
//
// IOHIDDevice_GetUsagePairs( inIOHIDDeviceRef )
//
// Purpose:	get the UsagePairs CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	CFArrayRef - the UsagePairs for this device
//

CFArrayRef IOHIDDevice_GetUsagePairs(IOHIDDeviceRef inIOHIDDeviceRef) {
	assert( IOHIDDeviceGetTypeID() == CFGetTypeID(inIOHIDDeviceRef) );
	return ( IOHIDDeviceGetProperty( inIOHIDDeviceRef, CFSTR(kIOHIDDeviceUsagePairsKey) ) );
}

//*************************************************************************
//
// IOHIDDevice_GetPrimaryUsage( inIOHIDDeviceRef )
//
// Purpose:	get the PrimaryUsage CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the PrimaryUsage for this device
//

uint32_t IOHIDDevice_GetPrimaryUsage(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDPrimaryUsageKey), &result);
	return (result);
} // IOHIDDevice_GetPrimaryUsage

//*************************************************************************
//
// IOHIDDevice_GetPrimaryUsagePage( inIOHIDDeviceRef )
//
// Purpose:	get the PrimaryUsagePage CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the PrimaryUsagePage for this device
//

uint32_t IOHIDDevice_GetPrimaryUsagePage(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDPrimaryUsagePageKey), &result);
	return (result);
} // IOHIDDevice_GetPrimaryUsagePage

//*************************************************************************
//
// IOHIDDevice_GetMaxInputReportSize( inIOHIDDeviceRef )
//
// Purpose:	get the MaxInputReportSize CFString for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t  - the MaxInputReportSize for this device
//

uint32_t IOHIDDevice_GetMaxInputReportSize(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDMaxInputReportSizeKey), &result);
	return (result);
} // IOHIDDevice_GetMaxInputReportSize

//*************************************************************************
//
// IOHIDDevice_GetMaxOutputReportSize( inIOHIDDeviceRef )
//
// Purpose:	get the MaxOutputReportSize for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the MaxOutput for this device
//

uint32_t IOHIDDevice_GetMaxOutputReportSize(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDMaxOutputReportSizeKey), &result);
	return (result);
} // IOHIDDevice_GetMaxOutputReportSize

//*************************************************************************
//
// IOHIDDevice_GetMaxFeatureReportSize( inIOHIDDeviceRef )
//
// Purpose:	get the MaxFeatureReportSize for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the MaxFeatureReportSize for this device
//

uint32_t IOHIDDevice_GetMaxFeatureReportSize(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDMaxFeatureReportSizeKey), &result);
	return (result);
} // IOHIDDevice_GetMaxFeatureReportSize

//*************************************************************************
//
// IOHIDDevice_GetReportInterval( inIOHIDDeviceRef )
//
// Purpose:	get the ReportInterval for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	uint32_t - the ReportInterval for this device
//
#ifndef kIOHIDReportIntervalKey
#define kIOHIDReportIntervalKey "ReportInterval"
#endif
uint32_t IOHIDDevice_GetReportInterval(IOHIDDeviceRef inIOHIDDeviceRef) {
	uint32_t result = 0;
	(void) IOHIDDevice_GetUInt32Property(inIOHIDDeviceRef, CFSTR(kIOHIDReportIntervalKey), &result);
	return (result);
} // IOHIDDevice_GetReportInterval

//*************************************************************************
//
// IOHIDDevice_GetQueue( inIOHIDDeviceRef )
//
// Purpose:	get the Queue for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	IOHIDQueueRef - the Queue for this device
//

IOHIDQueueRef IOHIDDevice_GetQueue(IOHIDDeviceRef inIOHIDDeviceRef) {
	IOHIDQueueRef result = 0;
	(void) IOHIDDevice_GetPtrProperty(inIOHIDDeviceRef, CFSTR(kIOHIDDevice_QueueKey), (void *) &result);
	if ( result ) {
		assert( IOHIDQueueGetTypeID() == CFGetTypeID(result) );
	}
	
	return (result);
} // IOHIDDevice_GetQueue

//*************************************************************************
//
// IOHIDDevice_SetQueue( inIOHIDDeviceRef, inQueueRef )
//
// Purpose:	Set the Queue for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//			inQueueRef - the Queue reference
//
// Returns:	nothing
//

void IOHIDDevice_SetQueue(IOHIDDeviceRef inIOHIDDeviceRef, IOHIDQueueRef inQueueRef) {
	IOHIDDevice_SetPtrProperty(inIOHIDDeviceRef, CFSTR(kIOHIDDevice_QueueKey), inQueueRef);
}

//*************************************************************************
//
// IOHIDDevice_GetTransaction( inIOHIDDeviceRef )
//
// Purpose:	get the Transaction for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//
// Returns:	IOHIDTransactionRef - the Transaction for this device
//

IOHIDTransactionRef IOHIDDevice_GetTransaction(IOHIDDeviceRef inIOHIDDeviceRef) {
	IOHIDTransactionRef result = 0;
	(void) IOHIDDevice_GetPtrProperty(inIOHIDDeviceRef, CFSTR(kIOHIDDevice_TransactionKey), (void *) &result);
	return (result);
} // IOHIDDevice_GetTransaction

//*************************************************************************
//
// IOHIDDevice_SetTransaction( inIOHIDDeviceRef, inTransactionRef )
//
// Purpose:	Set the Transaction for this device
//
// Inputs:  inIOHIDDeviceRef - the IDHIDDeviceRef for this device
//			inTransactionRef - the Transaction reference
//
// Returns:	nothing
//

void IOHIDDevice_SetTransaction(IOHIDDeviceRef inIOHIDDeviceRef, IOHIDTransactionRef inTransactionRef) {
	IOHIDDevice_SetPtrProperty(inIOHIDDeviceRef, CFSTR(kIOHIDDevice_TransactionKey), inTransactionRef);
}

//*****************************************************
#pragma mark - local (static) function implementations
//-----------------------------------------------------

//*************************************************************************
//
// IOHIDDevice_GetUInt32Property( inIOHIDDeviceRef, inKey, outValue )
//
// Purpose:	convieance function to return a uint32_t property of a device
//
// Inputs:	inIOHIDDeviceRef		- the device
//			inKey			- CFString for the
//			outValue		- address where to restore the element
// Returns:	the action cookie
//			outValue		- the device
//

static Boolean IOHIDDevice_GetUInt32Property(IOHIDDeviceRef inIOHIDDeviceRef, CFStringRef inKey, uint32_t *outValue) {
	Boolean result = FALSE;
	if ( inIOHIDDeviceRef ) {
		assert( IOHIDDeviceGetTypeID() == CFGetTypeID(inIOHIDDeviceRef) );
		
		CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, inKey);
		if ( tCFTypeRef ) {
			// if this is a number
			if ( CFNumberGetTypeID() == CFGetTypeID(tCFTypeRef) ) {
				// get it's value
				result = CFNumberGetValue( (CFNumberRef) tCFTypeRef, kCFNumberSInt32Type, outValue );
			}
		}
	}
	
	return (result);
}   // IOHIDDevice_GetUInt32Property

//*************************************************************************
//
// IOHIDDevice_SetUInt32Property( inIOHIDDeviceRef, inKey, inValue )
//
// Purpose:	convieance function to set a long property of an Device
//
// Inputs:	inIOHIDDeviceRef	- the Device
// inKey				- CFString for the key
// inValue				- the value to set it to
// Returns:	nothing
//
#if 0 // unused
static void IOHIDDevice_SetUInt32Property(IOHIDDeviceRef inIOHIDDeviceRef, CFStringRef inKey, uint32_t inValue) {
	CFNumberRef tCFNumberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &inValue);
	if ( tCFNumberRef ) {
		IOHIDDeviceSetProperty(inIOHIDDeviceRef, inKey, tCFNumberRef);
		CFRelease(tCFNumberRef);
	}
} // IOHIDDevice_SetUInt32Property
#endif
//*************************************************************************
//
// IOHIDDevice_GetPtrProperty( inIOHIDDeviceRef, inKey, outValue )
//
// Purpose:	convieance function to return a pointer property of a device
//
// Inputs:	inIOHIDDeviceRef		- the device
//			inKey			- CFString for the
//			outValue		- address where to restore the element
// Returns:	the action cookie
//			outValue		- the device
//

static Boolean IOHIDDevice_GetPtrProperty(IOHIDDeviceRef inIOHIDDeviceRef, CFStringRef inKey, void **outValue) {
	Boolean result = FALSE;
	if ( inIOHIDDeviceRef ) {
		assert( IOHIDDeviceGetTypeID() == CFGetTypeID(inIOHIDDeviceRef) );

		CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty(inIOHIDDeviceRef, inKey);
		if ( tCFTypeRef ) {
			// if this is a number
			if ( CFNumberGetTypeID() == CFGetTypeID(tCFTypeRef) ) {
				// get it's value
#ifdef __LP64__
				result = CFNumberGetValue( (CFNumberRef) tCFTypeRef, kCFNumberSInt64Type, outValue );
#else
				result = CFNumberGetValue( (CFNumberRef) tCFTypeRef, kCFNumberSInt32Type, outValue );
#endif // ifdef __LP64__
			}
		}
	}

	return (result);
} // IOHIDDevice_GetPtrProperty

//*************************************************************************
//
// IOHIDDevice_SetPtrProperty( inIOHIDDeviceRef, inKey, inValue )
//
// Purpose:	convieance function to set a long property of an Device
//
// Inputs:	inIOHIDDeviceRef	- the Device
//			inKey			- CFString for the key
//			inValue			- the value to set it to
// Returns:	nothing
//

static void IOHIDDevice_SetPtrProperty(IOHIDDeviceRef inIOHIDDeviceRef, CFStringRef inKey, void *inValue) {
#ifdef __LP64__
	CFNumberRef tCFNumberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &inValue);
#else
	CFNumberRef tCFNumberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &inValue);
#endif // ifdef __LP64__
	if ( tCFNumberRef ) {
		IOHIDDeviceSetProperty(inIOHIDDeviceRef, inKey, tCFNumberRef);
		CFRelease(tCFNumberRef);
	}
}   // IOHIDDevice_SetPtrProperty

//*****************************************************

#endif // MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
