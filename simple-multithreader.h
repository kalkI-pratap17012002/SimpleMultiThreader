// SimpleMultithreader: Header-only library for easy parallel loop execution
// CSE 231 Operating Systems Assignment-5
// November 2025
//
// This header provides parallel_for APIs that abstract away the complexity
// of using POSIX Pthreads for parallelizing loops. Supports both 1D and 2D loops.

#include <iostream>
#include <list>
#include <functional>
#include <stdlib.h>
#include <cstring>
#include <pthread.h>
#include <chrono>
#include <cstdio>

// Forward declaration
int user_main(int argc, char **argv);

// ============================================================================
// Thread Data Structures
// ============================================================================

// Contains work assignment for each thread in 1D parallel_for
struct ThreadWork1D {
    int start;                              
    int end;                                
    std::function<void(int)>* lambda;       
};

// Work assignment for 2D parallel_for
// The 2D iteration space gets flattened to 1D for easier distribution
// Converting back: i = low1 + (idx / (high2-low2)), j = low2 + (idx % (high2-low2))
struct ThreadWork2D {
    int start;                              
    int end;                                
    int low1, high1;                        
    int low2, high2;                        
    std::function<void(int, int)>* lambda;  
};

// ============================================================================
// Worker Functions
// ============================================================================

void* worker_1d(void* arg) {
    ThreadWork1D* work = static_cast<ThreadWork1D*>(arg);
    
    for (int i = work->start; i < work->end; i++) {
        (*(work->lambda))(i);
    }
    
    return NULL;
}

// Worker for 2D - converts linear indices back to (i,j) coordinates
void* worker_2d(void* arg) {
    ThreadWork2D* work = static_cast<ThreadWork2D*>(arg);
    
    int inner_size = work->high2 - work->low2;
    
    for (int idx = work->start; idx < work->end; idx++) {
        // Convert linear index back to 2D coordinates
        // This part took a while to get right - had to think through the math
        int i = work->low1 + (idx / inner_size);
        int j = work->low2 + (idx % inner_size);
        
        (*(work->lambda))(i, j);
    }
    
    return NULL;
}

// ============================================================================
// parallel_for Implementation (1D)
// ============================================================================

void parallel_for(int low, int high, std::function<void(int)> &&lambda, int numThreads) {
    int total_iterations = high - low;
    
    if (total_iterations <= 0) {
        std::cout << "[parallel_for 1D] No iterations to execute. Execution time: 0 ms\n";
        return;
    }
    
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
    
    auto start_time = std::chrono::steady_clock::now();
    
    // ceiling division to distribute work evenly
    int iterations_per_thread = (total_iterations + numThreads - 1) / numThreads;
    
    // numThreads-1 workers because main thread also does work
    pthread_t* threads = new pthread_t[numThreads - 1];
    ThreadWork1D* thread_data = new ThreadWork1D[numThreads - 1];
    
    // need to track which threads actually got created successfully
    // so we don't try to join ones that failed
    bool* thread_success = new bool[numThreads - 1];
    for (int t = 0; t < numThreads - 1; t++) {
        thread_success[t] = false;
    }
    
    int workers_initialized = 0;
    for (int t = 0; t < numThreads - 1; t++) {
        int start_idx = low + (t + 1) * iterations_per_thread;
        int end_idx = low + (t + 2) * iterations_per_thread;
        
        if (end_idx > high) {
            end_idx = high;
        }
        
        if (start_idx >= high) {
            break;
        }
        
        thread_data[t].start = start_idx;
        thread_data[t].end = end_idx;
        thread_data[t].lambda = &lambda;
        
        workers_initialized = t + 1;
        
        // printf("Creating thread %d: [%d, %d)\n", t, start_idx, end_idx);  // debug
        int rc = pthread_create(&threads[t], NULL, worker_1d, (void*)&thread_data[t]);
        
        if (rc != 0) {
            std::cerr << "[ERROR] Failed to create thread " << t << ", return code: " << rc << "\n";
            std::cerr << "[FALLBACK] Will execute thread " << t << "'s work in main thread\n";
        } else {
            thread_success[t] = true;
        }
    }
    
    // main thread does the first chunk
    int main_start = low;
    int main_end = low + iterations_per_thread;
    if (main_end > high) {
        main_end = high;
    }
    
    for (int i = main_start; i < main_end; i++) {
        lambda(i);
    }
    
    // if any thread failed to create, do its work here
    for (int t = 0; t < workers_initialized; t++) {
        if (!thread_success[t] && thread_data[t].start < thread_data[t].end) {
            for (int i = thread_data[t].start; i < thread_data[t].end; i++) {
                lambda(i);
            }
        }
    }
    
    // wait for all workers
    for (int t = 0; t < workers_initialized; t++) {
        if (thread_success[t]) {
            int rc = pthread_join(threads[t], NULL);
            
            if (rc != 0) {
                std::cerr << "[ERROR] Failed to join thread " << t << ", return code: " << rc << "\n";
            }
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "[parallel_for 1D] Execution time (" << numThreads << " threads): " 
              << duration.count() << " ms\n";
    
    delete[] threads;
    delete[] thread_data;
    delete[] thread_success;
}

// ============================================================================
// parallel_for Implementation (2D)
// ============================================================================

void parallel_for(int low1, int high1, int low2, int high2, 
                  std::function<void(int, int)> &&lambda, int numThreads) {
    int outer_iterations = high1 - low1;
    int inner_iterations = high2 - low2;
    int total_iterations = outer_iterations * inner_iterations;
    
    if (total_iterations <= 0) {
        std::cout << "[parallel_for 2D] No iterations to execute. Execution time: 0 ms\n";
        return;
    }
    
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
    
    auto start_time = std::chrono::steady_clock::now();
    
    int iterations_per_thread = (total_iterations + numThreads - 1) / numThreads;
    
    pthread_t* threads = new pthread_t[numThreads - 1];
    ThreadWork2D* thread_data = new ThreadWork2D[numThreads - 1];
    
    bool* thread_success = new bool[numThreads - 1];
    for (int t = 0; t < numThreads - 1; t++) {
        thread_success[t] = false;
    }
    
    int workers_initialized = 0;
    for (int t = 0; t < numThreads - 1; t++) {
        int start_idx = (t + 1) * iterations_per_thread;
        int end_idx = (t + 2) * iterations_per_thread;
        
        if (end_idx > total_iterations) {
            end_idx = total_iterations;
        }
        
        if (start_idx >= total_iterations) {
            break;
        }
        
        // each thread needs both the linear range and the original loop bounds
        // for converting back to (i,j)
        thread_data[t].start = start_idx;
        thread_data[t].end = end_idx;
        thread_data[t].low1 = low1;
        thread_data[t].high1 = high1;
        thread_data[t].low2 = low2;
        thread_data[t].high2 = high2;
        thread_data[t].lambda = &lambda;
        
        workers_initialized = t + 1;
        
        int rc = pthread_create(&threads[t], NULL, worker_2d, (void*)&thread_data[t]);
        
        if (rc != 0) {
            std::cerr << "[ERROR] Failed to create thread " << t << " for 2D parallel_for, return code: " << rc << "\n";
            std::cerr << "[FALLBACK] Will execute thread " << t << "'s work in main thread\n";
        } else {
            thread_success[t] = true;
        }
    }
    
    // main thread handles first chunk
    int main_end = iterations_per_thread;
    if (main_end > total_iterations) {
        main_end = total_iterations;
    }
    
    for (int idx = 0; idx < main_end; idx++) {
        int i = low1 + (idx / inner_iterations);
        int j = low2 + (idx % inner_iterations);
        lambda(i, j);
    }
    
    // fallback for failed threads
    for (int t = 0; t < workers_initialized; t++) {
        if (!thread_success[t] && thread_data[t].start < thread_data[t].end) {
            for (int idx = thread_data[t].start; idx < thread_data[t].end; idx++) {
                int i = low1 + (idx / inner_iterations);
                int j = low2 + (idx % inner_iterations);
                lambda(i, j);
            }
        }
    }
    
    for (int t = 0; t < workers_initialized; t++) {
        if (thread_success[t]) {
            int rc = pthread_join(threads[t], NULL);
            
            if (rc != 0) {
                std::cerr << "[ERROR] Failed to join thread " << t << " for 2D parallel_for, return code: " << rc << "\n";
            }
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "[parallel_for 2D] Execution time (" << numThreads << " threads): " 
              << duration.count() << " ms\n";
    
    delete[] threads;
    delete[] thread_data;
    delete[] thread_success;
}

// ============================================================================
// Demo function from boilerplate
// ============================================================================

void demonstration(std::function<void()> && lambda) {
  lambda();
}

int main(int argc, char **argv) {
  // Lambda demonstration - shows how to capture variables
  int x=5,y=1;
  
  auto lambda1 = [x, &y](void) {
    y = 5;
    std::cout<<"====== Welcome to Assignment-"<<y<<" of the CSE231(A) ======\n";
  };
  
  demonstration(lambda1);

  int rc = user_main(argc, argv);
 
  auto lambda2 = []() {
    std::cout<<"====== Hope you enjoyed CSE231(A) ======\n";
  };
  demonstration(lambda2);
  return rc;
}

#define main user_main
