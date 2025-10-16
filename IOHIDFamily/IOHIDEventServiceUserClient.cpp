/*
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
#include <AssertMacros.h>
#include "IOHIDEventServiceUserClient.h"
#include "IOHIDEventServiceQueue.h"
#include "IOHIDEventData.h"
#include "IOHIDEvent.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDDebug.h"
#include <sys/proc.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include "IOHIDFamilyTrace.h"
#include <pexpert/pexpert.h>

#define kQueueSizeMin   0
#define kQueueSizeFake  128
#define kQueueSizeMax   16384
#define kEventSizeMax   131072 // 128KB

#define kIOHIDSystemUserAccessServiceEntitlement "com.apple.hid.system.user-access-service"

//===========================================================================
// IOHIDEventServiceUserClient class

#define super IOUserClient2022

OSDefineMetaClassAndStructors(IOHIDEventServiceUserClient, IOUserClient2022)

//==============================================================================
// IOHIDEventServiceUserClient::sMethods
//==============================================================================
const IOExternalMethodDispatch2022 IOHIDEventServiceUserClient::sMethods[kIOHIDEventServiceUserClientNumCommands] = {
    { //    kIOHIDEventServiceUserClientOpen
	(IOExternalMethodAction) &IOHIDEventServiceUserClient::_open,
	1, 0,
    0, 0,
    false
    },
    { //    kIOHIDEventServiceUserClientClose
	(IOExternalMethodAction) &IOHIDEventServiceUserClient::_close,
	1, 0,
    0, 0,
    false
    },
    { //    kIOHIDEventServiceUserClientCopyEvent
	(IOExternalMethodAction) &IOHIDEventServiceUserClient::_copyEvent,
    2, kIOUCVariableStructureSize,
    0, kIOUCVariableStructureSize,
    false
    },
    { //    kIOHIDEventServiceUserClientSetElementValue
	(IOExternalMethodAction) &IOHIDEventServiceUserClient::_setElementValue,
	3, 0,
    0, 0,
    false
    },
    { //    kIOHIDEventServiceUserClientCopyMatchingEvent
    (IOExternalMethodAction) &IOHIDEventServiceUserClient::_copyMatchingEvent,
    0, kIOUCVariableStructureSize,
    0, kIOUCVariableStructureSize,
    false
    },
};

enum {
    kUserClientStateOpen  = 0x1,
    kUserClientStateClose = 0x2
};

//==============================================================================
// IOHIDEventServiceUserClient::getService
//==============================================================================
IOService * IOHIDEventServiceUserClient::getService( void )
{
    return _owner;
}

//==============================================================================
// IOHIDEventServiceUserClient::clientClose
//==============================================================================
IOReturn IOHIDEventServiceUserClient::clientClose( void )
{
    terminate();
    
    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::registerNotificationPort
//==============================================================================
IOReturn IOHIDEventServiceUserClient::registerNotificationPort(
                            mach_port_t                 port, 
                            UInt32                      type,
                            UInt32                      refCon __unused )
{

    IOReturn result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    
    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceUserClient::registerNotificationPortGated), port, (void *)(intptr_t)type);
    
exit:

    return result;
}


IOReturn IOHIDEventServiceUserClient::registerNotificationPortGated(mach_port_t port, UInt32 type __unused, UInt32 refCon __unused)
{
    releaseNotificationPort(_queuePort);
    _queuePort = port;
    
    if (_queue) {
        _queue->setNotificationPort(port);
    }
    
    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::clientMemoryForType
//==============================================================================
IOReturn IOHIDEventServiceUserClient::clientMemoryForType(
                                                               UInt32                      type __unused,
                                                               IOOptionBits *              options,
                                                               IOMemoryDescriptor **       memory )
{
  
    IOReturn result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    
    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceUserClient::clientMemoryForTypeGated), options, memory);
  
exit:
  
    return result;
}

//==============================================================================
// IOHIDEventServiceUserClient::clientMemoryForType
//==============================================================================
IOReturn IOHIDEventServiceUserClient::clientMemoryForTypeGated(
                            IOOptionBits *              options,
                            IOMemoryDescriptor **       memory )
{
    IOReturn ret = kIOReturnNoMemory;
            
    if ( _queue ) {
        IOMemoryDescriptor * memoryToShare = _queue->getMemoryDescriptor();
    
        // if we got some memory
        if (memoryToShare)
        {
            // Memory will be released by user client
            // when last map is destroyed.

            memoryToShare->retain();

            ret = kIOReturnSuccess;
        }
        
        // set the result
        *options = 0;
        *memory  = memoryToShare;
    }
        
    return ret;
}



//==============================================================================
// IOHIDEventServiceUserClient::externalMethod
//==============================================================================
IOReturn IOHIDEventServiceUserClient::externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * args)
{
    ExternalMethodGatedArguments gatedArguments = {selector, args};
    IOReturn result;

    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceUserClient::externalMethodGated), &gatedArguments);

    return result;
}

//==============================================================================
// IOHIDEventServiceUserClient::externalMethodGated
//==============================================================================
IOReturn IOHIDEventServiceUserClient::externalMethodGated(ExternalMethodGatedArguments * arguments)
{
    IOReturn result = kIOReturnOffline;
  
    require(!isInactive(), exit);

    result = dispatchExternalMethod(arguments->selector, arguments->arguments, sMethods, sizeof(sMethods) / sizeof(sMethods[0]), this, NULL);
  
exit:
    return result;
}


//==============================================================================
// IOHIDEventServiceUserClient::initWithTask
//==============================================================================
bool IOHIDEventServiceUserClient::initWithTask(task_t owningTask, void * security_id, UInt32 type)
{
    bool result = false;
    OSObject* entitlement = NULL;
    
    require_action(super::initWithTask(owningTask, security_id, type), exit, HIDLogError("failed"));
    
    entitlement = copyClientEntitlement(owningTask, kIOHIDSystemUserAccessServiceEntitlement);
    if (entitlement) {
        result = (entitlement == kOSBooleanTrue);
        entitlement->release();
    }
    if (!result) {
        proc_t      process;
        process = (proc_t)get_bsdtask_info(owningTask);
        char name[255];
        bzero(name, sizeof(name));
        proc_name(proc_pid(process), name, sizeof(name));
        HIDServiceLogError("%s is not entitled", name);
        goto exit;
    }

    _owner        = NULL;
    _commandGate  = NULL;
    _state        = 0;
    _queue        = NULL;
    
exit:
    return result;
}

//==============================================================================
// IOHIDEventServiceUserClient::start
//==============================================================================
bool IOHIDEventServiceUserClient::start( IOService * provider )
{
    OSObject *    object;
    OSNumber *    num;
    uint32_t      queueSize = kQueueSizeMax;
    IOWorkLoop *  workLoop;
    boolean_t     result = false;
    OSSerializer * debugStateSerializer;
    uint32_t      forceNotifyUsagePair = 0;
    uint32_t      queueSizeOverride = 0;
    uint32_t      qOptions = 0;
  
    require (super::start(provider), exit);
  
    _owner = OSDynamicCast(IOHIDEventService, provider);
    require (_owner, exit);

    _owner->retain();

    // If the provider's usage pair matches this boot arg, set force notify flag on queue.
    if (PE_parse_boot_argn("hidq_force_usage_pair", &forceNotifyUsagePair, sizeof(forceNotifyUsagePair))) {
        uint32_t usagePage  = (forceNotifyUsagePair >> 16) & 0xffff;
        uint32_t usage      = forceNotifyUsagePair & 0xffff;

        if (usagePage == _owner->getPrimaryUsagePage() && usage == _owner->getPrimaryUsage()) {
            qOptions |= kIOHIDEventServiceQueueOptionNotificationForce;
        }
    }

    // Use property for queue size, if it exists.
    object = provider->copyProperty(kIOHIDEventServiceQueueSize);
    num = OSDynamicCast(OSNumber, object);
    if ( num ) {
        queueSize = num->unsigned32BitValue();
    }
    OSSafeReleaseNULL(object);

    // Use boot-arg for queue size - takes precedence over property.
    if (PE_parse_boot_argn("hidq_size", &queueSizeOverride, sizeof(queueSizeOverride)) && queueSizeOverride) {
        queueSize = queueSizeOverride;
        provider->setProperty(kIOHIDEventServiceQueueSize, queueSizeOverride, 32);
    }

    if ( queueSize ) {
        _queue = IOHIDEventServiceQueue::withCapacity(this, queueSize, qOptions);
        require(_queue, exit);
    }
  
    workLoop = getWorkLoop();
    require(workLoop, exit);
  
  
    _commandGate = IOCommandGate::commandGate(this);
    require(_commandGate, exit);
    require(workLoop->addEventSource(_commandGate) == kIOReturnSuccess, exit);
  
    debugStateSerializer = OSSerializer::forTarget(this, OSMemberFunctionCast(OSSerializerCallback, this, &IOHIDEventServiceUserClient::serializeDebugState));
    if (debugStateSerializer) {
        setProperty("DebugState", debugStateSerializer);
        debugStateSerializer->release();
    }

    setProperty(kIOUserClientDefaultLockingKey,                           kOSBooleanTrue);
    setProperty(kIOUserClientDefaultLockingSetPropertiesKey,              kOSBooleanTrue);
    setProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey, kOSBooleanFalse);
    setProperty(kIOUserClientEntitlementsKey,                             kOSBooleanFalse);
 
    result = true;

exit:
  
    return result;
}

void IOHIDEventServiceUserClient::stop( IOService * provider )
{
    close();
    
    IOWorkLoop * workLoop = getWorkLoop();
  
    if (workLoop && _commandGate) {
        workLoop->removeEventSource(_commandGate);
    }

    
    releaseNotificationPort(_queuePort);

    super::stop(provider);
}

//==============================================================================
// IOHIDEventServiceUserClient::_open
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_open(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference __unused,
                                IOExternalMethodArguments *     arguments)
{
    return target->open((IOOptionBits)arguments->scalarInput[0]);
}

//==============================================================================
// IOHIDEventServiceUserClient::open
//==============================================================================
IOReturn IOHIDEventServiceUserClient::open(IOOptionBits options)
{
    if (!_owner) {
        return kIOReturnOffline;
    }
    
    if (_state == kUserClientStateOpen) {
        return kIOReturnStillOpen;
    }
    
    _options = options;
    
    if (!_owner->open(  this,
                        options,
                        NULL, 
                        OSMemberFunctionCast(IOHIDEventService::Action, 
                        this, &IOHIDEventServiceUserClient::eventServiceCallback)) ) {
       return kIOReturnExclusiveAccess;
    }
    
    _state = kUserClientStateOpen;
    
    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::_close
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_close(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference __unused,
                                IOExternalMethodArguments *     arguments __unused)
{
    return target->close();
}

//==============================================================================
// IOHIDEventServiceUserClient::close
//==============================================================================
IOReturn IOHIDEventServiceUserClient::close()
{
    
    
    if (_owner && _state == kUserClientStateOpen) {
        _owner->close(this, _options | kIOHIDOpenedByEventSystem);
        _state = kUserClientStateClose;
    }

    return kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::_copyEvent
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_copyEvent(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference __unused, 
                                IOExternalMethodArguments *     arguments)
{
    IOHIDEvent *    inEvent     = NULL;
    IOHIDEvent *    outEvent    = NULL;
    IOReturn        ret         = kIOReturnError;
    IOByteCount     length      = 0;
    IOMemoryMap *   mmap        = NULL;
    void *          outData     = NULL;
    size_t          outSize     = 0;
    
    require_action(arguments->structureInputSize < kEventSizeMax, exit, ret = kIOReturnNoMemory);

    if ( arguments->structureInput && arguments->structureInputSize) {
        inEvent = IOHIDEvent::withBytes(arguments->structureInput, arguments->structureInputSize);
    }

    if ( arguments->structureOutputDescriptor ) {
        // Memory mapping could fail if process is terminating or userland is shutting down
        require_noerr((ret = arguments->structureOutputDescriptor->prepare()), exit);

        mmap = arguments->structureOutputDescriptor->map();
        require_action(mmap, map_fail, ret = kIOReturnNoMemory);

        outData = (void *)mmap->getVirtualAddress();
        outSize = arguments->structureOutputDescriptor->getLength();
    } else if ( arguments->structureOutput ) {
        outData = arguments->structureOutput;
        outSize = arguments->structureOutputSize;
    } else {
        HIDLogError("_copyEvent: No output data");
        goto exit;
    }

    do { 
        ret = target->copyEvent((IOHIDEventType)arguments->scalarInput[0], inEvent, &outEvent, (IOOptionBits)arguments->scalarInput[1]);
        
        if (ret) {
            break;
        }
            
        length = outEvent->getLength();
        
        if ( length > outSize ) {
            HIDLogError("event length:%d expected:%ld", (unsigned int)length, outSize);
            ret = kIOReturnBadArgument;
            break;
        }

        outEvent->readBytes(outData, outSize);
        if ( arguments->structureOutputDescriptor ) {
            arguments->structureOutputDescriptorSize = (uint32_t)length;
        } else {
            arguments->structureOutputSize = (uint32_t)length;
        }
    } while ( 0 );

map_fail:
    if ( arguments->structureOutputDescriptor ) {
        OSSafeReleaseNULL(mmap);
        arguments->structureOutputDescriptor->complete();
    }

exit:
    if ( inEvent )
        inEvent->release();
    
    if ( outEvent )
        outEvent->release();
        
    return ret;
}

//==============================================================================
// IOHIDEventServiceUserClient::copyEvent
//==============================================================================
IOReturn IOHIDEventServiceUserClient::copyEvent(IOHIDEventType type, IOHIDEvent * matching, IOHIDEvent ** event, IOOptionBits options)
{
    if (!event) {
        return kIOReturnBadArgument;
    }
    if (!_owner) {
        return kIOReturnOffline;
    }
    if (_state != kUserClientStateOpen) {
        return kIOReturnNotOpen;
    }

    *event = _owner->copyEvent(type, matching, options);
   
    return *event == NULL ? kIOReturnUnsupported : kIOReturnSuccess;
}

//==============================================================================
// IOHIDEventServiceUserClient::_setElementValue
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_setElementValue(
                                IOHIDEventServiceUserClient *   target, 
                                void *                          reference __unused,
                                IOExternalMethodArguments *     arguments)
{

    return target->setElementValue((UInt32)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2]);
}

//==============================================================================
// IOHIDEventServiceUserClient::setElementValue
//==============================================================================
IOReturn IOHIDEventServiceUserClient::setElementValue(UInt32 usagePage, UInt32 usage, UInt32 value)
{
    if (_owner && _state == kUserClientStateOpen) {
        return _owner->setElementValue(usagePage, usage, value);
    }
    return kIOReturnNoDevice;
}

//==============================================================================
// IOHIDEventServiceUserClient::_copyMatchingEvent
//==============================================================================
IOReturn IOHIDEventServiceUserClient::_copyMatchingEvent(
                                                 IOHIDEventServiceUserClient *   target,
                                                 void *                          reference __unused,
                                                 IOExternalMethodArguments *     arguments)
{
    IOReturn ret = kIOReturnError;
    OSObject *obj = NULL;
    OSDictionary *matching = NULL;
    OSData *result = NULL;
    
    require_action(arguments->structureVariableOutputData, exit, ret = kIOReturnBadArgument);
    
    if (arguments->structureInput && arguments->structureInputSize) {
        obj = OSUnserializeXML((const char *)arguments->structureInput,
                               arguments->structureInputSize);
        require_action(obj, exit, ret = kIOReturnBadArgument);
        
        matching = OSDynamicCast(OSDictionary, obj);
        require_action(matching, exit, ret = kIOReturnBadArgument);
    }
    
    ret = target->copyMatchingEvent(matching, &result);
    require(ret == kIOReturnSuccess && result, exit);
    
    // result will be released for us
    *arguments->structureVariableOutputData = result;
    
exit:
    OSSafeReleaseNULL(obj);
    
    return ret;
}

//==============================================================================
// IOHIDEventServiceUserClient::copyMatchingEvent
//==============================================================================
IOReturn IOHIDEventServiceUserClient::copyMatchingEvent(OSDictionary *matching, OSData **eventData)
{
    IOReturn ret = kIOReturnError;
    IOHIDEvent *event = NULL;
    
    require_action(eventData, exit, ret = kIOReturnBadArgument);
    require_action(_owner && _state == kUserClientStateOpen, exit, ret = kIOReturnNotOpen);
    
    event = _owner->copyMatchingEvent(matching);
    require_action(event, exit, ret = kIOReturnUnsupported);
    
    *eventData = event->createBytes();
    require_action(*eventData, exit, ret = kIOReturnNoMemory);
    
    ret = kIOReturnSuccess;
    
exit:
    OSSafeReleaseNULL(event);
    
    return ret;
}

//==============================================================================
// IOHIDEventServiceUserClient::didTerminate
//==============================================================================
bool IOHIDEventServiceUserClient::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{

    close ();

    return super::didTerminate(provider, options, defer);
}

//==============================================================================
// IOHIDEventServiceUserClient::free
//==============================================================================
void IOHIDEventServiceUserClient::free()
{
    OSSafeReleaseNULL(_queue);
    OSSafeReleaseNULL(_owner);
    OSSafeReleaseNULL(_commandGate);
  
    super::free();
}

//==============================================================================
// IOHIDEventServiceUserClient::setProperties
//==============================================================================
IOReturn IOHIDEventServiceUserClient::setProperties( OSObject * properties )
{
    return _owner ? _owner->setProperties(properties) : kIOReturnOffline;
}


//==============================================================================
// IOHIDEventServiceUserClient::eventServiceCallback
//==============================================================================
void IOHIDEventServiceUserClient::eventServiceCallback(
                                IOHIDEventService *             sender __unused,
                                void *                          context __unused,
                                IOHIDEvent *                    event, 
                                IOOptionBits                    options __unused)
{
    if (!_queue || _state != kUserClientStateOpen) {
        return;
    }

#if 0
    if (event && (event->getLatency(kMillisecondScale) > 500)) {
        IOLog("HID dispatch 0x%llx[%d]- high latency %llums", getRegistryEntryID(), (int)event->getType(), event->getLatency(kMillisecondScale));
    }
#endif
    
    _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDEventServiceUserClient::enqueueEventGated), event);
  
}

//==============================================================================
// IOHIDEventServiceUserClient::enqueueEventGated
//==============================================================================
void IOHIDEventServiceUserClient::enqueueEventGated( IOHIDEvent * event)
{
  //enqueue the event
    if (_queue) {
        ++_eventCount;
        _lastEventTime = mach_continuous_time();
        _lastEventType = event->getType();
        Boolean result = _queue->enqueueEvent(event);
        if (result == false) {
            _lastDroppedEventTime = _lastEventTime;
            ++_droppedEventCount;
            IOHID_DEBUG(kIOHIDDebugCode_HIDEventServiceEnqueueFail, event->getTimeStampOfType(kIOHIDEventTimestampTypeDefault), event->getOptions() & kIOHIDEventOptionContinuousTime, 0, 0);
        }
    }
}


//====================================================================================================
// IOHIDEventServiceUserClient::serializeDebugState
//====================================================================================================
bool   IOHIDEventServiceUserClient::serializeDebugState(void * ref __unused, OSSerialize * serializer) {
    bool          result = false;
    uint64_t      currentTime, deltaTime;
    uint64_t      nanoTime;
    OSDictionary  *debugDict = OSDictionary::withCapacity(6);
    OSNumber      *num;
    
    require(debugDict, exit);
    
    currentTime =  mach_continuous_time();
    
    if (_queue) {
        debugDict->setObject("EventQueue", _queue);
    }

    if (_eventCount) {
        num = OSNumber::withNumber(_eventCount, 64);
        if (num) {
            debugDict->setObject("EnqueueEventCount", num);
            OSSafeReleaseNULL(num);
        }
    }
    if (_lastEventTime) {
        deltaTime = AbsoluteTime_to_scalar(&currentTime) - AbsoluteTime_to_scalar(&(_lastEventTime));
        absolutetime_to_nanoseconds(deltaTime, &nanoTime);
        num = OSNumber::withNumber(nanoTime, 64);
        if (num) {
            debugDict->setObject("LastEventTime", num);
            OSSafeReleaseNULL(num);
        }
    }
    if (_lastEventType) {
        num = OSNumber::withNumber(_lastEventType, 32);
        if (num) {
            debugDict->setObject("LastEventType", num);
            OSSafeReleaseNULL(num);
        }
    }
    if (_lastDroppedEventTime) {
        deltaTime = AbsoluteTime_to_scalar(&currentTime) - AbsoluteTime_to_scalar(&(_lastDroppedEventTime));
        absolutetime_to_nanoseconds(deltaTime, &nanoTime);
        num = OSNumber::withNumber(nanoTime, 64);
        if (num) {
            debugDict->setObject("LastDroppedEventTime", num);
            OSSafeReleaseNULL(num);
        }
    }
    if (_droppedEventCount) {
        num = OSNumber::withNumber(_droppedEventCount, 32);
        if (num) {
            debugDict->setObject("DroppedEventCount", num);
            OSSafeReleaseNULL(num);
        }
    }
    result = debugDict->serialize(serializer);
    debugDict->release();

exit:
    return result;
}
