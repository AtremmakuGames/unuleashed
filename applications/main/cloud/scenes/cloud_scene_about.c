#include "../cloud_app_i.h"

void cloud_scene_about_on_enter(void* context) {
    CloudApp* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "Cloud\n\n"
        "Upload/download files to a\n"
        "public database over Wi-Fi.\n\n"
        "Requires an ESP32 Wi-Fi\n"
        "Devboard flashed with the\n"
        "cloud_http companion\n"
        "firmware (see\n"
        "documentation/devboard/\n"
        "cloud_companion).\n\n"
        "Backend: filebin.net");
    view_dispatcher_switch_to_view(app->view_dispatcher, CloudAppViewWidget);
}

bool cloud_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cloud_scene_about_on_exit(void* context) {
    CloudApp* app = context;
    widget_reset(app->widget);
}
