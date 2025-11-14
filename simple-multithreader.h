// ============================================================================
// SimpleMultithreader: Header-only library for easy parallel loop execution
// ============================================================================
// This header provides parallel_for APIs that abstract away the complexity
// of using POSIX Pthreads for parallelizing loops. It supports both 1D and
// 2D loop parallelization using C++11 lambda expressions.
//
// Author: CSE 231 Operating Systems Assignment-5
// Date: November 2025
// ============================================================================

#include <iostream>         // For std::cout, std::cerr (output streams)
#include <list>             // Included in boilerplate (not actively used)
#include <functional>       // For std::function (lambda function wrapper)
#include <stdlib.h>         // For general utilities like atoi
#include <cstring>          // For C string functions
#include <pthread.h>        // POSIX threads API for thread creation/management
#include <chrono>           // C++11 timing library for execution time measurement
#include <cstdio>           // For printf (alternative to std::cout for timing output)

// Forward declaration of user_main function
// The #define main user_main at the end redirects main() calls to user_main()
int user_main(int argc, char **argv);

// ============================================================================
// THREAD DATA STRUCTURES
// ============================================================================
// These structures hold all the data that needs to be passed to worker threads.
// Since pthread_create() only allows passing a single void* argument, we pack
// all necessary information (loop bounds, lambda function, thread ID) into
// these structs.
// ============================================================================

/**
 * ThreadWork1D: Data structure for 1D parallel_for worker threads
 * 
 * Purpose: Contains all information a worker thread needs to execute its 
 * assigned portion of a 1D loop (e.g., for(i=low; i<high; i++))
 * 
 * Fields:
 *   - start: Starting index for this thread's work (inclusive)
 *   - end: Ending index for this thread's work (exclusive, similar to [start, end) range)
 *   - lambda: Pointer to the lambda function that will be executed for each iteration
 *            The lambda takes one int parameter (the loop index i)
 * 
 * Memory Management:
 *   - This struct is allocated on the stack in parallel_for and passed by pointer
 *   - The lambda is stored as a pointer to avoid copying the std::function object
 *   - All threads share the same lambda, so we must ensure it remains valid
 *     until all threads complete (achieved by pthread_join before function returns)
 */
struct ThreadWork1D {
    int start;                              // Starting iteration index for this thread
    int end;                                // Ending iteration index (exclusive) for this thread
    std::function<void(int)>* lambda;       // Pointer to the lambda function to execute
};

/**
 * ThreadWork2D: Data structure for 2D parallel_for worker threads
 * 
 * Purpose: Contains all information a worker thread needs to execute its 
 * assigned portion of a 2D nested loop (e.g., for(i=...) for(j=...))
 * 
 * Fields:
 *   - start: Starting linear index in the flattened 2D iteration space
 *   - end: Ending linear index (exclusive) in the flattened 2D iteration space
 *   - low1, high1: Outer loop bounds (i loop bounds)
 *   - low2, high2: Inner loop bounds (j loop bounds)
 *   - lambda: Pointer to the lambda function that takes two parameters (i, j)
 * 
 * 2D to 1D Mapping:
 *   The 2D iteration space (i,j) is flattened into a 1D space for easy distribution.
 *   - Total iterations = (high1-low1) * (high2-low2)
 *   - Linear index = (i-low1) * (high2-low2) + (j-low2)
 *   - To recover (i,j) from linear index:
 *       i = low1 + (linear_index / (high2-low2))
 *       j = low2 + (linear_index % (high2-low2))
 * 
 * Example:
 *   For loops: for(i=0; i<3; i++) for(j=0; j<4; j++)
 *   Total iterations = 3 * 4 = 12
 *   Linear index 0 maps to (i=0, j=0)
 *   Linear index 5 maps to (i=1, j=1) because 5/(4) = 1, 5%(4) = 1
 */
struct ThreadWork2D {
    int start;                              // Starting linear index for this thread
    int end;                                // Ending linear index (exclusive) for this thread
    int low1, high1;                        // Outer loop bounds (i loop: low1 <= i < high1)
    int low2, high2;                        // Inner loop bounds (j loop: low2 <= j < high2)
    std::function<void(int, int)>* lambda;  // Pointer to lambda function taking (i, j)
};

// ============================================================================
// PTHREAD WORKER FUNCTIONS
// ============================================================================
// These are the entry point functions for worker threads created by pthread_create.
// They must have the signature: void* function_name(void* arg)
// They unpack the thread data, execute the assigned loop iterations, and return.
// ============================================================================

/**
 * worker_1d: Worker function for 1D parallel_for threads
 * 
 * Purpose: This function is called by each worker thread created in parallel_for (1D version).
 * It receives a ThreadWork1D structure, extracts the work assignment, and executes
 * the lambda function for each iteration in the assigned range [start, end).
 * 
 * Parameters:
 *   - arg: void pointer that actually points to a ThreadWork1D struct
 * 
 * Returns:
 *   - NULL (required by pthread API, we don't use the return value)
 * 
 * Thread Safety:
 *   - Each thread works on a disjoint range of iterations, so no synchronization
 *     is needed UNLESS the lambda function itself accesses shared data
 *   - The user is responsible for any synchronization inside the lambda if needed
 * 
 * Example Execution:
 *   If start=0, end=1000, the thread will call (*lambda)(0), (*lambda)(1), ..., (*lambda)(999)
 */
void* worker_1d(void* arg) {
    // Step 1: Cast the void* argument back to ThreadWork1D*
    // This is safe because we know parallel_for passes a ThreadWork1D*
    ThreadWork1D* work = static_cast<ThreadWork1D*>(arg);
    
    // Step 2: Execute the lambda function for each iteration in [start, end)
    // This is equivalent to: for(int i = start; i < end; i++) lambda(i);
    for (int i = work->start; i < work->end; i++) {
        // Dereference the lambda pointer and call it with the current index
        (*(work->lambda))(i);
    }
    
    // Step 3: Return NULL (standard pthread worker return)
    // The return value is not used in our implementation
    return NULL;
}

/**
 * worker_2d: Worker function for 2D parallel_for threads
 * 
 * Purpose: This function is called by each worker thread created in parallel_for (2D version).
 * It iterates through a linear range [start, end) and converts each linear index back
 * to 2D coordinates (i, j) before calling the lambda function.
 * 
 * Parameters:
 *   - arg: void pointer that actually points to a ThreadWork2D struct
 * 
 * Returns:
 *   - NULL (required by pthread API)
 * 
 * Linear to 2D Conversion Algorithm:
 *   Given linear index 'idx' and inner loop size (high2-low2):
 *   - i coordinate: low1 + (idx / inner_size)
 *   - j coordinate: low2 + (idx % inner_size)
 *   
 *   This recovers the original (i, j) coordinates from the flattened iteration space.
 * 
 * Example:
 *   For nested loops: for(i=0; i<3; i++) for(j=10; j<14; j++)
 *   Inner size = 14-10 = 4
 *   If linear index = 5:
 *     i = 0 + (5/4) = 1
 *     j = 10 + (5%4) = 11
 *   So the thread calls lambda(1, 11)
 */
void* worker_2d(void* arg) {
    // Step 1: Cast the void* argument back to ThreadWork2D*
    ThreadWork2D* work = static_cast<ThreadWork2D*>(arg);
    
    // Step 2: Calculate the size of the inner loop (j loop range)
    // This is used for converting linear index back to (i, j) coordinates
    int inner_size = work->high2 - work->low2;
    
    // Step 3: Iterate through the linear index range assigned to this thread
    for (int idx = work->start; idx < work->end; idx++) {
        // Step 3a: Convert linear index to outer loop index (i)
        // Integer division gives us which "row" we're in
        int i = work->low1 + (idx / inner_size);
        
        // Step 3b: Convert linear index to inner loop index (j)
        // Modulo gives us the position within the "row"
        int j = work->low2 + (idx % inner_size);
        
        // Step 3c: Call the lambda function with the 2D coordinates
        (*(work->lambda))(i, j);
    }
    
    // Step 4: Return NULL (standard pthread worker return)
    return NULL;
}

// ============================================================================
// PARALLEL_FOR IMPLEMENTATIONS
// ============================================================================
// These are the main API functions that users call to parallelize their loops.
// They handle thread creation, work distribution, execution timing, and cleanup.
// ============================================================================

/**
 * parallel_for (1D version): Parallelizes a single for-loop using Pthreads
 * 
 * Purpose: Takes a sequential loop for(i=low; i<high; i++) and executes it in
 * parallel across multiple threads. This reduces execution time for compute-intensive
 * loop bodies by distributing the work.
 * 
 * Parameters:
 *   - low: Starting index of the loop (inclusive)
 *   - high: Ending index of the loop (exclusive), so loop runs for indices [low, high)
 *   - lambda: C++11 lambda function (or any callable) taking one int parameter
 *            This function is called for each iteration: lambda(i) for i in [low, high)
 *   - numThreads: Total number of threads to use INCLUDING the main thread
 *                For example, numThreads=4 means main thread + 3 worker threads
 * 
 * Work Distribution Algorithm:
 *   - Total iterations = high - low
 *   - Iterations per thread = ceil(total_iterations / numThreads)
 *   - Each thread gets a chunk of consecutive iterations
 *   - The main thread also participates and executes the first chunk
 *   - This ensures all numThreads threads (including main) do work
 * 
 * Thread Creation Strategy:
 *   - We create (numThreads - 1) worker threads using pthread_create
 *   - The main thread executes the first chunk directly (no pthread_create needed)
 *   - This satisfies the requirement: "exactly numThreads threads including main"
 * 
 * Timing:
 *   - Measures wall-clock time from start of parallel execution to completion
 *   - Prints the execution time in milliseconds to help users track performance
 * 
 * Error Handling:
 *   - Checks pthread_create and pthread_join return values
 *   - Prints error messages if thread operations fail
 *   - Ensures pthread_join is called for all successfully created threads (cleanup)
 * 
 * Example Usage:
 *   parallel_for(0, 1000, [&](int i) { array[i] = i * 2; }, 4);
 *   This distributes 1000 iterations across 4 threads (main + 3 workers)
 */
void parallel_for(int low, int high, std::function<void(int)> &&lambda, int numThreads) {
    // ==============================================================
    // STEP 1: INPUT VALIDATION AND SPECIAL CASES
    // ==============================================================
    
    // Calculate total number of iterations needed
    int total_iterations = high - low;
    
    // Edge case: If no iterations, return immediately (nothing to do)
    if (total_iterations <= 0) {
        std::cout << "[parallel_for 1D] No iterations to execute. Execution time: 0 ms\n";
        return;
    }
    
    // Edge case: If only 1 thread requested, execute sequentially (no pthread overhead)
    if (numThreads <= 1) {
        auto start_time = std::chrono::steady_clock::now();
        for (int i = low; i < high; i++) {
            lambda(i);
        }
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "[parallel_for 1D] Execution time (sequential, 1 thread): " 
                  << duration.count() << " ms\n";
        return;
    }
    
    // ==============================================================
    // STEP 2: START TIMING (before any parallel work begins)
    // ==============================================================
    auto start_time = std::chrono::steady_clock::now();
    
    // ==============================================================
    // STEP 3: CALCULATE WORK DISTRIBUTION
    // ==============================================================
    
    // Calculate how many iterations each thread should handle
    // Using ceiling division: (a + b - 1) / b is equivalent to ceil(a/b)
    // This ensures all iterations are covered even if total_iterations is not divisible by numThreads
    int iterations_per_thread = (total_iterations + numThreads - 1) / numThreads;
    
    // ==============================================================
    // STEP 4: ALLOCATE THREAD RESOURCES
    // ==============================================================
    
    // Allocate array to store pthread handles for worker threads
    // We create (numThreads - 1) workers because main thread also does work
    pthread_t* threads = new pthread_t[numThreads - 1];
    
    // Allocate array to store work assignments for each worker thread
    ThreadWork1D* thread_data = new ThreadWork1D[numThreads - 1];
    
    // Track which threads were successfully created (for safe joining)
    // thread_success[t] = true means threads[t] contains a valid pthread_t handle
    bool* thread_success = new bool[numThreads - 1];
    for (int t = 0; t < numThreads - 1; t++) {
        thread_success[t] = false;  // Initialize all to false
    }
    
    // ==============================================================
    // STEP 5: CREATE WORKER THREADS AND ASSIGN WORK
    // ==============================================================
    
    // Create (numThreads - 1) worker threads
    // Thread 0's work will be done by the main thread (see STEP 6)
    // Track how many worker slots were actually initialized (may be less than numThreads-1)
    int workers_initialized = 0;
    for (int t = 0; t < numThreads - 1; t++) {
        // Calculate the start and end indices for this worker thread
        // Thread t handles chunk (t+1) because chunk 0 is for main thread
        int start_idx = low + (t + 1) * iterations_per_thread;
        int end_idx = low + (t + 2) * iterations_per_thread;
        
        // Clamp end_idx to 'high' to avoid going beyond the loop bounds
        // This is important for the last thread which might have fewer iterations
        if (end_idx > high) {
            end_idx = high;
        }
        
        // Skip creating thread if there's no work for it
        // This can happen if numThreads > total_iterations
        if (start_idx >= high) {
            break;  // No more work to distribute, exit loop
        }
        
        // Setup work assignment for this worker thread
        // This must be done before pthread_create so the data is available
        thread_data[t].start = start_idx;
        thread_data[t].end = end_idx;
        thread_data[t].lambda = &lambda;  // Store pointer to lambda (shared by all threads)
        
        // Track that we initialized this slot (even if pthread_create fails)
        // This ensures fallback/join loops won't access uninitialized data
        workers_initialized = t + 1;
        
        // Create the worker thread
        // pthread_create(thread_handle, attributes, worker_function, argument)
        // Returns 0 on success, error code on failure
        int rc = pthread_create(&threads[t], NULL, worker_1d, (void*)&thread_data[t]);
        
        // Error handling: Check if thread creation succeeded
        if (rc != 0) {
            std::cerr << "[ERROR] Failed to create thread " << t << ", return code: " << rc << "\n";
            std::cerr << "[FALLBACK] Will execute thread " << t << "'s work in main thread\n";
            // thread_success[t] remains false, so we won't try to join this thread
            // Fallback execution will handle this thread's work
        } else {
            thread_success[t] = true;  // Mark as successfully created
        }
    }
    
    // ==============================================================
    // STEP 6: MAIN THREAD EXECUTES ITS SHARE OF WORK
    // ==============================================================
    
    // The main thread (current thread) executes the first chunk of work
    // This chunk corresponds to thread index 0
    int main_start = low;
    int main_end = low + iterations_per_thread;
    if (main_end > high) {
        main_end = high;  // Clamp to loop bounds
    }
    
    // Execute the lambda for the main thread's assigned range
    for (int i = main_start; i < main_end; i++) {
        lambda(i);
    }
    
    // ==============================================================
    // STEP 6b: FALLBACK EXECUTION FOR FAILED THREAD CREATIONS
    // ==============================================================
    
    // If any threads failed to create, execute their work in the main thread
    // This ensures ALL iterations are executed even if pthread_create fails
    // Maintains correctness at the cost of reduced parallelism
    // Only check initialized worker slots to avoid accessing uninitialized data
    for (int t = 0; t < workers_initialized; t++) {
        if (!thread_success[t] && thread_data[t].start < thread_data[t].end) {
            // This thread failed to create, so execute its work sequentially
            for (int i = thread_data[t].start; i < thread_data[t].end; i++) {
                lambda(i);
            }
        }
    }
    
    // ==============================================================
    // STEP 7: WAIT FOR ALL WORKER THREADS TO COMPLETE
    // ==============================================================
    
    // Join all successfully created worker threads
    // pthread_join blocks until the specified thread terminates
    // Only join threads that were successfully created (thread_success[t] == true)
    // Only check initialized worker slots to avoid joining uninitialized handles
    // This ensures all work is complete before we proceed
    for (int t = 0; t < workers_initialized; t++) {
        if (thread_success[t]) {
            // This thread was successfully created, so join it
            int rc = pthread_join(threads[t], NULL);
            
            // Error handling: Check if join succeeded
            if (rc != 0) {
                std::cerr << "[ERROR] Failed to join thread " << t << ", return code: " << rc << "\n";
            }
        }
    }
    
    // ==============================================================
    // STEP 8: STOP TIMING AND PRINT EXECUTION TIME
    // ==============================================================
    
    auto end_time = std::chrono::steady_clock::now();
    
    // Calculate elapsed time in milliseconds
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Print execution time as required by assignment
    std::cout << "[parallel_for 1D] Execution time (" << numThreads << " threads): " 
              << duration.count() << " ms\n";
    
    // ==============================================================
    // STEP 9: CLEANUP ALLOCATED RESOURCES
    // ==============================================================
    
    delete[] threads;         // Free pthread handle array
    delete[] thread_data;     // Free work assignment array
    delete[] thread_success;  // Free thread success tracking array
}

/**
 * parallel_for (2D version): Parallelizes nested for-loops using Pthreads
 * 
 * Purpose: Takes nested loops for(i=low1; i<high1; i++) for(j=low2; j<high2; j++)
 * and executes them in parallel across multiple threads. Useful for matrix operations,
 * image processing, and other 2D computations.
 * 
 * Parameters:
 *   - low1, high1: Outer loop bounds (i loop runs from low1 to high1-1)
 *   - low2, high2: Inner loop bounds (j loop runs from low2 to high2-1)
 *   - lambda: C++11 lambda function taking two int parameters (i, j)
 *            Called for each pair: lambda(i, j) for all i in [low1,high1), j in [low2,high2)
 *   - numThreads: Total number of threads INCLUDING main thread
 * 
 * 2D Parallelization Strategy:
 *   - Flatten the 2D iteration space into a 1D linear space
 *   - Distribute the linear iterations among threads
 *   - Each thread converts its linear indices back to (i, j) coordinates
 *   - This approach is simpler than trying to partition a 2D grid
 * 
 * Work Distribution:
 *   - Total iterations = (high1 - low1) * (high2 - low2)
 *   - Each thread gets ~(total/numThreads) consecutive linear iterations
 *   - The mapping ensures all (i,j) pairs are covered exactly once
 * 
 * Advantages of Linear Flattening:
 *   - Simple, uniform work distribution
 *   - Reuses the same chunking logic as 1D version
 *   - Easy to handle arbitrary loop bounds
 *   - Good cache locality if iterations are processed in order
 * 
 * Example Usage:
 *   parallel_for(0, 100, 0, 100, [&](int i, int j) { C[i][j] = A[i][j] + B[i][j]; }, 4);
 *   This distributes 10000 (i,j) pairs across 4 threads for matrix addition
 */
void parallel_for(int low1, int high1, int low2, int high2, 
                  std::function<void(int, int)> &&lambda, int numThreads) {
    // ==============================================================
    // STEP 1: INPUT VALIDATION AND SPECIAL CASES
    // ==============================================================
    
    // Calculate dimensions of the 2D iteration space
    int outer_iterations = high1 - low1;  // Number of i values
    int inner_iterations = high2 - low2;  // Number of j values
    
    // Calculate total iterations by flattening the 2D space
    // This is equivalent to total number of (i, j) pairs
    int total_iterations = outer_iterations * inner_iterations;
    
    // Edge case: If no iterations, return immediately
    if (total_iterations <= 0) {
        std::cout << "[parallel_for 2D] No iterations to execute. Execution time: 0 ms\n";
        return;
    }
    
    // Edge case: If only 1 thread requested, execute sequentially
    if (numThreads <= 1) {
        auto start_time = std::chrono::steady_clock::now();
        for (int i = low1; i < high1; i++) {
            for (int j = low2; j < high2; j++) {
                lambda(i, j);
            }
        }
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "[parallel_for 2D] Execution time (sequential, 1 thread): " 
                  << duration.count() << " ms\n";
        return;
    }
    
    // ==============================================================
    // STEP 2: START TIMING
    // ==============================================================
    auto start_time = std::chrono::steady_clock::now();
    
    // ==============================================================
    // STEP 3: CALCULATE WORK DISTRIBUTION
    // ==============================================================
    
    // Calculate iterations per thread using ceiling division
    int iterations_per_thread = (total_iterations + numThreads - 1) / numThreads;
    
    // ==============================================================
    // STEP 4: ALLOCATE THREAD RESOURCES
    // ==============================================================
    
    // Allocate pthread handles for worker threads (numThreads - 1 workers)
    pthread_t* threads = new pthread_t[numThreads - 1];
    
    // Allocate work assignments for worker threads
    ThreadWork2D* thread_data = new ThreadWork2D[numThreads - 1];
    
    // Track which threads were successfully created (for safe joining)
    // thread_success[t] = true means threads[t] contains a valid pthread_t handle
    bool* thread_success = new bool[numThreads - 1];
    for (int t = 0; t < numThreads - 1; t++) {
        thread_success[t] = false;  // Initialize all to false
    }
    
    // ==============================================================
    // STEP 5: CREATE WORKER THREADS AND ASSIGN WORK
    // ==============================================================
    
    // Create worker threads (main thread will handle chunk 0)
    // Track how many worker slots were actually initialized (may be less than numThreads-1)
    int workers_initialized = 0;
    for (int t = 0; t < numThreads - 1; t++) {
        // Calculate linear index range for this worker thread
        // Thread t handles chunk (t+1) in the linear space
        int start_idx = (t + 1) * iterations_per_thread;
        int end_idx = (t + 2) * iterations_per_thread;
        
        // Clamp end_idx to total_iterations
        if (end_idx > total_iterations) {
            end_idx = total_iterations;
        }
        
        // Skip if no work for this thread
        if (start_idx >= total_iterations) {
            break;  // No more work to distribute, exit loop
        }
        
        // Setup work assignment with both linear indices and loop bounds
        // This must be done before pthread_create so the data is available
        thread_data[t].start = start_idx;
        thread_data[t].end = end_idx;
        thread_data[t].low1 = low1;
        thread_data[t].high1 = high1;
        thread_data[t].low2 = low2;
        thread_data[t].high2 = high2;
        thread_data[t].lambda = &lambda;  // Shared lambda pointer
        
        // Track that we initialized this slot (even if pthread_create fails)
        // This ensures fallback/join loops won't access uninitialized data
        workers_initialized = t + 1;
        
        // Create worker thread
        int rc = pthread_create(&threads[t], NULL, worker_2d, (void*)&thread_data[t]);
        
        // Error handling
        if (rc != 0) {
            std::cerr << "[ERROR] Failed to create thread " << t << " for 2D parallel_for, return code: " << rc << "\n";
            std::cerr << "[FALLBACK] Will execute thread " << t << "'s work in main thread\n";
            // thread_success[t] remains false, so we won't try to join this thread
            // Fallback execution will handle this thread's work
        } else {
            thread_success[t] = true;  // Mark as successfully created
        }
    }
    
    // ==============================================================
    // STEP 6: MAIN THREAD EXECUTES ITS SHARE OF WORK
    // ==============================================================
    
    // Main thread handles the first chunk (linear indices 0 to iterations_per_thread-1)
    int main_end = iterations_per_thread;
    if (main_end > total_iterations) {
        main_end = total_iterations;
    }
    
    // Execute main thread's work by converting linear indices to (i, j)
    for (int idx = 0; idx < main_end; idx++) {
        // Convert linear index to 2D coordinates
        int i = low1 + (idx / inner_iterations);
        int j = low2 + (idx % inner_iterations);
        
        // Call the lambda with 2D coordinates
        lambda(i, j);
    }
    
    // ==============================================================
    // STEP 6b: FALLBACK EXECUTION FOR FAILED THREAD CREATIONS
    // ==============================================================
    
    // If any threads failed to create, execute their work in the main thread
    // This ensures ALL iterations are executed even if pthread_create fails
    // Maintains correctness at the cost of reduced parallelism
    // Only check initialized worker slots to avoid accessing uninitialized data
    for (int t = 0; t < workers_initialized; t++) {
        if (!thread_success[t] && thread_data[t].start < thread_data[t].end) {
            // This thread failed to create, so execute its work sequentially
            for (int idx = thread_data[t].start; idx < thread_data[t].end; idx++) {
                // Convert linear index to 2D coordinates
                int i = low1 + (idx / inner_iterations);
                int j = low2 + (idx % inner_iterations);
                
                // Call the lambda with 2D coordinates
                lambda(i, j);
            }
        }
    }
    
    // ==============================================================
    // STEP 7: WAIT FOR ALL WORKER THREADS TO COMPLETE
    // ==============================================================
    
    // Join all successfully created worker threads
    // Only join threads that were successfully created (thread_success[t] == true)
    // Only check initialized worker slots to avoid joining uninitialized handles
    for (int t = 0; t < workers_initialized; t++) {
        if (thread_success[t]) {
            // This thread was successfully created, so join it
            int rc = pthread_join(threads[t], NULL);
            
            if (rc != 0) {
                std::cerr << "[ERROR] Failed to join thread " << t << " for 2D parallel_for, return code: " << rc << "\n";
            }
        }
    }
    
    // ==============================================================
    // STEP 8: STOP TIMING AND PRINT EXECUTION TIME
    // ==============================================================
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Print execution time as required
    std::cout << "[parallel_for 2D] Execution time (" << numThreads << " threads): " 
              << duration.count() << " ms\n";
    
    // ==============================================================
    // STEP 9: CLEANUP ALLOCATED RESOURCES
    // ==============================================================
    
    delete[] threads;         // Free pthread handle array
    delete[] thread_data;     // Free work assignment array
    delete[] thread_success;  // Free thread success tracking array
}

// ============================================================================
// DEMONSTRATION FUNCTION (from original boilerplate)
// ============================================================================

/* Demonstration on how to pass lambda as parameter.
 * "&&" means r-value reference. You may read about it online.
 */
void demonstration(std::function<void()> && lambda) {
  lambda();
}

int main(int argc, char **argv) {
  /* 
   * Declaration of a sample C++ lambda function
   * that captures variable 'x' by value and 'y'
   * by reference. Global variables are by default
   * captured by reference and are not to be supplied
   * in the capture list. Only local variables must be 
   * explicity captured if they are used inside lambda.
   */
  int x=5,y=1;
  // Declaring a lambda expression that accepts void type parameter
  auto /*name*/ lambda1 = /*capture list*/[/*by value*/ x, /*by reference*/ &y](void) {
    /* Any changes to 'x' will throw compilation error as x is captured by value */
    y = 5;
    std::cout<<"====== Welcome to Assignment-"<<y<<" of the CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  // Executing the lambda function
  demonstration(lambda1); // the value of x is still 5, but the value of y is now 5

  int rc = user_main(argc, argv);
 
  auto /*name*/ lambda2 = [/*nothing captured*/]() {
    std::cout<<"====== Hope you enjoyed CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  demonstration(lambda2);
  return rc;
}

#define main user_main


