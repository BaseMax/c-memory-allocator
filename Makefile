# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g
LDFLAGS = -pthread

# Source files
ALLOCATOR_SRCS = allocator.c allocator_ts.c
ALLOCATOR_OBJS = $(ALLOCATOR_SRCS:.c=.o)

# Targets
LIB = liballocator.a
TEST_PROG = test
BENCH_PROG = benchmark
EXAMPLE_PROG = example

.PHONY: all clean run-test run-bench run-example valgrind

all: $(LIB) $(TEST_PROG) $(BENCH_PROG) $(EXAMPLE_PROG)

# Build static library
$(LIB): $(ALLOCATOR_OBJS)
	ar rcs $@ $^

# Build test program
$(TEST_PROG): test.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build benchmark program
$(BENCH_PROG): benchmark.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build example program
$(EXAMPLE_PROG): example.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile object files
%.o: %.c allocator.h
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests
run-test: $(TEST_PROG)
	./$(TEST_PROG)

# Run benchmarks
run-bench: $(BENCH_PROG)
	./$(BENCH_PROG)

# Run example
run-example: $(EXAMPLE_PROG)
	./$(EXAMPLE_PROG)

# Run with Valgrind
valgrind: $(TEST_PROG)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TEST_PROG)

# Run benchmark with Valgrind
valgrind-bench: $(BENCH_PROG)
	valgrind --leak-check=full --show-leak-kinds=all ./$(BENCH_PROG)

# Clean build artifacts
clean:
	rm -f $(ALLOCATOR_OBJS) test.o benchmark.o example.o $(LIB) $(TEST_PROG) $(BENCH_PROG) $(EXAMPLE_PROG)

# Help
help:
	@echo "Custom Memory Allocator - Build Targets"
	@echo "======================================="
	@echo "make all          - Build library and all programs"
	@echo "make run-test     - Build and run tests"
	@echo "make run-bench    - Build and run benchmarks"
	@echo "make run-example  - Build and run example program"
	@echo "make valgrind     - Run tests with Valgrind"
	@echo "make valgrind-bench - Run benchmarks with Valgrind"
	@echo "make clean        - Remove build artifacts"
