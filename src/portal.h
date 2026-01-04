#pragma once
#include <gio/gio.h>

typedef void (*portal_signal_callback)(GVariant *parameters, void *user_data);

GDBusConnection *portal_get_dbus_connection(void);

void portal_create_request_path(char **out_path, char **out_token);
void portal_create_session_path(char **out_path, char **out_token);

void portal_signal_subscribe(const char *path, GCancellable *cancellable,
                             portal_signal_callback callback, void *user_data);
