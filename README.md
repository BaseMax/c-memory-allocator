# Custom Memory Allocator in C

> Custom Memory Allocator implementation demonstrating advanced memory management techniques in C.

A high-performance custom heap memory allocator that replaces the standard `malloc`, `free`, `calloc`, and `realloc` functions. This implementation demonstrates advanced memory management techniques including segregated free lists, block coalescing, and adaptive memory acquisition strategies.

## Features

- **Complete malloc/free/calloc/realloc replacement** - Drop-in compatible API
- **Dual memory acquisition strategy** - Uses `brk()` for small allocations and `mmap()` for large ones
- **Segregated free lists** - 10 size classes for efficient allocation and reduced fragmentation
- **Block splitting and coalescing** - Minimizes external and internal fragmentation
- **Thread-unsafe and thread-safe versions** - Choose performance or safety
- **Comprehensive benchmarks** - Performance comparison with system malloc
- **Valgrind compatible** - Proper memory tracking and leak detection
- **Detailed statistics** - Track allocations, frees, splits, and coalesces

## Architecture

### Allocation Strategy

The allocator uses a hybrid approach for memory acquisition:

1. **Small allocations (< 128KB)**: Uses `brk()` system call to expand the heap
   - More efficient for frequent small allocations
   - Better locality of reference
   - Heap grows in 64KB increments

2. **Large allocations (≥ 128KB)**: Uses `mmap()` for direct memory mapping
   - Avoids heap fragmentation for large blocks
   - Can be unmapped individually
   - Reduces memory waste

### Segregated Free Lists

The allocator maintains 10 size classes to reduce search time and fragmentation:

| Class | Size Range    | Use Case                    |
|-------|---------------|----------------------------|
| 0     | ≤ 32 bytes    | Small objects, metadata     |
| 1     | ≤ 64 bytes    | Small structures            |
| 2     | ≤ 128 bytes   | String buffers              |
| 3     | ≤ 256 bytes   | Medium structures           |
| 4     | ≤ 512 bytes   | Small arrays                |
| 5     | ≤ 1024 bytes  | Medium buffers              |
| 6     | ≤ 2048 bytes  | Large structures            |
| 7     | ≤ 4096 bytes  | Page-sized allocations      |
| 8     | ≤ 8192 bytes  | Multi-page allocations      |
| 9     | > 8192 bytes  | Large allocations           |

### Block Structure

Each allocated block has a header containing:
- Size of the block (including header)
- Pointers to next and previous blocks in free list
- Free/allocated status flag
- mmap allocation flag

```
+------------------+
| Block Header     |
| - size           |
| - next pointer   |
| - prev pointer   |
| - is_free flag   |
| - is_mmap flag   |
+------------------+
| User Data        |
| ...              |
+------------------+
```

### Coalescing

When a block is freed, the allocator checks adjacent blocks and merges them if they are also free. This reduces external fragmentation and makes larger contiguous blocks available for future allocations.

### Splitting

When allocating from a free block that is significantly larger than needed, the allocator splits it into two blocks:
- One block for the requested allocation
- One free block returned to the appropriate free list

Minimum split size ensures we don't create unusably small fragments.

## API Reference

### Thread-Unsafe Functions (Higher Performance)

```c
void* mem_malloc(size_t size);
```
Allocates `size` bytes and returns a pointer to the allocated memory.

```c
void mem_free(void* ptr);
```
Frees the memory space pointed to by `ptr`.

```c
void* mem_calloc(size_t nmemb, size_t size);
```
Allocates memory for an array of `nmemb` elements of `size` bytes each and initializes to zero.

```c
void* mem_realloc(void* ptr, size_t size);
```
Changes the size of the memory block pointed to by `ptr` to `size` bytes.

### Thread-Safe Functions (Mutex Protected)

```c
void* mem_malloc_ts(size_t size);
void mem_free_ts(void* ptr);
void* mem_calloc_ts(size_t nmemb, size_t size);
void* mem_realloc_ts(void* ptr, size_t size);
```

Thread-safe versions of the above functions, protected by a global mutex.

### Utility Functions

```c
void mem_print_stats(void);
```
Prints current allocator statistics to stdout.

```c
mem_stats_t mem_get_stats(void);
```
Returns a structure containing allocator statistics.

```c
void mem_reset(void);
```
Resets allocator statistics (for testing purposes).

## Building

### Prerequisites

- GCC compiler (or compatible)
- GNU Make
- POSIX-compliant system (Linux, macOS, BSD)
- pthread library
- (Optional) Valgrind for memory debugging

### Build Commands

```bash
# Build everything (library, tests, benchmarks)
make all

# Build only the library
make liballocator.a

# Build and run tests
make test

# Build and run benchmarks
make bench

# Clean build artifacts
make clean

# Show available targets
make help
```

## Usage

### Using the Library

1. Include the header in your code:
```c
#include "allocator.h"
```

2. Link against the library:
```bash
gcc -o myprogram myprogram.c -L. -lallocator -pthread
```

3. Use the allocator functions:
```c
void* ptr = mem_malloc(1024);
// Use the memory...
mem_free(ptr);
```

### Example Program

```c
#include <stdio.h>
#include <string.h>
#include "allocator.h"

int main(void) {
    // Allocate memory
    char* str = mem_malloc(100);
    strcpy(str, "Hello, custom allocator!");
    printf("%s\n", str);
    
    // Resize allocation
    str = mem_realloc(str, 200);
    strcat(str, " It works!");
    printf("%s\n", str);
    
    // Free memory
    mem_free(str);
    
    // Print statistics
    mem_print_stats();
    
    return 0;
}
```

## Testing

### Running Tests

```bash
make test
```

The test suite includes:
- Basic allocation/deallocation tests
- Edge case handling (NULL pointers, zero size, overflow)
- Calloc and realloc functionality
- Large allocation tests (mmap)
- Block coalescing verification
- Block splitting verification
- Thread-safe function tests

### Running Benchmarks

```bash
make bench
```

Benchmarks compare:
- Custom allocator vs system malloc
- Different allocation sizes
- Allocation/deallocation patterns
- Fragmentation behavior
- calloc and realloc performance

## Valgrind Compatibility

The allocator is designed to work with Valgrind for memory leak detection and debugging.

### Running with Valgrind

```bash
# Run tests with Valgrind
make valgrind

# Run benchmarks with Valgrind
make valgrind-bench
```

### Valgrind Notes

1. **Leak Detection**: The allocator properly tracks all allocations. Valgrind will report any leaked blocks.

2. **Heap Usage**: Valgrind shows both brk-based and mmap-based allocations.

3. **Invalid Access**: Valgrind detects out-of-bounds access and use-after-free errors.

4. **Performance Impact**: Valgrind significantly slows execution. Benchmark results under Valgrind are not representative of real performance.

5. **Known Behaviors**:
   - Heap blocks from `brk()` may show as "still reachable" at program exit (this is normal)
   - mmap blocks are individually tracked and unmapped
   - Free list pointers are properly maintained

### Example Valgrind Output

```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 0 bytes in 0 blocks
==12345==   total heap usage: 1,000 allocs, 1,000 frees, 256,000 bytes allocated
==12345==
==12345== All heap blocks were freed -- no leaks are possible
```

## Performance Characteristics

### Time Complexity

- **malloc**: O(n) where n is the number of free blocks in the searched size classes
  - Best case: O(1) when suitable block is at head of free list
  - Worst case: O(n) when searching multiple size classes

- **free**: O(1) for coalescing and free list insertion
  - Constant time with adjacent block checking

- **calloc**: O(n) where n is the allocation size (for zeroing)

- **realloc**: O(n) where n is the data size to copy
  - O(1) if current block is large enough

### Space Overhead

- **Block header**: 40 bytes per allocation (on 64-bit systems)
  - Can be optimized for smaller blocks if needed
  
- **Alignment**: 16-byte alignment ensures efficient memory access

- **Internal fragmentation**: Minimized by block splitting

- **External fragmentation**: Minimized by coalescing

### When to Use Thread-Safe vs Thread-Unsafe

**Thread-Unsafe (`mem_malloc`, etc.)**:
- Single-threaded applications
- Performance-critical code with external synchronization
- ~10-20% faster due to no mutex overhead

**Thread-Safe (`mem_malloc_ts`, etc.)**:
- Multi-threaded applications
- Shared memory pools
- When allocations can occur from any thread

## Implementation Details

### Constants

```c
#define MIN_BLOCK_SIZE 32          // Minimum block size for splitting
#define ALIGNMENT 16               // Memory alignment boundary
#define NUM_SIZE_CLASSES 10        // Number of segregated lists
#define MMAP_THRESHOLD (128 * 1024) // Use mmap above this size
#define BRK_INCREMENT (64 * 1024)   // Heap growth increment
```

These can be tuned based on workload characteristics.

### Statistics Tracked

- Total bytes allocated
- Total bytes freed
- Current memory usage
- Number of allocations
- Number of frees
- Number of block splits
- Number of block coalesces

## Limitations and Future Improvements

### Current Limitations

1. **No memory release**: Heap memory from `brk()` is never returned to the OS
2. **Global state**: Not suitable for use in shared libraries (without modifications)
3. **No NUMA awareness**: Assumes uniform memory access
4. **Fixed size classes**: Cannot adapt to workload patterns

### Potential Improvements

1. **Memory unmapping**: Return unused heap pages to OS using `madvise()`
2. **Per-thread caches**: Reduce lock contention in thread-safe version
3. **Adaptive size classes**: Learn from allocation patterns
4. **Better large block handling**: Red-black tree for large free blocks
5. **Memory defragmentation**: Compact heap during idle time
6. **SIMD optimization**: Use vector instructions for block searching
7. **Kernel integration**: Use more advanced system calls like `madvise()`

## Documentation

- **[API Reference](API.md)** - Detailed function documentation
- **[Architecture Guide](ARCHITECTURE.md)** - Design and implementation details
- **[Valgrind Guide](VALGRIND.md)** - Memory debugging and leak detection
- **[Quick Reference](QUICKREF.md)** - Quick command and API reference

## License

Seyyed Ali Mohammadiyeh (MAX BASE)
See LICENSE file for details.
