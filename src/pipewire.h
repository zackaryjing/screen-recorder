#pragma once

#include <pipewire/pipewire.h>
#include <stdint.h>
#include "pipewire.h"

struct pw_capture {
    int pipewire_fd;

    pw_thread_loop *loop;
    pw_context *context;
    pw_core *core;

    pw_stream *stream;
    spa_hook stream_listener;

    uint32_t node_id;
};

void pw_capture_start(struct pw_capture *cap);

void handle_sigint(int signo);
