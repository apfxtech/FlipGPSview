#pragma once

// Logging shim that maps the UBOX library's LOG_* macros onto the Flipper
// firmware FURI_LOG_* facility, so the parser sources can be reused unchanged.

#include <furi.h>

#define UBOX_LOG_TAG "UBOX"

#define LOG_TRACE(msg)   FURI_LOG_T(UBOX_LOG_TAG, msg)
#define LOG_TRACE_F(...) FURI_LOG_T(UBOX_LOG_TAG, __VA_ARGS__)
#define LOG_DEBUG(msg)   FURI_LOG_D(UBOX_LOG_TAG, msg)
#define LOG_DEBUG_F(...) FURI_LOG_D(UBOX_LOG_TAG, __VA_ARGS__)
#define LOG_INFO(msg)    FURI_LOG_I(UBOX_LOG_TAG, msg)
#define LOG_INFO_F(...)  FURI_LOG_I(UBOX_LOG_TAG, __VA_ARGS__)
#define LOG_WARN(msg)    FURI_LOG_W(UBOX_LOG_TAG, msg)
#define LOG_WARN_F(...)  FURI_LOG_W(UBOX_LOG_TAG, __VA_ARGS__)
#define LOG_ERROR(msg)   FURI_LOG_E(UBOX_LOG_TAG, msg)
#define LOG_ERROR_F(...) FURI_LOG_E(UBOX_LOG_TAG, __VA_ARGS__)

// Hex dumps are noisy on the hot RX path; compile them out but keep the
// arguments referenced so they never go stale.
#define LOG_HEX(msg, data, length) \
    do {                           \
        (void)(msg);               \
        (void)(data);              \
        (void)(length);            \
    } while(0)
