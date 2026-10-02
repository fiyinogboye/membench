# membench

A small C++17 suite of memory-hierarchy microbenchmarks. It measures cache and
DRAM latency and bandwidth directly, with no dependencies, and plots the results
so the L1 / L2 / L3 / DRAM boundaries of any CPU are visible at a glance.

| Test | What it measures |
|------|------------------|
| `latency` | Load-to-use latency vs working-set size, using a dependent pointer chase |
| `bandwidth` | Single-thread sequential read bandwidth vs working-set size |
| `stream` | STREAM-style triad bandwidth vs thread count (DRAM scaling) |

## Build

```bash
# CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# or directly
c++ -std=c++17 -O3 -pthread src/main.cpp -o membench
```

On x86 you can add `-march=native` for wider vectorization in the bandwidth tests.

## Run

```bash
./membench all --out-dir results
python3 scripts/plot.py results --title "Your CPU model" --out docs
```

Plotting needs `pip install matplotlib`.

```
Usage: membench [latency|bandwidth|stream|all] [options]
  --max-mb N      largest working set for latency/bandwidth (default 256)
  --stream-mb N   total array size for the triad test (default 512)
  --threads N     max threads for the triad test (default: all cores)
  --line N        node spacing in bytes; use the cache line size
                  (64 on x86, 128 on Apple Silicon) (default 64)
  --pin           pin threads to cores (Linux only)
  --out-dir DIR   write latency.csv / bandwidth.csv / stream.csv to DIR
```

For stable numbers, close other applications, plug in power, and run it a few times.

## Methodology

- **Latency.** Each node occupies one cache line and stores the offset of the next
  node. Nodes are linked into a single random cycle (Sattolo's shuffle), so every
  load depends on the previous one and the hardware prefetcher cannot predict the
  pattern. Time per load is total time divided by loads.
- **Bandwidth.** The working set is re-read repeatedly with a plain summation loop
  that the compiler vectorizes. Small sizes therefore measure the cache that holds
  the data; large sizes measure DRAM.
- **Triad.** `a[i] = b[i] + s*c[i]` over three arrays far larger than any cache,
  split evenly across threads. Memory is first-touched by the thread that uses it,
  which keeps pages NUMA-local. Bandwidth is counted as 24 bytes per element
  (two reads and one write), matching the STREAM convention.
- **Timing.** Each measurement repeats the kernel until a batch lasts at least
  30 ms, and reports the best of several batches to filter out OS noise.

## Reading the results

- Latency is flat while the working set fits in a cache level and steps up when it
  spills into the next one. The step positions give the effective cache sizes and
  the plateaus give each level's latency.
- On chiplet CPUs, the L3 step usually appears at the per-chiplet L3 capacity
  rather than the total L3 listed on the spec sheet.
- Very large working sets also include TLB-miss cost, so the last step can be
  steeper than raw DRAM latency.
- Triad bandwidth typically stops scaling well before all cores are in use, once
  the memory controllers are saturated.

## Layout

```
src/
  common.h      timing, aligned buffer, size sweep, thread helpers
  latency.h     pointer-chase latency test
  bandwidth.h   single-thread read bandwidth test
  stream.h      multithreaded triad test
  main.cpp      CLI and CSV output
scripts/plot.py CSV to PNG plots
```
