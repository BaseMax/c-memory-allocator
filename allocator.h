#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

/**
 * Custom Memory Allocator API
 * 
 * This allocator provides malloc/free/calloc/realloc replacements using:
 * - mmap/brk for memory acquisition
 * - Segregated free lists for efficient allocation
 * - Block splitting and coalescing to minimize fragmentation
 */

/* Thread-unsafe versions (faster) */
void* mem_malloc(size_t size);
void mem_free(void* ptr);
void* mem_calloc(size_t nmemb, size_t size);
void* mem_realloc(void* ptr, size_t size);

/* Thread-safe versions (with mutex protection) */
void* mem_malloc_ts(size_t size);
void mem_free_ts(void* ptr);
void* mem_calloc_ts(size_t nmemb, size_t size);
void* mem_realloc_ts(void* ptr, size_t size);

/* Utility functions */
void mem_print_stats(void);
void mem_reset(void);

/* Internal statistics structure */
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t num_allocations;
    size_t num_frees;
    size_t num_splits;
    size_t num_coalesces;
} mem_stats_t;

mem_stats_t mem_get_stats(void);

#endif /* ALLOCATOR_H */
