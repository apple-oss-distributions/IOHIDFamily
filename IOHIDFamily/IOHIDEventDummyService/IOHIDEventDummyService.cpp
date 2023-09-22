/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include "IOHIDEventDummyService.h"

#include <AssertMacros.h>
#include "IOHIDDebug.h"

OSDefineMetaClassAndStructors(IOHIDEventDummyService, IOHIDEventService);

bool IOHIDEventDummyService::handleStart(IOService* provider)
{
    bool result = false;
    _interface = OSDynamicCast(IOHIDInterface, provider);
    require(_interface, exit);

    require_action(_interface->open(this, 0,
                                   nullptr,
                                   nullptr),
                   exit,
                   HIDLogError("%s:0x%llx: failed to open %s:0x%llx",
                               getName(), getRegistryEntryID(), _interface->getName(), _interface->getRegistryEntryID()));
    result = true;
exit:
    return result;
}

bool IOHIDEventDummyService::didTerminate(IOService *provider, IOOptionBits options, bool *defer) {
    if (_interface) {
        _interface->close(this);
    }
    _interface = NULL;

    return IOHIDEventService::didTerminate(provider, options, defer);
}


