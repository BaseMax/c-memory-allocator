#define _GNU_SOURCE
#include "allocator.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

/* Configuration constants */
#define MIN_BLOCK_SIZE 32
#define ALIGNMENT 16
#define NUM_SIZE_CLASSES 10
#define MMAP_THRESHOLD (128 * 1024)  /* Use mmap for allocations > 128KB */
#define BRK_INCREMENT (64 * 1024)    /* Grow heap by 64KB chunks */

/* Block header structure */
typedef struct block_header {
    size_t size;                    /* Size of block (including header) */
    struct block_header* next;      /* Next block in free list */
    struct block_header* prev;      /* Previous block in free list */
    int is_free;                    /* 1 if free, 0 if allocated */
    int is_mmap;                    /* 1 if allocated via mmap */
} block_header_t;

/* Segregated free lists - bins for different size classes */
static block_header_t* free_lists[NUM_SIZE_CLASSES] = {NULL};

/* Statistics */
static mem_stats_t stats = {0};

/* Heap boundaries */
static void* heap_start = NULL;
static void* heap_end = NULL;

/* Helper function: Align size to ALIGNMENT boundary */
static inline size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

/* Helper function: Get size class index for a given size */
static int get_size_class(size_t size) {
    if (size <= 32) return 0;
    if (size <= 64) return 1;
    if (size <= 128) return 2;
    if (size <= 256) return 3;
    if (size <= 512) return 4;
    if (size <= 1024) return 5;
    if (size <= 2048) return 6;
    if (size <= 4096) return 7;
    if (size <= 8192) return 8;
    return 9;  /* Large allocations */
}

/* Remove block from free list */
static void remove_from_free_list(block_header_t* block) {
    int class_idx = get_size_class(block->size);
    
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        free_lists[class_idx] = block->next;
    }
    
    if (block->next) {
        block->next->prev = block->prev;
    }
    
    block->next = NULL;
    block->prev = NULL;
}

/* Add block to free list */
static void add_to_free_list(block_header_t* block) {
    int class_idx = get_size_class(block->size);
    
    block->next = free_lists[class_idx];
    block->prev = NULL;
    
    if (free_lists[class_idx]) {
        free_lists[class_idx]->prev = block;
    }
    
    free_lists[class_idx] = block;
    block->is_free = 1;
}

/* Coalesce adjacent free blocks */
static block_header_t* coalesce(block_header_t* block) {
    if (!block || block->is_mmap) {
        return block;
    }
    
    void* block_end = (void*)block + block->size;
    
    /* Check if next block exists within heap bounds */
    if ((void*)block_end >= heap_end || (void*)block_end < heap_start) {
        return block;
    }
    
    block_header_t* next_block = (block_header_t*)block_end;
    
    /* Additional safety check: ensure next block is within valid range */
    if ((void*)next_block + sizeof(block_header_t) > heap_end) {
        return block;
    }
    
    /* Check if next block is free */
    if (next_block->is_free && !next_block->is_mmap) {
        /* Coalesce with next block */
        remove_from_free_list(next_block);
        block->size += next_block->size;
        stats.num_coalesces++;
        
        /* Recursively coalesce */
        return coalesce(block);
    }
    
    return block;
}

/* Split block if it's too large */
static void split_block(block_header_t* block, size_t size) {
    size_t total_size = align_size(size + sizeof(block_header_t));
    
    if (block->size >= total_size + sizeof(block_header_t) + MIN_BLOCK_SIZE) {
        /* Create new free block from remainder */
        block_header_t* new_block = (block_header_t*)((void*)block + total_size);
        new_block->size = block->size - total_size;
        new_block->is_free = 1;
        new_block->is_mmap = 0;
        new_block->next = NULL;
        new_block->prev = NULL;
        
        block->size = total_size;
        
        add_to_free_list(new_block);
        stats.num_splits++;
    }
}

/* Expand heap using brk */
static void* expand_heap(size_t size) {
    size_t alloc_size = size < BRK_INCREMENT ? BRK_INCREMENT : align_size(size);
    
    void* old_brk = sbrk(0);
    if (old_brk == (void*)-1) {
        return NULL;
    }
    
    void* new_brk = sbrk(alloc_size);
    if (new_brk == (void*)-1) {
        return NULL;
    }
    
    if (heap_start == NULL) {
        heap_start = old_brk;
    }
    heap_end = sbrk(0);
    
    /* Create new free block */
    block_header_t* block = (block_header_t*)old_brk;
    block->size = alloc_size;
    block->is_free = 1;
    block->is_mmap = 0;
    block->next = NULL;
    block->prev = NULL;
    
    return block;
}

/* Find suitable free block */
static block_header_t* find_free_block(size_t size) {
    int start_class = get_size_class(size);
    
    /* Search in appropriate size class and larger ones */
    for (int i = start_class; i < NUM_SIZE_CLASSES; i++) {
        block_header_t* current = free_lists[i];
        
        while (current) {
            if (current->is_free && current->size >= size) {
                return current;
            }
            current = current->next;
        }
    }
    
    return NULL;
}

/* Thread-unsafe malloc implementation */
void* mem_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    size_t total_size = align_size(size + sizeof(block_header_t));
    block_header_t* block;
    
    /* Use mmap for large allocations */
    if (total_size >= MMAP_THRESHOLD) {
        void* ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            return NULL;
        }
        
        block = (block_header_t*)ptr;
        block->size = total_size;
        block->is_free = 0;
        block->is_mmap = 1;
        block->next = NULL;
        block->prev = NULL;
        
        stats.total_allocated += total_size;
        stats.current_usage += total_size;
        stats.num_allocations++;
        
        return (void*)((char*)block + sizeof(block_header_t));
    }
    
    /* Try to find free block */
    block = find_free_block(total_size);
    
    if (!block) {
        /* No suitable free block, expand heap */
        block = expand_heap(total_size);
        if (!block) {
            return NULL;
        }
    }
    
    /* Remove from free list */
    remove_from_free_list(block);
    
    /* Split if block is too large */
    split_block(block, size);
    
    block->is_free = 0;
    
    stats.total_allocated += block->size;
    stats.current_usage += block->size;
    stats.num_allocations++;
    
    return (void*)((char*)block + sizeof(block_header_t));
}

/* Thread-unsafe free implementation */
void mem_free(void* ptr) {
    if (!ptr) {
        return;
    }
    
    block_header_t* block = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    
    if (block->is_mmap) {
        /* Unmap large allocation */
        stats.total_freed += block->size;
        stats.current_usage -= block->size;
        stats.num_frees++;
        munmap(block, block->size);
        return;
    }
    
    stats.total_freed += block->size;
    stats.current_usage -= block->size;
    stats.num_frees++;
    
    /* Coalesce with adjacent free blocks */
    block->is_free = 1;
    block = coalesce(block);
    
    /* Add to appropriate free list */
    add_to_free_list(block);
}

/* Thread-unsafe calloc implementation */
void* mem_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    
    /* Check for overflow */
    size_t total = nmemb * size;
    if (total / nmemb != size) {
        return NULL;  /* Overflow */
    }
    
    void* ptr = mem_malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    
    return ptr;
}

/* Thread-unsafe realloc implementation */
void* mem_realloc(void* ptr, size_t size) {
    if (!ptr) {
        return mem_malloc(size);
    }
    
    if (size == 0) {
        mem_free(ptr);
        return NULL;
    }
    
    block_header_t* block = (block_header_t*)((char*)ptr - sizeof(block_header_t));
    size_t old_size = block->size - sizeof(block_header_t);
    
    if (old_size >= size) {
        /* Current block is large enough */
        return ptr;
    }
    
    /* Allocate new block and copy data */
    void* new_ptr = mem_malloc(size);
    if (!new_ptr) {
        return NULL;
    }
    
    memcpy(new_ptr, ptr, old_size);
    mem_free(ptr);
    
    return new_ptr;
}

/* Get statistics */
mem_stats_t mem_get_stats(void) {
    return stats;
}

/* Print statistics */
void mem_print_stats(void) {
    printf("Memory Allocator Statistics:\n");
    printf("  Total allocated: %zu bytes\n", stats.total_allocated);
    printf("  Total freed: %zu bytes\n", stats.total_freed);
    printf("  Current usage: %zu bytes\n", stats.current_usage);
    printf("  Number of allocations: %zu\n", stats.num_allocations);
    printf("  Number of frees: %zu\n", stats.num_frees);
    printf("  Number of splits: %zu\n", stats.num_splits);
    printf("  Number of coalesces: %zu\n", stats.num_coalesces);
}

/* Reset allocator state (for testing) */
void mem_reset(void) {
    /* Reset statistics */
    memset(&stats, 0, sizeof(stats));
    
    /* Clear free lists */
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        free_lists[i] = NULL;
    }
    
    /* Note: We don't reset heap_start/heap_end as brk() is global */
}
