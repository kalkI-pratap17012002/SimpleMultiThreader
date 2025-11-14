# SimpleMultithreader: Easy Parallel Loop Execution with Pthreads

A header-only C++ library that simplifies parallel loop execution using POSIX Pthreads and C++11 lambda expressions.

---

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [System Requirements](#system-requirements)
- [Project Structure](#project-structure)
- [Installation](#installation)
- [Compilation](#compilation)
- [Usage](#usage)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Performance](#performance)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)
- [Assignment Details](#assignment-details)

---

## Overview

**SimpleMultithreader** is a lightweight, header-only library designed for CSE 231 Operating Systems (Assignment-5). It provides an easy-to-use interface for parallelizing loops using POSIX Pthreads, reducing the complexity typically associated with multithreaded programming.

Instead of manually creating threads, distributing work, and managing synchronization, you can simply call `parallel_for` with a lambda function, and the library handles all the threading details automatically.

### Before SimpleMultithreader:
```cpp
// Manually manage threads, distribute work, synchronize...
// ~60+ lines of pthread code
```

### After SimpleMultithreader:
```cpp
// Just one line!
parallel_for(0, 1000, [&](int i) { array[i] = i * 2; }, 4);
```

---

## Features

✅ **Header-Only**: No compilation or linking required, just `#include "simple-multithreader.h"`

✅ **Easy-to-Use**: Parallelize loops with a single function call

✅ **1D and 2D Support**: Handle both single and nested loops

✅ **C++11 Lambda Support**: Use modern lambda expressions for loop bodies

✅ **Automatic Work Distribution**: Evenly distributes iterations across threads

✅ **Performance Timing**: Automatically prints execution time for each parallel loop

✅ **Error Handling**: Built-in error checking for all pthread operations

✅ **No Thread Pool**: Creates fresh threads for each call (educational design)

✅ **POSIX Pthreads**: Uses standard POSIX threads (portable across Linux/Unix)

---

## System Requirements

### Hardware
- Linux or Unix-based operating system (Ubuntu, Fedora, WSL, etc.)
- **Note**: macOS is NOT recommended for OS assignments
- Multi-core processor (recommended for observing parallel speedup)

### Software
- **C++ Compiler**: g++ with C++11 support (g++ 4.8.1 or later)
- **Build Tool**: GNU Make
- **Pthread Library**: POSIX threads (usually included in standard Linux distributions)

### Verification
```bash
# Check g++ version (should be 4.8.1+)
g++ --version

# Check for pthread support
ls /usr/lib/x86_64-linux-gnu/libpthread.so
# or
ls /usr/lib/libpthread.so

# Check for GNU Make
make --version
```

---

## Project Structure

```
SimpleMultithreader/
├── simple-multithreader.h    # Main header file (implementation)
├── vector.cpp                # Example: 1D vector addition
├── matrix.cpp                # Example: 2D matrix multiplication
├── Makefile                  # Build configuration
├── README.md                 # This file
├── REQUIREMENTS.md           # Requirement-to-implementation mapping
└── DEMO.md                   # Sample inputs and outputs
```

### File Descriptions

- **`simple-multithreader.h`**: The core library implementation
  - Thread data structures (ThreadWork1D, ThreadWork2D)
  - Worker functions (worker_1d, worker_2d)
  - parallel_for APIs (1D and 2D versions)
  - ~590 lines with extensive documentation

- **`vector.cpp`**: Demonstrates 1D parallel_for
  - Vector addition: C[i] = A[i] + B[i]
  - Default: 48 million elements
  - Configurable thread count and array size

- **`matrix.cpp`**: Demonstrates both 1D and 2D parallel_for
  - Matrix initialization (1D parallel_for)
  - Matrix multiplication (2D parallel_for)
  - Matrix cleanup (1D parallel_for)
  - Default: 1024×1024 matrices

- **`Makefile`**: Build automation
  - Compiles both examples
  - Flags: `-O3 -std=c++11 -lpthread`
  - Targets: `all`, `clean`, individual programs

---

## Installation

### Option 1: Direct Download
1. Clone or download the project files
2. Ensure all files are in the same directory
3. No installation needed (header-only library)

### Option 2: Quick Start
```bash
# Navigate to project directory
cd /path/to/SimpleMultithreader

# Verify files are present
ls simple-multithreader.h vector.cpp matrix.cpp Makefile

# Compile
make all

# Run tests
./vector 4
./matrix 4
```

---

## Compilation

### Using Make (Recommended)
```bash
# Clean previous builds
make clean

# Compile all programs
make all

# Compile specific program
make vector
make matrix
```

### Manual Compilation
```bash
# Compile vector program
g++ -O3 -std=c++11 -o vector vector.cpp -lpthread

# Compile matrix program
g++ -O3 -std=c++11 -o matrix matrix.cpp -lpthread
```

### Compilation Flags Explained
- `-O3`: Maximum optimization level (for performance)
- `-std=c++11`: Enable C++11 features (required for lambda support)
- `-lpthread`: Link with POSIX threads library
- `-o <name>`: Output executable name

### Common Compilation Issues

**Issue: "pthread.h: No such file or directory"**
```bash
# Solution: Install pthread development package
sudo apt-get install libc6-dev  # Ubuntu/Debian
sudo yum install glibc-devel    # Fedora/RHEL
```

**Issue: "error: 'function' is not a member of 'std'"**
```bash
# Solution: Ensure -std=c++11 flag is used
g++ -std=c++11 vector.cpp -lpthread
```

---

## Usage

### Basic Usage Pattern

1. **Include the header**:
   ```cpp
   #include "simple-multithreader.h"
   ```

2. **Call parallel_for** with your lambda:
   ```cpp
   parallel_for(start, end, [&](int i) {
       // Your loop body here
   }, numThreads);
   ```

3. **Compile with pthread**:
   ```bash
   g++ -std=c++11 your_program.cpp -lpthread
   ```

### Complete Example
```cpp
#include "simple-multithreader.h"
#include <iostream>

int main() {
    const int SIZE = 1000;
    int* data = new int[SIZE];
    
    // Initialize array in parallel
    parallel_for(0, SIZE, [=](int i) {
        data[i] = i * i;  // Calculate squares
    }, 4);  // Use 4 threads
    
    // Verify first few elements
    for (int i = 0; i < 5; i++) {
        std::cout << "data[" << i << "] = " << data[i] << "\n";
    }
    
    delete[] data;
    return 0;
}
```

---

## API Reference

### Function: `parallel_for` (1D version)

**Signature:**
```cpp
void parallel_for(int low, int high, 
                  std::function<void(int)> &&lambda, 
                  int numThreads);
```

**Parameters:**
- `low` (int): Starting index (inclusive)
- `high` (int): Ending index (exclusive) - loop runs for [low, high)
- `lambda` (std::function<void(int)>): Function to execute for each index
  - Takes one parameter: the current loop index `i`
  - Can capture variables: `[&]` (by reference) or `[=]` (by value)
- `numThreads` (int): Total number of threads INCLUDING main thread
  - Example: `numThreads=4` means 1 main + 3 workers = 4 total

**Behavior:**
- Executes `lambda(i)` for each `i` in range [low, high)
- Distributes work evenly across `numThreads` threads
- Main thread participates in execution
- Prints execution time in milliseconds
- Blocks until all iterations complete

**Example:**
```cpp
// Sequential equivalent: for(int i = 0; i < 1000; i++) array[i] = 0;
parallel_for(0, 1000, [&](int i) {
    array[i] = 0;
}, 8);  // Use 8 threads
```

---

### Function: `parallel_for` (2D version)

**Signature:**
```cpp
void parallel_for(int low1, int high1, int low2, int high2,
                  std::function<void(int, int)> &&lambda, 
                  int numThreads);
```

**Parameters:**
- `low1, high1` (int): Outer loop bounds (i dimension)
- `low2, high2` (int): Inner loop bounds (j dimension)
- `lambda` (std::function<void(int, int)>): Function taking (i, j) parameters
- `numThreads` (int): Total number of threads INCLUDING main thread

**Behavior:**
- Executes `lambda(i, j)` for all (i, j) pairs
- i ranges from [low1, high1), j ranges from [low2, high2)
- Flattens 2D iteration space for even work distribution
- Prints execution time in milliseconds

**Example:**
```cpp
// Sequential equivalent:
// for(int i = 0; i < 100; i++)
//     for(int j = 0; j < 100; j++)
//         matrix[i][j] = i + j;

parallel_for(0, 100, 0, 100, [&](int i, int j) {
    matrix[i][j] = i + j;
}, 4);  // Use 4 threads
```

---

## Examples

### Example 1: Vector Addition (1D)

```cpp
#include "simple-multithreader.h"
#include <assert.h>

int main(int argc, char** argv) {
    int numThread = argc>1 ? atoi(argv[1]) : 2;
    int size = argc>2 ? atoi(argv[2]) : 48000000;  
    
    // Allocate vectors
    int* A = new int[size];
    int* B = new int[size];
    int* C = new int[size];
    
    // Initialize
    std::fill(A, A+size, 1);
    std::fill(B, B+size, 1);
    std::fill(C, C+size, 0);
    
    // Parallel addition
    parallel_for(0, size, [&](int i) {
        C[i] = A[i] + B[i];
    }, numThread);
    
    // Verify
    for(int i=0; i<size; i++) assert(C[i] == 2);
    printf("Test Success\n");
    
    delete[] A; delete[] B; delete[] C;
    return 0;
}
```

**Run:**
```bash
./vector 4 10000000  # 4 threads, 10M elements
```

---

### Example 2: Matrix Multiplication (2D)

```cpp
#include "simple-multithreader.h"
#include <assert.h>

int main(int argc, char** argv) {
    int numThread = argc>1 ? atoi(argv[1]) : 2;
    int size = argc>2 ? atoi(argv[2]) : 1024;
    
    // Allocate matrices
    int** A = new int*[size];
    int** B = new int*[size];
    int** C = new int*[size];
    
    // Initialize in parallel (1D parallel_for)
    parallel_for(0, size, [=](int i) {
        A[i] = new int[size];
        B[i] = new int[size];
        C[i] = new int[size];
        std::fill(A[i], A[i]+size, 1);
        std::fill(B[i], B[i]+size, 1);
        std::fill(C[i], C[i]+size, 0);
    }, numThread);
    
    // Matrix multiplication (2D parallel_for)
    parallel_for(0, size, 0, size, [&](int i, int j) {
        for(int k=0; k<size; k++) {
            C[i][j] += A[i][k] * B[k][j];
        }
    }, numThread);
    
    // Verify
    for(int i=0; i<size; i++)
        for(int j=0; j<size; j++)
            assert(C[i][j] == size);
    printf("Test Success\n");
    
    // Cleanup
    parallel_for(0, size, [=](int i) {
        delete [] A[i]; delete [] B[i]; delete [] C[i];
    }, numThread);
    delete[] A; delete[] B; delete[] C;
    
    return 0;
}
```

**Run:**
```bash
./matrix 4 512  # 4 threads, 512×512 matrix
```

---

## Performance

### Expected Speedup

The library provides near-linear speedup for compute-intensive tasks:

| Threads | Expected Speedup | Best For |
|---------|------------------|----------|
| 1 | 1.0x (baseline) | Sequential |
| 2 | ~1.8x - 2.0x | Dual-core |
| 4 | ~3.5x - 4.0x | Quad-core |
| 8 | ~6.5x - 8.0x | 8-core |

**Note**: Actual speedup depends on:
- Problem size (larger = better parallelization)
- Workload per iteration (more computation = better speedup)
- Memory bandwidth (can become bottleneck)
- System load and hardware

### Performance Tips

1. **Use enough work per thread**: Each thread should have substantial work
   - Good: 1000+ iterations per thread
   - Bad: < 100 iterations per thread (overhead dominates)

2. **Problem size matters**: Larger problems scale better
   - Vector: Use size ≥ 1M for meaningful parallelization
   - Matrix: Use size ≥ 256×256 for good speedup

3. **Match threads to cores**: Use `numThreads ≈ number of CPU cores`
   ```bash
   # Check CPU cores
   lscpu | grep "^CPU(s):"
   ```

4. **Minimize memory contention**: Avoid false sharing
   - Good: Each thread writes to separate array regions (vector.cpp)
   - Bad: All threads writing to same cache line

---

## Testing

### Run Provided Tests

```bash
# Compile
make all

# Test vector with default settings (2 threads, 48M elements)
./vector

# Test vector with 4 threads and 10M elements
./vector 4 10000000

# Test matrix with default settings (2 threads, 1024×1024)
./matrix

# Test matrix with 8 threads and 512×512
./matrix 8 512
```

### Expected Output

**Vector Test:**
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (4 threads): 11 ms
Test Success
====== Hope you enjoyed CSE231(A) ======
```

**Matrix Test:**
```
====== Welcome to Assignment-5 of the CSE231(A) ======
[parallel_for 1D] Execution time (2 threads): 42 ms
[parallel_for 2D] Execution time (2 threads): 126 ms
Test Success. 
[parallel_for 1D] Execution time (2 threads): 1 ms
====== Hope you enjoyed CSE231(A) ======
```

### Verify Correctness

Both programs include assertions to verify correctness:
- **vector.cpp**: Checks that all `C[i] == 2` (since 1+1=2)
- **matrix.cpp**: Checks that all `C[i][j] == size` (matrix multiplication property)

If you see "Test Success", the parallel computation produced correct results! ✅

---

## Troubleshooting

### Compilation Errors

**Error: `pthread_create` undefined reference**
```bash
# Solution: Add -lpthread flag
g++ -std=c++11 vector.cpp -lpthread
```

**Error: `lambda expression` syntax error**
```bash
# Solution: Add -std=c++11 flag
g++ -std=c++11 vector.cpp -lpthread
```

### Runtime Errors

**Error: Thread creation failed**
- **Cause**: System limit on number of threads
- **Solution**: Reduce `numThreads` or increase system limit
  ```bash
  ulimit -u  # Check current limit
  ulimit -u 4096  # Increase limit
  ```

**Error: Segmentation fault**
- **Cause**: Array out of bounds or invalid memory access
- **Solution**: Check array sizes and loop bounds

### Performance Issues

**No speedup observed:**
1. Problem size too small → Increase size
2. Too many threads → Reduce to number of cores
3. Memory-bound workload → Can't parallelize memory bandwidth

**Slower with more threads:**
- Thread creation overhead exceeds benefit
- Use larger problem sizes
- Ensure threads have enough work

---

## Assignment Details

**Course**: CSE 231 Operating Systems (Section-A)  
**Assignment**: Assignment-5  
**Title**: SimpleMultithreader: Using Multithreading with Ease  
**Due Date**: November 22, 2025 (11:59 PM)  
**Weight**: 7% of course grade  
**Instructor**: Vivek Kumar

### Learning Objectives
1. Understand POSIX pthread programming
2. Learn work distribution in parallel programs
3. Practice C++11 lambda expressions
4. Implement header-only libraries
5. Handle thread synchronization and cleanup

### Submission Requirements
- Source files (simple-multithreader.h, vector.cpp, matrix.cpp, Makefile)
- Design document detailing implementation
- REQUIREMENTS.md (requirement mapping)
- README.md (this file)
- DEMO.md (sample outputs)

---

## Additional Resources

### C++11 Lambda Expressions
- [RIP Tutorial: What is a Lambda Expression](https://riptutorial.com/cplusplus/example/1854/what-is-a-lambda-expression-)
- [Embarcadero: Lambda Expressions for Beginners](https://blogs.embarcadero.com/lambda-expressions-for-beginners/)

### POSIX Threads
- `man pthread_create`
- `man pthread_join`
- [POSIX Threads Programming (LLNL)](https://hpc-tutorials.llnl.gov/posix/)

### Parallel Programming
- [Introduction to Parallel Computing](https://hpc.llnl.gov/documentation/tutorials/introduction-parallel-computing-tutorial)

---

## License

This project is created for educational purposes as part of CSE 231 Operating Systems course.

---

## Contact & Support

For assignment-related questions:
- Contact teaching staff via course forum
- Office hours: Check course schedule
- Email: Refer to course syllabus

---

**Happy Parallel Programming! 🚀**
