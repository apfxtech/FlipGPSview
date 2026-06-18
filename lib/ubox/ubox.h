#pragma once

// RX-only port of the UBOX u-blox protocol library. Only the byte-stream
// parsing path (sync -> header -> payload -> checksum -> typed message) is
// kept here; the transmit/build helpers from the original sources are omitted.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UBOX_SYNC1 0xb5
#define UBOX_SYNC2 0x62
#define UBOX_FRAME_OVERHEAD 8

#define UBX_NAV_CLASS 0x01
#define UBX_RXM_CLASS 0x02
#define UBX_INF_CLASS 0x04
#define UBX_ACK_CLASS 0x05
#define UBX_CFG_CLASS 0x06
#define UBX_UPD_CLASS 0x09
#define UBX_MON_CLASS 0x0A
#define UBX_AID_CLASS 0x0B
#define UBX_TIM_CLASS 0x0D
#define UBX_ESF_CLASS 0x10
#define UBX_MGA_CLASS 0x13
#define UBX_LOG_CLASS 0x21
#define UBX_SEC_CLASS 0x27
#define UBX_HNR_CLASS 0x28

#define UBX_ACK_ACK_MESSAGE 0x01
#define UBX_ACK_NAK_MESSAGE 0x00

#define UBX_NAV_PVT_MESSAGE 0x07
#define UBX_NAV_SAT_MESSAGE 0x35
#define UBX_NAV_ODO_MESSAGE 0x09
#define UBX_NAV_RESETODO_MESSAGE 0x10
#define UBX_NAV_TIMEUTC_MESSAGE 0x21

#define UBX_CFG_PMS_MESSAGE 0x86
#define UBX_CFG_ODO_MESSAGE 0x1e
#define UBX_CFG_NAV5_MESSAGE 0x24

#define UBX_MON_VER_MESSAGE 0x04

typedef struct UbloxFrame {
    uint8_t sync1;
    uint8_t sync2;
    uint8_t message_class;
    uint8_t id;
    uint16_t len;
    uint8_t* payload;
    uint8_t ck_a;
    uint8_t ck_b;
    bool valid;
} UbloxFrame;

typedef struct Ublox_NAV_PVT_Message {
    uint32_t iTOW;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t valid;
    uint32_t tAcc;
    int32_t nano;
    uint8_t fixType;
    uint8_t flags;
    uint8_t flags2;
    uint8_t numSV;
    int32_t lon;
    int32_t lat;
    int32_t height;
    int32_t hMSL;
    uint32_t hAcc;
    uint32_t vAcc;
    int32_t velN;
    int32_t velE;
    int32_t velD;
    int32_t gSpeed;
    int32_t headMot;
    uint32_t sAcc;
    uint32_t headAcc;
    uint16_t pDOP;
    uint16_t flags3;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
    uint8_t reserved4;
    int32_t headVeh;
    int16_t magDec;
    uint16_t magAcc;
} Ublox_NAV_PVT_Message;

typedef struct Ublox_NAV_ODO_Message {
    uint8_t version;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t reserved3;
    uint32_t iTOW;
    uint32_t distance;
    uint32_t totalDistance;
    uint32_t distanceStd;
} Ublox_NAV_ODO_Message;

typedef struct Ublox_NAV_TIMEUTC_Message {
    uint32_t iTOW;
    uint32_t tAcc;
    int32_t nano;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t valid;
} Ublox_NAV_TIMEUTC_Message;

uint16_t ubox_get_u16(const uint8_t* payload, size_t offset);
uint32_t ubox_get_u32(const uint8_t* payload, size_t offset);
const char* ubox_message_name(uint8_t message_class, uint8_t message_id);

#ifdef __cplusplus
}
#endif
