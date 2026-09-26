#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <expansion/expansion.h>
#include <storage/storage.h>

#include "scenes/cloud_scene.h"
#include "cloud_custom_event.h"
#include "cloud_settings.h"
#include "cloud_transport.h"
#include "cloud_worker.h"

#define CLOUD_TEXT_BUFFER_SIZE   CLOUD_SETTINGS_PASS_LEN
#define CLOUD_DOWNLOAD_DIR       "/ext/cloud_downloads"

typedef struct {
    Gui* gui;
    Expansion* expansion;
    DialogsApp* dialogs;

    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;

    CloudSettings settings;
    CloudWorker* worker;

    // Scratch buffer used by the shared text-edit scene.
    char text_buffer[CLOUD_TEXT_BUFFER_SIZE];

    // Path on the SD card of the file chosen for upload.
    FuriString* upload_source_path;
    // Remote (and local, for downloads) file name.
    FuriString* remote_name;
    // Destination path on the SD card for a download in progress.
    FuriString* download_dest_path;

    // Files returned by the last successful LIST, and how many of them there are.
    CloudFileEntry file_list[CLOUD_TRANSPORT_MAX_FILES];
    size_t file_count;
} CloudApp;

typedef enum {
    CloudAppViewSubmenu,
    CloudAppViewTextInput,
    CloudAppViewWidget,
} CloudAppView;
