/*!
 * HID.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT double HIDVersionNumber;
FOUNDATION_EXPORT const unsigned char HIDVersionString[];

#import <HID/HIDBase.h>
#import <HID/HIDManager.h>
#import <HID/HIDDevice.h>
#import <HID/HIDUserDevice.h>
#import <HID/HIDElement.h>
#import <HID/HIDEvent.h>
#import <HID/HIDEventAccessors.h>
#import <HID/HIDServiceClient.h>
#import <HID/HIDEventSystemClient.h>
