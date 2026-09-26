#include "../cloud_app_i.h"

enum CloudStartItem {
    CloudStartItemUpload,
    CloudStartItemBrowse,
    CloudStartItemSettings,
    CloudStartItemAbout,
};

static void cloud_scene_start_submenu_callback(void* context, uint32_t index) {
    CloudApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cloud_scene_start_on_enter(void* context) {
    CloudApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu, "Upload file", CloudStartItemUpload, cloud_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu,
        "Browse cloud files",
        CloudStartItemBrowse,
        cloud_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Wi-Fi & database",
        CloudStartItemSettings,
        cloud_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu, "About", CloudStartItemAbout, cloud_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, CloudSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, CloudAppViewSubmenu);
}

bool cloud_scene_start_on_event(void* context, SceneManagerEvent event) {
    CloudApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        scene_manager_set_scene_state(app->scene_manager, CloudSceneStart, event.event);

        if(event.event == CloudStartItemUpload) {
            FuriString* path = furi_string_alloc_set(EXT_PATH(""));
            DialogsFileBrowserOptions options;
            dialog_file_browser_set_basic_options(&options, "*", NULL);

            if(dialog_file_browser_show(app->dialogs, path, path, &options)) {
                furi_string_set(app->upload_source_path, path);
                scene_manager_next_scene(app->scene_manager, CloudSceneUploadProgress);
            }
            furi_string_free(path);
        } else if(event.event == CloudStartItemBrowse) {
            scene_manager_next_scene(app->scene_manager, CloudSceneBrowseLoading);
        } else if(event.event == CloudStartItemSettings) {
            scene_manager_next_scene(app->scene_manager, CloudSceneSettings);
        } else if(event.event == CloudStartItemAbout) {
            scene_manager_next_scene(app->scene_manager, CloudSceneAbout);
        }
    }
    return consumed;
}

void cloud_scene_start_on_exit(void* context) {
    CloudApp* app = context;
    submenu_reset(app->submenu);
}
