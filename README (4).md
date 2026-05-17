# SimpleMultithreader

> A header-only C++ library that wraps POSIX pthreads into a clean, lambda-driven parallel-for API — eliminating boilerplate thread management for 1D and 2D loop parallelization.

---

## Overview

Parallel programming with raw pthreads is verbose and error-prone. A simple parallel array sum requires roughly **3× more code** than its sequential equivalent — before you even add error handling. SimpleMultithreader collapses that boilerplate into a single function call.

The library exposes a `parallel_for` API that accepts C++11 lambdas, handles work partitioning, thread lifecycle, error recovery, and timing automatically. Users write sequential-style loop bodies and get parallel execution.

```cpp
// Before: ~40 lines of pthread boilerplate
// After:
parallel_for(0, N, [&](int i) {
    C[i] = A[i] + B[i];
}, numThreads);
```

---

## Key Features

- **Header-only** — single `#include`, zero linker configuration
- **Exact thread count** — runtime has precisely `numThreads` threads (main thread participates; `numThreads - 1` workers spawned)
- **1D and 2D parallel_for** — 2D iteration space is flattened for optimal load balancing regardless of loop shape
- **Graceful error recovery** — if `pthread_create` fails, the main thread absorbs that chunk and computation still completes correctly
- **Built-in timing** — each call reports wall-clock execution time via `std::chrono::steady_clock`
- **Sequential fast-path** — `numThreads == 1` skips all thread overhead entirely
- **No thread pool** — fresh threads per call; no hidden shared state

---

## API

### 1D — `parallel_for`

```cpp
void parallel_for(
    int low,
    int high,
    std::function<void(int)> &&lambda,
    int numThreads
);
```

Executes `lambda(i)` for every `i` in `[low, high)` across `numThreads` threads.

```cpp
parallel_for(0, size, [&](int i) {
    C[i] = A[i] + B[i];
}, 4);
```

### 2D — `parallel_for`

```cpp
void parallel_for(
    int low1, int high1,
    int low2, int high2,
    std::function<void(int, int)> &&lambda,
    int numThreads
);
```

Executes `lambda(i, j)` for every `(i, j)` in `[low1, high1) × [low2, high2)`. The 2D space is flattened before distribution, guaranteeing balanced work even when one dimension is much smaller than the other.

```cpp
parallel_for(0, rows, 0, cols, [&](int i, int j) {
    C[i][j] = compute(A, B, i, j);
}, 4);
```

---

## Build & Run

**Requirements:** g++ ≥ 4.8.1 (C++11), GNU Make, POSIX pthreads, Linux/Unix

```bash
make all          # build both examples
make vector       # vector addition demo
make matrix       # matrix multiplication demo
make clean
```

Manual compile:
```bash
g++ -O3 -std=c++11 -o vector vector.cpp -lpthread
g++ -O3 -std=c++11 -o matrix matrix.cpp -lpthread
```

### Running examples

```bash
./vector [numThreads] [arraySize]     # default: 2 threads, 48M elements
./matrix [numThreads] [matrixSize]    # default: 2 threads, 1024×1024

./vector 4 10000000
./matrix 4 512
```

Both programs run assertions on output correctness and print `Test Success` on pass.

---

## Implementation Highlights

### Work Distribution

Static partitioning using ceiling division:

```
chunk = ceil(total_iterations / numThreads)
Thread k → [low + k*chunk, low + (k+1)*chunk)
```

Main thread always takes the first chunk (it's already running; no creation latency). Workers take the rest. Last thread naturally absorbs any remainder.

### 2D Flattening

Rather than parallelizing only the outer loop (which starves threads when the outer dimension is small), both dimensions are collapsed into a single linear index space:

```
total = (high1 - low1) × (high2 - low2)

// Recovery during execution:
i = low1 + (linear_idx / (high2 - low2))
j = low2 + (linear_idx % (high2 - low2))
```

This makes the distribution algorithm identical to the 1D case and ensures even load across all shapes.

### Error Resilience

```
pthread_create fails for thread k
  → log error to stderr
  → execute thread k's chunk in main thread
  → continue creating remaining threads
  → result is always correct
```

`pthread_join` failures are similarly logged but do not abort remaining joins.

---

## Performance Characteristics

| Workload | Observation |
|---|---|
| Large vector addition (48M elements) | Near-linear speedup up to core count |
| Matrix multiplication (1024×1024) | Excellent speedup (high compute-per-element) |
| Small arrays (< 10K elements) | Thread creation overhead dominates; use fewer threads |
| `numThreads == 1` | Zero thread overhead; pure sequential execution |

**Recommendation:** match `numThreads` to physical CPU core count. Hyper-threading gains are workload-dependent.

---

## Known Limitations

- Thread creation per call (no pooling) adds latency for high-frequency small calls
- No automatic thread-count tuning based on problem size or hardware
- Nested `parallel_for` calls will over-subscribe threads; not currently supported
- Memory-bound workloads are limited by bandwidth, not thread count

---

## Project Structure

```
.
├── simple-multithreader.h   # Complete library implementation
├── vector.cpp               # 1D parallel_for demo (vector addition)
├── matrix.cpp               # 1D + 2D parallel_for demo (matrix multiply)
├── Makefile
├── README.md
├── DEMO.md                  # Sample outputs with timing data
└── REQUIREMENTS.md          # Requirement traceability
```

---

## Tech Stack

`C++11` · `POSIX pthreads` · `std::chrono` · `std::function` · `Lambda expressions` · `Linux`

---

## Repository

[github.com/kalkI-pratap17012002/SimpleMultiThreader-Using-Multithreading-with-Ease](https://github.com/kalkI-pratap17012002/SimpleMultiThreader-Using-Multithreading-with-Ease.git)
