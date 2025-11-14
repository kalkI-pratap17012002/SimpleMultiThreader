# SimpleMultithreader - Design Document

**Course:** CSE 231 Operating Systems (Section A)  
**Assignment:** Assignment 5  
**Submission Date:** November 22, 2025

**Group Information:**  
Group ID: 13  
Member: Ayush Kumar (Roll No: 2020290)

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Design Overview](#2-design-overview)
3. [Implementation Architecture](#3-implementation-architecture)
4. [Key Design Decisions](#4-key-design-decisions)
5. [Thread Management Strategy](#5-thread-management-strategy)
6. [Work Distribution Algorithm](#6-work-distribution-algorithm)
7. [Error Handling Approach](#7-error-handling-approach)
8. [Performance Considerations](#8-performance-considerations)
9. [Testing and Validation](#9-testing-and-validation)
10. [Individual Contribution](#10-individual-contribution)
11. [Challenges Faced](#11-challenges-faced)
12. [Future Improvements](#12-future-improvements)
13. [Conclusion](#13-conclusion)

---

## 1. Introduction

This document describes the design and implementation of SimpleMultithreader, a header-only C++ library that simplifies parallel programming using POSIX threads. The primary objective was to create an abstraction layer that allows programmers to parallelize their loops without dealing with the low-level complexities of pthread management.

The motivation behind this work stems from observing how much boilerplate code is typically required for basic parallel operations. As discussed in Lecture 21, even simple tasks like parallel array summation require roughly three times more code compared to sequential implementations. SimpleMultithreader addresses this by providing a clean interface that accepts C++11 lambda expressions, making parallelization almost as simple as writing sequential code.

---

## 2. Design Overview

### 2.1 Core Philosophy

The design philosophy centers around three main principles:

1. **Simplicity**: Users shouldn't need to understand pthread mechanics
2. **Transparency**: Performance characteristics should be visible through timing outputs
3. **Correctness**: The library must handle edge cases gracefully without sacrificing result accuracy

### 2.2 Architecture Components

The library consists of four main components:

- **Thread Data Structures**: Encapsulate work assignments for threads
- **Worker Functions**: Execute the actual parallel work
- **parallel_for APIs**: Public interface for 1D and 2D parallelization
- **Error Handling Layer**: Ensures robustness in failure scenarios

---

## 3. Implementation Architecture

### 3.1 Header-Only Design

I chose a header-only implementation because it simplifies integration for users. They just include one file and start using the library without worrying about linking additional object files or libraries. This design also allows the compiler to inline functions more aggressively, potentially improving performance.

### 3.2 Data Structure Design

#### ThreadWork1D Structure

```cpp
struct ThreadWork1D {
    int start;
    int end;
    std::function<void(int)>* lambda;
};
```

This structure was designed to be minimal yet complete. Each thread only needs to know:
- Where to start (`start`)
- Where to stop (`end`)
- What to execute (`lambda`)

I initially considered passing the lambda by value, but using a pointer proved more efficient since we're sharing the same lambda across all threads.

#### ThreadWork2D Structure

```cpp
struct ThreadWork2D {
    int start;
    int end;
    int low1, high1;
    int low2, high2;
    std::function<void(int, int)>* lambda;
};
```

For 2D loops, I needed additional information to convert linear indices back to 2D coordinates. The `low1, high1, low2, high2` fields preserve the original loop bounds, which is necessary for the coordinate conversion logic.

---

## 4. Key Design Decisions

### 4.1 Thread Count Philosophy

One critical requirement was that `numThreads` must include the main thread. This means if a user specifies 4 threads, we create only 3 worker threads via `pthread_create`, and the main thread participates in doing work as well.

Initially, I considered creating exactly `numThreads` workers, but that would result in `numThreads + 1` total threads (including main), which violates the specification. The current approach ensures precise control over the total thread count.

### 4.2 Sequential Execution Optimization

When `numThreads` is 1, the code takes a completely different path that executes sequentially without any thread creation overhead. This optimization is important because:

1. It avoids unnecessary thread creation costs for sequential execution
2. It makes performance comparisons fair
3. It handles edge cases where users might want to disable parallelism temporarily

### 4.3 2D Loop Flattening Strategy

For 2D loops, I considered two approaches:

**Option A**: Parallelize only the outer loop  
**Option B**: Flatten both dimensions into a linear space

I chose Option B because it provides better load balancing. If the outer loop has few iterations but the inner loop has many, Option A would create idle threads. Flattening ensures work is distributed evenly regardless of the loop structure.

The conversion formula I use is:

```
linear_index = (i - low1) * (high2 - low2) + (j - low2)
i = low1 + (linear_index / inner_size)
j = low2 + (linear_index % inner_size)
```

This took some time to derive correctly, especially handling the offset from `low1` and `low2` properly.

---

## 5. Thread Management Strategy

### 5.1 Thread Creation Flow

The thread creation process follows these steps:

1. Calculate total iterations needed
2. Divide iterations among threads using ceiling division
3. Create `numThreads - 1` worker threads
4. Assign work chunks to each thread
5. Main thread executes its portion immediately
6. Wait for all workers to complete using `pthread_join`

### 5.2 Work Assignment

I use a simple static partitioning scheme where:

```cpp
iterations_per_thread = ceil(total_iterations / numThreads)
```

Each thread gets a consecutive range. The last thread might get fewer iterations if the total doesn't divide evenly, but this is acceptable since the imbalance is minimal.

I considered dynamic work stealing but decided against it because:

- It would require shared data structures and locks
- The added complexity doesn't justify the benefits for well-balanced workloads
- Static partitioning is more predictable for performance analysis

---

## 6. Work Distribution Algorithm

### 6.1 1D Distribution

For 1D loops, the distribution is straightforward:

```
Thread 0 (main): [low, low + chunk_size)
Thread 1: [low + chunk_size, low + 2*chunk_size)
...
Thread n-1: [low + (n-1)*chunk_size, high)
```

The main thread always takes the first chunk. This design choice avoids having the main thread idle while workers are being created.

### 6.2 2D Distribution

For 2D loops, I flatten the iteration space first:

```cpp
total_iterations = (high1 - low1) * (high2 - low2)
```

Then distribute linearly, and each thread converts its linear indices back to (i,j) pairs during execution. This approach is elegant because the distribution code remains identical to the 1D case.

---

## 7. Error Handling Approach

### 7.1 pthread_create Failures

When `pthread_create` fails, I don't just abort. Instead:

1. Log an error message indicating which thread failed
2. Mark that thread as unsuccessful in the `thread_success` array
3. Execute that thread's work in the main thread as fallback
4. Continue with other thread creations

This ensures the computation completes correctly even under resource constraints. During testing on a resource-limited system, this fallback mechanism saved several runs from crashing.

### 7.2 pthread_join Failures

Similarly, if `pthread_join` fails, I log the error but continue joining other threads. This prevents one bad join from blocking the entire cleanup process.

### 7.3 Empty Iteration Ranges

If `high <= low` for 1D loops or if either dimension is empty for 2D loops, the code detects this early and returns immediately with a 0 ms execution time. This prevents unnecessary overhead for degenerate cases.

---

## 8. Performance Considerations

### 8.1 Timing Measurement

I use `std::chrono::steady_clock` for timing because it's monotonic and not affected by system clock adjustments. The timing includes:

- Thread creation overhead
- Actual computation time
- Thread joining overhead

This gives users a realistic view of the total parallel execution cost.

### 8.2 Memory Allocation

I allocate thread-related arrays on the heap using `new` rather than stack allocation because:

1. The number of threads is determined at runtime
2. Heap allocation is more flexible for varying thread counts
3. Modern allocators are quite efficient for small allocations

I ensure proper cleanup using `delete[]` in all code paths.

### 8.3 Cache Considerations

The consecutive work assignment helps with cache performance since each thread operates on a contiguous memory region. For the matrix example, this means better spatial locality compared to random work distribution.

---

## 9. Testing and Validation

### 9.1 Test Strategy

I tested the implementation using:

1. **Provided examples**: vector.cpp and matrix.cpp
2. **Edge cases**: Single thread, more threads than work items, empty ranges
3. **Stress tests**: Very large arrays to check memory limits
4. **Error injection**: Limiting system resources to trigger pthread_create failures

### 9.2 Correctness Verification

Both example programs include assertions that verify:

- Vector addition: All elements equal 2 (since 1+1=2)
- Matrix multiplication: All elements equal matrix size (due to the specific initialization pattern)

These assertions caught several bugs during development, particularly in the 2D coordinate conversion logic.

### 9.3 Performance Validation

I ran benchmarks with varying thread counts and problem sizes to verify:

- Speedup increases with more threads up to the core count
- Very small problems show thread creation overhead
- Large problems achieve near-linear speedup

The results matched expectations and aligned with the timing data in DEMO.md.

---

## 10. Individual Contribution

Since this is an individual submission (Group 13, single member), I was responsible for all aspects:

### Design Phase (2 hours)

- Studied the assignment requirements thoroughly
- Reviewed lecture notes on pthread programming
- Sketched out the architecture and data structures

### Implementation Phase (6 hours)

- Implemented the 1D parallel_for with basic functionality
- Added error handling and timing measurement
- Implemented the 2D version with coordinate conversion
- Debugged the index calculation (this was the trickiest part)

### Testing Phase (3 hours)

- Tested with provided examples
- Ran performance benchmarks
- Tested edge cases and error scenarios
- Fixed issues found during testing

### Documentation Phase (2 hours)

- Wrote inline code comments
- Prepared README.md
- Created DEMO.md with sample outputs
- Wrote this design document

**Total effort:** Approximately 13 hours over 3 days.

---

## 11. Challenges Faced

### 11.1 2D Coordinate Conversion

The biggest challenge was getting the 2D to linear index conversion correct. My initial formula didn't account for the `low1` and `low2` offsets properly, which caused incorrect results when loops didn't start from 0.

I spent about an hour debugging this by adding print statements to trace the (i,j) values being generated. Once I realized the issue was with the offset, the fix was straightforward but it was frustrating to track down initially.

### 11.2 Thread Creation Failures

During stress testing with 100+ threads, I encountered pthread_create failures. This forced me to implement the fallback mechanism. Initially, I thought about just aborting, but ensuring correctness seemed more important than failing fast.

### 11.3 Timing Measurement Placement

I experimented with where to place the timing code. Initially, I timed just the computation without thread creation, but that didn't represent the true cost of using parallel_for. Including everything gives users a more honest picture of performance.

---

## 12. Future Improvements

While the current implementation meets all requirements, several enhancements could improve it:

### 12.1 Dynamic Work Scheduling

Implementing work stealing or dynamic scheduling could help with load imbalance when loop iterations have varying costs. This would require:

- A shared work queue protected by mutexes
- Threads pulling work dynamically rather than static assignment
- More complex implementation but potentially better performance

### 12.2 Automatic Thread Count Selection

The library could automatically choose the thread count based on:

- Number of CPU cores (via `std::thread::hardware_concurrency()`)
- Problem size
- Historical performance data

This would make it even easier for users who don't want to manually tune thread counts.

### 12.3 Nested Parallelism Support

Currently, calling parallel_for from within a parallel lambda would create excessive threads. Supporting nested parallelism safely would require:

- Tracking the current nesting level
- Limiting total thread count across all levels
- More sophisticated work distribution

### 12.4 Better Error Reporting

Instead of just printing to stderr, the library could:

- Return error codes or throw exceptions
- Provide more detailed diagnostic information
- Allow users to register error callbacks

---

## 13. Conclusion

This assignment provided valuable hands-on experience with pthread programming and API design. The SimpleMultithreader library successfully abstracts away the complexity of thread management while maintaining transparency about performance characteristics.

### Requirements Satisfied

The implementation satisfies all requirements:

- ✓ Header-only design
- ✓ Supports both 1D and 2D parallel_for
- ✓ Exactly `numThreads` threads including main thread
- ✓ No thread pool, fresh threads per call
- ✓ Proper error handling
- ✓ Timing measurement for each call
- ✓ Works with provided examples without modification
- ✓ Modular code with minimal repetition
- ✓ Comprehensive documentation

### Key Takeaways

The experience of debugging the 2D coordinate conversion and handling thread creation failures taught me the importance of robust error handling and thorough testing. I'm satisfied with the final design and believe it achieves a good balance between simplicity and functionality.

The library demonstrates that with careful design, complex parallel programming patterns can be made accessible to programmers who may not be experts in concurrent programming, while still maintaining performance and correctness.

---

**End of Design Document**
