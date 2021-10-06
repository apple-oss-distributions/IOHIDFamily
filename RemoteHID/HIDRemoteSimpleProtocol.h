//
//  HIDRemoteSimpleProtocol_h
//  RemoteHID
//
//  Created by yg on 1/9/18.
//  Copyright © 2018 apple. All rights reserved.
//

#ifndef HIDRemoteSimpleProtocol_h
#define HIDRemoteSimpleProtocol_h

#include <TargetConditionals.h>
#include <AssertMacros.h>

#define HID_AACP_MESSAGE_TYPE   0x800  //AACP_CUSTOM_MESSAGE_TYPE_SENSOR

typedef enum {
    HIDPacketTypeDeviceConnect      = 0,
    HIDPacketTypeDeviceDisconnect   = 1,
    HIDPacketTypeHandleReport       = 2,
    HIDPacketTypeSetReport          = 3,
    HIDPacketTypeGetReport          = 4,
} HIDPacketType;

#if TARGET_OS_RTKIT
typedef enum {
    HIDReportTypeInput              = 0,
    HIDReportTypeOutput             = 1,
    HIDReportTypeFeature            = 2
} HIDReportType;
#endif

typedef struct __attribute__((__packed__)) {
    uint32_t        generation : 16;
    uint32_t        reserved1  : 16;
} HIDTransportHeader;

check_compile_time (sizeof (HIDTransportHeader) == 4);

typedef struct __attribute__((__packed__)) {
    uint32_t        deviceID   : 7;
    uint32_t        length     : 10;
    uint32_t        packetType : 3;
    uint32_t        hasTS      : 1;
    uint32_t        isResponse : 1;
    uint32_t        isError    : 1;
    uint32_t        reserved1  : 9;
} HIDDeviceHeader;

check_compile_time (sizeof (HIDDeviceHeader) == 4);

typedef struct __attribute__((__packed__)) {
    HIDDeviceHeader header;
    uint8_t         reportType;
    uint8_t         data[0];
} HIDDeviceReport;

check_compile_time (sizeof(HIDDeviceReport) == 5);

typedef struct __attribute__((__packed__)) {
    HIDDeviceHeader header;
    uint8_t         reportType;
    uint64_t        timestamp;
    uint8_t         data[0];
} HIDDeviceTimestampedReport;

check_compile_time (sizeof(HIDDeviceTimestampedReport) == 13);

typedef struct __attribute__((__packed__)) {
    HIDDeviceHeader header;
    uint8_t         data[0];
} HIDDeviceControl;

check_compile_time (sizeof(HIDDeviceControl) == 4);

#endif /* HIDRemoteSimpleProtocol_h */

