#include "cloud_transport.h"

#include <furi_hal.h>
#include <toolbox/strint.h>
#include <string.h>
#include <stdio.h>

#define TAG "CloudTransport"

#define CLOUD_RX_STREAM_SIZE 2048
#define CLOUD_LINE_TIMEOUT_MS  4000
#define CLOUD_WIFI_TIMEOUT_MS  20000
#define CLOUD_BYTE_TIMEOUT_MS  5000
#define CLOUD_CHUNK_SIZE       512

struct CloudTransport {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
};

static void cloud_transport_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    CloudTransport* transport = context;
    if(event & FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(transport->rx_stream, &data, 1, 0);
    }
}

CloudTransport* cloud_transport_alloc(void) {
    CloudTransport* transport = malloc(sizeof(CloudTransport));
    transport->rx_stream = furi_stream_buffer_alloc(CLOUD_RX_STREAM_SIZE, 1);

    transport->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(transport->serial) {
        furi_hal_serial_init(transport->serial, CLOUD_TRANSPORT_DEFAULT_BAUD);
        furi_hal_serial_configure_framing(
            transport->serial,
            FuriHalSerialDataBits8,
            FuriHalSerialParityNone,
            FuriHalSerialStopBits1);
        furi_hal_serial_async_rx_start(
            transport->serial, cloud_transport_rx_callback, transport, false);
    } else {
        FURI_LOG_E(TAG, "Failed to acquire UART, is it in use by another app?");
    }

    return transport;
}

void cloud_transport_free(CloudTransport* transport) {
    furi_assert(transport);
    if(transport->serial) {
        furi_hal_serial_async_rx_stop(transport->serial);
        furi_hal_serial_deinit(transport->serial);
        furi_hal_serial_control_release(transport->serial);
    }
    furi_stream_buffer_free(transport->rx_stream);
    free(transport);
}

static void cloud_transport_write_line(CloudTransport* transport, const char* line) {
    furi_hal_serial_tx(transport->serial, (const uint8_t*)line, strlen(line));
    furi_hal_serial_tx(transport->serial, (const uint8_t*)"\n", 1);
    furi_hal_serial_tx_wait_complete(transport->serial);
}

// Reads a single '\n' terminated line (trailing '\r' stripped) into `out`.
// Returns false on timeout.
static bool
    cloud_transport_read_line(CloudTransport* transport, FuriString* out, uint32_t timeout_ms) {
    furi_string_reset(out);
    uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(timeout_ms);

    while(true) {
        uint32_t now = furi_get_tick();
        if(now >= deadline) return false;

        uint8_t byte;
        size_t got = furi_stream_buffer_receive(transport->rx_stream, &byte, 1, deadline - now);
        if(got == 0) return false;

        if(byte == '\n') {
            if(furi_string_end_with(out, "\r")) {
                furi_string_left(out, furi_string_size(out) - 1);
            }
            return true;
        }

        if(furi_string_size(out) < CLOUD_TRANSPORT_LINE_LEN) {
            furi_string_push_back(out, (char)byte);
        }
    }
}

// Reads exactly `length` raw bytes into `buffer`. Returns false on timeout.
static bool cloud_transport_read_raw(
    CloudTransport* transport,
    uint8_t* buffer,
    size_t length,
    uint32_t timeout_ms) {
    size_t received = 0;
    uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(timeout_ms);

    while(received < length) {
        uint32_t now = furi_get_tick();
        if(now >= deadline) return false;

        size_t got = furi_stream_buffer_receive(
            transport->rx_stream, buffer + received, length - received, deadline - now);
        if(got == 0) return false;
        received += got;
    }
    return true;
}

bool cloud_transport_wifi_connect(
    CloudTransport* transport,
    const char* ssid,
    const char* password,
    FuriString* error_out) {
    if(!transport->serial) {
        if(error_out) furi_string_set(error_out, "UART unavailable");
        return false;
    }

    FuriString* cmd = furi_string_alloc_printf("WIFI_CONNECT %s %s", ssid, password);
    cloud_transport_write_line(transport, furi_string_get_cstr(cmd));
    furi_string_free(cmd);

    FuriString* line = furi_string_alloc();
    bool ok = cloud_transport_read_line(transport, line, CLOUD_WIFI_TIMEOUT_MS);
    if(ok) {
        ok = furi_string_equal(line, "WIFI_OK");
        if(!ok && error_out) furi_string_set(error_out, "Wi-Fi connect failed");
    } else if(error_out) {
        furi_string_set(error_out, "No response from Wi-Fi Devboard");
    }
    furi_string_free(line);
    return ok;
}

bool cloud_transport_list(
    CloudTransport* transport,
    const char* bin,
    CloudFileEntry* out,
    size_t max_entries,
    size_t* count_out,
    FuriString* error_out) {
    if(!transport->serial) {
        if(error_out) furi_string_set(error_out, "UART unavailable");
        return false;
    }

    FuriString* cmd = furi_string_alloc_printf("LIST %s", bin);
    cloud_transport_write_line(transport, furi_string_get_cstr(cmd));
    furi_string_free(cmd);

    size_t count = 0;
    FuriString* line = furi_string_alloc();
    bool success = false;

    while(true) {
        if(!cloud_transport_read_line(transport, line, CLOUD_LINE_TIMEOUT_MS)) {
            if(error_out) furi_string_set(error_out, "Timed out waiting for file list");
            break;
        }

        if(furi_string_equal(line, "END")) {
            success = true;
            break;
        }

        if(furi_string_start_with(line, "ERR")) {
            if(error_out) furi_string_set(error_out, furi_string_get_cstr(line));
            break;
        }

        if(furi_string_start_with(line, "FILE ") && count < max_entries) {
            char name[CLOUD_TRANSPORT_NAME_LEN] = {0};
            unsigned long size = 0;
            if(sscanf(furi_string_get_cstr(line) + 5, "%63s %lu", name, &size) >= 1) {
                strlcpy(out[count].name, name, sizeof(out[count].name));
                out[count].size = (uint32_t)size;
                count++;
            }
        }
    }

    furi_string_free(line);
    if(count_out) *count_out = count;
    return success;
}

bool cloud_transport_get(
    CloudTransport* transport,
    const char* bin,
    const char* name,
    Storage* storage,
    const char* dest_path,
    CloudProgressCallback progress_cb,
    void* progress_context,
    FuriString* error_out) {
    if(!transport->serial) {
        if(error_out) furi_string_set(error_out, "UART unavailable");
        return false;
    }

    FuriString* cmd = furi_string_alloc_printf("GET %s %s", bin, name);
    cloud_transport_write_line(transport, furi_string_get_cstr(cmd));
    furi_string_free(cmd);

    FuriString* line = furi_string_alloc();
    if(!cloud_transport_read_line(transport, line, CLOUD_LINE_TIMEOUT_MS)) {
        if(error_out) furi_string_set(error_out, "Timed out waiting for file");
        furi_string_free(line);
        return false;
    }

    if(!furi_string_start_with(line, "SIZE ")) {
        if(error_out) furi_string_set(error_out, furi_string_get_cstr(line));
        furi_string_free(line);
        return false;
    }

    uint32_t size = 0;
    char* endp = NULL;
    strint_to_uint32(furi_string_get_cstr(line) + 5, &endp, &size, 10);
    furi_string_free(line);

    File* file = storage_file_alloc(storage);
    bool success = storage_file_open(file, dest_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    uint8_t buffer[CLOUD_CHUNK_SIZE];
    uint32_t received = 0;
    while(success && received < size) {
        size_t chunk = size - received;
        if(chunk > sizeof(buffer)) chunk = sizeof(buffer);

        if(!cloud_transport_read_raw(transport, buffer, chunk, CLOUD_BYTE_TIMEOUT_MS)) {
            success = false;
            if(error_out) furi_string_set(error_out, "Connection lost during download");
            break;
        }

        if(storage_file_write(file, buffer, chunk) != chunk) {
            success = false;
            if(error_out) furi_string_set(error_out, "SD card write failed");
            break;
        }

        received += chunk;
        if(progress_cb) progress_cb(progress_context, received, size);
    }

    storage_file_close(file);
    storage_file_free(file);

    if(success) {
        FuriString* end_line = furi_string_alloc();
        success = cloud_transport_read_line(transport, end_line, CLOUD_LINE_TIMEOUT_MS) &&
                  furi_string_equal(end_line, "END");
        furi_string_free(end_line);
    }

    return success;
}

bool cloud_transport_put(
    CloudTransport* transport,
    const char* bin,
    const char* name,
    Storage* storage,
    const char* src_path,
    CloudProgressCallback progress_cb,
    void* progress_context,
    FuriString* error_out) {
    if(!transport->serial) {
        if(error_out) furi_string_set(error_out, "UART unavailable");
        return false;
    }

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, src_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        if(error_out) furi_string_set(error_out, "Could not open source file");
        return false;
    }
    uint64_t size = storage_file_size(file);

    FuriString* cmd = furi_string_alloc_printf("PUT %s %s %llu", bin, name, size);
    cloud_transport_write_line(transport, furi_string_get_cstr(cmd));
    furi_string_free(cmd);

    FuriString* line = furi_string_alloc();
    bool ready = cloud_transport_read_line(transport, line, CLOUD_LINE_TIMEOUT_MS) &&
                 furi_string_equal(line, "READY");
    if(!ready && error_out) {
        if(furi_string_empty(line)) {
            furi_string_set(error_out, "Devboard did not respond");
        } else {
            furi_string_set(error_out, line);
        }
    }

    bool success = ready;
    uint8_t buffer[CLOUD_CHUNK_SIZE];
    uint64_t sent = 0;
    while(success && sent < size) {
        size_t chunk = storage_file_read(file, buffer, sizeof(buffer));
        if(chunk == 0) break;
        furi_hal_serial_tx(transport->serial, buffer, chunk);
        sent += chunk;
        if(progress_cb) progress_cb(progress_context, (uint32_t)sent, (uint32_t)size);
    }
    if(success) furi_hal_serial_tx_wait_complete(transport->serial);

    storage_file_close(file);
    storage_file_free(file);

    if(success) {
        success = cloud_transport_read_line(transport, line, CLOUD_BYTE_TIMEOUT_MS) &&
                  furi_string_equal(line, "OK");
        if(!success && error_out) {
            if(furi_string_empty(line)) {
                furi_string_set(error_out, "Upload not acknowledged");
            } else {
                furi_string_set(error_out, line);
            }
        }
    }

    furi_string_free(line);
    return success;
}
