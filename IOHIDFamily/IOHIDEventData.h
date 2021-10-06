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

#ifndef _IOKIT_HID_IOHIDEVENTDATA_H
#define _IOKIT_HID_IOHIDEVENTDATA_H

#include <IOKit/IOTypes.h>
#include <IOKit/hid/IOHIDEventTypes.h>

#ifdef KERNEL
    #include <IOKit/IOLib.h>

    #define IOHIDEventRef IOHIDEvent *
#else
    #include <IOKit/hid/IOHIDEvent.h>

    typedef struct IOHIDEventData IOHIDEventData;
#endif

//==============================================================================
// IOHIDEventData Declarations
//==============================================================================

enum {
    kIOHIDEventOptionIgnore         = 0xf0000000,
    kIOHIDEventOptionIsRepeat       = 0x00010000,
    kIOHIDEventOptionIsZeroEvent    = 0x00800000,
};

#define IOHIDEVENT_BASE             \
    uint32_t            size;       \
    IOHIDEventType      type;       \
    uint32_t            options;    \
    uint8_t             depth;      \
    uint8_t             reserved[3];\

/*!
    @typedef    IOHIDEventData
    @abstract   
    @discussion 
    @field      size        Size, in bytes, of the memory detailing this
                            particular event
    @field      type        Type of this particular event
    @field      options     Event specific options
*/

struct IOHIDEventData{
    IOHIDEVENT_BASE;
};

typedef struct __attribute__((packed)) _IOHIDVendorDefinedEventData {
    IOHIDEVENT_BASE;
    uint16_t        usagePage;
    uint16_t        usage;
    uint32_t        version;
    uint32_t        length;
    uint8_t         data[0];
} IOHIDVendorDefinedEventData;

enum {
    kIOHIDKeyboardIsRepeat          = kIOHIDEventOptionIsRepeat, // DEPRECATED
    kIOHIDKeyboardStickyKeyDown     = 0x00020000,
    kIOHIDKeyboardStickyKeyLocked   = 0x00040000,
    kIOHIDKeyboardStickyKeyUp       = 0x00080000,
    kIOHIDKeyboardStickyKeysOn      = 0x00200000,
    kIOHIDKeyboardStickyKeysOff     = 0x00400000
};

typedef struct _IOHIDKeyboardEventData {
    IOHIDEVENT_BASE;                            // options = kHIDKeyboardRepeat, kIOHIDKeyboardStickyKeyDown, kIOHIDKeyboardStickyKeyLocked, kIOHIDKeyboardStickyKeyUp
    uint16_t        usagePage;
    uint16_t        usage;
    boolean_t       down;
    uint32_t        flags;
    uint8_t         pressCount;
} IOHIDKeyboardEventData;

typedef struct _IOHIDUnicodeEventData {
    IOHIDEVENT_BASE;
    uint32_t        encoding;
    IOFixed         quality;
    uint32_t        length;
    uint8_t         payload[0];
} IOHIDUnicodeEventData;

typedef struct _IOHIDLEDEventData {
    IOHIDEVENT_BASE;
    uint32_t        mask;
    uint8_t         number;
    boolean_t       state;
} IOHIDLEDEventData;

enum {
    kIOHIDTransducerRange               = 0x00010000,
    kIOHIDTransducerTouch               = 0x00020000,
    kIOHIDTransducerInvert              = 0x00040000,
    kIOHIDTransducerDisplayIntegrated   = 0x00080000
};

enum {
    kIOHIDDigitizerOrientationTypeTilt = 0,
    kIOHIDDigitizerOrientationTypePolar,
    kIOHIDDigitizerOrientationTypeQuality
};
typedef uint8_t IOHIDDigitizerOrientationType;

#define IOHIDAXISEVENT_BASE     \
    struct {                    \
        IOFixed x;              \
        IOFixed y;              \
        IOFixed z;              \
    } position;

typedef struct _IOHIDAxisEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    IOHIDAXISEVENT_BASE;
} IOHIDAxisEventData, IOHIDTranslationData, IOHIDRotationEventData, IOHIDScrollEventData, IOHIDScaleEventData, IOHIDVelocityData, IOHIDOrientationEventData;

typedef struct _IOHIDMotionEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    IOHIDAXISEVENT_BASE;
    uint32_t    motionType;
    uint32_t    motionSubType;
    uint32_t    motionSequence;
} IOHIDMotionEventData, IOHIDAccelerometerEventData, IOHIDGyroEventData, IOHIDCompassEventData;

typedef struct _IOHIDAmbientLightSensorEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    uint32_t        level;
    uint32_t        ch0;
    uint32_t        ch1;
    uint32_t        ch2;
    uint32_t        ch3;
    Boolean         brightnessChanged;
    IOHIDEventColorSpace  colorSpace;
    IOHIDDouble           colorComponent0;
    IOHIDDouble           colorComponent1;
    IOHIDDouble           colorComponent2;
} IOHIDAmbientLightSensorEventData;

typedef struct _IOHIDTemperatureEventData {
    IOHIDEVENT_BASE;                            
    IOFixed        level;
} IOHIDTemperatureEventData;

typedef struct _IOHIDProximityEventData {
    IOHIDEVENT_BASE;                            
    uint32_t        detectionMask;
    uint32_t        level;
} IOHIDProximityEventData;

typedef struct _IOHIDProgressEventData {
    IOHIDEVENT_BASE;
    uint32_t        eventType;
    IOFixed         level;
} IOHIDProgressEventData;

typedef struct _IOHIDBiometricEventData {
    IOHIDEVENT_BASE;
    uint32_t        eventType;
    IOFixed         level;
    uint16_t        usagePage;
    uint16_t        usage;
    uint32_t        flags;
    uint8_t         tapCount;
} IOHIDBiometricEventData;

typedef struct _IOHIDZoomToggleEventData {
    IOHIDEVENT_BASE;
} IOHIDZoomToggleEventData;

#define IOHIDBUTTONLITEEVENT_BASE       \
    struct {                            \
        uint32_t mask;                  \
    } button;

typedef struct _IOHIDButtonEventData {
    IOHIDEVENT_BASE;
    uint32_t        mask;
    IOFixed         pressure;
    uint8_t         number;
    uint8_t         clickCount;
    boolean_t       state;
} IOHIDButtonEventData;

enum {
    kIOHIDAccelerated                   = 0x00010000,
};

typedef struct _IOHIDPointerEventData {
    IOHIDEVENT_BASE;
    IOHIDAXISEVENT_BASE;
    IOHIDBUTTONLITEEVENT_BASE;
} IOHIDPointerEventData, IOHIDMouseEventData;

typedef struct _IOHIDMultiAxisPointerEventData {
    IOHIDEVENT_BASE;
    IOHIDAXISEVENT_BASE;
    IOHIDBUTTONLITEEVENT_BASE;
    struct {
        IOFixed x;
        IOFixed y;
        IOFixed z;
    } rotation;
    
} IOHIDMultiAxisPointerEventData;


typedef struct _IOHIDDigitizerEventData {
    IOHIDEVENT_BASE;                            // options = kIOHIDTransducerRange, kHIDTransducerTouch, kHIDTransducerInvert
    IOHIDAXISEVENT_BASE;

    uint32_t        transducerIndex;   
    uint32_t        transducerType;             // could overload this to include that both the hand and finger id.
    uint32_t        identity;                   // Specifies a unique ID of the current transducer action.
    uint32_t        eventMask;                  // the type of event that has occurred: range, touch, position
    uint32_t        childEventMask;             // CHILD: the type of event that has occurred: range, touch, position
    
    uint32_t        buttonMask;                 // Bit field representing the current button state
                                                // Pressure field are assumed to be scaled from 0.0 to 1.0
    IOFixed         pressure;                   // Force exerted against the digitizer surface by the transducer.
    IOFixed         auxPressure;                // Force exerted directly by the user on a transducer sensor.
    
    IOFixed         twist;                      // Specifies the clockwise rotation of the cursor around its own major axis.  Unsure it the device should declare units via properties or event.  My first inclination is force degrees as the is the unit already expected by AppKit, Carbon and OpenGL.
    
    uint32_t        orientationType;            // Specifies the orientation type used by the transducer.
    union {
        struct {                                // X Tilt and Y Tilt are used together to specify the tilt away from normal of a digitizer transducer. In its normal position, the values of X Tilt and Y Tilt for a transducer are both zero.
            IOFixed     x;                      // This quantity is used in conjunction with Y Tilt to represent the tilt away from normal of a transducer, such as a stylus. The X Tilt value represents the plane angle between the Y-Z plane and the plane containing the transducer axis and the Y axis. A positive X Tilt is to the right. 
            IOFixed     y;                      // This value represents the angle between the X-Z and transducer-X planes. A positive Y Tilt is toward the user.
        } tilt;
        struct {                                // X Tilt and Y Tilt are used together to specify the tilt away from normal of a digitizer transducer. In its normal position, the values of X Tilt and Y Tilt for a transducer are both zero.
            IOFixed  altitude;                  //The angle with the X-Y plane though a signed, semicicular range.  Positive values specify an angle downward and toward the positive Z axis. 
            IOFixed  azimuth;                   // Specifies the counter clockwise rotation of the cursor around the Z axis though a full circular range.
            IOFixed  quality;                   // If set, indicates that the transducer is sensed to be in a relatively noise-free region of digitizing.
            IOFixed  density;
            IOFixed  majorRadius;                // units in mm
            IOFixed  minorRadius;                // units in mm
        } polar;
        struct {
            IOFixed  quality;                    // If set, indicates that the transducer is sensed to be in a relatively noise-free region of digitizing.
            IOFixed  density;
            IOFixed  irregularity;
            IOFixed  majorRadius;                // units in mm
            IOFixed  minorRadius;                // units in mm
            IOFixed  accuracy;                   // The accuracy of the major/minor radius measurement, in mm. 0 indicates no data.
        } quality;
    }orientation;
    uint32_t        generationCount;             // Unique identfier/sequence number for this event
    uint32_t        willUpdateMask;              // Mask of fields in this event that will be updated in future
    uint32_t        didUpdateMask;               // Mask of fields that was updated in this event
} IOHIDDigitizerEventData;

typedef struct _IOHIDSwipeEventData {
    IOHIDEVENT_BASE;                            
    IOHIDAXISEVENT_BASE;
    IOHIDSwipeMask      swipeMask;      // legacy
    IOHIDGestureMotion  gestureMotion;  // horizontal, vertical, scale, rotate, tap, etc
    IOHIDGestureFlavor  flavor;         // event flavor for routing purposes
    IOFixed             progress;       // progress of gesture, as a fraction (1.0 = 100%)
}   IOHIDSwipeEventData, 
    IOHIDNavagationSwipeEventData, 
    IOHIDDockSwipeEventData, 
    IOHIDFluidTouchGestureData,
    IOHIDBoundaryScrollData;

typedef struct _IOHIDSymbolicHotKeyEventData {
    IOHIDEVENT_BASE;
    uint32_t    hotKey;
} IOHIDSymbolicHotKeyEventData;

enum {
    kIOHIDSymbolicHotKeyOptionIsCGSHotKey = 0x00010000,
};

typedef struct _IOHIDPowerEventData {
    IOHIDEVENT_BASE;
    int64_t         measurement; // 48.16 signed fixed point
    uint32_t        powerType;
    uint32_t        powerSubType;
} IOHIDPowerEventData;

typedef struct _IOHIDAtmosphericPressureEventData {
    IOHIDEVENT_BASE;
    IOFixed        level;
    uint32_t        sequence;
} IOHIDAtmosphericPressureEventData;

typedef struct _IOHIDForceEventData {
    IOHIDEVENT_BASE;
    uint32_t        behavior;
    IOFixed         progress;
    uint32_t        stage;
    IOFixed         stageProgress;
} IOHIDForceEventData;

typedef struct _IOHIDMotionActivityEventData {
    IOHIDEVENT_BASE;
    uint32_t        activityType;
    IOFixed         confidence;
} IOHIDMotionActivityEventData;

typedef struct _IOHIDMotionGestureEventData {
    IOHIDEVENT_BASE;
    uint32_t        gestureType;
    IOFixed         progress;
} IOHIDMotionGestureEventData;

typedef struct _IOHIDGameControllerEventData {
    IOHIDEVENT_BASE;
    uint32_t    controllerType;
    
    struct {
        IOFixed up;
        IOFixed down;
        IOFixed left;
        IOFixed right;
    } dpad;
    
    struct {
        IOFixed x;
        IOFixed y;
        IOFixed a;
        IOFixed b;
    }face;
    
    struct {
        IOFixed x;
        IOFixed y;
        IOFixed z;
        IOFixed rz;
    } joystick;
    
    struct {
        IOFixed l1;
        IOFixed l2;
        IOFixed r1;
        IOFixed r2;
    } shoulder;
} IOHIDGameControllerEventData;

typedef struct _IOHIDHumidityEventData {
    IOHIDEVENT_BASE;
    IOFixed         rh;
    uint32_t        sequence;
} IOHIDHumidityEventData;


/*!
    @typedef    IOHIDBrightnessEventData
    @abstract   Event represend btrightness change
    @discussion event dispatched on brightness change.
    @field      currentBrightness   Current  logival brightness
    @field      targetBrightness    target logial brightness
    @field      transitionTime      transition time form currentBrightness -> targetBrightness in us
*/
typedef struct _IOHIDBrightnessEventData {
    IOHIDEVENT_BASE;
    IOFixed         currentBrightness;
    IOFixed         targetBrightness;
    uint64_t        transitionTime;
} IOHIDBrightnessEventData;

/*!
    @typedef    IOHIDSystemQueueElement
    @abstract   Memory structure defining the layout of each event queue element
    @discussion The IOHIDEventQueueElement represents a portion of mememory in the
                new IOHIDEventQueue.  It is possible that a event queue element
                can contain multiple interpretations of a given event.  The first
                event is always considered the primary event.
    @field      timeStamp   Time at which event was dispatched
    @field      senderID    RegistryID of sending service
    @field      options     Options for further developement
    @field      eventCount  The number of events contained in this transaction
    @field      payload     Begining offset of contiguous mememory that contains the
                            pertinent attribute and event data
*/
typedef struct __attribute__((packed)) _IOHIDSystemQueueElement {
    uint64_t        timeStamp;
    uint64_t        senderID;
    uint32_t        options;
    uint32_t        attributeLength;
    uint32_t        eventCount;
    uint8_t         payload[0];
} IOHIDSystemQueueElement; 

//******************************************************************************
// MACROS
//******************************************************************************

#define IOHIDEventFieldEventType(field) ((field >> 16) & 0xffff)
#define IOHIDEventFieldOffset(field) (field & 0xffff)

#define SET_SUBFIELD_VALUE(var, value, start, mask) do { \
        var &= ~(mask); \
        var |= ((((uint32_t)value) << (start)) & (mask)); \
} while (0)

#define GET_SUBFIELD_VALUE(var, value, start, mask) \
        var = ((((uint32_t)value) & (mask)) >> start)

#define IOHIDEventGetSizeOfForce(type, size)\
        case kIOHIDEventTypeForce:      \
            size = sizeof(IOHIDForceEventData);\
            break;

#define IOHIDEventGetSize(type,size)    \
{                                       \
    switch ( type ) {                   \
        case kIOHIDEventTypeNULL:       \
        case kIOHIDEventTypeVendorDefined:\
        case kIOHIDEventTypeCollection: \
            size = sizeof(IOHIDVendorDefinedEventData);\
            break;                      \
        case kIOHIDEventTypeKeyboard:   \
            size = sizeof(IOHIDKeyboardEventData);\
            break;                      \
        case kIOHIDEventTypeTranslation:\
        case kIOHIDEventTypeRotation:   \
        case kIOHIDEventTypeScroll:     \
        case kIOHIDEventTypeScale:      \
        case kIOHIDEventTypeVelocity:   \
        case kIOHIDEventTypeOrientation:\
            size = sizeof(IOHIDAxisEventData);\
            break;                      \
        case kIOHIDEventTypeAccelerometer:\
        case kIOHIDEventTypeGyro:\
        case kIOHIDEventTypeCompass:\
            size = sizeof(IOHIDMotionEventData);\
            break;\
        case kIOHIDEventTypeAmbientLightSensor:\
            size = sizeof(IOHIDAmbientLightSensorEventData);\
            break;                      \
        case kIOHIDEventTypeProximity:  \
            size = sizeof(IOHIDProximityEventData);\
            break;                      \
        case kIOHIDEventTypeButton:     \
            size = sizeof(IOHIDButtonEventData);\
            break;                      \
        case kIOHIDEventTypeDigitizer:  \
            size = sizeof(IOHIDDigitizerEventData);\
            break;                      \
        case kIOHIDEventTypeTemperature:\
            size = sizeof(IOHIDTemperatureEventData);\
            break;                      \
        case kIOHIDEventTypeNavigationSwipe:\
        case kIOHIDEventTypeDockSwipe:\
        case kIOHIDEventTypeFluidTouchGesture:\
        case kIOHIDEventTypeBoundaryScroll:\
            size = sizeof(IOHIDSwipeEventData);\
            break;                      \
        case kIOHIDEventTypeMultiAxisPointer:\
            size = sizeof(IOHIDMultiAxisPointerEventData);\
            break;\
        case kIOHIDEventTypePointer:\
            size = sizeof(IOHIDPointerEventData);\
            break;                      \
        case kIOHIDEventTypeBiometric:\
            size = sizeof(IOHIDBiometricEventData);\
            break;                      \
        case kIOHIDEventTypeProgress:\
            size = sizeof(IOHIDProgressEventData);\
            break;                      \
        case kIOHIDEventTypeZoomToggle:\
            size = sizeof(IOHIDZoomToggleEventData);\
            break;                      \
        case kIOHIDEventTypeSymbolicHotKey:\
            size = sizeof(IOHIDSymbolicHotKeyEventData);\
            break;                      \
        case kIOHIDEventTypePower:\
            size = sizeof(IOHIDPowerEventData);\
            break;                      \
        case kIOHIDEventTypeLED:\
            size = sizeof(IOHIDLEDEventData);\
            break;                      \
        case kIOHIDEventTypeUnicode:    \
            size = sizeof(IOHIDUnicodeEventData);\
            break;                      \
        case kIOHIDEventTypeAtmosphericPressure:\
            size = sizeof(IOHIDAtmosphericPressureEventData);\
            break;                      \
        case kIOHIDEventTypeMotionActivity:\
            size = sizeof(IOHIDMotionActivityEventData);\
            break;                      \
        case kIOHIDEventTypeMotionGesture:\
            size = sizeof(IOHIDMotionGestureEventData);\
            break;                      \
        case kIOHIDEventTypeGameController:\
            size = sizeof(IOHIDGameControllerEventData);\
            break;                      \
        case kIOHIDEventTypeHumidity:   \
            size = sizeof(IOHIDHumidityEventData);\
            break;                      \
        case kIOHIDEventTypeBrightness:   \
            size = sizeof(IOHIDBrightnessEventData);\
            break;                      \
        default:                        \
            size = 0;                   \
            break;                      \
        IOHIDEventGetSizeOfForce(type, size)\
    }                                   \
}
#define IOHIDEventGetQueueElementSize(type,size)\
{                                               \
    IOHIDEventGetSize(type,size);               \
    size += sizeof(IOHIDSystemQueueElement);    \
}

#define kIOFixedNaN             ((IOFixed)0x80000000)
#define IOFixedIsNaN            ((value) == kIOFixedNaN)

#ifdef KERNEL
    #define IOHIDEventValueFloat_1(value)                       value
    #define IOHIDEventValueFloat_0(value)                       (((value) != kIOFixedNaN) ? (value)>>16 : (value))

    #define IOHIDEventValueFixed_1(value)                       value
    #define IOHIDEventValueFixed_0(value)                       (((value) != kIOFixedNaN) ? (value)<<16 : (value))

    #define IOHIDEventGetEventWithOptions(event, type, options) event->getEvent(type, options)
    #define GET_EVENTDATA(event)                                event->_data
#else
    #define IOHIDEventValueFloat_1(value)                       value
    #define IOHIDEventValueFloat_0(value)                       (((value) != kIOFixedNaN) ? ((value) / 65536.0) : (NAN))

    #define IOHIDEventValueFixed_1(value)                       value
    #define IOHIDEventValueFixed_0(value)                       (((value) != kIOFixedNaN) ? ((value) * 65536) : (kIOFixedNaN))

    #define GET_EVENTDATA(event)                                event->eventData
#endif

    #define IOHIDEventValueFixed_true  IOHIDEventValueFixed_1
    #define IOHIDEventValueFixed_false IOHIDEventValueFixed_0
    #define IOHIDEventValueFloat_true  IOHIDEventValueFloat_1
    #define IOHIDEventValueFloat_false IOHIDEventValueFloat_0
    #define IOHIDEventValueFloat(value, isFixed)                IOHIDEventValueFloat_##isFixed (value)
    #define IOHIDEventValueFixed(value, isFixed)                IOHIDEventValueFixed_##isFixed (value)


//==============================================================================
// IOHIDEventGetValue MACRO
//==============================================================================
#define IOHIDEventGetEventDataForce(eventData, fieldEvType, fieldOffset, value, isFixed)\



#define GET_EVENTDATA_VALUE(eventData, fieldEvType, fieldOffset, value, isFixed)\
{                                                       \
    switch ( fieldEvType ) {                            \
        case kIOHIDEventTypeNULL:                       \
        case kIOHIDEventTypeCollection:                 \
            switch ( fieldOffset ) {                    \
                case IOHIDEventFieldOffset(kIOHIDEventFieldIsRelative): \
                    value = (eventData->options & kIOHIDEventOptionIsAbsolute) == 0; \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldIsCollection): \
                    value = (eventData->options & kIOHIDEventOptionIsCollection) != 0; \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldIsPixelUnits): \
                    value = (eventData->options & kIOHIDEventOptionIsPixelUnits) != 0; \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldIsCenterOrigin): \
                    value = (eventData->options & kIOHIDEventOptionIsCenterOrigin) != 0; \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldIsBuiltIn): \
                    value = (eventData->options & kIOHIDEventOptionIsBuiltIn) != 0; \
                    break;                              \
            }; break;                                   \
        case kIOHIDEventTypeVendorDefined:              \
            {                                           \
                IOHIDVendorDefinedEventData * sysDef = (IOHIDVendorDefinedEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedUsagePage): \
                        value = sysDef->usagePage;      \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedUsage): \
                        value = sysDef->usage;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedVersion): \
                        value = sysDef->version;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedDataLength): \
                        value = sysDef->length;         \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedData): \
                        if (sysDef->data)               \
                            value = *((typeof(value)*) sysDef->data); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeUnicode:                    \
            {                                           \
                IOHIDUnicodeEventData * character = (IOHIDUnicodeEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldUnicodeEncoding): \
                        value = character->encoding;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldUnicodeQuality): \
                        value = IOHIDEventValueFloat(character->quality, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldUnicodeLength): \
                        value = character->length;      \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldUnicodePayload): \
                        if (character->payload)         \
                            value = *((typeof(value)*) character->payload); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeBiometric:                  \
            {                                           \
                IOHIDBiometricEventData * biometric = (IOHIDBiometricEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricEventType): \
                        value = biometric->eventType;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricLevel): \
                        value = IOHIDEventValueFloat(biometric->level, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricUsagePage): \
                        value = biometric->usagePage;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricUsage): \
                        value = biometric->usage;       \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricTapCount): \
                        value = biometric->tapCount;    \
                        break;                          \
                };                                      \
            };                                          \
            break;                                      \
        case kIOHIDEventTypeProgress:                   \
            {                                           \
                IOHIDProgressEventData * progress = (IOHIDProgressEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    /* case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricEventType): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProgressEventType): \
                        value = progress->eventType;    \
                        break;                          \
                    /* case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricLevel): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProgressLevel): \
                        value = IOHIDEventValueFloat(progress->level, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeButton:                     \
            {                                           \
                IOHIDButtonEventData * button = (IOHIDButtonEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonMask): \
                        value = button->mask;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonNumber): \
                        value = button->number;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonClickCount): \
                        value = button->clickCount;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonPressure): \
                        value = IOHIDEventValueFloat(button->pressure, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonState): \
                        value = button->state;          \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeCompass:                    \
        case kIOHIDEventTypeGyro:                       \
        case kIOHIDEventTypeAccelerometer:              \
            {                                           \
                IOHIDMotionEventData * motion = (IOHIDMotionEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassX): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroX): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerX): \
                        value = IOHIDEventValueFloat(motion->position.x, isFixed); \
                        break;                              \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassY): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroY): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerY): \
                        value = IOHIDEventValueFloat(motion->position.y, isFixed); \
                        break;                              \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassZ): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroZ): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerZ): \
                        value = IOHIDEventValueFloat(motion->position.z, isFixed); \
                        break;                              \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassType): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroType): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerType): \
                        value = motion->motionType;     \
                        break;                          \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassSubType): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroSubType): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerSubType): \
                        value = motion->motionSubType;     \
                        break;                          \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassSequence): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroSequence): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerSequence): \
                        value = motion->motionSequence;     \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypePointer:                     \
            {                                           \
                IOHIDPointerEventData * pointer = (IOHIDPointerEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPointerX): \
                        value = IOHIDEventValueFloat(pointer->position.x, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPointerY): \
                        value = IOHIDEventValueFloat(pointer->position.y, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPointerZ): \
                        value = IOHIDEventValueFloat(pointer->position.z, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPointerButtonMask): \
                        value = pointer->button.mask;       \
                        break;                              \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeMultiAxisPointer:                     \
            {                                           \
                IOHIDMultiAxisPointerEventData * pointer = (IOHIDMultiAxisPointerEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerX): \
                        value = IOHIDEventValueFloat(pointer->position.x, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerY): \
                        value = IOHIDEventValueFloat(pointer->position.y, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerZ): \
                        value = IOHIDEventValueFloat(pointer->position.z, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerRx): \
                        value = IOHIDEventValueFloat(pointer->rotation.x, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerRy): \
                        value = IOHIDEventValueFloat(pointer->rotation.y, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerRz): \
                        value = IOHIDEventValueFloat(pointer->rotation.z, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerButtonMask): \
                        value = pointer->button.mask;       \
                        break;                              \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeNavigationSwipe:            \
        case kIOHIDEventTypeDockSwipe:                  \
        case kIOHIDEventTypeFluidTouchGesture:          \
        case kIOHIDEventTypeBoundaryScroll:             \
            {                                           \
                IOHIDSwipeEventData * swipe = (IOHIDSwipeEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeMask):       \
                    /*
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollMask):    \
                    */                                  \
                        value = swipe->swipeMask;       \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeMotion):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollMotion):    \
                    */                                  \
                        value = swipe->gestureMotion;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeProgress):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollProgress):    \
                    */                                  \
                        value = IOHIDEventValueFloat(swipe->progress, isFixed);       \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipePositionX):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGesturePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollPositionX):    \
                    */                                  \
                        value = IOHIDEventValueFloat(swipe->position.x, isFixed);       \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipePositionY):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGesturePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollPositionY):    \
                    */                                  \
                        value = IOHIDEventValueFloat(swipe->position.y, isFixed);       \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeFlavor):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollFlavor):    \
                    */                                  \
                        value = swipe->flavor;          \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeTemperature:                \
            {                                           \
                IOHIDTemperatureEventData * temp = (IOHIDTemperatureEventData *)eventData; \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTemperatureLevel):       \
                        value = IOHIDEventValueFloat(temp->level, isFixed);\
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeTranslation:                \
        case kIOHIDEventTypeRotation:                   \
        case kIOHIDEventTypeScroll:                     \
        case kIOHIDEventTypeScale:                      \
        case kIOHIDEventTypeVelocity:                   \
        case kIOHIDEventTypeOrientation:                \
            {                                           \
                IOHIDAxisEventData * axis = (IOHIDAxisEventData *)eventData; \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationX):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationX):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollX):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleX):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityX):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationRadius):  \
                    */                                                              \
                        value = IOHIDEventValueFloat(axis->position.x, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationY):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationY):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollY):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleY):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityY):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationAzimuth): \
                    */                                                              \
                        value = IOHIDEventValueFloat(axis->position.y, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationZ):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationZ):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollZ):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleZ):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityZ):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationAltitude):\
                    */                                                              \
                        value = IOHIDEventValueFloat(axis->position.z, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollIsPixels):         \
                        value = ((axis->options & kIOHIDEventOptionPixelUnits) != 0);\
                        break;                              \
                };                                          \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeAmbientLightSensor:         \
            {                                           \
                IOHIDAmbientLightSensorEventData * alsEvent = (IOHIDAmbientLightSensorEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorLevel): \
                        value = alsEvent->level;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel0): \
                        value = alsEvent->ch0;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel1): \
                        value = alsEvent->ch1;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel2): \
                        value = alsEvent->ch2;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel3): \
                        value = alsEvent->ch3;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightDisplayBrightnessChanged): \
                        value = alsEvent->brightnessChanged; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightColorSpace): \
                        value = alsEvent->colorSpace;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightColorComponent0): \
                        value = alsEvent->colorComponent0; \
                        break;                             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightColorComponent1): \
                        value = alsEvent->colorComponent1; \
                        break;                           \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightColorComponent2): \
                        value = alsEvent->colorComponent2; \
                        break;                           \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeProximity:                  \
            {                                           \
                IOHIDProximityEventData * proxEvent = (IOHIDProximityEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProximityDetectionMask): \
                        value = proxEvent->detectionMask; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProximityLevel): \
                        value = proxEvent->level; \
                        break;                          \
                    };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeLED:                   \
            {                                           \
                IOHIDLEDEventData * ledEvent = (IOHIDLEDEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldLEDMask):    \
                        value = ledEvent->mask;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldLEDNumber):  \
                        value = ledEvent->number;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldLEDState):   \
                        value = ledEvent->state;         \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeKeyboard:                   \
            {                                           \
                IOHIDKeyboardEventData * keyEvent = (IOHIDKeyboardEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardUsagePage): \
                        value = keyEvent->usagePage;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardUsage):     \
                        value = keyEvent->usage;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardDown):    \
                        value = keyEvent->down;         \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardRepeat):  \
                        value = (keyEvent->options & kIOHIDEventOptionIsRepeat);\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardPressCount): \
                        value = keyEvent->pressCount;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardLongPress): \
                        GET_SUBFIELD_VALUE(value, keyEvent->flags, kIOHIDKeyboardLongPressBit, kIOHIDKeyboardLongPressMask); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardClickSpeed): \
                        GET_SUBFIELD_VALUE(value, keyEvent->flags, kIOHIDKeyboardClickSpeedStartBit, kIOHIDKeyboardClickSpeedMask); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardSlowKeyPhase): \
                        GET_SUBFIELD_VALUE(value, keyEvent->flags, kIOHIDKeyboardSlowKeyPhaseBit, kIOHIDKeyboardSlowKeyPhaseMask); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardMouseKeyToggle): \
                        GET_SUBFIELD_VALUE(value, keyEvent->flags, kIOHIDKeyboardMouseKeyToggleBit, kIOHIDKeyboardMouseKeyToggleMask); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeDigitizer:                  \
            {                                           \
                IOHIDDigitizerEventData * digEvent = (IOHIDDigitizerEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerX): \
                        value = IOHIDEventValueFloat(digEvent->position.x, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerY): \
                        value = IOHIDEventValueFloat(digEvent->position.y, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerZ): \
                        value = IOHIDEventValueFloat(digEvent->position.z, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerButtonMask): \
                        value = digEvent->buttonMask;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerType): \
                        value = digEvent->transducerType; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIndex): \
                        value = digEvent->transducerIndex; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIdentity): \
                        value = digEvent->identity;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerEventMask): \
                        value = digEvent->eventMask;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerChildEventMask): \
                        value = digEvent->childEventMask; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerRange): \
                        value = (digEvent->options & kIOHIDTransducerRange) != 0;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIsDisplayIntegrated): \
                        value = (digEvent->options & kIOHIDTransducerDisplayIntegrated) != 0;         \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTouch): \
                        value = (digEvent->options & kIOHIDTransducerTouch) != 0;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerCollection): \
                        value = (digEvent->options & kIOHIDEventOptionIsCollection) != 0;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerPressure): \
                        value = IOHIDEventValueFloat(digEvent->pressure, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAuxiliaryPressure): \
                        value = IOHIDEventValueFloat(digEvent->auxPressure, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTwist): \
                        value = IOHIDEventValueFloat(digEvent->twist, isFixed);        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltX): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltY): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAltitude): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAzimuth): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIrregularity): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQualityRadiiAccuracy): \
                        switch ( digEvent->orientationType ) {\
                            case kIOHIDDigitizerOrientationTypeTilt:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltX): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.tilt.x, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltY): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.tilt.y, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                            case kIOHIDDigitizerOrientationTypePolar:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAltitude): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.polar.altitude, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAzimuth): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.polar.azimuth, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.polar.quality, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.polar.density, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.polar.majorRadius, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.polar.minorRadius, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                            case kIOHIDDigitizerOrientationTypeQuality:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.quality, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.density, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIrregularity): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.irregularity, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.majorRadius, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.minorRadius, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQualityRadiiAccuracy): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.accuracy, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                        };                              \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerGenerationCount): \
                        value = digEvent->generationCount; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerWillUpdateMask): \
                        value = digEvent->willUpdateMask; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDidUpdateMask): \
                        value = digEvent->didUpdateMask;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerEstimatedMask): { \
                        uint32_t i = 0;                 \
                        i |= (digEvent->eventMask & kIOHIDDigitizerEventEstimatedAltitude) ? kIOHIDDigitizerEventUpdateAltitudeMask : 0; \
                        i |= (digEvent->eventMask & kIOHIDDigitizerEventEstimatedAzimuth) ? kIOHIDDigitizerEventUpdateAzimuthMask : 0; \
                        i |= (digEvent->eventMask & kIOHIDDigitizerEventEstimatedPressure) ? kIOHIDDigitizerEventUpdatePressureMask : 0; \
                        value = i;                      \
                        break;                          \
                    }                                   \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeZoomToggle:                 \
            {                                           \
            /*  IOHIDZoomToggleEventData * zoom = (IOHIDZoomToggleEventData *)eventData; */\
                switch ( fieldOffset ) {                \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeSymbolicHotKey:             \
            {                                           \
                IOHIDSymbolicHotKeyEventData * symbolicEvent = (IOHIDSymbolicHotKeyEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSymbolicHotKeyValue): \
                        value = symbolicEvent->hotKey;  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSymbolicHotKeyIsCGSEvent): \
                        value = (symbolicEvent->options & kIOHIDSymbolicHotKeyOptionIsCGSHotKey) != 0; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypePower:                      \
            {                                           \
                IOHIDPowerEventData * pwrEvent = (IOHIDPowerEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerMeasurement): \
                        value = IOHIDEventValueFloat(pwrEvent->measurement, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerType): \
                        value = pwrEvent->powerType;  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerSubType): \
                        value = pwrEvent->powerSubType; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeAtmosphericPressure:        \
            {                                           \
                IOHIDAtmosphericPressureEventData * apEvent = (IOHIDAtmosphericPressureEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAtmosphericPressureLevel): \
                        value = IOHIDEventValueFloat(apEvent->level, isFixed);  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAtmosphericSequence): \
                        value = apEvent->sequence;      \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeMotionActivity:             \
            {                                           \
                IOHIDMotionActivityEventData * maEvent = (IOHIDMotionActivityEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMotionActivityConfidence): \
                        value = IOHIDEventValueFloat(maEvent->confidence, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMotionActivityActivityType): \
                        value = maEvent->activityType;  \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeMotionGesture:              \
            {                                           \
                IOHIDMotionGestureEventData * mgEvent = (IOHIDMotionGestureEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMotionGestureProgress): \
                        value = IOHIDEventValueFloat(mgEvent->progress, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMotionGestureGestureType): \
                        value = mgEvent->gestureType;   \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeGameController:             \
            {                                           \
                IOHIDGameControllerEventData * gcEvent = (IOHIDGameControllerEventData *)eventData; \
                    switch ( fieldOffset ) {\
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerType): \
                            value = gcEvent->controllerType;   \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerDirectionPadUp): \
                            value = IOHIDEventValueFloat(gcEvent->dpad.up, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerDirectionPadDown): \
                            value = IOHIDEventValueFloat(gcEvent->dpad.down, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerDirectionPadLeft): \
                            value = IOHIDEventValueFloat(gcEvent->dpad.left, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerDirectionPadRight): \
                            value = IOHIDEventValueFloat(gcEvent->dpad.right, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerFaceButtonX): \
                            value = IOHIDEventValueFloat(gcEvent->face.x, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerFaceButtonY): \
                            value = IOHIDEventValueFloat(gcEvent->face.y, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerFaceButtonA): \
                            value = IOHIDEventValueFloat(gcEvent->face.a, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerFaceButtonB): \
                            value = IOHIDEventValueFloat(gcEvent->face.b, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerShoulderButtonL1): \
                            value = IOHIDEventValueFloat(gcEvent->shoulder.l1, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerShoulderButtonR1): \
                            value = IOHIDEventValueFloat(gcEvent->shoulder.r1, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerShoulderButtonL2): \
                            value = IOHIDEventValueFloat(gcEvent->shoulder.l2, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerShoulderButtonR2): \
                            value = IOHIDEventValueFloat(gcEvent->shoulder.r2, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerJoyStickAxisX): \
                            value = IOHIDEventValueFloat(gcEvent->joystick.x, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerJoyStickAxisY): \
                            value = IOHIDEventValueFloat(gcEvent->joystick.y, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerJoyStickAxisZ): \
                            value = IOHIDEventValueFloat(gcEvent->joystick.z, isFixed); \
                            break;                          \
                        case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerJoyStickAxisRz): \
                            value = IOHIDEventValueFloat(gcEvent->joystick.rz, isFixed); \
                            break;                          \
               };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeHumidity:              \
            {                                           \
                IOHIDHumidityEventData * hEvent = (IOHIDHumidityEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldHumidityRH): \
                        value = IOHIDEventValueFloat(hEvent->rh, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldHumiditySequence): \
                        value = hEvent->sequence;       \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeForce:        \
            {                                           \
                IOHIDForceEventData * forceEvent = (IOHIDForceEventData *)eventData; \
                switch ( fieldOffset ) { \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldForceBehavior): \
                        value = forceEvent->behavior;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldForceTransitionProgress): \
                        value = IOHIDEventValueFloat(forceEvent->progress,isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldForceStage): \
                        value = forceEvent->stage;      \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldForceStagePressure): \
                        value = IOHIDEventValueFloat(forceEvent->stageProgress,isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeBrightness:                 \
            {                                           \
                IOHIDBrightnessEventData * hEvent = (IOHIDBrightnessEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCurrentBrightness): \
                        value = IOHIDEventValueFloat(hEvent->currentBrightness,isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTargetBrightness): \
                        value = IOHIDEventValueFloat(hEvent->targetBrightness,isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTransitionTime): \
                        value = hEvent->transitionTime;     \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
    };                                                  \
}

#define GET_EVENTDATA_DATA(eventData, fieldEvType, fieldOffset, value)\
{                                                       \
    value = NULL;                                       \
    switch ( fieldEvType ) {                            \
        case kIOHIDEventTypeVendorDefined:              \
        {                                               \
            IOHIDVendorDefinedEventData * sysDef = (IOHIDVendorDefinedEventData*)eventData; \
            switch ( fieldOffset ) {                    \
                case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedData): \
                    value = sysDef->data;               \
                    break;                              \
            };                                          \
        }                                               \
        break;                                          \
        case kIOHIDEventTypeUnicode:                    \
        {                                               \
            IOHIDUnicodeEventData * charEvent = (IOHIDUnicodeEventData*)eventData; \
            switch ( fieldOffset ) {                    \
                case IOHIDEventFieldOffset(kIOHIDEventFieldUnicodePayload): \
                    value = charEvent->payload;         \
                    break;                              \
            };                                          \
        }                                               \
        break;                                          \
    };                                                  \
}

#define GET_EVENT_VALUE(event, field, value, options) \
{   IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);  \
    uint32_t        fieldOffset = IOHIDEventFieldOffset(field);     \
    IOHIDEventRef   ev          = (fieldEvType == kIOHIDEventTypeNULL) ? event : NULL;  \
    if ( ev || (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {\
        GET_EVENTDATA_VALUE(GET_EVENTDATA(ev),fieldEvType,fieldOffset,value, false);\
    }                                                               \
}

#define GET_EVENT_VALUE_FIXED(event, field, value, options) \
{   IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);  \
    uint32_t        fieldOffset = IOHIDEventFieldOffset(field);     \
    IOHIDEventRef   ev          = (fieldEvType == kIOHIDEventTypeNULL) ? event : NULL;  \
    if ( ev || (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {\
        GET_EVENTDATA_VALUE(GET_EVENTDATA(ev),fieldEvType,fieldOffset,value, true);\
    }                                                               \
}

#define GET_EVENT_DATA(event, field, value, options) \
{   IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);  \
    uint32_t        fieldOffset = IOHIDEventFieldOffset(field);     \
    IOHIDEventRef   ev          = (fieldEvType == kIOHIDEventTypeNULL) ? event : NULL;  \
    if ( ev || (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {\
        GET_EVENTDATA_DATA(GET_EVENTDATA(ev),fieldEvType,fieldOffset,value);\
    }                                                               \
}

//==============================================================================
// IOHIDEventSetValue MACRO
//==============================================================================
#define SET_EVENTDATA_VALUE(eventData, fieldEvType, fieldOffset, value, isFixed) \
{   switch ( fieldEvType ) {                            \
        case kIOHIDEventTypeNULL:                       \
        case kIOHIDEventTypeCollection:                 \
            {                                           \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldIsRelative): \
                        if ( value )                        \
                            eventData->options &= ~kIOHIDEventOptionIsAbsolute; \
                        else                                \
                            eventData->options |= kIOHIDEventOptionIsAbsolute; \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldIsCollection): \
                        if ( value )                        \
                            eventData->options |= kIOHIDEventOptionIsCollection; \
                        else                                \
                            eventData->options &= ~kIOHIDEventOptionIsCollection; \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldIsPixelUnits): \
                        if ( value )                        \
                            eventData->options |= kIOHIDEventOptionIsPixelUnits; \
                        else                                \
                            eventData->options &= ~kIOHIDEventOptionIsPixelUnits; \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldIsCenterOrigin): \
                        if ( value )                        \
                            eventData->options |= kIOHIDEventOptionIsCenterOrigin; \
                        else                                \
                            eventData->options &= ~kIOHIDEventOptionIsCenterOrigin; \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldIsBuiltIn): \
                        if ( value )                        \
                            eventData->options |= kIOHIDEventOptionIsBuiltIn; \
                        else                                \
                            eventData->options &= ~kIOHIDEventOptionIsBuiltIn; \
                        break;                              \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeVendorDefined:              \
            {                                           \
                IOHIDVendorDefinedEventData * sysDef = (IOHIDVendorDefinedEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedUsagePage): \
                        sysDef->usagePage = value;      \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedUsage): \
                        sysDef->usage = value;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedVersion): \
                        sysDef->version = value;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedData): \
                        if (sysDef->data)               \
                            *((typeof(value)*) sysDef->data) = value; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeUnicode:                    \
            {                                           \
                IOHIDUnicodeEventData * character = (IOHIDUnicodeEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldUnicodeEncoding): \
                        character->encoding = value;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldUnicodeQuality): \
                        character->quality = IOHIDEventValueFixed(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldUnicodeLength): \
                        character->length = value;      \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldUnicodePayload): \
                        if (character->payload)         \
                            *((typeof(value)*) character->payload) = value; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeBiometric:                  \
        {                                               \
            IOHIDBiometricEventData * biometric = (IOHIDBiometricEventData*)eventData; \
            switch ( fieldOffset ) {                    \
                case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricEventType): \
                    biometric->eventType = value;       \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricLevel): \
                    biometric->level = IOHIDEventValueFixed(value, isFixed); \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricUsagePage): \
                    biometric->usagePage = value;       \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricUsage): \
                    biometric->usage = value;           \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricTapCount): \
                    biometric->tapCount = value;        \
                    break;                              \
            };                                          \
        };                                              \
        break;                                          \
        case kIOHIDEventTypeProgress:                   \
            {                                           \
                IOHIDProgressEventData * progress = (IOHIDProgressEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    /* case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricEventType): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProgressEventType): \
                        progress->eventType = value;    \
                        break;                          \
                    /* case IOHIDEventFieldOffset(kIOHIDEventFieldBiometricLevel): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProgressLevel): \
                        progress->level = IOHIDEventValueFixed(value, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeButton:                     \
            {                                           \
                IOHIDButtonEventData * button = (IOHIDButtonEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonMask): \
                        button->mask = value;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonNumber): \
                        button->number = value;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonClickCount): \
                        button->clickCount = value;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonPressure): \
                        button->pressure = IOHIDEventValueFixed(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonState): \
                        button->state = value;          \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeCompass:                     \
        case kIOHIDEventTypeGyro:              \
        case kIOHIDEventTypeAccelerometer:              \
            {                                           \
                IOHIDMotionEventData * motion = (IOHIDMotionEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassX): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroX): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerX): \
                        motion->position.x = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassY): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroY): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerY): \
                        motion->position.y = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassZ): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroZ): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerZ): \
                        motion->position.z = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassType): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroType): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerType): \
                        motion->motionType = value;     \
                        break;                          \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassSubType): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroSubType): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerSubType): \
                        motion->motionSubType = value;     \
                        break;                          \
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldCompassSequence): */\
                    /*case IOHIDEventFieldOffset(kIOHIDEventFieldGyroSequence): */\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerSequence): \
                        motion->motionSequence = value;     \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypePointer:                     \
            {                                           \
                IOHIDPointerEventData * pointer = (IOHIDPointerEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPointerX): \
                        pointer->position.x = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPointerY): \
                        pointer->position.y = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPointerZ): \
                        pointer->position.z = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPointerButtonMask): \
                        pointer->button.mask = value;       \
                        break;                              \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeMultiAxisPointer:                     \
            {                                           \
                IOHIDMultiAxisPointerEventData * pointer = (IOHIDMultiAxisPointerEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerX): \
                        pointer->position.x = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerY): \
                        pointer->position.y = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerZ): \
                        pointer->position.z = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerRx): \
                        pointer->rotation.x = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerRy): \
                        pointer->rotation.y = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerRz): \
                        pointer->rotation.z = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMultiAxisPointerButtonMask): \
                        pointer->button.mask = value;       \
                        break;                              \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeNavigationSwipe:            \
        case kIOHIDEventTypeDockSwipe:                  \
        case kIOHIDEventTypeFluidTouchGesture:          \
        case kIOHIDEventTypeBoundaryScroll:             \
            {                                           \
                IOHIDSwipeEventData * swipe = (IOHIDSwipeEventData *)eventData; \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeMask):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollMask):    \
                    */                                  \
                        swipe->swipeMask = value;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeMotion):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollMotion):    \
                    */                                  \
                        swipe->gestureMotion = value;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeProgress):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollProgress):    \
                    */                                  \
                        swipe->progress = IOHIDEventValueFixed(value, isFixed);\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipePositionX):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGesturePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollPositionX):    \
                    */                                  \
                        swipe->position.x = IOHIDEventValueFixed(value, isFixed);\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipePositionY):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGesturePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollPositionY):    \
                    */                                  \
                        swipe->position.y = IOHIDEventValueFixed(value, isFixed);\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeFlavor):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollFlavor):    \
                    */                                  \
                        swipe->flavor = value;          \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeTemperature:                \
            {                                           \
                IOHIDTemperatureEventData * temp = (IOHIDTemperatureEventData *)eventData; \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTemperatureLevel):       \
                        temp->level = IOHIDEventValueFixed(value, isFixed);\
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeTranslation:                \
        case kIOHIDEventTypeRotation:                   \
        case kIOHIDEventTypeScroll:                     \
        case kIOHIDEventTypeScale:                      \
        case kIOHIDEventTypeVelocity:                   \
        case kIOHIDEventTypeOrientation:                \
            {                                           \
                IOHIDAxisEventData * axis = (IOHIDAxisEventData *)eventData;    \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationX):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationX):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollX):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleX):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityX):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerX);         \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationRadius):  \
                    */                                                              \
                        axis->position.x = IOHIDEventValueFixed(value, isFixed);             \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationY):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationY):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollY):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleY):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityY):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerY);         \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationAzimuth): \
                    */                                                              \
                        axis->position.y = IOHIDEventValueFixed(value, isFixed);             \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationZ):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationZ):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollZ):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleZ):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityZ):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerZ);         \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationAltitude):\
                    */                                                              \
                        axis->position.z = IOHIDEventValueFixed(value, isFixed);             \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollIsPixels):         \
                        if ( value )                        \
                            axis->options |= kIOHIDEventOptionPixelUnits;           \
                        else                                \
                            axis->options &= ~kIOHIDEventOptionPixelUnits;           \
                        break;                              \
                };                                          \
            }                                               \
            break;                                          \
        case kIOHIDEventTypeAmbientLightSensor:             \
            {                                               \
                IOHIDAmbientLightSensorEventData * alsEvent = (IOHIDAmbientLightSensorEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorLevel): \
                        alsEvent->level = value;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel0): \
                        alsEvent->ch0 = value;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel1): \
                        alsEvent->ch1 = value;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel2): \
                        alsEvent->ch2 = value;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel3): \
                        alsEvent->ch3 = value;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightDisplayBrightnessChanged): \
                        alsEvent->brightnessChanged = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightColorSpace): \
                        alsEvent->colorSpace = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightColorComponent0): \
                        alsEvent->colorComponent0 = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightColorComponent1): \
                        alsEvent->colorComponent1 = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightColorComponent2): \
                        alsEvent->colorComponent2 = value; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeProximity:                  \
            {                                           \
                IOHIDProximityEventData * proxEvent = (IOHIDProximityEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProximityDetectionMask): \
                        proxEvent->detectionMask = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProximityLevel): \
                        proxEvent->level = value; \
                        break;                          \
                    };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeLED:                   \
            {                                           \
                IOHIDLEDEventData * ledEvent = (IOHIDLEDEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldLEDMask):    \
                        ledEvent->mask = value;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldLEDNumber):  \
                        ledEvent->number = value;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldLEDState):   \
                        ledEvent->state = value;         \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeKeyboard:                       \
            {                                               \
                IOHIDKeyboardEventData * keyEvent = (IOHIDKeyboardEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardUsagePage): \
                        keyEvent->usagePage = value;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardUsage):     \
                        keyEvent->usage = value;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardDown):    \
                        keyEvent->down = value;         \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardPressCount): \
                        keyEvent->pressCount = value;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardLongPress): \
                        SET_SUBFIELD_VALUE(keyEvent->flags, value, kIOHIDKeyboardLongPressBit, kIOHIDKeyboardLongPressMask); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardClickSpeed): \
                        SET_SUBFIELD_VALUE(keyEvent->flags, value, kIOHIDKeyboardClickSpeedStartBit, kIOHIDKeyboardClickSpeedMask); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardSlowKeyPhase): \
                        SET_SUBFIELD_VALUE(keyEvent->flags, value, kIOHIDKeyboardSlowKeyPhaseBit, kIOHIDKeyboardSlowKeyPhaseMask);\
                        break;                          \
                   case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardMouseKeyToggle): \
                        SET_SUBFIELD_VALUE(keyEvent->flags, value, kIOHIDKeyboardMouseKeyToggleBit, kIOHIDKeyboardMouseKeyToggleMask); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardRepeat):  \
                        if ( value )                        \
                            keyEvent->options |= kIOHIDEventOptionIsRepeat;            \
                        else                                \
                            keyEvent->options &= ~kIOHIDEventOptionIsRepeat;           \
                        break;                              \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeDigitizer:                  \
            {                                           \
                IOHIDDigitizerEventData * digEvent = (IOHIDDigitizerEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerX): \
                        digEvent->position.x = IOHIDEventValueFixed(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerY): \
                        digEvent->position.y = IOHIDEventValueFixed(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerZ): \
                        digEvent->position.z = IOHIDEventValueFixed(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerButtonMask): \
                        digEvent->buttonMask = value;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerType): \
                        digEvent->transducerType = value;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIndex): \
                        digEvent->transducerIndex = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIdentity): \
                        digEvent->identity = value;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerEventMask): \
                        digEvent->eventMask = value;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerChildEventMask): \
                        digEvent->childEventMask = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerRange): \
                        if ( value )                        \
                            digEvent->options |= kIOHIDTransducerRange;         \
                        else                                \
                            digEvent->options &= ~kIOHIDTransducerRange;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIsDisplayIntegrated): \
                        if ( value )                        \
                            digEvent->options |= kIOHIDTransducerDisplayIntegrated;         \
                        else                                \
                            digEvent->options &= kIOHIDTransducerDisplayIntegrated;        \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTouch): \
                        if ( value )                        \
                            digEvent->options |= kIOHIDTransducerTouch;         \
                        else                                \
                            digEvent->options &= ~kIOHIDTransducerTouch;        \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerCollection): \
                        if ( value )                        \
                            digEvent->options |= kIOHIDEventOptionIsCollection;         \
                        else                                \
                            digEvent->options &= ~kIOHIDEventOptionIsCollection;        \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerPressure): \
                        digEvent->pressure = IOHIDEventValueFixed(value, isFixed);  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAuxiliaryPressure): \
                        digEvent->auxPressure = IOHIDEventValueFixed(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTwist): \
                        digEvent->twist = IOHIDEventValueFixed(value, isFixed);        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltX): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltY): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAltitude): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAzimuth): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIrregularity): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQualityRadiiAccuracy): \
                        switch ( digEvent->orientationType ) {\
                            case kIOHIDDigitizerOrientationTypeTilt:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltX): \
                                        digEvent->orientation.tilt.x = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltY): \
                                        digEvent->orientation.tilt.y = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                            case kIOHIDDigitizerOrientationTypePolar:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAltitude): \
                                        digEvent->orientation.polar.altitude = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAzimuth): \
                                        digEvent->orientation.polar.azimuth = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                                        digEvent->orientation.polar.quality = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                                        digEvent->orientation.polar.density = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                                        digEvent->orientation.polar.majorRadius = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                                        digEvent->orientation.polar.minorRadius = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                            case kIOHIDDigitizerOrientationTypeQuality:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                                        digEvent->orientation.quality.quality = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                                        digEvent->orientation.quality.density = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIrregularity): \
                                        digEvent->orientation.quality.irregularity = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                                        digEvent->orientation.quality.majorRadius = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                                        digEvent->orientation.quality.minorRadius = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQualityRadiiAccuracy): \
                                        digEvent->orientation.quality.accuracy = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                        };                              \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerGenerationCount): \
                        digEvent->generationCount = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerWillUpdateMask): \
                        digEvent->willUpdateMask = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDidUpdateMask): \
                        digEvent->didUpdateMask = value;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerEstimatedMask): { \
                        uint32_t i = value;             \
                        digEvent->eventMask |= ((i & kIOHIDDigitizerEventUpdateAltitudeMask) ? kIOHIDDigitizerEventEstimatedAltitude : 0); \
                        digEvent->eventMask |= ((i & kIOHIDDigitizerEventUpdateAzimuthMask) ? kIOHIDDigitizerEventEstimatedAzimuth : 0); \
                        digEvent->eventMask |= ((i & kIOHIDDigitizerEventUpdatePressureMask) ? kIOHIDDigitizerEventEstimatedPressure : 0); \
                        break;                          \
                    }                                   \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeZoomToggle:                 \
            {                                           \
            /*  IOHIDZoomToggleEventData * zoom = (IOHIDZoomToggleEventData *)eventData; */\
                switch ( fieldOffset ) {                \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeSymbolicHotKey:             \
            {                                           \
                IOHIDSymbolicHotKeyEventData * symbolicEvent = (IOHIDSymbolicHotKeyEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSymbolicHotKeyValue): \
                        symbolicEvent->hotKey = value;  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSymbolicHotKeyIsCGSEvent): \
                        if ( value )                    \
                            symbolicEvent->options |= kIOHIDSymbolicHotKeyOptionIsCGSHotKey; \
                        else                            \
                            symbolicEvent->options &= ~kIOHIDSymbolicHotKeyOptionIsCGSHotKey; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypePower:                      \
            {                                           \
                IOHIDPowerEventData * pwrEvent = (IOHIDPowerEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerMeasurement): \
                        pwrEvent->measurement = IOHIDEventValueFixed(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerType): \
                        pwrEvent->powerType = value;  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerSubType): \
                        pwrEvent->powerSubType = value; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeAtmosphericPressure:        \
            {                                           \
                IOHIDAtmosphericPressureEventData * apEvent = (IOHIDAtmosphericPressureEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAtmosphericPressureLevel): \
                        apEvent->level = IOHIDEventValueFixed(value, isFixed);  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAtmosphericSequence): \
                        apEvent->sequence = value;      \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeMotionActivity:             \
            {                                           \
                IOHIDMotionActivityEventData * maEvent = (IOHIDMotionActivityEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMotionActivityConfidence): \
                        maEvent->confidence = IOHIDEventValueFixed(value, isFixed);  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMotionActivityActivityType): \
                        maEvent->activityType = value;  \
                        break;                          \
                    };                                  \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeMotionGesture:              \
            {                                           \
                IOHIDMotionGestureEventData * mgEvent = (IOHIDMotionGestureEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMotionGestureProgress): \
                        mgEvent->progress = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMotionGestureGestureType): \
                        mgEvent->gestureType = value;   \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeGameController:             \
            {                                           \
                IOHIDGameControllerEventData * gcEvent = (IOHIDGameControllerEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerType): \
                        gcEvent->controllerType = value;   \
                    break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerDirectionPadUp): \
                        gcEvent->dpad.up = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerDirectionPadDown): \
                        gcEvent->dpad.down = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerDirectionPadLeft): \
                        gcEvent->dpad.left = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerDirectionPadRight): \
                        gcEvent->dpad.right = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerFaceButtonX): \
                        gcEvent->face.x = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerFaceButtonY): \
                        gcEvent->face.y = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerFaceButtonA): \
                        gcEvent->face.a = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerFaceButtonB): \
                        gcEvent->face.b = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerShoulderButtonL1): \
                        gcEvent->shoulder.l1 = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerShoulderButtonR1): \
                        gcEvent->shoulder.r1 = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerShoulderButtonL2): \
                        gcEvent->shoulder.l2 = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerShoulderButtonR2): \
                        gcEvent->shoulder.r2 = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerJoyStickAxisX): \
                        gcEvent->joystick.x = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerJoyStickAxisY): \
                        gcEvent->joystick.y = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerJoyStickAxisZ): \
                        gcEvent->joystick.z = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldGameControllerJoyStickAxisRz): \
                        gcEvent->joystick.rz = IOHIDEventValueFloat(value, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeHumidity:              \
            {                                           \
                IOHIDHumidityEventData * hEvent = (IOHIDHumidityEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldHumidityRH): \
                        hEvent->rh = IOHIDEventValueFloat(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldHumiditySequence): \
                        hEvent->sequence = value; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeForce:                      \
            {                                           \
                IOHIDForceEventData * forceEvent = (IOHIDForceEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldForceBehavior): \
                        forceEvent->behavior = value;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldForceTransitionProgress): \
                        forceEvent->progress = IOHIDEventValueFixed(value,isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldForceStage): \
                        forceEvent->stage = value;      \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldForceStagePressure): \
                        forceEvent->stageProgress = IOHIDEventValueFixed(value,isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeBrightness:                 \
            {                                           \
                IOHIDBrightnessEventData * hEvent = (IOHIDBrightnessEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCurrentBrightness): \
                        hEvent->currentBrightness = IOHIDEventValueFloat(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTargetBrightness): \
                        hEvent->targetBrightness = IOHIDEventValueFloat(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTransitionTime): \
                        hEvent->transitionTime = value; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
    };                                                  \
}

#define SET_EVENT_VALUE(event, field, value, options)               \
{   IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);  \
    uint32_t        fieldOffset = IOHIDEventFieldOffset(field);     \
    IOHIDEventRef   ev          = (fieldEvType == kIOHIDEventTypeNULL) ? event : NULL;  \
    if ( ev || (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {\
        SET_EVENTDATA_VALUE(GET_EVENTDATA(ev),fieldEvType,fieldOffset,value, false);\
    }                                                               \
}

#define SET_EVENT_VALUE_FIXED(event, field, value, options)               \
{   IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);  \
    uint32_t        fieldOffset = IOHIDEventFieldOffset(field);     \
    IOHIDEventRef   ev          = (fieldEvType == kIOHIDEventTypeNULL) ? event : NULL;  \
    if ( ev || (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {\
        SET_EVENTDATA_VALUE(GET_EVENTDATA(ev),fieldEvType,fieldOffset,value, true);\
    }                                                               \
}

#endif /* _IOKIT_HID_IOHIDEVENTDATA_H */
