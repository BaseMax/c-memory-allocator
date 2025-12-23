# Architecture and Design Documentation

## Table of Contents

1. [Overview](#overview)
2. [Design Goals](#design-goals)
3. [Memory Acquisition Strategy](#memory-acquisition-strategy)
4. [Segregated Free Lists](#segregated-free-lists)
5. [Block Management](#block-management)
6. [Allocation Algorithm](#allocation-algorithm)
7. [Deallocation Algorithm](#deallocation-algorithm)
8. [Thread Safety](#thread-safety)
9. [Performance Characteristics](#performance-characteristics)
10. [Design Trade-offs](#design-trade-offs)

## Overview

This custom memory allocator is a production-quality replacement for the standard C library's malloc/free/calloc/realloc functions. It demonstrates modern memory allocation techniques used in real-world allocators.

### Key Components

```
┌─────────────────────────────────────────────────────┐
│                  Application Code                    │
└─────────────────────┬───────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────┐
│              Public API (allocator.h)                │
│  mem_malloc() mem_free() mem_calloc() mem_realloc() │
└─────────────────────┬───────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────┐
│          Core Allocator (allocator.c)                │
│  ┌────────────────┐  ┌────────────────┐             │
│  │ Segregated     │  │ Block          │             │
│  │ Free Lists     │  │ Management     │             │
│  │ (10 classes)   │  │ (split/merge)  │             │
│  └────────────────┘  └────────────────┘             │
└─────────────────────┬───────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────┐
│          Memory Acquisition Layer                    │
│  ┌──────────────┐          ┌──────────────┐         │
│  │   brk()      │          │   mmap()     │         │
│  │  Small allocs│          │  Large allocs│         │
│  │  < 128KB     │          │  ≥ 128KB     │         │
│  └──────────────┘          └──────────────┘         │
└─────────────────────────────────────────────────────┘
```

## Design Goals

1. **Performance**: Competitive with system malloc for common use cases
2. **Low Fragmentation**: Minimize both internal and external fragmentation
3. **Predictability**: Consistent performance characteristics
4. **Simplicity**: Easy to understand and maintain
5. **Safety**: Proper bounds checking and error handling
6. **Debuggability**: Clear statistics and Valgrind compatibility

## Memory Acquisition Strategy

### Two-Tier Approach

The allocator uses different strategies based on allocation size:

#### Small Allocations (< 128KB)

Uses `brk()` (via `sbrk()`) to expand the heap:

**Advantages:**
- Fast: Simple pointer arithmetic
- Good locality: Heap is contiguous
- Low overhead: No system call per allocation

**Disadvantages:**
- Can't release memory in the middle
- Potential for fragmentation at heap level

**Implementation:**
```c
void* expand_heap(size_t size) {
    size_t alloc_size = size < BRK_INCREMENT ? BRK_INCREMENT : align_size(size);
    void* old_brk = sbrk(0);
    void* new_brk = sbrk(alloc_size);
    // ... create block header ...
}
```

#### Large Allocations (≥ 128KB)

Uses `mmap()` for direct memory mapping:

**Advantages:**
- Can release immediately with `munmap()`
- No heap fragmentation
- Memory is zero-initialized by kernel

**Disadvantages:**
- System call overhead
- TLB pressure with many mappings

**Implementation:**
```c
void* ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
```

### Threshold Selection

The 128KB threshold is chosen based on:
- Typical memory page size (4KB) × 32
- Balance between system call overhead and fragmentation
- Common allocation patterns in C programs

## Segregated Free Lists

### Size Classes

The allocator maintains 10 separate free lists:

| Class | Size Range     | Target Use Cases                    |
|-------|----------------|-------------------------------------|
| 0     | ≤ 32 B         | Tiny objects, small strings         |
| 1     | ≤ 64 B         | Small structures, short strings     |
| 2     | ≤ 128 B        | Cache-line sized objects            |
| 3     | ≤ 256 B        | Medium structures                   |
| 4     | ≤ 512 B        | Small buffers                       |
| 5     | ≤ 1 KB         | Typical buffers                     |
| 6     | ≤ 2 KB         | Network packets, file I/O           |
| 7     | ≤ 4 KB         | Page-sized buffers                  |
| 8     | ≤ 8 KB         | Large buffers                       |
| 9     | > 8 KB         | Very large allocations (pre-mmap)   |

### Benefits

1. **Fast Search**: Check appropriate size class first
2. **Reduced Fragmentation**: Similar-sized objects grouped together
3. **Cache Efficiency**: Better spatial locality
4. **Predictable Performance**: O(n) where n is blocks in size class

### Data Structure

```c
typedef struct block_header {
    size_t size;                    // Total block size including header
    struct block_header* next;      // Next in free list
    struct block_header* prev;      // Previous in free list
    int is_free;                    // Allocation status
    int is_mmap;                    // Allocation method
} block_header_t;

static block_header_t* free_lists[NUM_SIZE_CLASSES];
```

## Block Management

### Block Header

Each allocation has a metadata header:

```
┌──────────────────────────────────────┐
│ size (8 bytes)                       │  Total size including header
├──────────────────────────────────────┤
│ next (8 bytes)                       │  Free list linkage
├──────────────────────────────────────┤
│ prev (8 bytes)                       │  Free list linkage
├──────────────────────────────────────┤
│ is_free (4 bytes)                    │  Status flags
├──────────────────────────────────────┤
│ is_mmap (4 bytes)                    │  Allocation method
├══════════════════════════════════════┤
│                                      │
│  User Data                           │
│  (size - sizeof(block_header_t))     │
│                                      │
└──────────────────────────────────────┘
```

**Header Size**: 40 bytes on 64-bit systems

### Alignment

All allocations are aligned to 16-byte boundaries:

```c
static inline size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}
```

**Benefits:**
- SIMD instruction compatibility
- Cache line alignment
- Required by some architectures

### Block Splitting

When allocating from a larger free block:

**Before:**
```
┌────────────────────────────────────────────┐
│ Free Block (size = 1024)                   │
└────────────────────────────────────────────┘
```

**After (allocating 128 bytes):**
```
┌──────────────────┐┌───────────────────────┐
│ Allocated (128)  ││ Free (896)            │
└──────────────────┘└───────────────────────┘
```

**Conditions for splitting:**
```c
if (block->size >= total_size + sizeof(block_header_t) + MIN_BLOCK_SIZE)
```

Must have enough space for:
1. Requested allocation + header
2. New header for remainder
3. Minimum usable space (32 bytes)

### Block Coalescing

When freeing a block, merge with adjacent free blocks:

**Before:**
```
┌──────────┐┌──────────┐┌──────────┐┌──────────┐
│ Allocated││  Free    ││  Free    ││ Allocated│
└──────────┘└──────────┘└──────────┘└──────────┘
```

**After (freeing first allocated block):**
```
┌────────────────────────────────────┐┌──────────┐
│ Free (coalesced)                   ││ Allocated│
└────────────────────────────────────┘└──────────┘
```

**Algorithm:**
```c
block_header_t* coalesce(block_header_t* block) {
    void* block_end = (void*)block + block->size;
    block_header_t* next_block = (block_header_t*)block_end;
    
    if (next_block is valid and free) {
        remove_from_free_list(next_block);
        block->size += next_block->size;
        return coalesce(block);  // Recursive
    }
    return block;
}
```

## Allocation Algorithm

### mem_malloc(size) Flow

```
START
  │
  ├─ size == 0? ─→ return NULL
  │
  ├─ Align size and add header overhead
  │
  ├─ size >= MMAP_THRESHOLD?
  │   │
  │   YES ─→ mmap() ─→ return ptr + header_size
  │   │
  │   NO
  │   │
  │   ├─ Find free block in appropriate size class
  │   │
  │   ├─ Found?
  │   │   │
  │   │   NO ─→ expand_heap() ─→ Get new block
  │   │   │
  │   │   YES ─→ Use existing block
  │   │
  │   ├─ Remove from free list
  │   │
  │   ├─ Split if too large
  │   │
  │   ├─ Mark as allocated
  │   │
  │   └─ return ptr + header_size
```

### Search Strategy

1. Calculate size class: `class = get_size_class(size)`
2. Search from `class` to `NUM_SIZE_CLASSES - 1`
3. Return first block where `block->size >= size`
4. If no block found, expand heap

### Best Fit vs First Fit

This allocator uses **First Fit** with segregated lists:

**Advantages:**
- Faster: O(1) to O(n) where n is small
- Simpler implementation
- Segregation provides implicit "good fit"

**Disadvantages:**
- May not find optimal block
- Can cause fragmentation in large class

## Deallocation Algorithm

### mem_free(ptr) Flow

```
START
  │
  ├─ ptr == NULL? ─→ return
  │
  ├─ Get block header: block = ptr - sizeof(header)
  │
  ├─ is_mmap?
  │   │
  │   YES ─→ munmap(block, block->size) ─→ return
  │   │
  │   NO
  │   │
  │   ├─ Mark as free
  │   │
  │   ├─ Coalesce with adjacent free blocks
  │   │
  │   └─ Add to appropriate free list
```

### Coalescing Direction

Current implementation: **Forward coalescing only**

- Merges with blocks that come after in memory
- Simpler than bidirectional
- Still effective at reducing fragmentation

**Future improvement**: Bidirectional coalescing
- Would require footer in each block
- +8 bytes overhead per allocation
- Better fragmentation reduction

## Thread Safety

### Thread-Unsafe Version (Default)

```c
void* mem_malloc(size_t size) {
    // Direct implementation
    // No locking
}
```

**Use when:**
- Single-threaded applications
- Manual synchronization at higher level
- Performance is critical

### Thread-Safe Version

```c
static pthread_mutex_t allocator_mutex = PTHREAD_MUTEX_INITIALIZER;

void* mem_malloc_ts(size_t size) {
    pthread_mutex_lock(&allocator_mutex);
    void* ptr = mem_malloc(size);
    pthread_mutex_unlock(&allocator_mutex);
    return ptr;
}
```

**Characteristics:**
- Global mutex: Simple but coarse-grained
- All allocations are serialized
- ~10-20% performance overhead

**Future improvement**: Per-thread caches
- Thread-local free lists
- Reduce contention
- Complex implementation

## Performance Characteristics

### Time Complexity

| Operation | Best Case | Average Case | Worst Case |
|-----------|-----------|--------------|------------|
| malloc    | O(1)      | O(n)         | O(n)       |
| free      | O(1)      | O(1)         | O(1)       |
| calloc    | O(n)      | O(n)         | O(n)       |
| realloc   | O(1)      | O(n)         | O(n)       |

Where n = number of free blocks in searched size classes

### Space Complexity

**Overhead per allocation:**
- Header: 40 bytes
- Alignment: up to 15 bytes
- Total: ~40-55 bytes

**Efficiency:**
- Small allocations (64B): ~60-85% efficient
- Medium allocations (1KB): ~95% efficient  
- Large allocations (>8KB): ~99% efficient

### Fragmentation

**Internal fragmentation:**
- Minimized by splitting
- Alignment padding: ≤15 bytes
- Header overhead: Fixed 40 bytes

**External fragmentation:**
- Reduced by coalescing
- Segregated bins help
- Can accumulate in long-running programs

## Design Trade-offs

### 1. Segregated Bins vs Single Free List

**Chosen: Segregated Bins**

Pros:
- Faster allocation (smaller search space)
- Better locality
- Implicit size-based best-fit

Cons:
- More complex
- 10 pointers of overhead
- Possible fragmentation between classes

### 2. Forward-Only vs Bidirectional Coalescing

**Chosen: Forward-Only**

Pros:
- Simpler implementation
- Less memory overhead (no footer)
- Still reduces most fragmentation

Cons:
- Misses some coalescing opportunities
- Can't merge with previous block

### 3. Global Mutex vs Fine-Grained Locking

**Chosen: Global Mutex (for thread-safe version)**

Pros:
- Simple implementation
- No deadlock risk
- Easy to reason about

Cons:
- Serializes all operations
- Scalability bottleneck
- Not optimal for many threads

### 4. mmap Threshold: 128KB

**Rationale:**
- System call overhead amortized
- Page size × 32 (good for most systems)
- Avoids heap fragmentation for large objects

**Trade-off:**
- Lower threshold: Less heap fragmentation, more system calls
- Higher threshold: Fewer system calls, more heap fragmentation

### 5. Block Header Size: 40 bytes

**Could be reduced to 24 bytes:**
```c
struct {
    size_t size;     // 8 bytes
    void* next;      // 8 bytes
    uint32_t flags;  // 4 bytes (is_free, is_mmap, etc.)
    uint32_t pad;    // 4 bytes (alignment)
};  // 24 bytes
```

**Kept at 40 bytes for:**
- Clearer code
- Easier debugging
- Double-linked lists (easier removal)

## Summary

This allocator demonstrates key concepts used in production allocators:

1. **Hybrid acquisition**: brk for small, mmap for large
2. **Segregated storage**: Multiple size classes
3. **Defragmentation**: Splitting and coalescing
4. **Thread safety**: Optional mutex protection
5. **Performance**: Competitive with system malloc

The design prioritizes clarity and correctness over maximum performance, making it an excellent educational example and a solid foundation for further optimization.
