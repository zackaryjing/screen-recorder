#include <iostream>
#include <glib.h>
#include "screencast-portal.hpp"
#include "pipewire.h"
#include "utils.h"

using namespace std;

int main(int argc, char *argv[]) {
    parse_cli(argc, argv);

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    cout << "[SR] screen record starting" << endl;
    const auto capture = static_cast<struct ScreencastPortalCapture *>(
        screencast_portal_desktop_capture_create(true));

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    g_main_loop_run(loop);

    cout << "[SR] screen record ending..." << endl;
    screencast_portal_capture_destroy(capture);
    screencast_portal_unload();
    g_main_loop_unref(loop);
    cout << "[SR] screen record ended" << endl;

    return 0;

}
