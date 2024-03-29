/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2017 Apple Computer, Inc.  All Rights Reserved.
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
#import <Foundation/Foundation.h>
#import <mach/mach_time.h>
#import "HIDLibElement.h"
#import <IOKit/hid/IOHIDLibPrivate.h>

@implementation HIDLibElement

@synthesize elementRef = _element;
@synthesize psKey = _psKey;
@synthesize elementStruct = _elementStruct;
@synthesize defaultValueRef = _defaultValue;
@synthesize isConstant = _isConstant;
@synthesize isUpdated = _isUpdated;

- (nullable instancetype)initWithElementRef:(nonnull IOHIDElementRef)elementRef
{
    self = [super init];
    
    if (!self) {
        return self;
    }

    if (!elementRef) {
        return nil;
    }

    CFRetain(elementRef);
    _element = elementRef;
    
    return self;
}

- (nullable instancetype)initWithElementStruct:(IOHIDElementStruct *)elementStruct
                                        parent:(IOHIDElementRef)parent
                                         index:(uint32_t)index
{
    self = [super init];
    
    if (!self) {
        return self;
    }

    _element = NULL;

    CFDataRef dummyData = CFDataCreateMutable(kCFAllocatorDefault, 1);
    
    bcopy(elementStruct, &_elementStruct, sizeof(IOHIDElementStruct));
    
    if (dummyData) {
        _element = _IOHIDElementCreateWithParentAndData(kCFAllocatorDefault,
                                                        parent,
                                                        dummyData,
                                                        &_elementStruct,
                                                        index);
        
        CFRelease(dummyData);
    }
    
    if (!_element) {
        return nil;
    }
    
    return self;
}

- (void)dealloc
{
    if (_defaultValue) {
        CFRelease(_defaultValue);
    }
    
    if (_element) {
        CFRelease(_element);
    }
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"element: %p type: %d uP: 0x%02x u: 0x%02x cookie: %d val: %ld",
            _element, self.type, self.usagePage, self.usage, self.elementCookie, (long)self.integerValue];
}

- (BOOL)isEqualToHIDLibElement:(HIDLibElement *)element {
    if (!element) {
        return NO;
    }
    
    if (self->_element != element->_element) {
        return NO;
    }
    
    return YES;
}

- (BOOL)isEqual:(id)object {
    if (self == object) {
        return YES;
    }
    
    if (![object isKindOfClass:[HIDLibElement class]]) {
        return NO;
    }
    
    return [self isEqualToHIDLibElement:(HIDLibElement *)object];
}

- (IOHIDElementRef)elementRef
{
    return _element;
}

- (void)setElementRef:(IOHIDElementRef)elementRef
{
    if (_element == elementRef) {
        return;
    }

    if (_element) {
        CFRelease(_element);
    }

    if (elementRef) {
        CFRetain(elementRef);
    }

    _element = elementRef;
}

- (IOHIDValueRef)valueRef
{
    return _element ? _IOHIDElementGetValue(_element) : NULL;
}

-(void)setValueRef:(IOHIDValueRef)valueRef
{
    if (_element) {
        _IOHIDElementSetValue(_element, valueRef);
    }
}

- (IOHIDValueRef)defaultValueRef
{
    return _defaultValue;
}

-(void)setDefaultValueRef:(IOHIDValueRef)defaultValueRef
{
    if (_defaultValue == defaultValueRef) {
        return;
    }

    if (_defaultValue) {
        CFRelease(_defaultValue);
    }
    
    if (defaultValueRef) {
        CFRetain(defaultValueRef);
    }
    
    _defaultValue = defaultValueRef;
}

- (NSInteger)integerValue
{
    return self.valueRef ? IOHIDValueGetIntegerValue(self.valueRef) : 0;
}

- (void)setIntegerValue:(NSInteger)integerValue
{
    IOHIDValueRef value = IOHIDValueCreateWithIntegerValue(
                                                           kCFAllocatorDefault,
                                                           _element,
                                                           mach_absolute_time(),
                                                           integerValue);
    self.valueRef = value;
    if (value) {
        CFRelease(value);
    }
}

- (nullable NSData *)dataValue
{
    return self.valueRef ? [NSData dataWithBytes:(void *)IOHIDValueGetBytePtr(self.valueRef)
                                          length:IOHIDValueGetLength(self.valueRef)] : nil;
}

- (void)setDataValue:(NSData *)dataValue
{
    IOHIDValueRef value = IOHIDValueCreateWithBytes(
                                                    kCFAllocatorDefault,
                                                    _element,
                                                    0,
                                                    [dataValue bytes],
                                                    [dataValue length]);
    self.valueRef = value;
    if (value) {
        CFRelease(value);
    }
}

- (uint64_t)timestamp
{
    return self.valueRef ? IOHIDValueGetTimeStamp(self.valueRef) : 0;
}

- (NSInteger)length
{
    return _element ? _IOHIDElementGetLength(_element) : 0;
}

- (uint32_t)elementCookie
{
    return _element ? (uint32_t)IOHIDElementGetCookie(_element) : 0;
}

- (uint32_t)collectionCookie
{
    return _elementStruct.parentCookie;
}

- (IOHIDElementType)type
{
    return _element ? IOHIDElementGetType(_element) : 0;
}

- (uint32_t)usage
{
    return _element ? IOHIDElementGetUsage(_element) : 0;
}

- (uint32_t)usagePage
{
    return _element ? IOHIDElementGetUsagePage(_element) : 0;
}

- (uint32_t)unit
{
    return _element ? IOHIDElementGetUnit(_element) : 0;
}

- (uint8_t)unitExponent
{
    return _element ? IOHIDElementGetUnitExponent(_element) : 0;
}

- (uint8_t)reportID
{
    return _element ? IOHIDElementGetReportID(_element) : 0;
}

- (uint32_t)usageMin
{
    return _elementStruct.usageMin;
}

- (uint32_t)usageMax
{
    return _elementStruct.usageMax;
}

- (IOHIDElementCollectionType)collectionType
{
    return _element ? IOHIDElementGetCollectionType(_element) : 0;
}

@end
