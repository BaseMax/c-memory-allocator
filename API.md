# API Documentation

## Overview

The custom memory allocator provides a complete replacement for the standard C library memory allocation functions. This document describes each function in detail, including parameters, return values, error conditions, and usage examples.

## Header File

```c
#include "allocator.h"
```

## Core Functions (Thread-Unsafe)

These functions provide the fastest performance for single-threaded applications or when external synchronization is used.

---

### mem_malloc

**Signature:**
```c
void* mem_malloc(size_t size);
```

**Description:**  
Allocates `size` bytes of memory and returns a pointer to the allocated memory. The memory is not initialized.

**Parameters:**
- `size`: Number of bytes to allocate

**Return Value:**
- Success: Pointer to allocated memory (aligned to 16-byte boundary)
- Failure: `NULL` if size is 0 or allocation fails

**Memory Source:**
- Allocations < 128KB: Uses `brk()` to expand heap
- Allocations ≥ 128KB: Uses `mmap()` for direct mapping

**Example:**
```c
char* buffer = mem_malloc(1024);
if (buffer == NULL) {
    fprintf(stderr, "Allocation failed\n");
    return -1;
}
// Use buffer...
mem_free(buffer);
```

**Notes:**
- Memory is uninitialized; use `mem_calloc()` for zero-initialized memory
- Returned pointer is suitable for any data type (properly aligned)
- Overhead: ~40 bytes per allocation for metadata

---

### mem_free

**Signature:**
```c
void mem_free(void* ptr);
```

**Description:**  
Frees the memory space pointed to by `ptr`, which must have been returned by a previous call to `mem_malloc()`, `mem_calloc()`, or `mem_realloc()`.

**Parameters:**
- `ptr`: Pointer to memory to free, or `NULL`

**Return Value:**
- None

**Behavior:**
- If `ptr` is `NULL`, no operation is performed
- If `ptr` was allocated via mmap, calls `munmap()` to unmap
- If `ptr` was allocated via brk, marks block as free and coalesces with adjacent free blocks
- Adds freed block to appropriate free list for reuse

**Example:**
```c
void* data = mem_malloc(256);
// Use data...
mem_free(data);
data = NULL;  // Good practice to prevent use-after-free
```

**Warnings:**
- Freeing the same pointer twice is undefined behavior
- Freeing a pointer not returned by this allocator is undefined behavior
- Accessing memory after freeing it is undefined behavior

---

### mem_calloc

**Signature:**
```c
void* mem_calloc(size_t nmemb, size_t size);
```

**Description:**  
Allocates memory for an array of `nmemb` elements of `size` bytes each and returns a pointer to the allocated memory. The memory is set to zero.

**Parameters:**
- `nmemb`: Number of elements
- `size`: Size of each element in bytes

**Return Value:**
- Success: Pointer to zero-initialized allocated memory
- Failure: `NULL` if either parameter is 0, multiplication would overflow, or allocation fails

**Safety Features:**
- Checks for integer overflow in `nmemb * size`
- Returns `NULL` instead of allocating wrong size on overflow

**Example:**
```c
int* array = mem_calloc(100, sizeof(int));
if (array == NULL) {
    fprintf(stderr, "Calloc failed\n");
    return -1;
}
// All elements are initialized to 0
for (int i = 0; i < 100; i++) {
    assert(array[i] == 0);
}
mem_free(array);
```

**Overflow Protection:**
```c
// This will safely return NULL instead of allocating wrong size
size_t huge = (size_t)-1;
void* ptr = mem_calloc(huge, 2);  // Returns NULL
```

---

### mem_realloc

**Signature:**
```c
void* mem_realloc(void* ptr, size_t size);
```

**Description:**  
Changes the size of the memory block pointed to by `ptr` to `size` bytes. The contents are unchanged in the range from the start of the region up to the minimum of the old and new sizes.

**Parameters:**
- `ptr`: Pointer to previously allocated memory, or `NULL`
- `size`: New size in bytes

**Return Value:**
- Success: Pointer to reallocated memory (may be different from `ptr`)
- Failure: `NULL` if allocation fails (original block unchanged)

**Special Cases:**
- If `ptr` is `NULL`, equivalent to `mem_malloc(size)`
- If `size` is 0, equivalent to `mem_free(ptr)` and returns `NULL`
- If new size is smaller than old size, returns same pointer
- If new size is larger, may allocate new block and copy data

**Example:**
```c
char* str = mem_malloc(50);
strcpy(str, "Hello");

// Need more space
str = mem_realloc(str, 100);
if (str == NULL) {
    fprintf(stderr, "Realloc failed\n");
    return -1;
}
strcat(str, ", World!");

mem_free(str);
```

**Important:**
- Original pointer may be invalid after realloc (don't use old pointer)
- If realloc fails, original block is still valid
- Always check return value before using

**Pattern for safety:**
```c
void* new_ptr = mem_realloc(old_ptr, new_size);
if (new_ptr == NULL && new_size != 0) {
    // Realloc failed, old_ptr is still valid
    mem_free(old_ptr);
    return -1;
}
old_ptr = new_ptr;
```

---

## Thread-Safe Functions

These functions are protected by a global mutex and safe to call from multiple threads simultaneously.

### mem_malloc_ts

**Signature:**
```c
void* mem_malloc_ts(size_t size);
```

**Description:**  
Thread-safe version of `mem_malloc()`. Protected by global mutex.

**Performance:**
- ~10-20% slower than `mem_malloc()` due to mutex overhead
- Serializes all allocations across threads

**Example:**
```c
// In multithreaded code
void* worker_thread(void* arg) {
    void* data = mem_malloc_ts(1024);
    // Safe to call from multiple threads
    mem_free_ts(data);
    return NULL;
}
```

---

### mem_free_ts

**Signature:**
```c
void mem_free_ts(void* ptr);
```

**Description:**  
Thread-safe version of `mem_free()`. Protected by global mutex.

---

### mem_calloc_ts

**Signature:**
```c
void* mem_calloc_ts(size_t nmemb, size_t size);
```

**Description:**  
Thread-safe version of `mem_calloc()`. Protected by global mutex.

---

### mem_realloc_ts

**Signature:**
```c
void* mem_realloc_ts(void* ptr, size_t size);
```

**Description:**  
Thread-safe version of `mem_realloc()`. Protected by global mutex.

---

## Utility Functions

### mem_print_stats

**Signature:**
```c
void mem_print_stats(void);
```

**Description:**  
Prints current allocator statistics to stdout.

**Output Example:**
```
Memory Allocator Statistics:
  Total allocated: 1048576 bytes
  Total freed: 524288 bytes
  Current usage: 524288 bytes
  Number of allocations: 1000
  Number of frees: 500
  Number of splits: 250
  Number of coalesces: 125
```

**Use Cases:**
- Debugging memory usage
- Performance analysis
- Memory leak detection

**Example:**
```c
// At program exit
mem_print_stats();
```

---

### mem_get_stats

**Signature:**
```c
mem_stats_t mem_get_stats(void);
```

**Description:**  
Returns a structure containing current allocator statistics.

**Return Value:**
```c
typedef struct {
    size_t total_allocated;   // Total bytes allocated (lifetime)
    size_t total_freed;        // Total bytes freed (lifetime)
    size_t current_usage;      // Currently allocated bytes
    size_t num_allocations;    // Number of allocation calls
    size_t num_frees;          // Number of free calls
    size_t num_splits;         // Number of block splits
    size_t num_coalesces;      // Number of block coalesces
} mem_stats_t;
```

**Example:**
```c
mem_stats_t stats = mem_get_stats();
printf("Memory usage: %zu bytes\n", stats.current_usage);
printf("Fragmentation operations: %zu splits, %zu coalesces\n",
       stats.num_splits, stats.num_coalesces);

// Check for memory leaks
if (stats.num_allocations != stats.num_frees) {
    fprintf(stderr, "Warning: %zu allocations without free\n",
            stats.num_allocations - stats.num_frees);
}
```

---

### mem_reset

**Signature:**
```c
void mem_reset(void);
```

**Description:**  
Resets allocator statistics to zero. Primarily for testing purposes.

**Warning:**
- Does NOT free allocated memory
- Does NOT reset heap state
- Only resets internal counters
- Should only be used in test code

**Example:**
```c
void test_allocator(void) {
    mem_reset();  // Clear previous statistics
    
    void* ptr = mem_malloc(100);
    mem_free(ptr);
    
    mem_stats_t stats = mem_get_stats();
    assert(stats.num_allocations == 1);
    assert(stats.num_frees == 1);
}
```

---

## Error Handling

### NULL Returns

All allocation functions return `NULL` on failure:

```c
void* ptr = mem_malloc(1024);
if (ptr == NULL) {
    // Handle error
    fprintf(stderr, "Out of memory\n");
    return -1;
}
```

### Common Failure Causes

1. **Out of memory**: System has no more memory available
2. **Size overflow**: Calloc parameters would overflow
3. **System call failure**: brk() or mmap() failed
4. **Size is zero**: Requesting 0 bytes

---

## Best Practices

### 1. Always Check Return Values

```c
void* ptr = mem_malloc(size);
if (ptr == NULL) {
    // Handle error
    return -1;
}
```

### 2. Free All Allocations

```c
void* ptr = mem_malloc(1024);
// ... use ptr ...
mem_free(ptr);
```

### 3. Set Pointers to NULL After Free

```c
mem_free(ptr);
ptr = NULL;  // Prevents use-after-free
```

### 4. Check Realloc Carefully

```c
void* new_ptr = mem_realloc(ptr, new_size);
if (new_ptr == NULL) {
    // ptr is still valid
    mem_free(ptr);
    return -1;
}
ptr = new_ptr;
```

### 5. Use Calloc for Arrays

```c
// Prefer this
int* arr = mem_calloc(100, sizeof(int));

// Over this
int* arr = mem_malloc(100 * sizeof(int));
memset(arr, 0, 100 * sizeof(int));
```

### 6. Choose Thread-Safe vs Thread-Unsafe Appropriately

```c
// Single-threaded or externally synchronized
void* ptr = mem_malloc(size);

// Multi-threaded without external sync
void* ptr = mem_malloc_ts(size);
```

---

## Common Pitfalls

### 1. Memory Leaks

```c
// BAD: Memory leak
void* ptr = mem_malloc(1024);
ptr = mem_malloc(2048);  // Lost reference to first allocation

// GOOD: Free before reassigning
mem_free(ptr);
ptr = mem_malloc(2048);
```

### 2. Use After Free

```c
// BAD: Use after free
void* ptr = mem_malloc(100);
mem_free(ptr);
memset(ptr, 0, 100);  // Undefined behavior!

// GOOD: Don't use after free
void* ptr = mem_malloc(100);
memset(ptr, 0, 100);
mem_free(ptr);
```

### 3. Double Free

```c
// BAD: Double free
void* ptr = mem_malloc(100);
mem_free(ptr);
mem_free(ptr);  // Undefined behavior!

// GOOD: Set to NULL
void* ptr = mem_malloc(100);
mem_free(ptr);
ptr = NULL;
mem_free(ptr);  // Safe: free(NULL) is a no-op
```

### 4. Buffer Overflow

```c
// BAD: Writing beyond allocated size
char* str = mem_malloc(10);
strcpy(str, "This is a very long string");  // Buffer overflow!

// GOOD: Allocate enough space
size_t len = strlen("This is a very long string") + 1;
char* str = mem_malloc(len);
strcpy(str, "This is a very long string");
```

---

## Performance Tips

### 1. Allocate Once, Reuse

```c
// Better: Allocate once
char* buffer = mem_malloc(1024);
for (int i = 0; i < 1000; i++) {
    use_buffer(buffer);
}
mem_free(buffer);

// Worse: Allocate repeatedly
for (int i = 0; i < 1000; i++) {
    char* buffer = mem_malloc(1024);
    use_buffer(buffer);
    mem_free(buffer);
}
```

### 2. Use Appropriate Sizes

```c
// Good: Allocate what you need
void* ptr = mem_malloc(actual_size);

// Wasteful: Over-allocating
void* ptr = mem_malloc(actual_size * 10);
```

### 3. Prefer Calloc for Zero-Initialization

```c
// More efficient
int* arr = mem_calloc(100, sizeof(int));

// Less efficient  
int* arr = mem_malloc(100 * sizeof(int));
memset(arr, 0, 100 * sizeof(int));
```

---

## Compatibility

### Standard C Library Compatibility

Functions are designed to be compatible with standard C library:

| Custom Allocator | Standard C Library |
|------------------|-------------------|
| `mem_malloc()`   | `malloc()`        |
| `mem_free()`     | `free()`          |
| `mem_calloc()`   | `calloc()`        |
| `mem_realloc()`  | `realloc()`       |

### Important Differences

1. **Do not mix**: Don't free `malloc()` memory with `mem_free()` or vice versa
2. **No LD_PRELOAD**: This doesn't replace system malloc globally
3. **Custom statistics**: Provides additional statistics not in standard library

---

## Example: Complete Program

```c
#include <stdio.h>
#include <string.h>
#include "allocator.h"

int main(void) {
    // Allocate buffer
    char* buffer = mem_malloc(100);
    if (!buffer) {
        return 1;
    }
    
    strcpy(buffer, "Hello, World!");
    printf("%s\n", buffer);
    
    // Resize
    buffer = mem_realloc(buffer, 200);
    if (!buffer) {
        return 1;
    }
    
    strcat(buffer, " - Extended!");
    printf("%s\n", buffer);
    
    // Allocate array
    int* numbers = mem_calloc(10, sizeof(int));
    if (!numbers) {
        mem_free(buffer);
        return 1;
    }
    
    for (int i = 0; i < 10; i++) {
        numbers[i] = i * i;
    }
    
    // Print statistics
    mem_print_stats();
    
    // Clean up
    mem_free(buffer);
    mem_free(numbers);
    
    return 0;
}
```

Compile and run:
```bash
gcc -o myprogram myprogram.c -L. -lallocator -pthread
./myprogram
```
