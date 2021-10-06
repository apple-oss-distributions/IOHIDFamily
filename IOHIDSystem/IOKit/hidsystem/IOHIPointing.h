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
#ifndef _IOHIPOINTING_H
#define _IOHIPOINTING_H

#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/hidsystem/IOHIDevice.h>
#include <IOKit/hidsystem/IOHIDTypes.h>

/* Start Action Definitions */

/*
 * HISTORICAL NOTE:
 *   The following entry points were part of the IOHIPointingEvents
 *   protocol.
 */
typedef void (*RelativePointerEventAction)(OSObject * target,
                        /* buttons */      int        buttons,
                        /* deltaX */       int        dx,
                        /* deltaY */       int        dy,
                        /* atTime */       AbsoluteTime ts);

typedef void (*AbsolutePointerEventAction)(OSObject * target,
                        /* buttons */      int        buttons,
                        /* at */           Point *    newLoc,
                        /* withBounds */   Bounds *   bounds,
                        /* inProximity */  bool       proximity,
                        /* withPressure */ int        pressure,
                        /* withAngle */    int        stylusAngle,
                        /* atTime */       AbsoluteTime ts);

typedef void (*ScrollWheelEventAction)(OSObject * target,
                                       short      deltaAxis1,
                                       short      deltaAxis2,
                                       short      deltaAxis3,
                                       AbsoluteTime ts);

/* Event Callback Definitions */

typedef void (*RelativePointerEventCallback)(
                        /* target */       OSObject * target,
                        /* buttons */      int        buttons,
                        /* deltaX */       int        dx,
                        /* deltaY */       int        dy,
                        /* atTime */       AbsoluteTime ts,
                        /* sender */       OSObject * sender,
                        /* refcon */       void *     refcon);

typedef void (*AbsolutePointerEventCallback)(
                        /* target */       OSObject * target,
                        /* buttons */      int        buttons,
                        /* at */           Point *    newLoc,
                        /* withBounds */   Bounds *   bounds,
                        /* inProximity */  bool       proximity,
                        /* withPressure */ int        pressure,
                        /* withAngle */    int        stylusAngle,
                        /* atTime */       AbsoluteTime ts,
                        /* sender */       OSObject * sender,
                        /* refcon */       void *     refcon);

typedef void (*ScrollWheelEventCallback)(
                        /* target */       OSObject * target,
                        /* delta1 */       short      deltaAxis1,
                        /* delta2 */       short      deltaAxis2,
                        /* delta3 */       short      deltaAxis3,
                        /* fixedDelta1 */  IOFixed    fixedDelta1,
                        /* fixedDelta2 */  IOFixed    fixedDelta2,
                        /* fixedDelta3 */  IOFixed    fixedDelta3,
                        /* atTime */       AbsoluteTime ts,
                        /* sender */       OSObject * sender,
                        /* refcon */       void *     refcon);

/* End Action Definitions */
class IOHIDPointingDevice;

class IOHIPointing : public IOHIDevice
{
    OSDeclareDefaultStructors(IOHIPointing);
    
    friend class IOHITablet;

private:
    IOLock *		_deviceLock;  // Lock for all device access
    int			_buttonMode;  // The "handedness" of the pointer
    IOFixed		_acceleration;
    bool		_convertAbsoluteToRelative;
    bool		_contactToMove;
    bool		_hadContact;
    Point		_previousLocation;
    UInt8		_pressureThresholdToClick;	// A scale factor of 0 to 255 to determine how much pressure is necessary to generate a primary mouse click - a value of 255 means no click will be generated
    void *		_scaleSegments;
    IOItemCount		_scaleSegCount;
    IOFixed		_fractX;
    IOFixed		_fractY;

    OSObject *                 _relativePointerEventTarget;
    RelativePointerEventAction _relativePointerEventAction;
    OSObject *                 _absolutePointerEventTarget;
    AbsolutePointerEventAction _absolutePointerEventAction;
    OSObject *                 _scrollWheelEventTarget;
    ScrollWheelEventAction     _scrollWheelEventAction;
    
    #define SCROLL_TIME_DELTA_COUNT		8

    struct ExpansionData { 
    
        // Added for scroll whell accel support
        IOFixed		scrollAcceleration;
        void *		scrollScaleSegments;
        IOItemCount	scrollScaleSegCount;

        UInt32		scrollTimeDeltas1[SCROLL_TIME_DELTA_COUNT];
        UInt32		scrollTimeDeltas2[SCROLL_TIME_DELTA_COUNT];
        UInt32		scrollTimeDeltas3[SCROLL_TIME_DELTA_COUNT];
        UInt8 		scrollTimeDeltaIndex1;
        UInt8 		scrollTimeDeltaIndex2;
        UInt8 		scrollTimeDeltaIndex3;
        IOFixed		scrollLastDeltaAxis1;
        IOFixed		scrollLastDeltaAxis2;
        IOFixed		scrollLastDeltaAxis3;
        IOFixed		scrollFixedDeltaAxis1;
        IOFixed		scrollFixedDeltaAxis2;
        IOFixed		scrollFixedDeltaAxis3;        
        AbsoluteTime	scrollLastEventTime1;
        AbsoluteTime	scrollLastEventTime2;
        AbsoluteTime	scrollLastEventTime3;

        // Added to post events to the HID Manager
        IOHIDPointingDevice	* hidPointingNub;
        IOService 		* openClient;
        
        bool		isSeized;
    };

    ExpansionData *  _reserved;
    
protected:
  virtual void dispatchRelativePointerEvent(int        dx,
                                            int        dy,
                                            UInt32     buttonState,
                                            AbsoluteTime ts);
    
  virtual void dispatchAbsolutePointerEvent(Point *	newLoc,
                                            Bounds *	bounds,
                                            UInt32	buttonState,
                                            bool	proximity,
                                            int		pressure,
                                            int		pressureMin,
                                            int		pressureMax,
                                            int		stylusAngle,
                                            AbsoluteTime	ts);

  virtual void dispatchScrollWheelEvent(short deltaAxis1,
                                        short deltaAxis2,
                                        short deltaAxis3,
                                        AbsoluteTime ts);

public:
  virtual bool init(OSDictionary * properties = 0);
  virtual bool start(IOService * provider);
  virtual void stop( IOService * provider );
  virtual void free();

  virtual bool open(IOService *                client,
		    IOOptionBits	       options,
                    RelativePointerEventAction rpeAction,
                    AbsolutePointerEventAction apeAction,
                    ScrollWheelEventAction     sweAction);
                    
  bool open(        IOService *				client,
                    IOOptionBits			options,
                    void *,
                    RelativePointerEventCallback	rpeCallback,
                    AbsolutePointerEventCallback	apeCallback,
                    ScrollWheelEventCallback		sweCallback);

  virtual void close(IOService * client, IOOptionBits );
  virtual IOReturn message( UInt32 type, IOService * provider,
                              void * argument = 0 );

  virtual IOHIDKind hidKind();
  virtual bool 	    updateProperties( void );
  virtual IOReturn  setParamProperties( OSDictionary * dict );
  virtual IOReturn  powerStateWillChangeTo( IOPMPowerFlags powerFlags,
                        unsigned long newState, IOService * device);
  virtual IOReturn  powerStateDidChangeTo( IOPMPowerFlags powerFlags,
                        unsigned long newState, IOService * device);

protected: // for subclasses to implement
  virtual OSData *    copyAccelerationTable();
  virtual IOItemCount buttonCount();
  virtual IOFixed     resolution();

  // RY: Adding method to copy scroll wheel accel table.
  // Unfortunately, we don't have any padding, so this
  // is going to be non-virtual.
  /*virtual*/ OSData * copyScrollAccelerationTable();

private:
  virtual bool resetPointer();
  virtual void scalePointer(int * dxp, int * dyp);
  virtual void setupForAcceleration(IOFixed accl);
  
  // RY: Adding methods to support scroll wheel accel.
  // Unfortunately, we don't have any padding, so these
  // are going to be non-virtual.
  /*virtual*/ bool 	resetScroll();
  /*virtual*/ void 	scaleScrollAxes(IOFixed * axis1p, IOFixed * axis2p, IOFixed * axis3p);
  /*virtual*/ void 	setupScrollForAcceleration(IOFixed accl);
  
  // RY: We have to make sure that subclasses that will 
  // take advantage of this have their defined resolution 
  // in their property table.
  /*virtual*/ IOFixed	scrollResolution();
  
private:
  static void _relativePointerEvent( IOHIPointing * self,
				    int        buttons,
                       /* deltaX */ int        dx,
                       /* deltaY */ int        dy,
                       /* atTime */ AbsoluteTime ts);

  /* Tablet event reporting */
  static void _absolutePointerEvent(IOHIPointing * self,
				    int        buttons,
                 /* at */           Point *    newLoc,
                 /* withBounds */   Bounds *   bounds,
                 /* inProximity */  bool       proximity,
                 /* withPressure */ int        pressure,
                 /* withAngle */    int        stylusAngle,
                 /* atTime */       AbsoluteTime ts);

  /* Mouse scroll wheel event reporting */
  static void _scrollWheelEvent(IOHIPointing *self,
                                short deltaAxis1,
                                short deltaAxis2,
                                short deltaAxis3,
                                AbsoluteTime ts);

};

#endif /* !_IOHIPOINTING_H */
