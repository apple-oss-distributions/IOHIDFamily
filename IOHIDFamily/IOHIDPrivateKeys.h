/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2009 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_HID_IOHIDPRIVATEKEYS_H_
#define _IOKIT_HID_IOHIDPRIVATEKEYS_H_

#include <sys/cdefs.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDProperties.h>
#include <IOKit/hid/IOHIDEventServiceKeys_Private.h>

__BEGIN_DECLS

/*!
    @defined HID Device Property Keys
    @abstract Keys that represent properties of a particular device.
    @discussion Keys that represent properties of a particular element.  Can be added
        to your matching dictionary when refining searches for HID devices.
*/
#define kIOHIDInputReportElementsKey        "InputReportElements"

#define kIOHIDReportModeKey                 "ReportMode"

typedef enum { 
    kIOHIDReportModeTypeNormal      = 0,
    kIOHIDReportModeTypeFiltered
} IOHIDReportModeType;

/* Following table is used to convert Apple USB keyboard IDs into a numbering
 scheme that can be combined with ADB handler IDs for both Cocoa and Carbon */
enum {
    kgestUSBUnknownANSIkd   = 3,       /* Unknown ANSI keyboard */
    kgestUSBGenericANSIkd   = 40,      /* Generic ANSI keyboard */
    kgestUSBGenericISOkd    = 41,      /* Generic ANSI keyboard */
    kgestUSBGenericJISkd    = 42,      /* Generic ANSI keyboard */
    
    kgestUSBCosmoANSIKbd    = 198,     /* (0xC6) Gestalt Cosmo USB Domestic (ANSI) Keyboard */
    kprodUSBCosmoANSIKbd    = 0x201,   // The actual USB product ID in hardware
    kgestUSBCosmoISOKbd     = 199,     /* (0xC7) Cosmo USB International (ISO) Keyboard */
    kprodUSBCosmoISOKbd     = 0x202,
    kgestUSBCosmoJISKbd     = 200,     /* (0xC8) Cosmo USB Japanese (JIS) Keyboard */
    kprodUSBCosmoJISKbd     = 0x203,
    kgestUSBAndyANSIKbd       = 204,      /* (0xCC) Andy USB Keyboard Domestic (ANSI) Keyboard */
    kprodUSBAndyANSIKbd     = 0x204,
    kgestUSBAndyISOKbd      = 205,      /* (0xCD) Andy USB Keyboard International (ISO) Keyboard */
    kprodUSBAndyISOKbd      = 0x205,
    kgestUSBAndyJISKbd      = 206,      /* (0xCE) Andy USB Keyboard Japanese (JIS) Keyboard */
    kprodUSBAndyJISKbd      = 0x206,
    
    kgestQ6ANSIKbd          = 31,      /* (031) Apple Q6 Keyboard Domestic (ANSI) Keyboard */
    kprodQ6ANSIKbd          = 0x208,
    kgestQ6ISOKbd           = 32,      /* (32) Apple Q6 Keyboard International (ISO) Keyboard */
    kprodQ6ISOKbd           = 0x209,
    kgestQ6JISKbd           = 33,      /* (33) Apple Q6 Keyboard Japanese (JIS) Keyboard */
    kprodQ6JISKbd           = 0x20a,
    
    kgestQ30ANSIKbd         = 34,      /* (34) Apple Q30 Keyboard Domestic (ANSI) Keyboard */
    kprodQ30ANSIKbd         = 0x20b,
    kgestQ30ISOKbd          = 35,      /* (35) Apple Q30 Keyboard International (ISO) Keyboard */
    kprodQ30ISOKbd          = 0x20c,
    kgestQ30JISKbd          = 36,      /* (36) Apple Q30 Keyboard Japanese (JIS) Keyboard */
    kprodQ30JISKbd          = 0x20d,
    
    kgestFountainANSIKbd    = 37,      /* (37) Apple Fountain Keyboard Domestic (ANSI) Keyboard */
    kprodFountainANSIKbd    = 0x20e,
    kgestFountainISOKbd     = 38,      /* (38) Apple Fountain Keyboard International (ISO) Keyboard */
    kprodFountainISOKbd     = 0x20f,
    kgestFountainJISKbd     = 39,      /* (39) Apple Fountain Keyboard Japanese (JIS) Keyboard */
    kprodFountainJISKbd     = 0x210,
    
    kgestSantaANSIKbd       = 37,      /* (37) Apple Santa Keyboard Domestic (ANSI) Keyboard */
    kprodSantaANSIKbd       = 0x211,
    kgestSantaISOKbd        = 38,      /* (38) Apple Santa Keyboard International (ISO) Keyboard */
    kprodSantaISOKbd        = 0x212,
    kgestSantaJISKbd        = 39,      /* (39) Apple Santa Keyboard Japanese (JIS) Keyboard */
    kprodSantaJISKbd        = 0x213,
    
    kgestM89ISOKbd          = 47,      /* (47) Apple M89 Wired (ISO) Keyboard */
    kgestM90ISOKbd          = 44      /* (44) Apple M90 Wireless (ISO) Keyboard */
};

/*!
    @defined kIOHIDLogLevelKey
    @abstract Level of detailed logging generated by device driver.
    @discussion The values should match level used in syslog and asl.  
*/

#define kIOHIDLogLevelKey                   "LogLevel"

typedef enum {
     kIOHIDLogLevelTypeEmergency = 0,
     kIOHIDLogLevelTypeAlert,
     kIOHIDLogLevelTypeCritical,
     kIOHIDLogLevelTypeError,
     kIOHIDLogLevelTypeWarning,
     kIOHIDLogLevelTypeNotice,
     kIOHIDLogLevelTypeInfo,
     kIOHIDLogLevelTypeDebug,
     kIOHIDLogLevelTypeTrace
} IOHIDLogLevelType;

/*!
    @defined kIOHIDTraceConfigKey
    @abstract enable/configure ktrace signposts
    @discussion property should be set on event system
*/

#define kIOHIDDebugConfigKey               "HIDDebug"

enum {
    kIOHIDDebugTraceWithKTrace         = 0x1,
    kIOHIDDebugTraceWithOsLog          = 0x2,
    kIOHIDDebugTraceUserDevice         = 0x4,
    kIOHIDDebugTraceEventSystem        = 0x8,
    kIOHIDDebugPerfEvent               = 0x10,
};

/*!
    @defined kIOHIDPerfKey
    @abstract tag events with perf event
    @discussion perf event will be added as child to top level event
*/

#define kIOHIDPerfEventKey                 "PerfEvent"


/*!
    @defined HID Element Dictionary Keys
    @abstract Keys that represent properties of a particular elements.
    @discussion These keys can also be added to a matching dictionary 
        when searching for elements via copyMatchingElements.  
*/
#define kIOHIDElementCollectionCookieKey    "CollectionCookie"

#define kIOHIDAbsoluteAxisBoundsRemovalPercentage   "AbsoluteAxisBoundsRemovalPercentage"

#define kIOHIDEventServiceQueueSize         "QueueSize"
#define kIOHIDAltSenderIdKey                "alt_sender_id"

#define kIOHIDMaxReportEnqueueSizeKey        "MaxQueuedReportSize"

#define kIOHIDAppleVendorSupported          "AppleVendorSupported"

#define kIOHIDSetButtonPropertiesKey        "SetButtonProperties"
#define kIOHIDSetButtonPriorityKey          "SetButtonPriority"
#define kIOHIDSetButtonDelayKey             "SetButtonDelay"
#define kIOHIDSetButtonLoggingKey           "SetButtonLogging"

#define kIOHIDSetDoubleClickTimeoutKey      "SetDoubleClickTimeout"

#define kIOHIDSupportsGlobeKeyKey           "SupportsGlobeKey"

#define kIOHIDSupportsSiriKeyKey            "SupportsSiriKey"

#define kIOHIDCompatibilityInterface        "HIDCompatibilityInterface"

#define kIOHIDAuthenticatedDeviceKey        "Authenticated"

#define kIOHIDFastPathHasEntitlementKey     "FastPathHasEntitlement"

/*!
 * @define      kIOHIDFastPathMotionEventEntitlementKey
 * @abstract    Fast Path User Client has privileges to receive motion events during restricted states.
 * @discussion  Property published on IOHIDEventServiceFastPathUserClient designating that the driver
 *              should provide motion events to this client even while in a motion event restricted state
 *              (kIOHIDMotionEventRestrictedKey).
 */
#define kIOHIDFastPathMotionEventEntitlementKey "FastPathMotionEventEntitlement"

/*!
 * @define kIOHIDCapsLockLEDDarkWakeInhibitKey
 * @abstract Boolean value indicating if the caps lock LED should be inhibited in the dark wake state.
 * @discussion  Key value must be set on domain using the appropriate user  in order to inhibit caps lock LED
 *              Usage:```sudo -u _hidd defaults write com.apple.iohid HIDCapsLockLEDDarkWakeInhibit [state, can be 0 or 1]```
 */
#define kIOHIDCapsLockLEDDarkWakeInhibitKey "HIDCapsLockLEDDarkWakeInhibit"


/*!
    @typedef    IOHIDHomeButtonType
    @abstract   Integer value that describes the type of home/menu button on the device.
    @discussion For use with MobileGestalt key kMGQHomeButtonType. This query is
                applicable to standard iOS. Use with other platforms or nonstandard
                iOS devices could have unexpected results.
    @constant   kIOHIDHomeButtonTypeMechanical  Mechanical home button.
    @constant   kIOHIDHomeButtonTypeSSHB        "Solid-state" virtual home button.
    @constant   kIOHIDHomeButtonTypeNone        No home button - e.g. on-screen virtual home bar.
 */
typedef enum {
    kIOHIDHomeButtonTypeMechanical  = 0,
    kIOHIDHomeButtonTypeSSHB        = 1,
    kIOHIDHomeButtonTypeNone        = 2
} IOHIDHomeButtonType;



__END_DECLS

#endif /* !_IOKIT_HID_IOHIDPRIVATEKEYS_H_ */
