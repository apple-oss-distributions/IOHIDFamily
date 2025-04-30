/*
 * Copyright (c) 1998-2014 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include <IOKit/IOService.h>
#include <IOKit/IOProviderPropertyMerger.h>

#ifndef _IOKIT_IOHIDPROVIDERPROPERTYMERGER_H
#define _IOKIT_IOHIDPROVIDERPROPERTYMERGER_H
class IOHIDProviderPropertyMerger : public IOProviderPropertyMerger
{
    OSDeclareDefaultStructors(IOHIDProviderPropertyMerger);

protected:
    struct ExpansionData { };
    
    /*! @var reserved
        Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;

    virtual bool mergeProperties(IOService *  provider, OSDictionary * properties);
    virtual bool mergeDictionaries(OSDictionary * source, OSDictionary * target);

public:
    
    virtual IOService * probe(IOService * provider, SInt32 * score) APPLE_KEXT_OVERRIDE;
};

#endif /* ! _IOKIT_IOHIDPROVIDERPROPERTYMERGER_H */
