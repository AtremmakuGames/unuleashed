#include "../cloud_app_i.h"

#include <string.h>

static void cloud_scene_text_edit_callback(void* context) {
    CloudApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CloudEventTextInputDone);
}

void cloud_scene_text_edit_on_enter(void* context) {
    CloudApp* app = context;
    CloudTextEditField field =
        scene_manager_get_scene_state(app->scene_manager, CloudSceneTextEdit);

    const char* header = "Set value";
    const char* current = "";

    if(field == CloudTextEditSsid) {
        header = "Wi-Fi SSID";
        current = app->settings.ssid;
    } else if(field == CloudTextEditPassword) {
        header = "Wi-Fi password";
        current = app->settings.password;
    } else {
        header = "Database (filebin) name";
        current = app->settings.bin;
    }

    strlcpy(app->text_buffer, current, sizeof(app->text_buffer));

    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input,
        cloud_scene_text_edit_callback,
        app,
        app->text_buffer,
        sizeof(app->text_buffer),
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, CloudAppViewTextInput);
}

bool cloud_scene_text_edit_on_event(void* context, SceneManagerEvent event) {
    CloudApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == CloudEventTextInputDone) {
        CloudTextEditField field =
            scene_manager_get_scene_state(app->scene_manager, CloudSceneTextEdit);

        if(field == CloudTextEditSsid) {
            strlcpy(app->settings.ssid, app->text_buffer, sizeof(app->settings.ssid));
        } else if(field == CloudTextEditPassword) {
            strlcpy(app->settings.password, app->text_buffer, sizeof(app->settings.password));
        } else {
            strlcpy(app->settings.bin, app->text_buffer, sizeof(app->settings.bin));
        }

        cloud_settings_save(&app->settings);
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    return consumed;
}

void cloud_scene_text_edit_on_exit(void* context) {
    CloudApp* app = context;
    text_input_reset(app->text_input);
}
