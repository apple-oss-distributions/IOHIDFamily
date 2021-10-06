/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_HID_IOHIDKEYBOARD_H
#define _IOKIT_HID_IOHIDKEYBOARD_H

#include <IOKit/hidsystem/IOHIDTypes.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include "IOHIDDevice.h"

enum {
    kUSB_LEFT_CONTROL_BIT = 0x01,
    kUSB_LEFT_SHIFT_BIT = 0x02,
    kUSB_LEFT_ALT_BIT = 0x04,
    kUSB_LEFT_FLOWER_BIT = 0x08,

    kUSB_RIGHT_CONTROL_BIT = 0x10,
    kUSB_RIGHT_SHIFT_BIT = 0x20,
    kUSB_RIGHT_ALT_BIT = 0x040,
    kUSB_RIGHT_FLOWER_BIT = 0x80
};

enum {
    kUSB_LOWSPEED_MAXPACKET = 8,
    kUSB_CAPSLOCKLED_SET = 2,
    kUSB_NUMLOCKLED_SET = 1
};

/* Following table is used to convert Apple USB keyboard IDs into a numbering
   scheme that can be combined with ADB handler IDs for both Cocoa and Carbon */
enum {
    kgestUSBCosmoANSIKbd      = 198,     /* (0xC6) Gestalt Cosmo USB Domestic (ANSI) Keyboard */
    kprodUSBCosmoANSIKbd      = 0x201,   // The actual USB product ID in hardware
    kgestUSBCosmoISOKbd       = 199,     /* (0xC7) Cosmo USB International (ISO) Keyboard */
    kprodUSBCosmoISOKbd       = 0x202,
    kgestUSBCosmoJISKbd       = 200,     /* (0xC8) Cosmo USB Japanese (JIS) Keyboard */
    kprodUSBCosmoJISKbd       = 0x203,
    kgestUSBAndyANSIKbd       = 204,      /* (0xCC) Andy USB Keyboard Domestic (ANSI) Keyboard */
    kprodUSBAndyANSIKbd       = 0x204,
    kgestUSBAndyISOKbd        = 205,      /* (0xCD) Andy USB Keyboard International (ISO) Keyboard */
    kprodUSBAndyISOKbd	      = 0x205,
    kgestUSBAndyJISKbd        = 206,       /* (0xCE) Andy USB Keyboard Japanese (JIS) Keyboard */
    kprodUSBAndyJISKbd	      = 0x206,

    kgestQ6ANSIKbd	      = 31,      /* (0x31) Apple Q6 Keyboard Domestic (ANSI) Keyboard */
    kprodQ6ANSIKbd	      = 0x208,
    kgestQ6ISOKbd	      = 32,      /* (0x32) Apple Q6 Keyboard International (ISO) Keyboard */
    kprodQ6ISOKbd	      = 0x209,
    kgestQ6JISKbd	      = 33,      /* (0x33) Apple Q6 Keyboard Japanese (JIS) Keyboard */
    kprodQ6JISKbd	      = 0x20a,
    
    kgestQ30ANSIKbd	      = 34,      /* (0x34) Apple Q30 Keyboard Domestic (ANSI) Keyboard */
    kprodQ30ANSIKbd	      = 0x20b,
    kgestQ30ISOKbd	      = 35,      /* (0x35) Apple Q30 Keyboard International (ISO) Keyboard */
    kprodQ30ISOKbd	      = 0x20c,
    kgestQ30JISKbd	      = 36,      /* (0x36) Apple Q30 Keyboard Japanese (JIS) Keyboard */
    kprodQ30JISKbd	      = 0x20d    

};

#define ADB_CONVERTER_LEN       0xff + 1   //length of array def_usb_2_adb_keymap[]

class IOHIDKeyboard : public IOHIKeyboard
{
    OSDeclareDefaultStructors(IOHIDKeyboard)

    IOHIDDevice	*		_provider;

    // Values from the provider
    UInt16			_productID;
    UInt16			_vendorID;
        
    // LED Specific Members
    IOHIDElementCookie  	_ledCookies[2];
    UInt32 *			_ledValuePtrs[2];
    UInt8			_numLeds;
    UInt8			_ledState;
    thread_call_t		_asyncLEDThread;


    // Modifier Specific Members
    UInt32 *			_modifierValuePtrs[8];
    UInt8			_oldmodifier;
    bool			_flower_key;  //Mac Command key
    bool			_control_key; //Control needed for 3-finger reboot
    
    // Scan Code Array Specific Members
    UInt32 *			_keyCodeArrayValuePtr;
    UInt32			_keyCodeArrayElementBitSize;
    UInt32			_keyCodeArrayElementCount;
    UInt32 *			_oldArraySelectors;
    UInt8                       _usb_2_adb_keymap[ADB_CONVERTER_LEN + 1];
    
    // *** PRIVATE HELPER METHODS ***
    void   			Set_LED_States(UInt8);
    UInt32 			handlerID();
    bool			findDesiredElements(OSArray *elements);
    // *** END PRIVATE HELPER METHODS ***
    
    // static methods for callbacks, the command gate, new threads, etc.
    static void 		AsyncLED (OSObject *target);

public:    
    // Allocator
    static IOHIDKeyboard * 	Keyboard(OSArray *elements);
    
    // IOService methods
    virtual bool		init(OSDictionary * properties = 0);
    virtual bool		start(IOService * provider);
    virtual void 		stop(IOService *  provider);
    virtual void 		free();

    virtual void 		handleReport();

    // IOHIDevice methods
    UInt32 			interfaceID();
    UInt32 			deviceType();

    // IOHIKeyboard methods
    UInt32 			maxKeyCodes();
    const unsigned char * 	defaultKeymapOfLength(UInt32 * length);
    void 			setAlphaLockFeedback(bool LED_state);
    void 			setNumLockFeedback(bool LED_state);
    bool 			doesKeyLock(unsigned key);
    unsigned 			getLEDStatus();

};


#endif /* !_IOKIT_HID_IOHIDKEYBOARD_H */
