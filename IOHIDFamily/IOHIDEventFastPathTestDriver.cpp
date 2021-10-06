/* add your code here */

#include <AssertMacros.h>
#include "IOHIDEventFastPathTestDriver.h"
#include "IOHIDUsageTables.h"
#include "IOHIDEventServiceFastPathUserClient.h"
#include "IOHIDEvent.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDDebug.h"

#define KEventCacheSize 20
#define KFastPathQueueSize 4096

//===========================================================================
// IOHIDFastClientData class
class IOHIDFastClientData : public OSObject
{
    OSDeclareDefaultStructors(IOHIDFastClientData)
    
    OSDictionary * _propertyCache;
    OSArray      * _eventCache;

public:

    static IOHIDFastClientData * withClientInfo(IOService *client __unused) {
        IOHIDFastClientData * self = OSTypeAlloc(IOHIDFastClientData);
        if (!self) {
            return self;
        }
        if (self->init()) {
            self->_propertyCache = OSDictionary::withCapacity(1);
            self->_eventCache = OSArray::withCapacity(KEventCacheSize);
            if (self->_eventCache) {
                for (int index = 0 ; index < KEventCacheSize ; index++) {
                    IOHIDEvent * event = IOHIDEvent::keyboardEvent(mach_absolute_time(), 1, 1, 0);
                    if (event) {
                        (self->_eventCache)->setObject (event);
                        event->release();
                    }
                }
            }
        }
        if (!self->_propertyCache || !self->_eventCache) {
            self->release();
            self = NULL;
        }
        return self;
    }
    
    inline OSDictionary * getPropertyCache() {
        return _propertyCache;
    }
    
    inline OSArray *  getEventCache() {
        return _eventCache;
    }
    
    virtual void free() APPLE_KEXT_OVERRIDE {
        if (_eventCache) {
            _eventCache->release();
        }
        if (_propertyCache) {
            _propertyCache->release();
        }
        OSObject::free();
    }
};

OSDefineMetaClassAndStructors(IOHIDFastClientData, OSObject)

//===========================================================================
// IOHIDEventFastPathDriver class

#define super IOHIDEventDriver

OSDefineMetaClassAndStructors( IOHIDEventFastPathDriver, IOHIDEventDriver )

//====================================================================================================
// IOHIDEventFastPathDriver::handleStart
//====================================================================================================
bool IOHIDEventFastPathDriver::handleStart( IOService * provider )
{
    
    if (!super::handleStart(provider)) {
        return false;
    }
    
    fastClients = OSDictionary::withCapacity(1);
    if (!fastClients) {
        return false;
    }
    // disable standard event queue
    setProperty (kIOHIDEventServiceQueueSize, 0ull, 32);
    
    return true;
}


//====================================================================================================
// IOHIDEventFastPathDriver::free
//====================================================================================================
void IOHIDEventFastPathDriver::free ()
{
    
    OSSafeReleaseNULL(fastClients);

    super::free();
}


//====================================================================================================
// IOHIDEventFastPathDriver::dispatchEvent
//====================================================================================================
void IOHIDEventFastPathDriver::dispatchEvent(IOHIDEvent * event __unused, IOOptionBits options __unused)
{

}

//====================================================================================================
// IOHIDEventFastPathDriver::copyEventForClient
//====================================================================================================
IOHIDEvent * IOHIDEventFastPathDriver::copyEventForClient (OSObject * copySpec, IOOptionBits options, void * clientContext)
{
    HIDLogDebug ("IOHIDEventFastPathDriver::copyEventForClient (0x%lx,0x%x,0x%lx)", (uintptr_t)copySpec, options, (uintptr_t)clientContext);
    
    IOHIDFastClientData * clientData = (IOHIDFastClientData*) clientContext;
    
    uint32_t        copyCount   = 1;
    IOHIDEvent *    event       = NULL;
    IOHIDEvent *    collection  = NULL;
    
    if (copySpec) {
        if (OSDynamicCast(OSDictionary, copySpec)) {
            OSNumber * number = OSDynamicCast(OSNumber, ((OSDictionary*)copySpec)->getObject("NumberOfEventToCopy"));
            if (number && number->unsigned32BitValue () > 0) {
                copyCount = number->unsigned32BitValue ();
            }
        } else if (OSDynamicCast(OSData, copySpec)) {
            copyCount = *((const uint32_t*)(const void *)((OSData*)copySpec)->getBytesNoCopy());
        }
    }


    OSArray * events = clientData->getEventCache();


    int start = 0;
    int end   = copyCount < events->getCount () ? copyCount : events->getCount();

    for (int index = start; index < end; index++) {
        if (collection) {
            collection->appendChild ((IOHIDEvent*)events->getObject(index));
            continue;
        }
        if (event) {
            collection = IOHIDEvent::withType(kIOHIDEventTypeCollection);
            if (!collection) {
                HIDLogDebug ("IOHIDEventFastPathDriver failed to create collection");
                continue;
            }
            
            HIDLogDebug ("IOHIDEventFastPathDriver collection add child");
            
            collection->appendChild (event);
            event->release();
            collection->appendChild ((IOHIDEvent*)events->getObject(index));
            continue;
        }
        event = (IOHIDEvent*)events->getObject(index);
        event->retain();
    }
    return collection ? collection : event;
}

//====================================================================================================
// IOHIDEventFastPathDriver::copyPropertyForClient
//====================================================================================================
OSObject *  IOHIDEventFastPathDriver::copyPropertyForClient (const char * aKey, void * clientContext)  const
{
    HIDLogDebug("IOHIDEventFastPathDriver::copyPropertyForClient(%s,%lx)", aKey ? aKey : "null", (uintptr_t)clientContext);
    IOHIDFastClientData *clientData = (IOHIDFastClientData*) clientContext;
    
    if (aKey && !strcmp(aKey, kIOHIDEventServiceQueueSize)) {
        return OSNumber::withNumber(KFastPathQueueSize, 32);
    }
    
    OSObject * value =  clientData->getPropertyCache()->getObject(aKey);
    if (value) {
        value->retain();
    }
    return value;
}

//====================================================================================================
// IOHIDEventFastPathDriver::copyPropertyForClient
//====================================================================================================
IOReturn  IOHIDEventFastPathDriver::setPropertiesForClient (OSObject * properties, void * clientContext)
{
    HIDLogDebug ("IOHIDEventFastPathDriver::setPropertiesForClient(%lx,%lx)", (uintptr_t)properties, (uintptr_t)clientContext);
    IOHIDFastClientData * clientData = (IOHIDFastClientData *) clientContext;
    OSDictionary * propDict = OSDynamicCast(OSDictionary, properties);
    if (propDict) {
        clientData->getPropertyCache()->merge(propDict);
    }
    return kIOReturnSuccess;
}

//====================================================================================================
// IOHIDEventFastPathDriver::openForClient
//====================================================================================================
bool  IOHIDEventFastPathDriver::openForClient (IOService * client, IOOptionBits options, OSDictionary *property, void ** clientContext)
{
    bool                result;
    IOHIDFastClientData *clientData;
    
    result = super::open(client, options, NULL, NULL);
    require (result, exit);
    
    clientData = IOHIDFastClientData::withClientInfo(client);
    require (clientData, exit);

    *clientContext = (void *) clientData;

    fastClients->setObject((const OSSymbol *)client, clientData);
    
    clientData->release();

    if (property) {
        clientData->getPropertyCache()->merge(property);
    }

exit:

    HIDLogDebug ("IOHIDEventFastPathDriver::openForClient(%lx,%x,%lx,%lx) = %d", (uintptr_t)client, options, (uintptr_t)property, (uintptr_t)clientData, result);
   
    if (!result) {
        super::close (client, options);
    }
    return result;
}

//====================================================================================================
// IOHIDEventFastPathDriver::closeForClient
//====================================================================================================
void IOHIDEventFastPathDriver::closeForClient(IOService *client, void *context, IOOptionBits options) {
    
    HIDLogDebug ("IOHIDEventFastPathDriver::closeForClient(%lx,%x,%lx)", (uintptr_t)client, options, (uintptr_t)context);
    
    fastClients->removeObject((const OSSymbol *)client);

    super::close (client, options);
}


