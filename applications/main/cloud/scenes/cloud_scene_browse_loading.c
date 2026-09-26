#include "../cloud_app_i.h"

#include <string.h>

static void cloud_scene_browse_loading_done_callback(void* context) {
    CloudApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CloudEventWorkerDone);
}

void cloud_scene_browse_loading_on_enter(void* context) {
    CloudApp* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "Connecting to Wi-Fi and\nfetching the cloud file list...\nPlease wait.");
    view_dispatcher_switch_to_view(app->view_dispatcher, CloudAppViewWidget);

    cloud_worker_start(
        app->worker,
        CloudWorkerOpList,
        &app->settings,
        NULL,
        NULL,
        NULL,
        cloud_scene_browse_loading_done_callback,
        app);
}

bool cloud_scene_browse_loading_on_event(void* context, SceneManagerEvent event) {
    CloudApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == CloudEventWorkerDone) {
        cloud_worker_join(app->worker);
        consumed = true;

        if(cloud_worker_get_success(app->worker)) {
            size_t count = 0;
            const CloudFileEntry* files = cloud_worker_get_files(app->worker, &count);
            memcpy(app->file_list, files, count * sizeof(CloudFileEntry));
            app->file_count = count;
            scene_manager_next_scene(app->scene_manager, CloudSceneBrowseList);
        } else {
            widget_reset(app->widget);
            FuriString* text = furi_string_alloc_printf(
                "Could not load file list:\n\n%s", cloud_worker_get_error(app->worker));
            widget_add_text_scroll_element(
                app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
            furi_string_free(text);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        cloud_worker_join(app->worker);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, CloudSceneStart);
        consumed = true;
    }
    return consumed;
}

void cloud_scene_browse_loading_on_exit(void* context) {
    CloudApp* app = context;
    widget_reset(app->widget);
}
