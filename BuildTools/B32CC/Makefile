# Simplified SmallerC Makefile for Core Compiler Only
# Builds MIPS backend
# TODO: integrate this with project root Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2

SRCDIR = v0100

# Core compiler sources (relative to current directory)
CORE_SOURCES = smlrc.c

# Output binaries
SMLRC_MIPS = smlrc-mips

# Common compiler flags for targets
COMMON_FLAGS = 

.PHONY: all clean mips test-mips help

# Default target builds backend
all: mips

# Build MIPS backend
mips: $(SMLRC_MIPS)

$(SMLRC_MIPS): $(CORE_SOURCES) cgmips.c
	$(CC) $(CFLAGS) -DMIPS $(COMMON_FLAGS) $(CORE_SOURCES) -o $@


# Test that MIPS backend still works
test-mips: $(SMLRC_MIPS)
	@echo "Testing MIPS backend compilation..."
	@echo 'int main(void) { return 42; }' > test.c
	./$(SMLRC_MIPS) test.c test.asm
	@echo "MIPS compilation successful! Generated test.asm"
	@rm -f test.c test.asm


# Clean generated files
clean:
	rm -f $(SMLRC_MIPS) test.c *.asm

# Show usage help
help:
	@echo "SmallerC Simplified Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all        - Build MIPS backend (default)"
	@echo "  mips       - Build MIPS backend only"
	@echo "  test-mips  - Test MIPS backend compilation"
	@echo "  clean      - Remove generated files"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Usage Examples:"
	@echo "  make                    # Build MIPS backend"
	@echo "  make mips               # Build MIPS backend only"
	@echo "  make test-mips          # Test MIPS compilation"
	@echo ""
	@echo "To compile C files:"
	@echo "  ./smlrc-mips input.c output.asm      # Use MIPS backend"
