#include "cloud_settings.h"

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/stream.h>
#include <string.h>

#define CLOUD_SETTINGS_DIR  "/ext/apps_data/cloud"
#define CLOUD_SETTINGS_PATH CLOUD_SETTINGS_DIR "/config.txt"

static void cloud_settings_set_defaults(CloudSettings* settings) {
    memset(settings, 0, sizeof(CloudSettings));
    strlcpy(settings->bin, "flipper-cloud-demo", sizeof(settings->bin));
}

void cloud_settings_load(CloudSettings* settings) {
    furi_assert(settings);
    cloud_settings_set_defaults(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    if(file_stream_open(stream, CLOUD_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();

        if(stream_read_line(stream, line)) {
            furi_string_trim(line);
            strlcpy(settings->ssid, furi_string_get_cstr(line), sizeof(settings->ssid));
        }
        if(stream_read_line(stream, line)) {
            furi_string_trim(line);
            strlcpy(settings->password, furi_string_get_cstr(line), sizeof(settings->password));
        }
        if(stream_read_line(stream, line)) {
            furi_string_trim(line);
            if(!furi_string_empty(line)) {
                strlcpy(settings->bin, furi_string_get_cstr(line), sizeof(settings->bin));
            }
        }

        furi_string_free(line);
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

bool cloud_settings_save(const CloudSettings* settings) {
    furi_assert(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, CLOUD_SETTINGS_DIR);

    Stream* stream = file_stream_alloc(storage);
    bool success = false;

    if(file_stream_open(stream, CLOUD_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        success = true;
        success &= (stream_write_format(stream, "%s\n", settings->ssid) > 0);
        success &= (stream_write_format(stream, "%s\n", settings->password) > 0);
        success &= (stream_write_format(stream, "%s\n", settings->bin) > 0);
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    return success;
}
