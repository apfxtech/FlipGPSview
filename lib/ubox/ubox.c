#include "ubox.h"

#include "debug.h"

uint16_t ubox_get_u16(const uint8_t* payload, size_t offset) {
    return (uint16_t)(((uint16_t)payload[offset + 1] << 8) | payload[offset]);
}

uint32_t ubox_get_u32(const uint8_t* payload, size_t offset) {
    return (uint32_t)(
        ((uint32_t)payload[offset]) | ((uint32_t)payload[offset + 1] << 8) |
        ((uint32_t)payload[offset + 2] << 16) | ((uint32_t)payload[offset + 3] << 24));
}

const char* ubox_message_name(uint8_t message_class, uint8_t message_id) {
    switch(message_class) {
    case UBX_NAV_CLASS:
        switch(message_id) {
        case UBX_NAV_PVT_MESSAGE:
            return "NAV-PVT";
        case UBX_NAV_ODO_MESSAGE:
            return "NAV-ODO";
        case UBX_NAV_RESETODO_MESSAGE:
            return "NAV-RESETODO";
        case UBX_NAV_TIMEUTC_MESSAGE:
            return "NAV-TIMEUTC";
        case UBX_NAV_SAT_MESSAGE:
            return "NAV-SAT";
        default:
            return "NAV-UNKNOWN";
        }

    case UBX_ACK_CLASS:
        switch(message_id) {
        case UBX_ACK_ACK_MESSAGE:
            return "ACK-ACK";
        case UBX_ACK_NAK_MESSAGE:
            return "ACK-NAK";
        default:
            return "ACK-UNKNOWN";
        }

    case UBX_CFG_CLASS:
        switch(message_id) {
        case UBX_CFG_PMS_MESSAGE:
            return "CFG-PMS";
        case UBX_CFG_ODO_MESSAGE:
            return "CFG-ODO";
        case UBX_CFG_NAV5_MESSAGE:
            return "CFG-NAV5";
        default:
            return "CFG-UNKNOWN";
        }

    case UBX_MON_CLASS:
        switch(message_id) {
        case UBX_MON_VER_MESSAGE:
            return "MON-VER";
        default:
            return "MON-UNKNOWN";
        }

    default:
        return "UNKNOWN";
    }
}
