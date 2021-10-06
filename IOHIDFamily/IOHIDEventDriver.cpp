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

#include <AssertMacros.h>
#include "IOHIDEventDriver.h"
#include "IOHIDInterface.h"
#include "IOHIDKeys.h"
#include "IOHIDTypes.h"
#include "AppleHIDUsageTables.h"
#include "IOHIDPrivateKeys.h"
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/IOLib.h>
#include <IOKit/usb/USB.h>
#include "IOHIDFamilyTrace.h"
#include "IOHIDEventTypes.h"
#include "IOHIDDebug.h"
#include "IOHIDEvent.h"

enum {
    kMouseButtons   = 0x1,
    kMouseXYAxis    = 0x2,
    kBootMouse      = (kMouseXYAxis | kMouseButtons)
};

enum {
    kBootProtocolNone   = 0,
    kBootProtocolKeyboard,
    kBootProtocolMouse
};


enum {
    kIOParseVendorMessageReport,
    kIODispatchVendorMessageReport
};
#define GetReportType( type )                                               \
    ((type <= kIOHIDElementTypeInput_ScanCodes) ? kIOHIDReportTypeInput :   \
    (type <= kIOHIDElementTypeOutput) ? kIOHIDReportTypeOutput :            \
    (type <= kIOHIDElementTypeFeature) ? kIOHIDReportTypeFeature : -1)

#define GET_AXIS_COUNT(usage) (usage-kHIDUsage_GD_X+ 1)
#define GET_AXIS_INDEX(usage) (usage-kHIDUsage_GD_X)

#define kDefaultAbsoluteAxisRemovalPercentage           15
#define kDefaultPreferredAxisRemovalPercentage          10

#define kHIDUsage_MFiGameController_LED0                0xFF00

//===========================================================================
// EventElementCollection class
class EventElementCollection: public OSObject
{
    OSDeclareDefaultStructors(EventElementCollection)
public:
    OSArray *       elements;
    IOHIDElement *  collection;
    
    static EventElementCollection * candidate(IOHIDElement * parent);
    
    virtual void free();
    virtual OSDictionary * copyProperties() const;
    virtual bool serialize(OSSerialize * serializer) const;
};

OSDefineMetaClassAndStructors(EventElementCollection, OSObject)

EventElementCollection * EventElementCollection::candidate(IOHIDElement * gestureCollection)
{
    EventElementCollection * result = NULL;
    
    result = new EventElementCollection;
    
    require(result, exit);
    
    require_action(result->init(), exit, result=NULL);
    
    result->collection = gestureCollection;
    
    if ( result->collection )
        result->collection->retain();
    
    result->elements = OSArray::withCapacity(4);
    require_action(result->elements, exit, OSSafeReleaseNULL(result));
    
exit:
    return result;
}

OSDictionary * EventElementCollection::copyProperties() const
{
    OSDictionary * tempDictionary = OSDictionary::withCapacity(2);
    if ( tempDictionary ) {
        tempDictionary->setObject(kIOHIDElementParentCollectionKey, collection);
        tempDictionary->setObject(kIOHIDElementKey, elements);
    }
    return tempDictionary;
}

bool EventElementCollection::serialize(OSSerialize * serializer) const
{
    OSDictionary * tempDictionary = copyProperties();
    bool result = false;
    
    if ( tempDictionary ) {
        tempDictionary->serialize(serializer);
        tempDictionary->release();
        
        result = true;
    }
    
    return result;
}

void EventElementCollection::free()
{
    OSSafeReleaseNULL(collection);
    OSSafeReleaseNULL(elements);
    OSObject::free();
}

//===========================================================================
// DigitizerTransducer class
class DigitizerTransducer: public EventElementCollection
{
    OSDeclareDefaultStructors(DigitizerTransducer)
public:

    uint32_t  type;
    uint32_t  touch;
    boolean_t inRange;
    IOFixed   X;
    IOFixed   Y;
    IOFixed   Z;
  
    static DigitizerTransducer * transducer(uint32_t type, IOHIDElement * parent);
    
    virtual OSDictionary * copyProperties() const;
};

OSDefineMetaClassAndStructors(DigitizerTransducer, EventElementCollection)

DigitizerTransducer * DigitizerTransducer::transducer(uint32_t digitzerType, IOHIDElement * digitizerCollection)
{
    DigitizerTransducer * result = NULL;
    
    result = new DigitizerTransducer;
    
    require(result, exit);
    
    require_action(result->init(), exit, result=NULL);
    
    result->type        = digitzerType;
    result->collection  = digitizerCollection;
    result->touch       = 0;
    result->X = result->Y = result->Z = 0;
    result->inRange     = false;
  
    if ( result->collection )
        result->collection->retain();
    
    result->elements = OSArray::withCapacity(4);
    require_action(result->elements, exit, OSSafeReleaseNULL(result));
    
exit:
    return result;
}

OSDictionary * DigitizerTransducer::copyProperties() const
{
    OSDictionary * tempDictionary = EventElementCollection::copyProperties();
    if ( tempDictionary ) {
        OSNumber * number = OSNumber::withNumber(type, 32);
        if ( number ) {
            tempDictionary->setObject("Type", number);
            number->release();
        }
    }
    return tempDictionary;
}

//===========================================================================
// IOHIDEventDriver class

#define super IOHIDEventService

OSDefineMetaClassAndStructors( IOHIDEventDriver, IOHIDEventService )

#define _led                            _reserved->led
#define _keyboard                       _reserved->keyboard
#define _scroll                         _reserved->scroll
#define _relative                       _reserved->relative
#define _multiAxis                      _reserved->multiAxis
#define _gameController                 _reserved->gameController
#define _digitizer                      _reserved->digitizer
#define _unicode                        _reserved->unicode
#define _absoluteAxisRemovalPercentage  _reserved->absoluteAxisRemovalPercentage
#define _preferredAxisRemovalPercentage _reserved->preferredAxisRemovalPercentage
#define _lastReportTime                 _reserved->lastReportTime
#define _vendorMessage                  _reserved->vendorMessage

//====================================================================================================
// IOHIDEventDriver::init
//====================================================================================================
bool IOHIDEventDriver::init( OSDictionary * dictionary )
{
    bool result;
    
    require_action(super::init(dictionary), exit, result=false);

    _reserved = IONew(ExpansionData, 1);
    bzero(_reserved, sizeof(ExpansionData));
    
    _preferredAxisRemovalPercentage = kDefaultPreferredAxisRemovalPercentage;
    
    result = true;
    
exit:
    return result;
}

//====================================================================================================
// IOHIDEventDriver::free
//====================================================================================================
void IOHIDEventDriver::free ()
{
    OSSafeReleaseNULL(_relative.elements);
    OSSafeReleaseNULL(_multiAxis.elements);
    OSSafeReleaseNULL(_digitizer.transducers);
    OSSafeReleaseNULL(_digitizer.touchCancelElement);
    OSSafeReleaseNULL(_digitizer.deviceModeElement);
    OSSafeReleaseNULL(_scroll.elements);
    OSSafeReleaseNULL(_led.elements);
    OSSafeReleaseNULL(_keyboard.elements);
    OSSafeReleaseNULL(_unicode.legacyElements);
    OSSafeReleaseNULL(_unicode.gesturesCandidates);
    OSSafeReleaseNULL(_unicode.gestureStateElement);
    OSSafeReleaseNULL(_gameController.elements);
    OSSafeReleaseNULL(_vendorMessage.elements);
    OSSafeReleaseNULL(_vendorMessage.pendingEvents);
    OSSafeReleaseNULL(_supportedElements);

    if (_reserved) {
        IODelete(_reserved, ExpansionData, 1);
        _reserved = NULL;
    }

    super::free();
}

//====================================================================================================
// IOHIDEventDriver::handleStart
//====================================================================================================
bool IOHIDEventDriver::handleStart( IOService * provider )
{
    _interface = OSDynamicCast( IOHIDInterface, provider );

    if ( !_interface )
        return false;

    IOService * service = this;

    // Check to see if this is a product of an IOHIDDeviceShim
    while ( NULL != (service = service->getProvider()) ) {
        if(service->metaCast("IOHIDDeviceShim") && !service->metaCast("IOHIDPointingEventDevice") && !service->metaCast("IOHIDKeyboardEventDevice")) {
            return false;
        }
    }

    if (!_interface->open(this, 0, OSMemberFunctionCast(IOHIDInterface::InterruptReportAction, this, &IOHIDEventDriver::handleInterruptReport), NULL))
        return false;

    UInt32      bootProtocol    = 0;
    OSObject *  obj             = _interface->copyProperty("BootProtocol");
    OSNumber *  number          = OSDynamicCast(OSNumber, obj);

    if (number) {
      bootProtocol = number->unsigned32BitValue();
      setProperty("BootProtocol", number);
    }
    OSSafeReleaseNULL(obj);
    
#if TARGET_OS_EMBEDDED
    obj = _interface->copyProperty(kIOHIDAuthenticatedDeviceKey);
    OSBoolean *authenticated = OSDynamicCast(OSBoolean, obj);
    _authenticatedDevice = (authenticated && authenticated == kOSBooleanTrue);
    OSSafeReleaseNULL(obj);
    if ((conformTo(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad) ||
        conformTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick)) &&
        _authenticatedDevice == false) {
        HIDLogError("Un-authenticated game controller device attached");
        return false;
    }
#else
    _authenticatedDevice = true;
#endif

    obj = copyProperty(kIOHIDAbsoluteAxisBoundsRemovalPercentage, gIOServicePlane);
    number = OSDynamicCast(OSNumber, obj);
    if ( number ) {
        _absoluteAxisRemovalPercentage = number->unsigned32BitValue();
    }
    OSSafeReleaseNULL(obj);
    
    OSArray *elements = _interface->createMatchingElements();
    bool result = false;

    _keyboard.appleVendorSupported = getProperty(kIOHIDAppleVendorSupported, gIOServicePlane);

    if ( elements ) {
        if ( parseElements ( elements, bootProtocol )) {
            result = true;
        }
    }
    OSSafeReleaseNULL(elements);

    OSSerializer * debugStateSerializer = OSSerializer::forTarget(this, OSMemberFunctionCast(OSSerializerCallback, this, &IOHIDEventDriver::serializeDebugState));
    if (debugStateSerializer) {
        setProperty("DebugState", debugStateSerializer);
        debugStateSerializer->release();
    }
  

    return result;
}

//====================================================================================================
// IOHIDEventDriver::getTransport
//====================================================================================================
OSString * IOHIDEventDriver::getTransport ()
{
    return _interface ? _interface->getTransport() : (OSString *)OSSymbol::withCString("unknown:") ;
}

//====================================================================================================
// IOHIDEventDriver::getManufacturer
//====================================================================================================
OSString * IOHIDEventDriver::getManufacturer ()
{
    return _interface ? _interface->getManufacturer() : (OSString *)OSSymbol::withCString("unknown:") ;
}

//====================================================================================================
// IOHIDEventDriver::getProduct
//====================================================================================================
OSString * IOHIDEventDriver::getProduct ()
{
    return _interface ? _interface->getProduct() : (OSString *)OSSymbol::withCString("unknown:") ;
}

//====================================================================================================
// IOHIDEventDriver::getSerialNumber
//====================================================================================================
OSString * IOHIDEventDriver::getSerialNumber ()
{
    return _interface ? _interface->getSerialNumber() : (OSString *)OSSymbol::withCString("unknown:") ;
}

//====================================================================================================
// IOHIDEventDriver::getLocationID
//====================================================================================================
UInt32 IOHIDEventDriver::getLocationID ()
{
    return _interface ? _interface->getLocationID() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getVendorID
//====================================================================================================
UInt32 IOHIDEventDriver::getVendorID ()
{
    return _interface ? _interface->getVendorID() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getVendorIDSource
//====================================================================================================
UInt32 IOHIDEventDriver::getVendorIDSource ()
{
    return _interface ? _interface->getVendorIDSource() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getProductID
//====================================================================================================
UInt32 IOHIDEventDriver::getProductID ()
{
    return _interface ? _interface->getProductID() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getVersion
//====================================================================================================
UInt32 IOHIDEventDriver::getVersion ()
{
    return _interface ? _interface->getVersion() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getCountryCode
//====================================================================================================
UInt32 IOHIDEventDriver::getCountryCode ()
{
    return _interface ? _interface->getCountryCode() : -1 ;
}


//====================================================================================================
// IOHIDEventDriver::handleStop
//====================================================================================================
void IOHIDEventDriver::handleStop(  IOService * provider __unused )
{
    //_interface->close(this);
}

//=============================================================================================
// IOHIDEventDriver::didTerminate
//=============================================================================================
bool IOHIDEventDriver::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    if (_interface)
        _interface->close(this);
    _interface = NULL;

    return super::didTerminate(provider, options, defer);
}

//====================================================================================================
// IOHIDEventDriver::parseElements
//====================================================================================================
bool IOHIDEventDriver::parseElements ( OSArray* elementArray, UInt32 bootProtocol)
{
    OSArray *   pendingElements         = NULL;
    OSArray *   pendingButtonElements   = NULL;
    bool        result                  = false;
    UInt32      count, index;

    if ( bootProtocol == kBootProtocolMouse )
        _bootSupport = kBootMouse;
    
    require_action(elementArray, exit, result = false);

    _supportedElements = elementArray;
    _supportedElements->retain();

    for ( index=0, count=elementArray->getCount(); index<count; index++ ) {
        IOHIDElement *  element     = NULL;

        element = OSDynamicCast(IOHIDElement, elementArray->getObject(index));
        if ( !element )
            continue;
        
        if ( element->getReportID() != 0 )
            _multipleReports = true;

        if ( element->getType() == kIOHIDElementTypeCollection )
            continue;
        
        if ( element->getUsage() == 0 )
            continue;

        if (    parseVendorMessageElement(element) ||
                parseDigitizerElement(element) ||
                parseGameControllerElement(element) ||
                parseMultiAxisElement(element) ||
                parseRelativeElement(element) ||
                parseScrollElement(element) ||
                parseLEDElement(element) ||
                parseKeyboardElement(element) ||
                parseUnicodeElement(element)
                ) {
            result = true;
            continue;
        }
        
        if (element->getUsagePage() == kHIDPage_Button) {
#if !TARGET_OS_EMBEDDED
            IOHIDElement *  parent = element;
            while ((parent = parent->getParentElement()) != NULL) {
                if (parent->getUsagePage() == kHIDPage_Consumer) {
                    break;
                }
            }
            if (parent != NULL) {
              continue;
            }
#endif
            if ( !pendingButtonElements ) {
                pendingButtonElements = OSArray::withCapacity(4);
                require_action(pendingButtonElements, exit, result = false);
            }
            pendingButtonElements->setObject(element);
            continue;
        }
        
        if ( !pendingElements ) {
            pendingElements = OSArray::withCapacity(4);
            require_action(pendingElements, exit, result = false);
        }
        
        pendingElements->setObject(element);
    }
    
    _digitizer.native = (_digitizer.transducers && _digitizer.transducers->getCount() != 0);
    
    if( pendingElements ) {
        for ( index=0, count=pendingElements->getCount(); index<count; index++ ) {
            IOHIDElement *  element     = NULL;
            
            element = OSDynamicCast(IOHIDElement, pendingElements->getObject(index));
            if ( !element )
                continue;
            
            if ( parseDigitizerTransducerElement(element) )
                result = true;
        }
    }
    
    if( pendingButtonElements ) {
        for ( index=0, count=pendingButtonElements->getCount(); index<count; index++ ) {
            IOHIDElement *  element = NULL;

            element = OSDynamicCast(IOHIDElement, pendingButtonElements->getObject(index));
            if ( !element )
                continue;
            
            if ( _relative.elements && _relative.elements->getCount() ){
                _relative.elements->setObject(element);
            } else if ( _gameController.capable ) {
                _gameController.elements->setObject(element);
            } else if ( _multiAxis.capable ) {
                _multiAxis.elements->setObject(element);
            } else if ( _digitizer.transducers && _digitizer.transducers->getCount() ) {
                DigitizerTransducer * transducer = OSDynamicCast(DigitizerTransducer, _digitizer.transducers->getObject(0));
                if ( transducer )
                    transducer->elements->setObject(element);
            } else if ( _relative.elements ) {
                _relative.elements->setObject(element);
            }
        }
    }
    
    processDigitizerElements();
    processGameControllerElements();
    processMultiAxisElements();
    processUnicodeElements();
    
    setRelativeProperties();
    setDigitizerProperties();
    setGameControllerProperties();
    setMultiAxisProperties();
    setScrollProperties();
    setLEDProperties();
    setKeyboardProperties();
    setUnicodeProperties();
    setAccelerationProperties();
    setVendorMessageProperties();
    
exit:

    if ( pendingElements )
        pendingElements->release();
    
    if ( pendingButtonElements )
        pendingButtonElements->release();
    
    return result || _bootSupport;
}

//====================================================================================================
// IOHIDEventDriver::processDigitizerElements
//====================================================================================================
void IOHIDEventDriver::processDigitizerElements()
{
    OSArray *               newTransducers      = NULL;
    OSArray *               orphanedElements    = NULL;
    DigitizerTransducer *   rootTransducer      = NULL;
    UInt32                  index, count;
    
    require(_digitizer.transducers, exit);
    
    newTransducers = OSArray::withCapacity(4);
    require(newTransducers, exit);
    
    orphanedElements = OSArray::withCapacity(4);
    require(orphanedElements, exit);
    
    // RY: Let's check for transducer validity. If there isn't an X axis, odds are
    // this transducer was created due to a malformed descriptor.  In this case,
    // let's collect the orphansed elements and insert them into the root transducer.
    for (index=0, count=_digitizer.transducers->getCount(); index<count; index++) {
        OSArray *               pendingElements = NULL;
        DigitizerTransducer *   transducer;
        bool                    valid = false;
        UInt32                  elementIndex, elementCount;
        
        transducer = OSDynamicCast(DigitizerTransducer, _digitizer.transducers->getObject(index));
        if ( !transducer )
            continue;
        
        if ( !transducer->elements )
            continue;
        
        pendingElements = OSArray::withCapacity(4);
        if ( !pendingElements )
            continue;
        
        for (elementIndex=0, elementCount=transducer->elements->getCount(); elementIndex<elementCount; elementIndex++) {
            IOHIDElement * element = OSDynamicCast(IOHIDElement, transducer->elements->getObject(elementIndex));
            
            if ( !element )
                continue;
            
            if ( element->getUsagePage()==kHIDPage_GenericDesktop && element->getUsage()==kHIDUsage_GD_X )
                valid = true;
            
            pendingElements->setObject(element);
        }
        
        if ( valid ) {
            newTransducers->setObject(transducer);
            
            if ( !rootTransducer )
                rootTransducer = transducer;
        } else {
            orphanedElements->merge(pendingElements);
        }
        
        if ( pendingElements )
            pendingElements->release();
    }
    
    OSSafeReleaseNULL(_digitizer.transducers);
    
    require(newTransducers->getCount(), exit);
    
    _digitizer.transducers = newTransducers;
    _digitizer.transducers->retain();

    if ( rootTransducer ) {
        for (index=0, count=orphanedElements->getCount(); index<count; index++) {
            IOHIDElement * element = OSDynamicCast(IOHIDElement, orphanedElements->getObject(index));
            
            if ( !element )
                continue;
            
            rootTransducer->elements->setObject(element);
        }
    }
    
    if ( _digitizer.deviceModeElement ) {
        _digitizer.deviceModeElement->setValue(1);
        _relative.disabled  = true;
        _multiAxis.disabled = true;
    }

    if (!conformTo(kHIDPage_AppleVendor, kHIDUsage_AppleVendor_DFR)) {
        setProperty("SupportsInk", 1, 32);
    }
exit:
    if ( orphanedElements )
        orphanedElements->release();
    
    if ( newTransducers )
        newTransducers->release();
    
    return;
}

//====================================================================================================
// IOHIDEventDriver::processGameControllerElements
//====================================================================================================
#define GAME_CONTROLLER_STANDARD_MASK 0x00000F3F
#define GAME_CONTROLLER_EXTENDED_MASK (0x000270C0 | GAME_CONTROLLER_STANDARD_MASK)
#define GAME_CONTROLLER_FORM_FITTING_MASK (0x1000000)

void IOHIDEventDriver::processGameControllerElements()
{
    UInt32 index, count;
    
    require(_gameController.elements, exit);
    
    _gameController.extended = (_gameController.capable & GAME_CONTROLLER_EXTENDED_MASK) == GAME_CONTROLLER_EXTENDED_MASK;
    _gameController.formFitting = (_gameController.capable & GAME_CONTROLLER_FORM_FITTING_MASK) == GAME_CONTROLLER_FORM_FITTING_MASK;
    
    for (index=0, count=_gameController.elements->getCount(); index<count; index++) {
        IOHIDElement *  element     = OSDynamicCast(IOHIDElement, _gameController.elements->getObject(index));
        UInt32          reportID    = 0;
        
        if ( !element )
            continue;
        
        if (element->getUsagePage() == kHIDPage_LEDs) {
            if (element->getUsage() == kHIDUsage_MFiGameController_LED0)
                element->setValue(1);
            continue;
        }
        
        reportID = element->getReportID();
 
        if ( reportID > _gameController.sendingReportID )
            _gameController.sendingReportID = reportID;
    }
    
exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::processMultiAxisElements
//====================================================================================================
void IOHIDEventDriver::processMultiAxisElements()
{
    UInt32 translationMask, rotationMask, index, count;
    
    require(_multiAxis.elements, exit);
    
    translationMask = (1<<GET_AXIS_INDEX(kHIDUsage_GD_X)) | (1<<GET_AXIS_INDEX(kHIDUsage_GD_Y));
    rotationMask    = (1<<GET_AXIS_INDEX(kHIDUsage_GD_Rx)) | (1<<GET_AXIS_INDEX(kHIDUsage_GD_Ry));
    
    for (index=0, count=_multiAxis.elements->getCount(); index<count; index++) {
        IOHIDElement *  element     = OSDynamicCast(IOHIDElement, _multiAxis.elements->getObject(index));
        UInt32          reportID    = 0;
        
        if ( !element )
            continue;
        
        reportID = element->getReportID();
        
        switch ( element->getUsagePage() ) {
            case kHIDPage_GenericDesktop:
                switch ( element->getUsage() ) {
                    case kHIDUsage_GD_Z:
                        if ( (_multiAxis.capable & rotationMask) == 0) {
                            _multiAxis.options |= kMultiAxisOptionZForScroll;
                            if ( reportID > _multiAxis.sendingReportID )
                                _multiAxis.sendingReportID = reportID;
                        }
                        break;
                    case kHIDUsage_GD_Rx:
                    case kHIDUsage_GD_Ry:
                        if ( _multiAxis.capable & translationMask ) {
                            _multiAxis.options |= kMultiAxisOptionRotationForTranslation;
                            if ( reportID > _multiAxis.sendingReportID )
                                _multiAxis.sendingReportID = reportID;
                        }
                        break;
                    case kHIDUsage_GD_Rz:
                        {
                            SInt32 removal = _preferredAxisRemovalPercentage;
                            
                            if ( (_multiAxis.capable & rotationMask) != 0) {
                                removal *= 2;
                            }
                            calibrateCenteredPreferredStateElement(element, removal);
                        }
                        break;
                }
                break;
        }
    }
    
exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::processUnicodeElements
//====================================================================================================
void IOHIDEventDriver::processUnicodeElements()
{
    
}


//====================================================================================================
// IOHIDEventDriver::setRelativeProperties
//====================================================================================================
void IOHIDEventDriver::setRelativeProperties()
{
    OSDictionary * properties = OSDictionary::withCapacity(4);
    
    require(properties, exit);
    require(_relative.elements, exit);

    properties->setObject(kIOHIDElementKey, _relative.elements);
    
    setProperty("RelativePointer", properties);

exit:
    OSSafeReleaseNULL(properties);
}

//====================================================================================================
// IOHIDEventDriver::setDigitizerProperties
//====================================================================================================
void IOHIDEventDriver::setDigitizerProperties()
{
    OSDictionary * properties = OSDictionary::withCapacity(4);
    
    require(properties, exit);
    require(_digitizer.transducers, exit);

#if TARGET_OS_TV
    _digitizer.collectionDispatch = true;
#else
    if (conformTo (kHIDPage_AppleVendor, kHIDUsage_AppleVendor_DFR)) {
        _digitizer.collectionDispatch = true;
    }
#endif
  
    properties->setObject("touchCancelElement", _digitizer.touchCancelElement);
    properties->setObject("Transducers", _digitizer.transducers);
    properties->setObject("DeviceModeElement", _digitizer.deviceModeElement);
    properties->setObject("collectionDispatch", _digitizer.collectionDispatch ? kOSBooleanTrue : kOSBooleanFalse);
  
    setProperty("Digitizer", properties);
    
exit:
    OSSafeReleaseNULL(properties);
}

//====================================================================================================
// IOHIDEventDriver::setGameControllerProperties
//====================================================================================================
void IOHIDEventDriver::setGameControllerProperties()
{
    OSDictionary *  properties  = OSDictionary::withCapacity(4);
    OSNumber *      number      = NULL;
    
    require(properties, exit);
    require(_gameController.elements, exit);
    
    properties->setObject(kIOHIDElementKey, _gameController.elements);
    
    number = OSNumber::withNumber(_gameController.capable, 32);
    require(number, exit);

    properties->setObject("GameControllerCapabilities", number);
    OSSafeReleaseNULL(number);
    
    setProperty("GameControllerPointer", properties);
    
    number = OSNumber::withNumber(_gameController.extended ? kIOHIDGameControllerTypeExtended : kIOHIDGameControllerTypeStandard, 32);
    require(number, exit);

    setProperty(kIOHIDGameControllerTypeKey, number);
    OSSafeReleaseNULL(number);
    
    if (_gameController.formFitting) {
        setProperty(kIOHIDGameControllerFormFittingKey, kOSBooleanTrue);
    }
    
exit:
    OSSafeReleaseNULL(properties);
}

//====================================================================================================
// IOHIDEventDriver::setMultiAxisProperties
//====================================================================================================
void IOHIDEventDriver::setMultiAxisProperties()
{
    OSDictionary *  properties  = OSDictionary::withCapacity(4);
    OSNumber *      number      = NULL;
    
    require(properties, exit);
    require(_multiAxis.elements, exit);
    
    properties->setObject(kIOHIDElementKey, _multiAxis.elements);
    
    number = OSNumber::withNumber(_multiAxis.capable, 32);
    require(number, exit);
    
    properties->setObject("AxisCapabilities", number);
    
    setProperty("MultiAxisPointer", properties);
    
exit:
    OSSafeReleaseNULL(number);
    OSSafeReleaseNULL(properties);
}

//====================================================================================================
// IOHIDEventDriver::setScrollProperties
//====================================================================================================
void IOHIDEventDriver::setScrollProperties()
{
    OSDictionary * properties = OSDictionary::withCapacity(4);
    
    require(properties, exit);
    require(_scroll.elements, exit);
    
    properties->setObject(kIOHIDElementKey, _scroll.elements);
    
    setProperty("Scroll", properties);
    
exit:
    OSSafeReleaseNULL(properties);
}

//====================================================================================================
// IOHIDEventDriver::setLEDProperties
//====================================================================================================
void IOHIDEventDriver::setLEDProperties()
{
    OSDictionary * properties = OSDictionary::withCapacity(4);
    
    require(properties, exit);
    require(_led.elements, exit);
    
    properties->setObject(kIOHIDElementKey, _led.elements);
    
    setProperty("LED", properties);
    
exit:
    OSSafeReleaseNULL(properties);
}

//====================================================================================================
// IOHIDEventDriver::setKeyboardProperties
//====================================================================================================
void IOHIDEventDriver::setKeyboardProperties()
{
    OSDictionary * properties = OSDictionary::withCapacity(4);
    
    require(properties, exit);
    require(_keyboard.elements, exit);
    
    properties->setObject(kIOHIDElementKey, _keyboard.elements);
    
    setProperty("Keyboard", properties);
    
exit:
    OSSafeReleaseNULL(properties);
}

//====================================================================================================
// IOHIDEventDriver::setUnicodeProperties
//====================================================================================================
void IOHIDEventDriver::setUnicodeProperties()
{
    OSDictionary * properties = NULL;
    OSSerializer * serializer = NULL;
    
    require(_unicode.legacyElements || _unicode.gesturesCandidates, exit);
    
    properties = OSDictionary::withCapacity(4);
    require(properties, exit);

    if ( _unicode.legacyElements ) {
        OSNumber * number = OSNumber::withNumber(_unicode.legacyElements->getCount(), 32);
        properties->setObject("Legacy", number);
        number->release();
    }
    
    if ( _unicode.gesturesCandidates )
        properties->setObject("Gesture", _unicode.gesturesCandidates);
    
    
    if ( _unicode.gestureStateElement ) {
        properties->setObject("GestureCharacterStateElement", _unicode.gestureStateElement);
        serializer = OSSerializer::forTarget(this, OSMemberFunctionCast(OSSerializerCallback, this, &IOHIDEventDriver::serializeCharacterGestureState));
        require(serializer, exit);
        setProperty(kIOHIDDigitizerGestureCharacterStateKey, serializer);
    }
    
    setProperty("Unicode", properties);
    
exit:
    OSSafeReleaseNULL(serializer);
    OSSafeReleaseNULL(properties);
}

//====================================================================================================
// IOHIDEventDriver::setVendorMessageProperties
//====================================================================================================
void IOHIDEventDriver::setVendorMessageProperties()
{
    OSDictionary * properties = OSDictionary::withCapacity(4);
    
    require(properties, exit);
    require(_vendorMessage.elements, exit);

    properties->setObject(kIOHIDElementKey, _vendorMessage.elements);
    
    setProperty("VendorMessage", properties );

exit:
    OSSafeReleaseNULL(properties);
}

//====================================================================================================
// IOHIDEventDriver::conformTo
//====================================================================================================
bool  IOHIDEventDriver::conformTo (UInt32 usagePage, UInt32 usage) {
    bool result = false;
    OSArray     *deviceUsagePairs   = getDeviceUsagePairs();
    if (deviceUsagePairs && deviceUsagePairs->getCount()) {
        for (UInt32 index=0; index < deviceUsagePairs->getCount(); index++) {
            OSDictionary *pair = OSDynamicCast(OSDictionary, deviceUsagePairs->getObject(index));
            if (pair) {
                OSNumber *number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsagePageKey));
                if (number) {
                    if (usagePage !=  number->unsigned32BitValue()) {
                        continue;
                    }
                }
                number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsageKey));
                if (number) {
                    if (usage == number->unsigned32BitValue()) {
                        result = true;
                        break;
                    }
                }
            }
        }
    }
    return result;
}


//====================================================================================================
// IOHIDEventDriver::setAccelerationProperties
//====================================================================================================
void IOHIDEventDriver::setAccelerationProperties()
{
#if !TARGET_OS_EMBEDDED
    bool        pointer             = false;
    OSArray     *deviceUsagePairs   = getDeviceUsagePairs();
    
    if (deviceUsagePairs && deviceUsagePairs->getCount()) {
        for (UInt32 index=0; index < deviceUsagePairs->getCount(); index++) {
            OSDictionary *pair = OSDynamicCast(OSDictionary, deviceUsagePairs->getObject(index));
            
            if (pair) {
                OSNumber *number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsagePageKey));
                if (number) {
                    UInt32 usagePage = number->unsigned32BitValue();
                    if (usagePage != kHIDPage_GenericDesktop) {
                        continue;
                    }
                }
                
                number = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsageKey));
                if (number) {
                    UInt32 usage = number->unsigned32BitValue();
                    if (usage == kHIDUsage_GD_Mouse) {
                        if (!getProperty(kIOHIDPointerAccelerationTypeKey))
                            setProperty(kIOHIDPointerAccelerationTypeKey, kIOHIDMouseAccelerationType);
                        
                        if (_scroll.elements) {
                            if (!getProperty(kIOHIDScrollAccelerationTypeKey))
                                setProperty(kIOHIDScrollAccelerationTypeKey, kIOHIDMouseScrollAccelerationKey);
                        }
                        
                        return;
                    } else if (usage == kHIDUsage_GD_Pointer) {
                        pointer = true;
                    }
                }
            }
        }
        
        // this is a pointer only device
        if (pointer) {
            if (!getProperty(kIOHIDPointerAccelerationTypeKey))
                setProperty(kIOHIDPointerAccelerationTypeKey, kIOHIDPointerAccelerationKey);
            
            if (_scroll.elements) {
                if (!getProperty(kIOHIDScrollAccelerationTypeKey))
                    setProperty(kIOHIDScrollAccelerationTypeKey, kIOHIDScrollAccelerationKey);
            }
        }
    }
#endif /* TARGET_OS_EMBEDDED */
}

//====================================================================================================
// IOHIDEventDriver::serializeCharacterGestureState
//====================================================================================================
bool IOHIDEventDriver::serializeCharacterGestureState(void * , OSSerialize * serializer)
{
    OSNumber *  number  = NULL;
    UInt32      value   = 0;
    bool        result  = false;
    
    require(_unicode.gestureStateElement, exit);
    
    value = _unicode.gestureStateElement->getValue();
    
    number = OSNumber::withNumber(value, 32);
    require(number, exit);
    
    result = number->serialize(serializer);
    
exit:
    OSSafeReleaseNULL(number);
    return result;
}

//====================================================================================================
// IOHIDEventDriver::setProperties
//====================================================================================================
IOReturn IOHIDEventDriver::setProperties( OSObject * properties )
{
    IOReturn        result          = kIOReturnUnsupported;
    OSDictionary *  propertyDict    = OSDynamicCast(OSDictionary, properties);
    OSBoolean *     boolVal         = NULL;
    
    require(propertyDict, exit);
    
    if ( (boolVal = OSDynamicCast(OSBoolean, propertyDict->getObject(kIOHIDDigitizerGestureCharacterStateKey)))) {
        require(_unicode.gestureStateElement, exit);
        
        _unicode.gestureStateElement->setValue(boolVal==kOSBooleanTrue ? 1 : 0);
        result = kIOReturnSuccess;
    }
    
    
exit:
    if ( result != kIOReturnSuccess )
        result = super::setProperties(properties);
    
    return result;
}


//====================================================================================================
// IOHIDEventDriver::parseDigitizerElement
//====================================================================================================
bool IOHIDEventDriver::parseDigitizerElement(IOHIDElement * element)
{
    IOHIDElement *          parent          = NULL;
    bool                    result          = false;

    parent = element;
    while ( (parent = parent->getParentElement()) ) {
        bool application = false;
        switch ( parent->getCollectionType() ) {
            case kIOHIDElementCollectionTypeLogical:
            case kIOHIDElementCollectionTypePhysical:
                break;
            case kIOHIDElementCollectionTypeApplication:
                application = true;
                break;
            default:
                continue;
        }
        
        if ( parent->getUsagePage() != kHIDPage_Digitizer )
            continue;

        if ( application ) {
            if ( parent->getUsage() < kHIDUsage_Dig_Digitizer )
                continue;
            
            if ( parent->getUsage() > kHIDUsage_Dig_DeviceConfiguration )
                continue;
        }
        else {
            if ( parent->getUsage() < kHIDUsage_Dig_Stylus )
                continue;
            
            if ( parent->getUsage() > kHIDUsage_Dig_GestureCharacter )
                continue;
        }

        break;
    }
    
    require_action(parent, exit, result=false);
  
    if (element->getUsagePage() == kHIDPage_AppleVendorMultitouch && element->getUsage() == kHIDUsage_AppleVendorMultitouch_TouchCancel) {
        OSSafeReleaseNULL(_digitizer.touchCancelElement);
        element->retain();
        _digitizer.touchCancelElement = element;
    }

    switch ( parent->getUsage() ) {
        case kHIDUsage_Dig_DeviceSettings:
            if  ( element->getUsagePage() == kHIDPage_Digitizer && element->getUsage() == kHIDUsage_Dig_DeviceMode ) {
                _digitizer.deviceModeElement = element;
                _digitizer.deviceModeElement->retain();
                result = true;
            }
            goto exit;
            break;
        case kHIDUsage_Dig_GestureCharacter:
            result = parseUnicodeElement(element);
            goto exit;
            break;
        default:
            break;
    }
    
    result = parseDigitizerTransducerElement(element, parent);

exit:
    return result;
}

//====================================================================================================
// IOHIDEventDriver::parseDigitizerTransducerElement
//====================================================================================================
bool IOHIDEventDriver::parseDigitizerTransducerElement(IOHIDElement * element, IOHIDElement * parent)
{
    DigitizerTransducer *   transducer      = NULL;
    bool                    shouldCalibrate = false;
    bool                    result          = false;
    UInt32                  index, count;
    
    switch ( element->getUsagePage() ) {
        case kHIDPage_GenericDesktop:
            switch ( element->getUsage() ) {
                case kHIDUsage_GD_X:
                case kHIDUsage_GD_Y:
                case kHIDUsage_GD_Z:
                    require_action_quiet((element->getFlags() & kIOHIDElementFlagsRelativeMask) == 0, exit, result=false);
                    shouldCalibrate = true;
                    break;
            }
            break;
    }
    
    require_action(GetReportType(element->getType()) == kIOHIDReportTypeInput, exit, result=false);
    
    // If we are coming in through non digitizer origins, only allow this if we don't already have digitizer support
    if ( !parent ) {
        require_action(!_digitizer.native, exit, result=false);
    }

    if ( !_digitizer.transducers ) {
        _digitizer.transducers = OSArray::withCapacity(4);
        require_action(_digitizer.transducers, exit, result=false);
    }
    
    // go through exising transducers
    for ( index=0,count=_digitizer.transducers->getCount(); index<count; index++) {
        DigitizerTransducer * tempTransducer;
        
        tempTransducer = OSDynamicCast(DigitizerTransducer,_digitizer.transducers->getObject(index));
        if ( !tempTransducer )
            continue;
        
        if ( tempTransducer->collection != parent )
            continue;
        
        transducer = tempTransducer;
        break;
    }
    
    // no match, let's create one
    if ( !transducer ) {
        uint32_t type = kDigitizerTransducerTypeStylus;
        
        if ( parent ) {
            switch ( parent->getUsage() )  {
                case kHIDUsage_Dig_Puck:
                    type = kDigitizerTransducerTypePuck;
                    break;
                case kHIDUsage_Dig_Finger:
                case kHIDUsage_Dig_TouchScreen:
                case kHIDUsage_Dig_TouchPad:
                    type = kDigitizerTransducerTypeFinger;
                    break;
                default:
                    break;
            }
        }
        
        transducer = DigitizerTransducer::transducer(type, parent);
        require_action(transducer, exit, result=false);
        
        _digitizer.transducers->setObject(transducer);
        transducer->release();
    }
    
    if ( shouldCalibrate )
        calibrateJustifiedPreferredStateElement(element, _absoluteAxisRemovalPercentage);
  
    transducer->elements->setObject(element);
    result = true;
    
exit:
    return result;
}

//====================================================================================================
// IOHIDEventDriver::parseGameControllerElement
//====================================================================================================
bool IOHIDEventDriver::parseGameControllerElement(IOHIDElement * element)
{
    UInt32 usagePage    = element->getUsagePage();
    bool   store        = false;
    bool   ret          = false;
    
    require(_authenticatedDevice, exit);
    
    if ( !_gameController.elements ) {
        _gameController.elements = OSArray::withCapacity(4);
        require(_gameController.elements, exit);
    }
    
    switch ( usagePage ) {
        case kHIDPage_GenericDesktop:
        case kHIDPage_Button:
        case kHIDPage_Game:
            _gameController.capable |= checkGameControllerElement(element);
            if ( !_gameController.capable )
                break;
            
            ret = store = true;
            break;
 
	case kHIDPage_LEDs:
            store = true;
            break;
    }
    
    require(store, exit);
    
    _gameController.elements->setObject(element);
    
exit:
    return ret;
}

//====================================================================================================
// IOHIDEventDriver::parseMultiAxisElement
//====================================================================================================
bool IOHIDEventDriver::parseMultiAxisElement(IOHIDElement * element)
{
    UInt32 usagePage    = element->getUsagePage();
    UInt32 usage        = element->getUsage();
    bool   store        = false;
    
    if ( !_multiAxis.elements ) {
        _multiAxis.elements = OSArray::withCapacity(4);
        require(_multiAxis.elements, exit);
    }
    
    switch ( usagePage ) {
        case kHIDPage_GenericDesktop:
            switch ( usage ) {
                case kHIDUsage_GD_X:
                case kHIDUsage_GD_Y:
                case kHIDUsage_GD_Z:
                case kHIDUsage_GD_Rx:
                case kHIDUsage_GD_Ry:
                case kHIDUsage_GD_Rz:
                    _multiAxis.capable |= checkMultiAxisElement(element);
                    if ( !_multiAxis.capable )
                        break;

                    calibrateCenteredPreferredStateElement(element, _preferredAxisRemovalPercentage);
                    store = true;
                    break;
            }
            break;
    }
    
    require(store, exit);
    
    _multiAxis.elements->setObject(element);

exit:
    return store;
}

//====================================================================================================
// IOHIDEventDriver::parseRelativeElement
//====================================================================================================
bool IOHIDEventDriver::parseRelativeElement(IOHIDElement * element)
{
    UInt32 usagePage    = element->getUsagePage();
    UInt32 usage        = element->getUsage();
    bool   store        = false;
    
    if ( !_relative.elements ) {
        _relative.elements = OSArray::withCapacity(4);
        require(_relative.elements, exit);
    }

    switch ( usagePage ) {
        case kHIDPage_GenericDesktop:
            switch ( usage ) {
                case kHIDUsage_GD_X:
                case kHIDUsage_GD_Y:
                    if ((element->getFlags() & kIOHIDElementFlagsRelativeMask) == 0)
                        break;
                    _bootSupport &= ~kMouseXYAxis;
                    store = true;
                    break;
            }
            break;
    }
    
    require(store, exit);
    
    _relative.elements->setObject(element);

exit:
    return store;
}

//====================================================================================================
// IOHIDEventDriver::parseScrollElement
//====================================================================================================
bool IOHIDEventDriver::parseScrollElement(IOHIDElement * element)
{
    UInt32 usagePage    = element->getUsagePage();
    UInt32 usage        = element->getUsage();
    bool   store        = false;
    
    if ( !_scroll.elements ) {
        _scroll.elements = OSArray::withCapacity(4);
        require(_scroll.elements, exit);
    }

    switch ( usagePage ) {
        case kHIDPage_GenericDesktop:
            switch ( usage ) {
                case kHIDUsage_GD_Dial:
                case kHIDUsage_GD_Wheel:
                case kHIDUsage_GD_Z:
                    
                    if ((element->getFlags() & (kIOHIDElementFlagsNoPreferredMask|kIOHIDElementFlagsRelativeMask)) == 0) {
                        calibrateCenteredPreferredStateElement(element, _preferredAxisRemovalPercentage);
                    }
                    
                    store = true;
                    break;
            }
            break;
        case kHIDPage_Consumer:
            switch ( usage ) {
                case kHIDUsage_Csmr_ACPan:
                    store = true;
                    break;
            }
            break;
    }
    
    require(store, exit);
    
    _scroll.elements->setObject(element);

exit:
    return store;
}

//====================================================================================================
// IOHIDEventDriver::parseLEDElement
//====================================================================================================
bool IOHIDEventDriver::parseLEDElement(IOHIDElement * element)
{
    UInt32 usagePage    = element->getUsagePage();
    bool   store        = false;
    
    if ( !_led.elements ) {
        _led.elements = OSArray::withCapacity(4);
        require(_led.elements, exit);
    }
    
    switch ( usagePage ) {
        case kHIDPage_LEDs:
            store = true;
            break;
    }
    
    require(store, exit);
    
    _led.elements->setObject(element);
    
exit:
    return store;
}


//====================================================================================================
// IOHIDEventDriver::parseKeyboardElement
//====================================================================================================
bool IOHIDEventDriver::parseKeyboardElement(IOHIDElement * element)
{
    UInt32 usagePage    = element->getUsagePage();
    UInt32 usage        = element->getUsage();
    bool   store        = false;
  
    if ( !_keyboard.elements ) {
        _keyboard.elements = OSArray::withCapacity(4);
        require(_keyboard.elements, exit);
    }

    switch ( usagePage ) {
        case kHIDPage_GenericDesktop:
            switch ( usage ) {
                case kHIDUsage_GD_Start:
                case kHIDUsage_GD_Select:
                case kHIDUsage_GD_SystemPowerDown:
                case kHIDUsage_GD_SystemSleep:
                case kHIDUsage_GD_SystemWakeUp:
                case kHIDUsage_GD_SystemContextMenu:
                case kHIDUsage_GD_SystemMainMenu:
                case kHIDUsage_GD_SystemAppMenu:
                case kHIDUsage_GD_SystemMenuHelp:
                case kHIDUsage_GD_SystemMenuExit:
                case kHIDUsage_GD_SystemMenuSelect:
                case kHIDUsage_GD_SystemMenuRight:
                case kHIDUsage_GD_SystemMenuLeft:
                case kHIDUsage_GD_SystemMenuUp:
                case kHIDUsage_GD_SystemMenuDown:
                case kHIDUsage_GD_DPadUp:
                case kHIDUsage_GD_DPadDown:
                case kHIDUsage_GD_DPadRight:
                case kHIDUsage_GD_DPadLeft:
                    store = true;
                    break;
            }
            break;
        case kHIDPage_KeyboardOrKeypad:
            if (( usage < kHIDUsage_KeyboardA ) || ( usage > kHIDUsage_KeyboardRightGUI ))
                break;
        case kHIDPage_Consumer:
            if (usage == kHIDUsage_Csmr_ACKeyboardLayoutSelect)
                setProperty(kIOHIDSupportsGlobeKeyKey, kOSBooleanTrue);
        case kHIDPage_Telephony:
            store = true;
            break;
        case kHIDPage_AppleVendorTopCase:
            if (_keyboard.appleVendorSupported) {
                switch (usage) {
                    case kHIDUsage_AV_TopCase_BrightnessDown:
                    case kHIDUsage_AV_TopCase_BrightnessUp:
                    case kHIDUsage_AV_TopCase_IlluminationDown:
                    case kHIDUsage_AV_TopCase_IlluminationUp:
                    case kHIDUsage_AV_TopCase_KeyboardFn:
                    store = true;
                    break;
                }
            }
            break;
        case kHIDPage_AppleVendorKeyboard:
            if (_keyboard.appleVendorSupported) {
                switch (usage) {
                    case kHIDUsage_AppleVendorKeyboard_Spotlight:
                    case kHIDUsage_AppleVendorKeyboard_Dashboard:
                    case kHIDUsage_AppleVendorKeyboard_Function:
                    case kHIDUsage_AppleVendorKeyboard_Launchpad:
                    case kHIDUsage_AppleVendorKeyboard_Reserved:
                    case kHIDUsage_AppleVendorKeyboard_CapsLockDelayEnable:
                    case kHIDUsage_AppleVendorKeyboard_PowerState:
                    case kHIDUsage_AppleVendorKeyboard_Expose_All:
                    case kHIDUsage_AppleVendorKeyboard_Expose_Desktop:
                    case kHIDUsage_AppleVendorKeyboard_Brightness_Up:
                    case kHIDUsage_AppleVendorKeyboard_Brightness_Down:
                    case kHIDUsage_AppleVendorKeyboard_Language:
                    store = true;
                    break;
                }
            }
            break;
    }
    
    require(store, exit);
    
    _keyboard.elements->setObject(element);

exit:
    return store;
}

//====================================================================================================
// IOHIDEventDriver::parseUnicodeElement
//====================================================================================================
#if TARGET_OS_EMBEDDED
bool IOHIDEventDriver::parseUnicodeElement(IOHIDElement * element)
{
    bool store = false;

    store = parseLegacyUnicodeElement(element);
    require(!store, exit);
    
    store = parseGestureUnicodeElement(element);

exit:
    return store;
}

//====================================================================================================
// IOHIDEventDriver::parseLegacyUnicodeElement
//====================================================================================================
bool IOHIDEventDriver::parseLegacyUnicodeElement(IOHIDElement * element)
{
    UInt32 usagePage    = element->getUsagePage();
    bool   store        = false;
    
    if ( !_unicode.legacyElements ) {
        _unicode.legacyElements = OSArray::withCapacity(4);
        require(_unicode.legacyElements, exit);
    }
    
    require(usagePage==kHIDPage_Unicode, exit);
    
    _unicode.legacyElements->setObject(element);
    store = true;
    
exit:
    return store;
}

//====================================================================================================
// IOHIDEventDriver::parseGestureUnicodeElement
//====================================================================================================
bool IOHIDEventDriver::parseGestureUnicodeElement(IOHIDElement * element)
{
    IOHIDElement *              parent      = NULL;
    EventElementCollection *    candidate   = NULL;
    UInt32                      usagePage   = element->getUsagePage();
    UInt32                      usage       = element->getUsage();
    bool                        result      = false;
    UInt32                      index, count;
    
    switch ( usagePage ) {
        case kHIDPage_Digitizer:
            switch ( usage ) {
                case kHIDUsage_Dig_GestureCharacterQuality:
                    calibrateJustifiedPreferredStateElement(element, 0);
                case kHIDUsage_Dig_GestureCharacterData:
                case kHIDUsage_Dig_GestureCharacterDataLength:
                case kHIDUsage_Dig_GestureCharacterEncodingUTF8:
                case kHIDUsage_Dig_GestureCharacterEncodingUTF16LE:
                case kHIDUsage_Dig_GestureCharacterEncodingUTF16BE:
                    result = true;
                    break;
                case kHIDUsage_Dig_GestureCharacterEnable:
                    if ( element->getType() == kIOHIDElementTypeFeature ) {
                        _unicode.gestureStateElement = element;
                        _unicode.gestureStateElement->retain();
                        result = true;
                        goto exit;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    
    require(result, exit);

    parent = element;
    while ( (parent = parent->getParentElement()) ) {
        switch ( parent->getCollectionType() ) {
            case kIOHIDElementCollectionTypeLogical:
            case kIOHIDElementCollectionTypePhysical:
                break;
            default:
                continue;
        }
        
        if ( parent->getUsagePage() != kHIDPage_Digitizer )
            continue;

        if ( parent->getUsage() != kHIDUsage_Dig_GestureCharacter)
            continue;
        
        break;
    }
    
    require_action(parent, exit, result=false);
    
    require_action(GetReportType(element->getType()) == kIOHIDReportTypeInput, exit, result=false);
    
    if ( !_unicode.gesturesCandidates ) {
        _unicode.gesturesCandidates = OSArray::withCapacity(4);
        require_action(_unicode.gesturesCandidates, exit, result=false);
    }
    
    // go through exising transducers
    for ( index=0,count=_unicode.gesturesCandidates->getCount(); index<count; index++) {
        EventElementCollection * tempCandidate;
        
        tempCandidate = OSDynamicCast(EventElementCollection,_unicode.gesturesCandidates->getObject(index));
        if ( !tempCandidate )
            continue;
        
        if ( tempCandidate->collection != parent )
            continue;
        
        candidate = tempCandidate;
        break;
    }
    
    // no match, let's create one
    if ( !candidate ) {
        candidate = EventElementCollection::candidate(parent);
        require_action(candidate, exit, result=false);
        
        _unicode.gesturesCandidates->setObject(candidate);
        candidate->release();
    }
    
    candidate->elements->setObject(element);
    result = true;
    
exit:
    return result;
}

#else
bool IOHIDEventDriver::parseUnicodeElement(IOHIDElement * )
{
    return false;
}
bool IOHIDEventDriver::parseLegacyUnicodeElement(IOHIDElement * element)
{
    return false;
}
bool IOHIDEventDriver::parseGestureUnicodeElement(IOHIDElement * element)
{
    return false;
}
#endif


//====================================================================================================
// IOHIDEventDriver::parseVendorMessageElement
//====================================================================================================
bool IOHIDEventDriver::parseVendorMessageElement(IOHIDElement * element)
{
    IOHIDElement *          parent          = NULL;
    bool                    result          = false;
  
    parent = element->getParentElement();
  
    if (parent &&
        (parent->getCollectionType() == kIOHIDElementCollectionTypeApplication ||
         parent->getCollectionType() == kIOHIDElementCollectionTypePhysical) &&
        parent->getUsagePage() == kHIDPage_AppleVendor &&
        parent->getUsage() == kHIDUsage_AppleVendor_Message) {
        if (!_vendorMessage.elements) {
            _vendorMessage.elements = OSArray::withCapacity(1);
            require(_vendorMessage.elements, exit);
        }
        _vendorMessage.elements->setObject(element);
         result = true;
    }
exit:
    return result;
}



//====================================================================================================
// IOHIDEventDriver::checkGameControllerElement
//====================================================================================================
UInt32 IOHIDEventDriver::checkGameControllerElement(IOHIDElement * element)
{
    UInt32 result   = 0;
    UInt32 base     = 0;
    UInt32 offset   = 0;
    UInt32 usagePage= element->getUsagePage();
    UInt32 usage    = element->getUsage();
    bool   preferred=false;

    require(!element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse), exit);
    require(!element->conformsTo(kHIDPage_Digitizer), exit);
    require(element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad) ||
            element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick), exit);
    require((element->getFlags() & (kIOHIDElementFlagsNoPreferredMask|kIOHIDElementFlagsRelativeMask)) == 0, exit);

    switch (usagePage) {
        case kHIDPage_GenericDesktop:
            require((element->getFlags() & kIOHIDElementFlagsVariableMask) != 0, exit);
            
            switch (usage) {
                case kHIDUsage_GD_X:
                case kHIDUsage_GD_Y:
                case kHIDUsage_GD_Z:
                case kHIDUsage_GD_Rz:
                    offset = 12;
                    base = kHIDUsage_GD_X;
                    preferred = true;
                    break;
                    
                case kHIDUsage_GD_DPadUp:
                case kHIDUsage_GD_DPadDown:
                case kHIDUsage_GD_DPadLeft:
                case kHIDUsage_GD_DPadRight:
                    offset = 8;
                    base = kHIDUsage_GD_DPadUp;
                    break;
                default:
                    goto exit;
            }
            
            break;
            
        case kHIDPage_Button:
            require(usage >= 1 && usage <= 8, exit);
            
            base = kHIDUsage_Button_1;
            offset = 0;
            
            break;
            
        case kHIDPage_Game:
            if (usage == kHIDUsage_Game_GamepadFormFitting) {
                base = kHIDUsage_Game_GamepadFormFitting;
                offset = 24;
            }
            
            break;
            
        default:
            goto exit;
            break;
    };
    
    if ( preferred )
        calibrateCenteredPreferredStateElement(element, _preferredAxisRemovalPercentage);
    else
        calibrateJustifiedPreferredStateElement(element, _preferredAxisRemovalPercentage);
    
    result = (1<<((usage-base)+ offset));
    
exit:
    return result;
}

//====================================================================================================
// IOHIDEventDriver::checkMultiAxisElement
//====================================================================================================
UInt32 IOHIDEventDriver::checkMultiAxisElement(IOHIDElement * element)
{
    UInt32 result = 0;
    
    require((element->getFlags() & kIOHIDElementFlagsVariableMask) != 0, exit);
    require(!element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse), exit);
    require(!element->conformsTo(kHIDPage_Digitizer), exit);
    
    if ( ((element->getFlags() & (kIOHIDElementFlagsNoPreferredMask|kIOHIDElementFlagsRelativeMask)) == 0) ||
         element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_MultiAxisController) ||
         element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick) ) {
        result = (1<<(element->getUsage()-kHIDUsage_GD_X));
    }

exit:
    return result;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEventDriver::calibrateCenteredPreferredStateElement
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void IOHIDEventDriver::calibrateCenteredPreferredStateElement(IOHIDElement * element, SInt32 removalPercentage)
{
    UInt32 mid      = element->getLogicalMin() + ((element->getLogicalMax() - element->getLogicalMin()) / 2);
    UInt32 satMin   = element->getLogicalMin();
    UInt32 satMax   = element->getLogicalMax();
    UInt32 diff     = ((satMax - satMin) * removalPercentage) / 200;
    UInt32 dzMin    = mid - diff;
    UInt32 dzMax    = mid + diff;
    satMin          += diff;
    satMax          -= diff;

    element->setCalibration(-1, 1, satMin, satMax, dzMin, dzMax);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEventDriver::calibrateJustifiedPreferredStateElement
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void IOHIDEventDriver::calibrateJustifiedPreferredStateElement(IOHIDElement * element, SInt32 removalPercentage)
{
    UInt32 satMin   = element->getLogicalMin();
    UInt32 satMax   = element->getLogicalMax();
    UInt32 diff     = ((satMax - satMin) * removalPercentage) / 200;
    satMin          += diff;
    satMax          -= diff;

    element->setCalibration(0, 1, satMin, satMax);
}

//====================================================================================================
// IOHIDEventDriver::getReportElements
//====================================================================================================
OSArray * IOHIDEventDriver::getReportElements()
{
    return _supportedElements;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEventDriver::setButtonState
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static inline void setButtonState(UInt32 * state, UInt32 bit, UInt32 value)
{
    UInt32 buttonMask = (1 << bit);

    if ( value != 0 )
        (*state) |= buttonMask;
    else
        (*state) &= ~buttonMask;
}

//====================================================================================================
// IOHIDEventDriver::handleInterruptReport
//====================================================================================================
void IOHIDEventDriver::handleInterruptReport (
                                AbsoluteTime                timeStamp,
                                IOMemoryDescriptor *        report,
                                IOHIDReportType             reportType,
                                UInt32                      reportID)
{
    if (!readyForReports() || reportType!= kIOHIDReportTypeInput)
        return;
  
    _lastReportTime = timeStamp;
  
    IOHID_DEBUG(kIOHIDDebugCode_InturruptReport, reportType, reportID, getRegistryEntryID(), 0);
  
    handleVendorMessageReport(timeStamp, report, reportID, kIOParseVendorMessageReport);
  
    handleBootPointingReport(timeStamp, report, reportID);
    handleRelativeReport(timeStamp, reportID);
    handleGameControllerReport(timeStamp, reportID);
    handleMultiAxisPointerReport(timeStamp, reportID);
    handleDigitizerReport(timeStamp, reportID);
    handleScrollReport(timeStamp, reportID);
    handleKeboardReport(timeStamp, reportID);
    handleUnicodeReport(timeStamp, reportID);

    handleVendorMessageReport(timeStamp, report, reportID, kIODispatchVendorMessageReport);

}

//====================================================================================================
// IOHIDEventDriver::handleBootPointingReport
//====================================================================================================
void IOHIDEventDriver::handleBootPointingReport(AbsoluteTime timeStamp, IOMemoryDescriptor * report, UInt32 reportID)
{
    UInt32          bootOffset      = 0;
    IOByteCount     reportLength    = 0;
    UInt32          buttonState     = 0;
    SInt32          dX              = 0;
    SInt32          dY              = 0;

    require((_bootSupport & kBootMouse) == kBootMouse, exit);
    require(reportID==0, exit);

    // Get a pointer to the data in the descriptor.
    reportLength = report->getLength();
    require(reportLength >= 3, exit);

    report->readBytes( 0, (void *)_keyboard.bootMouseData, sizeof(_keyboard.bootMouseData) );
    
    if ( _multipleReports )
        bootOffset = 1;

    buttonState = _keyboard.bootMouseData[bootOffset];

    dX = _keyboard.bootMouseData[bootOffset + 1];

    dY = _keyboard.bootMouseData[bootOffset + 2];

    dispatchRelativePointerEvent(timeStamp, dX, dY, buttonState);

exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::handleGameControllerReport
//====================================================================================================
void IOHIDEventDriver::handleGameControllerReport(AbsoluteTime timeStamp, UInt32 reportID)
{
    bool        handled     = false;
    UInt32      index, count;
    
    
    require_quiet(_gameController.capable, exit);
    
    require_quiet(_gameController.elements, exit);
    
    for (index=0, count=_gameController.elements->getCount(); index<count; index++) {
        IOHIDElement *  element;
        IOFixed         elementFixedVal;
        IOFixed *       gcFixedVal = NULL;
        AbsoluteTime    elementTimeStamp;
        UInt32          usagePage, usage;
        bool            elementIsCurrent;
        
        element = OSDynamicCast(IOHIDElement, _gameController.elements->getObject(index));
        if ( !element )
            continue;
        
        elementTimeStamp = element->getTimeStamp();
        elementIsCurrent = (element->getReportID()==reportID) && (CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp)==0);
        
        if ( !elementIsCurrent )
            continue;
        
        usagePage   = element->getUsagePage();
        usage       = element->getUsage();
        
        switch ( usagePage ) {
            case kHIDPage_GenericDesktop:
                switch ( usage ) {
                    case kHIDUsage_GD_X:
                        gcFixedVal = &_gameController.joystick.x;
                        break;
                    case kHIDUsage_GD_Y:
                        gcFixedVal = &_gameController.joystick.y;
                        break;
                    case kHIDUsage_GD_Z:
                        gcFixedVal = &_gameController.joystick.z;
                        break;
                    case kHIDUsage_GD_Rz:
                        gcFixedVal = &_gameController.joystick.rz;
                        break;
                    case kHIDUsage_GD_DPadUp:
                        gcFixedVal = &_gameController.dpad.up;
                        break;
                    case kHIDUsage_GD_DPadDown:
                        gcFixedVal = &_gameController.dpad.down;
                        break;
                    case kHIDUsage_GD_DPadLeft:
                        gcFixedVal = &_gameController.dpad.left;
                        break;
                    case kHIDUsage_GD_DPadRight:
                        gcFixedVal = &_gameController.dpad.right;
                        break;
                }
                break;
            case kHIDPage_Button:
                switch ( usage ) {
                    case 1:
                        gcFixedVal = &_gameController.face.a;
                        break;
                    case 2:
                        gcFixedVal = &_gameController.face.b;
                        break;
                    case 3:
                        gcFixedVal = &_gameController.face.x;
                        break;
                    case 4:
                        gcFixedVal = &_gameController.face.y;
                        break;
                    case 5:
                        gcFixedVal = &_gameController.shoulder.l1;
                        break;
                    case 6:
                        gcFixedVal = &_gameController.shoulder.r1;
                        break;
                    case 7:
                        gcFixedVal = &_gameController.shoulder.l2;
                        break;
                    case 8:
                        gcFixedVal = &_gameController.shoulder.r2;
                        break;
                        
                }
                break;
        }

        if ( gcFixedVal && ( *gcFixedVal != ( elementFixedVal = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated) ) ) )
        {
            *gcFixedVal = elementFixedVal;
            handled = true;
        }
    }
    
    // Don't dispatch an event if no controller elements have changed since the last dispatch.
    require_quiet(handled, exit);
    
    require_quiet(reportID == _gameController.sendingReportID, exit);
    
    if ( _gameController.extended ) {
        dispatchExtendedGameControllerEvent(timeStamp, _gameController.dpad.up, _gameController.dpad.down, _gameController.dpad.left, _gameController.dpad.right, _gameController.face.x, _gameController.face.y, _gameController.face.a, _gameController.face.b, _gameController.shoulder.l1, _gameController.shoulder.r1, _gameController.shoulder.l2, _gameController.shoulder.r2, _gameController.joystick.x, _gameController.joystick.y, _gameController.joystick.z, _gameController.joystick.rz);
    } else {
        dispatchStandardGameControllerEvent(timeStamp, _gameController.dpad.up, _gameController.dpad.down, _gameController.dpad.left, _gameController.dpad.right, _gameController.face.x, _gameController.face.y, _gameController.face.a, _gameController.face.b, _gameController.shoulder.l1, _gameController.shoulder.r1);
    }
    
exit:
    return;
}


//====================================================================================================
// IOHIDEventDriver::handleMultiAxisPointerReport
//====================================================================================================
void IOHIDEventDriver::handleMultiAxisPointerReport(AbsoluteTime timeStamp, UInt32 reportID)
{
    bool        handled     = false;
    UInt32      index, count;
    
    require_quiet(!_multiAxis.disabled, exit);

    require_quiet(_multiAxis.capable, exit);

    require_quiet(_multiAxis.elements, exit);

    for (index=0, count=_multiAxis.elements->getCount(); index<count; index++) {
        IOHIDElement *  element;
        AbsoluteTime    elementTimeStamp;
        UInt32          usagePage, usage;
        bool            elementIsCurrent;
        
        element = OSDynamicCast(IOHIDElement, _multiAxis.elements->getObject(index));
        if ( !element )
            continue;
        
        elementTimeStamp = element->getTimeStamp();
        elementIsCurrent = (element->getReportID()==reportID) && (CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp)==0);
        
        if ( !elementIsCurrent )
            continue;
        
        handled |= elementIsCurrent;
        
        usagePage   = element->getUsagePage();
        usage       = element->getUsage();

        switch ( usagePage ) {
            case kHIDPage_GenericDesktop:
                switch ( usage ) {
                    case kHIDUsage_GD_X:
                    case kHIDUsage_GD_Y:
                    case kHIDUsage_GD_Z:
                    case kHIDUsage_GD_Rx:
                    case kHIDUsage_GD_Ry:
                    case kHIDUsage_GD_Rz:
                        _multiAxis.axis[GET_AXIS_INDEX(element->getUsage())] = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        break;
                }
                break;
            case kHIDPage_Button:
                setButtonState(&_multiAxis.buttonState, (usage - 1), element->getValue());
                break;
        }
    }

    require_quiet(handled, exit);
    
    require_quiet(reportID == _multiAxis.sendingReportID, exit);

    dispatchMultiAxisPointerEvent(timeStamp, _multiAxis.buttonState, _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_X)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Y)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Z)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Rx)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Ry)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Rz)], _multiAxis.options);
exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::handleRelativeReport
//====================================================================================================
void IOHIDEventDriver::handleRelativeReport(AbsoluteTime timeStamp, UInt32 reportID)
{
    bool        handled     = false;
    SInt32      dX          = 0;
    SInt32      dY          = 0;
    UInt32      buttonState = 0;
    UInt32      index, count;
    
    require_quiet(!_relative.disabled, exit);
    
    require_quiet(_relative.elements, exit);

    for (index=0, count=_relative.elements->getCount(); index<count; index++) {
        IOHIDElement *  element;
        AbsoluteTime    elementTimeStamp;
        UInt32          usagePage, usage;
        bool            elementIsCurrent;
        
        element = OSDynamicCast(IOHIDElement, _relative.elements->getObject(index));
        if ( !element )
            continue;
        
        elementTimeStamp = element->getTimeStamp();
        elementIsCurrent = (element->getReportID()==reportID) && (CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp)==0);
        
        handled |= elementIsCurrent;
        
        usagePage   = element->getUsagePage();
        usage       = element->getUsage();

        switch ( usagePage ) {
            case kHIDPage_GenericDesktop:
                switch ( element->getUsage() ) {
                    case kHIDUsage_GD_X:
                        dX = elementIsCurrent ? element->getValue() : 0;
                        break;
                    case kHIDUsage_GD_Y:
                        dY = elementIsCurrent ? element->getValue() : 0;
                        break;
                }
                break;
            case kHIDPage_Button:
                setButtonState(&buttonState, (usage - 1), element->getValue());
                break;
        }
    }
    
    require_quiet(handled, exit);
    
    dispatchRelativePointerEvent(timeStamp, dX, dY, buttonState);
    
exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::handleDigitizerReport
//====================================================================================================
void IOHIDEventDriver::handleDigitizerReport(AbsoluteTime timeStamp, UInt32 reportID)
{
    UInt32 index, count;

    require_quiet(_digitizer.transducers, exit);
  
    if (_digitizer.collectionDispatch) {
      
        handleDigitizerCollectionReport (timeStamp, reportID);
        return;
    }
  
    for (index=0, count = _digitizer.transducers->getCount(); index<count; index++) {
        DigitizerTransducer * transducer = NULL;
        
        transducer = OSDynamicCast(DigitizerTransducer, _digitizer.transducers->getObject(index));
        if ( !transducer ) {
            continue;
        }

        handleDigitizerTransducerReport(transducer, timeStamp, reportID);
    }

exit:

    return;
}

//====================================================================================================
// IOHIDEventDriver::handleDigitizerCollectionReport
//====================================================================================================
void IOHIDEventDriver::handleDigitizerCollectionReport(AbsoluteTime timeStamp, UInt32 reportID) {

    UInt32 index, count;

    IOHIDEvent* collectionEvent = NULL;
    IOHIDEvent* event = NULL;
  
    bool    touch       = false;
    bool    range       = false;
    UInt32  mask        = 0;
    UInt32  finger      = 0;
    UInt32  buttons     = 0;
    IOFixed touchX      = 0;
    IOFixed touchY      = 0;
    IOFixed inRangeX    = 0;
    IOFixed inRangeY    = 0;
    UInt32  touchCount  = 0;
    UInt32  inRangeCount= 0;
  
    if (_digitizer.touchCancelElement && _digitizer.touchCancelElement->getReportID()==reportID) {
        AbsoluteTime elementTimeStamp =  _digitizer.touchCancelElement->getTimeStamp();
        if (CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp)==0) {
            collectionEvent = IOHIDEvent::digitizerEvent(timeStamp, 0, kIOHIDDigitizerTransducerTypeFinger, false, 0, 0, 0, 0, 0, 0, 0, 0);
          mask |= _digitizer.touchCancelElement->getValue() ? kIOHIDDigitizerEventCancel : 0;
        }
    }

    require_quiet(_digitizer.transducers, exit);
  
    for (index=0, count = _digitizer.transducers->getCount(); index<count; index++) {
        DigitizerTransducer * transducer = NULL;
        
        transducer = OSDynamicCast(DigitizerTransducer, _digitizer.transducers->getObject(index));
        if ( !transducer ) {
            continue;
        }
      
        event = createDigitizerTransducerEventForReport(transducer, timeStamp, reportID);
        if (event) {
            if (collectionEvent == NULL) {
                collectionEvent = IOHIDEvent::digitizerEvent(timeStamp, 0, kIOHIDDigitizerTransducerTypeFinger, false, 0, 0, 0, 0, 0, 0, 0, 0);
                require_quiet(collectionEvent, exit);
                collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerCollection, TRUE);
            }
            bool eventTouch = event->getIntegerValue(kIOHIDEventFieldDigitizerTouch) ? true : false;
            if (eventTouch) {
                touchX += event->getFixedValue (kIOHIDEventFieldDigitizerX);
                touchY += event->getFixedValue (kIOHIDEventFieldDigitizerY);
                ++touchCount;
            }
          
            bool eventInRange = event->getIntegerValue(kIOHIDEventFieldDigitizerRange) ? true : false;
            if (eventInRange) {
                inRangeX += event->getFixedValue (kIOHIDEventFieldDigitizerX);
                inRangeY += event->getFixedValue (kIOHIDEventFieldDigitizerY);
                ++inRangeCount;
            }
          
            touch |= eventTouch;
            range |= eventInRange;
            mask  |= event->getIntegerValue(kIOHIDEventFieldDigitizerEventMask);
            buttons |= event->getIntegerValue(kIOHIDEventFieldDigitizerButtonMask);
            if (event->getIntegerValue(kIOHIDEventFieldDigitizerType) == kIOHIDDigitizerTransducerTypeFinger) {
                finger++;
            }
            collectionEvent->appendChild(event);
            collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerCollection, TRUE);
            event->release();
        }
    }
  
  
    if (collectionEvent) {
        if (touchCount) {
            _digitizer.centroidX = IOFixedDivide(touchX, touchCount << 16);
            _digitizer.centroidY = IOFixedDivide(touchY, touchCount << 16);
        } else if (inRangeCount) {
            _digitizer.centroidX = IOFixedDivide(inRangeX, inRangeCount << 16);
            _digitizer.centroidY = IOFixedDivide(inRangeY, inRangeCount << 16);
        }
        collectionEvent->setFixedValue(kIOHIDEventFieldDigitizerX, _digitizer.centroidX);
        collectionEvent->setFixedValue(kIOHIDEventFieldDigitizerY, _digitizer.centroidY);
        collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerRange, range);
        collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, mask);
        collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerTouch, touch);
        collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerButtonMask, buttons);
        if (finger > 1) {
            collectionEvent->getIntegerValue(kIOHIDDigitizerTransducerTypeHand);
        }
        dispatchEvent(collectionEvent);
        collectionEvent->release();
    }

exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::handleDigitizerReport
//====================================================================================================
IOHIDEvent* IOHIDEventDriver::handleDigitizerTransducerReport(DigitizerTransducer * transducer, AbsoluteTime timeStamp, UInt32 reportID)
{
    bool                    handled         = false;
    UInt32                  elementIndex    = 0;
    UInt32                  elementCount    = 0;
    UInt32                  buttonState     = 0;
    UInt32                  transducerID    = reportID;
    IOFixed                 X               = 0;
    IOFixed                 Y               = 0;
    IOFixed                 Z               = 0;
    IOFixed                 tipPressure     = 0;
    IOFixed                 barrelPressure  = 0;
    IOFixed                 tiltX           = 0;
    IOFixed                 tiltY           = 0;
    IOFixed                 twist           = 0;
    bool                    invert          = false;
    bool                    inRange         = true;
    bool                    valid           = true;
  
    require_quiet(transducer->elements, exit);

    for (elementIndex=0, elementCount=transducer->elements->getCount(); elementIndex<elementCount; elementIndex++) {
        IOHIDElement *  element;
        AbsoluteTime    elementTimeStamp;
        bool            elementIsCurrent;
        UInt32          usagePage;
        UInt32          usage;
        UInt32          value;
        
        element = OSDynamicCast(IOHIDElement, transducer->elements->getObject(elementIndex));
        if ( !element )
            continue;
        
        elementTimeStamp = element->getTimeStamp();
        elementIsCurrent = (element->getReportID()==reportID) && (CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp)==0);
        
        usagePage   = element->getUsagePage();
        usage       = element->getUsage();
        value       = element->getValue();
        
        switch ( usagePage ) {
            case kHIDPage_GenericDesktop:
                switch ( usage ) {
                    case kHIDUsage_GD_X:
                        X = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_GD_Y:
                        Y = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_GD_Z:
                        Z = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                }
                break;
            case kHIDPage_Button:
                setButtonState(&buttonState, (usage - 1), value);
                handled    |= elementIsCurrent;
                break;
            case kHIDPage_Digitizer:
                switch ( usage ) {
                    case kHIDUsage_Dig_TransducerIndex:
                    case kHIDUsage_Dig_ContactIdentifier:
                        transducerID = value;
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Touch:
                    case kHIDUsage_Dig_TipSwitch:
                        setButtonState ( &buttonState, 0, value);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_BarrelSwitch:
                        setButtonState ( &buttonState, 1, value);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Eraser:
                        setButtonState ( &buttonState, 2, value);
                        invert = value != 0;
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_InRange:
                        inRange = value != 0;
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_BarrelPressure:
                        barrelPressure = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_TipPressure:
                        tipPressure = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_XTilt:
                        tiltX = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_YTilt:
                        tiltY = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Twist:
                        twist = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Invert:
                        invert = value != 0;
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Quality:
                    case kHIDUsage_Dig_DataValid:
                        if ( value == 0 )
                            valid = false;
                        handled    |= elementIsCurrent;
                    default:
                        break;
                }
                break;
        }        
    }
    
    require(handled, exit);
    
    require(valid, exit);

    dispatchDigitizerEventWithTiltOrientation(timeStamp, transducerID, transducer->type, inRange, buttonState, X, Y, Z, tipPressure, barrelPressure, twist, tiltX, tiltY, invert ? IOHIDEventService::kDigitizerInvert : 0);

exit:

    return NULL;
}


//====================================================================================================
// IOHIDEventDriver::createDigitizerTransducerEventForReport
//====================================================================================================
IOHIDEvent* IOHIDEventDriver::createDigitizerTransducerEventForReport(DigitizerTransducer * transducer, AbsoluteTime timeStamp, UInt32 reportID)
{
    bool                    handled         = false;
    UInt32                  elementIndex    = 0;
    UInt32                  elementCount    = 0;
    UInt32                  buttonState     = 0;
    UInt32                  transducerID    = reportID;
    IOFixed                 X               = 0;
    IOFixed                 Y               = 0;
    IOFixed                 Z               = 0;
    IOFixed                 tipPressure     = 0;
    IOFixed                 barrelPressure  = 0;
    IOFixed                 tiltX           = 0;
    IOFixed                 tiltY           = 0;
    IOFixed                 twist           = 0;
    bool                    invert          = false;
    bool                    inRange         = true;
    bool                    valid           = true;
    UInt32                  eventMask       = 0;
    UInt32                  eventOptions    = 0;
    UInt32                  touch           = 0;
    IOHIDEvent              *event          = NULL;
  
    require_quiet(transducer->elements, exit);
  
  
    for (elementIndex=0, elementCount=transducer->elements->getCount(); elementIndex<elementCount; elementIndex++) {
        IOHIDElement *  element;
        AbsoluteTime    elementTimeStamp;
        bool            elementIsCurrent;
        UInt32          usagePage;
        UInt32          usage;
        UInt32          value;
        
        element = OSDynamicCast(IOHIDElement, transducer->elements->getObject(elementIndex));
        if ( !element )
            continue;
        
        elementTimeStamp = element->getTimeStamp();
        elementIsCurrent = (element->getReportID()==reportID) && (CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp)==0);
      
        usagePage   = element->getUsagePage();
        usage       = element->getUsage();
        value       = element->getValue();

        switch ( usagePage ) {
            case kHIDPage_GenericDesktop:
                switch ( usage ) {
                    case kHIDUsage_GD_X:
                        X = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_GD_Y:
                        Y = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_GD_Z:
                        Z = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                }
                break;
            case kHIDPage_Button:
                setButtonState(&buttonState, (usage - 1), value);
                handled    |= elementIsCurrent;
                break;
            case kHIDPage_Digitizer:
                switch ( usage ) {
                    case kHIDUsage_Dig_TransducerIndex:
                    case kHIDUsage_Dig_ContactIdentifier:
                        transducerID = value;
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Touch:
                    case kHIDUsage_Dig_TipSwitch:
                        setButtonState ( &buttonState, 0, value);
                        handled    |= (elementIsCurrent | buttonState != 0);
                        break;
                    case kHIDUsage_Dig_BarrelSwitch:
                        setButtonState ( &buttonState, 1, value);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Eraser:
                        setButtonState ( &buttonState, 2, value);
                        invert = value != 0;
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_InRange:
                        inRange = value != 0;
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_BarrelPressure:
                        barrelPressure = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_TipPressure:
                        tipPressure = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_XTilt:
                        tiltX = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_YTilt:
                        tiltY = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Twist:
                        twist = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Invert:
                        invert = value != 0;
                        handled    |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Quality:
                    case kHIDUsage_Dig_DataValid:
                        if ( value == 0 )
                            valid = false;
                        handled    |= elementIsCurrent;
                    default:
                        break;
                }
                break;
        }        
    }
  
    require(handled, exit);
    
    require(valid, exit);

    event = IOHIDEvent::digitizerEvent(timeStamp, transducerID, transducer->type, inRange, buttonState, X, Y, Z, tipPressure, barrelPressure, twist, eventOptions);
    require(event, exit);


    if ( tipPressure ) {
        touch |= 1;
    } else {
        touch |= buttonState & 1;
    }

    event->setIntegerValue(kIOHIDEventFieldDigitizerTouch, touch);

    if (touch != transducer->touch) {
        eventMask |= kIOHIDDigitizerEventTouch;
    }

    if (inRange && ( (transducer->X != X) || (transducer->Y != Y) || (transducer->Z != Z))) {
        eventMask |= kIOHIDDigitizerEventPosition;
    }

    if ( inRange ) {
        eventMask |= kIOHIDDigitizerEventRange;
        transducer->X = X;
        transducer->Y = Y;
        eventMask |= kIOHIDDigitizerEventIdentity;
    }
    transducer->inRange = inRange;

    event->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, eventMask);

    return event;

exit:

    return NULL;
}

//====================================================================================================
// IOHIDEventDriver::handleScrollReport
//====================================================================================================
void IOHIDEventDriver::handleScrollReport(AbsoluteTime timeStamp, UInt32 reportID)
{
    IOFixed     scrollVert  = 0;
    IOFixed     scrollHoriz = 0;

    UInt32      index, count;
    
    require_quiet(_scroll.elements, exit);

    for (index=0, count=_scroll.elements->getCount(); index<count; index++) {
        IOHIDElement *  element;
        AbsoluteTime    elementTimeStamp;
        UInt32          usagePage, usage;
        
        element = OSDynamicCast(IOHIDElement, _scroll.elements->getObject(index));
        if ( !element )
            continue;
        
        if ( element->getReportID() != reportID )
            continue;

        elementTimeStamp = element->getTimeStamp();
        if ( CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp) != 0 )
            continue;
        
        usagePage   = element->getUsagePage();
        usage       = element->getUsage();
        
        switch ( usagePage ) {
            case kHIDPage_GenericDesktop:
                switch ( usage ) {
                    case kHIDUsage_GD_Wheel:
                    case kHIDUsage_GD_Dial:
                        scrollVert = ((element->getFlags() & kIOHIDElementFlagsWrapMask) ? element->getValue(kIOHIDValueOptionsFlagRelativeSimple) : element->getValue()) << 16;
                        break;
                    case kHIDUsage_GD_Z:
                        scrollHoriz = ((element->getFlags() & kIOHIDElementFlagsWrapMask) ? element->getValue(kIOHIDValueOptionsFlagRelativeSimple) : element->getValue()) << 16;
                        break;
                    default:
                        break;
                }
                break;
            case kHIDPage_Consumer:
                switch ( usage ) {
                    case kHIDUsage_Csmr_ACPan:
                        scrollHoriz = (-element->getValue()) << 16;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    
    require(scrollVert || scrollHoriz, exit);
  
    dispatchScrollWheelEventWithFixed(timeStamp, scrollVert, scrollHoriz, 0);
    
exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::handleKeboardReport
//====================================================================================================
void IOHIDEventDriver::handleKeboardReport(AbsoluteTime timeStamp, UInt32 reportID)
{
    UInt32      volumeHandled   = 0;
    UInt32      volumeState     = 0;
    UInt32      index, count;
    
    require_quiet(_keyboard.elements, exit);
    
    for (index=0, count=_keyboard.elements->getCount(); index<count; index++) {
        IOHIDElement *  element;
        AbsoluteTime    elementTimeStamp;
        UInt32          usagePage, usage, value, preValue;
        
        element = OSDynamicCast(IOHIDElement, _keyboard.elements->getObject(index));
        if ( !element )
            continue;
        
        if ( element->getReportID() != reportID )
            continue;

        elementTimeStamp = element->getTimeStamp();
        if ( CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp) != 0 )
            continue;
        
        preValue    = element->getValue(kIOHIDValueOptionsFlagPrevious) != 0;
        value       = element->getValue() != 0;
        
        if ( value == preValue )
            continue;
        
        usagePage   = element->getUsagePage();
        usage       = element->getUsage();
        
        if ( usagePage == kHIDPage_Consumer ) {
            bool suppress = true;
            switch ( usage ) {
                case kHIDUsage_Csmr_VolumeIncrement:
                    volumeHandled   |= 0x1;
                    volumeState     |= (value) ? 0x1:0;
                    break;
                case kHIDUsage_Csmr_VolumeDecrement:
                    volumeHandled   |= 0x2;
                    volumeState     |= (value) ? 0x2:0;
                    break;
                case kHIDUsage_Csmr_Mute:
                    volumeHandled   |= 0x4;
                    volumeState     |= (value) ? 0x4:0;
                    break;
                default:
                    suppress = false;
                    break;
            }
            
            if ( suppress )
                continue;
        }
        
        dispatchKeyboardEvent(timeStamp, usagePage, usage, value);
    }

    // RY: Handle the case where Vol Increment, Decrement, and Mute are all down
    // If such an event occurs, it is likely that the device is defective,
    // and should be ignored.
    if ( (volumeState != 0x7) && (volumeHandled != 0x7) ) {
        // Volume Increment
        if ( volumeHandled & 0x1 )
            dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_VolumeIncrement, ((volumeState & 0x1) != 0));
        // Volume Decrement
        if ( volumeHandled & 0x2 )
            dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_VolumeDecrement, ((volumeState & 0x2) != 0));
        // Volume Mute
        if ( volumeHandled & 0x4 )
            dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_Mute, ((volumeState & 0x4) != 0));
    }

exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::handleUnicodeReport
//====================================================================================================
#if TARGET_OS_EMBEDDED
void IOHIDEventDriver::handleUnicodeReport(AbsoluteTime timeStamp, UInt32 reportID)
{
    handleUnicodeLegacyReport(timeStamp, reportID);
    handleUnicodeGestureReport(timeStamp, reportID);
}

//====================================================================================================
// IOHIDEventDriver::handleUnicodeLegacyReport
//====================================================================================================
void IOHIDEventDriver::handleUnicodeLegacyReport(AbsoluteTime timeStamp, UInt32 reportID)
{
    UInt32      index, count;
    
    require_quiet(_unicode.legacyElements, exit);

    for (index=0, count=_unicode.legacyElements->getCount(); index<count; index++) {
        IOHIDElement *  element;
        AbsoluteTime    elementTimeStamp;
        
        element = OSDynamicCast(IOHIDElement, _unicode.legacyElements->getObject(index));
        if ( !element )
            continue;
        
        if ( element->getReportID() != reportID )
            continue;
        
        elementTimeStamp = element->getTimeStamp();
        if ( CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp) != 0 )
            continue;
        
        switch ( element->getUsagePage() ) {
            case kHIDPage_Unicode:
                if ( element->getValue() ) {
                    uint32_t usage  =element->getUsage();
                    
                    if ( usage > 0 && usage <= 0xffff ) {
                        uint16_t unicode_char = usage;
                        
                        unicode_char = OSSwapHostToLittleInt16(unicode_char);
                        
                        dispatchUnicodeEvent(timeStamp, (UInt8 *)&unicode_char, sizeof(unicode_char));
                    }
                }
                break;
        }
    }
exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::handleUnicodeGestureReport
//====================================================================================================
void IOHIDEventDriver::handleUnicodeGestureReport(AbsoluteTime timeStamp, UInt32 reportID)
{
    IOHIDEvent * main = NULL;
    UInt32 index, count;
    
    require_quiet(_unicode.gesturesCandidates, exit);
    
    for (index=0, count=_unicode.gesturesCandidates->getCount(); index<count; index++) {
        EventElementCollection *  candidate   = NULL;
        IOHIDEvent *        event       = NULL;
        
        candidate = OSDynamicCast(EventElementCollection, _unicode.gesturesCandidates->getObject(index));
        if ( !candidate )
            continue;
        
        event = handleUnicodeGestureCandidateReport(candidate, timeStamp, reportID);
        if ( !event )
            continue;
        
        if ( main ) {
            main->appendChild(event);
        } else {
            main = event;
            main->retain();
        }
        event->release();
    }
    
    require(main, exit);
    
    dispatchEvent(main);
    
    main->release();
    
exit:
    return;
}

//====================================================================================================
// IOHIDEventDriver::handleUnicodeGestureCandidateReport
//====================================================================================================
IOHIDEvent * IOHIDEventDriver::handleUnicodeGestureCandidateReport(EventElementCollection * candidate, AbsoluteTime timeStamp, UInt32 reportID)
{
    IOHIDEvent *        result        = NULL;
    uint8_t *           payload       = NULL;
    uint32_t            payloadLength = 0;
    uint32_t            length        = 0;
    UnicodeEncodingType encoding      = kUnicodeEncodingTypeUTF16LE;
    IOFixed             quality       = (1<<16);
    bool                handled       = false;
    uint32_t            index, count;
    OSData *            payloadData   = NULL;
    require(candidate->elements, exit);
    
    for ( index=0, count=candidate->elements->getCount(); index<count; index++ ) {
        IOHIDElement *  element;
        AbsoluteTime    elementTimeStamp;
        bool            elementIsCurrent;
        UInt32          usagePage;
        UInt32          usage;
        
        element = OSDynamicCast(IOHIDElement, candidate->elements->getObject(index));
        if ( !element )
            continue;
        
        elementTimeStamp = element->getTimeStamp();
        elementIsCurrent = (element->getReportID()==reportID) && (CMP_ABSOLUTETIME(&timeStamp, &elementTimeStamp)==0);
        
        handled    |= elementIsCurrent;
        
        usagePage   = element->getUsagePage();
        usage       = element->getUsage();
        
        switch ( usagePage ) {
            case kHIDPage_Digitizer:
                switch ( usage ) {
                    case kHIDUsage_Dig_GestureCharacterData:
                        payloadData  = element->getDataValue();
                        if (payloadData) {
                            payload = (uint8_t*)payloadData->getBytesNoCopy();
                            payloadLength = payloadData->getLength ();
                        }
                        break;
                    case kHIDUsage_Dig_GestureCharacterDataLength:
                        length = element->getValue();
                        break;
                    case kHIDUsage_Dig_GestureCharacterEncodingUTF8:
                        if ( element->getValue() )
                            encoding = kUnicodeEncodingTypeUTF8;
                        break;
                    case kHIDUsage_Dig_GestureCharacterEncodingUTF16LE:
                        if ( element->getValue() )
                            encoding = kUnicodeEncodingTypeUTF16LE;
                        break;
                    case kHIDUsage_Dig_GestureCharacterEncodingUTF16BE:
                        if ( element->getValue() )
                            encoding = kUnicodeEncodingTypeUTF16BE;
                        break;
                    case kHIDUsage_Dig_GestureCharacterEncodingUTF32LE:
                        if ( element->getValue() )
                            encoding = kUnicodeEncodingTypeUTF32LE;
                        break;
                    case kHIDUsage_Dig_GestureCharacterEncodingUTF32BE:
                        if ( element->getValue() )
                            encoding = kUnicodeEncodingTypeUTF32BE;
                        break;
                    case kHIDUsage_Dig_GestureCharacterQuality:
                        quality = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    
    require(handled, exit);
    
    result = IOHIDEvent::unicodeEvent(timeStamp, payload, payloadLength < length ? payloadLength : length, encoding, quality, 0);

exit:
    return result;
}
#else
void IOHIDEventDriver::handleUnicodeReport(AbsoluteTime , UInt32 )
{
    
}
void IOHIDEventDriver::handleUnicodeLegacyReport(AbsoluteTime , UInt32 )
{
    
}
void IOHIDEventDriver::handleUnicodeGestureReport(AbsoluteTime , UInt32 )
{
    
}
IOHIDEvent * IOHIDEventDriver::handleUnicodeGestureCandidateReport(EventElementCollection *, AbsoluteTime , UInt32 )
{
    return NULL;
}
#endif

//====================================================================================================
// IOHIDEventDriver::handleVendorMessageReport
//====================================================================================================
void IOHIDEventDriver::handleVendorMessageReport(AbsoluteTime timeStamp,  IOMemoryDescriptor * report, UInt32 reportID, int phase) {
    if (_vendorMessage.elements) {
        if (phase == kIOParseVendorMessageReport) {
            //Prepare events and defer to be dispatched as child events
            for (int index = 0; index < _vendorMessage.elements->getCount(); index++) {
                if (!_vendorMessage.pendingEvents){
                    _vendorMessage.pendingEvents = OSArray::withCapacity (_vendorMessage.elements->getCount());
                    if (_vendorMessage.pendingEvents == NULL) {
                        break;
                    }
                }
                IOHIDElement * currentElement = OSDynamicCast(IOHIDElement, _vendorMessage.elements->getObject(index));
                if (currentElement) {
                    OSData *value = currentElement->getDataValue();
                    if (value && value->getLength()) {
                        const void *data = value->getBytesNoCopy();
                        unsigned int dataLength = value->getLength();
                        IOHIDEvent * event = IOHIDEvent::vendorDefinedEvent(
                                                          currentElement->getTimeStamp(),
                                                          currentElement->getUsagePage(),
                                                          currentElement->getUsage(),
                                                          0,
                                                          (UInt8*)data,
                                                          dataLength
                                                          );
                        if (event) {
                            _vendorMessage.pendingEvents->setObject(event);
                            event->release();
                        }
                    }
                }
            }
        } else if (_vendorMessage.pendingEvents && _vendorMessage.pendingEvents->getCount()) {
            //Events where not dispatched as child events  dispatch them as individual events.
            OSArray * pendingEvents = OSArray::withArray(_vendorMessage.pendingEvents);
            _vendorMessage.pendingEvents->flushCollection();
            if (pendingEvents) {
                for (int index = 0; index < pendingEvents->getCount(); index++) {
                    IOHIDEvent * event = OSDynamicCast (IOHIDEvent, pendingEvents->getObject(index));
                    if (event) {
                        dispatchEvent (event);
                    }
                }
                pendingEvents->release();
            }
        }
    }
}

//====================================================================================================
// IOHIDEventDriver::dispatchEvent
//====================================================================================================
void IOHIDEventDriver::dispatchEvent(IOHIDEvent * event, IOOptionBits options) {
  if (_vendorMessage.pendingEvents && _vendorMessage.pendingEvents->getCount()) {
      for (int index = 0; index < _vendorMessage.pendingEvents->getCount(); index++) {
          IOHIDEvent * childEvent = OSDynamicCast (IOHIDEvent, _vendorMessage.pendingEvents->getObject(index));
          if (childEvent) {
              event->appendChild(childEvent);
          }
      }
      _vendorMessage.pendingEvents->flushCollection();
  }
  super::dispatchEvent(event, options);
}


//====================================================================================================
// IOHIDEventDriver::setElementValue
//====================================================================================================
IOReturn IOHIDEventDriver::setElementValue (
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value )
{
    IOHIDElement *element = NULL;
    uint32_t count, index;

    require(usagePage == kHIDPage_LEDs , exit);
    
    require(_led.elements, exit);
    
    count = _led.elements->getCount();
    require(count, exit);
    
    for (index=0; index<count; index++) {
        IOHIDElement * temp = OSDynamicCast(IOHIDElement, _led.elements->getObject(index));
        
        if ( !temp )
            continue;
        
        if ( temp->getUsage() != usage )
            continue;
        
        element = temp;
        break;
    }
    
    require(element, exit);
    element->setValue(value);
    return kIOReturnSuccess;
exit:
    return kIOReturnUnsupported;
}

//====================================================================================================
// IOHIDEventDriver::getElementValue
//====================================================================================================
UInt32 IOHIDEventDriver::getElementValue (
                                UInt32                      usagePage,
                                UInt32                      usage )
{
    IOHIDElement *element = NULL;
    uint32_t count, index;
    
    require(usagePage == kHIDPage_LEDs , exit);
    
    require(_led.elements, exit);
    
    count = _led.elements->getCount();
    require(count, exit);
    
    for (index=0; index<count; index++) {
        IOHIDElement * temp = OSDynamicCast(IOHIDElement, _led.elements->getObject(index));
        
        if ( !temp )
            continue;
        
        if ( temp->getUsage() != usage )
            continue;
        
        element = temp;
        break;
    }
    
exit:
    return (element) ? element->getValue() : 0;
}

//====================================================================================================
// IOHIDEventDriver::serializeDebugState
//====================================================================================================
bool   IOHIDEventDriver::serializeDebugState(void * ref, OSSerialize * serializer) {
    bool          result = false;
    uint64_t      currentTime,deltaTime;
    uint64_t      nanoTime;
    OSNumber      *num;
    OSDictionary  *debugDict = OSDictionary::withCapacity(4);
  
    require(debugDict, exit);
    clock_get_uptime(&currentTime);
  
  
    if (_lastReportTime) {
        deltaTime = AbsoluteTime_to_scalar(&currentTime) - AbsoluteTime_to_scalar(&(_lastReportTime));

        absolutetime_to_nanoseconds(deltaTime, &nanoTime);
        num = OSNumber::withNumber(nanoTime, 64);
        if (num) {
            debugDict->setObject("LastReportTime", num);
            OSSafeReleaseNULL(num);
        }
    }
  
    result = debugDict->serialize(serializer);
    debugDict->release();

exit:
    return result;
}


OSMetaClassDefineReservedUnused(IOHIDEventDriver,  0);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  1);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  2);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  3);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  4);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  5);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  6);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  7);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  8);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  9);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 10);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 11);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 12);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 13);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 14);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 15);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 16);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 17);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 18);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 19);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 20);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 21);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 22);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 23);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 24);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 25);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 26);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 27);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 28);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 29);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 30);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 31);

