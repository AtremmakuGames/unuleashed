#pragma once

#include <furi.h>
#include <storage/storage.h>
#include "cloud_transport.h"
#include "cloud_settings.h"

typedef enum {
    CloudWorkerOpList,
    CloudWorkerOpGet,
    CloudWorkerOpPut,
} CloudWorkerOp;

typedef struct CloudWorker CloudWorker;

// Called on the worker thread as bytes are transferred; `context` is whatever
// was passed to cloud_worker_start.
typedef void (*CloudWorkerProgressCallback)(void* context, uint32_t transferred, uint32_t total);

// Called once the worker has finished, on the worker thread itself, right
// before it exits. Use it to notify the GUI thread (e.g. via
// view_dispatcher_send_custom_event); do not touch GUI views directly here.
typedef void (*CloudWorkerDoneCallback)(void* context);

CloudWorker* cloud_worker_alloc(void);
void cloud_worker_free(CloudWorker* worker);

// Starts the worker on a background thread. `local_path` is the SD card path
// to read from (Put) or write to (Get); ignored for List. `name` is the
// remote file name; ignored for List.
void cloud_worker_start(
    CloudWorker* worker,
    CloudWorkerOp op,
    const CloudSettings* settings,
    const char* name,
    const char* local_path,
    CloudWorkerProgressCallback progress_cb,
    CloudWorkerDoneCallback done_cb,
    void* context);

// Blocks until the worker thread has finished. Safe to call after the done
// callback has fired, or to force a join.
void cloud_worker_join(CloudWorker* worker);

// Valid to call only after the done callback has fired.
bool cloud_worker_get_success(CloudWorker* worker);
const char* cloud_worker_get_error(CloudWorker* worker);
const CloudFileEntry* cloud_worker_get_files(CloudWorker* worker, size_t* count);
