#include "cloud_app_i.h"

#include <string.h>

static bool cloud_app_custom_event_callback(void* context, uint32_t event) {
    CloudApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool cloud_app_back_event_callback(void* context) {
    CloudApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static CloudApp* cloud_app_alloc(void) {
    CloudApp* app = malloc(sizeof(CloudApp));
    memset(app, 0, sizeof(CloudApp));

    app->upload_source_path = furi_string_alloc();
    app->remote_name = furi_string_alloc();
    app->download_dest_path = furi_string_alloc();

    cloud_settings_load(&app->settings);
    app->worker = cloud_worker_alloc();

    // The Wi-Fi Devboard sits on the same UART pins the "expansion" service
    // normally owns; take the pins for ourselves for the lifetime of the app.
    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&cloud_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, cloud_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, cloud_app_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CloudAppViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CloudAppViewTextInput, text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CloudAppViewWidget, widget_get_view(app->widget));

    scene_manager_next_scene(app->scene_manager, CloudSceneStart);

    return app;
}

static void cloud_app_free(CloudApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, CloudAppViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, CloudAppViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, CloudAppViewWidget);
    submenu_free(app->submenu);
    text_input_free(app->text_input);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);

    expansion_enable(app->expansion);
    furi_record_close(RECORD_EXPANSION);

    cloud_worker_free(app->worker);

    furi_string_free(app->upload_source_path);
    furi_string_free(app->remote_name);
    furi_string_free(app->download_dest_path);

    free(app);
}

int32_t cloud_app(void* p) {
    UNUSED(p);
    CloudApp* app = cloud_app_alloc();

    view_dispatcher_run(app->view_dispatcher);

    cloud_app_free(app);
    return 0;
}
