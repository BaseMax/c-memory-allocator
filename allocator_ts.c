#define _GNU_SOURCE
#include "allocator.h"
#include <pthread.h>

/* Global mutex for thread-safe operations */
static pthread_mutex_t allocator_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Thread-safe malloc */
void* mem_malloc_ts(size_t size) {
    pthread_mutex_lock(&allocator_mutex);
    void* ptr = mem_malloc(size);
    pthread_mutex_unlock(&allocator_mutex);
    return ptr;
}

/* Thread-safe free */
void mem_free_ts(void* ptr) {
    pthread_mutex_lock(&allocator_mutex);
    mem_free(ptr);
    pthread_mutex_unlock(&allocator_mutex);
}

/* Thread-safe calloc */
void* mem_calloc_ts(size_t nmemb, size_t size) {
    pthread_mutex_lock(&allocator_mutex);
    void* ptr = mem_calloc(nmemb, size);
    pthread_mutex_unlock(&allocator_mutex);
    return ptr;
}

/* Thread-safe realloc */
void* mem_realloc_ts(void* ptr, size_t size) {
    pthread_mutex_lock(&allocator_mutex);
    void* new_ptr = mem_realloc(ptr, size);
    pthread_mutex_unlock(&allocator_mutex);
    return new_ptr;
}
