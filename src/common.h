#pragma once
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

inline double nowSec() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

// Prevent the optimizer from deleting a computed value or reusing loaded memory.
template <class T>
inline void doNotOptimize(const T& v) {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(v) : "memory");
#else
    volatile T sink = v;
    (void)sink;
#endif
}

inline void clobberMemory() {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : : "memory");
#endif
}

// Page-aligned heap buffer (portable: over-allocate, then align manually).
class AlignedBuffer {
public:
    explicit AlignedBuffer(size_t bytes, size_t align = 4096)
        : raw_(new unsigned char[bytes + align]), size_(bytes) {
        auto p = reinterpret_cast<uintptr_t>(raw_.get());
        p = (p + align - 1) & ~static_cast<uintptr_t>(align - 1);
        data_ = reinterpret_cast<unsigned char*>(p);
    }
    unsigned char* data() const { return data_; }
    size_t size() const { return size_; }

private:
    std::unique_ptr<unsigned char[]> raw_;
    unsigned char* data_ = nullptr;
    size_t size_ = 0;
};

// Working-set sizes: two points per octave (x1, x1.5, x2, ...) from 4 KB to maxBytes.
inline std::vector<size_t> sizeSweep(size_t maxBytes) {
    std::vector<size_t> sizes;
    for (size_t s = 4096; s <= maxBytes; s *= 2) {
        sizes.push_back(s);
        if (s / 2 * 3 <= maxBytes) sizes.push_back(s / 2 * 3);
    }
    return sizes;
}

inline std::string formatBytes(size_t b) {
    char buf[32];
    if (b >= (1u << 20)) std::snprintf(buf, sizeof buf, "%g MB", b / 1048576.0);
    else std::snprintf(buf, sizeof buf, "%g KB", b / 1024.0);
    return buf;
}

// Calls fn() repeatedly until a batch lasts >= targetSec, then returns the best
// (lowest) per-call time over several batches. Taking the minimum filters out
// interference from the OS and other processes.
template <class F>
double measureSeconds(F&& fn, double targetSec = 0.03, int trials = 3) {
    size_t reps = 1;
    while (true) {
        double t0 = nowSec();
        for (size_t i = 0; i < reps; ++i) fn();
        double dt = nowSec() - t0;
        if (dt >= targetSec || reps >= (size_t{1} << 32)) break;
        reps *= (dt < targetSec / 16) ? 8 : 2;
    }
    double best = 1e30;
    for (int t = 0; t < trials; ++t) {
        double t0 = nowSec();
        for (size_t i = 0; i < reps; ++i) fn();
        best = std::min(best, (nowSec() - t0) / reps);
    }
    return best;
}

inline bool g_pinThreads = false;

inline void pinToCpu(int cpu) {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
#else
    (void)cpu;
#endif
}

// Runs f(threadIndex) on `n` threads and waits for all of them.
template <class F>
void runThreads(int n, F&& f) {
    const int hw = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    for (int t = 0; t < n; ++t) {
        pool.emplace_back([&, t] {
            if (g_pinThreads) pinToCpu(t % hw);
            f(t);
        });
    }
    for (auto& th : pool) th.join();
}
