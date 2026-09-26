#include "../cloud_app_i.h"

enum CloudSettingsItem {
    CloudSettingsItemSsid,
    CloudSettingsItemPassword,
    CloudSettingsItemBin,
};

static void cloud_scene_settings_submenu_callback(void* context, uint32_t index) {
    CloudApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cloud_scene_settings_build_menu(CloudApp* app) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    FuriString* label = furi_string_alloc();

    furi_string_printf(
        label, "SSID: %s", app->settings.ssid[0] ? app->settings.ssid : "(not set)");
    submenu_add_item(
        submenu,
        furi_string_get_cstr(label),
        CloudSettingsItemSsid,
        cloud_scene_settings_submenu_callback,
        app);

    furi_string_printf(
        label, "Password: %s", app->settings.password[0] ? "********" : "(not set)");
    submenu_add_item(
        submenu,
        furi_string_get_cstr(label),
        CloudSettingsItemPassword,
        cloud_scene_settings_submenu_callback,
        app);

    furi_string_printf(label, "Database: %s", app->settings.bin);
    submenu_add_item(
        submenu,
        furi_string_get_cstr(label),
        CloudSettingsItemBin,
        cloud_scene_settings_submenu_callback,
        app);

    furi_string_free(label);
}

void cloud_scene_settings_on_enter(void* context) {
    CloudApp* app = context;
    cloud_scene_settings_build_menu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CloudAppViewSubmenu);
}

bool cloud_scene_settings_on_event(void* context, SceneManagerEvent event) {
    CloudApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        CloudTextEditField field;
        if(event.event == CloudSettingsItemSsid) {
            field = CloudTextEditSsid;
        } else if(event.event == CloudSettingsItemPassword) {
            field = CloudTextEditPassword;
        } else {
            field = CloudTextEditBin;
        }
        scene_manager_set_scene_state(app->scene_manager, CloudSceneTextEdit, field);
        scene_manager_next_scene(app->scene_manager, CloudSceneTextEdit);
    }
    return consumed;
}

void cloud_scene_settings_on_exit(void* context) {
    CloudApp* app = context;
    submenu_reset(app->submenu);
}
