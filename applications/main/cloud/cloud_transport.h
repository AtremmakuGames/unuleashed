#pragma once

// UART transport that talks to an ESP32 "Wi-Fi Devboard" running the companion
// "cloud_http" firmware (see documentation/devboard/cloud_companion/cloud_http.ino).
//
// The wire protocol is a small, line based, ASCII request/response scheme:
//
//   -> WIFI_CONNECT <ssid> <password>\n
//   <- WIFI_OK\n | WIFI_FAIL\n
//
//   -> LIST <bin>\n
//   <- FILE <name> <size>\n            (repeated, one line per file)
//   <- END\n                           (terminates a successful listing)
//   <- ERR <reason>\n                  (on failure)
//
//   -> GET <bin> <name>\n
//   <- SIZE <n>\n <n raw bytes> END\n  (on success)
//   <- ERR <reason>\n                  (on failure)
//
//   -> PUT <bin> <name> <size>\n
//   <- READY\n <size raw bytes sent by us> <- OK\n | ERR <reason>\n

#include <furi.h>
#include <furi_hal_serial.h>
#include <storage/storage.h>

#define CLOUD_TRANSPORT_MAX_FILES     32
#define CLOUD_TRANSPORT_NAME_LEN      64
#define CLOUD_TRANSPORT_LINE_LEN      160
#define CLOUD_TRANSPORT_DEFAULT_BAUD  115200

typedef struct {
    char name[CLOUD_TRANSPORT_NAME_LEN];
    uint32_t size;
} CloudFileEntry;

typedef struct CloudTransport CloudTransport;

// Progress callback invoked periodically during GET/PUT transfers.
// `total` is 0 if the total size is unknown.
typedef void (*CloudProgressCallback)(void* context, uint32_t transferred, uint32_t total);

CloudTransport* cloud_transport_alloc(void);
void cloud_transport_free(CloudTransport* transport);

// Connects the ESP32 companion to the configured Wi-Fi access point.
bool cloud_transport_wifi_connect(
    CloudTransport* transport,
    const char* ssid,
    const char* password,
    FuriString* error_out);

// Fetches the file list of the given public "bin" (database) into `out`.
bool cloud_transport_list(
    CloudTransport* transport,
    const char* bin,
    CloudFileEntry* out,
    size_t max_entries,
    size_t* count_out,
    FuriString* error_out);

// Downloads `name` from `bin` and saves it to `dest_path` on the SD card.
bool cloud_transport_get(
    CloudTransport* transport,
    const char* bin,
    const char* name,
    Storage* storage,
    const char* dest_path,
    CloudProgressCallback progress_cb,
    void* progress_context,
    FuriString* error_out);

// Uploads the file at `src_path` on the SD card to `bin` under `name`.
bool cloud_transport_put(
    CloudTransport* transport,
    const char* bin,
    const char* name,
    Storage* storage,
    const char* src_path,
    CloudProgressCallback progress_cb,
    void* progress_context,
    FuriString* error_out);
