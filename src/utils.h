#pragma once

#include <cmath>
#include <ctime>
#include <getopt.h>
#include <iostream>
#include <string>

using std::string;

class WindowMonitor {
public:
    static inline uint width;
    static inline uint height;
};

class SROptions {
public:
    SROptions() = delete;
    static inline uint outputWidth;
    static inline uint outputHeight;
    static inline uint inputFpsNum = 1;
    static inline uint inputFpsDen = 1;
    static inline uint outputFps = 30;
    static inline string outputFile;
};

static void parse_cli(int argc, char *argv[]) {
    static option long_options[] = {{"input-fps", required_argument, 0, 'i'},
                                    {"output-fps", required_argument, 0, 'o'},
                                    {"resolution", required_argument, 0, 'r'},
                                    {"output", required_argument, 0, 'f'},
                                    {"help", no_argument, 0, 'h'},
                                    {0, 0, 0, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "i:o:r:f:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'i':
                int num, denom;
                if (sscanf(optarg, "%d/%d", &num, &denom) == 2) {
                    SROptions::inputFpsNum = num;
                    SROptions::inputFpsDen = denom;
                }
                break;
            case 'o':
                SROptions::outputFps = std::atoi(optarg);
                break;
            case 'r': {
                int w = 0, h = 0;
                if (sscanf(optarg, "%dx%d", &w, &h) == 2) {
                    SROptions::outputWidth = w;
                    SROptions::outputHeight = h;
                } else {
                    std::cerr << "[Utils] Invalid resolution format, use WIDTHxHEIGHT\n";
                    std::exit(1);
                }
                break;
            }
            case 'f':
                SROptions::outputFile = optarg;
                break;
            case 'h':
            default:
                std::cout << "[Utils] Usage: program [--input-fps N/N] [--output-fps N] "
                             "[--resolution WxH] [--output FILE]\n";
                std::exit(0);
        }
    }

    if (SROptions::outputFile.empty()) {
        std::time_t t = std::time(nullptr);
        char buf[128];
        std::strftime(buf, sizeof(buf), "record_%Y%m%d_%H%M%S.mp4", std::localtime(&t));
        SROptions::outputFile = buf;
    }
}
