#include <stdio.h>
#include <string.h>
#include "allocator.h"

/**
 * Example program demonstrating the custom memory allocator
 */

void example_basic_usage(void) {
    printf("=== Example 1: Basic Usage ===\n");
    
    // Allocate memory
    char* message = (char*)mem_malloc(50);
    if (!message) {
        fprintf(stderr, "Allocation failed!\n");
        return;
    }
    
    strcpy(message, "Hello from custom allocator!");
    printf("Message: %s\n", message);
    
    // Free memory
    mem_free(message);
    printf("Memory freed successfully\n\n");
}

void example_calloc(void) {
    printf("=== Example 2: Using calloc ===\n");
    
    // Allocate and zero-initialize an array
    int* numbers = (int*)mem_calloc(10, sizeof(int));
    if (!numbers) {
        fprintf(stderr, "Calloc failed!\n");
        return;
    }
    
    printf("Initial values (should be 0): ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", numbers[i]);
    }
    printf("\n");
    
    // Use the array
    for (int i = 0; i < 10; i++) {
        numbers[i] = i * i;
    }
    
    printf("After assignment: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", numbers[i]);
    }
    printf("\n");
    
    mem_free(numbers);
    printf("Array freed\n\n");
}

void example_realloc(void) {
    printf("=== Example 3: Using realloc ===\n");
    
    // Start with small buffer
    char* buffer = (char*)mem_malloc(20);
    strcpy(buffer, "Short string");
    printf("Original: %s (allocated 20 bytes)\n", buffer);
    
    // Need more space
    buffer = (char*)mem_realloc(buffer, 100);
    strcat(buffer, " - now with much more content!");
    printf("After realloc: %s (allocated 100 bytes)\n", buffer);
    
    // Shrink back down
    buffer = (char*)mem_realloc(buffer, 30);
    printf("After shrinking: %s (allocated 30 bytes)\n", buffer);
    
    mem_free(buffer);
    printf("Buffer freed\n\n");
}

void example_large_allocation(void) {
    printf("=== Example 4: Large Allocation (uses mmap) ===\n");
    
    // Allocate 1 MB (will use mmap)
    size_t size = 1024 * 1024;
    char* large_buffer = (char*)mem_malloc(size);
    if (!large_buffer) {
        fprintf(stderr, "Large allocation failed!\n");
        return;
    }
    
    printf("Allocated %zu bytes (1 MB)\n", size);
    
    // Use the memory
    memset(large_buffer, 'X', size);
    printf("Filled with 'X': first char = '%c', last char = '%c'\n", 
           large_buffer[0], large_buffer[size-1]);
    
    mem_free(large_buffer);
    printf("Large buffer freed (unmapped)\n\n");
}

void example_mixed_allocations(void) {
    printf("=== Example 5: Mixed Size Allocations ===\n");
    
    void* ptrs[10];
    size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};
    
    // Allocate various sizes
    for (int i = 0; i < 10; i++) {
        ptrs[i] = mem_malloc(sizes[i]);
        printf("Allocated %zu bytes\n", sizes[i]);
    }
    
    printf("\n");
    mem_print_stats();
    printf("\n");
    
    // Free in different order
    for (int i = 9; i >= 0; i--) {
        mem_free(ptrs[i]);
    }
    
    printf("All allocations freed\n\n");
}

void example_thread_safe(void) {
    printf("=== Example 6: Thread-Safe Functions ===\n");
    
    // Use thread-safe versions
    void* ptr1 = mem_malloc_ts(100);
    void* ptr2 = mem_malloc_ts(200);
    
    printf("Allocated using thread-safe functions\n");
    
    mem_free_ts(ptr1);
    mem_free_ts(ptr2);
    
    printf("Freed using thread-safe functions\n\n");
}

int main(void) {
    printf("Custom Memory Allocator - Example Program\n");
    printf("==========================================\n\n");
    
    example_basic_usage();
    example_calloc();
    example_realloc();
    example_large_allocation();
    example_mixed_allocations();
    example_thread_safe();
    
    printf("=== Final Statistics ===\n");
    mem_print_stats();
    
    return 0;
}
