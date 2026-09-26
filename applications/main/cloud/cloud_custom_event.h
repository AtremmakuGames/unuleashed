#pragma once

// Custom events sent through the view dispatcher. Submenu items send their
// own index (0, 1, 2, ...) as the custom event, so real custom event codes
// start well above any realistic submenu size.
typedef enum {
    CloudEventTextInputDone = 100,
    CloudEventWorkerDone = 101,
    CloudEventBack = 102,
} CloudCustomEvent;

// Which field is being edited by the shared "text edit" scene, stashed via
// scene_manager_set_scene_state before navigating to it.
typedef enum {
    CloudTextEditSsid,
    CloudTextEditPassword,
    CloudTextEditBin,
} CloudTextEditField;
