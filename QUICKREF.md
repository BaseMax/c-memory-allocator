# Quick Reference Guide

## Building

```bash
make all          # Build everything
make clean        # Clean build artifacts
make run-test     # Run tests
make run-bench    # Run benchmarks
make run-example  # Run example program
make help         # Show all targets
```

## Basic Usage

```c
#include "allocator.h"

// Allocate
void* ptr = mem_malloc(1024);

// Free
mem_free(ptr);

// Allocate and zero-initialize
int* arr = mem_calloc(100, sizeof(int));

// Resize
ptr = mem_realloc(ptr, 2048);
```

## Thread-Safe Versions

```c
// Use _ts suffix for thread-safe functions
void* ptr = mem_malloc_ts(1024);
mem_free_ts(ptr);
```

## Statistics

```c
// Print statistics
mem_print_stats();

// Get statistics programmatically
mem_stats_t stats = mem_get_stats();
printf("Current usage: %zu bytes\n", stats.current_usage);
```

## Function Summary

| Function | Description | Thread-Safe |
|----------|-------------|-------------|
| `mem_malloc(size)` | Allocate memory | No |
| `mem_free(ptr)` | Free memory | No |
| `mem_calloc(n, size)` | Allocate and zero | No |
| `mem_realloc(ptr, size)` | Resize allocation | No |
| `mem_malloc_ts(size)` | Allocate memory | Yes |
| `mem_free_ts(ptr)` | Free memory | Yes |
| `mem_calloc_ts(n, size)` | Allocate and zero | Yes |
| `mem_realloc_ts(ptr, size)` | Resize allocation | Yes |
| `mem_print_stats()` | Print statistics | - |
| `mem_get_stats()` | Get statistics | - |

## Key Constants

```c
MIN_BLOCK_SIZE    32        // Minimum block size
ALIGNMENT         16        // Memory alignment
NUM_SIZE_CLASSES  10        // Number of free lists
MMAP_THRESHOLD    131072    // 128KB - use mmap above this
BRK_INCREMENT     65536     // 64KB - heap growth size
```

## Size Classes

| Class | Max Size | Typical Use |
|-------|----------|-------------|
| 0 | 32 B | Tiny objects |
| 1 | 64 B | Small structs |
| 2 | 128 B | Cache-line sized |
| 3 | 256 B | Medium structs |
| 4 | 512 B | Small buffers |
| 5 | 1 KB | Typical buffers |
| 6 | 2 KB | Network packets |
| 7 | 4 KB | Page-sized |
| 8 | 8 KB | Large buffers |
| 9 | >8 KB | Very large |

## Common Patterns

### Error Checking
```c
void* ptr = mem_malloc(size);
if (!ptr) {
    fprintf(stderr, "Allocation failed\n");
    return -1;
}
```

### Safe Realloc
```c
void* new_ptr = mem_realloc(old_ptr, new_size);
if (!new_ptr && new_size != 0) {
    mem_free(old_ptr);
    return -1;
}
old_ptr = new_ptr;
```

### Array Allocation
```c
int* arr = mem_calloc(count, sizeof(int));
// All elements initialized to 0
```

### String Allocation
```c
size_t len = strlen(str) + 1;
char* copy = mem_malloc(len);
strcpy(copy, str);
```

## Valgrind Testing

```bash
# Test for memory leaks
make valgrind

# Run benchmarks with Valgrind
make valgrind-bench
```

## Debugging Tips

1. **Check return values**: Always check if allocation succeeded
2. **Use statistics**: Call `mem_print_stats()` to track usage
3. **Set to NULL**: After freeing, set pointer to NULL
4. **Valgrind**: Run with Valgrind to catch errors
5. **Match calls**: Ensure every malloc has a corresponding free

## Performance Tips

1. **Batch allocations**: Allocate once, reuse many times
2. **Right-size**: Don't over-allocate
3. **Thread-unsafe**: Use non-_ts versions when possible
4. **Large allocs**: >128KB allocations use mmap (fast to free)
5. **Zero-init**: Use calloc instead of malloc+memset

## Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| Segmentation fault | Use after free | Don't use after calling free |
| Memory leak | Missing free | Free all allocations |
| Double free | Freeing twice | Set to NULL after free |
| Buffer overflow | Writing beyond size | Allocate enough space |
| NULL pointer | Allocation failed | Check return value |

## Files

```
allocator.h       - Public API header
allocator.c       - Core implementation
allocator_ts.c    - Thread-safe wrappers
test.c            - Test suite
benchmark.c       - Performance benchmarks
example.c         - Usage examples
Makefile          - Build configuration
README.md         - Main documentation
ARCHITECTURE.md   - Design documentation
API.md            - Detailed API reference
VALGRIND.md       - Valgrind guide
```

## Links

- **Main docs**: README.md
- **API reference**: API.md
- **Architecture**: ARCHITECTURE.md
- **Valgrind guide**: VALGRIND.md
