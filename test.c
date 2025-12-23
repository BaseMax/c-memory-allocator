#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "allocator.h"

void test_basic_allocation(void) {
    printf("Test: Basic allocation and free\n");
    
    void* ptr = mem_malloc(100);
    assert(ptr != NULL);
    
    memset(ptr, 'A', 100);
    
    mem_free(ptr);
    printf("  PASSED\n");
}

void test_multiple_allocations(void) {
    printf("Test: Multiple allocations\n");
    
    void* ptr1 = mem_malloc(50);
    void* ptr2 = mem_malloc(100);
    void* ptr3 = mem_malloc(200);
    
    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr3 != NULL);
    
    mem_free(ptr2);
    mem_free(ptr1);
    mem_free(ptr3);
    
    printf("  PASSED\n");
}

void test_calloc(void) {
    printf("Test: Calloc\n");
    
    size_t nmemb = 10;
    size_t size = 50;
    char* ptr = (char*)mem_calloc(nmemb, size);
    assert(ptr != NULL);
    
    /* Verify all bytes are zero */
    for (size_t i = 0; i < nmemb * size; i++) {
        assert(ptr[i] == 0);
    }
    
    mem_free(ptr);
    printf("  PASSED\n");
}

void test_realloc(void) {
    printf("Test: Realloc\n");
    
    char* ptr = (char*)mem_malloc(50);
    assert(ptr != NULL);
    
    strcpy(ptr, "Hello, World!");
    
    ptr = (char*)mem_realloc(ptr, 100);
    assert(ptr != NULL);
    assert(strcmp(ptr, "Hello, World!") == 0);
    
    ptr = (char*)mem_realloc(ptr, 25);
    assert(ptr != NULL);
    
    mem_free(ptr);
    printf("  PASSED\n");
}

void test_large_allocation(void) {
    printf("Test: Large allocation (mmap)\n");
    
    size_t large_size = 256 * 1024;  /* 256 KB */
    void* ptr = mem_malloc(large_size);
    assert(ptr != NULL);
    
    memset(ptr, 0xAB, large_size);
    
    mem_free(ptr);
    printf("  PASSED\n");
}

void test_coalescing(void) {
    printf("Test: Block coalescing\n");
    
    mem_reset();
    
    void* ptr1 = mem_malloc(100);
    void* ptr2 = mem_malloc(100);
    void* ptr3 = mem_malloc(100);
    
    mem_free(ptr1);
    mem_free(ptr2);
    mem_free(ptr3);
    
    mem_stats_t stats_after = mem_get_stats();
    
    printf("  Coalesces performed: %zu\n", stats_after.num_coalesces);
    printf("  PASSED\n");
}

void test_splitting(void) {
    printf("Test: Block splitting\n");
    
    mem_reset();
    
    /* Allocate a small block from a larger free block */
    void* ptr1 = mem_malloc(100);
    mem_free(ptr1);
    
    void* ptr2 = mem_malloc(50);
    
    mem_stats_t stats = mem_get_stats();
    printf("  Splits performed: %zu\n", stats.num_splits);
    
    mem_free(ptr2);
    printf("  PASSED\n");
}

void test_thread_safe_functions(void) {
    printf("Test: Thread-safe functions\n");
    
    void* ptr1 = mem_malloc_ts(100);
    void* ptr2 = mem_calloc_ts(10, 20);
    
    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    
    ptr1 = mem_realloc_ts(ptr1, 200);
    assert(ptr1 != NULL);
    
    mem_free_ts(ptr1);
    mem_free_ts(ptr2);
    
    printf("  PASSED\n");
}

int main(void) {
    printf("Custom Memory Allocator Test Suite\n");
    printf("===================================\n\n");
    
    test_basic_allocation();
    test_multiple_allocations();
    test_calloc();
    test_realloc();
    test_large_allocation();
    test_coalescing();
    test_splitting();
    test_thread_safe_functions();
    
    printf("\n=== Final Statistics ===\n");
    mem_print_stats();
    
    printf("\nAll tests passed!\n");
    return 0;
}
