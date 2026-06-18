#include <string.h>

#include <minmea.h>
#include "gps_uart.h"

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

// Called from USB when the host sends data to the virtual COM port
static void gps_uart_on_cdc_rx(void* context) {
    GpsUart* gps_uart = (GpsUart*)context;
    furi_thread_flags_set(furi_thread_get_id(gps_uart->thread), WorkerEvtRxDone);
}

static const CdcCallbacks gps_uart_cdc_cb = {
    NULL, // tx_ep_callback
    gps_uart_on_cdc_rx, // rx_ep_callback
    NULL, // state_callback
    NULL, // ctrl_line_callback
    NULL, // config_callback
};

// Configure the USB stack to expose a single CDC (virtual COM) port and take
// it over from the CLI, so the app can receive NMEA data from the host.
static void gps_uart_usb_init(GpsUart* gps_uart) {
    gps_uart->cli_vcp = furi_record_open(RECORD_CLI_VCP);
    gps_uart->usb_if_prev = furi_hal_usb_get_config();

    furi_hal_usb_unlock();
    cli_vcp_disable(gps_uart->cli_vcp);
    furi_check(furi_hal_usb_set_config(&usb_cdc_single, NULL) == true);
}

static void gps_uart_usb_deinit(GpsUart* gps_uart) {
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(gps_uart->usb_if_prev, NULL);
    cli_vcp_enable(gps_uart->cli_vcp);
    furi_record_close(RECORD_CLI_VCP);
    gps_uart->cli_vcp = NULL;
}

static void gps_uart_parse_nmea(GpsUart* gps_uart, char* line) {
    switch(minmea_sentence_id(line, false)) {
    case MINMEA_SENTENCE_RMC: {
        struct minmea_sentence_rmc frame;
        if(minmea_parse_rmc(&frame, line)) {
            gps_uart->status.valid = frame.valid;
            gps_uart->status.latitude = minmea_tocoord(&frame.latitude);
            gps_uart->status.longitude = minmea_tocoord(&frame.longitude);
            gps_uart->status.speed = minmea_tofloat(&frame.speed);
            gps_uart->status.course = minmea_tofloat(&frame.course);
            gps_uart->status.time_hours = frame.time.hours;
            gps_uart->status.time_minutes = frame.time.minutes;
            gps_uart->status.time_seconds = frame.time.seconds;

            notification_message_block(gps_uart->notifications, &sequence_blink_green_10);
        }
    } break;

    case MINMEA_SENTENCE_GGA: {
        struct minmea_sentence_gga frame;
        if(minmea_parse_gga(&frame, line)) {
            gps_uart->status.latitude = minmea_tocoord(&frame.latitude);
            gps_uart->status.longitude = minmea_tocoord(&frame.longitude);
            gps_uart->status.altitude = minmea_tofloat(&frame.altitude);
            gps_uart->status.altitude_units = frame.altitude_units;
            gps_uart->status.fix_quality = frame.fix_quality;
            gps_uart->status.satellites_tracked = frame.satellites_tracked;
            gps_uart->status.time_hours = frame.time.hours;
            gps_uart->status.time_minutes = frame.time.minutes;
            gps_uart->status.time_seconds = frame.time.seconds;

            notification_message_block(gps_uart->notifications, &sequence_blink_magenta_10);
        }
    } break;

    case MINMEA_SENTENCE_GLL: {
        struct minmea_sentence_gll frame;
        if(minmea_parse_gll(&frame, line)) {
            gps_uart->status.latitude = minmea_tocoord(&frame.latitude);
            gps_uart->status.longitude = minmea_tocoord(&frame.longitude);
            gps_uart->status.time_hours = frame.time.hours;
            gps_uart->status.time_minutes = frame.time.minutes;
            gps_uart->status.time_seconds = frame.time.seconds;

            notification_message_block(gps_uart->notifications, &sequence_blink_red_10);
        }
    } break;

    default:
        break;
    }
}

static int32_t gps_uart_worker(void* context) {
    GpsUart* gps_uart = (GpsUart*)context;

    size_t rx_offset = 0;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEvtStop) {
            break;
        }

        if(events & WorkerEvtRxDone) {
            int32_t len = 0;
            do {
                // receive bytes from the USB CDC port into rx_buf, starting at rx_offset
                // from the start of the buffer; the maximum we can receive is
                // RX_BUF_SIZE - 1 - rx_offset
                len = furi_hal_cdc_receive(
                    GPS_VCP_CH,
                    gps_uart->rx_buf + rx_offset,
                    (uint16_t)(RX_BUF_SIZE - 1 - rx_offset));
                if(len > 0) {
                    // increase rx_offset by the number of bytes received, and null-terminate rx_buf
                    rx_offset += len;
                    gps_uart->rx_buf[rx_offset] = '\0';

                    // look for strings ending in newlines, starting at the start of rx_buf
                    char* line_current = (char*)gps_uart->rx_buf;
                    while(1) {
                        // skip null characters
                        while(*line_current == '\0' &&
                              line_current < (char*)gps_uart->rx_buf + rx_offset - 1) {
                            line_current++;
                        }

                        // find the next newline
                        char* newline = strchr(line_current, '\n');
                        if(newline) // newline found
                        {
                            // put a null terminator in place of the newline, to delimit the line string
                            *newline = '\0';

                            // attempt to parse the line as a NMEA sentence
                            gps_uart_parse_nmea(gps_uart, line_current);

                            // move the cursor to the character after the newline
                            line_current = newline + 1;
                        } else // no more newlines found
                        {
                            if(line_current >
                               (char*)gps_uart->rx_buf) // at least one line was found
                            {
                                // clear parsed lines, and move any leftover bytes to the start of rx_buf
                                rx_offset = 0;
                                while(
                                    *line_current) // stop when the original rx_offset terminator is reached
                                {
                                    gps_uart->rx_buf[rx_offset++] = *(line_current++);
                                }
                            }
                            break; // go back to receiving bytes from the serial stream
                        }
                    }
                }
            } while(len > 0);
        }
    }

    return 0;
}

void gps_uart_init_thread(GpsUart* gps_uart) {
    furi_assert(gps_uart);
    gps_uart->status.valid = false;
    gps_uart->status.latitude = 0.0;
    gps_uart->status.longitude = 0.0;
    gps_uart->status.speed = 0.0;
    gps_uart->status.course = 0.0;
    gps_uart->status.altitude = 0.0;
    gps_uart->status.altitude_units = ' ';
    gps_uart->status.fix_quality = 0;
    gps_uart->status.satellites_tracked = 0;
    gps_uart->status.time_hours = 0;
    gps_uart->status.time_minutes = 0;
    gps_uart->status.time_seconds = 0;

    gps_uart->thread = furi_thread_alloc();
    furi_thread_set_name(gps_uart->thread, "GpsUartWorker");
    furi_thread_set_stack_size(gps_uart->thread, 1024);
    furi_thread_set_context(gps_uart->thread, gps_uart);
    furi_thread_set_callback(gps_uart->thread, gps_uart_worker);

    furi_thread_start(gps_uart->thread);

    // Route incoming USB CDC data to the worker thread
    furi_hal_cdc_set_callbacks(GPS_VCP_CH, (CdcCallbacks*)&gps_uart_cdc_cb, gps_uart);

    // Wake up host-side GPS forwarders that wait for a first byte
    furi_hal_cdc_send(GPS_VCP_CH, (uint8_t*)"wakey wakey\r\n", strlen("wakey wakey\r\n"));
}

void gps_uart_deinit_thread(GpsUart* gps_uart) {
    furi_assert(gps_uart);
    furi_hal_cdc_set_callbacks(GPS_VCP_CH, NULL, NULL);
    furi_thread_flags_set(furi_thread_get_id(gps_uart->thread), WorkerEvtStop);
    furi_thread_join(gps_uart->thread);
    furi_thread_free(gps_uart->thread);
}

GpsUart* gps_uart_enable() {
    GpsUart* gps_uart = malloc(sizeof(GpsUart));

    gps_uart->notifications = furi_record_open(RECORD_NOTIFICATION);

    gps_uart->baudrate = gps_baudrates[current_gps_baudrate];
    gps_uart->backlight_on = false;
    gps_uart->speed_units = KNOTS;
    gps_uart->deep_sleep_enabled = false;
    gps_uart->view_state = NORMAL;

    gps_uart_usb_init(gps_uart);
    gps_uart_init_thread(gps_uart);

    return gps_uart;
}

void gps_uart_disable(GpsUart* gps_uart) {
    furi_assert(gps_uart);
    gps_uart_deinit_thread(gps_uart);
    gps_uart_usb_deinit(gps_uart);
    furi_record_close(RECORD_NOTIFICATION);

    free(gps_uart);
}
