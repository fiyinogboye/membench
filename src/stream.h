#pragma once
#include <utility>
#include "common.h"

struct StreamPoint {
    int threads;
    double gbs;
};

// STREAM-style triad (a[i] = b[i] + s * c[i]) over three arrays that together
// are far larger than any cache, split evenly across `threads` threads.
// Memory is first-touched by the thread that will use it, so on NUMA systems
// pages land on that thread's local node.
// Reported as 24 bytes per element (2 reads + 1 write), like STREAM.
inline double triadGBs(size_t n, int threads) {
    AlignedBuffer A(n * sizeof(double)), B(n * sizeof(double)), C(n * sizeof(double));
    auto* a = reinterpret_cast<double*>(A.data());
    auto* b = reinterpret_cast<double*>(B.data());
    auto* c = reinterpret_cast<double*>(C.data());

    auto slice = [&](int t) {
        return std::pair<size_t, size_t>{n * t / threads, n * (t + 1) / threads};
    };

    runThreads(threads, [&](int t) {
        auto [lo, hi] = slice(t);
        for (size_t i = lo; i < hi; ++i) { a[i] = 0.0; b[i] = 1.0; c[i] = 2.0; }
    });

    constexpr int kReps = 10;
    const double scalar = 3.0;
    auto kernel = [&](int t) {
        auto [lo, hi] = slice(t);
        double* __restrict aa = a + lo;
        const double* __restrict bb = b + lo;
        const double* __restrict cc = c + lo;
        const size_t len = hi - lo;
        for (int r = 0; r < kReps; ++r) {
            for (size_t i = 0; i < len; ++i) aa[i] = bb[i] + scalar * cc[i];
            clobberMemory();
        }
    };

    double best = 0;
    for (int trial = 0; trial < 4; ++trial) {  // trial 0 doubles as warm-up
        const double t0 = nowSec();
        runThreads(threads, kernel);
        const double dt = nowSec() - t0;
        best = std::max(best, 24.0 * static_cast<double>(n) * kReps / dt / 1e9);
    }
    return best;
}

inline std::vector<StreamPoint> runStream(size_t totalBytes, int maxThreads) {
    const size_t n = totalBytes / (3 * sizeof(double));
    std::vector<int> counts;
    for (int t = 1; t < maxThreads; t *= 2) counts.push_back(t);
    counts.push_back(maxThreads);

    std::vector<StreamPoint> out;
    std::printf("\n== Triad bandwidth: %s total, thread scaling ==\n%10s %12s\n",
                formatBytes(totalBytes).c_str(), "threads", "GB/s");
    for (int t : counts) {
        double gbs = triadGBs(n, t);
        std::printf("%10d %12.1f\n", t, gbs);
        std::fflush(stdout);
        out.push_back({t, gbs});
    }
    return out;
}
