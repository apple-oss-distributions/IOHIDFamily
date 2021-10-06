/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2013 Apple Computer, Inc.  All Rights Reserved.
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

#include <AssertMacros.h>
#include <TargetConditionals.h>
#include <stdint.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/IOLib.h>
#include <IOKit/usb/USB.h>

#include "IOHIDKeys.h"
#include "IOHIDSystem.h"
#include "IOHIDEventService.h"
#include "IOHIDInterface.h"
#include "IOHIDPrivateKeys.h"
#include "AppleHIDUsageTables.h"
#include "OSStackRetain.h"

#if !TARGET_OS_EMBEDDED
    #include "IOHIDPointing.h"
    #include "IOHIDKeyboard.h"
    #include "IOHIDConsumer.h"
#endif /* !TARGET_OS_EMBEDDED */

#include "IOHIDEventData.h"

#include "IOHIDFamilyPrivate.h"
#include "IOHIDevicePrivateKeys.h"
#include "ev_private.h"
#include "IOHIDFamilyTrace.h"

extern "C" int  kern_stack_snapshot_with_reason(char *reason);
extern "C" kern_return_t sysdiagnose_notify_user(uint32_t keycode);

enum {
    kBootProtocolNone   = 0,
    kBootProtocolKeyboard,
    kBootProtocolMouse
};

enum {
    kShimEventProcessor = 0x01
};

#define     kDefaultFixedResolution             (400 << 16)
#define     kDefaultScrollFixedResolution       (9 << 16)

#define     kMaxSystemAbsoluteRangeUnsigned     65535
#define     kMaxSystemAbsoluteRangeSigned       32767
#define     kMaxSystemBarrelPressure            kMaxSystemAbsoluteRangeSigned
#define     kMaxSystemTipPressure               kMaxSystemAbsoluteRangeUnsigned

#define     kDelayedOption                      (1<<31)

#define     NUB_LOCK                            if (_nubLock) IORecursiveLockLock(_nubLock)
#define     NUB_UNLOCK                          if (_nubLock) IORecursiveLockUnlock(_nubLock)

#if TARGET_OS_EMBEDDED
    #define     SET_HID_PROPERTIES_EMBEDDED(service)                                \
        service->setProperty(kIOHIDPrimaryUsagePageKey, getPrimaryUsagePage(), 32); \
        service->setProperty(kIOHIDPrimaryUsageKey, getPrimaryUsage(), 32);
#else
    #define     SET_HID_PROPERTIES_EMBEDDED(service)                                \
        {};
#endif


#define     SET_HID_PROPERTIES(service)                                     \
    service->setProperty(kIOHIDTransportKey, getTransport());               \
    service->setProperty(kIOHIDLocationIDKey, getLocationID(), 32);         \
    service->setProperty(kIOHIDVendorIDKey, getVendorID(), 32);             \
    service->setProperty(kIOHIDVendorIDSourceKey, getVendorIDSource(), 32); \
    service->setProperty(kIOHIDProductIDKey, getProductID(), 32);           \
    service->setProperty(kIOHIDVersionNumberKey, getVersion(), 32);         \
    service->setProperty(kIOHIDCountryCodeKey, getCountryCode(), 32);       \
    service->setProperty(kIOHIDManufacturerKey, getManufacturer());         \
    service->setProperty(kIOHIDProductKey, getProduct());                   \
    service->setProperty(kIOHIDSerialNumberKey, getSerialNumber());         \
    service->setProperty(kIOHIDDeviceUsagePairsKey, getDeviceUsagePairs()); \
    service->setProperty(kIOHIDReportIntervalKey, getReportInterval(), 32);

#define		_provider							_reserved->provider
#define     _workLoop                           _reserved->workLoop
#define     _deviceUsagePairs                   _reserved->deviceUsagePairs
#define     _commandGate                        _reserved->commandGate
#define     _keyboard                           _reserved->keyboard
#define     _multiAxis                          _reserved->multiAxis
#define     _digitizer                          _reserved->digitizer
#define     _relativePointer                    _reserved->relativePointer

#if TARGET_OS_EMBEDDED

#define     _clientDict                         _reserved->clientDict

#define     kDebuggerDelayMS                    2500
#define     kDebuggerLongDelayMS                5000
#define     kATVChordDelayMS                    5000
#define     kDelayedStackshotMask               (1 << 31)

//===========================================================================
// IOHIDClientData class
class IOHIDClientData : public OSObject
{
    OSDeclareDefaultStructors(IOHIDClientData)

    IOService * client;
    void *      context;
    void *      action;

public:
    static IOHIDClientData* withClientInfo(IOService *client, void* context, void * action);
    inline IOService *  getClient()     { return client; }
    inline void *       getContext()    { return context; }
    inline void *       getAction()     { return action; }
};

#endif /* TARGET_OS_EMBEDDED */

//===========================================================================
// IOHIDEventService class

#define super IOService

OSDefineMetaClassAndAbstractStructors( IOHIDEventService, IOService )
//====================================================================================================
// IOHIDEventService::init
//====================================================================================================
bool IOHIDEventService::init ( OSDictionary * properties )
{
    if (!super::init(properties))
        return false;

    _reserved = IONew(ExpansionData, 1);
    bzero(_reserved, sizeof(ExpansionData));

    _nubLock = IORecursiveLockAlloc();

#if TARGET_OS_EMBEDDED
    _clientDict = OSDictionary::withCapacity(2);
    if ( _clientDict == 0 )
        return false;
#endif /* TARGET_OS_EMBEDDED */

    _keyboard.eject.delayMS = kEjectKeyDelayMS;

    return true;
}
//====================================================================================================
// IOHIDEventService::start
//====================================================================================================
bool IOHIDEventService::start ( IOService * provider )
{
    UInt32      bootProtocol = 0;
    OSNumber    *number       = NULL;

    _provider = provider;

    if ( !super::start(provider) )
        return false;

    if ( !handleStart(provider) )
        return false;

    _workLoop = getWorkLoop();
    if ( !_workLoop )
        return false;

    _workLoop->retain();
    
    _keyboard.appleVendorSupported = (getProperty(kIOHIDAppleVendorSupported, gIOServicePlane) == kOSBooleanTrue);

    _keyboard.eject.timer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::ejectTimerCallback));
    if (!_keyboard.eject.timer || (_workLoop->addEventSource(_keyboard.eject.timer) != kIOReturnSuccess))
        return false;

    number = (OSNumber*)copyProperty(kIOHIDKeyboardEjectDelay);
    if ( OSDynamicCast(OSNumber, number) )
        _keyboard.eject.delayMS = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);

    _keyboard.caps.timer =
            IOTimerEventSource::timerEventSource(this,
                                                 OSMemberFunctionCast(IOTimerEventSource::Action,
                                                                      this,
                                                                      &IOHIDEventService::capsTimerCallback));
    if (!_keyboard.caps.timer || (_workLoop->addEventSource(_keyboard.caps.timer) != kIOReturnSuccess))
        return false;

    _multiAxis.timer =
    IOTimerEventSource::timerEventSource(this,
                                         OSMemberFunctionCast(IOTimerEventSource::Action,
                                                              this,
                                                              &IOHIDEventService::multiAxisTimerCallback));
    if (!_multiAxis.timer || (_workLoop->addEventSource(_multiAxis.timer) != kIOReturnSuccess))
        return false;


    _commandGate = IOCommandGate::commandGate(this);
    if (!_commandGate || (_workLoop->addEventSource(_commandGate) != kIOReturnSuccess))
        return false;

    calculateCapsLockDelay();

    calculateStandardType();

    SET_HID_PROPERTIES(this);
    SET_HID_PROPERTIES_EMBEDDED(this);

    number = (OSNumber*)copyProperty("BootProtocol");
    if (OSDynamicCast(OSNumber, number))
        bootProtocol = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);

    parseSupportedElements (getReportElements(), bootProtocol);

#if !TARGET_OS_EMBEDDED
    if ((!_consumerNub && _keyboardNub) || (!_keyboardNub && _consumerNub)) {
        OSDictionary * matchingDictionary = IOService::serviceMatching( "IOHIDEventService" );
        if ( matchingDictionary ) {
            OSDictionary *      propertyMatch = OSDictionary::withCapacity(4);

            if (propertyMatch) {
                OSObject *          object;
                object = copyProperty(kIOHIDTransportKey);
                if (object) propertyMatch->setObject(kIOHIDTransportKey, object);
                OSSafeReleaseNULL(object);

                object = copyProperty(kIOHIDVendorIDKey);
                if (object) propertyMatch->setObject(kIOHIDVendorIDKey, object);
                OSSafeReleaseNULL(object);

                object = copyProperty(kIOHIDProductIDKey);
                if (object) propertyMatch->setObject(kIOHIDProductIDKey, object);
                OSSafeReleaseNULL(object);

                object = copyProperty(kIOHIDLocationIDKey);
                if (object) propertyMatch->setObject(kIOHIDLocationIDKey, object);
                OSSafeReleaseNULL(object);

                matchingDictionary->setObject(gIOPropertyMatchKey, propertyMatch);

                propertyMatch->release();
            }
            _publishNotify = addMatchingNotification( gIOPublishNotification,
                             matchingDictionary,
                             &IOHIDEventService::_publishMatchingNotificationHandler,
                             this, 0 );
            matchingDictionary->release();
        }
    }
#endif /* TARGET_OS_EMBEDDED */

    _readyForInputReports = true;

    registerService(kIOServiceAsynchronous);

    return true;
}

#if !TARGET_OS_EMBEDDED

//====================================================================================================
// stopAndReleaseShim
//====================================================================================================

static void stopAndReleaseShim ( IOService * service, IOService * provider )
{
    if ( !service )
        return;

    IOService * serviceProvider = service->getProvider();

    if ( serviceProvider == provider )
    {
        service->stop(provider);
        service->detach(provider);
    }
    service->release();
}

#endif /* TARGET_OS_EMBEDDED */

//====================================================================================================
// IOHIDEventService::stop
//====================================================================================================
void IOHIDEventService::stop( IOService * provider )
{
    handleStop ( provider );
    _provider = NULL;

    if (_keyboard.caps.timer) {
        _keyboard.caps.timer->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.caps.timer);

        _keyboard.caps.timer->release();
        _keyboard.caps.timer = 0;
    }

    if (_keyboard.eject.timer) {
        _keyboard.eject.timer->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.eject.timer);

        _keyboard.eject.timer->release();
        _keyboard.eject.timer = 0;
    }

    if (_multiAxis.timer) {
        _multiAxis.timer->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_multiAxis.timer);

        _multiAxis.timer->release();
        _multiAxis.timer = 0;
    }

    if (_commandGate) {
        if ( _workLoop )
            _workLoop->removeEventSource(_commandGate);

        _commandGate->release();
        _commandGate = 0;
    }

#if TARGET_OS_EMBEDDED

    if ( _keyboard.debug.nmiTimer ) {
        _keyboard.debug.nmiTimer->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.debug.nmiTimer);

        _keyboard.debug.nmiTimer->release();
        _keyboard.debug.nmiTimer = 0;
    }

    if ( _keyboard.debug.stackshotTimer ) {
        _keyboard.debug.stackshotTimer->cancelTimeout();
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.debug.stackshotTimer);

        _keyboard.debug.stackshotTimer->release();
        _keyboard.debug.stackshotTimer = 0;
    }
#else

    NUB_LOCK;

    stopAndReleaseShim ( _keyboardNub, this );
    _keyboardNub = 0;

    stopAndReleaseShim ( _pointingNub, this );
    _pointingNub = 0;

    stopAndReleaseShim ( _consumerNub, this );
    _consumerNub = 0;

    if (_publishNotify) {
        _publishNotify->remove();
    	_publishNotify = 0;
    }

    NUB_UNLOCK;
#endif /* TARGET_OS_EMBEDDED */

    super::stop( provider );
}

//====================================================================================================
// IOHIDEventService::matchPropertyTable
//====================================================================================================
bool IOHIDEventService::matchPropertyTable(OSDictionary * table, SInt32 * score)
{
    RETAIN_ON_STACK(this);
    // Ask our superclass' opinion.
    if (super::matchPropertyTable(table, score) == false)
        return false;

    return MatchPropertyTable(this, table, score);
}

//====================================================================================================
// IOHIDEventService::_publishMatchingNotificationHandler
//====================================================================================================
bool IOHIDEventService::_publishMatchingNotificationHandler(
    void * target,
    void * /* ref */,
    IOService * newService,
    IONotifier * /* notifier */)
{
#if !TARGET_OS_EMBEDDED
    IOHIDEventService * self    = (IOHIDEventService *) target;
    IOHIDEventService * service = (IOHIDEventService *) newService;
    IONotifier * publishNotify  = NULL;
    // NUB_LOCK;
    if (self->_nubLock) IORecursiveLockLock(self->_nubLock);
    if (self->_publishNotify) {
        if ( service->_keyboardNub ) {
            if ( self->_keyboardNub
                    && self->_keyboardNub->isDispatcher()
                    && !service->_keyboardNub->isDispatcher() ) {
                stopAndReleaseShim ( self->_keyboardNub, self );
                self->_keyboardNub = 0;
            }

            if ( !self->_keyboardNub ) {
                self->_keyboardNub = service->_keyboardNub;
                self->_keyboardNub->retain();

                if (self->_publishNotify) {
                    publishNotify = self->_publishNotify;
                    self->_publishNotify = 0;
                }
            }
        }

        if ( service->_consumerNub ) {
            if ( self->_consumerNub
                    && self->_consumerNub->isDispatcher()
                    && !service->_consumerNub->isDispatcher() ) {
                stopAndReleaseShim ( self->_consumerNub, self );
                self->_consumerNub = 0;
            }

            if ( !self->_consumerNub ) {
                self->_consumerNub = service->_consumerNub;
                self->_consumerNub->retain();

                if (self->_publishNotify) {
                    publishNotify = self->_publishNotify;
                    self->_publishNotify = 0;
                }
            }
        }
    }
    // NUB_UNLOCK;
    if (self->_nubLock) IORecursiveLockUnlock(self->_nubLock);
    if (publishNotify) publishNotify->remove();
#endif /* TARGET_OS_EMBEDDED */
    return true;
}

//====================================================================================================
// IOHIDEventService::calculateCapsLockDelay
//====================================================================================================
void IOHIDEventService::calculateCapsLockDelay()
{
    OSNumber        *delay = NULL;
    OSNumber        *delayOverride = NULL;
    OSDictionary    *deviceParameters = NULL;
    OSArray         *mappings = NULL;
    UInt32          count = 0;

    // default to no delay
    _keyboard.caps.delayMS = 0;

    // If this keyboard does not support delay, get out. Otherwise, use it.
    delay = (OSNumber*)copyProperty(kIOHIDKeyboardCapsLockDelay);
    if (!OSDynamicCast(OSNumber, delay))
        goto GET_OUT;
    _keyboard.caps.delayMS = delay->unsigned32BitValue();

    // If there is an override in place, use that.

    delayOverride = (OSNumber*)copyProperty(kIOHIDKeyboardCapsLockDelayOverride);
    if (OSDynamicCast(OSNumber, delayOverride))
        _keyboard.caps.delayMS = delayOverride->unsigned32BitValue();
    OSSafeReleaseNULL(delayOverride);

    // If there is no delay at this point, get out.
    if (!_keyboard.caps.delayMS)
        goto GET_OUT;

    // At this point, we need to scan all of the modifier mappings (if any) to see
    // if the NX_MODIFIERKEY_ALPHALOCK is remapped to something other than the
    // NX_MODIFIERKEY_ALPHALOCK.
    deviceParameters = (OSDictionary*)copyProperty(kIOHIDEventServicePropertiesKey);
    if (!OSDynamicCast(OSDictionary, deviceParameters))
        goto GET_OUT;

    mappings = OSDynamicCast(OSArray, deviceParameters->getObject(kIOHIDKeyboardModifierMappingPairsKey));
    if (!mappings) goto GET_OUT;

    count = mappings->getCount();
    if ( count ) {
        for ( unsigned i=0; i < count; i++ ) {
            OSDictionary    *pair   = OSDynamicCast(OSDictionary, mappings->getObject(i));
            OSNumber        *number         = NULL;
            SInt32   src    = 0;
            SInt32   dst    = 0;

            if ( !pair ) continue;

            number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDKeyboardModifierMappingSrcKey));

            if ( !number ) continue;

            src = number->unsigned32BitValue();

            if (src != NX_MODIFIERKEY_ALPHALOCK) continue;

            number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDKeyboardModifierMappingDstKey));

            if ( !number ) continue;

            dst = number->unsigned32BitValue();

            if (dst == NX_MODIFIERKEY_ALPHALOCK) continue;

            // NX_MODIFIERKEY_ALPHALOCK is remapped. Set delay to 0 and get out.
            _keyboard.caps.delayMS = 0;
            goto GET_OUT;
        }
    }

GET_OUT:
    OSSafeReleaseNULL(deviceParameters);
    OSSafeReleaseNULL(delay);
    IOHID_DEBUG(kIOHIDDebugCode_CalculatedCapsDelay, _keyboard.caps.delayMS, 0, 0, 0);
}

//====================================================================================================
// IOHIDEventService::calculateStandardType
//====================================================================================================
void IOHIDEventService::calculateStandardType()
{
    IOHIDStandardType   result = kIOHIDStandardTypeANSI;
    OSNumber *          number;

        number = (OSNumber*)copyProperty(kIOHIDStandardTypeKey);
        if ( OSDynamicCast(OSNumber, number) ) {
            result = number->unsigned32BitValue();
        }
        else {
            OSSafeReleaseNULL(number);
            UInt16 productID    = getProductID();
            UInt16 vendorID     = getVendorID();

            if (vendorID == kIOUSBVendorIDAppleComputer) {

                switch (productID) {
                    case kprodUSBCosmoISOKbd:  //Cosmo ISO
                    case kprodUSBAndyISOKbd:  //Andy ISO
                    case kprodQ6ISOKbd:  //Q6 ISO
                    case kprodQ30ISOKbd:  //Q30 ISO
#if TARGET_OS_EMBEDDED
                        _keyboard.swapISO = true;
#endif /* TARGET_OS_EMBEDDED */
                        // fall through
                    case kprodFountainISOKbd:  //Fountain ISO
                    case kprodSantaISOKbd:  //Santa ISO
                        result = kIOHIDStandardTypeISO;
                        break;
                    case kprodUSBCosmoJISKbd:  //Cosmo JIS
                    case kprodUSBAndyJISKbd:  //Andy JIS is 0x206
                    case kprodQ6JISKbd:  //Q6 JIS
                    case kprodQ30JISKbd:  //Q30 JIS
                    case kprodFountainJISKbd:  //Fountain JIS
                    case kprodSantaJISKbd:  //Santa JIS
                        result = kIOHIDStandardTypeJIS;
                        break;
                }

                setProperty(kIOHIDStandardTypeKey, result, 32);
            }
        }
    OSSafeReleaseNULL(number);

#if TARGET_OS_EMBEDDED
    if ( !_keyboard.swapISO && result == kIOHIDStandardTypeISO ) {
        number = (OSNumber*)copyProperty("alt_handler_id");
        if ( OSDynamicCast(OSNumber, number) ) {
            switch (number->unsigned32BitValue()) {
                case kgestUSBCosmoISOKbd:
                case kgestUSBAndyISOKbd:
                case kgestQ6ISOKbd:
                case kgestQ30ISOKbd:
                case kgestM89ISOKbd:
                case kgestUSBGenericISOkd:
                    _keyboard.swapISO = true;
                    break;
            }
        }
        OSSafeReleaseNULL(number);
    }
#endif /* TARGET_OS_EMBEDDED */
}

//====================================================================================================
// IOHIDEventService::setSystemProperties
//====================================================================================================
IOReturn IOHIDEventService::setSystemProperties( OSDictionary * properties )
{
    OSDictionary *  dict        = NULL;
    OSArray *       array       = NULL;
    OSNumber *      number      = NULL;
    bool            setCapsDelay= false;

    if ( !properties )
        return kIOReturnBadArgument;

    if ( properties->getObject(kIOHIDDeviceParametersKey) != kOSBooleanTrue ) {
        OSDictionary * propsCopy = OSDictionary::withDictionary(properties);
        if ( propsCopy ) {
            propsCopy->setObject(kIOHIDEventServicePropertiesKey, kOSBooleanTrue);

#if !TARGET_OS_EMBEDDED
            if ( _keyboardNub )
                _keyboardNub->setParamProperties(properties);

            if ( _pointingNub )
                _pointingNub->setParamProperties(properties);

            if ( _consumerNub )
                _consumerNub->setParamProperties(properties);
#endif
            propsCopy->release();
        }
    }

    number = OSDynamicCast(OSNumber, properties->getObject(kIOHIDKeyboardCapsLockDelayOverride));
    if (number) {
        setProperty(kIOHIDKeyboardCapsLockDelayOverride, number);
        setCapsDelay = true;
    }

    if ( ( array = OSDynamicCast(OSArray, properties->getObject(kIOHIDKeyboardModifierMappingPairsKey)) ) ) {
        UInt32  srcVirtualCode, dstVirtualCode;
        Boolean capsMap = FALSE;

        for (UInt32 index=0; index<array->getCount(); index++) {

            dict = OSDynamicCast(OSDictionary, array->getObject(index));
            if ( !dict )
                continue;

            number = OSDynamicCast(OSNumber, dict->getObject(kIOHIDKeyboardModifierMappingSrcKey));
            if ( !number )
                continue;

            srcVirtualCode = number->unsigned32BitValue();
            if ( srcVirtualCode != NX_MODIFIERKEY_ALPHALOCK )
                continue;

            number = OSDynamicCast(OSNumber, dict->getObject(kIOHIDKeyboardModifierMappingDstKey));
            if ( !number )
                continue;

            dstVirtualCode = number->unsigned32BitValue();
            if ( dstVirtualCode == srcVirtualCode )
                continue;

            capsMap = TRUE;

            break;
        }

        if ( capsMap ) {
            // Clear out the delay
            _keyboard.caps.delayMS = 0;
            setCapsDelay = false;
        }
        else if ( !_keyboard.caps.delayMS ) {
            setCapsDelay = true;
        }
    }

    if (setCapsDelay) {
        calculateCapsLockDelay();
    }
    if ( properties->getObject(kIOHIDDeviceParametersKey) == kOSBooleanTrue ) {
        OSDictionary * eventServiceProperties = (OSDictionary*)copyProperty(kIOHIDEventServicePropertiesKey);
        if ( OSDynamicCast(OSDictionary, eventServiceProperties) ) {
            if (eventServiceProperties->setOptions(0, 0) & OSDictionary::kImmutable) {
                OSDictionary * copyEventServiceProperties = (OSDictionary*)eventServiceProperties->copyCollection();
                eventServiceProperties ->release();
                eventServiceProperties = copyEventServiceProperties;
            }
        } else {
            OSSafeReleaseNULL(eventServiceProperties);
            eventServiceProperties = OSDictionary::withCapacity(4);
        }

        if ( eventServiceProperties ) {
            eventServiceProperties->merge(properties);
            eventServiceProperties->removeObject(kIOHIDResetKeyboardKey);
            eventServiceProperties->removeObject(kIOHIDResetPointerKey);
            eventServiceProperties->removeObject(kIOHIDDeviceParametersKey);
            setProperty(kIOHIDEventServicePropertiesKey, eventServiceProperties);
            eventServiceProperties->release();
        }
    }

    return kIOReturnSuccess;
}

//====================================================================================================
// IOHIDEventService::setProperties
//====================================================================================================
IOReturn IOHIDEventService::setProperties( OSObject * properties )
{
    OSDictionary *  propertyDict    = OSDynamicCast(OSDictionary, properties);
    IOReturn        ret             = kIOReturnBadArgument;

    if ( propertyDict ) {
        propertyDict->setObject(kIOHIDDeviceParametersKey, kOSBooleanTrue);
        ret = setSystemProperties( propertyDict );
        propertyDict->removeObject(kIOHIDDeviceParametersKey);
    }

    return ret;
}


//====================================================================================================
// IOHIDEventService::parseSupportedElements
//====================================================================================================
void IOHIDEventService::parseSupportedElements ( OSArray * elementArray, UInt32 bootProtocol )
{
    UInt32              count               = 0;
    UInt32              index               = 0;
    UInt32              usage               = 0;
    UInt32              usagePage           = 0;
    UInt32              supportedModifiers  = 0;
    UInt32              buttonCount         = 0;
    IOHIDElement *      element             = 0;
    OSArray *           functions           = 0;
    IOFixed             pointingResolution  = 0;
    IOFixed             scrollResolution    = 0;
    bool                pointingDevice      = false;
    bool                keyboardDevice      = false;
    bool                consumerDevice      = false;

    switch ( bootProtocol )
    {
        case kBootProtocolMouse:
            pointingDevice = true;
            break;
        case kBootProtocolKeyboard:
            keyboardDevice = true;
            break;
    }

    if ( elementArray )
    {
        count = elementArray->getCount();

        for ( index = 0; index < count; index++ )
        {
            element = OSDynamicCast(IOHIDElement, elementArray->getObject(index));

            if ( !element )
                continue;

            usagePage   = element->getUsagePage();
            usage       = element->getUsage();

            switch ( usagePage )
            {
                case kHIDPage_GenericDesktop:
                    switch ( usage )
                    {
                        case kHIDUsage_GD_Mouse:
                            pointingDevice      = true;
                            break;
                        case kHIDUsage_GD_X:
                            if ( !(pointingResolution = determineResolution(element)) )
                                pointingResolution = kDefaultFixedResolution;
                            break;
                        case kHIDUsage_GD_Z:
                        case kHIDUsage_GD_Wheel:
                            if ( !(scrollResolution = determineResolution(element)) )
                                scrollResolution = kDefaultScrollFixedResolution;
                            break;
                        case kHIDUsage_GD_SystemPowerDown:
                        case kHIDUsage_GD_SystemSleep:
                        case kHIDUsage_GD_SystemWakeUp:
                            consumerDevice      = true;
                            break;
                    }
                    break;

                case kHIDPage_Button:
                    buttonCount ++;
                    break;

                case kHIDPage_KeyboardOrKeypad:
                    keyboardDevice = true;
                    switch ( usage )
                    {
                        case kHIDUsage_KeyboardLeftControl:
                            supportedModifiers |= NX_CONTROLMASK;
                            supportedModifiers |= NX_DEVICELCTLKEYMASK;
                            break;
                        case kHIDUsage_KeyboardLeftShift:
                            supportedModifiers |= NX_SHIFTMASK;
                            supportedModifiers |= NX_DEVICELSHIFTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardLeftAlt:
                            supportedModifiers |= NX_ALTERNATEMASK;
                            supportedModifiers |= NX_DEVICELALTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardLeftGUI:
                            supportedModifiers |= NX_COMMANDMASK;
                            supportedModifiers |= NX_DEVICELCMDKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightControl:
                            supportedModifiers |= NX_CONTROLMASK;
                            supportedModifiers |= NX_DEVICERCTLKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightShift:
                            supportedModifiers |= NX_SHIFTMASK;
                            supportedModifiers |= NX_DEVICERSHIFTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightAlt:
                            supportedModifiers |= NX_ALTERNATEMASK;
                            supportedModifiers |= NX_DEVICERALTKEYMASK;
                            break;
                        case kHIDUsage_KeyboardRightGUI:
                            supportedModifiers |= NX_COMMANDMASK;
                            supportedModifiers |= NX_DEVICERCMDKEYMASK;
                            break;
                        case kHIDUsage_KeyboardCapsLock:
                            supportedModifiers |= NX_ALPHASHIFT_STATELESS_MASK;
                            supportedModifiers |= NX_DEVICE_ALPHASHIFT_STATELESS_MASK;
                            break;
                    }
                    break;

                case kHIDPage_Consumer:
                    consumerDevice = true;
                    break;
                case kHIDPage_Digitizer:
                    pointingDevice = true;
                    switch ( usage )
                    {
                        case kHIDUsage_Dig_Pen:
                        case kHIDUsage_Dig_LightPen:
                        case kHIDUsage_Dig_TouchScreen:
                            setProperty(kIOHIDDisplayIntegratedKey, true);
                            break;
                        case kHIDUsage_Dig_TipSwitch:
                        case kHIDUsage_Dig_BarrelSwitch:
                        case kHIDUsage_Dig_Eraser:
                            buttonCount ++;
                        default:
                            break;
                    }
                    break;
                case kHIDPage_AppleVendorTopCase:
                    if ((usage == kHIDUsage_AV_TopCase_KeyboardFn) &&
                        (_keyboard.appleVendorSupported))
                    {
                        supportedModifiers |= NX_SECONDARYFNMASK;
                    }
                    break;
            }

            // Cache device functions
            if ((element->getType() == kIOHIDElementTypeCollection) &&
                ((element->getCollectionType() == kIOHIDElementCollectionTypeApplication) ||
                (element->getCollectionType() == kIOHIDElementCollectionTypePhysical)))
            {
                OSNumber * usagePageRef, * usageRef;
                OSDictionary * pairRef;

                if(!functions) functions = OSArray::withCapacity(2);

                pairRef     = OSDictionary::withCapacity(2);
                usageRef    = OSNumber::withNumber(usage, 32);
                usagePageRef= OSNumber::withNumber(usagePage, 32);

                pairRef->setObject(kIOHIDDeviceUsageKey, usageRef);
                pairRef->setObject(kIOHIDDeviceUsagePageKey, usagePageRef);

                UInt32 	pairCount = functions->getCount();
                bool 	found = false;
                for(unsigned i=0; i<pairCount; i++)
                {
                    OSDictionary *tempPair = (OSDictionary *)functions->getObject(i);

                    if ( NULL != (found = tempPair->isEqualTo(pairRef)) )
                        break;
                }

                if (!found)
                {
                    functions->setObject(functions->getCount(), pairRef);
                }

                pairRef->release();
                usageRef->release();
                usagePageRef->release();
            }
        }
        
        if (_deviceUsagePairs) {
            _deviceUsagePairs->release();
        }
        _deviceUsagePairs = functions;
    }

    NUB_LOCK;

    if ( pointingDevice )
    {
        if ( pointingResolution )
            setProperty(kIOHIDPointerResolutionKey, pointingResolution, 32);

        if ( scrollResolution )
            setProperty(kIOHIDScrollResolutionKey, scrollResolution, 32);

        _pointingNub = newPointingShim(buttonCount, pointingResolution, scrollResolution, kShimEventProcessor);
    }
    if ( keyboardDevice )
    {
        _keyboardNub = newKeyboardShim(supportedModifiers, kShimEventProcessor);
    }
    if ( consumerDevice )
    {
        _consumerNub = newConsumerShim(kShimEventProcessor);
    }

    NUB_UNLOCK;
}

//====================================================================================================
// IOHIDEventService::newPointingShim
//====================================================================================================
IOHIDPointing * IOHIDEventService::newPointingShim (
                            UInt32          buttonCount,
                            IOFixed         pointerResolution,
                            IOFixed         scrollResolution,
                            IOOptionBits    options)
{
#if !TARGET_OS_EMBEDDED // {
    bool            isDispatcher = ((options & kShimEventProcessor) == 0);
    IOHIDPointing   *pointingNub = IOHIDPointing::Pointing(buttonCount, pointerResolution, scrollResolution, isDispatcher);;
    OSNumber        *value;
    
    require(pointingNub, no_nub);

	SET_HID_PROPERTIES(pointingNub);

    require(pointingNub->attach(this), no_attach);
    require(pointingNub->start(this), no_start);
    value = OSNumber::withNumber(getRegistryEntryID(), 64);
    if (value) {
        pointingNub->setProperty(kIOHIDAltSenderIdKey, value);
        value->release();
    }
    return pointingNub;

no_start:
    pointingNub->detach(this);

no_attach:
    pointingNub->release();
    pointingNub = NULL;

no_nub:

#endif // } TARGET_OS_EMBEDDED
    return NULL;
}

//====================================================================================================
// IOHIDEventService::newKeyboardShim
//====================================================================================================
IOHIDKeyboard * IOHIDEventService::newKeyboardShim (
                                UInt32          supportedModifiers,
                                IOOptionBits    options)
{
#if !TARGET_OS_EMBEDDED // {
    bool            isDispatcher = ((options & kShimEventProcessor) == 0);
    IOHIDKeyboard   *keyboardNub = IOHIDKeyboard::Keyboard(supportedModifiers, isDispatcher);

    require(keyboardNub, no_nub);

        SET_HID_PROPERTIES(keyboardNub);

    require(keyboardNub->attach(this), no_attach);
    require(keyboardNub->start(this), no_start);
    keyboardNub->setProperty(kIOHIDAltSenderIdKey, OSNumber::withNumber(getRegistryEntryID(), 64));

    return keyboardNub;

no_start:
    keyboardNub->detach(this);

no_attach:
    keyboardNub->release();
    keyboardNub = NULL;

no_nub:

#endif // } TARGET_OS_EMBEDDED
    return NULL;
}

//====================================================================================================
// IOHIDEventService::newConsumerShim
//====================================================================================================
IOHIDConsumer * IOHIDEventService::newConsumerShim ( IOOptionBits options )
{
#if !TARGET_OS_EMBEDDED // {
    bool            isDispatcher = ((options & kShimEventProcessor) == 0);
    IOHIDConsumer   *consumerNub = IOHIDConsumer::Consumer(isDispatcher);;

    require(consumerNub, no_nub);

        SET_HID_PROPERTIES(consumerNub);

    require(consumerNub->attach(this), no_attach);
    require(consumerNub->start(this), no_start);
    consumerNub->setProperty(kIOHIDAltSenderIdKey, OSNumber::withNumber(getRegistryEntryID(), 64));

    return consumerNub;

no_start:
    consumerNub->detach(this);

no_attach:
    consumerNub->release();
    consumerNub = NULL;

no_nub:

#endif // } TARGET_OS_EMBEDDED
    return NULL;
}

//====================================================================================================
// IOHIDEventService::determineResolution
//====================================================================================================
IOFixed IOHIDEventService::determineResolution ( IOHIDElement * element )
{
    IOFixed resolution = 0;
    bool supportResolution = true;

#if !TARGET_OS_EMBEDDED
    if ((element->getFlags() & kIOHIDElementFlagsRelativeMask) != 0) {

        if ( element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_MultiAxisController) )
            supportResolution = false;
    }
    else {
        supportResolution = false;
    }
#endif /* !TARGET_OS_EMBEDDED */
    
    if ( supportResolution ) {
        if ((element->getPhysicalMin() != element->getLogicalMin()) &&
            (element->getPhysicalMax() != element->getLogicalMax()))
        {
            SInt32 logicalDiff = (element->getLogicalMax() - element->getLogicalMin());
            SInt32 physicalDiff = (element->getPhysicalMax() - element->getPhysicalMin());

            // Since IOFixedDivide truncated fractional part and can't use floating point
            // within the kernel, have to convert equation when using negative exponents:
            // _resolution = ((logMax -logMin) * 10 **(-exp))/(physMax -physMin)

            // Even though unitExponent is stored as SInt32, The real values are only
            // a signed nibble that doesn't expand to the full 32 bits.
            SInt32 resExponent = element->getUnitExponent() & 0x0F;

            if (resExponent < 8)
            {
                for (int i = resExponent; i > 0; i--)
                {
                    physicalDiff *=  10;
                }
            }
            else
            {
                for (int i = 0x10 - resExponent; i > 0; i--)
                {
                    logicalDiff *= 10;
                }
            }
            resolution = (logicalDiff / physicalDiff) << 16;
        }
    }

    return resolution;
}

//====================================================================================================
// IOHIDEventService::free
//====================================================================================================
void IOHIDEventService::free()
{
    IORecursiveLock* tempLock = NULL;

    if ( _nubLock ) {
        IORecursiveLockLock(_nubLock);
        tempLock = _nubLock;
        _nubLock = NULL;
    }

    if (_keyboard.eject.timer) {
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.eject.timer);

        _keyboard.eject.timer->release();
        _keyboard.eject.timer = 0;
    }

    if (_commandGate) {
        if ( _workLoop )
            _workLoop->removeEventSource(_commandGate);

        _commandGate->release();
        _commandGate = 0;
    }

    if (_keyboard.caps.timer) {
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.caps.timer);

        _keyboard.caps.timer->release();
        _keyboard.caps.timer = 0;
    }

    if ( _deviceUsagePairs ) {
        _deviceUsagePairs->release();
        _deviceUsagePairs = NULL;
    }
    
#if TARGET_OS_EMBEDDED
    if ( _clientDict ) {
        assert(_clientDict->getCount() == 0);
        _clientDict->release();
        _clientDict = NULL;
    }

    if (_keyboard.debug.nmiTimer) {
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.debug.nmiTimer);

        _keyboard.debug.nmiTimer->release();
        _keyboard.debug.nmiTimer = 0;
    }

    if (_keyboard.debug.stackshotTimer) {
        if ( _workLoop )
            _workLoop->removeEventSource(_keyboard.debug.stackshotTimer);

        _keyboard.debug.stackshotTimer->release();
        _keyboard.debug.stackshotTimer = 0;
    }

#endif /* TARGET_OS_EMBEDDED */

    if ( _workLoop ) {
        // not our workloop. don't stop it.
        _workLoop->release();
        _workLoop = NULL;
    }

    if (_reserved) {
        IODelete(_reserved, ExpansionData, 1);
        _reserved = NULL;
    }

    if ( tempLock ) {
        IORecursiveLockUnlock(tempLock);
        IORecursiveLockFree(tempLock);
    }

    super::free();
}

//==============================================================================
// IOHIDEventService::handleOpen
//==============================================================================
bool IOHIDEventService::handleOpen(IOService *  client,
                                    IOOptionBits options,
                                    void *       argument)
{
#if TARGET_OS_EMBEDDED

    bool accept = false;
    do {
        // Was this object already registered as our client?

        if ( _clientDict->getObject((const OSSymbol *)client) ) {
            accept = true;
            break;
        }

        // Add the new client object to our client dict.
        if ( !OSDynamicCast(IOHIDClientData, (OSObject *)argument) ||
                !_clientDict->setObject((const OSSymbol *)client, (IOHIDClientData *)argument))
            break;

        accept = true;
    } while (false);

    return accept;

#else

    return super::handleOpen(client, options, argument);

#endif /* TARGET_OS_EMBEDDED */

}

//==============================================================================
// IOHIDEventService::handleClose
//==============================================================================
void IOHIDEventService::handleClose(IOService * client, IOOptionBits options)
{
#if TARGET_OS_EMBEDDED
    if ( _clientDict->getObject((const OSSymbol *)client) )
        _clientDict->removeObject((const OSSymbol *)client);
#else
    super::handleClose(client, options);
#endif /* TARGET_OS_EMBEDDED */
}

//==============================================================================
// IOHIDEventService::handleIsOpen
//==============================================================================
bool IOHIDEventService::handleIsOpen(const IOService * client) const
{
#if TARGET_OS_EMBEDDED
    if (client)
        return _clientDict->getObject((const OSSymbol *)client) != NULL;
    else
        return (_clientDict->getCount() > 0);
#else
    return super::handleIsOpen(client);
#endif /* TARGET_OS_EMBEDDED */
}

//====================================================================================================
// IOHIDEventService::handleStart
//====================================================================================================
bool IOHIDEventService::handleStart( IOService * provider __unused )
{
    return true;
}

//====================================================================================================
// IOHIDEventService::handleStop
//====================================================================================================
void IOHIDEventService::handleStop(  IOService * provider __unused )
{}

//====================================================================================================
// IOHIDEventService::getTransport
//====================================================================================================
OSString * IOHIDEventService::getTransport ()
{
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDTransportKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getManufacturer
//====================================================================================================
OSString * IOHIDEventService::getManufacturer ()
{
    // vtn3: This is not safe, but I am unsure how to fix it
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDManufacturerKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getProduct
//====================================================================================================
OSString * IOHIDEventService::getProduct ()
{
    // vtn3: This is not safe, but I am unsure how to fix it
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDProductKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getSerialNumber
//====================================================================================================
OSString * IOHIDEventService::getSerialNumber ()
{
    // vtn3: This is not safe, but I am unsure how to fix it
    return _provider ? OSDynamicCast(OSString, _provider->getProperty(kIOHIDSerialNumberKey)) : 0;
}

//====================================================================================================
// IOHIDEventService::getLocationID
//====================================================================================================
UInt32 IOHIDEventService::getLocationID ()
{
	UInt32 value = 0;

	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDLocationIDKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getVendorID
//====================================================================================================
UInt32 IOHIDEventService::getVendorID ()
{
	UInt32 value = 0;

	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDVendorIDKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getVendorIDSource
//====================================================================================================
UInt32 IOHIDEventService::getVendorIDSource ()
{
	UInt32 value = 0;

	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDVendorIDSourceKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getProductID
//====================================================================================================
UInt32 IOHIDEventService::getProductID ()
{
	UInt32 value = 0;

	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDProductIDKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getVersion
//====================================================================================================
UInt32 IOHIDEventService::getVersion ()
{
	UInt32 value = 0;

	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDVersionNumberKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
		OSSafeReleaseNULL(number);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getCountryCode
//====================================================================================================
UInt32 IOHIDEventService::getCountryCode ()
{
	UInt32 value = 0;

	if ( _provider ) {
		OSNumber * number = (OSNumber*)_provider->copyProperty(kIOHIDCountryCodeKey);
		if ( OSDynamicCast(OSNumber, number) )
			value = number->unsigned32BitValue();
        OSSafeReleaseNULL(number);
	}
    return value;
}

//====================================================================================================
// IOHIDEventService::getReportElements
//====================================================================================================
OSArray * IOHIDEventService::getReportElements()
{
    return 0;
}

//====================================================================================================
// IOHIDEventService::setElementValue
//====================================================================================================
void IOHIDEventService::setElementValue (
                                UInt32                      usagePage __unused,
                                UInt32                      usage __unused,
                                UInt32                      value __unused )
{
}

//====================================================================================================
// IOHIDEventService::getElementValue
//====================================================================================================
UInt32 IOHIDEventService::getElementValue (
                                UInt32                      usagePage __unused,
                                UInt32                      usage __unused )
{
    return 0;
}


//====================================================================================================
// ejectTimerCallback
//====================================================================================================
void IOHIDEventService::ejectTimerCallback(IOTimerEventSource *sender __unused)
{
    IOHID_DEBUG(kIOHIDDebugCode_EjectCallback, _keyboard.eject.state, 0, 0, 0);
    if ( _keyboard.eject.state ) {
        AbsoluteTime timeStamp;

        clock_get_uptime(&timeStamp);

        dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_Eject, 1, _keyboard.eject.options | kDelayedOption);
        dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_Eject, 0, _keyboard.eject.options | kDelayedOption);

        _keyboard.eject.state = 0;
    }
}

//====================================================================================================
// capsTimerCallback
//====================================================================================================
void IOHIDEventService::capsTimerCallback(IOTimerEventSource *sender __unused)
{
    IOHID_DEBUG(kIOHIDDebugCode_CapsCallback, _keyboard.caps.state, 0, 0, 0);
    AbsoluteTime timeStamp;
    
    clock_get_uptime(&timeStamp);
#if TARGET_OS_EMBEDDED
    dispatchKeyboardEvent(timeStamp, kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardCapsLock, 1, _keyboard.caps.options | kDelayedOption);
#else
    if ( _keyboard.caps.state ) {
        dispatchKeyboardEvent(timeStamp, kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardCapsLock, 1, _keyboard.caps.options | kDelayedOption);
        dispatchKeyboardEvent(timeStamp, kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardCapsLock, 0, _keyboard.caps.options | kDelayedOption);

        _keyboard.caps.state = 0;
    }
#endif
}


#if TARGET_OS_EMBEDDED
//==============================================================================
// IOHIDEventService::debuggerTimerCallback
//==============================================================================
void IOHIDEventService::debuggerTimerCallback(IOTimerEventSource *sender)
{
    if ( _keyboard.debug.mask && _keyboard.debug.mask == _keyboard.debug.startMask   )
        PE_enter_debugger("NMI");
}

#endif /* TARGET_OS_EMBEDDED */

#if TARGET_OS_EMBEDDED
//==============================================================================
// IOHIDEventService::stackshotTimerCallback
//==============================================================================
void IOHIDEventService::stackshotTimerCallback(IOTimerEventSource *sender)
{
    if ( _keyboard.debug.mask && _keyboard.debug.mask == _keyboard.debug.startMask ) {
        _keyboard.debug.stackshotHeld = 1;
    }
}

#endif /* TARGET_OS_EMBEDDED */

//==============================================================================
// IOHIDEventService::multiAxisTimerCallback
//==============================================================================
void IOHIDEventService::multiAxisTimerCallback(IOTimerEventSource *sender __unused)
{
    AbsoluteTime timestamp;

    clock_get_uptime(&timestamp);
    dispatchMultiAxisPointerEvent(timestamp, _multiAxis.buttonState, _multiAxis.x, _multiAxis.y, _multiAxis.z, _multiAxis.rX, _multiAxis.rY, _multiAxis.rZ, _multiAxis.options | kIOHIDEventOptionIsRepeat);
}


//====================================================================================================
// IOHIDEventService::dispatchKeyboardEvent
//====================================================================================================
void IOHIDEventService::dispatchKeyboardEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value,
                                IOOptionBits                options)
{
    if ( ! _readyForInputReports )
        return;

#if TARGET_OS_EMBEDDED // {
    IOHIDEvent * event = NULL;
    UInt32 debugMask = 0;
    if ( !_keyboard.debug.nmiMask ) {
        OSData * nmi_mask = OSDynamicCast(OSData, getProperty("button-nmi_mask", gIOServicePlane));
        if ( nmi_mask) {
            _keyboard.debug.nmiMask = *(UInt32 *) nmi_mask->getBytesNoCopy();
            _keyboard.debug.nmiDelay = kDebuggerLongDelayMS;
        } else {
#if TARGET_OS_TV // Apple TV NMI keychord: FAV (List button) + PlayPause
            _keyboard.debug.nmiMask = 0x50;
            _keyboard.debug.nmiDelay = kATVChordDelayMS;
#else
            _keyboard.debug.nmiMask = 0x3;
            _keyboard.debug.nmiDelay = kDebuggerDelayMS;
#endif // TARGET_OS_TV
        }
    }
    

    switch (usagePage) {
        case kHIDPage_KeyboardOrKeypad:
            if ( _keyboard.swapISO ) {

                switch ( usage ) {
                    case kHIDUsage_KeyboardGraveAccentAndTilde:
                        usage = kHIDUsage_KeyboardNonUSBackslash;
                        break;
                    case kHIDUsage_KeyboardNonUSBackslash:
                        usage = kHIDUsage_KeyboardGraveAccentAndTilde;
                        break;
                }
            }
            break;
        case kHIDPage_Consumer:
            switch (usage) {
                case kHIDUsage_Csmr_Power:
                    debugMask = 0x1;
                    break;
                case kHIDUsage_Csmr_VolumeDecrement:
#if TARGET_OS_TV // Volume- should be treated differently than Volume+ for ATV stackshot only
                    debugMask = 0x20;
                    break;
#endif // TARGET_OS_TV
                case kHIDUsage_Csmr_VolumeIncrement:
                    debugMask = 0x2;
                    break;
                case kHIDUsage_Csmr_Menu:
                    debugMask = 0x4;
                    break;
                case kHIDUsage_Csmr_Help:
                    debugMask = 0x8;
                    break;
                case kHIDUsage_Csmr_PlayOrPause:
                    debugMask = 0x10;
                    break;
                case kHIDUsage_Csmr_DataOnScreen:
                    debugMask = 0x40;
                    break;
            };
            break;
        case kHIDPage_Telephony:
            switch (usage) {
                case kHIDUsage_Tfon_Hold:
                    debugMask = 0x1;
                    break;
            };
            break;
    };

    if ( value )
        _keyboard.debug.mask |= debugMask;
    else
        _keyboard.debug.mask &= ~debugMask;

    if ( _keyboard.debug.mask == _keyboard.debug.nmiMask ) {
        if ( !_keyboard.debug.nmiTimer ) {
            _keyboard.debug.nmiTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::debuggerTimerCallback));
            if (_keyboard.debug.nmiTimer) {
                if ((_workLoop->addEventSource(_keyboard.debug.nmiTimer) != kIOReturnSuccess)) {
                    _keyboard.debug.nmiTimer->release();
                    _keyboard.debug.nmiTimer = NULL;
                }
            }
        }
        if ( _keyboard.debug.nmiTimer ) {
            _keyboard.debug.nmiTimer->setTimeoutMS( _keyboard.debug.nmiDelay );
            _keyboard.debug.startMask = _keyboard.debug.mask;
        }
    }

    // stackshot keychord check
    if(_keyboard.debug.mask == 0x3 || // Power + Volume
       _keyboard.debug.mask == 0x6 || // Menu (Home)  + Volume
       _keyboard.debug.mask == 0xc || // Menu (Crown) + Help (Pill)
       _keyboard.debug.mask == 0x30) {// ATV PlayPause + Volume-
       // Only create the timer for the watch.
       if (_keyboard.debug.mask == 0xc ) {
            if ( !_keyboard.debug.stackshotTimer ) {
                _keyboard.debug.stackshotTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDEventService::stackshotTimerCallback));
                if ( _keyboard.debug.stackshotTimer ) {
                    if ((_workLoop->addEventSource(_keyboard.debug.stackshotTimer) != kIOReturnSuccess)) {
                        _keyboard.debug.stackshotTimer->release();
                        _keyboard.debug.stackshotTimer = NULL;
                    }
                }
            }
            if ( _keyboard.debug.stackshotTimer ) {
                _keyboard.debug.stackshotTimer->setTimeoutMS( 1000 );
                _keyboard.debug.startMask = _keyboard.debug.mask;
            }
        }
        handle_stackshot_keychord(_keyboard.debug.mask);
    }
    if ( _keyboard.debug.mask == 0x0 ) {
        // Always reset flag on release.
        if ( _keyboard.debug.stackshotHeld ) {
            handle_stackshot_keychord( 0xc | kDelayedStackshotMask);
        }
		if ( _keyboard.debug.stackshotTimer ) {
			_keyboard.debug.stackshotTimer->cancelTimeout();
		}
        _keyboard.debug.stackshotHeld = 0;
    }

    // Keyboard caps lock delay - quick taps of caps lock could be accidental, so ignore
    if ( _keyboard.caps.delayMS && (usagePage == kHIDPage_KeyboardOrKeypad) && (usage == kHIDUsage_KeyboardCapsLock ) ) {
        if ( (options & kDelayedOption) == 0) {
            if ( value ) {
                if ( ( getElementValue(kHIDPage_LEDs, kHIDUsage_LED_CapsLock) == 0 ) ) {
                    _keyboard.caps.options = options;

                    if ( _keyboard.caps.timer )
                        _keyboard.caps.timer->setTimeoutMS( _keyboard.caps.delayMS );
                }
                else {
                    event = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, value!=0, _keyboard.caps.options);
                }
            }
            else {
                if ( ( getElementValue(kHIDPage_LEDs, kHIDUsage_LED_CapsLock) != 0 ) ) {
                    event = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, value!=0, _keyboard.caps.options);
                }
                else if ( _keyboard.caps.state ) {
                    event = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, value!=0, _keyboard.caps.options);
                    _keyboard.caps.state = 0;
                }

                if ( _keyboard.caps.timer )
                    _keyboard.caps.timer->cancelTimeout();
            }
        }
        else {
            event = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, value!=0, _keyboard.caps.options);
            _keyboard.caps.state = 1;
        }
    }
    else
    {
        event = IOHIDEvent::keyboardEvent(timeStamp, usagePage, usage, value!=0, options);
    }
    
    if ( !event )
        return;

    dispatchEvent(event);

    event->release();

#else // } {

    NUB_LOCK;

    IOHID_DEBUG(kIOHIDDebugCode_DispatchKeyboard, usagePage, usage, value, options);

    if ((( usagePage == kHIDPage_KeyboardOrKeypad ) &&
            (usage != kHIDUsage_KeyboardLockingNumLock) &&
            !(_keyboard.caps.delayMS && (usage == kHIDUsage_KeyboardCapsLock))) ||
            (_keyboard.appleVendorSupported &&
             ((usagePage == kHIDPage_AppleVendorKeyboard) ||
              ((usagePage == kHIDPage_AppleVendorTopCase) &&
               (usage == kHIDUsage_AV_TopCase_KeyboardFn))))) {
        if ( !_keyboardNub )
            _keyboardNub = newKeyboardShim();

        if ( _keyboardNub )
            _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, (value != 0), options);

    }
    else {
        if ( !_consumerNub )
            _consumerNub = newConsumerShim();

        if ( _consumerNub ) {
            if ( (usagePage == kHIDPage_Consumer) && (usage == kHIDUsage_Csmr_Eject) && ((options & kDelayedOption) == 0) && _keyboardNub && ((_keyboardNub->eventFlags() & SPECIALKEYS_MODIFIER_MASK) == 0)) {
                if (( _keyboard.eject.state != value ) && _keyboard.eject.timer) {
                    if ( value ) {
                        _keyboard.eject.options       = options;

                        _keyboard.eject.timer->setTimeoutMS( _keyboard.eject.delayMS );
                    }
                    else {
                        _keyboard.eject.timer->cancelTimeout();
                    }

                   _keyboard.eject.state = value;
                }
            }
            else  if (!(((options & kDelayedOption) == 0) && _keyboard.eject.state && (usagePage == kHIDPage_Consumer) && (usage == kHIDUsage_Csmr_Eject))) {
                _consumerNub->dispatchConsumerEvent(_keyboardNub, timeStamp, usagePage, usage, value, options);
            }
        }

        if ( _keyboard.caps.delayMS && (usagePage == kHIDPage_KeyboardOrKeypad) && (usage == kHIDUsage_KeyboardCapsLock)) {
            if ( (options & kDelayedOption) == 0) {

                if ( getElementValue(kHIDPage_LEDs, kHIDUsage_LED_CapsLock) == 0 ) {
                    if (( _keyboard.caps.state != value ) && _keyboard.caps.timer ) {
                        if ( value ) {
                            _keyboard.caps.options       = options;

                            _keyboard.caps.timer->setTimeoutMS( _keyboard.caps.delayMS );
                        }
                        else {
                            _keyboard.caps.timer->cancelTimeout();
                        }

                        _keyboard.caps.state = value;
                    }
                }
                else {
                    _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);
                }
            }
            else  if (!( ((options & kDelayedOption) == 0) && _keyboard.caps.state ) ) {
                _keyboardNub->dispatchKeyboardEvent(timeStamp, usagePage, usage, value, options);
            }
        }
    }

    NUB_UNLOCK;

#endif /* TARGET_OS_EMBEDDED */ // }

}


//====================================================================================================
// IOHIDEventService::dispatchRelativePointerEvent
//====================================================================================================
void IOHIDEventService::dispatchRelativePointerEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      dx,
                                SInt32                      dy,
                                UInt32                      buttonState,
                                IOOptionBits                options)
{
    IOHID_DEBUG(kIOHIDDebugCode_DispatchRelativePointer, dx, dy, buttonState, options);

    if ( ! _readyForInputReports )
        return;

    if ( !dx && !dy && buttonState == _relativePointer.buttonState )
        return;

#if TARGET_OS_EMBEDDED
    
    IOHIDEvent *event = IOHIDEvent::relativePointerEvent(timeStamp, dx, dy, 0, buttonState, _relativePointer.buttonState);

    if ( event ) {
        dispatchEvent(event);
        event->release();
    }

#else
    NUB_LOCK;

    if ( !_pointingNub )
        _pointingNub = newPointingShim();

    if ( _pointingNub )
        _pointingNub->dispatchRelativePointerEvent(timeStamp, dx, dy, buttonState, options);

    NUB_UNLOCK;
#endif /* TARGET_OS_EMBEDDED */
    
    _relativePointer.buttonState = buttonState;
}

#if TARGET_OS_EMBEDDED
static IOFixed __ScaleToFixed(int32_t value, int32_t min, int32_t max)
{
    int32_t range = max - min;
    int32_t offset = value - min;

    return IOFixedDivide(offset<<16, range<<16);
}
#endif

//====================================================================================================
// IOHIDEventService::dispatchAbsolutePointerEvent
//====================================================================================================
void IOHIDEventService::dispatchAbsolutePointerEvent(
                                                     AbsoluteTime                timeStamp,
                                                     SInt32                      x,
                                                     SInt32                      y,
                                                     IOGBounds *                 bounds,
                                                     UInt32                      buttonState,
                                                     bool                        inRange,
                                                     SInt32                      tipPressure,
                                                     SInt32                      tipPressureMin,
                                                     SInt32                      tipPressureMax,
                                                     IOOptionBits                options)
{
#if TARGET_OS_EMBEDDED

    dispatchDigitizerEvent(timeStamp, 0, kDigitizerTransducerTypeStylus, inRange, buttonState, __ScaleToFixed(x, bounds->minx, bounds->maxx), __ScaleToFixed(y, bounds->miny, bounds->maxy), __ScaleToFixed(tipPressure, tipPressureMin, tipPressureMax));

#else
    IOHID_DEBUG(kIOHIDDebugCode_DispatchAbsolutePointer, x, y, buttonState, options);

    if ( ! _readyForInputReports )
        return;

    if ( !inRange ) {
        buttonState = 0;
        tipPressure = tipPressureMin;
    }

    NUB_LOCK;

    if ( !_pointingNub )
        _pointingNub = newPointingShim();

    IOGPoint newLoc;

    newLoc.x = x;
    newLoc.y = y;

    _pointingNub->dispatchAbsolutePointerEvent(timeStamp, &newLoc, bounds, buttonState, inRange, tipPressure, tipPressureMin, tipPressureMax, options);

    NUB_UNLOCK;
#endif /* !TARGET_OS_EMBEDDED */

}

//====================================================================================================
// IOHIDEventService::dispatchScrollWheelEvent
//====================================================================================================
void IOHIDEventService::dispatchScrollWheelEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      deltaAxis1,
                                SInt32                      deltaAxis2,
                                SInt32                      deltaAxis3,
                                IOOptionBits                options)
{
    bool momentumOrPhase = options & (kHIDDispatchOptionScrollMomentumAny | kHIDDispatchOptionPhaseAny);
    IOHID_DEBUG(kIOHIDDebugCode_DispatchScroll, deltaAxis1, deltaAxis2, deltaAxis3, options);

    if ( ! _readyForInputReports )
        return;
    
    if ( !deltaAxis1 && !deltaAxis2 && !deltaAxis3 && !momentumOrPhase )
        return;

#if TARGET_OS_EMBEDDED

    IOHIDEvent *event = IOHIDEvent::scrollEvent(timeStamp, deltaAxis2, deltaAxis1, deltaAxis3); //yxz should be xyz

    if ( event ) {
        dispatchEvent(event);
        event->release();
    }

#else

    NUB_LOCK;

    if ( !_pointingNub )
        _pointingNub = newPointingShim();

    if ( _pointingNub )
        _pointingNub->dispatchScrollWheelEvent(timeStamp, deltaAxis1, deltaAxis2, deltaAxis3, options);

    NUB_UNLOCK;
#endif /* TARGET_OS_EMBEDDED */
}

#if !TARGET_OS_EMBEDDED
static void ScalePressure(SInt32 *pressure, SInt32 pressureMin, SInt32 pressureMax, SInt32 systemPressureMin, SInt32 systemPressureMax)
{
    SInt64  systemScale = systemPressureMax - systemPressureMin;


    *pressure = ((pressureMin != pressureMax)) ?
                (((unsigned)(*pressure - pressureMin) * systemScale) /
                (unsigned)( pressureMax - pressureMin)) + systemPressureMin: 0;
}
#endif /* TARGET_OS_EMBEDDED */

//====================================================================================================
// IOHIDEventService::dispatchTabletPointEvent
//====================================================================================================
void IOHIDEventService::dispatchTabletPointerEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      transducerID __unused,
                                SInt32                      x,
                                SInt32                      y,
                                SInt32                      z,
                                IOGBounds *                 bounds __unused,
                                UInt32                      buttonState,
                                SInt32                      tipPressure,
                                SInt32                      tipPressureMin,
                                SInt32                      tipPressureMax,
                                SInt32                      barrelPressure,
                                SInt32                      barrelPressureMin,
                                SInt32                      barrelPressureMax,
                                SInt32                      tiltX,
                                SInt32                      tiltY,
                                UInt32                      twist,
                                IOOptionBits                options)
{
#if !TARGET_OS_EMBEDDED
    IOHID_DEBUG(kIOHIDDebugCode_DispatchTabletPointer, x, y, buttonState, options);

    if ( ! _readyForInputReports )
        return;

    NUB_LOCK;

    if ( !_pointingNub )
        _pointingNub = newPointingShim();

    NXEventData         tabletData = {};

    ScalePressure(&tipPressure, tipPressureMin, tipPressureMax, 0, kMaxSystemTipPressure);
    ScalePressure(&barrelPressure, barrelPressureMin, barrelPressureMax, -kMaxSystemBarrelPressure, kMaxSystemBarrelPressure);

    IOGPoint newLoc;

    newLoc.x = x;
    newLoc.y = y;

    //IOHIDSystem::scaleLocationToCurrentScreen(&newLoc, bounds);

    tabletData.tablet.x                    = newLoc.x;
    tabletData.tablet.y                    = newLoc.y;
    tabletData.tablet.z                    = z;
    tabletData.tablet.buttons              = buttonState;
    tabletData.tablet.pressure             = tipPressure;
    tabletData.tablet.tilt.x               = tiltX;
    tabletData.tablet.tilt.y               = tiltY;
    tabletData.tablet.rotation             = twist;
    tabletData.tablet.tangentialPressure   = barrelPressure;
    tabletData.tablet.deviceID             = _digitizer.deviceID;

    _pointingNub->dispatchTabletEvent(&tabletData, timeStamp);

    NUB_UNLOCK;
#endif /* !TARGET_OS_EMBEDDED */
}

//====================================================================================================
// IOHIDEventService::dispatchTabletProximityEvent
//====================================================================================================
void IOHIDEventService::dispatchTabletProximityEvent(
                                AbsoluteTime                timeStamp,
                                UInt32                      transducerID,
                                bool                        inRange,
                                bool                        invert,
                                UInt32                      vendorTransducerUniqueID,
                                UInt32                      vendorTransducerSerialNumber,
                                IOOptionBits                options)
{
#if !TARGET_OS_EMBEDDED
    IOHID_DEBUG(kIOHIDDebugCode_DispatchTabletProx, transducerID, vendorTransducerUniqueID, vendorTransducerSerialNumber, options);

    if ( ! _readyForInputReports )
        return;

    NUB_LOCK;

    if ( !_pointingNub )
        _pointingNub = newPointingShim();

    NXEventData tabletData      = {};
    UInt32      capabilityMask  = NX_TABLET_CAPABILITY_DEVICEIDMASK | NX_TABLET_CAPABILITY_ABSXMASK | NX_TABLET_CAPABILITY_ABSYMASK;
    
    if ( _digitizer.deviceID == 0 )
        _digitizer.deviceID = IOHIDPointing::generateDeviceID();
    
    if ( options & kDigitizerCapabilityButtons )
        capabilityMask |= NX_TABLET_CAPABILITY_BUTTONSMASK;
    if ( options & kDigitizerCapabilityPressure )
        capabilityMask |= NX_TABLET_CAPABILITY_PRESSUREMASK;
    if ( options & kDigitizerCapabilityTangentialPressure )
        capabilityMask |= NX_TABLET_CAPABILITY_TANGENTIALPRESSUREMASK;
    if ( options & kDigitizerCapabilityZ )
        capabilityMask |= NX_TABLET_CAPABILITY_ABSZMASK;
    if ( options & kDigitizerCapabilityTiltX )
        capabilityMask |= NX_TABLET_CAPABILITY_TILTXMASK;
    if ( options & kDigitizerCapabilityTiltY )
        capabilityMask |= NX_TABLET_CAPABILITY_TILTYMASK;
    if ( options & kDigitizerCapabilityTwist )
        capabilityMask |= NX_TABLET_CAPABILITY_ROTATIONMASK;

    tabletData.proximity.vendorID               = getVendorID();
    tabletData.proximity.tabletID               = getProductID();
    tabletData.proximity.pointerID              = transducerID;
    tabletData.proximity.deviceID               = _digitizer.deviceID;
    tabletData.proximity.vendorPointerType      = NX_TABLET_POINTER_PEN;
    tabletData.proximity.pointerSerialNumber    = vendorTransducerSerialNumber;
    tabletData.proximity.uniqueID               = vendorTransducerUniqueID;
    tabletData.proximity.capabilityMask         = capabilityMask;
    tabletData.proximity.enterProximity         = inRange;
    tabletData.proximity.pointerType            = invert ? NX_TABLET_POINTER_ERASER : NX_TABLET_POINTER_PEN;

    _pointingNub->dispatchProximityEvent(&tabletData, timeStamp);

    NUB_UNLOCK;

#endif /* !TARGET_OS_EMBEDDED */
}

bool IOHIDEventService::readyForReports()
{
    return _readyForInputReports;
}

//==============================================================================
// IOHIDEventService::getDeviceUsagePairs
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  0);
OSArray * IOHIDEventService::getDeviceUsagePairs()
{
    //RY: Correctly deal with kIOHIDDeviceUsagePairsKey
    OSArray * providerUsagePairs = _provider ? (OSArray*)_provider->copyProperty(kIOHIDDeviceUsagePairsKey) : NULL;

    if ( OSDynamicCast(OSArray, providerUsagePairs) && ( providerUsagePairs != _deviceUsagePairs ) ) {
        setProperty(kIOHIDDeviceUsagePairsKey, providerUsagePairs);
        if ( _deviceUsagePairs )
            _deviceUsagePairs->release();

        _deviceUsagePairs = providerUsagePairs;
        _deviceUsagePairs->retain();
    }
#if TARGET_OS_EMBEDDED
    else if ( !_deviceUsagePairs ) {
        _deviceUsagePairs = OSArray::withCapacity(2);

        if ( _deviceUsagePairs ) {
            OSDictionary * pair = OSDictionary::withCapacity(2);

            if ( pair ) {
                OSNumber * number;

                number = OSNumber::withNumber(getPrimaryUsagePage(), 32);
                if ( number ) {
                    pair->setObject(kIOHIDDeviceUsagePageKey, number);
                    number->release();
                }

                number = OSNumber::withNumber(getPrimaryUsage(), 32);
                if ( number ) {
                    pair->setObject(kIOHIDDeviceUsageKey, number);
                    number->release();
                }

                _deviceUsagePairs->setObject(pair);
                pair->release();
            }
        }
    }
#endif
    OSSafeRelease(providerUsagePairs);

    return _deviceUsagePairs;
}

//==============================================================================
// IOHIDEventService::getReportInterval
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  1);
UInt32 IOHIDEventService::getReportInterval()
{
    UInt32 interval = 8000; // default to 8 milliseconds
    OSObject *object = copyProperty(kIOHIDReportIntervalKey, gIOServicePlane, kIORegistryIterateRecursively | kIORegistryIterateParents);
    if ( OSDynamicCast(OSNumber, object) )
        interval = ((OSNumber*)object)->unsigned32BitValue();
    OSSafeReleaseNULL(object);

    return interval;
}

#define kCenteredPointerMaxRelativeValue 8
#define GET_RELATIVE_VALUE_FROM_CENTERED(centered,relative) \
    relative = (centered * kCenteredPointerMaxRelativeValue) >> 16;\

OSMetaClassDefineReservedUsed(IOHIDEventService,  2);
//==============================================================================
// IOHIDEventService::dispatchMultiAxisPointerEvent
//==============================================================================
void IOHIDEventService::dispatchMultiAxisPointerEvent(
                                                    AbsoluteTime               timeStamp,
                                                    UInt32                     buttonState,
                                                    IOFixed                    x,
                                                    IOFixed                    y,
                                                    IOFixed                    z,
                                                    IOFixed                    rX,
                                                    IOFixed                    rY,
                                                    IOFixed                    rZ,
                                                    IOOptionBits               options)
{

    bool    validAxis       = false;
    bool    validRelative   = false;
    bool    validScroll     = false;
    bool    isZButton       = false;
    UInt32  interval        = 0;

    if ( ! _readyForInputReports )
        return;

    validRelative   = ( options & kMultiAxisOptionRotationForTranslation ) ? rX || rY || _multiAxis.rX || _multiAxis.rY : x || y || _multiAxis.x || _multiAxis.y;
    validScroll     = rZ || _multiAxis.rZ;

    validAxis       = x || y || z || rX || rY || rZ || _multiAxis.x || _multiAxis.y || _multiAxis.z || _multiAxis.rX || _multiAxis.rY || _multiAxis.rZ;

    if ( options & kMultiAxisOptionZForScroll ) {
        validScroll |= z || _multiAxis.z;
    }
    // if z greater than .75 make it a button
    else if ( z > 0xc000 ){
        isZButton = true;
        buttonState |= 1;
    }

    validRelative |= buttonState != _multiAxis.buttonState;

    if ( validAxis || validRelative || validScroll ) {

        SInt32 dx = 0;
        SInt32 dy = 0;
        SInt32 sx = 0;
        SInt32 sy = 0;

        if ( !isZButton && (options & kMultiAxisOptionRotationForTranslation) ) {
            GET_RELATIVE_VALUE_FROM_CENTERED(-rY, dx);
            GET_RELATIVE_VALUE_FROM_CENTERED(rX, dy);
        } else {
            GET_RELATIVE_VALUE_FROM_CENTERED(x, dx);
            GET_RELATIVE_VALUE_FROM_CENTERED(y, dy);
        }

        GET_RELATIVE_VALUE_FROM_CENTERED(rZ, sy);

        if ( options & kMultiAxisOptionZForScroll )
            GET_RELATIVE_VALUE_FROM_CENTERED(z, sx);

#if TARGET_OS_EMBEDDED
        IOHIDEvent * subEvent = IOHIDEvent::multiAxisPointerEvent(timeStamp, x, y, z, rX, rY, rZ, buttonState, _multiAxis.buttonState, options);
        if ( subEvent ) {

            IOHIDEvent * event;

            if ( validRelative || (!validRelative && !validScroll) ) {
                event = IOHIDEvent::relativePointerEvent(timeStamp, dx, dy, 0, buttonState);
                if ( event ) {

                    if ( subEvent ) {
                        event->appendChild(subEvent);
                    }

                    dispatchEvent(event);
                    event->release();
                }
            }

            if ( validScroll ) {
                event = IOHIDEvent::scrollEvent(timeStamp, sx, sy, 0);
                if ( event ) {

                    if ( subEvent ) {
                        event->appendChild(subEvent);
                    }

                    dispatchEvent(event);
                    event->release();
                }
            }

            subEvent->release();
        }
#else
        dispatchRelativePointerEvent(timeStamp, dx, dy, buttonState, options);
        dispatchScrollWheelEvent(timeStamp, sy, sx, 0, options);
#endif

        if ( (options & kIOHIDEventOptionIsRepeat) == 0 ) {
            _multiAxis.timer->cancelTimeout();
            if ( validAxis )
                interval = getReportInterval() + getReportInterval()/2;
        } else if ( validAxis ) {
            interval = getReportInterval();
        }

        if ( interval )
            _multiAxis.timer->setTimeoutUS(interval);

    }

    _multiAxis.x            = x;
    _multiAxis.y            = y;
    _multiAxis.z            = z;
    _multiAxis.rX           = rX;
    _multiAxis.rY           = rY;
    _multiAxis.rZ           = rZ;
    _multiAxis.buttonState  = buttonState;
    _multiAxis.options      = (options & ~kIOHIDEventOptionIsRepeat);
}

//==============================================================================
// IOHIDEventService::dispatchDigitizerEventWithOrientation
//==============================================================================
void IOHIDEventService::dispatchDigitizerEventWithOrientation(
                                AbsoluteTime                    timeStamp,
                                UInt32                          transducerID,
                                DigitizerTransducerType         type __unused,
                                bool                            inRange,
                                UInt32                          buttonState,
                                IOFixed                         x,
                                IOFixed                         y,
                                IOFixed                         z,
                                IOFixed                         tipPressure,
                                IOFixed                         auxPressure,
                                IOFixed                         twist,
                                DigitizerOrientationType        orientationType,
                                IOFixed *                       orientationParams,
                                UInt32                          orientationParamCount,
                                IOOptionBits                    options)
{
    IOHID_DEBUG(kIOHIDDebugCode_DispatchDigitizer, x, y, buttonState, options);

    IOFixed params[5]   = {};
    bool    touch       = false;

    if ( ! _readyForInputReports )
        return;

    if ( !inRange ) {
        buttonState = 0;
        tipPressure = 0;
    }

    if ( orientationParams ) {
        orientationParamCount = min(5, orientationParamCount);

        bcopy(orientationParams, params, sizeof(IOFixed) * orientationParamCount);
    }

#if TARGET_OS_EMBEDDED
    IOHIDEvent *    collectionEvent = NULL;
    IOHIDEvent *    childEvent      = NULL;
    SInt32          eventMask       = 0;   // what's changed
    UInt32          eventOptions    = 0;

    if ( options & kDigitizerInvert )
        eventOptions |= kIOHIDTransducerInvert;

    childEvent = IOHIDEvent::digitizerEvent(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, eventOptions);
    require(childEvent, exit);

    buttonState |= (tipPressure>>16) & 1;

    if ( tipPressure )
        touch |= 1;
    else
        touch |= buttonState & 1;

    childEvent->setIntegerValue(kIOHIDEventFieldDigitizerTouch, touch);
    if (touch != _digitizer.touch) {
        eventMask |= kIOHIDDigitizerEventTouch;
    }

    if (inRange != _digitizer.range) {
        eventMask |= kIOHIDDigitizerEventRange;

        if ( inRange ) {
            _digitizer.x = x;
            _digitizer.y = y;
            eventMask |= kIOHIDDigitizerEventIdentity;
        }
    }

    if (inRange && ( (_digitizer.x != x) || (_digitizer.y != y) || (_digitizer.z != z) ) ) {
        eventMask |= kIOHIDDigitizerEventPosition;
    }


    childEvent->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, eventMask);

    collectionEvent = IOHIDEvent::digitizerEvent(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, eventOptions);
    require(collectionEvent, exit);

    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerCollection, TRUE);
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerRange, childEvent->getIntegerValue(kIOHIDEventFieldDigitizerRange));
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, childEvent->getIntegerValue(kIOHIDEventFieldDigitizerEventMask));
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerTouch, childEvent->getIntegerValue(kIOHIDEventFieldDigitizerTouch));

    collectionEvent->appendChild(childEvent);

    dispatchEvent(collectionEvent);

exit:
    if ( collectionEvent )
        collectionEvent->release();

    if ( childEvent )
        childEvent->release();

#else

    bool invert = options & kDigitizerInvert;

    // Entering proximity
    if ( inRange && inRange != _digitizer.range ) {
        dispatchTabletProximityEvent(timeStamp, transducerID, inRange, invert, options);
    }

    if ( inRange ) {
        Bounds  bounds = {0, kMaxSystemAbsoluteRangeSigned, 0, kMaxSystemAbsoluteRangeSigned};

        SInt32 scaledX      = ((SInt64)x * kMaxSystemAbsoluteRangeSigned) >> 16;
        SInt32 scaledY      = ((SInt64)y * kMaxSystemAbsoluteRangeSigned) >> 16;
        SInt32 scaledZ      = ((SInt64)z * kMaxSystemAbsoluteRangeSigned) >> 16;
        SInt32 scaledTP     = ((SInt64)tipPressure * EV_MAXPRESSURE) >> 16;
        SInt32 scaledBP     = ((SInt64)auxPressure * EV_MAXPRESSURE) >> 16;
        SInt32 scaledTiltX  = (((SInt64)params[0] * kMaxSystemAbsoluteRangeSigned)/90) >> 16;
        SInt32 scaledTiltY  = (((SInt64)params[1] * kMaxSystemAbsoluteRangeSigned)/90) >> 16;

        if ( orientationType != kDigitizerOrientationTypeTilt )
            bzero(params, sizeof(params));

        dispatchTabletPointerEvent(timeStamp, transducerID, scaledX, scaledY, scaledZ, &bounds, buttonState, scaledTP, 0, EV_MAXPRESSURE, scaledBP, 0, EV_MAXPRESSURE, scaledTiltX, scaledTiltY, twist>>10 /*10:6 fixed*/);

        dispatchAbsolutePointerEvent(timeStamp, scaledX, scaledY, &bounds, buttonState, inRange, scaledTP, 0, EV_MAXPRESSURE);
    }

    if ( !inRange && inRange != _digitizer.range ) {
        dispatchTabletProximityEvent(timeStamp, transducerID, inRange, invert, options);
    }



#endif /* TARGET_OS_EMBEDDED */

    _digitizer.range        = inRange;
    _digitizer.x            = x;
    _digitizer.y            = y;
    _digitizer.z            = z;
    _digitizer.touch        = touch;

}

//==============================================================================
// IOHIDEventService::dispatchDigitizerEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  3);
void IOHIDEventService::dispatchDigitizerEvent(
                                               AbsoluteTime                    timeStamp,
                                               UInt32                          transducerID,
                                               DigitizerTransducerType         type,
                                               bool                            inRange,
                                               UInt32                          buttonState,
                                               IOFixed                         x,
                                               IOFixed                         y,
                                               IOFixed                         z,
                                               IOFixed                         tipPressure,
                                               IOFixed                         auxPressure,
                                               IOFixed                         twist,
                                               IOOptionBits                    options )
{
    dispatchDigitizerEventWithOrientation(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, kDigitizerOrientationTypeTilt, NULL, 0, options);
}

//==============================================================================
// IOHIDEventService::dispatchDigitizerEventWithTiltOrientation
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  4);
void IOHIDEventService::dispatchDigitizerEventWithTiltOrientation(
                                                                  AbsoluteTime                    timeStamp,
                                                                  UInt32                          transducerID,
                                                                  DigitizerTransducerType         type,
                                                                  bool                            inRange,
                                                                  UInt32                          buttonState,
                                                                  IOFixed                         x,
                                                                  IOFixed                         y,
                                                                  IOFixed                         z,
                                                                  IOFixed                         tipPressure,
                                                                  IOFixed                         auxPressure,
                                                                  IOFixed                         twist,
                                                                  IOFixed                         tiltX,
                                                                  IOFixed                         tiltY,
                                                                  IOOptionBits                    options)
{
    IOFixed params[] = {tiltX, tiltY};

    dispatchDigitizerEventWithOrientation(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, kDigitizerOrientationTypeTilt, params, sizeof(params)/sizeof(IOFixed), options);
}


//==============================================================================
// IOHIDEventService::dispatchDigitizerEventWithPolarOrientation
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  5);
void IOHIDEventService::dispatchDigitizerEventWithPolarOrientation(
                                                                   AbsoluteTime                    timeStamp,
                                                                   UInt32                          transducerID,
                                                                   DigitizerTransducerType         type,
                                                                   bool                            inRange,
                                                                   UInt32                          buttonState,
                                                                   IOFixed                         x,
                                                                   IOFixed                         y,
                                                                   IOFixed                         z,
                                                                   IOFixed                         tipPressure,
                                                                   IOFixed                         auxPressure,
                                                                   IOFixed                         twist,
                                                                   IOFixed                         altitude,
                                                                   IOFixed                         azimuth,
                                                                   IOOptionBits                    options)
{
    IOFixed params[] = {altitude, azimuth};

    dispatchDigitizerEventWithOrientation(timeStamp, transducerID, type, inRange, buttonState, x, y, z, tipPressure, auxPressure, twist, kDigitizerOrientationTypePolar, params, sizeof(params)/sizeof(IOFixed), options);
}


//==============================================================================
// IOHIDEventService::dispatchUnicodeEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  6);
void IOHIDEventService::dispatchUnicodeEvent(AbsoluteTime timeStamp, UInt8 * payload, UInt32 length, UnicodeEncodingType encoding, IOFixed quality, IOOptionBits options)
{
#if TARGET_OS_EMBEDDED
    IOHIDEvent * event = IOHIDEvent::unicodeEvent(timeStamp, payload, length, encoding, quality, options);
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
#else
#pragma unused(timeStamp, payload, length, encoding, quality, options)
#endif
}

#if TARGET_OS_EMBEDDED
//==============================================================================
// IOHIDEventService::dispatchStandardGameControllerEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  12);
void IOHIDEventService::dispatchStandardGameControllerEvent(
                                                            AbsoluteTime                    timeStamp,
                                                            IOFixed                         dpadUp,
                                                            IOFixed                         dpadDown,
                                                            IOFixed                         dpadLeft,
                                                            IOFixed                         dpadRight,
                                                            IOFixed                         faceX,
                                                            IOFixed                         faceY,
                                                            IOFixed                         faceA,
                                                            IOFixed                         faceB,
                                                            IOFixed                         shoulderL,
                                                            IOFixed                         shoulderR,
                                                            IOOptionBits                    options)
{
    IOHIDEvent * event = IOHIDEvent::standardGameControllerEvent(timeStamp, dpadUp, dpadDown, dpadLeft, dpadRight, faceX, faceY, faceA, faceB, shoulderL, shoulderR, options);
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
}

//==============================================================================
// IOHIDEventService::dispatchExtendedGameControllerEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  13);
void IOHIDEventService::dispatchExtendedGameControllerEvent(
                                                            AbsoluteTime                    timeStamp,
                                                            IOFixed                         dpadUp,
                                                            IOFixed                         dpadDown,
                                                            IOFixed                         dpadLeft,
                                                            IOFixed                         dpadRight,
                                                            IOFixed                         faceX,
                                                            IOFixed                         faceY,
                                                            IOFixed                         faceA,
                                                            IOFixed                         faceB,
                                                            IOFixed                         shoulderL1,
                                                            IOFixed                         shoulderR1,
                                                            IOFixed                         shoulderL2,
                                                            IOFixed                         shoulderR2,
                                                            IOFixed                         joystickX,
                                                            IOFixed                         joystickY,
                                                            IOFixed                         joystickZ,
                                                            IOFixed                         joystickRz,
                                                            IOOptionBits                    options)
{
    IOHIDEvent * event = IOHIDEvent::extendedGameControllerEvent(timeStamp, dpadUp, dpadDown, dpadLeft, dpadRight, faceX, faceY, faceA, faceB, shoulderL1, shoulderR1, shoulderL2, shoulderR2, joystickX, joystickY, joystickZ, joystickRz, options);
    
    if ( event ) {
        dispatchEvent(event);
        event->release();
    }
}


void IOHIDEventService::close(IOService *forClient, IOOptionBits options)
{
    _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventService::closeGated), forClient, &options);
}

void IOHIDEventService::closeGated(IOService *forClient, IOOptionBits * pOptions)
{
    super::close(forClient, *pOptions);
}

OSDefineMetaClassAndStructors(IOHIDClientData, OSObject)

IOHIDClientData * IOHIDClientData::withClientInfo(IOService *client, void* context, void * action)
{
    IOHIDClientData * data = new IOHIDClientData;

    if (!data) { }
    else if (data->init()) {
        data->client  = client;
        data->context = context;
        data->action  = action;
    } else {
        data->release();
        data = NULL;
    }

    return data;
}

//==============================================================================
// IOHIDEventService::open
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  7);
bool IOHIDEventService::open(   IOService *                 client,
                                IOOptionBits                options,
                                void *                      context,
                                Action                      action)
{
    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventService::openGated), client, &options, context, (void*)action);
}

//==============================================================================
// IOHIDEventService::dispatchEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  7);
void IOHIDEventService::dispatchEvent(IOHIDEvent * event, IOOptionBits options)
{
    OSCollectionIterator *  iterator = OSCollectionIterator::withCollection(_clientDict);
    IOHIDClientData *       clientData;
    OSObject *              clientKey;
    IOService *             client;
    void *                  context;
    Action                  action;

    event->setSenderID(getRegistryEntryID());

    IOHID_DEBUG(kIOHIDDebugCode_DispatchHIDEvent, options, 0, 0, 0);

    if ( !iterator )
        return;

    while ((clientKey = iterator->getNextObject())) {

        clientData = OSDynamicCast(IOHIDClientData, _clientDict->getObject((const OSSymbol *)clientKey));

        if ( !clientData )
            continue;

        client  = clientData->getClient();
        context = clientData->getContext();
        action  = (Action)clientData->getAction();

        if ( action )
            (*action)(client, this, context, event, options);
    }

    iterator->release();

}

//==============================================================================
// IOHIDEventService::getPrimaryUsagePage
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  7);
UInt32 IOHIDEventService::getPrimaryUsagePage ()
{
    UInt32		primaryUsagePage = 0;
    OSArray *	deviceUsagePairs = getDeviceUsagePairs();

    if ( deviceUsagePairs && deviceUsagePairs->getCount() ) {
        OSDictionary * pair = OSDynamicCast(OSDictionary, deviceUsagePairs->getObject(0));

        if ( pair ) {
            OSNumber * number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsagePageKey));

            if ( number )
                primaryUsagePage = number->unsigned32BitValue();
        }
    }

    return primaryUsagePage;
}

//==============================================================================
// IOHIDEventService::getPrimaryUsage
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  10);
UInt32 IOHIDEventService::getPrimaryUsage ()
{
    UInt32		primaryUsage		= 0;
    OSArray *	deviceUsagePairs	= getDeviceUsagePairs();

    if ( deviceUsagePairs && deviceUsagePairs->getCount() ) {
        OSDictionary * pair = OSDynamicCast(OSDictionary, deviceUsagePairs->getObject(0));

        if ( pair ) {
            OSNumber * number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsageKey));

            if ( number )
                primaryUsage = number->unsigned32BitValue();
        }
    }

    return primaryUsage;
}

//==============================================================================
// IOHIDEventService::copyEvent
//==============================================================================
OSMetaClassDefineReservedUsed(IOHIDEventService,  11);
IOHIDEvent * IOHIDEventService::copyEvent(
                                IOHIDEventType              type,
                                IOHIDEvent *                matching,
                                IOOptionBits                options)
{
    return NULL;
}

//==============================================================================
// IOHIDEventService::openGated
//==============================================================================
bool IOHIDEventService::openGated(IOService *                 client,
                                  IOOptionBits *              pOptions,
                                  void *                      context,
                                  Action                      action)
{
    IOHIDClientData * clientData =
    IOHIDClientData::withClientInfo(client, context, (void*)action);
    bool ret = false;

    if ( clientData ) {
        if ( super::open(client, *pOptions, clientData) )
            ret = true;
        clientData->release();
    }

    return ret;
}



#else

OSMetaClassDefineReservedUnused(IOHIDEventService,  7);
OSMetaClassDefineReservedUnused(IOHIDEventService,  8);
OSMetaClassDefineReservedUnused(IOHIDEventService,  9);
OSMetaClassDefineReservedUnused(IOHIDEventService, 10);
OSMetaClassDefineReservedUnused(IOHIDEventService, 11);
OSMetaClassDefineReservedUnused(IOHIDEventService, 12);
OSMetaClassDefineReservedUnused(IOHIDEventService, 13);
#endif /* TARGET_OS_EMBEDDED */
OSMetaClassDefineReservedUnused(IOHIDEventService, 14);
OSMetaClassDefineReservedUnused(IOHIDEventService, 15);
OSMetaClassDefineReservedUnused(IOHIDEventService, 16);
OSMetaClassDefineReservedUnused(IOHIDEventService, 17);
OSMetaClassDefineReservedUnused(IOHIDEventService, 18);
OSMetaClassDefineReservedUnused(IOHIDEventService, 19);
OSMetaClassDefineReservedUnused(IOHIDEventService, 20);
OSMetaClassDefineReservedUnused(IOHIDEventService, 21);
OSMetaClassDefineReservedUnused(IOHIDEventService, 22);
OSMetaClassDefineReservedUnused(IOHIDEventService, 23);
OSMetaClassDefineReservedUnused(IOHIDEventService, 24);
OSMetaClassDefineReservedUnused(IOHIDEventService, 25);
OSMetaClassDefineReservedUnused(IOHIDEventService, 26);
OSMetaClassDefineReservedUnused(IOHIDEventService, 27);
OSMetaClassDefineReservedUnused(IOHIDEventService, 28);
OSMetaClassDefineReservedUnused(IOHIDEventService, 29);
OSMetaClassDefineReservedUnused(IOHIDEventService, 30);
OSMetaClassDefineReservedUnused(IOHIDEventService, 31);

