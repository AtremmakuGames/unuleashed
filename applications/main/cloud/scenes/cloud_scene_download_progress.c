#include "../cloud_app_i.h"
#include <storage/storage.h>

static void cloud_scene_download_progress_done_callback(void* context) {
    CloudApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CloudEventWorkerDone);
}

void cloud_scene_download_progress_on_enter(void* context) {
    CloudApp* app = context;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, CLOUD_DOWNLOAD_DIR);
    furi_record_close(RECORD_STORAGE);

    widget_reset(app->widget);
    FuriString* text = furi_string_alloc_printf(
        "Connecting to Wi-Fi and\ndownloading\n%s\nfrom the cloud...\nPlease wait.",
        furi_string_get_cstr(app->remote_name));
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, CloudAppViewWidget);

    cloud_worker_start(
        app->worker,
        CloudWorkerOpGet,
        &app->settings,
        furi_string_get_cstr(app->remote_name),
        furi_string_get_cstr(app->download_dest_path),
        NULL,
        cloud_scene_download_progress_done_callback,
        app);
}

bool cloud_scene_download_progress_on_event(void* context, SceneManagerEvent event) {
    CloudApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == CloudEventWorkerDone) {
        cloud_worker_join(app->worker);

        widget_reset(app->widget);
        FuriString* text = furi_string_alloc();
        if(cloud_worker_get_success(app->worker)) {
            furi_string_printf(
                text,
                "Download complete!\n\nSaved to:\n%s",
                furi_string_get_cstr(app->download_dest_path));
        } else {
            furi_string_printf(
                text, "Download failed:\n\n%s", cloud_worker_get_error(app->worker));
        }
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
        furi_string_free(text);

        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        cloud_worker_join(app->worker);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, CloudSceneStart);
        consumed = true;
    }
    return consumed;
}

void cloud_scene_download_progress_on_exit(void* context) {
    CloudApp* app = context;
    widget_reset(app->widget);
}
