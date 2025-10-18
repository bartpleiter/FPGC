# Simplified SmallerC Makefile for Core Compiler Only
# TODO: clean up and integrate this with project root Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2

SRCDIR = v0100

# Core compiler sources (relative to current directory)
CORE_SOURCES = smlrc.c

# Output binaries
SMLRC_B32P2 = output/smlrc-b32p2

# Common compiler flags for targets
COMMON_FLAGS = 

.PHONY: all clean b32p2 test-b32p2 help

# Default target builds backend
all: b32p2

# Build B32P2 backend
b32p2: $(SMLRC_B32P2)

$(SMLRC_B32P2): $(CORE_SOURCES) cgb32p2.inc
	$(CC) $(CFLAGS) -DNO_ANNOTATIONS $(COMMON_FLAGS) $(CORE_SOURCES) -o $@


# Very simple test that should be replaced with proper test suite
test-b32p2: $(SMLRC_B32P2)
	./$(SMLRC_B32P2) tests/a.c output/out.asm

# Clean generated files
clean:
	rm -f $(SMLRC_B32P2) output/out.asm
