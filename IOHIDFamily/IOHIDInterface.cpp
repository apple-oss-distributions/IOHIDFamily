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

#include <IOKit/IOLib.h>    // IOMalloc*/IOFree*
#include "IOHIDInterface.h"
#include "IOHIDDevice.h"
#include "IOHIDElementPrivate.h"
#include "IOHIDLibUserClient.h"
#include "OSStackRetain.h"
#include "IOHIDDebug.h"
#include <IOKit/hidsystem/IOHIDShared.h>
#include <AssertMacros.h>
#include "IOHIDFamilyTrace.h"
#include <IOKitUser/IODataQueueDispatchSource.h>
#include "IOHIDFamilyPrivate.h"
#include "IOHIDPrivateKeys.h"
#include "DriverKitSharedDefs.h"
#include <IOKit/IOKitKeys.h>

#define BUFFER_CREATE_WARNING          100
#define BUFFER_OUTSTANDING_WARNING     20
#define kIOHIDTransportDextEntitlement "com.apple.developer.driverkit.transport.hid"

#define dispatch_workloop_sync(b)            \
if (!isInactive() && _commandGate) {         \
    _commandGate->runActionBlock(^IOReturn{  \
        if (isInactive()) {                  \
            return kIOReturnOffline;         \
        };                                   \
        b                                    \
        return kIOReturnSuccess;             \
    });                                      \
}


//===========================================================================
// IOHIDInterface class

#define super IOService

OSDefineMetaClassAndStructors( IOHIDInterface, IOService )

// RESERVED IOHIDInterface CLASS VARIABLES
// Defined here to avoid conflicts from within header file
#define _reportInterval             _reserved->reportInterval
#define _reportAction               _reserved->reportAction
#define _workLoop                   _reserved->workLoop
#define _commandGate                _reserved->commandGate
#define _deviceElements             _reserved->deviceElements
#define _reportPool                 _reserved->reportPool
#define _opened                     _reserved->opened
#define _terminated                 _reserved->terminated
#define _bufferLeakPanic            _reserved->bufferLeakPanic
#define _debugStats                 _reserved->debugStats

//---------------------------------------------------------------------------
// IOHIDInterface::free

void IOHIDInterface::free()
{
    OSSafeReleaseNULL(_transportString);
    OSSafeReleaseNULL(_elementArray);
    OSSafeReleaseNULL(_manufacturerString);
    OSSafeReleaseNULL(_productString);
    OSSafeReleaseNULL(_serialNumberString);
    OSSafeReleaseNULL(_deviceElements);
    OSSafeReleaseNULL(_reportPool);
    OSSafeReleaseNULL(_debugStats);
    
    if (_commandGate) {
        if ( _workLoop ) {
            _workLoop->removeEventSource(_commandGate);
        }
        OSSafeReleaseNULL(_commandGate);
    }
    
    OSSafeReleaseNULL(_workLoop);

    if ( _reserved )
    {
        IOFreeType(_reserved, ExpansionData);
    }
    
    if (_owner) {
        OSSafeReleaseNULL(_owner);
    }

    super::free();
}

//---------------------------------------------------------------------------
// IOHIDInterface::init

bool IOHIDInterface::init( OSDictionary * dictionary )
{
    if ( !super::init(dictionary) )
        return false;

    _reserved = IOMallocType(ExpansionData);

    if (!_reserved)
        return false;            
        
    memset(_maxReportSize, 0, sizeof(IOByteCount) * kIOHIDReportTypeCount);
        
    return true;
}

//---------------------------------------------------------------------------
// IOHIDInterface::withElements

IOHIDInterface * IOHIDInterface::withElements( OSArray * elements )
{
    IOHIDInterface * nub = new IOHIDInterface;
    
    if ((nub == 0) || !nub->init() || !elements)
    {
        if (nub) nub->release();
        return 0;
    }
    
    nub->_elementArray  = elements;
    nub->_elementArray->retain();
    
    return nub;
}

//---------------------------------------------------------------------------
// IOHIDInterface::message

IOReturn IOHIDInterface::message(UInt32 type,
                                 IOService * provider,
                                 void * argument)
{
    IOReturn result = kIOReturnSuccess;
    OSIterator * iter;
    IOService  * service;
    bool         terminating = false;

    if (type == kIOMessageServiceIsRequestingClose) {
        result = messageClients(type, argument);
    } else if  (type == kIOHIDMessageRelayServiceInterfaceActive && provider != _owner) {
        result = _owner->message(type, this, argument);
    } else if (type == kIOHIDDeviceWillTerminate && provider == _owner) {
        _terminated = true;
    } else if (type == kIOHIDMessageInterfaceRematch && provider == _owner) {
        iter = getClientIterator();
        if (iter) {
            while ((service = OSDynamicCast(IOService, iter->getNextObject()))) {
                terminating = true;
                service->terminate(kIOServiceTerminateWithRematch);
            }
            OSSafeReleaseNULL(iter);
        }
        if (!terminating) {
            registerService();
        }
    } else {
        result = super::message(type, provider, argument);
    }
    
    return result;
}


//---------------------------------------------------------------------------
// IOHIDInterface::start

bool IOHIDInterface::start( IOService * provider )
{
    bool builtin = false;
    OSArray *entitlements = NULL;
    OSArray *subArray = NULL;
    OSSerializer * debugSerializer  = NULL;
    
#define SET_STR_FROM_PROP(key, val)         \
    do {                                    \
        OSObject *obj = copyProperty(key);  \
        val = OSDynamicCast(OSString, obj); \
        if (!val) {                         \
            OSSafeReleaseNULL(obj);         \
        }                                   \
    } while (0)
    
#define SET_INT_FROM_PROP(key, val)                     \
    do {                                                \
        OSObject *obj = copyProperty(key);              \
        OSNumber *num = OSDynamicCast(OSNumber, obj);   \
        if (num) {                                      \
            val = num->unsigned32BitValue();            \
        }                                               \
        OSSafeReleaseNULL(obj);                         \
    } while (0)
    
    if ( !super::start(provider) )
        return false;
		
	_owner = OSDynamicCast( IOHIDDevice, provider );
	
	if ( !_owner )
		return false;
    
    // bump up reference in case we fail to attach
    _owner->retain();
    
    _deviceElements = _owner->_elementArray;
    if (!_deviceElements) {
        return false;
    }
    _deviceElements->retain();
    
    _workLoop = getWorkLoop();
    if ( !_workLoop ) {
        return false;
    }
    
    _workLoop->retain();
    
    _commandGate = IOCommandGate::commandGate(this);
    if (!_commandGate || (_workLoop->addEventSource(_commandGate) != kIOReturnSuccess)) {
        return false;
    }

    SET_STR_FROM_PROP(kIOHIDTransportKey, _transportString);
    SET_STR_FROM_PROP(kIOHIDManufacturerKey, _manufacturerString);
    SET_STR_FROM_PROP(kIOHIDProductKey, _productString);
    SET_STR_FROM_PROP(kIOHIDSerialNumberKey, _serialNumberString);
    
    SET_INT_FROM_PROP(kIOHIDLocationIDKey, _locationID);
    SET_INT_FROM_PROP(kIOHIDVendorIDKey, _vendorID);
    SET_INT_FROM_PROP(kIOHIDVendorIDSourceKey, _vendorIDSource);
    SET_INT_FROM_PROP(kIOHIDProductIDKey, _productID);
    SET_INT_FROM_PROP(kIOHIDVersionNumberKey, _version);
    SET_INT_FROM_PROP(kIOHIDCountryCodeKey, _countryCode);
    SET_INT_FROM_PROP(kIOHIDMaxInputReportSizeKey, _maxReportSize[kIOHIDReportTypeInput]);
    SET_INT_FROM_PROP(kIOHIDMaxOutputReportSizeKey, _maxReportSize[kIOHIDReportTypeOutput]);
    SET_INT_FROM_PROP(kIOHIDMaxFeatureReportSizeKey, _maxReportSize[kIOHIDReportTypeFeature]);
    SET_INT_FROM_PROP(kIOHIDReportIntervalKey, _reportInterval);
    
    OSObject *object = _owner->copyProperty(kIOHIDPhysicalDeviceUniqueIDKey);
    OSString *string = OSDynamicCast(OSString, object);
    if ( string ) setProperty(kIOHIDPhysicalDeviceUniqueIDKey, string);
    OSSafeReleaseNULL(object);
    
    object = _owner->copyProperty(kIOHIDBuiltInKey);
    OSBoolean *boolean = OSDynamicCast(OSBoolean, object);
    if (boolean) {
        setProperty(kIOHIDBuiltInKey, boolean);
        builtin = boolean->getValue();
    }
    OSSafeReleaseNULL(object);

    object = _owner->copyProperty(kIOHIDPointerAccelerationSupportKey);
    boolean = OSDynamicCast(OSBoolean, object);
    if (boolean) {
        setProperty(kIOHIDPointerAccelerationSupportKey, boolean);
    }
    OSSafeReleaseNULL(object);

    object = _owner->copyProperty(kIOHIDScrollAccelerationSupportKey);
    boolean = OSDynamicCast(OSBoolean, object);
    if (boolean) {
        setProperty(kIOHIDScrollAccelerationSupportKey, boolean);
    }
    OSSafeReleaseNULL(object);
    
    entitlements = OSArray::withCapacity(1);
    if (!entitlements) {
        return false;
    }
    
    OSString *str = OSString::withCString(kIOHIDTransportDextEntitlement);
    
    if (builtin) {
        subArray = OSArray::withCapacity(2);
        
        if (subArray) {
            if (str) {
                subArray->setObject(str);
                str->release();
            }
            
            str = OSString::withCString(kIODriverKitTransportBuiltinEntitlementKey);
            if (str) {
                subArray->setObject(str);
                str->release();
            }
            
            entitlements->setObject(subArray);
            subArray->release();
        }
    } else {
        if (str) {
            entitlements->setObject(str);
            str->release();
        }
    }
    
    setProperty(kIOServiceDEXTEntitlementsKey, entitlements);
    entitlements->release();

    debugSerializer = OSSerializer::forTarget(this, OSMemberFunctionCast(OSSerializerCallback, this, &IOHIDInterface::serializeDebugState));
    if (debugSerializer) {
        super::setProperty("DebugState", debugSerializer);
        OSSafeReleaseNULL(debugSerializer);
    }
    
    HIDServiceLog("start for %s:0x%llx", provider->getName(), provider->getRegistryEntryID());
    
    registerService(kIOServiceAsynchronous);
    
    return true;
}

//====================================================================================================
// IOHIDInterface::stop
//====================================================================================================
void IOHIDInterface::stop( IOService * provider )
{
    super::stop(provider);
}

//---------------------------------------------------------------------------
// IOHIDInterface::matchPropertyTable

bool IOHIDInterface::matchPropertyTable(
                                OSDictionary *              table, 
                                SInt32 *                    score)
{
    IOService * provider;
    bool        ret;
    RETAIN_ON_STACK(this);
    
    if ( !super::matchPropertyTable(table, score) || !(provider = OSDynamicCast(IOService, copyParentEntry(gIOServicePlane))) )
        return false;
        
    // We should retain a reference to our provider while calling matchPropertyTable.
    // This is necessary in a situation where a user space process could be searching
    // the registry during termination.
    
    bool isPartofMultiInterfaceDevice = (provider->getProperty(kIOHIDMultipleInterfaceEnabledKey) == kOSBooleanTrue);
    
    if (isPartofMultiInterfaceDevice) {
        ret = MatchPropertyTable(this, table, score);
    } else {
        ret = provider->matchPropertyTable(table, score);
    }
    
    provider->release();
    
    return ret;
}

//---------------------------------------------------------------------------
// IOHIDInterface::open

bool IOHIDInterface::open (
                                IOService *                 client,
                                IOOptionBits                options,
                                InterruptReportAction       action,
                                void *                      refCon)
{
    if (!super::open(client, options)) {
        return false;
    }
        
    if (!_owner || !_owner->IOService::open(client, options)) {
        super::close(client, options);
        return false;
    }
    
    // Do something with action and refcon here
    _interruptTarget = client;
    _interruptAction = action;
    _interruptRefCon = refCon;
    
    return true;
}

void IOHIDInterface::close(  
								IOService *					client,
								IOOptionBits				options)
{
    _opened = false;

    if (_owner) {
        _owner->close(client, options);
    }
    
    _commandGate->runActionBlock(^IOReturn{
        
        _interruptTarget = NULL;
        _interruptAction = NULL;
        _interruptRefCon = NULL;
        
        if (_reportPool) {
            _reportPool->flushCollection();
        }
           
        OSSafeReleaseNULL(_reportAction);
        
        return kIOReturnSuccess;
    });
    
    super::close(client, options);
}

static const OSSymbol * propagateProps[] = {kIOHIDPropagatePropertyKeys};

bool IOHIDInterface::setProperty( const OSSymbol * key, OSObject * object)
{
    require(_owner, exit);

    for (const OSSymbol * prop : propagateProps) {
        if (key->isEqualTo(prop)) {
            _owner->setProperty(key, object);
        }
    }

exit:
    return super::setProperty(key, object);
}

OSString * IOHIDInterface::getTransport ()
{
    return _transportString;
}

OSString * IOHIDInterface::getManufacturer ()
{
    return _manufacturerString;
}

OSString * IOHIDInterface::getProduct ()
{
    return _productString;
}

OSString * IOHIDInterface::getSerialNumber ()
{
    return _serialNumberString;
}

UInt32 IOHIDInterface::getLocationID ()
{
    return _locationID;
}

UInt32 IOHIDInterface::getVendorID ()
{
    return _vendorID;
}

UInt32 IOHIDInterface::getVendorIDSource ()
{
    return _vendorIDSource;
}

UInt32 IOHIDInterface::getProductID ()
{
    return _productID;
}

UInt32 IOHIDInterface::getVersion ()
{
    return _version;
}

UInt32 IOHIDInterface::getCountryCode ()
{
    return _countryCode;
}

IOByteCount IOHIDInterface::getMaxReportSize (IOHIDReportType type)
{
    return _maxReportSize[type];
}


OSArray * IOHIDInterface::createMatchingElements (
                                OSDictionary *              matching, 
                                IOOptionBits                options)
{
    UInt32                    count        = _elementArray->getCount();
    IOHIDElementPrivate *   element        = NULL;
    OSArray *                elements    = NULL;

    if ( count )
    {
        if ( matching )
        {

            elements = OSArray::withCapacity(count);

            for ( UInt32 i = 0; i < count; i ++)
            {
                // Compare properties.
                if (( element = (IOHIDElementPrivate *)_elementArray->getObject(i) )
                        && element->matchProperties(matching))
                {
                    elements->setObject(element);
                }
            }
        }
        else {
            if (options & kIOHIDSearchDeviceElements) {
                elements = OSArray::withArray(_deviceElements);
            } else {
                elements = OSArray::withArray(_elementArray);
            }
        }
    }

    return elements;
}

void IOHIDInterface::handleReport ( 
                                AbsoluteTime                timestamp,
                                IOMemoryDescriptor *        report,
                                IOHIDReportType             reportType,
                                UInt32                      reportID,
                                IOOptionBits                options __unused)
{
    if ( !_interruptAction )
        return;
    
    if (_reportAction) {
        (*_interruptAction)(this, timestamp, report, reportType, reportID, _interruptRefCon);
    } else {
        (*_interruptAction)(_interruptTarget, timestamp, report, reportType, reportID, _interruptRefCon);
    }
    
}

    
IOReturn IOHIDInterface::setReport ( 
                                IOMemoryDescriptor *        report,
                                IOHIDReportType             reportType,
                                UInt32                      reportID,
                                IOOptionBits                options)
{
    return _owner ? _owner->setReport(report, reportType, (reportID | (options << 8))) : kIOReturnOffline;
}

IOReturn IOHIDInterface::getReport ( 
                                IOMemoryDescriptor *        report,
                                IOHIDReportType             reportType,
                                UInt32                      reportID,
                                IOOptionBits                options)
{
    return _owner ? _owner->getReport(report, reportType, (reportID | (options << 8))) : kIOReturnOffline;
}


IOReturn IOHIDInterface::setReport ( 
                                IOMemoryDescriptor *        report __unused,
                                IOHIDReportType             reportType __unused,
                                UInt32                      reportID __unused,
                                IOOptionBits                options __unused,
                                UInt32                      completionTimeout __unused,
                                CompletionAction *          completion __unused)
{
    return kIOReturnUnsupported;
}
    

IOReturn IOHIDInterface::getReport ( 
                                IOMemoryDescriptor *        report __unused,
                                IOHIDReportType             reportType __unused,
                                UInt32                      reportID __unused,
                                IOOptionBits                options __unused,
                                UInt32                      completionTimeout __unused,
                                CompletionAction *          completion __unused)
{
    return kIOReturnUnsupported;
}

OSMetaClassDefineReservedUsed(IOHIDInterface,  0);
UInt32 IOHIDInterface::getReportInterval ()
{
    return _reportInterval;
}

OSMetaClassDefineReservedUnused(IOHIDInterface,  1);
OSMetaClassDefineReservedUnused(IOHIDInterface,  2);
OSMetaClassDefineReservedUnused(IOHIDInterface,  3);
OSMetaClassDefineReservedUnused(IOHIDInterface,  4);
OSMetaClassDefineReservedUnused(IOHIDInterface,  5);
OSMetaClassDefineReservedUnused(IOHIDInterface,  6);
OSMetaClassDefineReservedUnused(IOHIDInterface,  7);
OSMetaClassDefineReservedUnused(IOHIDInterface,  8);
OSMetaClassDefineReservedUnused(IOHIDInterface,  9);
OSMetaClassDefineReservedUnused(IOHIDInterface, 10);
OSMetaClassDefineReservedUnused(IOHIDInterface, 11);
OSMetaClassDefineReservedUnused(IOHIDInterface, 12);
OSMetaClassDefineReservedUnused(IOHIDInterface, 13);
OSMetaClassDefineReservedUnused(IOHIDInterface, 14);
OSMetaClassDefineReservedUnused(IOHIDInterface, 15);
OSMetaClassDefineReservedUnused(IOHIDInterface, 16);
OSMetaClassDefineReservedUnused(IOHIDInterface, 17);
OSMetaClassDefineReservedUnused(IOHIDInterface, 18);
OSMetaClassDefineReservedUnused(IOHIDInterface, 19);
OSMetaClassDefineReservedUnused(IOHIDInterface, 20);
OSMetaClassDefineReservedUnused(IOHIDInterface, 21);
OSMetaClassDefineReservedUnused(IOHIDInterface, 22);
OSMetaClassDefineReservedUnused(IOHIDInterface, 23);
OSMetaClassDefineReservedUnused(IOHIDInterface, 24);
OSMetaClassDefineReservedUnused(IOHIDInterface, 25);
OSMetaClassDefineReservedUnused(IOHIDInterface, 26);
OSMetaClassDefineReservedUnused(IOHIDInterface, 27);
OSMetaClassDefineReservedUnused(IOHIDInterface, 28);
OSMetaClassDefineReservedUnused(IOHIDInterface, 29);
OSMetaClassDefineReservedUnused(IOHIDInterface, 30);
OSMetaClassDefineReservedUnused(IOHIDInterface, 31);


#pragma clang diagnostic ignored "-Wunused-parameter"

#include <IOKit/IOUserServer.h>
//#include "HIDDriverKit/Implementation/IOKitUser/IOHIDInterface.h"

void IOHIDInterface::HandleReportPrivate(AbsoluteTime timestamp,
                                         IOMemoryDescriptor *report,
                                         IOHIDReportType type,
                                         UInt32 reportID,
                                         void *ctx)
{
    dispatch_workloop_sync({
        handleReportGated(timestamp, report, type, reportID, ctx);
    });
}

void IOHIDInterface::handleReportGated(AbsoluteTime timestamp,
                                       IOMemoryDescriptor *report,
                                       IOHIDReportType type,
                                       UInt32 reportID,
                                       void *ctx)
{
    IOBufferMemoryDescriptor *poolReport = NULL;
    DextDebugStats *stats;
    
    require(_opened && !_terminated, exit);
    
    require_action(_reportPool, exit, HIDServiceLogError("No report pool"));

    poolReport = OSDynamicCast(IOBufferMemoryDescriptor, _reportPool->getLastObject());

    stats = _debugStats ? (DextDebugStats *)_debugStats->getVirtualAddress() : NULL;
    
    if (!poolReport || report->getLength() > poolReport->getLength()) {
        HIDServiceLogDebug("Creating temporary buffer for report data");
        if (poolReport) {
            HIDServiceLogError("Report too large %d %d", (int)report->getLength(), (int)poolReport->getLength());
        }

        if (stats) {
            if ((stats->createdBuffers - stats->releasedBuffers) && (stats->createdBuffers - stats->releasedBuffers) % BUFFER_OUTSTANDING_WARNING == 0) {
                HIDServiceLogError("Large amount of outstanding buffers: %u %u", stats->reportAvailableCalls, stats->reportAvailableRuns);
            }
            stats->createdBuffers++;
            if (stats->createdBuffers % BUFFER_CREATE_WARNING == 0) {
                HIDServiceLogError("Significant amount of temporary report buffers created.");
            }
        }

        poolReport = IOBufferMemoryDescriptor::withOptions(kIOMemoryDirectionOutIn | kIOMemoryKernelUserShared, report->getLength(), page_size);
        require_action(poolReport, exit, HIDServiceLogError("BMD create"));
    } else {
        poolReport->retain();
        _reportPool->removeObject(_reportPool->getCount() - 1);
    }
    
    memset(poolReport->getBytesNoCopy(), 0, poolReport->getLength());
    
    report->prepare();
    report->readBytes(0, poolReport->getBytesNoCopy(), report->getLength());
    report->complete();
    
    IOHID_DEBUG(kIOHIDDebugCode_DK_Intf_HandleReport,
                getRegistryEntryID(),
                __OSAbsoluteTime(timestamp),
                reportID,
                report);
    
    if (stats) {
        stats->reportAvailableCalls++;
    }
    ReportAvailable(timestamp,
                    reportID,
                    (uint32_t)report->getLength(),
                    type,
                    poolReport,
                    (OSAction *)ctx);
    
    poolReport->release();
    
exit:
    return;
}

kern_return_t
IMPL(IOHIDInterface, AddReportToPool)
{
    __block IOReturn ret = kIOReturnError;
    
    dispatch_workloop_sync({
        ret = addReportToPoolGated(report);
    });
    
    return ret;
}

IOReturn IOHIDInterface::addReportToPoolGated(IOMemoryDescriptor *report)
{
    IOReturn ret = kIOReturnError;
    
    if (!_reportPool) {
        _reportPool = OSArray::withCapacity(1);
    }
    
    require_action(_reportPool, exit, ret = kIOReturnNoMemory);
    
    _reportPool->setObject(report);
    
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

kern_return_t
IMPL(IOHIDInterface, Open)
{
    __block bool result = false;
    
    dispatch_workloop_sync({
        result = openGated(forClient, options, action);
    });
    
    return result ? kIOReturnSuccess : kIOReturnError;
}

bool IOHIDInterface::openGated(IOService *forClient, IOOptionBits options, OSAction *action)
{
    bool result = false;
    InterruptReportAction reportAction;
    
    reportAction = OSMemberFunctionCast(InterruptReportAction, this,
                                        &IOHIDInterface::HandleReportPrivate);
    
    require(action, exit);
    
    require_action(!_opened, exit, result = true);
    
    OSSafeReleaseNULL(_reportAction);
    
    _reportAction = action;
    _reportAction->retain();
    
    _opened = open(forClient, options, reportAction, (void *)_reportAction);
    result = _opened;
    
exit:
    return result;
}

kern_return_t
IMPL(IOHIDInterface, Close)
{
    close(forClient, options);
    return kIOReturnSuccess;
}

kern_return_t
IMPL(IOHIDInterface, SetReport)
{
    return setReport(report, reportType, reportID, options);
}

kern_return_t
IMPL(IOHIDInterface, GetReport)
{
    return getReport(report, reportType, reportID, options);
}

kern_return_t
IMPL(IOHIDInterface, GetSupportedCookies)
{
    IOReturn ret = kIOReturnError;
    IOBufferMemoryDescriptor *md = NULL;
    UInt32 *buff = NULL;
    
    require_action(cookies, exit, ret = kIOReturnBadArgument);
    
    md = IOBufferMemoryDescriptor::withOptions(kIODirectionInOut |
                                               kIOMemoryKernelUserShared,
                                               _elementArray->getCount() * sizeof(UInt32));
    require_action(md, exit, ret = kIOReturnNoMemory);
    
    buff = (UInt32 *)md->getBytesNoCopy();
    
    for (unsigned int i = 0; i < _elementArray->getCount(); i++) {
        IOHIDElement *element = (IOHIDElement *)_elementArray->getObject(i);
        buff[i] = (UInt32)element->getCookie();
    }
    
    *cookies = md;
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

kern_return_t
IMPL(IOHIDInterface, SetElementValues)
{
    IOReturn ret = kIOReturnError;
    UInt8 *values = NULL;
    IOBufferMemoryDescriptor *md = NULL;
    
    md = OSDynamicCast(IOBufferMemoryDescriptor, elementValues);
    require_action(md && count, exit, ret = kIOReturnBadArgument);

    values = (UInt8 *)md->getBytesNoCopy();
    
    // Post the data to the device
    ret = _owner->postElementTransaction(values, (UInt32)md->getLength());
    require_noerr_action(ret, exit, HIDServiceLogError("postElementValues failed: 0x%x", ret));

exit:
    return ret;
}

kern_return_t
IMPL(IOHIDInterface, GetElementValues)
{
    IOReturn ret = kIOReturnError;
    UInt8 *values = NULL;
    IOHIDElementCookie *cookies = NULL;
    UInt32 totalSize = 0;
    UInt32 offset = 0;
    IOBufferMemoryDescriptor *md = NULL;
    uint32_t allocSize;
    
    md = OSDynamicCast(IOBufferMemoryDescriptor, elementValues);
    require_action(md && count, exit, ret = kIOReturnBadArgument);
    
    require(!os_mul_overflow(count, sizeof(IOHIDElementCookie), &allocSize), exit);
    
    values = (UInt8 *)md->getBytesNoCopy();

    // ignore -Wxnu-typed-allocators, as we're not storing pointers here,
    // no matter what the underlying type definition is
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wxnu-typed-allocators"
    cookies = (IOHIDElementCookie *)IONewData(IOHIDElementCookie, count);
    #pragma clang diagnostic pop
    require_action(cookies, exit, ret = kIOReturnNoMemory);
    
    memset(cookies, 0, count);
    
    // get the cookies from the buffer
    for (unsigned int i = 0; i < count; i++) {
        IOHIDElementValueHeader *header = NULL;
        IOHIDElementPrivate *element = NULL;
        UInt32 valueSize = 0;
        
        // make sure we are within bounds of md.
        assert(offset <= (elementValues->getLength() - sizeof(IOHIDElementValueHeader)));
        
        header = (IOHIDElementValueHeader *)values;
        
        element = (IOHIDElementPrivate *)_deviceElements->getObject(header->cookie);
        require_action(element, exit, {
            HIDLogError("No element for cookie %d", (unsigned int)header->cookie);
            ret = kIOReturnBadArgument;
        });
        
        valueSize = (UInt32)element->getByteSize();
        
        // size check
        totalSize += sizeof(IOHIDElementValueHeader) + valueSize;
        require_action(totalSize <= elementValues->getLength(), exit, {
            HIDLogError("IOHIDInterface GetElementValues totalSize: %d length: %d",
                        (unsigned int)totalSize, (int)elementValues->getLength());
            ret = kIOReturnBadArgument;
        });
        
        cookies[i] = (IOHIDElementCookie)header->cookie;
        values += sizeof(IOHIDElementValueHeader) + valueSize;
        offset += sizeof(IOHIDElementValueHeader) + valueSize;
    }
    
    ret = _owner->updateElementValues(cookies, count);
    require_noerr_action(ret, exit, HIDServiceLogError("updateElementValues failed: 0x%x", ret));
    
    values = (UInt8 *)md->getBytesNoCopy();
    totalSize = 0;
    
    // update the buffer with the element values
    for (unsigned int i = 0; i < count; i++) {
        IOHIDElementValueHeader *header = NULL;
        IOHIDElementPrivate *element = NULL;
        UInt32 valueSize = 0;
        header = (IOHIDElementValueHeader *)values;
        
        element = (IOHIDElementPrivate *)_deviceElements->getObject(header->cookie);
        if (!element) {
            HIDLog("No element for cookie %d", (unsigned int)header->cookie);
            continue;
        }
        
        valueSize = (UInt32)element->getByteSize();
        
        // size check
        totalSize += sizeof(IOHIDElementValueHeader) + valueSize;
        if (totalSize > elementValues->getLength()) {
            HIDLog("IOHIDInterface GetElementValues totalSize: %d length: %d",
                   (unsigned int)totalSize, (int)elementValues->getLength());
            continue;
        }
        
        bcopy(element->getDataValue()->getBytesNoCopy(), header->value, valueSize);
        
        values += sizeof(IOHIDElementValueHeader) + valueSize;
    }
    
exit:
    if (cookies) {
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wxnu-typed-allocators"
        IODeleteData(cookies, IOHIDElementCookie, count);
        #pragma clang diagnostic pop
    }
    
    return ret;
}

kern_return_t
IMPL(IOHIDInterface, SendDebugBuffer)
{
    if (debug) {
        _debugStats = debug->map();
    }
    return kIOReturnSuccess;
}

bool IOHIDInterface::serializeDebugState(void *ref __unused,
                                         OSSerialize *serializer)
{
    bool             result = false;
    OSDictionary   * dict = OSDictionary::withCapacity(4);
    OSNumber       * num;
    DextDebugStats   stats = {0, 0, 0, 0};

    require(dict, exit);

    if (_debugStats) {
        memcpy(&stats, (void *)_debugStats->getVirtualAddress(), sizeof(DextDebugStats));
    }

    if ((num = OSNumber::withNumber(stats.reportAvailableCalls, 32))) {
        dict->setObject("ReportAvailableCalls", num);
        OSSafeReleaseNULL(num);
    }
    if ((num = OSNumber::withNumber(stats.reportAvailableRuns, 32))) {
        dict->setObject("ReportAvailableRuns", num);
        OSSafeReleaseNULL(num);
    }
    if ((num = OSNumber::withNumber(stats.createdBuffers, 32))) {
        dict->setObject("CreatedBuffers", num);
        OSSafeReleaseNULL(num);
    }
    if ((num = OSNumber::withNumber(stats.releasedBuffers, 32))) {
        dict->setObject("ReleasedBuffers", num);
        OSSafeReleaseNULL(num);
    }

    result = dict->serialize(serializer);

exit:
    OSSafeReleaseNULL(dict);
    return result;
}
