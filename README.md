# SimpleMultithreader - Operating Systems Assignment 5

**Course:** CSE 231 Operating Systems (Section A)  
**Assignment:** Assignment 5  
**Due Date:** November 22, 2025  
**Weightage:** 7%

**Submitted By:**  
Name: Ayush Kumar  
Roll No: 2020290  
Group No: 13

**GitHub Repository:** [Will be added]

---

## Introduction

This project implements a header-only C++ library called SimpleMultithreader that simplifies parallel loop execution using POSIX threads. The library provides an easy-to-use interface for parallelizing both one-dimensional and two-dimensional loops through the parallel_for API, which accepts C++11 lambda expressions.

The main goal is to abstract away the complexity of pthread programming so that users can parallelize their loops with minimal code changes. Instead of manually managing thread creation, work distribution, and synchronization, users can simply call parallel_for with their loop body as a lambda function.

---

## Project Structure

The submission includes the following files:

- **simple-multithreader.h** - Main header file containing the complete implementation
- **vector.cpp** - Example program demonstrating 1D parallel_for (vector addition)
- **matrix.cpp** - Example program demonstrating both 1D and 2D parallel_for (matrix multiplication)
- **Makefile** - Build configuration for compiling the example programs
- **README.md** - This file
- **DEMO.md** - Detailed sample inputs and expected outputs
- **REQUIREMENTS.md** - Mapping of assignment requirements to implementation

---

## System Requirements

### Hardware
- Linux or Unix-based system (Ubuntu, Fedora, or WSL on Windows)
- MacOS is not recommended as per assignment guidelines
- Multi-core processor recommended for observing parallel speedup

### Software
- C++ compiler with C++11 support (g++ 4.8.1 or later)
- GNU Make
- POSIX threads library (pthread)

---

## Building the Project

To compile the example programs:

```bash
make all
```

To compile individual programs:

```bash
make vector
make matrix
```

To clean build artifacts:

```bash
make clean
```

Manual compilation (if needed):

```bash
g++ -O3 -std=c++11 -o vector vector.cpp -lpthread
g++ -O3 -std=c++11 -o matrix matrix.cpp -lpthread
```

---

## Usage

### Basic Usage

Include the header file in your program:

```cpp
#include "simple-multithreader.h"
```

Call parallel_for with your loop:

```cpp
parallel_for(0, 1000, [&](int i) {
    array[i] = i * i;
}, 4);
```

Compile with pthread support:

```bash
g++ -std=c++11 your_program.cpp -lpthread
```

### Running the Examples

Vector addition example (1D parallel_for):

```bash
./vector [numThreads] [arraySize]
```

Default values: 2 threads, 48 million elements

```bash
./vector
./vector 4 10000000
```

Matrix multiplication example (2D parallel_for):

```bash
./matrix [numThreads] [matrixSize]
```

Default values: 2 threads, 1024x1024 matrix

```bash
./matrix
./matrix 4 512
```

---

## API Reference

### parallel_for (1D version)

```cpp
void parallel_for(int low, int high, std::function<void(int)> &&lambda, int numThreads);
```

Parameters:
- low: Starting index (inclusive)
- high: Ending index (exclusive)
- lambda: Function to execute for each index
- numThreads: Total number of threads including main thread

Example:

```cpp
parallel_for(0, size, [&](int i) {
    C[i] = A[i] + B[i];
}, 4);
```

### parallel_for (2D version)

```cpp
void parallel_for(int low1, int high1, int low2, int high2, 
                  std::function<void(int, int)> &&lambda, int numThreads);
```

Parameters:
- low1, high1: Outer loop bounds
- low2, high2: Inner loop bounds
- lambda: Function to execute for each (i,j) pair
- numThreads: Total number of threads including main thread

Example:

```cpp
parallel_for(0, rows, 0, cols, [&](int i, int j) {
    C[i][j] = A[i][j] + B[i][j];
}, 4);
```

---

## Implementation Details

### Thread Creation Strategy

The library creates exactly numThreads threads including the main thread. For example, if numThreads is 4, then 3 worker threads are created using pthread_create, and the main thread also participates in executing the loop iterations. This ensures we have exactly 4 threads doing work.

### Work Distribution

Work is distributed evenly across all threads using a simple chunking approach. The total number of iterations is divided by the number of threads, and each thread gets a consecutive chunk of iterations. The last thread may get slightly fewer iterations if the total is not evenly divisible.

For 2D loops, the two-dimensional iteration space is flattened into a one-dimensional space before distribution. Each thread then converts its linear index range back to (i,j) coordinates during execution.

### Error Handling

The implementation checks return codes from pthread_create and pthread_join. If thread creation fails, the work that would have been assigned to that thread is executed by the main thread as a fallback. This ensures correctness even when system resources are limited.

### Timing

Each call to parallel_for automatically measures and prints its execution time in milliseconds using the C++11 chrono library. This helps users track the performance benefit of parallelization.

---

## Testing

Both example programs include assertions to verify correctness:

- vector.cpp checks that all elements of the result array equal 2 (since 1+1=2)
- matrix.cpp checks that all elements of the result matrix equal the matrix size

If you see "Test Success" in the output, the parallel computation produced correct results.

Sample test commands:

```bash
./vector 4 10000000
./matrix 4 512
```

---

## Performance Observations

Performance depends on several factors including problem size, number of CPU cores, and memory bandwidth. Generally:

- Vector addition shows good speedup for large arrays (10 million+ elements)
- Matrix multiplication shows excellent speedup due to high computation per element
- Using more threads than CPU cores may not improve performance
- Very small problems may run slower in parallel due to thread creation overhead

Recommended thread counts:
- 2-4 threads for dual-core or quad-core systems
- 8 threads for 8-core systems
- Generally match the number of physical CPU cores

---

## Known Limitations

- Thread creation overhead makes parallelization inefficient for very small problems
- No automatic tuning of thread count based on problem size
- Memory-bound workloads may not see significant speedup
- No support for nested parallelism (calling parallel_for from within a parallel_for)

---

## Assignment Requirements Met

This implementation satisfies all requirements specified in the assignment document:

1. Provides both 1D and 2D parallel_for interfaces with correct signatures
2. Does not use any thread pool concept - creates fresh threads for each call
3. Runtime has exactly numThreads threads including main thread
4. Threads are created and destroyed within each parallel_for call
5. Code is modular with separate worker functions for 1D and 2D cases
6. Works with provided example programs without modification
7. Prints execution time for each parallel_for call
8. Includes proper error checking for pthread operations
9. Code is documented with explanatory comments
10. Header-only implementation as required
11. Uses C++11 lambda expressions
12. Only uses POSIX pthread APIs, no C++11 threading

---

## Group Contribution

**Ayush Kumar (2020290):**
- Designed and implemented the core parallel_for functions
- Implemented work distribution algorithm for both 1D and 2D cases
- Added error handling and fallback execution logic
- Wrote the worker thread functions
- Created timing measurement code
- Prepared documentation files
- Tested with provided example programs

---

## References

The following resources were consulted during development:

- POSIX Threads Programming guide
- C++11 lambda expression tutorials from course materials
- pthread man pages (pthread_create, pthread_join)
- Assignment 5 specification document
- Lecture 21 notes on parallel programming

---

## Compilation Flags Used

The Makefile uses the following flags:

- **-O3** - Maximum optimization for performance
- **-std=c++11** - Enable C++11 features required for lambda expressions
- **-lpthread** - Link with POSIX threads library

---

## Contact Information

For questions or issues related to this submission:

Name: Ayush Kumar  
Roll No: 2020290  
Group No: 13  
Course: CSE 231 Operating Systems (Section A)

---

## Acknowledgments

Thanks to Professor Vivek Kumar for the assignment specification and the teaching staff for their support during implementation.

---

**End of README**
