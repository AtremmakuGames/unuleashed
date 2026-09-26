#include "cloud_worker.h"

#include <string.h>

struct CloudWorker {
    FuriThread* thread;

    CloudWorkerOp op;
    CloudSettings settings;
    FuriString* name;
    FuriString* local_path;

    CloudWorkerProgressCallback progress_cb;
    CloudWorkerDoneCallback done_cb;
    void* context;

    bool success;
    FuriString* error;
    CloudFileEntry files[CLOUD_TRANSPORT_MAX_FILES];
    size_t file_count;
};

static void cloud_worker_progress_trampoline(void* context, uint32_t transferred, uint32_t total) {
    CloudWorker* worker = context;
    if(worker->progress_cb) worker->progress_cb(worker->context, transferred, total);
}

static int32_t cloud_worker_thread(void* context) {
    CloudWorker* worker = context;

    CloudTransport* transport = cloud_transport_alloc();

    worker->success =
        cloud_transport_wifi_connect(transport, worker->settings.ssid, worker->settings.password, worker->error);

    if(worker->success) {
        Storage* storage = furi_record_open(RECORD_STORAGE);

        if(worker->op == CloudWorkerOpList) {
            worker->success = cloud_transport_list(
                transport,
                worker->settings.bin,
                worker->files,
                CLOUD_TRANSPORT_MAX_FILES,
                &worker->file_count,
                worker->error);
        } else if(worker->op == CloudWorkerOpGet) {
            worker->success = cloud_transport_get(
                transport,
                worker->settings.bin,
                furi_string_get_cstr(worker->name),
                storage,
                furi_string_get_cstr(worker->local_path),
                cloud_worker_progress_trampoline,
                worker,
                worker->error);
        } else if(worker->op == CloudWorkerOpPut) {
            worker->success = cloud_transport_put(
                transport,
                worker->settings.bin,
                furi_string_get_cstr(worker->name),
                storage,
                furi_string_get_cstr(worker->local_path),
                cloud_worker_progress_trampoline,
                worker,
                worker->error);
        }

        furi_record_close(RECORD_STORAGE);
    }

    cloud_transport_free(transport);

    if(worker->done_cb) worker->done_cb(worker->context);

    return 0;
}

CloudWorker* cloud_worker_alloc(void) {
    CloudWorker* worker = malloc(sizeof(CloudWorker));
    memset(worker, 0, sizeof(CloudWorker));
    worker->name = furi_string_alloc();
    worker->local_path = furi_string_alloc();
    worker->error = furi_string_alloc();
    return worker;
}

void cloud_worker_free(CloudWorker* worker) {
    furi_assert(worker);
    if(worker->thread) {
        furi_thread_join(worker->thread);
        furi_thread_free(worker->thread);
    }
    furi_string_free(worker->name);
    furi_string_free(worker->local_path);
    furi_string_free(worker->error);
    free(worker);
}

void cloud_worker_start(
    CloudWorker* worker,
    CloudWorkerOp op,
    const CloudSettings* settings,
    const char* name,
    const char* local_path,
    CloudWorkerProgressCallback progress_cb,
    CloudWorkerDoneCallback done_cb,
    void* context) {
    furi_assert(worker);
    if(worker->thread) {
        furi_thread_join(worker->thread);
        furi_thread_free(worker->thread);
        worker->thread = NULL;
    }

    worker->op = op;
    worker->settings = *settings;
    furi_string_set(worker->name, name ? name : "");
    furi_string_set(worker->local_path, local_path ? local_path : "");
    worker->progress_cb = progress_cb;
    worker->done_cb = done_cb;
    worker->context = context;
    worker->success = false;
    furi_string_reset(worker->error);
    worker->file_count = 0;

    worker->thread = furi_thread_alloc_ex("CloudWorker", 2048, cloud_worker_thread, worker);
    furi_thread_start(worker->thread);
}

void cloud_worker_join(CloudWorker* worker) {
    furi_assert(worker);
    if(worker->thread) {
        furi_thread_join(worker->thread);
    }
}

bool cloud_worker_get_success(CloudWorker* worker) {
    return worker->success;
}

const char* cloud_worker_get_error(CloudWorker* worker) {
    return furi_string_get_cstr(worker->error);
}

const CloudFileEntry* cloud_worker_get_files(CloudWorker* worker, size_t* count) {
    if(count) *count = worker->file_count;
    return worker->files;
}
