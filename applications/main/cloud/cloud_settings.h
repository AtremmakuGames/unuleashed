#pragma once

#include <stdbool.h>
#include <stddef.h>

#define CLOUD_SETTINGS_SSID_LEN 33
#define CLOUD_SETTINGS_PASS_LEN 65
#define CLOUD_SETTINGS_BIN_LEN  33

typedef struct {
    char ssid[CLOUD_SETTINGS_SSID_LEN];
    char password[CLOUD_SETTINGS_PASS_LEN];
    // filebin.net "bin" identifier: acts as the shared public folder / database name
    // that this firmware uploads to and downloads from.
    char bin[CLOUD_SETTINGS_BIN_LEN];
} CloudSettings;

// Loads settings from SD card, filling in defaults for anything missing.
void cloud_settings_load(CloudSettings* settings);

// Persists settings to SD card.
bool cloud_settings_save(const CloudSettings* settings);
