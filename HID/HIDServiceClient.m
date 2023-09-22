/*!
 * HIDServiceClient.m
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#import <HID/HIDServiceClient.h>
#import <IOKit/hid/IOHIDServiceClient.h>
#import <os/assumes.h>

@implementation HIDServiceClient (HIDFramework)

- (id)propertyForKey:(NSString *)key
{
    return (id)CFBridgingRelease(IOHIDServiceClientCopyProperty(
                                        (__bridge IOHIDServiceClientRef)self,
                                        (__bridge CFStringRef)key));
}

- (NSDictionary *)propertiesForKeys:(NSArray<NSString *> *)keys
{
    return (NSDictionary *)CFBridgingRelease(IOHIDServiceClientCopyProperties(
                                        (__bridge IOHIDServiceClientRef)self,
                                        (__bridge CFArrayRef)keys));
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key
{
    return IOHIDServiceClientSetProperty((__bridge IOHIDServiceClientRef)self,
                                         (__bridge CFStringRef)key,
                                         (__bridge CFTypeRef)value);
}

- (BOOL)conformsToUsagePage:(NSInteger)usagePage usage:(NSInteger)usage
{
    return IOHIDServiceClientConformsTo((__bridge IOHIDServiceClientRef)self,
                                        (uint32_t)usagePage,
                                        (uint32_t)usage);
}

- (HIDEvent *)eventMatching:(NSDictionary *)matching
{
    return (HIDEvent *)CFBridgingRelease(IOHIDServiceClientCopyMatchingEvent(
                                        (__bridge IOHIDServiceClientRef)self,
                                        (__bridge CFDictionaryRef)matching));
}

static void _removalCallback(void *target __unused,
                             void *refcon __unused,
                             IOHIDServiceClientRef service)
{
    HIDServiceClient *me = (__bridge HIDServiceClient *)service;
    
    if (me->_client.removalHandler) {
        ((__bridge HIDBlock)me->_client.removalHandler)();
        Block_release(me->_client.removalHandler);
        me->_client.removalHandler = nil;
    }
}

- (void)setRemovalHandler:(HIDBlock)handler
{
    os_unfair_recursive_lock_lock(&_client.callbackLock);
    os_assert(!_client.removalHandler, "Removal handler already set");
    _client.removalHandler = (void *)Block_copy((__bridge const void *)handler);
    os_unfair_recursive_lock_unlock(&_client.callbackLock);
    IOHIDServiceClientRegisterRemovalCallback(
                                        (__bridge IOHIDServiceClientRef)self,
                                        _removalCallback,
                                        nil,
                                        nil);
}

- (uint64_t)serviceID
{
    id regID = (__bridge id)IOHIDServiceClientGetRegistryID(
                                        (__bridge IOHIDServiceClientRef)self);
    return regID ? [regID unsignedLongLongValue] : 0;
}

@end
