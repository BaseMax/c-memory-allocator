#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "allocator.h"

#define NUM_ITERATIONS 100000
#define MAX_ALLOC_SIZE 4096

/* Benchmark custom allocator */
double benchmark_custom_allocator(void) {
    clock_t start = clock();
    void* ptrs[1000] = {NULL};
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int idx = i % 1000;
        size_t size = (rand() % MAX_ALLOC_SIZE) + 1;
        
        if (ptrs[idx]) {
            mem_free(ptrs[idx]);
        }
        
        ptrs[idx] = mem_malloc(size);
        if (ptrs[idx]) {
            memset(ptrs[idx], 0, size);
        }
    }
    
    /* Free all remaining allocations */
    for (int i = 0; i < 1000; i++) {
        if (ptrs[i]) {
            mem_free(ptrs[i]);
        }
    }
    
    clock_t end = clock();
    return ((double)(end - start)) / CLOCKS_PER_SEC;
}

/* Benchmark system malloc */
double benchmark_system_malloc(void) {
    clock_t start = clock();
    void* ptrs[1000] = {NULL};
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int idx = i % 1000;
        size_t size = (rand() % MAX_ALLOC_SIZE) + 1;
        
        if (ptrs[idx]) {
            free(ptrs[idx]);
        }
        
        ptrs[idx] = malloc(size);
        if (ptrs[idx]) {
            memset(ptrs[idx], 0, size);
        }
    }
    
    /* Free all remaining allocations */
    for (int i = 0; i < 1000; i++) {
        if (ptrs[i]) {
            free(ptrs[i]);
        }
    }
    
    clock_t end = clock();
    return ((double)(end - start)) / CLOCKS_PER_SEC;
}

/* Benchmark calloc */
double benchmark_calloc(void) {
    clock_t start = clock();
    
    for (int i = 0; i < NUM_ITERATIONS / 10; i++) {
        size_t nmemb = rand() % 100 + 1;
        size_t size = rand() % 100 + 1;
        
        void* ptr = mem_calloc(nmemb, size);
        mem_free(ptr);
    }
    
    clock_t end = clock();
    return ((double)(end - start)) / CLOCKS_PER_SEC;
}

/* Benchmark realloc */
double benchmark_realloc(void) {
    clock_t start = clock();
    
    void* ptr = NULL;
    for (int i = 0; i < NUM_ITERATIONS / 10; i++) {
        size_t size = rand() % MAX_ALLOC_SIZE + 1;
        ptr = mem_realloc(ptr, size);
        if (ptr) {
            memset(ptr, i & 0xFF, size);
        }
    }
    
    mem_free(ptr);
    
    clock_t end = clock();
    return ((double)(end - start)) / CLOCKS_PER_SEC;
}

/* Test fragmentation */
void test_fragmentation(void) {
    printf("\n=== Fragmentation Test ===\n");
    mem_reset();
    
    void* ptrs[100];
    
    /* Allocate many blocks */
    for (int i = 0; i < 100; i++) {
        ptrs[i] = mem_malloc(128);
    }
    
    /* Free every other block */
    for (int i = 0; i < 100; i += 2) {
        mem_free(ptrs[i]);
        ptrs[i] = NULL;
    }
    
    printf("After freeing every other block:\n");
    mem_print_stats();
    
    /* Allocate larger blocks to test coalescing */
    for (int i = 0; i < 100; i += 2) {
        ptrs[i] = mem_malloc(256);
    }
    
    printf("\nAfter allocating larger blocks:\n");
    mem_print_stats();
    
    /* Free all */
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) {
            mem_free(ptrs[i]);
        }
    }
    
    printf("\nAfter freeing all:\n");
    mem_print_stats();
}

/* Test various allocation sizes */
void test_allocation_sizes(void) {
    printf("\n=== Allocation Size Test ===\n");
    mem_reset();
    
    size_t sizes[] = {1, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 
                      8192, 16384, 32768, 65536, 131072, 262144};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        void* ptr = mem_malloc(sizes[i]);
        printf("Allocated %zu bytes: %s\n", sizes[i], ptr ? "SUCCESS" : "FAILED");
        if (ptr) {
            memset(ptr, 0xFF, sizes[i]);
            mem_free(ptr);
        }
    }
    
    mem_print_stats();
}

/* Test edge cases */
void test_edge_cases(void) {
    printf("\n=== Edge Cases Test ===\n");
    mem_reset();
    
    /* Test NULL and zero size */
    void* p1 = mem_malloc(0);
    printf("malloc(0): %s\n", p1 == NULL ? "NULL (correct)" : "non-NULL");
    
    /* Test free(NULL) */
    mem_free(NULL);
    printf("free(NULL): completed without crash\n");
    
    /* Test realloc edge cases */
    void* p2 = mem_realloc(NULL, 100);
    printf("realloc(NULL, 100): %s\n", p2 ? "SUCCESS" : "FAILED");
    mem_free(p2);
    
    void* p3 = mem_malloc(100);
    void* p4 = mem_realloc(p3, 0);
    printf("realloc(ptr, 0): %s\n", p4 == NULL ? "NULL (correct)" : "non-NULL");
    
    /* Test calloc overflow */
    void* p5 = mem_calloc((size_t)-1, 2);
    printf("calloc overflow check: %s\n", p5 == NULL ? "NULL (correct)" : "non-NULL");
    
    mem_print_stats();
}

int main(void) {
    srand(time(NULL));
    
    printf("Custom Memory Allocator Benchmark Suite\n");
    printf("========================================\n\n");
    
    /* Run allocation size tests */
    test_allocation_sizes();
    
    /* Run edge case tests */
    test_edge_cases();
    
    /* Run fragmentation test */
    test_fragmentation();
    
    /* Run performance benchmarks */
    printf("\n=== Performance Benchmarks ===\n");
    mem_reset();
    
    double custom_time = benchmark_custom_allocator();
    printf("Custom allocator: %.3f seconds\n", custom_time);
    mem_print_stats();
    
    printf("\n");
    double system_time = benchmark_system_malloc();
    printf("System malloc: %.3f seconds\n", system_time);
    
    double ratio = custom_time / system_time;
    printf("\nPerformance ratio (custom/system): %.2fx\n", ratio);
    
    /* Calloc benchmark */
    printf("\n");
    mem_reset();
    double calloc_time = benchmark_calloc();
    printf("Calloc benchmark: %.3f seconds\n", calloc_time);
    
    /* Realloc benchmark */
    printf("\n");
    mem_reset();
    double realloc_time = benchmark_realloc();
    printf("Realloc benchmark: %.3f seconds\n", realloc_time);
    
    return 0;
}
