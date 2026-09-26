#include "../cloud_app_i.h"

#define CLOUD_BROWSE_EMPTY_INDEX 0xFFFF

static void cloud_scene_browse_list_submenu_callback(void* context, uint32_t index) {
    CloudApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cloud_scene_browse_list_on_enter(void* context) {
    CloudApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    if(app->file_count == 0) {
        submenu_add_item(
            submenu,
            "(no files in database)",
            CLOUD_BROWSE_EMPTY_INDEX,
            cloud_scene_browse_list_submenu_callback,
            app);
    } else {
        FuriString* label = furi_string_alloc();
        for(size_t i = 0; i < app->file_count; i++) {
            furi_string_printf(
                label,
                "%s (%lu B)",
                app->file_list[i].name,
                (unsigned long)app->file_list[i].size);
            submenu_add_item(
                submenu,
                furi_string_get_cstr(label),
                i,
                cloud_scene_browse_list_submenu_callback,
                app);
        }
        furi_string_free(label);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, CloudAppViewSubmenu);
}

bool cloud_scene_browse_list_on_event(void* context, SceneManagerEvent event) {
    CloudApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event < app->file_count) {
            furi_string_set(app->remote_name, app->file_list[event.event].name);
            furi_string_printf(
                app->download_dest_path,
                "%s/%s",
                CLOUD_DOWNLOAD_DIR,
                app->file_list[event.event].name);
            scene_manager_next_scene(app->scene_manager, CloudSceneDownloadProgress);
        }
    }
    return consumed;
}

void cloud_scene_browse_list_on_exit(void* context) {
    CloudApp* app = context;
    submenu_reset(app->submenu);
}
