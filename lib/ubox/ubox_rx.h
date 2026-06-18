#pragma once

#include "ubox.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum UboxRxUpdateResult {
    UBOX_RX_UPDATE_NONE = 0,
    UBOX_RX_UPDATE_FRAME = 1 << 0,
    UBOX_RX_UPDATE_NAV_PVT = 1 << 1,
    UBOX_RX_UPDATE_NAV_ODO = 1 << 2,
    UBOX_RX_UPDATE_NAV_TIMEUTC = 1 << 3,
    UBOX_RX_UPDATE_CHECKSUM_ERROR = 1 << 4,
    UBOX_RX_UPDATE_NO_MEMORY = 1 << 5,
} UboxRxUpdateResult;

typedef enum UboxRxState {
    UBOX_RX_STATE_SYNC1,
    UBOX_RX_STATE_SYNC2,
    UBOX_RX_STATE_CLASS,
    UBOX_RX_STATE_ID,
    UBOX_RX_STATE_LEN1,
    UBOX_RX_STATE_LEN2,
    UBOX_RX_STATE_PAYLOAD,
    UBOX_RX_STATE_CK_A,
    UBOX_RX_STATE_CK_B,
} UboxRxState;

typedef struct UboxRx {
    UboxRxState state;
    UbloxFrame frame;
    size_t payload_capacity;
    size_t payload_index;
    uint8_t calculated_ck_a;
    uint8_t calculated_ck_b;

    Ublox_NAV_PVT_Message nav_pvt;
    Ublox_NAV_ODO_Message nav_odo;
    Ublox_NAV_TIMEUTC_Message nav_timeutc;
} UboxRx;

void ubox_rx_init(UboxRx* rx);
void ubox_rx_reset(UboxRx* rx);
void ubox_rx_free(UboxRx* rx);
UboxRxUpdateResult ubox_rx_update(UboxRx* rx, const uint8_t* bytes, size_t length);

#ifdef __cplusplus
}
#endif
