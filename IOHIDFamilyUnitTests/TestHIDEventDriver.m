//
//  TestHIDEventDriver.m
//  IOHIDFamily
//
//  Created by yg on 2/6/17.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDEventSystemTestController.h"
#include "IOHIDUserDeviceTestController.h"
#include "IOHIDUnitTestDescriptors.h"
#include <IOKit/hid/IOHIDEventSystemPrivate.h>



@interface TestHIDEventDriver : XCTestCase

@property dispatch_queue_t                  eventControllerQueue;
@property dispatch_queue_t                  rootQueue;
@property IOHIDEventSystemTestController *  eventController;
@property IOHIDUserDeviceTestController *   sourceController;

@end

@implementation TestHIDEventDriver

- (void)setUp {
    [super setUp];

    self.rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
    
    self.eventControllerQueue = dispatch_queue_create_with_target ("IOHIDEventSystemTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.eventControllerQueue != nil);

}

- (void)tearDown {
    if (self.sourceController) {
        [self.sourceController invalidate];
    }

    @autoreleasepool {
        self.sourceController = nil;
    }
    
    if (self.eventController) {
        
        // Check if event system reset occur
        XCTAssert(self.eventController.eventSystemResetCount == 0, "IOHIDEventSystem resetCount:%d", (int)self.eventController.eventSystemResetCount);
       
        [self.eventController invalidate];
    }

    @autoreleasepool {
        self.eventController = nil;
    }

    [super tearDown];
}

-(void)setupTestSystem : (NSData *) descriptorData {
    
    if (self.sourceController) {
        [self.sourceController invalidate];
    }
    @autoreleasepool {
        self.sourceController = nil;
    }
    
    if (self.eventController) {
        // Check if event system reset occur
        XCTAssert(self.eventController.eventSystemResetCount == 0, "IOHIDEventSystem resetCount:%d", (int)self.eventController.eventSystemResetCount);
        [self.eventController invalidate];
    }
    
    @autoreleasepool {
        self.eventController = nil;
    }
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    self.sourceController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:uniqueID andQueue:nil];
    HIDXCTAssertAndThrowTrue(self.sourceController != nil);
    
    self.eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:uniqueID :self.eventControllerQueue :kIOHIDEventSystemClientTypeAdmin];
    HIDXCTAssertAndThrowTrue(self.eventController != nil);
}


- (void)MAC_OS_ONLY_TEST_CASE(testGameController) {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDGameControllerDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDGameControllerDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    for (int index = 0; index < 10; index++) {
        report.BTN_CountedBufferGamePadButton2 = 20 + index;
        report.BTN_CountedBufferGamePadButton3 = 20 + index;
        report.GD_CountedBufferGamePadPointerX = 20 + index;
        report.GD_CountedBufferGamePadPointerY = 20 + index;
        status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
        XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    }
   
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];

    HIDTestEventLatency(stats);
    
    XCTAssert(stats.totalCount == 9,
                  "events count:%lu expected:%d events:%@ ", (unsigned long)stats.totalCount , 9, events);

    XCTAssert (stats.counts[kIOHIDEventTypeGameController] == 9, "kIOHIDEventTypeGameController count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeGameController], events);
}


- (void)testHeadsetTelephony {
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDHeadsetTelephonyDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDHeadsetTelephonyDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    for (int index = 0; index < 10; index++) {
        report.CD_ConsumerControlVolumeIncrement = 1;
        report.CD_ConsumerControlVolumeDecrement = 0;
        status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
        XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
        report.CD_ConsumerControlVolumeIncrement = 0;
        report.CD_ConsumerControlVolumeDecrement = 1;
        status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
        XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    }

    report.CD_ConsumerControlVolumeIncrement = 0;
    report.CD_ConsumerControlVolumeDecrement = 0;
    
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);

    XCTAssert(stats.totalCount == 40,
              "events count:%lu expected:%d events:%@\n", (unsigned long)stats.totalCount , 40, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeKeyboard] == 40, "kIOHIDEventTypeKeyboard count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeKeyboard], events);
}

- (void)testMediaPlayback {
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDMediaPlaybackDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDMediaPlaybackDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    report.CD_ConsumerControlScanNextTrack = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.CD_ConsumerControlScanNextTrack = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    report.CD_ConsumerControlMute = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.CD_ConsumerControlMute = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
  
    report.CD_ConsumerControlRandomPlay = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.CD_ConsumerControlRandomPlay = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    report.CD_ConsumerControlRepeat = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.CD_ConsumerControlRepeat = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    report.CD_ConsumerControlAcPromote = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.CD_ConsumerControlAcPromote = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
   
    report.CD_ConsumerControlAcDemote = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.CD_ConsumerControlAcDemote = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    report.CD_ConsumerControlAcAddToCart = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.CD_ConsumerControlAcAddToCart = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);

    XCTAssert(stats.totalCount == 14,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 14, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeKeyboard] == 14, "kIOHIDEventTypeKeyboard count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeKeyboard], events);
}
 
    
- (void)testMediaPlaybackAndTelephony {
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDTelephonyAndMediaPlaybackDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDTelephonyAndMediaPlaybackDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    report.TEL_ConsumerControlFlash = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.TEL_ConsumerControlFlash = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);

    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d \n%@\n", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeKeyboard] == 2, "kIOHIDEventTypeKeyboard count:%lu", (unsigned long)stats.counts[kIOHIDEventTypeNULL]);
}
    

- (void)EMBEDDED_OS_ONLY_TEST_CASE(testSingleTouch) {

    IOReturn status;
    
    static uint8_t descriptor [] = {HIDSingleTouchDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDSingleTouchDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    report.DIG_TouchScreenFingerTouch = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.DIG_TouchScreenFingerTouch = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);

    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeDigitizer] == 2, "kIOHIDEventTypeDigitizer count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeDigitizer], events);
}

- (void)EMBEDDED_OS_ONLY_TEST_CASE(testBasicGestureRecognition) {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDBasicGestureRecognitionDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDBasicGestureRecognitionDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    
    report.DIG_TouchPadFingerTouch = 0x1;
    
    report.DIG_TouchPad0063 = 0x1;
    report.DIG_TouchPad0062 = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.DIG_TouchPad0063 = 0x202;
    report.DIG_TouchPad0062 = 2;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.DIG_TouchPad0063 = 0x030303;
    report.DIG_TouchPad0062 = 3;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    report.DIG_TouchPad0063 = 0x04040404;
    report.DIG_TouchPad0062 = 4;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);

    //
    // If service dispatch collection Digitizer->collectionDispatch  then  for each report with touch on we will dispatch digitizer event
    // regardless of whatever any other params have changed. @todo we might be smart in kernel to handle this case but for now fix test
    //
    NSDictionary * digitizerProp = (NSDictionary *)CFBridgingRelease(IOHIDServiceClientCopyProperty (self.eventController.eventService, CFSTR("Digitizer")));
    
    uint32_t numOfDigitizerEvents = 1;
    
    if (digitizerProp && digitizerProp [@"collectionDispatch"] && ((NSNumber *)digitizerProp [@"collectionDispatch"]).boolValue == YES) {
        numOfDigitizerEvents = 4;
    }
    
    XCTAssert(stats.totalCount == numOfDigitizerEvents + 4,
              "events count:%d expected:%d events:%@", stats.totalCount , numOfDigitizerEvents + 4, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeUnicode] == 4 && stats.counts[kIOHIDEventTypeDigitizer] == numOfDigitizerEvents, "kIOHIDEventTypeUnicode:%lu kIOHIDEventTypeDigitizer:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeUnicode], (unsigned long)stats.counts[kIOHIDEventTypeDigitizer] , events);
}


- (void)EMBEDDED_OS_ONLY_TEST_CASE(testGestureRecognitionAndAltInterpretation) {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDGestureRecognitionAndAltInterpretationDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDGestureRecognitionAndAltInterpretationDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    
    report.DIG_TouchPadFingerTouch = 0x1;
    report.DIG_TouchPad0063 = 0x1;
    report.DIG_TouchPad0062 = 1;
    report.DIG_TouchPad0061 = 80;
    report.DIG_VendorDefined0063 = 0x202;
    report.DIG_VendorDefined0062 = 2;
    report.DIG_VendorDefined0061 = 90;

    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);

    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeUnicode] == 1 && stats.counts[kIOHIDEventTypeDigitizer] == 1 , "kIOHIDEventTypeUnicode count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeUnicode], events);
}

- (void)testMultiAxisController {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDMultiAxisControllerDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDMultiAxisControllerDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    
    report.GD_MultiaxisControllerX = 80;
    report.GD_MultiaxisControllerY = 80;
    report.BTN_MultiaxisControllerButton1 = 01;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    report.GD_MultiaxisControllerX = 00;
    report.GD_MultiaxisControllerY = 00;
    report.BTN_MultiaxisControllerButton1 = 00;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);

    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypePointer] == 2, "kIOHIDEventTypePointer count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypePointer], events);
}

- (void) testMediaButtons {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDSimpleMediaButtonsDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDSimpleMediaButtonsDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    
    report.CD_ConsumerControlScanNextTrack = 1;
    report.CD_ConsumerControlScanPreviousTrack = 1;
    report.CD_ConsumerControlPlayPause = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    report.CD_ConsumerControlScanNextTrack = 0;
    report.CD_ConsumerControlScanPreviousTrack = 0;
    report.CD_ConsumerControlPlayPause = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);

    XCTAssert(stats.totalCount == 6,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 6, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeKeyboard] == 6, "kIOHIDEventTypeKeyboard count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeKeyboard], events);
}

- (void) testTelephonyButtons {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDTelephonyButtonsFlashNumpadDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDTelephonyButtonsFlashNumpadDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    
    report.TEL_PhoneFlash = 1;
    report.TEL_PhonePhoneMute = 1;
    report.TEL_PhonePhoneKey0 = 1;
    report.TEL_PhonePhoneKey9 = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    report.TEL_PhoneFlash = 0;
    report.TEL_PhonePhoneMute = 0;
    report.TEL_PhonePhoneKey0 = 0;
    report.TEL_PhonePhoneKey9 = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);

    XCTAssert(stats.totalCount == 8,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 8, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeKeyboard] == 8, "kIOHIDEventTypeKeyboard count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeKeyboard], events);
    
}

- (void) testDigitizerTouchCancel {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDDigitizerWithTouchCancel};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDDigitizerWithTouchCancelInputReport report;
    memset (&report, 0 , sizeof(report));
    
    
    report.DIG_TouchPadFingerTransducerIndex = 1;
    report.DIG_TouchPadFingerTouch = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    report.DIG_TouchPadFingerUntouch = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    report.DIG_TouchPadFingerTouch = 0;
    report.DIG_TouchPadFingerUntouch = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);
    
    XCTAssert(stats.totalCount == 3,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 3, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeDigitizer] == 3, "kIOHIDEventTypeDigitizer count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeDigitizer], events);
    
    IOHIDEventRef event;
    CFIndex eventMask;

    event = (__bridge IOHIDEventRef) events[0];
    eventMask = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerEventMask);
    XCTAssert ((eventMask & kIOHIDDigitizerEventCancel) == 0);

    event = (__bridge IOHIDEventRef) events[1];
    eventMask = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerEventMask);
    XCTAssert ((eventMask & kIOHIDDigitizerEventCancel) == kIOHIDDigitizerEventCancel);

    event = (__bridge IOHIDEventRef) events[2];
    eventMask = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerEventMask);
    XCTAssert ((eventMask & kIOHIDDigitizerEventCancel) == 0);

}

- (void)MAC_OS_ONLY_TEST_CASE(testMouse) {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDMouseDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    [self setupTestSystem :  descriptorData];

    HIDMouseDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    report.GD_MousePointerX = 1;
    report.GD_MousePointerY = 1;
 
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    report.GD_MousePointerX = 0;
    report.GD_MousePointerY = 0;
    report.GD_MousePointerWheel = 1;
    
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);
    
    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypePointer] == 1 && stats.counts[kIOHIDEventTypeScroll] == 1, "Events:%@", events);

    
}

- (void)MAC_OS_ONLY_TEST_CASE(testAbsolutePointer) {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDPointerAbsoluteDescriptor};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    [self setupTestSystem :  descriptorData];
    
    HIDPointerAbsoluteDescriptorInputReport report;
    memset (&report, 0 , sizeof(report));
    
    report.GD_MousePointerX = 1;
    report.GD_MousePointerY = 1;
    
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    report.GD_MousePointerX = 1000;
    report.GD_MousePointerY = 1000;
    
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);
    
    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypePointer] == 2, "Events:%@", events);

    XCTAssert (IOHIDEventIsAbsolute((IOHIDEventRef)events[0]), "Events:%@", events);
    XCTAssert (IOHIDEventIsAbsolute((IOHIDEventRef)events[1]), "Events:%@", events);

}

- (void)testBiometricHumanPresenceAndProximity {

    IOReturn status;
    
    static uint8_t descriptor [] = {HIDBiometricHumanPresenceAndProximity};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
  
    [self setupTestSystem :  descriptorData];
    
    HIDBiometricHumanPresenceAndProximityInputReport report;
    memset (&report, 0 , sizeof(report));
    
    report.SNS_BiometricHumanPresenceDataFieldHumanPresence = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];

    report.SNS_BiometricHumanPresenceDataFieldHumanPresence = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];

    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeBiometric] == 2, "Events:%@", events);

}

- (void)EMBEDDED_OS_ONLY_TEST_CASE(testCarpaySelectButton) {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDCarplaySelectButton};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    [self setupTestSystem :  descriptorData];
    
    HIDCarplaySelectButtonInputReport report;
    memset (&report, 0 , sizeof(report));
    
    report.BTN_ConsumerControlButton1 = 1;
    
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    report.BTN_ConsumerControlButton1 = 0;

    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);
    
    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypePointer] == 2, "Events:%@", events);
}

- (void)testAccel {
    
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDAccel};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDAccelInputReport01 report;
    
    memset (&report, 0 , sizeof(report));
    
    report.reportId = 1;
    
    report.MotionAccelerometer3D000B = 1000;
    
    report.SNS_MotionAccelerometer3DDataFieldAccelerationAxisX = 1000;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    
    report.SNS_MotionAccelerometer3DDataFieldAccelerationAxisY = 1000;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
 
    report.SNS_MotionAccelerometer3DDataFieldAccelerationAxisZ = 1000;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    
    // just repeat last report
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];

    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    XCTAssert(stats.totalCount == 4,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeAccelerometer] == 4, "Events:%@", events);
}


- (void)testClefEvents {
    IOReturn status;
    
    static uint8_t descriptor [] = {HIDClef};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDClefInputReport01 report01;
    memset (&report01, 0 , sizeof(report01));
    
    report01.reportId = 1;
    report01.VEN_VendorDefined000C = 0x010101;
    report01.VEN_VendorDefined0001 = 0x010101;
    status = [self.sourceController handleReport:(uint8_t*)&report01 Length:sizeof(report01) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    status = [self.sourceController handleReport:(uint8_t*)&report01 Length:sizeof(report01) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    
    HIDClefInputReport02 report02;
    memset (&report02, 0 , sizeof(report02));
    report02.reportId = 2;
    report02.VEN_VendorDefined0002 = 0x020202;
    report02.VEN_VendorDefined000C = 0x020202;
    
    status = [self.sourceController handleReport:(uint8_t*)&report02 Length:sizeof(report02) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    status = [self.sourceController handleReport:(uint8_t*)&report02 Length:sizeof(report02) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    XCTAssert(stats.totalCount == 4,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    for (id event in events) {
        NSArray * children = ( NSArray * ) IOHIDEventGetChildren((IOHIDEventRef)event);
        XCTAssert(children && children.count == 1, "event:%@", event);
    }
    
    XCTAssert (stats.counts[kIOHIDEventTypeVendorDefined] == 4, "Events:%@", events);
}


#define OrientationReportForUsage(u) (u - kHIDUsage_AppleVendorMotion_DeviceOrientationTypeAmbiguous + 1)

- (void)testOrientationEvents {
    IOReturn status;
    CFIndex  value;
    
    static uint8_t descriptor [] = {HIDOrientation};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];

    
    IOHIDEventRef event = IOHIDServiceClientCopyEvent (self.eventController.eventService, kIOHIDEventTypeOrientation, NULL, 0);
    XCTAssert(event == NULL, "Event:%@", event);

    HIDOrientationInputReport report;
    memset (&report, 0 , sizeof(report));
    
    report.OrientationDeviceOrientation = OrientationReportForUsage(kHIDUsage_AppleVendorMotion_DeviceOrientationTypeAmbiguous);
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    event = IOHIDServiceClientCopyEvent (self.eventController.eventService, kIOHIDEventTypeOrientation, NULL, 0);
    HIDXCTAssertAndThrowTrue(event != NULL);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationOrientationType);
    XCTAssert (value == kIOHIDOrientationTypeCMUsage);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationDeviceOrientationUsage);
    XCTAssert (value == kHIDUsage_AppleVendorMotion_DeviceOrientationTypeAmbiguous, "Event:%@", event);

    CFRelease (event);
    
    report.OrientationDeviceOrientation = OrientationReportForUsage(kHIDUsage_AppleVendorMotion_DeviceOrientationTypePortrait);
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    event = IOHIDServiceClientCopyEvent (self.eventController.eventService, kIOHIDEventTypeOrientation, NULL, 0);
    HIDXCTAssertAndThrowTrue(event != NULL);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationOrientationType);
    XCTAssert (value == kIOHIDOrientationTypeCMUsage);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationDeviceOrientationUsage);
    XCTAssert (value == kHIDUsage_AppleVendorMotion_DeviceOrientationTypePortrait, "Event:%@", event);

    CFRelease (event);

    report.OrientationDeviceOrientation = OrientationReportForUsage(kHIDUsage_AppleVendorMotion_DeviceOrientationTypeAmbiguous);
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    event = IOHIDServiceClientCopyEvent (self.eventController.eventService, kIOHIDEventTypeOrientation, NULL, 0);
    HIDXCTAssertAndThrowTrue(event != NULL);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationOrientationType);
    XCTAssert (value == kIOHIDOrientationTypeCMUsage);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationDeviceOrientationUsage);
    XCTAssert (value == kHIDUsage_AppleVendorMotion_DeviceOrientationTypeAmbiguous, "Event:%@", event);

    CFRelease (event);

    report.OrientationDeviceOrientation = OrientationReportForUsage(kHIDUsage_AppleVendorMotion_DeviceOrientationTypePortraitUpsideDown);
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    event = IOHIDServiceClientCopyEvent (self.eventController.eventService, kIOHIDEventTypeOrientation, NULL, 0);
    HIDXCTAssertAndThrowTrue(event != NULL);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationOrientationType);
    XCTAssert (value == kIOHIDOrientationTypeCMUsage);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationDeviceOrientationUsage);
    XCTAssert (value == kHIDUsage_AppleVendorMotion_DeviceOrientationTypePortraitUpsideDown, "Event:%@", event);

    CFRelease (event);

    // out of bounds value
    report.OrientationDeviceOrientation = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    XCTAssert(stats.totalCount == 4,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeOrientation] == 4, "Events:%@", events);
}


- (void)MAC_OS_ONLY_TEST_CASE(testGamecontrollerEventWithTubmstickButtons) {
    
    IOReturn status;
    CFIndex  value;
    
    static uint8_t descriptor [] = {HIDGameControllerWithThumbstickButtons};
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem :  descriptorData];
    
    HIDGameControllerWithThumbstickButtonsInputReport report;
    memset (&report, 0 , sizeof(report));
 
    report.BTN_CountedBufferGamePadButton9 = 1;
    report.BTN_CountedBufferGamePadButton10 = 1;

    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];

    report.BTN_CountedBufferGamePadButton9 = 0;
    report.BTN_CountedBufferGamePadButton10 = 0;

    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }

    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    HIDTestEventLatency(stats);
    
    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d events:%@ ", (unsigned long)stats.totalCount , 2, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeGameController] == 2, "kIOHIDEventTypeGameController count:%lu events:%@", (unsigned long)stats.counts[kIOHIDEventTypeGameController], events);
    

    value = IOHIDEventGetIntegerValue ((IOHIDEventRef)events[0], kIOHIDEventFieldGameControllerThumbstickButtonRight);
    XCTAssert (value == 1);

    value = IOHIDEventGetIntegerValue ((IOHIDEventRef)events[0], kIOHIDEventFieldGameControllerThumbstickButtonLeft);
    XCTAssert (value == 1);

    value = IOHIDEventGetIntegerValue ((IOHIDEventRef)events[1], kIOHIDEventFieldGameControllerThumbstickButtonRight);
    XCTAssert (value == 0);
    
    value = IOHIDEventGetIntegerValue ((IOHIDEventRef)events[1], kIOHIDEventFieldGameControllerThumbstickButtonLeft);
    XCTAssert (value == 0);
}
- (void)testCameraEvents {
    IOReturn status;
    static uint8_t descriptor[] = { HIDCameraDescriptor };
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    [self setupTestSystem:descriptorData];
    
    HIDCameraDescriptorInputReport report = { 0 };
    
    report.CAM_VendorDefinedCameraShutter = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    
    report.CAM_VendorDefinedCameraShutter = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    
    report.CAM_VendorDefinedCameraAutoFocus = 1;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    
    report.CAM_VendorDefinedCameraAutoFocus = 0;
    status = [self.sourceController handleReport:(uint8_t*)&report Length:sizeof(report) andInterval:2000];
    
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
    
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    XCTAssert(stats.totalCount == 4,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 4, events);
    
    XCTAssert (stats.counts[kIOHIDEventTypeKeyboard] == 4, "Events:%@", events);
}

@end


