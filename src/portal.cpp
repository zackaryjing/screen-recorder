#include "portal.h"
#include <cstdint>
#include <stdio.h>


struct portal_signal_call {
    GCancellable *cancellable;
    portal_signal_callback callback;
    gpointer user_data;
    char *request_path;
    guint signal_id;
    gulong cancelled_id;
};


#define REQUEST_PATH "/org/freedesktop/portal/desktop/request/%s/sr%u"
#define SESSION_PATH "/org/freedesktop/portal/desktop/session/%s/sr%u"

static GDBusConnection *connection = nullptr;

static void ensure_connection() {
    g_autoptr(GError) error = nullptr;
    if (!connection) {
        connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);

        if (error) {
            printf("[portals] Error retrieving D-Bus connection: %s", error->message);
        }
    }
}

char *get_sender_name() {
    char *sender_name;
    char *aux;

    ensure_connection();

    sender_name = strdup(g_dbus_connection_get_unique_name(connection) + 1);

    /* Replace dots by underscores */
    while ((aux = strstr(sender_name, ".")) != nullptr)
        *aux = '_';

    return sender_name;
}

GDBusConnection *portal_get_dbus_connection() {
    ensure_connection();
    return connection;
}

void portal_create_request_path(char **out_path, char **out_token) {
    static uint32_t request_token_count = 0;
    request_token_count++;

    if (out_token) {
        if (asprintf(out_token, "sr%u", request_token_count) < 0)
            *out_token = nullptr;
    }

    if (out_path) {
        char *sender_name = get_sender_name();
        if (asprintf(out_path, REQUEST_PATH, sender_name, request_token_count) < 0)
            *out_path = nullptr;
    }
}


void portal_create_session_path(char **out_path, char **out_token) {
    static uint32_t session_token_count = 0;
    session_token_count++;

    if (out_token) {
        if (asprintf(out_token, "sr%u", session_token_count) < 0)
            *out_token = nullptr;
    }

    if (out_path) {
        char *sender_name = get_sender_name();
        if (asprintf(out_path, SESSION_PATH, sender_name, session_token_count) < 0)
            *out_path = nullptr;
    }
}

static void portal_signal_call_free(portal_signal_call *call) {
    if (call->signal_id)
        g_dbus_connection_signal_unsubscribe(portal_get_dbus_connection(), call->signal_id);

    if (call->cancelled_id > 0)
        g_signal_handler_disconnect(call->cancellable, call->cancelled_id);
}

static void on_cancelled_cb(GCancellable *cancellable, void *data) {
    const auto call = (portal_signal_call *) data;

    printf("[portals] Request cancelled");

    g_dbus_connection_call(portal_get_dbus_connection(), "org.freedesktop.portal.Desktop",
                           call->request_path, "org.freedesktop.portal.Request", "Close", nullptr,
                           nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);

    portal_signal_call_free(call);
}

static void on_response_received_cb(GDBusConnection *connection, const char *sender_name,
                                    const char *object_path, const char *interface_name,
                                    const char *signal_name, GVariant *parameters,
                                    void *user_data) {
    const auto call = static_cast<portal_signal_call *>(user_data);

    if (call->callback)
        call->callback(parameters, call->user_data);

    portal_signal_call_free(call);
}

void portal_signal_subscribe(const char *path, GCancellable *cancellable,
                             portal_signal_callback callback, gpointer user_data) {
    struct portal_signal_call *call;

    call = new portal_signal_call{};
    call->request_path = strdup(path);
    call->callback = callback;
    call->user_data = user_data;
    call->cancellable = cancellable ? g_object_ref(cancellable) : nullptr;
    call->cancelled_id = cancellable ? g_signal_connect(cancellable, "cancelled",
                                                        G_CALLBACK(on_cancelled_cb), call)
                                     : 0;
    call->signal_id = g_dbus_connection_signal_subscribe(
            portal_get_dbus_connection(), "org.freedesktop.portal.Desktop",
            "org.freedesktop.portal.Request", "Response", call->request_path, nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE, on_response_received_cb, call, nullptr);
}
