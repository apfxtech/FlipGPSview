#include "ubox_rx.h"

#include "debug.h"

#include <stdlib.h>

static void ubox_rx_checksum(UboxRx* rx, uint8_t byte) {
    rx->calculated_ck_a = rx->calculated_ck_a + byte;
    rx->calculated_ck_b = rx->calculated_ck_b + rx->calculated_ck_a;
}

static bool ubox_rx_reserve_payload(UboxRx* rx, uint16_t length) {
    if(length <= rx->payload_capacity) {
        LOG_TRACE_F(
            "UBOX::rx_reserve_payload: reuse capacity=%zu for length=%u",
            rx->payload_capacity,
            length);
        return true;
    }

    LOG_DEBUG_F(
        "UBOX::rx_reserve_payload: grow payload buffer from %zu to %u",
        rx->payload_capacity,
        length);
    uint8_t* payload = realloc(rx->frame.payload, length);
    if(payload == NULL) {
        LOG_ERROR_F("UBOX::rx_reserve_payload: realloc failed for length=%u", length);
        return false;
    }

    rx->frame.payload = payload;
    rx->payload_capacity = length;
    return true;
}

static void ubox_rx_restart_frame(UboxRx* rx) {
    LOG_TRACE("UBOX::rx_restart_frame");
    rx->state = UBOX_RX_STATE_SYNC1;
    rx->frame.sync1 = 0;
    rx->frame.sync2 = 0;
    rx->frame.message_class = 0;
    rx->frame.id = 0;
    rx->frame.len = 0;
    rx->frame.ck_a = 0;
    rx->frame.ck_b = 0;
    rx->frame.valid = false;
    rx->payload_index = 0;
    rx->calculated_ck_a = 0;
    rx->calculated_ck_b = 0;
}

static void ubox_rx_wait_for_next_frame(UboxRx* rx) {
    LOG_TRACE("UBOX::rx_wait_for_next_frame");
    rx->state = UBOX_RX_STATE_SYNC1;
    rx->payload_index = 0;
    rx->calculated_ck_a = 0;
    rx->calculated_ck_b = 0;
}

static UboxRxUpdateResult ubox_rx_parse_nav_pvt(UboxRx* rx) {
    uint8_t* payload = rx->frame.payload;

    if(rx->frame.message_class != UBX_NAV_CLASS || rx->frame.id != UBX_NAV_PVT_MESSAGE ||
       rx->frame.len != 92) {
        return UBOX_RX_UPDATE_NONE;
    }

    LOG_DEBUG("UBOX::rx_parse_nav_pvt: parsing NAV-PVT payload");
    Ublox_NAV_PVT_Message nav_pvt = {
        .iTOW = ubox_get_u32(payload, 0),
        .year = ubox_get_u16(payload, 4),
        .month = payload[6],
        .day = payload[7],
        .hour = payload[8],
        .min = payload[9],
        .sec = payload[10],
        .valid = payload[11],
        .tAcc = ubox_get_u32(payload, 12),
        .nano = (int32_t)ubox_get_u32(payload, 16),
        .fixType = payload[20],
        .flags = payload[21],
        .flags2 = payload[22],
        .numSV = payload[23],
        .lon = (int32_t)ubox_get_u32(payload, 24),
        .lat = (int32_t)ubox_get_u32(payload, 28),
        .height = (int32_t)ubox_get_u32(payload, 32),
        .hMSL = (int32_t)ubox_get_u32(payload, 36),
        .hAcc = ubox_get_u32(payload, 40),
        .vAcc = ubox_get_u32(payload, 44),
        .velN = (int32_t)ubox_get_u32(payload, 48),
        .velE = (int32_t)ubox_get_u32(payload, 52),
        .velD = (int32_t)ubox_get_u32(payload, 56),
        .gSpeed = (int32_t)ubox_get_u32(payload, 60),
        .headMot = (int32_t)ubox_get_u32(payload, 64),
        .sAcc = ubox_get_u32(payload, 68),
        .headAcc = ubox_get_u32(payload, 72),
        .pDOP = ubox_get_u16(payload, 76),
        .flags3 = ubox_get_u16(payload, 78),
        .reserved1 = payload[80],
        .reserved2 = payload[81],
        .reserved3 = payload[82],
        .reserved4 = payload[83],
        .headVeh = (int32_t)ubox_get_u32(payload, 84),
        .magDec = (int16_t)ubox_get_u16(payload, 88),
        .magAcc = ubox_get_u16(payload, 90),
    };

    rx->nav_pvt = nav_pvt;
    LOG_INFO("UBOX::rx_parse_nav_pvt: NAV-PVT message parsed");
    LOG_DEBUG_F(
        "UBOX::rx_parse_nav_pvt: fix=%u sv=%u lat=%ld lon=%ld hMSL=%ld gSpeed=%ld",
        nav_pvt.fixType,
        nav_pvt.numSV,
        (long)nav_pvt.lat,
        (long)nav_pvt.lon,
        (long)nav_pvt.hMSL,
        (long)nav_pvt.gSpeed);
    return UBOX_RX_UPDATE_NAV_PVT;
}

static UboxRxUpdateResult ubox_rx_parse_nav_odo(UboxRx* rx) {
    uint8_t* payload = rx->frame.payload;

    if(rx->frame.message_class != UBX_NAV_CLASS || rx->frame.id != UBX_NAV_ODO_MESSAGE ||
       rx->frame.len != 20) {
        return UBOX_RX_UPDATE_NONE;
    }

    LOG_DEBUG("UBOX::rx_parse_nav_odo: parsing NAV-ODO payload");
    Ublox_NAV_ODO_Message nav_odo = {
        .version = payload[0],
        .reserved1 = payload[1],
        .reserved2 = payload[2],
        .reserved3 = payload[3],
        .iTOW = ubox_get_u32(payload, 4),
        .distance = ubox_get_u32(payload, 8),
        .totalDistance = ubox_get_u32(payload, 12),
        .distanceStd = ubox_get_u32(payload, 16),
    };

    rx->nav_odo = nav_odo;
    LOG_INFO("UBOX::rx_parse_nav_odo: NAV-ODO message parsed");
    LOG_DEBUG_F(
        "UBOX::rx_parse_nav_odo: distance=%lu total=%lu std=%lu",
        (unsigned long)nav_odo.distance,
        (unsigned long)nav_odo.totalDistance,
        (unsigned long)nav_odo.distanceStd);
    return UBOX_RX_UPDATE_NAV_ODO;
}

static UboxRxUpdateResult ubox_rx_parse_nav_timeutc(UboxRx* rx) {
    uint8_t* payload = rx->frame.payload;

    if(rx->frame.message_class != UBX_NAV_CLASS || rx->frame.id != UBX_NAV_TIMEUTC_MESSAGE ||
       rx->frame.len != 20) {
        return UBOX_RX_UPDATE_NONE;
    }

    LOG_DEBUG("UBOX::rx_parse_nav_timeutc: parsing NAV-TIMEUTC payload");
    Ublox_NAV_TIMEUTC_Message nav_timeutc = {
        .iTOW = ubox_get_u32(payload, 0),
        .tAcc = ubox_get_u32(payload, 4),
        .nano = (int32_t)ubox_get_u32(payload, 8),
        .year = ubox_get_u16(payload, 12),
        .month = payload[14],
        .day = payload[15],
        .hour = payload[16],
        .min = payload[17],
        .sec = payload[18],
        .valid = payload[19],
    };

    rx->nav_timeutc = nav_timeutc;
    LOG_INFO("UBOX::rx_parse_nav_timeutc: NAV-TIMEUTC message parsed");
    LOG_DEBUG_F(
        "UBOX::rx_parse_nav_timeutc: %u-%02u-%02u %02u:%02u:%02u valid=0x%02X",
        nav_timeutc.year,
        nav_timeutc.month,
        nav_timeutc.day,
        nav_timeutc.hour,
        nav_timeutc.min,
        nav_timeutc.sec,
        nav_timeutc.valid);
    return UBOX_RX_UPDATE_NAV_TIMEUTC;
}

static UboxRxUpdateResult ubox_rx_parse_frame_payload(UboxRx* rx) {
    return (UboxRxUpdateResult)(
        ubox_rx_parse_nav_pvt(rx) | ubox_rx_parse_nav_odo(rx) | ubox_rx_parse_nav_timeutc(rx));
}

void ubox_rx_init(UboxRx* rx) {
    if(rx == NULL) {
        LOG_WARN("UBOX::rx_init: rx is NULL");
        return;
    }

    *rx = (UboxRx){0};
    ubox_rx_restart_frame(rx);
    LOG_DEBUG("UBOX::rx_init: parser initialized");
}

void ubox_rx_reset(UboxRx* rx) {
    if(rx == NULL) {
        LOG_WARN("UBOX::rx_reset: rx is NULL");
        return;
    }

    ubox_rx_restart_frame(rx);
    LOG_DEBUG("UBOX::rx_reset: parser reset");
}

void ubox_rx_free(UboxRx* rx) {
    if(rx == NULL) {
        LOG_WARN("UBOX::rx_free: rx is NULL");
        return;
    }

    LOG_DEBUG_F("UBOX::rx_free: payload_capacity=%zu", rx->payload_capacity);
    free(rx->frame.payload);
    rx->frame.payload = NULL;
    rx->payload_capacity = 0;
    ubox_rx_restart_frame(rx);
}

UboxRxUpdateResult ubox_rx_update(UboxRx* rx, const uint8_t* bytes, size_t length) {
    if(rx == NULL || bytes == NULL || length == 0) {
        LOG_TRACE_F("UBOX::rx_update: ignored rx=%p bytes=%p length=%zu", rx, bytes, length);
        return UBOX_RX_UPDATE_NONE;
    }

    LOG_TRACE_F("UBOX::rx_update: bytes=%p length=%zu state=%d", bytes, length, rx->state);
    LOG_HEX("UBOX::rx_update: input", bytes, length);
    UboxRxUpdateResult result = UBOX_RX_UPDATE_NONE;

    for(size_t i = 0; i < length; i++) {
        uint8_t byte = bytes[i];

        switch(rx->state) {
        case UBOX_RX_STATE_SYNC1:
            if(byte == 0xb5) {
                LOG_TRACE_F("UBOX::rx_update: sync1 found at chunk_pos=%zu", i);
                ubox_rx_restart_frame(rx);
                rx->frame.sync1 = byte;
                rx->state = UBOX_RX_STATE_SYNC2;
            }
            break;

        case UBOX_RX_STATE_SYNC2:
            if(byte == 0x62) {
                LOG_TRACE_F("UBOX::rx_update: sync2 found at chunk_pos=%zu", i);
                rx->frame.sync2 = byte;
                rx->state = UBOX_RX_STATE_CLASS;
            } else if(byte == 0xb5) {
                LOG_TRACE_F("UBOX::rx_update: repeated sync1 at chunk_pos=%zu", i);
                rx->frame.sync1 = byte;
            } else {
                LOG_WARN_F("UBOX::rx_update: invalid sync2 byte=0x%02X at chunk_pos=%zu", byte, i);
                ubox_rx_restart_frame(rx);
            }
            break;

        case UBOX_RX_STATE_CLASS:
            rx->frame.message_class = byte;
            ubox_rx_checksum(rx, byte);
            rx->state = UBOX_RX_STATE_ID;
            break;

        case UBOX_RX_STATE_ID:
            rx->frame.id = byte;
            ubox_rx_checksum(rx, byte);
            rx->state = UBOX_RX_STATE_LEN1;
            break;

        case UBOX_RX_STATE_LEN1:
            rx->frame.len = byte;
            ubox_rx_checksum(rx, byte);
            rx->state = UBOX_RX_STATE_LEN2;
            break;

        case UBOX_RX_STATE_LEN2:
            rx->frame.len = (uint16_t)((byte << 8) | rx->frame.len);
            LOG_TRACE_F(
                "UBOX::rx_update: header class=0x%02X id=0x%02X payload_len=%u",
                rx->frame.message_class,
                rx->frame.id,
                rx->frame.len);
            ubox_rx_checksum(rx, byte);
            rx->payload_index = 0;
            if(!ubox_rx_reserve_payload(rx, rx->frame.len)) {
                result = (UboxRxUpdateResult)(result | UBOX_RX_UPDATE_NO_MEMORY);
                ubox_rx_restart_frame(rx);
                break;
            }
            rx->state = rx->frame.len == 0 ? UBOX_RX_STATE_CK_A : UBOX_RX_STATE_PAYLOAD;
            break;

        case UBOX_RX_STATE_PAYLOAD:
            rx->frame.payload[rx->payload_index++] = byte;
            ubox_rx_checksum(rx, byte);
            if(rx->payload_index == rx->frame.len) {
                rx->state = UBOX_RX_STATE_CK_A;
            }
            break;

        case UBOX_RX_STATE_CK_A:
            rx->frame.ck_a = byte;
            rx->state = UBOX_RX_STATE_CK_B;
            break;

        case UBOX_RX_STATE_CK_B:
            rx->frame.ck_b = byte;
            if(rx->calculated_ck_a == rx->frame.ck_a && rx->calculated_ck_b == rx->frame.ck_b) {
                rx->frame.valid = true;
                LOG_DEBUG_F(
                    "UBOX::rx_update: valid frame class=0x%02X id=0x%02X payload_len=%u",
                    rx->frame.message_class,
                    rx->frame.id,
                    rx->frame.len);
                LOG_HEX("UBOX::rx_update: payload", rx->frame.payload, rx->frame.len);
                result = (UboxRxUpdateResult)(
                    result | UBOX_RX_UPDATE_FRAME | ubox_rx_parse_frame_payload(rx));
            } else {
                LOG_ERROR_F(
                    "UBOX::rx_update: checksum mismatch class=0x%02X id=0x%02X calc=0x%02X 0x%02X packet=0x%02X 0x%02X",
                    rx->frame.message_class,
                    rx->frame.id,
                    rx->calculated_ck_a,
                    rx->calculated_ck_b,
                    rx->frame.ck_a,
                    rx->frame.ck_b);
                result = (UboxRxUpdateResult)(result | UBOX_RX_UPDATE_CHECKSUM_ERROR);
                ubox_rx_restart_frame(rx);
                break;
            }
            ubox_rx_wait_for_next_frame(rx);
            break;
        }
    }

    LOG_TRACE_F("UBOX::rx_update: result=0x%02X next_state=%d", result, rx->state);
    return result;
}
