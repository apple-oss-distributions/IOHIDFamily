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

/*
 * Changes to this API are expected.
 */

#ifndef _IOKIT_IOHIDLibUserClient_H_
#define _IOKIT_IOHIDLibUserClient_H_

#include <IOKit/hid/IOHIDKeys.h>

#define kMaxLocalCookieArrayLength  512
#define kIOHIDDefaultMaxReportSize   8192        // 8K

enum IOHIDLibUserClientConnectTypes {
	kIOHIDLibUserClientConnectManager = 0x00484944 /* HID */
};

// port types: I did consider adding queue ports to this
// as well, but i'm not comfty with sending object pointers
// as a type.
enum IOHIDLibUserClientPortTypes {
	kIOHIDLibUserClientAsyncPortType = 0,
	kIOHIDLibUserClientDeviceValidPortType
};

enum IOHIDLibUserClientCommandCodes {
	kIOHIDLibUserClientDeviceIsValid, // 0
	kIOHIDLibUserClientOpen,
	kIOHIDLibUserClientClose,
	kIOHIDLibUserClientCreateQueue,
	kIOHIDLibUserClientDisposeQueue,
	kIOHIDLibUserClientAddElementToQueue, // 5
	kIOHIDLibUserClientRemoveElementFromQueue,
	kIOHIDLibUserClientQueueHasElement,
	kIOHIDLibUserClientStartQueue,
	kIOHIDLibUserClientStopQueue,
	kIOHIDLibUserClientUpdateElementValues, // 10
	kIOHIDLibUserClientPostElementValues,
	kIOHIDLibUserClientGetReport,
	kIOHIDLibUserClientSetReport,
	kIOHIDLibUserClientGetElementCount,
	kIOHIDLibUserClientGetElements, //15
	kIOHIDLibUserClientSetQueueAsyncPort,
    kIOHIDLibUserClientReleaseReport,
    kIOHIDLibUserClientResumeReports,
	kIOHIDLibUserClientNumCommands // 19
};

enum IOHIDElementValueFlags {
    kIOHIDElementValueOOBReport = 0x01
};

enum IOHIDUpdateElementFlags {
    kIOHIDElementPreventPoll = 0x01
};

typedef struct IOHIDElementValueHeader {
    uint32_t cookie;
    uint32_t length;
    uint32_t value[0];
} IOHIDElementValueHeader;

__BEGIN_DECLS

typedef struct _IOHIDElementValue
{
	IOHIDElementCookie	cookie;
    UInt32              flags:8;
	UInt32              totalSize:24;
	AbsoluteTime        timestamp;
	UInt32              generation;
	UInt32              value[1];
}IOHIDElementValue;

#define ELEMENT_VALUE_REPORT_SIZE(elem) (elem->totalSize - sizeof(*elem) + sizeof(elem->value))
#define ELEMENT_VALUE_HEADER_SIZE(elem) (sizeof(*elem) - sizeof(elem->value))

typedef struct _IOHIDReportReq
{
	UInt32		reportType;
	UInt32		reportID;
	void		*reportBuffer;
	UInt32		reportBufferSize;
}IOHIDReportReq;

struct IOHIDElementStruct;
typedef struct IOHIDElementStruct IOHIDElementStruct;
struct IOHIDElementStruct
{
	UInt32				cookieMin;
	UInt32				cookieMax;
	UInt32				parentCookie;
	UInt32				type;
	UInt32				collectionType;
	UInt32				flags;
	UInt32				usagePage;
	UInt32				usageMin;
	UInt32				usageMax;
	SInt32				min;
	SInt32				max;
	SInt32				scaledMin;
	SInt32				scaledMax;
	UInt32				size;
	UInt32				reportSize;
	UInt32				reportCount;
	UInt32				reportID;
	UInt32				unit;
	UInt32				unitExponent;
	UInt32				duplicateValueSize;
	UInt32				duplicateIndex;
	UInt32				bytes;
	UInt32				valueSize;
    UInt32              rawReportCount;
};

typedef struct _IOHIDQueueHeader
{
	uint64_t _Atomic status;
} IOHIDQueueHeader;

enum IOHIDQueueStatus {
	kIOHIDQueueStatusBlocked = 0x1
};

enum {
	kHIDElementType			= 0,
	kHIDReportHandlerType
};

#ifndef ALIGN_DATA_SIZE
#define ALIGN_DATA_SIZE(size) (((size) + 3) / 4 * 4)
#endif

__END_DECLS

#if KERNEL

#include <kern/queue.h>
#include <mach/mach_types.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

class IOHIDDevice;
class IOHIDEventQueue;
class IOHIDReportElementQueue;
class IOSyncer;
struct IOHIDCompletion;

enum {
	kHIDQueueStateEnable,
	kHIDQueueStateDisable,
	kHIDQueueStateClear
};

#if defined(KERNEL) && !defined(KERNEL_PRIVATE)
class __deprecated_msg("Use DriverKit") IOHIDLibUserClient : public IOUserClient2022
#else
class IOHIDLibUserClient : public IOUserClient2022
#endif
{
	OSDeclareDefaultStructors(IOHIDLibUserClient)
	
	bool resourceNotification(void *refCon, IOService *service, IONotifier *notifier);
	void resourceNotificationGated();
	
	void setStateForQueues(UInt32 state, IOOptionBits options = 0);
	
	void setValid(bool state);
	
	IOReturn dispatchMessage(void* message);
	bool serializeDebugState(void *ref, OSSerialize *serializer);

public:
	bool attach(IOService * provider) APPLE_KEXT_OVERRIDE;

    IOReturn processElement(IOHIDElementValue *element, IOHIDReportElementQueue *queue);
	
protected:
	static const IOExternalMethodDispatch2022 sMethods[kIOHIDLibUserClientNumCommands];

	IOHIDDevice *fNub;
	IOWorkLoop *fWL;
	IOCommandGate *fGate;
	IOInterruptEventSource * fResourceES;
	
	OSArray *fQueueMap;
    queue_head_t fReportList;
    queue_head_t fBlockedReports;

	UInt32 fPid;
	task_t fClient;
    UInt32 fReportLimit;
	mach_port_t fWakePort;
	mach_port_t fValidPort;
    OSSet       *_pending;
    bool fClientSuspended;

    UInt32 fSetReportCnt;
    UInt32 fSetReportErrCnt;
    UInt32 fGetReportCnt;
    UInt32 fGetReportErrCnt;
	
	void * fValidMessage;
	
	bool fClientOpened;
    bool fClientSeized;
	bool fNubIsKeyboard;
    
    // entitlements
    bool _customQueueSizeEntitlement;
    bool _privilegedClient;
    bool _protectedAccessClient;
    bool _interfaceRematchEntitlement;
	
	IOOptionBits fCachedOptionBits;
		
	IONotifier * fResourceNotification;
	
	UInt64 fCachedConsoleUsersSeed;
	
	bool	fValid;

    IOLock * _queueLock;

	// Methods
	virtual bool initWithTask(task_t owningTask, void *security_id, UInt32 type) APPLE_KEXT_OVERRIDE;
	
	virtual IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;

	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

	virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer) APPLE_KEXT_OVERRIDE;
		
	virtual void free(void) APPLE_KEXT_OVERRIDE;

	virtual IOReturn message(UInt32 type, IOService * provider, void * argument = 0 ) APPLE_KEXT_OVERRIDE;
	virtual IOReturn messageGated(UInt32 type, IOService * provider, void * argument = 0 );
	
	virtual IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;

	virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon ) APPLE_KEXT_OVERRIDE;
	virtual IOReturn registerNotificationPortGated(mach_port_t port, UInt32 type, UInt32 refCon );

	// return the shared memory for type (called indirectly)
	virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits * options, IOMemoryDescriptor ** memory) APPLE_KEXT_OVERRIDE;
	IOReturn         clientMemoryForTypeGated(UInt32 type, IOOptionBits * options, IOMemoryDescriptor ** memory);
						
    IOReturn externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * arguments) APPLE_KEXT_OVERRIDE;
	IOReturn externalMethodGated(void * args);


	// Open the IOHIDDevice
	static IOReturn _open(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		open(IOOptionBits options);

	// Close the IOHIDDevice
	static IOReturn _close(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		close();
				
	// Get Element Counts
	static IOReturn _getElementCount(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);	
	IOReturn		getElementCount(uint64_t * outElementCount, uint64_t * outReportElementCount);

	// Get Elements
	static IOReturn _getElements(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);	
	IOReturn		getElements(uint32_t elementType, void *elementBuffer, uint32_t *elementBufferSize);
	IOReturn		getElements(uint32_t elementType, IOMemoryDescriptor * mem,  uint32_t *elementBufferSize);

	// Device Valid
	static IOReturn _deviceIsValid(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);	
	IOReturn		deviceIsValid(bool *status, uint64_t *generation);
	
	// Set queue port
	static IOReturn _setQueueAsyncPort(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		setQueueAsyncPort(IOHIDEventQueue * queue, mach_port_t port);

	// Create a queue
	static IOReturn _createQueue(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		createQueue(uint32_t flags, uint32_t depth, uint64_t * outQueue);

	// Dispose a queue
	static IOReturn _disposeQueue(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		disposeQueue(IOHIDEventQueue * queue);

	// Add an element to a queue
	static IOReturn _addElementToQueue(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		addElementToQueue(IOHIDEventQueue * queue, IOHIDElementCookie elementCookie, uint32_t flags, uint64_t *pSizeChange);
   
	// remove an element from a queue
	static IOReturn _removeElementFromQueue (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		removeElementFromQueue (IOHIDEventQueue * queue, IOHIDElementCookie elementCookie, uint64_t *pSizeChange);
	
	// Check to see if a queue has an element
	static IOReturn _queueHasElement (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		queueHasElement (IOHIDEventQueue * queue, IOHIDElementCookie elementCookie, uint64_t * pHasElement);
	
	// start a queue
	static IOReturn _startQueue (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		startQueue (IOHIDEventQueue * queue);
	
	// stop a queue
	static IOReturn _stopQueue (IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		stopQueue (IOHIDEventQueue * queue);
							
	// Update Feature element value
	static IOReturn	_updateElementValues(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
    IOReturn		updateElementValues(const IOHIDElementCookie * lCookies, uint32_t cookieSize, IOMemoryDescriptor * outputElementsDesc, uint32_t outputElementsDescSize, IOOptionBits options, uint32_t timeout = 0, IOHIDCompletion * completion = 0, IOBufferMemoryDescriptor * elementData = 0);
    IOReturn        updateElementValues(const IOHIDElementCookie * lCookies, uint32_t cookieSize, void *outputElements, uint32_t outputElementsSize, IOOptionBits options, uint32_t timeout = 0, IOHIDCompletion * completion = 0, IOBufferMemoryDescriptor * elementData = 0);
												
	// Post element value
	static IOReturn _postElementValues(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
    IOReturn        postElementValues(IOMemoryDescriptor * desc, uint32_t timeout = 0, IOHIDCompletion * completion = 0);
	IOReturn		postElementValues(const uint8_t * data, uint32_t dataSize, uint32_t timeout = 0, IOHIDCompletion * completion = 0);
												
	// Get report
	static IOReturn _getReport(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		getReport(void * reportBuffer, uint32_t * pOutsize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout = 0, IOHIDCompletion * completion = 0);
    IOReturn        getReport(IOMemoryDescriptor * mem, uint32_t * pOutsize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout = 0, IOHIDCompletion * completion = 0);

	// Set report
	static IOReturn _setReport(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	IOReturn		setReport(const void * reportBuffer, uint32_t reportBufferSize, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout = 0, IOHIDCompletion * completion = 0);
	IOReturn		setReport(IOMemoryDescriptor * mem, IOHIDReportType reportType, uint32_t reportID, uint32_t timeout = 0, IOHIDCompletion * completion = 0);

    static void _releaseReport(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
    void        releaseReport(mach_vm_address_t reportToken);

    void CommitComplete(void * param, IOReturn status, UInt32 remaining);
    IOReturn CommitCompleteGated(void * param, IOReturn status, UInt32 remaining);
	void ReqComplete(void * param, IOReturn status, UInt32 remaining);
	IOReturn ReqCompleteGated(void * param, IOReturn status, UInt32 remaining);

	u_int createTokenForQueue(IOHIDEventQueue *queue);
	void removeQueueFromMap(IOHIDEventQueue *queue);
	IOHIDEventQueue* getQueueForToken(u_int token);
	
	// Iterator over valid tokens.  Start at 0 (not a valid token) 
	// and keep calling it with the return value till you get 0 
	// (still not a valid token).  vtn3
	u_int getNextTokenForToken(u_int token);

	// Handle enqueuing 
	Boolean handleEnqueue(void *queueData, UInt32 dataSize, IOHIDReportElementQueue *queue);
	bool canDropReport();

	static IOReturn _resumeReports(IOHIDLibUserClient * target, void * reference, IOExternalMethodArguments * arguments);
	void resumeReports();

	OSArray * getElementsForType(uint32_t elementType);
};


class IOHIDOOBReportDescriptor : public IOBufferMemoryDescriptor
{
    OSDeclareDefaultStructors(IOHIDOOBReportDescriptor)

public:
    queue_chain_t qc;
    IOMemoryMap * mapping;

    static IOHIDOOBReportDescriptor * inTaskWithBytes(
        task_t       task,
        const void * bytes,
        vm_size_t    withLength,
        IODirection  withDirection,
        bool         withContiguousMemory = false);
};

#endif /* KERNEL */

#endif /* ! _IOKIT_IOHIDLibUserClient_H_ */

