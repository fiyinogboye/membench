#pragma once
#include <numeric>
#include <random>
#include "common.h"

struct LatencyPoint {
    size_t bytes;
    double ns;
};

// Average load-to-use latency for a working set of `bytes`, measured by pointer
// chasing: every load's address depends on whether the previous load's result, so the
// CPU cannot overlap them or prefetch ahead. One node per cache line, linked in
// a random single cycle (Sattolo's algorithm) so the access pattern defeats the
// hardware prefetcher.
inline double chaseLatencyNs(size_t bytes, size_t line) {
    const size_t n = bytes / line;
    AlignedBuffer buf(bytes);
    unsigned char* base = buf.data();

    std::vector<uint32_t> next(n);
    std::iota(next.begin(), next.end(), 0u);
    std::mt19937_64 rng(0x9e3779b97f4a7c15ull);
    for (size_t i = n - 1; i > 0; --i) {
        size_t j = rng() % i;
        std::swap(next[i], next[j]);
    }
    for (size_t i = 0; i < n; ++i)
        *reinterpret_cast<size_t*>(base + i * line) = static_cast<size_t>(next[i]) * line;

    size_t p = 0;
    auto chase = [&](size_t steps) {
        for (size_t i = 0; i < steps; ++i) p = *reinterpret_cast<const size_t*>(base + p);
        doNotOptimize(p);
    };

    chase(std::min<size_t>(2 * n, size_t{1} << 22));  // warm caches and TLB

    constexpr size_t kSteps = size_t{1} << 16;
    const double sec = measureSeconds([&] { chase(kSteps); });
    return sec / kSteps * 1e9;
}

inline std::vector<LatencyPoint> runLatency(size_t maxBytes, size_t line) {
    std::vector<LatencyPoint> out;
    std::printf("\n== Latency: pointer chase, %zu-byte nodes ==\n%10s %12s\n", line, "size", "ns/load");
    for (size_t bytes : sizeSweep(maxBytes)) {
        double ns = chaseLatencyNs(bytes, line);
        std::printf("%10s %12.2f\n", formatBytes(bytes).c_str(), ns);
        std::fflush(stdout);
        out.push_back({bytes, ns});
    }
    return out;
}
