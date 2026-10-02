#pragma once
#include "common.h"

struct BandwidthPoint {
    size_t bytes;
    double gbs;
};

// Single-thread sequential read bandwidth over a working set of `bytes`.
// The working set is re-read many times, so small sizes measure the cache that
// holds it and large sizes measure DRAM.
inline double readBandwidthGBs(size_t bytes) {
    AlignedBuffer buf(bytes);
    auto* p = reinterpret_cast<uint64_t*>(buf.data());
    const size_t n = bytes / sizeof(uint64_t);
    for (size_t i = 0; i < n; ++i) p[i] = i * 2654435761u;

    uint64_t sink = 0;
    auto pass = [&] {
        clobberMemory();
        uint64_t s = 0;
        for (size_t i = 0; i < n; ++i) s += p[i];
        sink += s;
        doNotOptimize(sink);
    };
    const double sec = measureSeconds(pass);
    return static_cast<double>(bytes) / sec / 1e9;
}

inline std::vector<BandwidthPoint> runBandwidth(size_t maxBytes) {
    std::vector<BandwidthPoint> out;
    std::printf("\n== Read bandwidth: 1 thread, sequential ==\n%10s %12s\n", "size", "GB/s");
    for (size_t bytes : sizeSweep(maxBytes)) {
        double gbs = readBandwidthGBs(bytes);
        std::printf("%10s %12.1f\n", formatBytes(bytes).c_str(), gbs);
        std::fflush(stdout);
        out.push_back({bytes, gbs});
    }
    return out;
}
