#pragma once
#include <gio/gio.h>
#include <stdint.h>


enum PortalCaptureType {
    PORTAL_CAPTURE_TYPE_MONITOR = 1 << 0,
    PORTAL_CAPTURE_TYPE_WINDOW = 1 << 1,
    PORTAL_CAPTURE_TYPE_VIRTUAL = 1 << 2,
};

enum PortalCursorMode {
    PORTAL_CURSOR_MODE_HIDDEN = 1 << 0,
    PORTAL_CURSOR_MODE_EMBEDDED = 1 << 1,
    PORTAL_CURSOR_MODE_METADATA = 1 << 2,
};

enum SrPortalCaptureType {
    SR_PORTAL_CAPTURE_TYPE_MONITOR = PORTAL_CAPTURE_TYPE_MONITOR,
    SR_PORTAL_CAPTURE_TYPE_WINDOW = PORTAL_CAPTURE_TYPE_WINDOW,
    SR_PORTAL_CAPTURE_TYPE_UNIFIED = PORTAL_CAPTURE_TYPE_MONITOR | PORTAL_CAPTURE_TYPE_WINDOW,
};

struct ScreencastPortalCapture {
    SrPortalCaptureType capture_type;

    GCancellable *cancellable;

    char *sessionHandle;
    char *restoreToken;

    uint32_t pipewireNode;
    bool cursorVisible;
    bool test_is_good;

    int pipewireFd;
};

void *
screencast_portal_desktop_capture_create(bool cursorVisible); // NOLINT(*-use-trailing-return-type)
void screencast_portal_capture_destroy(void *data);
void screencast_portal_unload();
