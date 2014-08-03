//	    File: HID_Config_Utilities.c
//	Abstract: Implementation of the HID configuration utilities
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
#define LOG_SCORING	0

#include <stdlib.h> // malloc
#include <time.h> // clock

#include <AssertMacros.h>

#include "HID_Utilities_External.h"

// ---------------------------------

// polls single device's elements for a change greater than kPercentMove.  Times out after given time
// returns 1 and pointer to element if found
// returns 0 and NULL for both parameters if not found

unsigned char HIDConfigureSingleDeviceAction(IOHIDDeviceRef inIOHIDDeviceRef, IOHIDElementRef *outIOHIDElementRef, float timeout) {
	if ( !inIOHIDDeviceRef ) {
		return (0);
	}
	if ( 0 == HIDHaveDeviceList() ) {             // if we do not have a device list
		return (0);                                 // return 0
	}
	
	Boolean found = FALSE;
	
	// build list of device and elements to save current values
	int maxElements = HIDCountDeviceElements(inIOHIDDeviceRef, kHIDElementTypeInput);
	int *saveValueArray = (int *) calloc( maxElements, sizeof(int) );              // 2D array to save values
	
	// store initial values on first pass / compare to initial value on subsequent passes
	Boolean first = TRUE;
	
	// get all the elements from this device
	CFArrayRef elementCFArrayRef = IOHIDDeviceCopyMatchingElements(inIOHIDDeviceRef, NULL, kIOHIDOptionsTypeNone);
	// if that worked...
	if ( elementCFArrayRef ) {
		clock_t start = clock(), end;
		
		// poll all devices and elements
		while ( !found ) {
			int currElementIndex = 0;
			CFIndex idx, cnt = CFArrayGetCount(elementCFArrayRef);
			for ( idx = 0; idx < cnt; idx++ ) {
				*outIOHIDElementRef = (IOHIDElementRef) CFArrayGetValueAtIndex(elementCFArrayRef, idx);
				if ( !*outIOHIDElementRef ) {
					continue;
				}
				
				// is this an input element?
				IOHIDElementType type = IOHIDElementGetType(*outIOHIDElementRef);
				
				switch ( type ) {
						// these types are inputs
					case kIOHIDElementTypeInput_Misc:
					case kIOHIDElementTypeInput_Button:
					case kIOHIDElementTypeInput_Axis:
					case kIOHIDElementTypeInput_ScanCodes:
					default:
					{
						break;
					}
					case kIOHIDElementTypeOutput:
					case kIOHIDElementTypeFeature:
					case kIOHIDElementTypeCollection:
					{
						*outIOHIDElementRef = NULL;                            // these types are not ( Skip them )
						break;
					}
				}                                                             /* switch */
				if ( !*outIOHIDElementRef ) {
					continue;                                                 // skip this element
				}
				
				// get this elements current value
				int value = 0;  // default value is zero
				IOHIDValueRef tIOHIDValueRef;
				IOReturn ioReturn = IOHIDDeviceGetValue(inIOHIDDeviceRef, *outIOHIDElementRef, &tIOHIDValueRef);
				if ( kIOReturnSuccess == ioReturn ) {
					value = IOHIDValueGetScaledValue(tIOHIDValueRef, kIOHIDValueScaleTypePhysical);
				}
				if ( first ) {
					saveValueArray[currElementIndex] = value;
				} else {
					CFIndex min = IOHIDElementGetLogicalMin(*outIOHIDElementRef);
					CFIndex max = IOHIDElementGetLogicalMax(*outIOHIDElementRef);
					
					int initialValue = saveValueArray[currElementIndex];
					int delta = (float)(max - min) * kPercentMove * 0.01;
					// is the new value within +/- delta of the initial value?
					if ( ( (initialValue + delta) < value ) || ( (initialValue - delta) > value ) ) {
						found = 1;  // ( yes! ) mark as found
						break;
					}
				}   // if ( first )
				
				currElementIndex++; // bump element index
			}                       // next idx
			
			first = FALSE;          // no longer the first pass
			
			// are we done?
			end = clock();
			double secs = (double)(end - start) / CLOCKS_PER_SEC;
			if ( secs > timeout ) {
				break;              // ( yes ) timeout
			}
		}                           // while ( !found )
		
		CFRelease(elementCFArrayRef);
	}                               // if ( elementCFArrayRef )
	// return device and element moved
	if ( found ) {
		return (1);
	} else {
		*outIOHIDElementRef = NULL;
		return (0);
	}
} // HIDConfigureSingleDeviceAction

//*************************************************************************
//
// HIDConfigureAction( outIOHIDDeviceRef, outIOHIDElementRef, inTimeout )
//
// Purpose:	polls all devices and elements for a change greater than kPercentMove.
//			Times out after given time returns 1 and pointer to device and element
//			if found; returns 0 and NULL for both parameters if not found
//
// Inputs:	outIOHIDDeviceRef	- address where to store the device
//			outIOHIDElementRef	- address where to store the element
//			inTimeout	- the timeout
// Returns:	Boolean		- if successful
//			outIOHIDDeviceRef	- the device
//			outIOHIDElementRef	- the element
//

Boolean HIDConfigureAction(IOHIDDeviceRef *outIOHIDDeviceRef, IOHIDElementRef *outIOHIDElementRef, float inTimeout) {
	// param error?
	if ( !outIOHIDDeviceRef || !outIOHIDElementRef ) {
		return (0);
	}
	if ( !gDeviceCFArrayRef ) { // if we do not have a device list
		// and  we can't build another list
		if ( !HIDBuildDeviceList(0, 0) || !gDeviceCFArrayRef ) {
			return (FALSE);   // bail
		}
	}
	
	IOHIDDeviceRef tIOHIDDeviceRef;
	IOHIDElementRef tIOHIDElementRef;
	
	// remember when we start; used to calculate timeout
	clock_t start = clock(), end;
	
	// determine the maximum number of elements
	CFIndex maxElements = 0;
	CFIndex devIndex, devCount = CFArrayGetCount(gDeviceCFArrayRef);
	for ( devIndex = 0; devIndex < devCount; devIndex++ ) {
		tIOHIDDeviceRef = (IOHIDDeviceRef) CFArrayGetValueAtIndex(gDeviceCFArrayRef, devIndex);
		if ( !tIOHIDDeviceRef ) {
			continue;               // skip this one
		}
		
		CFIndex count = HIDCountDeviceElements(tIOHIDDeviceRef, kHIDElementTypeInput);
		if ( count > maxElements ) {
			maxElements = count;
		}
	}
	
	// allocate an array of int's in which to store devCount * maxElements values
	double *saveValueArray = (double *) calloc( devCount * maxElements, sizeof(double) );     // clear 2D array to save values
	
	// on first pass store initial values / compare current values to initial values on subsequent passes
	Boolean found = FALSE, first = TRUE;
	
	while ( !found ) {
		double maxDeltaPercent = 0; // we want to find the one that moves the most ( percentage wise )
		for ( devIndex = 0; devIndex < devCount; devIndex++ ) {
			tIOHIDDeviceRef = (IOHIDDeviceRef) CFArrayGetValueAtIndex(gDeviceCFArrayRef, devIndex);
			if ( !tIOHIDDeviceRef ) {
				continue;                       // skip this one
			}
			
#ifdef DEBUG
			long vendorID = IOHIDDevice_GetVendorID(tIOHIDDeviceRef);
			long productID = IOHIDDevice_GetProductID(tIOHIDDeviceRef);
#endif
			gElementCFArrayRef = IOHIDDeviceCopyMatchingElements(tIOHIDDeviceRef, NULL, kIOHIDOptionsTypeNone);
			if ( gElementCFArrayRef ) {
				CFIndex eleIndex, eleCount = CFArrayGetCount(gElementCFArrayRef);
				for ( eleIndex = 0; eleIndex < eleCount; eleIndex++ ) {
					tIOHIDElementRef = (IOHIDElementRef) CFArrayGetValueAtIndex(gElementCFArrayRef, eleIndex);
					if ( !tIOHIDElementRef ) {
						continue;
					}
					
					IOHIDElementType tIOHIDElementType = IOHIDElementGetType(tIOHIDElementRef);
					// only care about inputs (no outputs or features)
					if ( tIOHIDElementType <= kIOHIDElementTypeInput_ScanCodes ) {
						if ( IOHIDElementIsArray(tIOHIDElementRef) ) {
							//printf( "ARRAY!\n" );
							continue;   // skip array elements
						}
						
						uint32_t usagePage = IOHIDElementGetUsagePage(tIOHIDElementRef);
						uint32_t usage = IOHIDElementGetUsage(tIOHIDElementRef);
						uint32_t reportCount = IOHIDElementGetReportCount(tIOHIDElementRef);
#ifdef DEBUG
						if ( first ) {
							IOHIDElementCookie cookie = IOHIDElementGetCookie(tIOHIDElementRef);
							printf("%s, dev: {ref:%p, ven: 0x%08lX, pro: 0x%08lX}, ele: {ref:%p, cookie: %p, usage:%04lX:%08lX}\n",
							       __PRETTY_FUNCTION__,
							       tIOHIDDeviceRef,
							       vendorID,
							       productID,
							       tIOHIDElementRef,
							       cookie,
							       (long unsigned int) usagePage,
							       (long unsigned int) usage);
							fflush(stdout);
							if ( (0x054C == vendorID) && (0x0268 == productID) && (0x001E == (UInt32) cookie) ) {
								//printf( "DING!\n" );
							}
						}
						
#endif
#if 1                   // work-around for IOHIDValueGetScaledValue crash (when element report count > 1)
						if ( reportCount > 1 ) {
							//printf( "REPORT!\n" );
							continue; // skip reports
						}
						
#endif
						// ignore PID elements and arrays
						if ( (kHIDPage_PID != usagePage) && (((uint32_t)-1) != usage) ) {
							// get this elements current value
							double value = 0.0; // default value is zero
							IOHIDValueRef tIOHIDValueRef;
							IOReturn ioReturn = IOHIDDeviceGetValue(tIOHIDDeviceRef, tIOHIDElementRef, &tIOHIDValueRef);
							if ( kIOReturnSuccess == ioReturn ) {
								value = IOHIDValueGetScaledValue(tIOHIDValueRef, kIOHIDValueScaleTypePhysical);
							}
							if ( first ) {
								saveValueArray[(devIndex * maxElements) + eleIndex] = value;
							} else {
								double initialValue = saveValueArray[(devIndex * maxElements) + eleIndex];
								
								CFIndex valueMin = IOHIDElementGetPhysicalMin(tIOHIDElementRef);
								CFIndex valueMax = IOHIDElementGetPhysicalMax(tIOHIDElementRef);
								
								double deltaPercent = fabs( (initialValue - value) * 100.0 / (valueMax - valueMin) );
#if 0							// debug code useful to dump out value info for specific (vendorID, productID, usagePage and usage) device
								if ( !first ) {
									// Device: 0x13b6a0 = { Logitech Inc. - WingMan Force 3D,   vendorID:	0x046D,     productID:	0xC283,
									// usage: 0x0001:0x0004, "Generic Desktop Joystick"
									if ( (vendorID == 0x046D) && (productID == 0xC283) ) {
										if ( (kHIDPage_GenericDesktop == usagePage) && (kHIDUsage_GD_Rz == usage) ) {
											printf("initial: %6.2f, value: %6.2f, diff: %6.2f, delta percent: %6.2f!\n",
											       initialValue,
											       value,
											       fabs(initialValue - value),
											       deltaPercent);
										}
									}
								}
								
								deltaPercent = 0.0;
#endif
								if ( deltaPercent >= kPercentMove ) {
									found = TRUE;
									if ( deltaPercent > maxDeltaPercent ) {
										maxDeltaPercent = deltaPercent;
										
										*outIOHIDDeviceRef = tIOHIDDeviceRef;
										*outIOHIDElementRef = tIOHIDElementRef;
									}
									
									break;
								}
							}   // if first
							
						}       // if usage
						
					}           // if type
					
				}               // for elements...
				
				CFRelease(gElementCFArrayRef);
				gElementCFArrayRef = NULL;
			}   // if ( gElementCFArrayRef )
			if ( found ) {
				// HIDDumpElementInfo( tIOHIDElementRef );
				break; // DONE!
			}
		}                   // for devices
		
		first = FALSE;          // no longer the first pass
		
		// are we done?
		end = clock();
		double secs = (double)(end - start) / CLOCKS_PER_SEC;
		if ( secs > inTimeout ) {
			break;              // ( yes ) timeout
		}
	}   //	while ( !found )
	// return device and element moved
	if ( !found ) {
		*outIOHIDDeviceRef = NULL;
		*outIOHIDElementRef = NULL;
	}
	
	return (found);
}   // HIDConfigureAction

//*************************************************************************
//
// HIDSaveElementPref( inKeyCFStringRef, inAppCFStringRef, inIOHIDDeviceRef, inIOHIDElementRef )
//
// Purpose:	Save the device & element values into the specified key in the specified applications preferences
//
// Inputs:	inKeyCFStringRef	- the preference key
//			inAppCFStringRef	- the application identifier
//			inIOHIDDeviceRef			- the device
//			inIOHIDElementRef			- the element
// Returns:	Boolean				- if successful
//

Boolean HIDSaveElementPref(const CFStringRef inKeyCFStringRef,
                           CFStringRef       inAppCFStringRef,
                           IOHIDDeviceRef    inIOHIDDeviceRef,
						   IOHIDElementRef   inIOHIDElementRef) {
	Boolean success = FALSE;
	if ( inKeyCFStringRef && inAppCFStringRef && inIOHIDDeviceRef && inIOHIDElementRef ) {
		long vendorID = IOHIDDevice_GetVendorID(inIOHIDDeviceRef);
		require(vendorID, Oops);
		
		long productID = IOHIDDevice_GetProductID(inIOHIDDeviceRef);
		require(productID, Oops);
		
		long locID = IOHIDDevice_GetLocationID(inIOHIDDeviceRef);
		require(locID, Oops);
		
		uint32_t usagePage = IOHIDDevice_GetUsagePage(inIOHIDDeviceRef);
		uint32_t usage = IOHIDDevice_GetUsage(inIOHIDDeviceRef);
		if ( !usagePage || !usage ) {
			usagePage = IOHIDDevice_GetPrimaryUsagePage(inIOHIDDeviceRef);
			usage = IOHIDDevice_GetPrimaryUsage(inIOHIDDeviceRef);
		}
		
		require(usagePage && usage, Oops);
		
		uint32_t usagePageE = IOHIDElementGetUsagePage(inIOHIDElementRef);
		uint32_t usageE = IOHIDElementGetUsage(inIOHIDElementRef);
		IOHIDElementCookie eleCookie = IOHIDElementGetCookie(inIOHIDElementRef);
		
		CFStringRef prefCFStringRef = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
															   CFSTR("d:{v:%ld, p:%ld, l:%ld, p:%ld, u:%ld}, e:{p:%ld, u:%ld, c:%ld}"),
															   vendorID, productID, locID, usagePage, usage, 
															   usagePageE, usageE, eleCookie);
		if ( prefCFStringRef ) {
			CFPreferencesSetAppValue(inKeyCFStringRef, prefCFStringRef, inAppCFStringRef);
			CFRelease(prefCFStringRef);
			success = TRUE;
		}
	}
	
Oops:   ;
	return (success);
}   // HIDSaveElementPref

//*************************************************************************
//
// HIDRestoreElementPref( inKeyCFStringRef, inAppCFStringRef, outIOHIDDeviceRef, outIOHIDElementRef )
//
// Purpose:	Find the specified preference in the specified application
//
// Inputs:	inKeyCFStringRef	- the preference key
//			inAppCFStringRef	- the application identifier
//			outIOHIDDeviceRef	- address where to restore the device
//			outIOHIDElementRef	- address where to restore the element
// Returns:	Boolean				- if successful
//			outIOHIDDeviceRef	- the device
//			outIOHIDElementRef	- the element
//

Boolean HIDRestoreElementPref(CFStringRef      inKeyCFStringRef,
                              CFStringRef      inAppCFStringRef,
                              IOHIDDeviceRef * outIOHIDDeviceRef,
                              IOHIDElementRef *outIOHIDElementRef) {
	Boolean found = FALSE;
	if ( inKeyCFStringRef && inAppCFStringRef && outIOHIDDeviceRef && outIOHIDElementRef ) {
		CFPropertyListRef prefCFPropertyListRef = CFPreferencesCopyAppValue(inKeyCFStringRef, inAppCFStringRef);
		if ( prefCFPropertyListRef ) {
			if ( CFStringGetTypeID() == CFGetTypeID(prefCFPropertyListRef) ) {
				char buffer[256];
				if ( CFStringGetCString( (CFStringRef) prefCFPropertyListRef, buffer, sizeof(buffer),
				                        kCFStringEncodingUTF8 ) )
				{
					HID_info_rec searchHIDInfo;
					
					int count = sscanf(buffer,
					                   "d:{v:%d, p:%d, l:%d, p:%d, u:%d}, e:{p:%d, u:%d, c:%ld}",
					                   &searchHIDInfo.device.vendorID,
					                   &searchHIDInfo.device.productID,
					                   &searchHIDInfo.device.locID,
					                   &searchHIDInfo.device.usagePage,
					                   &searchHIDInfo.device.usage,
					                   &searchHIDInfo.element.usagePage,
					                   &searchHIDInfo.element.usage,
					                   (long *) &searchHIDInfo.element.cookie);
					if ( 8 == count ) { // if we found all eight parameters…
						// and can find a device & element that matches these…
						if ( HIDFindDeviceAndElement(&searchHIDInfo, outIOHIDDeviceRef, outIOHIDElementRef) ) {
							found = TRUE;
						}
					}
				}
			} else {
				// We found the entry with this key but it's the wrong type; delete it.
				CFPreferencesSetAppValue(inKeyCFStringRef, NULL, inAppCFStringRef);
				(void) CFPreferencesAppSynchronize(inAppCFStringRef);
			}
			
			CFRelease(prefCFPropertyListRef);
		}
	}
	
	return (found);
}   // HIDRestoreElementPref

//*************************************************************************
//
// HIDFindDeviceAndElement( inSearchInfo, outFoundDevice, outFoundElement )
//
// Purpose:	find the closest matching device and element for this action
//
// Notes:	matches device: serial, vendorID, productID, location, inUsagePage, usage
//			matches element: cookie, inUsagePage, usage,
//
// Inputs:	inSearchInfo	- the device & element info we searching for
//			outFoundDevice	- the address of the best matching device
//			outFoundElement	- the address of the best matching element
//
// Returns:	Boolean			- TRUE if we find a match
//			outFoundDevice	- the best matching device
//			outFoundElement	- the best matching element
//

Boolean HIDFindDeviceAndElement(const HID_info_rec *inSearchInfo, IOHIDDeviceRef *outFoundDevice, IOHIDElementRef *outFoundElement) {
	Boolean result = FALSE;
	
	IOHIDDeviceRef bestIOHIDDeviceRef = NULL;
	IOHIDElementRef bestIOHIDElementRef = NULL;
	long bestScore = 0;
	
	CFIndex devIndex, devCount = CFArrayGetCount(gDeviceCFArrayRef);
	for ( devIndex = 0; devIndex < devCount; devIndex++ ) {
		long deviceScore = 1;
		
		IOHIDDeviceRef tIOHIDDeviceRef = (IOHIDDeviceRef) CFArrayGetValueAtIndex(gDeviceCFArrayRef, devIndex);
		if ( !tIOHIDDeviceRef ) {
			continue;
		}
		// match vendorID, productID (+10, +8)
		if ( inSearchInfo->device.vendorID ) {
			long vendorID = IOHIDDevice_GetVendorID(tIOHIDDeviceRef);
			if ( vendorID ) {
				if ( inSearchInfo->device.vendorID == vendorID ) {
					deviceScore += 10;
					if ( inSearchInfo->device.productID ) {
						long productID = IOHIDDevice_GetProductID(tIOHIDDeviceRef);
						if ( productID ) {
							if ( inSearchInfo->device.productID == productID ) {
								deviceScore += 8;
							}   // if ( inSearchInfo->device.productID == productID )
							
						}       // if ( productID )
						
					}           // if ( inSearchInfo->device.productID )
					
				}               // if (inSearchInfo->device.vendorID == vendorID)
				
			}                   // if vendorID
			
		}                       // if search->device.vendorID
		// match usagePage & usage (+9)
		if ( inSearchInfo->device.usagePage && inSearchInfo->device.usage ) {
			uint32_t usagePage = IOHIDDevice_GetUsagePage(tIOHIDDeviceRef) ;
			uint32_t usage = IOHIDDevice_GetUsage(tIOHIDDeviceRef);
			if ( !usagePage || !usage ) {
				usagePage = IOHIDDevice_GetPrimaryUsagePage(tIOHIDDeviceRef);
				usage = IOHIDDevice_GetPrimaryUsage(tIOHIDDeviceRef);
			}
			if ( usagePage ) {
				if ( inSearchInfo->device.usagePage == usagePage ) {
					if ( usage ) {
						if ( inSearchInfo->device.usage == usage ) {
							deviceScore += 9;
						}   // if ( inSearchInfo->usage == usage )
						
					}       // if ( usage )
					
				}           // if ( inSearchInfo->usagePage == usagePage )
				
			}               // if ( usagePage )
			
		}                   // if ( inSearchInfo->usagePage && inSearchInfo->usage )
		// match location ID (+5)
		if ( inSearchInfo->device.locID ) {
			long locID = IOHIDDevice_GetLocationID(tIOHIDDeviceRef);
			if ( locID ) {
				if ( inSearchInfo->device.locID == locID ) {
					deviceScore += 5;
				}
			}
		}
		
		// iterate over all elements of this device
		gElementCFArrayRef = IOHIDDeviceCopyMatchingElements(tIOHIDDeviceRef, NULL, 0);
		if ( gElementCFArrayRef ) {
			CFIndex eleIndex, eleCount = CFArrayGetCount(gElementCFArrayRef);
			for ( eleIndex = 0; eleIndex < eleCount; eleIndex++ ) {
				IOHIDElementRef tIOHIDElementRef = (IOHIDElementRef) CFArrayGetValueAtIndex(gElementCFArrayRef, eleIndex);
				if ( !tIOHIDElementRef ) {
					continue;
				}
				
				long score = deviceScore;
				// match usage page, usage & cookie
				if ( inSearchInfo->element.usagePage && inSearchInfo->element.usage ) {
					uint32_t usagePage = IOHIDElementGetUsagePage(tIOHIDElementRef);
					if ( inSearchInfo->element.usagePage == usagePage ) {
						uint32_t usage = IOHIDElementGetUsage(tIOHIDElementRef);
						if ( inSearchInfo->element.usage == usage ) {
							score += 5;
							IOHIDElementCookie cookie = IOHIDElementGetCookie(tIOHIDElementRef);
							if ( inSearchInfo->element.cookie == cookie ) {
								score += 4;
							}                                               // cookies match
							
						} else {
							score = 0;
						}                                                   // usages match
						
					} else {
						score = 0;
					}                                                       // usage pages match
					
				}                                                           // if ( search usage page & usage )
				
#if LOG_SCORING
				if ( kHIDPage_KeyboardOrKeypad != tElementRef->usagePage ) {    // skip keyboards here
					printf("%s: ( %ld:%ld )-I-Debug, score: %ld\t",
					       __PRETTY_FUNCTION__,
					       inSearchInfo->element.usagePage,
					       inSearchInfo->element.usage,
					       score);
					HIDPrintElement(tIOHIDElementRef);
				}
				
#endif // LOG_SCORING
				if ( score > bestScore ) {
					bestIOHIDDeviceRef = tIOHIDDeviceRef;
					bestIOHIDElementRef = tIOHIDElementRef;
					bestScore = score;
#if LOG_SCORING
					printf("%s: ( %ld:%ld )-I-Debug, better score: %ld\t",
					       __PRETTY_FUNCTION__,
					       inSearchInfo->element.usagePage,
					       inSearchInfo->element.usage,
					       score);
					HIDPrintElement(bestIOHIDElementRef);
#endif // LOG_SCORING
				}
			}   // for elements...
			
			CFRelease(gElementCFArrayRef);
			gElementCFArrayRef = NULL;
		}       // if ( gElementCFArrayRef )
		
	}           // for ( devIndex = 0; devIndex < devCount; devIndex++ )
	if ( bestIOHIDDeviceRef || bestIOHIDElementRef ) {
		*outFoundDevice = bestIOHIDDeviceRef;
		*outFoundElement = bestIOHIDElementRef;
#if LOG_SCORING
		printf("%s: ( %ld:%ld )-I-Debug, best score: %ld\t",
		       __PRETTY_FUNCTION__,
		       inSearchInfo->element.usagePage,
		       inSearchInfo->element.usage,
		       bestScore);
		HIDPrintElement(bestIOHIDElementRef);
#endif // LOG_SCORING
		result = TRUE;
	}
	
	return (result);
}   // HIDFindDeviceAndElement

// ---------------------------------

// takes input records, save required info
// assume file is open and at correct position.
// will always write to file (if file exists) size of HID_info_rec, even if device and or element is bad

void HIDSaveElementConfig(FILE *fileRef, IOHIDDeviceRef inIOHIDDeviceRef, IOHIDElementRef inIOHIDElementRef, int actionCookie) {
	// must save:
	// actionCookie
	// Device: serial,vendorID, productID, location, usagePage, usage
	// Element: cookie, usagePage, usage,
	HID_info_rec hidInfoRec;
	HIDSetElementConfig(&hidInfoRec, inIOHIDDeviceRef, inIOHIDElementRef, actionCookie);
	// write to file
	if ( fileRef ) {
		fwrite( (void *)&hidInfoRec, sizeof(HID_info_rec), 1, fileRef );
	}
} // HIDSaveElementConfig

// ---------------------------------

// take file, read one record (assume file position is correct and file is open)
// search for matching device
// return pDevice, pElement and cookie for action

int HIDRestoreElementConfig(FILE *fileRef, IOHIDDeviceRef *outIOHIDDeviceRef, IOHIDElementRef *outIOHIDElementRef) {
	// Device: serial,vendorID, productID, location, usagePage, usage
	// Element: cookie, usagePage, usage,
	
	HID_info_rec hidInfoRec;
	fread( (void *) &hidInfoRec, 1, sizeof(HID_info_rec), fileRef );
	return ( HIDGetElementConfig(&hidInfoRec, outIOHIDDeviceRef, outIOHIDElementRef) );
} // HIDRestoreElementConfig

// ---------------------------------

// Set up a config record for saving
// takes an input records, returns record user can save as they want
// Note: the save rec must be pre-allocated by the calling app and will be filled out
void HIDSetElementConfig(HID_info_ptr     inHIDInfoPtr,
                         IOHIDDeviceRef  inIOHIDDeviceRef,
                         IOHIDElementRef inIOHIDElementRef,
                         int             actionCookie) {
	// must save:
	// actionCookie
	// Device: serial,vendorID, productID, location, usagePage, usage
	// Element: cookie, usagePage, usage,
	inHIDInfoPtr->actionCookie = actionCookie;
	// device
	// need to add serial number when I have a test case
	if ( inIOHIDDeviceRef && inIOHIDElementRef ) {
		inHIDInfoPtr->device.vendorID = IOHIDDevice_GetVendorID(inIOHIDDeviceRef);
		inHIDInfoPtr->device.productID = IOHIDDevice_GetProductID(inIOHIDDeviceRef);
		inHIDInfoPtr->device.locID = IOHIDDevice_GetLocationID(inIOHIDDeviceRef);
		inHIDInfoPtr->device.usage = IOHIDDevice_GetUsage(inIOHIDDeviceRef);
		inHIDInfoPtr->device.usagePage = IOHIDDevice_GetUsagePage(inIOHIDDeviceRef);
		
		inHIDInfoPtr->element.usagePage = IOHIDElementGetUsagePage(inIOHIDElementRef);
		inHIDInfoPtr->element.usage = IOHIDElementGetUsage(inIOHIDElementRef);
		inHIDInfoPtr->element.minReport = IOHIDElement_GetCalibrationSaturationMin(inIOHIDElementRef);
		inHIDInfoPtr->element.maxReport = IOHIDElement_GetCalibrationSaturationMax(inIOHIDElementRef);
		inHIDInfoPtr->element.cookie = IOHIDElementGetCookie(inIOHIDElementRef);
	} else {
		inHIDInfoPtr->device.vendorID = 0;
		inHIDInfoPtr->device.productID = 0;
		inHIDInfoPtr->device.locID = 0;
		inHIDInfoPtr->device.usage = 0;
		inHIDInfoPtr->device.usagePage = 0;
		
		inHIDInfoPtr->element.usagePage = 0;
		inHIDInfoPtr->element.usage = 0;
		inHIDInfoPtr->element.minReport = 0;
		inHIDInfoPtr->element.maxReport = 0;
		inHIDInfoPtr->element.cookie = 0;
	}
} // HIDSetElementConfig

// ---------------------------------
#if 0	// debug utility function to dump config record
void HIDDumpConfig(HID_info_ptr inHIDInfoPtr) {
	printf(
		   "Config Record for action: %d\n\t vendor: %d    product: %d    location: %d\n\t usage: %d    usagePage: %d\n\t element.usagePage: %d    element.usage: %d\n\t minReport: %d    maxReport: %d\n\t cookie: %d\n",
		   inHIDInfoPtr->actionCookie,
		   inHIDInfoPtr->device.vendorID,
		   inHIDInfoPtr->device.productID,
		   inHIDInfoPtr->locID,
		   inHIDInfoPtr->usage,
		   inHIDInfoPtr->usagePage,
		   inHIDInfoPtr->element.usagePage,
		   inHIDInfoPtr->element.usage,
		   inHIDInfoPtr->minReport,
		   inHIDInfoPtr->maxReport,
		   inHIDInfoPtr->cookie);
} // HIDDumpConfig
#endif // 0
// ---------------------------------

// Get matching element from config record
// takes a pre-allocated and filled out config record
// search for matching device
// return pDevice, pElement and cookie for action
int HIDGetElementConfig(HID_info_ptr inHIDInfoPtr, IOHIDDeviceRef *outIOHIDDeviceRef, IOHIDElementRef *outIOHIDElementRef) {
	if ( !inHIDInfoPtr->device.locID && !inHIDInfoPtr->device.vendorID && !inHIDInfoPtr->device.productID && !inHIDInfoPtr->device.usage
		&& !inHIDInfoPtr->device.usagePage )                                                                                                                            //
	{                                                                                                                                                                   //
		// early out
		*outIOHIDDeviceRef = NULL;
		*outIOHIDElementRef = NULL;
		return (inHIDInfoPtr->actionCookie);
	}
	
	IOHIDDeviceRef tIOHIDDeviceRef, foundIOHIDDeviceRef = NULL;
	IOHIDElementRef tIOHIDElementRef, foundIOHIDElementRef = NULL;
	
	CFIndex devIdx, devCnt, idx, cnt;
	// compare to current device list for matches
	// look for device
	if ( inHIDInfoPtr->device.locID && inHIDInfoPtr->device.vendorID && inHIDInfoPtr->device.productID ) { // look for specific device
		// type plug in to same port
		devCnt = CFArrayGetCount(gDeviceCFArrayRef);
		for ( devIdx = 0; devIdx < devCnt; devIdx++ ) {
			tIOHIDDeviceRef = (IOHIDDeviceRef) CFArrayGetValueAtIndex(gDeviceCFArrayRef, devIdx);
			if ( !tIOHIDDeviceRef ) {
				continue;       // skip this device
			}
			
			long locID = IOHIDDevice_GetLocationID(tIOHIDDeviceRef);
			long vendorID = IOHIDDevice_GetVendorID(tIOHIDDeviceRef);
			long productID = IOHIDDevice_GetProductID(tIOHIDDeviceRef);
			if ( (inHIDInfoPtr->device.locID == locID)
			    && (inHIDInfoPtr->device.vendorID == vendorID)
			    && (inHIDInfoPtr->device.productID == productID) )
			{
				foundIOHIDDeviceRef = tIOHIDDeviceRef;
			}
			if ( foundIOHIDDeviceRef ) {
				break;
			}
		}   // next devIdx
		if ( foundIOHIDDeviceRef ) {
			CFArrayRef elementCFArrayRef = IOHIDDeviceCopyMatchingElements(foundIOHIDDeviceRef, NULL, kIOHIDOptionsTypeNone);
			if ( elementCFArrayRef ) {
				cnt = CFArrayGetCount(elementCFArrayRef);
				for ( idx = 0; idx < cnt; idx++ ) {
					tIOHIDElementRef = (IOHIDElementRef) CFArrayGetValueAtIndex(elementCFArrayRef, idx);
					if ( !tIOHIDElementRef ) {
						continue;       // skip this element
					}
					
					IOHIDElementCookie cookie = IOHIDElementGetCookie(tIOHIDElementRef);
					if ( inHIDInfoPtr->element.cookie == cookie ) {
						foundIOHIDElementRef = tIOHIDElementRef;
					}
					if ( foundIOHIDElementRef ) {
						break;
					}
				}
				if ( !foundIOHIDElementRef ) {
					cnt = CFArrayGetCount(elementCFArrayRef);
					for ( idx = 0; idx < cnt; idx++ ) {
						tIOHIDElementRef = (IOHIDElementRef) CFArrayGetValueAtIndex(elementCFArrayRef, idx);
						if ( !tIOHIDElementRef ) {
							continue;                           // skip this element
						}
						
						uint32_t usagePage = IOHIDElementGetUsagePage(tIOHIDElementRef);
						uint32_t usage = IOHIDElementGetUsage(tIOHIDElementRef);
						if ( (inHIDInfoPtr->element.usage == usage) && (inHIDInfoPtr->element.usagePage == usagePage) ) {
							foundIOHIDElementRef = tIOHIDElementRef;
						}
						if ( foundIOHIDElementRef ) {
							break;
						}
					}   // next idx
					
				}   // if ( !foundIOHIDElementRef )
				if ( foundIOHIDElementRef ) { // if same device
					// setup the calibration
					IOHIDElement_SetupCalibration(tIOHIDElementRef);
					
					IOHIDElement_SetCalibrationSaturationMin(tIOHIDElementRef, inHIDInfoPtr->element.minReport);
					IOHIDElement_SetCalibrationSaturationMax(tIOHIDElementRef, inHIDInfoPtr->element.maxReport);
				}
				
				CFRelease(elementCFArrayRef);
			}   // if ( elementCFArrayRef )
			
		}   // if ( foundIOHIDDeviceRef )
		// if we have not found a match, look at just vendor and product
		if ( (!foundIOHIDDeviceRef) && (inHIDInfoPtr->device.vendorID && inHIDInfoPtr->device.productID) ) {
			devCnt = CFArrayGetCount(gDeviceCFArrayRef);
			for ( devIdx = 0; devIdx < devCnt; devIdx++ ) {
				tIOHIDDeviceRef = (IOHIDDeviceRef) CFArrayGetValueAtIndex(gDeviceCFArrayRef, devIdx);
				if ( !tIOHIDDeviceRef ) {
					continue;       // skip this device
				}
				
				long vendorID = IOHIDDevice_GetVendorID(tIOHIDDeviceRef);
				long productID = IOHIDDevice_GetProductID(tIOHIDDeviceRef);
				if ( (inHIDInfoPtr->device.vendorID == vendorID)
				    && (inHIDInfoPtr->device.productID == productID) )
				{
					foundIOHIDDeviceRef = tIOHIDDeviceRef;
				}
				if ( foundIOHIDDeviceRef ) {
					break;
				}
			}
			// match elements by cookie since same device type
			if ( foundIOHIDDeviceRef ) {
				CFArrayRef elementCFArrayRef = IOHIDDeviceCopyMatchingElements(foundIOHIDDeviceRef, NULL, kIOHIDOptionsTypeNone);
				if ( elementCFArrayRef ) {
					cnt = CFArrayGetCount(elementCFArrayRef);
					for ( idx = 0; idx < cnt; idx++ ) {
						tIOHIDElementRef = (IOHIDElementRef) CFArrayGetValueAtIndex(elementCFArrayRef, idx);
						if ( !tIOHIDElementRef ) {
							continue;       // skip this element
						}
						
						IOHIDElementCookie cookie = IOHIDElementGetCookie(tIOHIDElementRef);
						if ( inHIDInfoPtr->element.cookie == cookie ) {
							foundIOHIDElementRef = tIOHIDElementRef;
						}
						if ( foundIOHIDElementRef ) {
							break;
						}
					}
					// if no cookie match (should NOT occur) match on usage
					if ( !foundIOHIDElementRef ) {
						cnt = CFArrayGetCount(elementCFArrayRef);
						for ( idx = 0; idx < cnt; idx++ ) {
							tIOHIDElementRef = (IOHIDElementRef) CFArrayGetValueAtIndex(elementCFArrayRef, idx);
							if ( !tIOHIDElementRef ) {
								continue;                           // skip this element
							}
							
							uint32_t usagePage = IOHIDElementGetUsagePage(tIOHIDElementRef);
							uint32_t usage = IOHIDElementGetUsage(tIOHIDElementRef);
							if ( (inHIDInfoPtr->element.usage == usage)
							    && (inHIDInfoPtr->element.usagePage == usagePage) )
							{
								foundIOHIDElementRef = tIOHIDElementRef;
							}
							if ( foundIOHIDElementRef ) {
								break;
							}
						}   // next idx
						
					}   // if ( !foundIOHIDElementRef )
					if ( foundIOHIDElementRef ) { // if same device
						// setup the calibration
						IOHIDElement_SetupCalibration(tIOHIDElementRef);
						IOHIDElement_SetCalibrationSaturationMin(tIOHIDElementRef, inHIDInfoPtr->element.minReport);
						IOHIDElement_SetCalibrationSaturationMax(tIOHIDElementRef, inHIDInfoPtr->element.maxReport);
					}
					
					CFRelease(elementCFArrayRef);
				}   // if ( elementCFArrayRef )
				
			}   // if ( foundIOHIDDeviceRef )
			
		}   // if ( device not found & vendorID & productID )
		
	}   // if (  inHIDInfoPtr->locID && inHIDInfoPtr->device.vendorID && inHIDInfoPtr->device.productID )
	// can't find matching device return NULL, do not return first device
	if ( (!foundIOHIDDeviceRef) || (!foundIOHIDElementRef) ) {
		// no HID device
		*outIOHIDDeviceRef = NULL;
		*outIOHIDElementRef = NULL;
		return (inHIDInfoPtr->actionCookie);
	} else {
		// HID device
		*outIOHIDDeviceRef = foundIOHIDDeviceRef;
		*outIOHIDElementRef = foundIOHIDElementRef;
		return (inHIDInfoPtr->actionCookie);
	}
} // HIDGetElementConfig
