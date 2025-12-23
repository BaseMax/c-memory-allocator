# Valgrind Compatibility Guide

## Overview

The custom memory allocator is designed to work correctly with Valgrind, the popular memory debugging and profiling tool. This document explains how to use Valgrind with the allocator and what to expect.

## Running with Valgrind

### Basic Memory Check

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test
```

This will run the test suite and check for:
- Memory leaks
- Invalid memory access
- Use of uninitialized values
- Double frees
- Invalid frees

### Running Benchmarks with Valgrind

```bash
valgrind --leak-check=full ./benchmark
```

Note: Benchmarks will run significantly slower under Valgrind (10-50x slower). This is expected behavior.

## Expected Valgrind Output

### Clean Run (No Leaks)

```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 0 bytes in 0 blocks
==12345==   total heap usage: X allocs, X frees, Y bytes allocated
==12345==
==12345== All heap blocks were freed -- no leaks are possible
==12345==
==12345== For lists of detected and suppressed errors, rerun with: -s
==12345== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

### Understanding Heap Usage

Valgrind tracks both:
1. **brk-based allocations**: Memory from `sbrk()` system call
2. **mmap-based allocations**: Direct memory mappings for large blocks

The allocator uses both mechanisms, which Valgrind correctly identifies.

## Common Valgrind Scenarios

### 1. "Still Reachable" Blocks

If you see "still reachable" blocks at program exit:

```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 65,536 bytes in 1 blocks
==12345==   total heap usage: 100 allocs, 99 frees, 100,000 bytes allocated
==12345==
==12345== 65,536 bytes in 1 blocks are still reachable
```

**Explanation**: The heap space acquired via `brk()` is not automatically returned to the OS. This is normal behavior and not a memory leak. The memory is managed by the allocator's free lists.

**Solution**: This is expected. To verify it's not a leak, ensure all user-level `mem_malloc()` calls have corresponding `mem_free()` calls.

### 2. mmap Allocations

Large allocations (≥128KB) use `mmap()` directly:

```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 0 bytes in 0 blocks
==12345==   total heap usage: 50 allocs, 50 frees, 500,000 bytes allocated
```

These are properly tracked by Valgrind and will show as freed when `mem_free()` is called.

### 3. Invalid Read/Write

If Valgrind reports invalid memory access:

```
==12345== Invalid write of size 4
==12345==    at 0x4E89BD7: (your code)
==12345==  Address 0x... is 4 bytes after a block of size 100 alloc'd
```

**Cause**: Buffer overflow - writing beyond allocated memory
**Solution**: Check array bounds and allocated sizes

### 4. Use After Free

```
==12345== Invalid read of size 4
==12345==    at 0x4E89BD7: (your code)
==12345==  Address 0x... is 0 bytes inside a block of size 100 free'd
```

**Cause**: Using memory after calling `mem_free()`
**Solution**: Don't access pointers after freeing them

### 5. Double Free

```
==12345== Invalid free() / delete / delete[] / realloc()
==12345==    at 0x4C2EDEB: mem_free (allocator.c:...)
```

**Cause**: Calling `mem_free()` twice on the same pointer
**Solution**: Set pointers to NULL after freeing

## Valgrind Best Practices

### 1. Check During Development

Run Valgrind frequently during development:

```bash
make clean
make test
valgrind --leak-check=full ./test
```

### 2. Use Suppressions for Known Issues

If the allocator's internal state shows as "still reachable" (which is normal), create a suppression file:

```
{
   allocator_heap_residual
   Memcheck:Leak
   match-leak-kinds: reachable
   fun:sbrk
   ...
}
```

Use it with:
```bash
valgrind --suppressions=allocator.supp --leak-check=full ./test
```

### 3. Track Origins

For uninitialized value errors, use `--track-origins=yes`:

```bash
valgrind --track-origins=yes ./test
```

This helps identify where uninitialized values come from.

### 4. Verbose Output

For detailed information:

```bash
valgrind -v --leak-check=full --show-leak-kinds=all ./test
```

## Interpreting Results

### Memory Leak Categories

Valgrind classifies leaks as:

1. **Definitely lost**: Memory you allocated but no longer have pointers to
   - **Action**: Fix these! These are real leaks.

2. **Indirectly lost**: Memory pointed to by definitely lost blocks
   - **Action**: Fix the "definitely lost" blocks first.

3. **Possibly lost**: Pointers to memory exist but may not be valid
   - **Action**: Investigate these.

4. **Still reachable**: Memory that could still be accessed at program exit
   - **Note**: For the allocator's heap space, this is normal.

### Statistics Interpretation

```
total heap usage: 1,000 allocs, 1,000 frees, 256,000 bytes allocated
```

- **allocs**: Total calls to allocation functions
- **frees**: Total calls to free functions  
- **bytes allocated**: Total bytes requested (not including overhead)

If allocs == frees and no "definitely lost" blocks exist, your program is clean.

## Testing Checklist

Use this checklist when testing with Valgrind:

- [ ] No "definitely lost" blocks
- [ ] No "indirectly lost" blocks
- [ ] No invalid reads or writes
- [ ] No use of uninitialized values
- [ ] No double frees
- [ ] Same number of allocations and frees
- [ ] "Still reachable" blocks are expected (heap space)

## Performance Impact

Valgrind adds significant overhead:

| Test Type | Normal Speed | Valgrind Speed | Slowdown |
|-----------|--------------|----------------|----------|
| Unit tests | <1 second   | 2-5 seconds    | 5-10x    |
| Benchmarks | 0.02 seconds | 1-2 seconds    | 50-100x  |

**Note**: Never use Valgrind results to measure performance. Use it only for correctness checking.

## Integration with CI/CD

Example GitHub Actions workflow:

```yaml
name: Valgrind Check

on: [push, pull_request]

jobs:
  valgrind:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install Valgrind
        run: sudo apt-get install -y valgrind
      - name: Build
        run: make all
      - name: Run Valgrind
        run: make valgrind
```

## Common Issues and Solutions

### Issue: "Conditional jump depends on uninitialised value"

**Cause**: Using memory before initializing it
**Solution**: Initialize all variables, or use `mem_calloc()` instead of `mem_malloc()`

### Issue: "Mismatched free() / delete / delete[]"

**Cause**: Mixing system malloc with custom allocator
**Solution**: Don't mix `malloc()` with `mem_free()` or vice versa

### Issue: "Source and destination overlap"

**Cause**: Overlapping memory regions in `memcpy()`
**Solution**: Use `memmove()` instead of `memcpy()` for overlapping regions

## Additional Resources

- Valgrind Quick Start: https://valgrind.org/docs/manual/quick-start.html
- Valgrind Manual: https://valgrind.org/docs/manual/manual.html
- Memcheck Documentation: https://valgrind.org/docs/manual/mc-manual.html

## Conclusion

The custom memory allocator is designed to be Valgrind-friendly. Regular Valgrind testing ensures memory safety and helps catch bugs early in development. "Still reachable" heap blocks are expected and normal for this allocator implementation.
