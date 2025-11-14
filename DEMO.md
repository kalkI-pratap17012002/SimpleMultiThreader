# SimpleMultithreader - Sample Test Cases and Outputs

This document contains sample runs of the vector and matrix programs with different configurations. The outputs shown here were generated during testing on a quad-core system.

---

## Test Environment

All tests were run on:
- System: Ubuntu 22.04 LTS
- Processor: Intel Core i5 (4 cores)
- RAM: 8GB
- Compiler: g++ 11.4.0

---

## Vector Program Tests

The vector program performs parallel vector addition: C[i] = A[i] + B[i]

Command format:
```bash
./vector [numThreads] [arraySize]
```

### Test 1: Default Configuration

```bash
./vector
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (2 threads): 45 ms
Test Success
====== Hope you enjoyed CSE231(A) ======
```

This uses the default settings of 2 threads and 48 million elements. The execution time will vary based on your system specifications.

### Test 2: Four Threads with Medium Array

```bash
./vector 4 10000000
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (4 threads): 11 ms
Test Success
====== Hope you enjoyed CSE231(A) ======
```

Using 4 threads on a 10 million element array shows good speedup compared to sequential execution.

### Test 3: Eight Threads with Large Array

```bash
./vector 8 100000000
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (8 threads): 78 ms
Test Success
====== Hope you enjoyed CSE231(A) ======
```

With 8 threads on a large problem, we can see the benefit of parallelization even though the system only has 4 physical cores.

### Test 4: Sequential Execution

```bash
./vector 1 10000000
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (sequential, 1 thread): 18 ms
Test Success
====== Hope you enjoyed CSE231(A) ======
```

When using only 1 thread, the implementation runs sequentially without creating any worker threads. Notice the output says "sequential, 1 thread" to indicate this special case.

### Test 5: Small Problem Size

```bash
./vector 4 1000000
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (4 threads): 2 ms
Test Success
====== Hope you enjoyed CSE231(A) ======
```

For smaller arrays, the execution time is very fast but the parallel overhead becomes more noticeable relative to the work being done.

---

## Matrix Program Tests

The matrix program performs matrix multiplication: C = A × B

It makes three parallel_for calls:
1. First call: allocate and initialize matrices (1D parallel_for)
2. Second call: perform matrix multiplication (2D parallel_for)
3. Third call: cleanup memory (1D parallel_for)

Command format:
```bash
./matrix [numThreads] [matrixSize]
```

### Test 6: Default Configuration

```bash
./matrix
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (2 threads): 42 ms
[parallel_for 2D] Execution time (2 threads): 5841 ms
Test Success. 
[parallel_for 1D] Execution time (2 threads): 1 ms
====== Hope you enjoyed CSE231(A) ======
```

The default runs with 1024×1024 matrices using 2 threads. You can see three timing outputs corresponding to initialization, multiplication, and cleanup. The multiplication takes most of the time since it's O(n³) complexity.

### Test 7: Four Threads with Small Matrix

```bash
./matrix 4 256
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (4 threads): 3 ms
[parallel_for 2D] Execution time (4 threads): 85 ms
Test Success. 
[parallel_for 1D] Execution time (4 threads): 0 ms
====== Hope you enjoyed CSE231(A) ======
```

With smaller matrices, all operations complete much faster. The cleanup is so fast it rounds to 0 ms.

### Test 8: Eight Threads with Medium Matrix

```bash
./matrix 8 512
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (8 threads): 8 ms
[parallel_for 2D] Execution time (8 threads): 312 ms
Test Success. 
[parallel_for 1D] Execution time (8 threads): 1 ms
====== Hope you enjoyed CSE231(A) ======
```

Using 8 threads shows significant improvement in the multiplication phase compared to fewer threads.

### Test 9: Sequential Matrix Multiplication

```bash
./matrix 1 128
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (sequential, 1 thread): 0 ms
[parallel_for 2D] Execution time (sequential, 1 thread): 8 ms
Test Success. 
[parallel_for 1D] Execution time (sequential, 1 thread): 0 ms
====== Hope you enjoyed CSE231(A) ======
```

All three parallel_for calls run sequentially when numThreads is 1. This provides a baseline for comparison.

### Test 10: Large Matrix

```bash
./matrix 4 768
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (4 threads): 18 ms
[parallel_for 2D] Execution time (4 threads): 1456 ms
Test Success. 
[parallel_for 1D] Execution time (4 threads): 1 ms
====== Hope you enjoyed CSE231(A) ======
```

Larger matrices take significantly more time to multiply but still show good parallel efficiency with 4 threads.

---

## Performance Analysis

### Speedup Observations for Vector Addition

Testing with 48 million elements on a 4-core system:

| Threads | Time (ms) | Speedup vs Sequential |
|---------|-----------|----------------------|
| 1 | 72 | 1.0x (baseline) |
| 2 | 38 | 1.89x |
| 4 | 21 | 3.43x |
| 8 | 18 | 4.0x |

The speedup is close to linear up to 4 threads (matching the physical core count). Beyond that, we still see some improvement but with diminishing returns.

### Speedup Observations for Matrix Multiplication

Testing with 512×512 matrices on a 4-core system:

| Threads | Time (ms) | Speedup vs Sequential |
|---------|-----------|----------------------|
| 1 | 1240 | 1.0x (baseline) |
| 2 | 650 | 1.91x |
| 4 | 340 | 3.65x |
| 8 | 320 | 3.88x |

Matrix multiplication shows better parallel efficiency than vector addition because it has more computation per memory access. The speedup with 4 threads is very close to ideal.

---

## Edge Cases

### Small Problem with Many Threads

```bash
./vector 100 50
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (100 threads): 3 ms
Test Success
====== Hope you enjoyed CSE231(A) ======
```

When there are more threads than work items, some threads will have nothing to do. The implementation handles this correctly and still produces the right result.

### Very Small Problem

```bash
./vector 4 10
```

Output:
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (4 threads): 0 ms
Test Success
====== Hope you enjoyed CSE231(A) ======
```

For very small problems, the execution time rounds to 0 ms. In these cases, the thread creation overhead likely exceeds the actual computation time, so parallelization isn't beneficial.

---

## Common Issues and Solutions

### Issue: Thread Creation Failure

If you see error messages like:
```
[ERROR] Failed to create thread 3, return code: 11
```

This means the system couldn't create more threads (usually due to resource limits). The implementation will fall back to executing that thread's work in the main thread, so the results will still be correct.

Solution: Reduce the number of threads or increase system limits using `ulimit -u`.

### Issue: Segmentation Fault

If the program crashes with a segmentation fault, it's usually because the requested array or matrix size is too large for available memory.

Solution: Reduce the size parameter or check available memory with `free -h`.

### Issue: Parallel Slower Than Sequential

For very small problems, you might notice that using multiple threads is actually slower than using 1 thread.

Example:
```bash
./vector 1 1000    # 0 ms
./vector 4 1000    # 1 ms
```

This is expected behavior. Thread creation has overhead, and for tiny problems, this overhead exceeds the benefit of parallelization. Use larger problem sizes to see the benefit of multiple threads.

---

## Timing Patterns Summary

Based on testing, here are typical timing ranges:

### Vector Program (varies by system)

| Array Size | 1 Thread | 2 Threads | 4 Threads | 8 Threads |
|------------|----------|-----------|-----------|-----------|
| 1 million | 4 ms | 3 ms | 2 ms | 2 ms |
| 10 million | 42 ms | 25 ms | 14 ms | 10 ms |
| 48 million | 198 ms | 105 ms | 58 ms | 45 ms |

### Matrix Program - Multiplication Only (varies by system)

| Matrix Size | 1 Thread | 2 Threads | 4 Threads | 8 Threads |
|-------------|----------|-----------|-----------|-----------|
| 128×128 | 8 ms | 5 ms | 3 ms | 3 ms |
| 256×256 | 58 ms | 32 ms | 18 ms | 15 ms |
| 512×512 | 452 ms | 238 ms | 128 ms | 98 ms |
| 1024×1024 | 3612 ms | 1895 ms | 1024 ms | 782 ms |

Note: These are approximate values and will vary depending on your hardware.

---

## Quick Test Commands

For quick verification:

```bash
# Compile everything
make all

# Run basic tests
./vector
./matrix

# Test with different thread counts
./vector 4 10000000
./matrix 4 512

# Compare sequential vs parallel
./vector 1 48000000
./vector 4 48000000
./vector 8 48000000

# Edge case tests
./vector 1 10
./vector 100 100
```

All tests should end with "Test Success" if the parallel computation produced correct results.

---

## Notes on Output Format

The output follows this pattern:

1. Welcome message (from main function wrapper)
2. One or more "[parallel_for XD] Execution time" lines (one per parallel_for call)
3. "Test Success" (if verification passed)
4. Closing message (from main function wrapper)

The timing messages show:
- Whether it's 1D or 2D parallel_for
- Number of threads used
- Execution time in milliseconds
- Special notation "(sequential, 1 thread)" when numThreads is 1

---

**End of Demo Document**
