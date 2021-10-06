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

__BEGIN_DECLS

/*!
    @defined HID Device Property Keys
    @abstract Keys that represent properties of a paticular device.
    @discussion Keys that represent properties of a paticular element.  Can be added
        to your matching dictionary when refining searches for HID devices.
*/
#define kIOHIDInputReportElementsKey        "InputReportElements"

/*!
    @defined HID Element Dictionary Keys
    @abstract Keys that represent properties of a particular elements.
    @discussion These keys can also be added to a matching dictionary 
        when searching for elements via copyMatchingElements.  
*/
#define kIOHIDElementCollectionCookieKey    "CollectionCookie"

#define kIOHIDKeyboardCapsLockDelay         "CapsLockDelay"
#define kIOHIDKeyboardEjectDelay            "EjectDelay"

#define kIOHIDAbsoluteAxisBoundsRemovalPercentage   "AbsoluteAxisBoundsRemovalPercentage"

#define kIOHIDSystemMouseButtonTimeout       "MouseButtonTimeout"

__END_DECLS

#endif /* !_IOKIT_HID_IOHIDPRIVATEKEYS_H_ */
