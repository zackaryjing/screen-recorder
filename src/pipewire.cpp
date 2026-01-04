#include <cmath>
#include <cstdio>
#include <fcntl.h>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <spa/debug/format.h>
#include <spa/utils/result.h>

#include "pipewire.h"
#include "utils.h"


static FILE *ffmpeg_pipe = nullptr;


uint64_t now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

void start_ffmpeg_pipe() {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -y -loglevel error -stats "
             "-f rawvideo "
             "-pix_fmt bgra "
             "-s %dx%d "
             "-r %d "
             "-i - "
             "-c:v libx264 "
             "-preset ultrafast "
             "-tune zerolatency "
             "-crf 30 "
             "-pix_fmt yuv420p "
             "-vf scale=%d:%d:flags=fast_bilinear "
             "-movflags +faststart+frag_keyframe+empty_moov "
             "%s",
             WindowMonitor::width, WindowMonitor::height, SROptions::outputFps,
             SROptions::outputWidth, SROptions::outputHeight, SROptions::outputFile.c_str());

    ffmpeg_pipe = popen(cmd, "w");
}

void close_ffmpeg_pipe() {
    if (ffmpeg_pipe) {
        pclose(ffmpeg_pipe);
        ffmpeg_pipe = nullptr;
    }
}

void handle_sigint(int signo) {
    close_ffmpeg_pipe();
    exit(0);
}


static void on_process(void *data) {
    auto *cap = static_cast<pw_capture *>(data);

    static uint64_t last_write_ns = 0;
    static uint64_t frame_count = 0;
    frame_count++;

    pw_buffer *b = pw_stream_dequeue_buffer(cap->stream);
    if (!b)
        return;

    const spa_buffer *buf = b->buffer;
    if (!buf || buf->datas[0].chunk->size == 0) {
        pw_stream_queue_buffer(cap->stream, b);
        return;
    }

    if (!ffmpeg_pipe)
        start_ffmpeg_pipe();

    bool should_write = false;

    // 根据目标 fps 丢帧
    uint64_t t = now_ns();
    if (t - last_write_ns >= 1000000000ull * SROptions::inputFpsNum / SROptions::inputFpsDen) {
        last_write_ns = t;
        should_write = true;
    }

    if (should_write && ffmpeg_pipe) {
        fwrite(buf->datas[0].data, 1, WindowMonitor::width * WindowMonitor::height * 4,
               ffmpeg_pipe);
    }

    pw_stream_queue_buffer(cap->stream, b);
}

void on_param(void *data, uint32_t id, const struct spa_pod *param) {
    static bool sizeGot = false;
    if (not sizeGot) {
        pw_capture *cap = static_cast<pw_capture *>(data);

        if (param == nullptr)
            return;

        spa_video_info_raw info;
        if (spa_format_video_raw_parse(param, &info) >= 0) {
            sizeGot = true;
            WindowMonitor::width = info.size.width;
            WindowMonitor::height = info.size.height;
            if (SROptions::outputHeight == 0) {
                SROptions::outputHeight = WindowMonitor::height;
                SROptions::outputWidth = WindowMonitor::width;
            }
            printf("[pipewire] Got actual width=%d height=%d\n", WindowMonitor::width,
                   WindowMonitor::height);
        }
    }
}


static constexpr pw_stream_events stream_events = {
        PW_VERSION_STREAM_EVENTS,
        .param_changed = on_param,
        .process = on_process,
};

void pw_capture_start(pw_capture *cap) {
    printf("[pipewire] start capturing\n");
    pw_init(nullptr, nullptr);

    cap->loop = pw_thread_loop_new("pw-loop", NULL);
    pw_thread_loop_start(cap->loop);

    pw_thread_loop_lock(cap->loop);

    cap->context = pw_context_new(pw_thread_loop_get_loop(cap->loop), nullptr, 0);

    cap->core = pw_context_connect_fd(cap->context, fcntl(cap->pipewire_fd, F_DUPFD_CLOEXEC, 3),
                                      nullptr, 0);

    if (!cap->core) {
        fprintf(stderr, "pw_context_connect_fd failed\n");
        pw_thread_loop_unlock(cap->loop);
        return;
    }

    pw_properties *props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY,
                                             "Capture", PW_KEY_MEDIA_ROLE, "Screen", NULL);

    cap->stream = pw_stream_new(cap->core, "screen-capture", props);


    pw_stream_add_listener(cap->stream, &cap->stream_listener, &stream_events, cap);

    spa_pod_builder b;
    uint8_t buffer[1024];
    const spa_pod *params[1];

    spa_pod_builder_init(&b, buffer, sizeof(buffer));
    auto framerate = SPA_FRACTION(SROptions::inputFpsNum, SROptions::inputFpsDen);
    printf("[pipewire] targeting fps num: %d ,fps denom: %d\n", SROptions::inputFpsNum,
           SROptions::inputFpsDen);
    constexpr auto min_framerate = SPA_FRACTION(0, 1);
    uint maxRate = SROptions::inputFpsNum / SROptions::inputFpsDen + 1;
    auto max_framerate = SPA_FRACTION(maxRate, 1);
    auto resolution = SPA_RECTANGLE(1920, 1180);
    auto min_resolution = SPA_RECTANGLE(1, 1);
    auto max_resolution = SPA_RECTANGLE(8192, 4320);


    params[0] = static_cast<spa_pod *>(spa_pod_builder_add_object(
            &b, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat, SPA_FORMAT_mediaType,
            SPA_POD_Id(SPA_MEDIA_TYPE_video), SPA_FORMAT_mediaSubtype,
            SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), SPA_FORMAT_VIDEO_format,
            SPA_POD_Id(SPA_VIDEO_FORMAT_BGRA), SPA_FORMAT_VIDEO_size,
            SPA_POD_CHOICE_RANGE_Rectangle(&resolution, &min_resolution, &max_resolution),
            SPA_FORMAT_VIDEO_framerate,
            SPA_POD_CHOICE_RANGE_Fraction(&framerate, &min_framerate, &max_framerate)));

    pw_stream_connect(
            cap->stream, PW_DIRECTION_INPUT, cap->node_id,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
            params, 1);


    pw_stream_set_active(cap->stream, true);

    pw_thread_loop_unlock(cap->loop);
}
