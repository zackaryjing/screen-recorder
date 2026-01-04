#include <cstddef>
#include <cstdint>

#include <gio/gunixfdlist.h>
#include <pipewire/pipewire.h>

#include "pipewire.h"
#include "portal.h"
#include "screencast-portal.hpp"

static GDBusProxy *screencast_proxy = nullptr;

void ensure_screencast_portal_proxy() {
    g_autoptr(GError) error = nullptr;
    if (!screencast_proxy) {
        screencast_proxy = g_dbus_proxy_new_sync(
                portal_get_dbus_connection(), G_DBUS_PROXY_FLAGS_NONE, nullptr,
                "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                "org.freedesktop.portal.ScreenCast", NULL, &error);

        if (error) {
            printf("[portals] Error retrieving D-Bus proxy: %s\n", error->message);
            return;
        }
    }
}

GDBusProxy *get_screencast_portal_proxy() {
    ensure_screencast_portal_proxy();
    return screencast_proxy;
}

uint32_t get_available_capture_types() {
    g_autoptr(GVariant) cached_source_types = nullptr;

    ensure_screencast_portal_proxy();

    if (!screencast_proxy)
        return 0;

    cached_source_types =
            g_dbus_proxy_get_cached_property(screencast_proxy, "AvailableSourceTypes");
    const uint32_t available_source_types =
            cached_source_types ? g_variant_get_uint32(cached_source_types) : 0;

    return available_source_types;
}

uint32_t get_available_cursor_modes() {
    g_autoptr(GVariant) cached_cursor_modes = nullptr;

    ensure_screencast_portal_proxy();

    if (!screencast_proxy)
        return 0;

    cached_cursor_modes =
            g_dbus_proxy_get_cached_property(screencast_proxy, "AvailableCursorModes");
    const uint32_t available_cursor_modes =
            cached_cursor_modes ? g_variant_get_uint32(cached_cursor_modes) : 0;

    return available_cursor_modes;
}

uint32_t get_screencast_version(void) {
    g_autoptr(GVariant) cached_version = NULL;
    uint32_t version;

    ensure_screencast_portal_proxy();

    if (!screencast_proxy)
        return 0;

    cached_version = g_dbus_proxy_get_cached_property(screencast_proxy, "version");
    version = cached_version ? g_variant_get_uint32(cached_version) : 0;

    return version;
}

/* ------------------------------------------------- */

const char *capture_type_to_string(enum SrPortalCaptureType capture_type) {
    switch (capture_type) {
        case SR_PORTAL_CAPTURE_TYPE_MONITOR:
            return "monitor";
        case SR_PORTAL_CAPTURE_TYPE_WINDOW:
            return "window";
        case SR_PORTAL_CAPTURE_TYPE_UNIFIED:
            return "monitor and window";
        default:
            return "unknown";
    }
}

/* ------------------------------------------------- */

void on_pipewire_remote_opened_cb(GObject *source, GAsyncResult *res, void *user_data) {
    ScreencastPortalCapture *capture;
    g_autoptr(GUnixFDList) fd_list = nullptr;
    g_autoptr(GVariant) result = nullptr;
    g_autoptr(GError) error = nullptr;
    int pipewire_fd;
    int fd_index;

    capture = static_cast<ScreencastPortalCapture *>(user_data);
    result =
            g_dbus_proxy_call_with_unix_fd_list_finish(G_DBUS_PROXY(source), &fd_list, res, &error);
    if (error) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            printf("[pipewire] Error retrieving pipewire fd: %s\n", error->message);
        return;
    }

    g_variant_get(result, "(h)", &fd_index, &error);

    pipewire_fd = g_unix_fd_list_get(fd_list, fd_index, &error);
    capture->pipewireFd = pipewire_fd;
    if (error) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            printf("[pipewire] Error retrieving pipewire fd: %s\n", error->message);
        return;
    }

    auto pw_cap = new pw_capture{};
    pw_cap->pipewire_fd = capture->pipewireFd;
    pw_cap->node_id = capture->pipewireNode;
    pw_capture_start(pw_cap);
}

void open_pipewire_remote(ScreencastPortalCapture *capture) {
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    g_dbus_proxy_call_with_unix_fd_list(get_screencast_portal_proxy(), "OpenPipeWireRemote",
                                        g_variant_new("(oa{sv})", capture->sessionHandle, &builder),
                                        G_DBUS_CALL_FLAGS_NONE, -1, NULL, capture->cancellable,
                                        on_pipewire_remote_opened_cb, capture);
}

/* ------------------------------------------------- */

void on_start_response_received_cb(GVariant *parameters, void *user_data) {
    ScreencastPortalCapture *capture = (ScreencastPortalCapture *) user_data;
    g_autoptr(GVariant) stream_properties = nullptr;
    g_autoptr(GVariant) streams = nullptr;
    g_autoptr(GVariant) result = nullptr;
    GVariantIter iter;
    uint32_t response;

    g_variant_get(parameters, "(u@a{sv})", &response, &result);

    if (response != 0) {
        printf("[pipewire] Failed to start screencast, denied or cancelled by user\n");
        return;
    }

    streams = g_variant_lookup_value(result, "streams", G_VARIANT_TYPE_ARRAY);

    g_variant_iter_init(&iter, streams);

    const size_t n_streams = g_variant_iter_n_children(&iter);
    if (n_streams != 1) {
        printf("[pipewire] Received more than one stream when only one was expected. "
               "This is probably a bug in the desktop portal implementation you are "
               "using.\n");

        // The KDE Desktop portal implementation sometimes sends an invalid
        // response where more than one stream is attached, and only the
        // last one is the one we're looking for. This is the only known
        // buggy implementation, so let's at least try to make it work here.
        for (size_t i = 0; i < n_streams - 1; i++) {
            g_autoptr(GVariant) throwaway_properties = nullptr;
            uint32_t throwaway_pipewire_node;

            g_variant_iter_loop(&iter, "(u@a{sv})", &throwaway_pipewire_node,
                                &throwaway_properties);
        }
    }

    g_variant_iter_loop(&iter, "(u@a{sv})", &capture->pipewireNode, &stream_properties);

    if (get_screencast_version() >= 4) {
        g_autoptr(GVariant) restore_token = nullptr;
    }

    printf("[pipewire] source selected, setting up screencast\n");

    open_pipewire_remote(capture);
}

void on_started_cb(GObject *source, GAsyncResult *res, void *user_data) {
    (void) (user_data);

    g_autoptr(GVariant) result = NULL;
    g_autoptr(GError) error = NULL;

    result = g_dbus_proxy_call_finish(G_DBUS_PROXY(source), res, &error);
    if (error) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            printf("[pipewire] Error selecting screencast source: %s\n", error->message);
        return;
    }
}

void start(struct ScreencastPortalCapture *capture) {
    GVariantBuilder builder;
    char *request_token;
    char *request_path;

    portal_create_request_path(&request_path, &request_token);

    printf("[pipewire] Asking for %s\n", capture_type_to_string(capture->capture_type));

    portal_signal_subscribe(request_path, capture->cancellable, on_start_response_received_cb,
                            capture);

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string(request_token));

    g_dbus_proxy_call(get_screencast_portal_proxy(), "Start",
                      g_variant_new("(osa{sv})", capture->sessionHandle, "", &builder),
                      G_DBUS_CALL_FLAGS_NONE, -1, capture->cancellable, on_started_cb, nullptr);
}

/* ------------------------------------------------- */

void on_select_source_response_received_cb(GVariant *parameters, void *user_data) {
    auto *capture = static_cast<ScreencastPortalCapture *>(user_data);
    g_autoptr(GVariant) ret = nullptr;
    uint32_t response;

    printf("[pipewire] Response to select source received\n");

    g_variant_get(parameters, "(u@a{sv})", &response, &ret);

    if (response != 0) {
        printf("[pipewire] Failed to select source, denied or cancelled by user\n");
        return;
    }

    start(capture);
}

void on_source_selected_cb(GObject *source, GAsyncResult *res, void *user_data) {
    g_autoptr(GVariant) result = nullptr;
    g_autoptr(GError) error = nullptr;

    result = g_dbus_proxy_call_finish(G_DBUS_PROXY(source), res, &error);
    if (error) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            printf("[pipewire] Error selecting screencast source: %s\n", error->message);
        return;
    }
}

void select_source(struct ScreencastPortalCapture *capture) {
    GVariantBuilder builder;
    uint32_t available_cursor_modes;
    char *request_token;
    char *request_path;

    portal_create_request_path(&request_path, &request_token);

    portal_signal_subscribe(request_path, capture->cancellable,
                            on_select_source_response_received_cb, capture);

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "types", g_variant_new_uint32(capture->capture_type));
    g_variant_builder_add(&builder, "{sv}", "multiple", g_variant_new_boolean(FALSE));
    g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string(request_token));

    available_cursor_modes = get_available_cursor_modes();

    if (available_cursor_modes & PORTAL_CURSOR_MODE_METADATA)
        g_variant_builder_add(&builder, "{sv}", "cursor_mode",
                              g_variant_new_uint32(PORTAL_CURSOR_MODE_METADATA));
    else if ((available_cursor_modes & PORTAL_CURSOR_MODE_EMBEDDED) && capture->cursorVisible)
        g_variant_builder_add(&builder, "{sv}", "cursor_mode",
                              g_variant_new_uint32(PORTAL_CURSOR_MODE_EMBEDDED));
    else
        g_variant_builder_add(&builder, "{sv}", "cursor_mode",
                              g_variant_new_uint32(PORTAL_CURSOR_MODE_HIDDEN));

    if (get_screencast_version() >= 4) {
        g_variant_builder_add(&builder, "{sv}", "persist_mode", g_variant_new_uint32(2));
        if (capture->restoreToken && *capture->restoreToken) {
            g_variant_builder_add(&builder, "{sv}", "restore_token",
                                  g_variant_new_string(capture->restoreToken));
        }
    }

    g_dbus_proxy_call(get_screencast_portal_proxy(), "SelectSources",
                      g_variant_new("(oa{sv})", capture->sessionHandle, &builder),
                      G_DBUS_CALL_FLAGS_NONE, -1, capture->cancellable, on_source_selected_cb,
                      nullptr);

    free(request_token);
    free(request_path);
}

/* ------------------------------------------------- */

void on_create_session_response_received_cb(GVariant *parameters, void *user_data) {
    struct ScreencastPortalCapture *capture = (ScreencastPortalCapture *) user_data;
    g_autoptr(GVariant) session_handle_variant = NULL;
    g_autoptr(GVariant) result = NULL;
    uint32_t response;

    g_variant_get(parameters, "(u@a{sv})", &response, &result);

    if (response != 0) {
        printf("[pipewire] Failed to create session, denied or cancelled by user\n");
        return;
    }

    printf("[pipewire] Screencast session created\n");

    session_handle_variant = g_variant_lookup_value(result, "session_handle", nullptr);
    capture->sessionHandle = g_variant_dup_string(session_handle_variant, nullptr);

    select_source(capture);
}

void on_session_created_cb(GObject *source, GAsyncResult *res, void *user_data) {
    (void) (user_data);

    g_autoptr(GVariant) result = nullptr;
    g_autoptr(GError) error = nullptr;

    result = g_dbus_proxy_call_finish(G_DBUS_PROXY(source), res, &error);
    if (error) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            printf("[pipewire] Error creating screencast session: %s\n", error->message);
        return;
    }
}

void create_session(struct ScreencastPortalCapture *capture) {
    GVariantBuilder builder;
    char *session_token;
    char *request_token;
    char *request_path;

    portal_create_request_path(&request_path, &request_token);
    portal_create_session_path(nullptr, &session_token);

    portal_signal_subscribe(request_path, capture->cancellable,
                            on_create_session_response_received_cb, capture);

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string(request_token));
    g_variant_builder_add(&builder, "{sv}", "session_handle_token",
                          g_variant_new_string(session_token));

    g_dbus_proxy_call(get_screencast_portal_proxy(), "CreateSession",
                      g_variant_new("(a{sv})", &builder), G_DBUS_CALL_FLAGS_NONE, -1,
                      capture->cancellable, on_session_created_cb, nullptr);
}

/* ------------------------------------------------- */

gboolean init_screencast_capture(struct ScreencastPortalCapture *capture) {
    GDBusConnection *connection;
    GDBusProxy *proxy;

    capture->cancellable = g_cancellable_new();
    connection = portal_get_dbus_connection();
    if (!connection)
        return FALSE;
    proxy = get_screencast_portal_proxy();
    if (!proxy)
        return FALSE;

    printf("[pipewire] pipeWire initialized\n");

    create_session(capture);

    return TRUE;
}


void *screencast_portal_desktop_capture_create(bool cursorVisible) {
    const auto capture = new ScreencastPortalCapture{};
    capture->capture_type = SR_PORTAL_CAPTURE_TYPE_WINDOW;
    capture->cursorVisible = cursorVisible;

    init_screencast_capture(capture);

    return capture;
}

void screencast_portal_capture_destroy(void *data) {
    const auto capture = static_cast<ScreencastPortalCapture *>(data);

    if (!capture)
        return;

    if (capture->sessionHandle) {
        g_dbus_connection_call(portal_get_dbus_connection(), "org.freedesktop.portal.Desktop",
                               capture->sessionHandle, "org.freedesktop.portal.Session", "Close",
                               nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);

        g_clear_pointer(&capture->sessionHandle, g_free);
    }

    g_cancellable_cancel(capture->cancellable);
    g_clear_object(&capture->cancellable);
}


void screencast_portal_unload() { g_clear_object(&screencast_proxy); }
