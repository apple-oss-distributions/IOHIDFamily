//
//  TestFilterClient.m
//  IOHIDFamily
//
//  Created by YG on 10/25/16.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventSystemTestController.h"
#import "IOHIDUserDeviceTestController.h"
#import "IOHIDDeviceTestController.h"
#import "IOHIDUnitTestDescriptors.h"
#include <IOKit/hid/IOHIDEventSystemKeys.h>


static boolean_t EventSystemEventFilterCallback (void * _Nullable target, void * _Nullable refcon __unused, void * _Nullable sender, IOHIDEventRef event);

static void EventSystemResetCallback(void * _Nullable target, void * _Nullable context __unused);

static void EventSystemEventCallback (void * _Nullable target, void * _Nullable refcon __unused, void * _Nullable sender, IOHIDEventRef event);

static void EventSystemPropertyChangedCallback (void * _Nullable target, void * _Nullable context, CFStringRef property, CFTypeRef value);


static uint8_t descriptor[] = {
    HIDVendorMessage32BitDescriptor
};

@interface TestEventSystemClient : XCTestCase

@property IOHIDUserDeviceTestController *   sourceController;

@property dispatch_queue_t                  rootQueue;
@property dispatch_queue_t                  clientQueue;


@property IOHIDEventSystemClientRef         eventSystemClient;
@property IOHIDEventSystemClientRef         testEventSystemClient;

@property NSMutableArray *                  events;
@property NSMutableArray *                  filteredEvents;

@property NSInteger                         resetCount;
@property NSInteger                         propertyCount;

@property NSString*                         uniqueID;

@end

@implementation TestEventSystemClient

- (void)setUp {
    [super setUp];

    self.rootQueue = IOHIDUnitTestCreateRootQueue(37, 2);
  
    self.clientQueue = dispatch_queue_create_with_target ("IOHIDEventSystemClientQueue", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.clientQueue != nil);

    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    self.uniqueID = [[[NSUUID alloc] init] UUIDString];
  
    self.events = [[NSMutableArray alloc] init];

    self.filteredEvents = [[NSMutableArray alloc] init];

    self.sourceController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:self.uniqueID andQueue:nil];
    HIDXCTAssertAndThrowTrue(EVAL(self.sourceController != nil));

    self.eventSystemClient =  IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    HIDXCTAssertAndThrowTrue (self.eventSystemClient != NULL);

    self.testEventSystemClient =  IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    HIDXCTAssertAndThrowTrue (self.testEventSystemClient != NULL);

    XCTAssert (IOHIDEventSystemClientGetTypeID() == CFGetTypeID(self.testEventSystemClient));

    IOHIDEventSystemClientRegisterEventCallback (self.eventSystemClient, EventSystemEventCallback, (__bridge void * _Nullable)(self), NULL);
    NSDictionary * matching = @{@kIOHIDPhysicalDeviceUniqueIDKey : self.uniqueID};
    IOHIDEventSystemClientSetMatching (self.eventSystemClient, (CFDictionaryRef)matching);
    IOHIDEventSystemClientRegisterResetCallback(self.eventSystemClient, EventSystemResetCallback, (__bridge void * _Nullable)(self), NULL);
    dispatch_sync (self.clientQueue, ^{
        IOHIDEventSystemClientScheduleWithDispatchQueue(self.eventSystemClient, self.clientQueue);
        // this is barrier that guaranteed that IOHIDEventSystemClientScheduleWithDispatchQueue completed
        CFTypeRef val = IOHIDEventSystemClientCopyProperty(self.eventSystemClient, CFSTR (kIOHIDEventSystemClientIsUnresponsive));
        if (val) {
            CFRelease(val);
        }
    });
  
}

- (void)tearDown {
  
    if (self.clientQueue) {
        dispatch_sync (self.clientQueue, ^{
            IOHIDEventSystemClientUnscheduleFromDispatchQueue(self.eventSystemClient, self.clientQueue);
        });
        self.clientQueue = NULL;
    }
    if (self.eventSystemClient) {
        CFRelease(self.eventSystemClient);
        self.eventSystemClient = NULL;
    }

    if (self.testEventSystemClient) {
        CFRelease(self.testEventSystemClient);
        self.testEventSystemClient = NULL;
    }

    [super tearDown];
}

- (void)testServiceClient {
    
    usleep(kDefaultReportDispatchCompletionTime);
    
    NSArray * services = CFBridgingRelease(IOHIDEventSystemClientCopyServices(self.eventSystemClient));
    XCTAssert (services != nil && services.count == 1, "Copy services:%@", services);
               
    XCTAssert (IOHIDServiceClientGetTypeID() == CFGetTypeID((IOHIDServiceClientRef)services[0]));
    
    NSNumber * registryID = IOHIDServiceClientGetRegistryID ((IOHIDServiceClientRef)services[0]);
    XCTAssert (registryID != nil);
    
    IOHIDServiceClientRef serviceClient = IOHIDEventSystemClientCopyServiceForRegistryID (self.eventSystemClient, registryID.unsignedLongLongValue);
    XCTAssert  ((IOHIDServiceClientRef)CFBridgingRetain(services[0]) == serviceClient);

    XCTAssert(IOHIDServiceClientConformsTo (serviceClient, kHIDPage_AppleVendor, kHIDUsage_AppleVendor_Message));
    
    XCTAssert (IOHIDEventSystemClientRegistryIDConformsTo (self.eventSystemClient, registryID.unsignedLongLongValue, kHIDPage_AppleVendor, kHIDUsage_AppleVendor_Message) == true);

    CFStringRef testPropertyKey = CFSTR ("ClientTestProperty");
    IOHIDServiceClientSetProperty(serviceClient, testPropertyKey, testPropertyKey);
    CFTypeRef value = IOHIDServiceClientCopyProperty (serviceClient, testPropertyKey);
    
    XCTAssert (CFEqual(testPropertyKey, value));
    if (value) {
        CFRelease(value);
    }
    if (serviceClient) {
        CFRelease (serviceClient);
    }
    
    @autoreleasepool {
        self.sourceController = nil;
        services = nil;
        registryID = nil;
    }

    usleep(kDefaultReportDispatchCompletionTime);
    
    services = CFBridgingRelease(IOHIDEventSystemClientCopyServices(self.eventSystemClient));
    XCTAssert (services == nil || (services && services.count == 0), "Copy service: %@", services);

}


- (void)testSystemProperty {
    CFStringRef testPropertyKey = CFSTR ("TestProperty");
    IOHIDEventSystemClientSetProperty(self.testEventSystemClient, testPropertyKey, testPropertyKey);
    CFTypeRef value = IOHIDEventSystemClientCopyProperty (self.testEventSystemClient, testPropertyKey);
    XCTAssert (CFEqual(testPropertyKey, value));
    if (value) {
        CFRelease(value);
    }
}


- (void)testEventDispatch {

    IOReturn status;
    
    HIDVendorMessage32BitDescriptorInputReport report;
    
    IOHIDEventBlock handler =  ^(void * __unused _Nullable target, void * __unused _Nullable refcon, void * __unused _Nullable sender, IOHIDEventRef __unused event) {
        [self.events addObject:(__bridge id) event];
    };
    
    dispatch_sync (self.clientQueue, ^{
        IOHIDEventSystemClientRegisterEventBlock (self.eventSystemClient, handler, (__bridge void * _Nullable)(self), NULL);
    });
    
    sleep (kServiceMatchingTimeout);
    
    report.VEN_VendorDefined0023 = 1;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 2;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 3;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    XCTAssertTrue(self.events.count == 3, "events:%@", self.events);
    
    dispatch_sync (self.clientQueue, ^{
        IOHIDEventSystemClientUnregisterEventBlock (self.eventSystemClient, handler, (__bridge void * _Nullable)(self), NULL);
    });

    [self.events removeAllObjects];
    
    report.VEN_VendorDefined0023 = 1;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 2;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 3;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
     XCTAssertTrue(self.events.count == 0, "events:%@", self.events);
}


- (void)testRunloopEventDispatch {
    
    IOReturn status;
    
    HIDVendorMessage32BitDescriptorInputReport report;
    
    IOHIDEventBlock handler =  ^(void * __unused _Nullable target, void * __unused _Nullable refcon, void *  __unused _Nullable sender, __unused IOHIDEventRef event) {
        [self.events addObject:(__bridge id) event];
    };
    
    dispatch_sync (self.clientQueue, ^{
        IOHIDEventSystemClientUnscheduleFromDispatchQueue(self.eventSystemClient, self.clientQueue);
    });

    
    CFRunLoopRef runLoop = IOHIDUnitTestCreateRunLoop (31);
    HIDXCTAssertAndThrowTrue(runLoop != NULL);
   
    CFRunLoopPerformBlock(runLoop, kCFRunLoopDefaultMode, ^{
        IOHIDEventSystemClientUnregisterEventBlock (self.eventSystemClient, handler, (__bridge void * _Nullable)(self), NULL);
        IOHIDEventSystemClientRegisterEventBlock (self.eventSystemClient, handler, (__bridge void * _Nullable)(self), NULL);
        IOHIDEventSystemClientScheduleWithRunLoop (self.eventSystemClient, runLoop, kCFRunLoopDefaultMode);
        CFTypeRef val = IOHIDEventSystemClientCopyProperty(self.eventSystemClient, CFSTR (kIOHIDEventSystemClientIsUnresponsive));
        if (val) {
            CFRelease(val);
        }
    });
    CFRunLoopWakeUp(runLoop);

    sleep (kServiceMatchingTimeout);
    
    report.VEN_VendorDefined0023 = 1;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 2;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 3;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    XCTAssertTrue(self.events.count == 3, "events:%@", self.events);

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    CFRunLoopPerformBlock(runLoop, kCFRunLoopDefaultMode, ^{
        IOHIDEventSystemClientUnregisterEventBlock (self.eventSystemClient, handler, (__bridge void * _Nullable)(self), NULL);
        IOHIDEventSystemClientUnscheduleWithRunLoop (self.eventSystemClient, runLoop, kCFRunLoopDefaultMode);
        dispatch_semaphore_signal(sema);
    });
    CFRunLoopWakeUp(runLoop);
    XCTAssert(dispatch_semaphore_wait (sema, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC)) == 0);
    
    IOHIDUnitTestDestroyRunLoop (runLoop);
}


- (void)testEventEventConnectionFilter {

    IOReturn status;

    HIDVendorMessage32BitDescriptorInputReport report;
    
    dispatch_sync (self.clientQueue, ^{
        IOHIDEventSystemClientRegisterEventCallback (self.eventSystemClient, EventSystemEventCallback, (__bridge void * _Nullable)(self), NULL);
    });
    
    sleep (kServiceMatchingTimeout);
    
    report.VEN_VendorDefined0023 = 1;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    XCTAssertTrue(self.events.count == 1, "events:%@", self.events);
    
    dispatch_sync (self.clientQueue, ^{
        NSArray * eventFilterMask = @[];
        IOHIDEventSystemClientSetProperty (self.eventSystemClient, CFSTR(kIOHIDClientEventFilterKey),(CFArrayRef)eventFilterMask);
    });
    
    
    report.VEN_VendorDefined0023 = 2;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    XCTAssertTrue(self.events.count == 1, "events:%@", self.events);


    dispatch_sync (self.clientQueue, ^{
        NSArray * eventFilterMask = @[@(kIOHIDEventTypeVendorDefined)];
        IOHIDEventSystemClientSetProperty (self.eventSystemClient, CFSTR(kIOHIDClientEventFilterKey),(CFArrayRef)eventFilterMask);
    });

    report.VEN_VendorDefined0023 = 3;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    XCTAssertTrue(self.events.count == 2, "events:%@", self.events);

}

- (void)testEventFilterClient {
  
    IOReturn status;
    
    HIDVendorMessage32BitDescriptorInputReport report;

    dispatch_sync (self.clientQueue, ^{
        IOHIDEventSystemClientRegisterEventCallback (self.eventSystemClient, EventSystemEventCallback, (__bridge void * _Nullable)(self), NULL);
        IOHIDEventSystemClientRegisterEventFilterCallback (self.eventSystemClient, EventSystemEventFilterCallback, (__bridge void * _Nullable)(self), NULL);
    });

    sleep (kServiceMatchingTimeout);

    
    report.VEN_VendorDefined0023 = 1;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 2;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 3;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
  
     // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    XCTAssertTrue(self.filteredEvents.count == 1 && self.events.count == 2, "filteredEventd:%@ eventCount:%@", self.filteredEvents, self.events);

    dispatch_sync (self.clientQueue, ^{
        IOHIDEventSystemClientUnregisterEventCallback (self.eventSystemClient, EventSystemEventCallback, (__bridge void * _Nullable)(self), NULL);
        IOHIDEventSystemClientUnregisterEventFilterCallback (self.eventSystemClient, EventSystemEventFilterCallback, (__bridge void * _Nullable)(self), NULL);
    });

    [self.filteredEvents removeAllObjects];
    [self.events removeAllObjects];

  
    report.VEN_VendorDefined0023 = 1;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 2;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.VEN_VendorDefined0023 = 3;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
  
    XCTAssertTrue(self.filteredEvents.count == 0 && self.events.count == 0, "filteredEventd:%@ eventCount:%@", self.filteredEvents, self.events);

    XCTAssert(self.resetCount == 0);
}

- (void)testPropertyNotification {

    self.propertyCount = 0;
    
    CFStringRef testPropertyKey = CFSTR ("TestPropertyCallback");

    dispatch_sync (self.clientQueue, ^{
        IOHIDEventSystemClientRegisterPropertyChangedCallback (self.eventSystemClient, testPropertyKey , EventSystemPropertyChangedCallback, (__bridge void * _Nullable)(self), NULL);
    });


    usleep(kDefaultReportDispatchCompletionTime);

    for (int i = 0; i < 10; i++) {
        NSNumber * value = @(i);
        Boolean status = IOHIDEventSystemClientSetProperty(self.testEventSystemClient, testPropertyKey, (__bridge CFTypeRef _Nonnull)(value));
        XCTAssert(status);
    }

    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    XCTAssertTrue (self.propertyCount == 10, "propertyCount:%ld expected:%d", (long)self.propertyCount , 10);

    dispatch_sync (self.clientQueue, ^{
        IOHIDEventSystemClientUnregisterPropertyChangedCallback (self.eventSystemClient, testPropertyKey , EventSystemPropertyChangedCallback, (__bridge void * _Nullable)(self), NULL);
    });

    self.propertyCount = 0;

    for (int i = 0; i < 10; i++) {
        Boolean status = IOHIDEventSystemClientSetProperty(self.testEventSystemClient, testPropertyKey, testPropertyKey);
        XCTAssert(status);
    }

    usleep(kDefaultReportDispatchCompletionTime);
    
    XCTAssertTrue (self.propertyCount == 0, "propertyCount:%ld expected:%d", (long)self.propertyCount , 0);
}

- (void)testClientEventDispatch {
    
    UInt32 value = 0x55555555;
    
    IOHIDEventRef event;
    event = IOHIDEventCreateVendorDefinedEvent(kCFAllocatorDefault,
                                               mach_absolute_time(),
                                               kHIDPage_AppleVendor,
                                               kHIDUsage_AppleVendor_Message,
                                               0,
                                               (uint8_t*)&value,
                                               sizeof(value),
                                               0);
    
    TestLog("IOHIDEventSystemClientDispatchEvent:%@", event);
    IOHIDEventSystemClientDispatchEvent(self.testEventSystemClient, event);
    CFRelease(event);
    
    event = IOHIDEventCreateVendorDefinedEvent(kCFAllocatorDefault,
                                               mach_absolute_time(),
                                               kHIDPage_AppleVendor,
                                               kHIDUsage_AppleVendor_Message,
                                               0,
                                               (uint8_t*)&value,
                                               sizeof(value),
                                               0);
    
    TestLog("IOHIDEventSystemClientDispatchEvent:%@", event);
    IOHIDEventSystemClientDispatchEvent(self.testEventSystemClient, event);
    CFRelease(event);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    XCTAssert(self.events.count == 2, "events:%@", self.events);
}


- (void)testEventSystemDebugDump {
    CFArrayRef value;
    
    value = IOHIDEventSystemClientCopyProperty(self.testEventSystemClient, CFSTR(kIOHIDClientRecordsKey));
    XCTAssert(value != NULL && CFGetTypeID(value) == CFArrayGetTypeID() && CFArrayGetCount(value) != 0, "kIOHIDClientRecordsKey:%@",  value);
    
    value = IOHIDEventSystemClientCopyProperty(self.testEventSystemClient, CFSTR(kIOHIDServiceRecordsKey));
    XCTAssert(value != NULL && CFGetTypeID(value) == CFArrayGetTypeID() && CFArrayGetCount(value) != 0, "kIOHIDServiceRecordsKey:%@",  value);

    value = IOHIDEventSystemClientCopyProperty(self.testEventSystemClient, CFSTR(kIOHIDSessionFilterDebugKey));
    XCTAssert(value != NULL && CFGetTypeID(value) == CFArrayGetTypeID() && CFArrayGetCount(value) != 0, "kIOHIDSessionFilterDebugKey:%@",  value);

}


-(void) propertyChangeCallback: (nonnull __unused CFStringRef) property And: (nullable __unused CFTypeRef) value {
    ++self.propertyCount;
}

-(void) eventCallback: (nonnull __unused IOHIDEventRef) event  For: (nullable __unused IOHIDServiceClientRef) service  {
   [self.events addObject:(__bridge id) event];
}

-(BOOL) filterCallback: (nonnull __unused IOHIDEventRef) event  For: (nullable __unused IOHIDServiceClientRef) service  {
    if (IOHIDEventConformsTo(event, kIOHIDEventTypeVendorDefined)) {
        uint8_t *payload  = NULL;
        CFIndex  lenght   = 0;
        IOHIDEventGetVendorDefinedData (event, &payload, &lenght);
        if (*payload == 2) {
            [self.filteredEvents addObject:(__bridge id) event];
            return YES;
        }
    }
    return NO;
}

-(void) resetCallback {
    ++self.resetCount;
}

@end


boolean_t EventSystemEventFilterCallback (void * _Nullable target, void * _Nullable refcon __unused, void * _Nullable sender, IOHIDEventRef event) {
    TestEventSystemClient *self = (__bridge TestEventSystemClient *)target;
    return [self filterCallback : event For:(IOHIDServiceClientRef)sender];
}

void EventSystemResetCallback(void * _Nullable target, void * _Nullable context __unused) {
    TestEventSystemClient *self = (__bridge TestEventSystemClient *)target;
    [self resetCallback];
}

void EventSystemEventCallback (void * _Nullable target, void * _Nullable refcon __unused, void * _Nullable sender, IOHIDEventRef event) {
    TestEventSystemClient *self = (__bridge TestEventSystemClient *)target;
    [self eventCallback : event For:(IOHIDServiceClientRef)sender];
}

void EventSystemPropertyChangedCallback (void * _Nullable target, void * _Nullable __unused context, CFStringRef property, CFTypeRef value) {
    TestEventSystemClient *self = (__bridge TestEventSystemClient *)target;
    [self propertyChangeCallback : property And:value];
}
