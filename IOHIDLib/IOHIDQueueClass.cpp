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
#define CFRUNLOOP_NEW_API 1

#include <CoreFoundation/CFMachPort.h>
//#include <IOKit/hid/IOHIDLib.h>
//#include <unistd.h>

#include "IOHIDQueueClass.h"
#include "IOHIDLibUserClient.h"

__BEGIN_DECLS
#include <IOKit/IODataQueueClient.h>
#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
#include <System/libkern/OSCrossEndian.h>
__END_DECLS

#define ownerCheck() do {		\
    if (!fOwningDevice)			\
	return kIOReturnNoDevice;	\
} while (0)

#define connectCheck() do {		\
    if ((!fOwningDevice) ||		\
    	(!fOwningDevice->fConnection))	\
	return kIOReturnNoDevice;	\
} while (0)

#define openCheck() do {            \
    if (!fOwningDevice ||           \
        !fOwningDevice->fIsOpen ||  \
        !fIsCreated)                \
        return kIOReturnNotOpen;    \
} while (0)

#define terminatedCheck() do {		\
    if ((!fOwningDevice) ||		\
         (fOwningDevice->fIsTerminated))\
        return kIOReturnNotAttached;	\
} while (0)    

#define allChecks() do {		\
    ownerCheck();           \
    connectCheck();			\
    openCheck();			\
    terminatedCheck();			\
} while (0)

#define deviceInitiatedChecks() do {	\
    connectCheck();			\
    openCheck();			\
    terminatedCheck();			\
} while (0)

IOHIDQueueClass::IOHIDQueueClass()
: IOHIDIUnknown(NULL)
{
    fHIDQueue.pseudoVTable = (IUnknownVTbl *)  &sHIDQueueInterfaceV1;
    fHIDQueue.obj = this;
    
    fAsyncPort              = MACH_PORT_NULL;
    fAsyncPortIsCreated     = false;
    fIsCreated              = false;
    fIsStopped              = false;
    fEventCallback          = NULL;
    fEventTarget            = NULL;
    fEventRefcon            = NULL;
    fQueueRef               = 0;
    fQueueMappedMemory      = NULL;
    fQueueMappedMemorySize  = 0;
    fQueueEntrySizeChanged  = false;
}

IOHIDQueueClass::~IOHIDQueueClass()
{
    // if we are owned, detatch
    if (fOwningDevice)
        fOwningDevice->detachQueue(this);
		
	if (fAsyncPort && fAsyncPortIsCreated)
        mach_port_deallocate(mach_task_self(), fAsyncPort);
}

HRESULT IOHIDQueueClass::queryInterface(REFIID /*iid*/, void **	/*ppv*/)
{
    // ��� should we return our parent if that type is asked for???
    
    return E_NOINTERFACE;
}

IOReturn IOHIDQueueClass::
createAsyncEventSource(CFRunLoopSourceRef *source)
{
    IOReturn ret;
    CFMachPortRef cfPort;
    CFMachPortContext context;
    Boolean shouldFreeInfo;

    if (!fAsyncPort) {     
        ret = createAsyncPort(0);
        if (kIOReturnSuccess != ret)
            return ret;
    }

    context.version = 1;
    context.info = this;
    context.retain = NULL;
    context.release = NULL;
    context.copyDescription = NULL;

    cfPort = CFMachPortCreateWithPort(NULL, fAsyncPort,
                (CFMachPortCallBack) IOHIDQueueClass::queueEventSourceCallback,
                &context, &shouldFreeInfo);
    if (!cfPort)
        return kIOReturnNoMemory;
    
    fCFSource = CFMachPortCreateRunLoopSource(NULL, cfPort, 0);
    CFRelease(cfPort);
    if (!fCFSource)
        return kIOReturnNoMemory;

    if (source)
        *source = fCFSource;

    return kIOReturnSuccess;
}

CFRunLoopSourceRef IOHIDQueueClass::getAsyncEventSource()
{
    return fCFSource;
}

/* CFMachPortCallBack */
void IOHIDQueueClass::queueEventSourceCallback(CFMachPortRef cfPort, mach_msg_header_t *msg, CFIndex size, void *info){
    
    IOHIDQueueClass *queue = (IOHIDQueueClass *)info;
    
    if ( queue ) {
        if ( queue->fEventCallback ) {
                
            (queue->fEventCallback)(queue->fEventTarget, 
                            kIOReturnSuccess, 
                            queue->fEventRefcon, 
                            (void *)&queue->fHIDQueue);
        }
    }
}


IOReturn IOHIDQueueClass::createAsyncPort(mach_port_t *port)
{
    IOReturn	ret;
    mach_port_t asyncPort;
    connectCheck();
    
    ret = IOCreateReceivePort(kOSAsyncCompleteMessageID, &asyncPort);
    if (kIOReturnSuccess == ret) {
		fAsyncPortIsCreated = true;
		
        if (port)
            *port = asyncPort;

		return setAsyncPort(asyncPort);
    }

    return ret;
}

mach_port_t IOHIDQueueClass::getAsyncPort()
{
    return fAsyncPort;
}

IOReturn IOHIDQueueClass::setAsyncPort(mach_port_t port)
{
    if ( !port )
		return kIOReturnError;

	fAsyncPort = port;

	if (!fIsCreated)
		return kIOReturnSuccess;
		
	natural_t				asyncRef[1];
	int						input[1];
	mach_msg_type_number_t 	len = 0;

	input[0] = (int) fQueueRef;
	// async kIOHIDLibUserClientSetQueueAsyncPort, kIOUCScalarIScalarO, 1, 0
	return io_async_method_scalarI_scalarO(
			fOwningDevice->fConnection, fAsyncPort, asyncRef, 1,
			kIOHIDLibUserClientSetQueueAsyncPort, input, 1, NULL, &len);
}


IOReturn IOHIDQueueClass::create (UInt32 flags, UInt32 depth)
{
    IOReturn ret = kIOReturnSuccess;

    connectCheck();
    
    // ���todo, check flags/depth to see if different (might need to recreate?)
    if (fIsCreated)
        return kIOReturnSuccess;

    // sent message to create queue
    //  kIOHIDLibUserClientCreateQueue, kIOUCScalarIScalarO, 2, 1
    int args[6], i = 0;
    args[i++] = flags;
    args[i++] = depth;
    mach_msg_type_number_t len = 1;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientCreateQueue, args, i, (int *) &fQueueRef, &len);
    if (ret != kIOReturnSuccess)
        return ret;
    
    // we have created it
    fIsCreated = true;
    fCreatedFlags = flags;
    fCreatedDepth = depth;
    
    // if we have async port, set it on other side
    if (fAsyncPort)
    {
        ret = setAsyncPort(fAsyncPort);
        if (ret != kIOReturnSuccess) {
            (void) this->dispose();
            return ret;
        }
    }
        
    return ret;
}

IOReturn IOHIDQueueClass::dispose()
{
    IOReturn ret = kIOReturnSuccess;

    allChecks();

    // ���� TODO unmap memory when that call works
    if ( fQueueMappedMemory )
    {
        ret = IOConnectUnmapMemory (fOwningDevice->fConnection, 
                                    fQueueRef, 
                                    mach_task_self(), 
                                    (vm_address_t)fQueueMappedMemory);
        fQueueMappedMemory = NULL;
        fQueueMappedMemorySize = 0;
    }    


    // sent message to dispose queue
    mach_msg_type_number_t len = 0;

    //  kIOHIDLibUserClientDisposeQueue, kIOUCScalarIScalarO, 1, 0
    int args[6], i = 0;
    args[i++] = fQueueRef;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientDisposeQueue, args, i, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;

    // mark it dead
    fIsCreated = false;
        
    fQueueRef = 0;
    
    return kIOReturnSuccess;
}

/* Any number of hid elements can feed the same queue */
IOReturn IOHIDQueueClass::addElement (
                            IOHIDElementCookie elementCookie,
                            UInt32 flags)
{
    IOReturn    ret = kIOReturnSuccess;

    allChecks();

    //  kIOHIDLibUserClientAddElementToQueue, kIOUCScalarIScalarO, 3, 0
    int args[6], i = 0, sizeChange=0;
    args[i++] = fQueueRef;
    args[i++] = (int) elementCookie;
    args[i++] = flags;
    mach_msg_type_number_t len = 1;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientAddElementToQueue, args, i, &sizeChange, &len);
    if (ret != kIOReturnSuccess)
        return ret;

    fQueueEntrySizeChanged = sizeChange;

    return kIOReturnSuccess;
}

IOReturn IOHIDQueueClass::removeElement (IOHIDElementCookie elementCookie)
{
    IOReturn ret = kIOReturnSuccess;

    allChecks();

    //  kIOHIDLibUserClientRemoveElementFromQueue, kIOUCScalarIScalarO, 2, 0
    int args[6], i = 0, sizeChange=0;
    args[i++] = fQueueRef;
    args[i++] = (int) elementCookie;
    mach_msg_type_number_t len = 1;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientRemoveElementFromQueue, args, i, &sizeChange, &len);
    if (ret != kIOReturnSuccess)
        return ret;

    fQueueEntrySizeChanged = sizeChange;

    return kIOReturnSuccess;
}

Boolean IOHIDQueueClass::hasElement (IOHIDElementCookie elementCookie)
{
    int returnHasElement = 0;

    // cannot do allChecks(), since return is a Boolean
    if (((!fOwningDevice) ||
    	(!fOwningDevice->fConnection)) ||
        (!fIsCreated))
        return false;

    //  kIOHIDLibUserClientQueueHasElement, kIOUCScalarIScalarO, 2, 1
    int args[6], i = 0;
    args[i++] = fQueueRef;
    args[i++] = (int) elementCookie;
    mach_msg_type_number_t len = 1;
    IOReturn ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientQueueHasElement, args, 
            i, &returnHasElement, &len);
    if (ret != kIOReturnSuccess)
        return false;

    return returnHasElement;
}


/* start/stop data delivery to a queue */
IOReturn IOHIDQueueClass::start ()
{
    IOReturn ret = kIOReturnSuccess;
    
    allChecks();
    
    // if the queue size changes, we will need to dispose of the 
    // queue mapped memory
    if ( fQueueEntrySizeChanged && fQueueMappedMemory )
    {
        ret = IOConnectUnmapMemory (fOwningDevice->fConnection, 
                                    fQueueRef, 
                                    mach_task_self(), 
                                    (vm_address_t)fQueueMappedMemory);
        fQueueMappedMemory      = NULL;
        fQueueMappedMemorySize  = 0;
    }    

    //  kIOHIDLibUserClientStartQueue, kIOUCScalarIScalarO, 1, 0
    int args[6], i = 0;
    args[i++] = fQueueRef;
    mach_msg_type_number_t len = 0;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientStartQueue, args, i, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;
    
    fIsStopped = false;

    // get the queue shared memory
    if ( !fQueueMappedMemory )
    {
        vm_address_t address = nil;
        vm_size_t size = 0;
        
        ret = IOConnectMapMemory (	fOwningDevice->fConnection, 
                                    fQueueRef, 
                                    mach_task_self(), 
                                    &address, 
                                    &size, 
                                    kIOMapAnywhere	);
        if (ret == kIOReturnSuccess)
        {
            fQueueMappedMemory = (IODataQueueMemory *) address;
            fQueueMappedMemorySize = size;
            fQueueEntrySizeChanged  = false;
        }
    }

    return kIOReturnSuccess;
}

IOReturn IOHIDQueueClass::stop ()
{
    IOReturn ret = kIOReturnSuccess;

    allChecks();

    //  kIOHIDLibUserClientStopQueue, kIOUCScalarIScalarO, 1, 0
    int args[6], i = 0;
    args[i++] = fQueueRef;
    mach_msg_type_number_t len = 0;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientStopQueue, args, i, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;
        
    fIsStopped = true;
        
    // ��� TODO after we stop the queue, we should empty the queue here, in user space
    // (to be consistant with setting the head from user space)
    
    return kIOReturnSuccess;
}

/* read next event from a queue */
/* maxtime, if non-zero, limits read events to those that occured */
/*   on or before maxTime */
/* timoutMS is the timeout in milliseconds, a zero timeout will cause */
/*	this call to be non-blocking (returning queue empty) if there */
/*	is a NULL callback, and blocking forever until the queue is */
/*	non-empty if their is a valid callback */
IOReturn IOHIDQueueClass::getNextEvent (
                        IOHIDEventStruct *	event,
                        AbsoluteTime		maxTime,
                        UInt32 			timeoutMS)
{
    IOReturn ret = kIOReturnSuccess;
    
    allChecks();
    
    if ( !fQueueMappedMemory )
        return kIOReturnNoMemory;

    // check entry size
    IODataQueueEntry *  nextEntry = IODataQueuePeek(fQueueMappedMemory);
    UInt32              entrySize;

	// if queue empty, then stop
	if (nextEntry == NULL)
		return kIOReturnUnderrun;

    entrySize = nextEntry->size;
    ROSETTA_ONLY(
        entrySize = OSSwapInt32(entrySize);
    );

    UInt32 dataSize = sizeof(IOHIDElementValue);
    
    // check size of next entry
    // Make sure that it is not smaller than IOHIDElementValue
    if (entrySize < sizeof(IOHIDElementValue))
        printf ("IOHIDQueueClass: Queue size mismatch (%ld, %ld)\n", entrySize, sizeof(IOHIDElementValue));
    
    // dequeue the item
//    printf ("IOHIDQueueClass::getNextEvent about to dequeue\n");
    ret = IODataQueueDequeue(fQueueMappedMemory, NULL, &dataSize);
//    printf ("IODataQueueDequeue result %lx\n", (UInt32) ret);
    

    // if we got an entry
    if (ret == kIOReturnSuccess && nextEntry)
    {
        IOHIDElementValue * nextElementValue = (IOHIDElementValue *) &(nextEntry->data);
        
        void *              longValue = 0;
        UInt32              longValueSize = 0;
        SInt32              value = 0;
        UInt64              timestamp = 0;
        IOHIDElementCookie  cookie = 0;

        // check size of result
        if ( dataSize >= sizeof(IOHIDElementValue))
        {
            
            timestamp = *(UInt64 *)& nextElementValue->timestamp;
            cookie = nextElementValue->cookie;

			ROSETTA_ONLY(
                timestamp   = OSSwapInt64(timestamp);
                cookie      = (IOHIDElementCookie)OSSwapInt32((UInt32)cookie);
            );
            
            if (dataSize == sizeof(IOHIDElementValue))
            {
                value = nextElementValue->value[0];

                ROSETTA_ONLY(
                    value		= OSSwapInt32(value);
                );
            }
            else
            {
                longValueSize = fOwningDevice->getElementByteSize(cookie);
                longValue = malloc( longValueSize );
                bzero(longValue, longValueSize);
                
                // *** FIX ME ***
                // Since we are getting mapped memory, we should probably
                // hold a shared lock
                fOwningDevice->convertWordToByte(nextElementValue->value, (UInt8 *)longValue, longValueSize);
            }
            
        }
        else
            printf ("IOHIDQueueClass: Queue size mismatch (%ld, %ld)\n", dataSize, sizeof(IOHIDElementValue));
        
        // copy the data to the event struct
        event->type = fOwningDevice->getElementType(cookie);
        event->elementCookie = cookie;
        event->value = value;
        *(UInt64 *)& event->timestamp = timestamp;
        event->longValueSize = longValueSize;
        event->longValue = longValue;
    }
    
    
    return ret;
}


/* set a callback for notification when queue transistions from non-empty */
/* callback, if non-NULL is a callback to be called when data is */
/*  inserted to the queue  */
/* callbackTarget and callbackRefcon are passed to the callback */
IOReturn IOHIDQueueClass::setEventCallout (
                        IOHIDCallbackFunction 	callback,
                        void * 			callbackTarget,
                        void *			callbackRefcon)
{
    fEventCallback = callback;
    fEventTarget = callbackTarget;
    fEventRefcon = callbackRefcon;
    
    return kIOReturnSuccess;
}


/* Get the current notification callout */
IOReturn IOHIDQueueClass::getEventCallout (
                        IOHIDCallbackFunction * 	outCallback,
                        void **                     outCallbackTarget,
                        void **                     outCallbackRefcon)
{

    if (outCallback)
        *outCallback        = fEventCallback;
        
    if (outCallbackTarget)
        *outCallbackTarget  = fEventTarget;
        
    if (outCallbackRefcon)
        *outCallbackRefcon  = fEventRefcon;
    
    return kIOReturnSuccess;
}


IOHIDQueueInterface IOHIDQueueClass::sHIDQueueInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDQueueClass::queueCreateAsyncEventSource,
    &IOHIDQueueClass::queueGetAsyncEventSource,
    &IOHIDQueueClass::queueCreateAsyncPort,
    &IOHIDQueueClass::queueGetAsyncPort,
    &IOHIDQueueClass::queueCreate,
    &IOHIDQueueClass::queueDispose,
    &IOHIDQueueClass::queueAddElement,
    &IOHIDQueueClass::queueRemoveElement,
    &IOHIDQueueClass::queueHasElement,
    &IOHIDQueueClass::queueStart,
    &IOHIDQueueClass::queueStop,
    &IOHIDQueueClass::queueGetNextEvent,
    &IOHIDQueueClass::queueSetEventCallout,
    &IOHIDQueueClass::queueGetEventCallout,
};

// Methods for routing asynchronous completion plumbing.
IOReturn IOHIDQueueClass::
queueCreateAsyncEventSource(void *self, CFRunLoopSourceRef *source)
    { return getThis(self)->createAsyncEventSource(source); }

CFRunLoopSourceRef IOHIDQueueClass::
queueGetAsyncEventSource(void *self)
    { return getThis(self)->getAsyncEventSource(); }

IOReturn IOHIDQueueClass::
queueCreateAsyncPort(void *self, mach_port_t *port)
    { return getThis(self)->createAsyncPort(port); }

mach_port_t IOHIDQueueClass::
queueGetAsyncPort(void *self)
    { return getThis(self)->getAsyncPort(); }

/* Basic IOHIDQueue interface */
IOReturn IOHIDQueueClass::
        queueCreate (void * 			self, 
                    UInt32 			flags,
                    UInt32			depth)
    { return getThis(self)->create(flags, depth); }

IOReturn IOHIDQueueClass::queueDispose (void * self)
    { return getThis(self)->dispose(); }

/* Any number of hid elements can feed the same queue */
IOReturn IOHIDQueueClass::queueAddElement (void * self,
                            IOHIDElementCookie elementCookie,
                            UInt32 flags)
    { return getThis(self)->addElement(elementCookie, flags); }

IOReturn IOHIDQueueClass::queueRemoveElement (void * self, IOHIDElementCookie elementCookie)
    { return getThis(self)->removeElement(elementCookie); }

Boolean IOHIDQueueClass::queueHasElement (void * self, IOHIDElementCookie elementCookie)
    { return getThis(self)->hasElement(elementCookie); }

/* start/stop data delivery to a queue */
IOReturn IOHIDQueueClass::queueStart (void * self)
    { return getThis(self)->start(); }

IOReturn IOHIDQueueClass::queueStop (void * self)
    { return getThis(self)->stop(); }

/* read next event from a queue */
IOReturn IOHIDQueueClass::queueGetNextEvent (
                        void * 			self,
                        IOHIDEventStruct *	event,
                        AbsoluteTime		maxTime,
                        UInt32 			timeoutMS)
    { return getThis(self)->getNextEvent(event, maxTime, timeoutMS); }

/* set a callback for notification when queue transistions from non-empty */
IOReturn IOHIDQueueClass::queueSetEventCallout (
                        void * 			self,
                        IOHIDCallbackFunction   callback,
                        void * 			callbackTarget,
                        void *			callbackRefcon)
    { return getThis(self)->setEventCallout(callback, callbackTarget, callbackRefcon); }

/* Get the current notification callout */
IOReturn IOHIDQueueClass::queueGetEventCallout (
                        void * 			self,
                        IOHIDCallbackFunction * outCallback,
                        void ** 		outCallbackTarget,
                        void **			outCallbackRefcon)
    { return getThis(self)->getEventCallout(outCallback, outCallbackTarget, outCallbackRefcon); }

