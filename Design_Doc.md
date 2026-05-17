# SimpleMultithreader — Technical Design Document

**Project:** Header-only C++ parallel-for library over POSIX pthreads  
**Author:** Ayush Kumar  
**Stack:** C++11 · POSIX pthreads · Linux · std::chrono · Lambda expressions

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [Design Goals](#2-design-goals)
3. [Architecture](#3-architecture)
4. [Core Data Structures](#4-core-data-structures)
5. [Thread Management](#5-thread-management)
6. [Work Distribution Algorithm](#6-work-distribution-algorithm)
7. [2D Iteration Space Flattening](#7-2d-iteration-space-flattening)
8. [Error Handling & Resilience](#8-error-handling--resilience)
9. [Performance Design](#9-performance-design)
10. [Testing Strategy](#10-testing-strategy)
11. [Design Trade-offs](#11-design-tradeoffs)
12. [Future Improvements](#12-future-improvements)

---

## 1. Problem Statement

Parallelizing a loop with raw POSIX pthreads requires writing approximately **3× more code** than the sequential equivalent — even for trivial operations. For every parallel task a developer must:

- Define a struct to pass arguments through the `void*` interface
- Write a worker function that unpacks arguments and iterates its range
- Manually compute work partitions (and handle remainders correctly)
- Call `pthread_create` and `pthread_join` with proper error checking
- Handle thread creation failures without corrupting the result

This friction discourages parallelism in practice. SimpleMultithreader eliminates it entirely behind a two-function API that accepts C++11 lambdas.

---

## 2. Design Goals

| Priority | Goal | Rationale |
|---|---|---|
| P0 | **Correctness** | Result must be identical to sequential execution regardless of thread count or failure conditions |
| P0 | **Exact thread count** | `numThreads` includes the main thread; no over- or under-subscription |
| P1 | **Zero setup cost for users** | Header-only; single `#include`; no linker flags beyond `-lpthread` |
| P1 | **Transparent performance** | Execution time printed per call so users can measure parallelism benefit |
| P2 | **Resilience** | `pthread_create` failure must not corrupt or abort the computation |
| P2 | **Sequential efficiency** | `numThreads == 1` must have zero threading overhead |

---

## 3. Architecture

```
User Code
    │
    │  parallel_for(low, high, lambda, numThreads)
    ▼
┌─────────────────────────────────────┐
│          parallel_for API           │  ← public interface (header-only)
│  - validates range                  │
│  - starts chrono timer              │
│  - fast-path: numThreads == 1       │
└─────────────┬───────────────────────┘
              │
              ▼
┌─────────────────────────────────────┐
│       Work Partitioner              │
│  - ceiling division                 │
│  - assigns [start, end) per thread  │
│  - main thread takes chunk 0        │
└──────┬──────────────────────────────┘
       │
       ├──► pthread_create × (numThreads - 1)
       │         │
       │         ▼
       │    ┌─────────────┐
       │    │ worker_1D / │  ← unpack ThreadWork, iterate range, call lambda
       │    │ worker_2D   │
       │    └─────────────┘
       │
       ├──► main thread executes its own chunk inline
       │
       └──► pthread_join × (numThreads - 1)
                 │
                 ▼
            chrono stop → print elapsed ms
```

The design keeps a strict layering: the public API owns lifecycle and timing, the partitioner owns math, the worker functions own execution.

---

## 4. Core Data Structures

### `ThreadWork1D`

```cpp
struct ThreadWork1D {
    int start;
    int end;
    std::function<void(int)>* lambda;
};
```

Minimal: a thread only needs its iteration range and a pointer to the shared lambda. Passing by pointer (not value) avoids copying the lambda's captured closure for every thread.

### `ThreadWork2D`

```cpp
struct ThreadWork2D {
    int start;          // linear index range assigned to this thread
    int end;
    int low1, high1;    // original outer loop bounds
    int low2, high2;    // original inner loop bounds
    std::function<void(int, int)>* lambda;
};
```

The original bounds are preserved so each thread can recover `(i, j)` from its linear index without an additional shared data structure.

---

## 5. Thread Management

### Lifecycle

```
parallel_for called
  │
  ├── numThreads == 1 ?  →  execute sequentially, return
  │
  ├── Allocate: pthread_t[], ThreadWork[], bool thread_success[]
  │
  ├── For k in [1, numThreads):
  │     ThreadWork[k] = {chunk_start_k, chunk_end_k, &lambda}
  │     pthread_create(&tid[k], NULL, worker, &ThreadWork[k])
  │       └── on failure: thread_success[k] = false
  │
  ├── Main thread executes ThreadWork[0] inline (chunk 0)
  │
  ├── For k in [1, numThreads):
  │     if thread_success[k]: pthread_join(tid[k], NULL)
  │     else: execute ThreadWork[k] inline (fallback)
  │
  └── delete[], print elapsed time, return
```

The main thread is never idle during the worker phase — it always executes chunk 0 immediately after spawning workers. This avoids the anti-pattern where the main thread waits while doing no useful work.

### Thread Count Invariant

If the user requests `N` threads, exactly `N` threads participate in computation: 1 main + `(N-1)` workers. This is enforced structurally — worker creation is bounded by `numThreads - 1`.

---

## 6. Work Distribution Algorithm

Static partitioning with ceiling division:

```
chunk_size = ceil(total_iterations / numThreads)

Thread k gets: [low + k * chunk_size,  min(low + (k+1) * chunk_size, high))
```

This guarantees:
- Every iteration in `[low, high)` is assigned to exactly one thread
- No thread gets more than `chunk_size` iterations
- The last thread naturally handles the remainder (it may get fewer, never more)
- Imbalance is at most `numThreads - 1` iterations — negligible at any practical scale

**Why static over dynamic (work-stealing)?**  
Dynamic scheduling requires a shared queue, mutexes, and atomic operations. For the general case of uniform-cost iterations (array ops, matrix multiply), static partitioning achieves equivalent balance without synchronization overhead. Dynamic scheduling is a future improvement for irregular workloads.

---

## 7. 2D Iteration Space Flattening

### The Problem with Outer-Loop-Only Parallelism

If a 2D loop has shape `(4 outer) × (1,000,000 inner)` and we parallelize only the outer loop with 8 threads, 4 threads are idle. Load balance depends entirely on the outer dimension.

### Flattening Solution

Both dimensions are collapsed into a single linear index space before distribution:

```
total = (high1 - low1) × (high2 - low2)
```

Distribution is then identical to the 1D case. Each thread recovers `(i, j)` during execution:

```
inner_size = high2 - low2

i = low1 + (linear_idx / inner_size)
j = low2 + (linear_idx % inner_size)
```

The `low1`/`low2` offsets are critical — loops don't always start at 0. The formula was derived and validated against loops with arbitrary start indices.

**Result:** load is balanced regardless of loop shape. A `(2 outer) × (1,000,000 inner)` loop distributes evenly across 8 threads instead of leaving 6 idle.

---

## 8. Error Handling & Resilience

### `pthread_create` Failure

```cpp
int rc = pthread_create(&tid[k], NULL, worker_1D, &work[k]);
if (rc != 0) {
    fprintf(stderr, "pthread_create failed for thread %d: %s\n", k, strerror(rc));
    thread_success[k] = false;
    // chunk k will be executed by main thread during join phase
}
```

The fallback is a semantic guarantee: **computation always completes correctly**, even under resource exhaustion. During stress testing with 100+ threads, this mechanism converted would-be crashes into successful (slower) runs.

### `pthread_join` Failure

Join failures are logged to stderr. The loop continues joining remaining threads — one bad join does not block cleanup of all others.

### Empty Range Guard

```cpp
if (high <= low) { /* print 0 ms, return immediately */ }
```

Prevents division-by-zero and unnecessary allocations for degenerate inputs.

---

## 9. Performance Design

### Timing Scope

`std::chrono::steady_clock` wraps the entire call including thread creation and joining. This is intentional: it gives users a truthful view of the actual cost of parallelization, not just the computation portion. Monotonic clock avoids system-time adjustments distorting measurements.

### Memory

Thread arrays are heap-allocated (size known only at runtime). All allocations are paired with `delete[]` on every exit path. No memory leaks under failure conditions.

### Cache Locality

Consecutive static partitioning means each thread operates on a contiguous memory region. For array and matrix workloads this maximizes spatial locality and minimizes false sharing between thread chunks.

---

## 10. Testing Strategy

### Correctness Tests

Both example programs include assertions that verify exact output values:

- **vector.cpp** — all elements of `C` must equal `2` (A[i]=1, B[i]=1, C[i]=A[i]+B[i])
- **matrix.cpp** — all elements of result matrix must equal `N` (matrix size), due to structured initialization

These assertions caught real bugs during development, particularly in the 2D coordinate recovery formula with non-zero lower bounds.

### Edge Cases Exercised

| Case | Expected Behavior |
|---|---|
| `numThreads == 1` | Sequential fast-path, zero thread overhead |
| `numThreads > total_iterations` | Some threads get empty ranges; handled gracefully |
| `high <= low` | Immediate return, 0ms printed |
| `pthread_create` forced failure | Fallback execution; correct result |
| 2D loop with `low1, low2 != 0` | Correct `(i,j)` recovery verified against assertions |

### Performance Benchmarks

Benchmarks across thread counts and problem sizes confirmed:
- Speedup scales with thread count up to physical core count
- Thread creation overhead dominates for small problems (< ~100K iterations)
- Near-linear speedup for large arrays and matrix workloads
- Results consistent with theoretical analysis

---

## 11. Design Trade-offs

| Decision | Chosen | Rejected | Reason |
|---|---|---|---|
| Thread creation | Fresh per call | Thread pool | Simpler; no hidden shared state; correct for assignment spec |
| Work scheduling | Static partition | Dynamic/work-stealing | Sufficient for uniform workloads; avoids mutex overhead |
| 2D parallelism | Flatten both dims | Outer loop only | Better load balance for asymmetric loops |
| Lambda passing | Pointer to shared | Copy per thread | Avoids closure copy overhead |
| Timing scope | Full call (incl. create/join) | Compute only | Honest representation of total parallel cost |
| Failure mode | Fallback to main | Abort | Correctness over fail-fast |

---

## 12. Future Improvements

**Dynamic Work Scheduling**  
A shared atomic work counter would let threads self-assign iterations, improving balance for irregular workloads (variable-cost iterations). Trade-off: requires atomic operations or mutex, adds implementation complexity.

**Automatic Thread Count**  
`std::thread::hardware_concurrency()` could provide a sensible default, with optional override. Useful for users who don't want to tune manually.

**Nested Parallelism Guard**  
Detecting when `parallel_for` is called from within a parallel lambda and suppressing inner thread creation (running sequentially instead) would prevent thread over-subscription.

**Exception Propagation**  
Lambda exceptions currently terminate the worker thread silently. A future version could capture exceptions via `std::exception_ptr` and rethrow from the main thread after joining.

---

## Requirements Coverage

| Requirement | Status |
|---|---|
| Header-only implementation | ✅ |
| 1D `parallel_for` with correct signature | ✅ |
| 2D `parallel_for` with correct signature | ✅ |
| Exactly `numThreads` threads including main | ✅ |
| No thread pool — fresh threads per call | ✅ |
| Proper `pthread_create` / `pthread_join` error handling | ✅ |
| Execution time printed per call | ✅ |
| Works with provided examples unmodified | ✅ |
| Modular code with minimal repetition | ✅ |
| C++11 lambda support | ✅ |
| POSIX pthread APIs only (no `std::thread`) | ✅ |

---

*SimpleMultithreader demonstrates that careful API design can make complex parallel programming patterns accessible without sacrificing performance, correctness, or observability.*
