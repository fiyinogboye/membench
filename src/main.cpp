// membench: memory hierarchy microbenchmarks.
//   latency    pointer-chase latency vs working-set size (reveals cache levels)
//   bandwidth  single-thread read bandwidth vs working-set size
//   stream     multi-thread triad bandwidth vs thread count (DRAM scaling)
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "bandwidth.h"
#include "common.h"
#include "latency.h"
#include "stream.h"

static void usage(const char* prog) {
    std::printf(
        "Usage: %s [latency|bandwidth|stream|all] [options]\n"
        "  --max-mb N      largest working set for latency/bandwidth (default 256)\n"
        "  --stream-mb N   total array size for the triad test (default 512)\n"
        "  --threads N     max threads for the triad test (default: all cores)\n"
        "  --line N        node spacing in bytes; use the cache line size\n"
        "                  (64 on x86, 128 on Apple Silicon) (default 64)\n"
        "  --pin           pin threads to cores (Linux only)\n"
        "  --out-dir DIR   write latency.csv / bandwidth.csv / stream.csv to DIR\n",
        prog);
}

int main(int argc, char** argv) {
    std::string mode = "all";
    size_t maxMB = 256, streamMB = 512, line = 64;
    int threads = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    std::string outDir;

    for (int i = 1; i < argc; ++i) {
        auto is = [&](const char* s) { return std::strcmp(argv[i], s) == 0; };
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) { usage(argv[0]); std::exit(1); }
            return argv[++i];
        };
        if (is("latency") || is("bandwidth") || is("stream") || is("all")) mode = argv[i];
        else if (is("--max-mb")) maxMB = std::strtoull(next(), nullptr, 10);
        else if (is("--stream-mb")) streamMB = std::strtoull(next(), nullptr, 10);
        else if (is("--threads")) threads = std::max(1, std::atoi(next()));
        else if (is("--line")) line = std::strtoull(next(), nullptr, 10);
        else if (is("--pin")) g_pinThreads = true;
        else if (is("--out-dir")) outDir = next();
        else { usage(argv[0]); return is("--help") ? 0 : 1; }
    }
    if (line < 8 || (line & (line - 1)) != 0) {
        std::fprintf(stderr, "--line must be a power of two >= 8\n");
        return 1;
    }
#if !defined(__linux__)
    if (g_pinThreads) std::fprintf(stderr, "note: --pin is only supported on Linux; ignoring\n");
#endif
    if (g_pinThreads) pinToCpu(0);

    std::printf("membench  |  hardware threads: %u  |  pointer size: %zu bits\n",
                std::thread::hardware_concurrency(), sizeof(void*) * 8);
#ifdef __VERSION__
    std::printf("compiler: %s\n", __VERSION__);
#endif

    if (!outDir.empty()) std::filesystem::create_directories(outDir);
    auto csv = [&](const char* name) { return std::ofstream(std::filesystem::path(outDir) / name); };

    const size_t maxBytes = maxMB << 20;
    if (mode == "latency" || mode == "all") {
        auto r = runLatency(maxBytes, line);
        if (!outDir.empty()) {
            auto f = csv("latency.csv");
            f << "bytes,ns\n";
            for (auto& p : r) f << p.bytes << ',' << p.ns << '\n';
        }
    }
    if (mode == "bandwidth" || mode == "all") {
        auto r = runBandwidth(maxBytes);
        if (!outDir.empty()) {
            auto f = csv("bandwidth.csv");
            f << "bytes,gbs\n";
            for (auto& p : r) f << p.bytes << ',' << p.gbs << '\n';
        }
    }
    if (mode == "stream" || mode == "all") {
        auto r = runStream(streamMB << 20, threads);
        if (!outDir.empty()) {
            auto f = csv("stream.csv");
            f << "threads,gbs\n";
            for (auto& p : r) f << p.threads << ',' << p.gbs << '\n';
        }
    }
    return 0;
}
