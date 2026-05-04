# FPGC-Camera — Host-side test build
#
# Builds and runs the image processing pipeline tests on the host PC.
# All code uses integer-only arithmetic to match B32P3 CPU capabilities.

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS =

SRCS_LIB = image_proc.c pgm_io.c
OBJS_LIB = $(SRCS_LIB:.c=.o)

.PHONY: all clean test generate

all: unit_tests pipeline_test

# Unit tests (no I/O, pure algorithm verification)
unit_tests: unit_tests.o image_proc.o
	$(CC) $(LDFLAGS) -o $@ $^

# Full pipeline test (reads/writes PGM files)
pipeline_test: pipeline_test.o $(OBJS_LIB)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Generate synthetic test images
generate: pipeline_test
	@mkdir -p testdata output
	./pipeline_test --generate

# Run all tests
test: unit_tests pipeline_test
	@echo "=== Unit tests ==="
	./unit_tests
	@echo ""
	@echo "=== Pipeline tests (synthetic images) ==="
	@mkdir -p testdata output
	./pipeline_test --generate
	./pipeline_test testdata/gradient.pgm output/gradient
	./pipeline_test testdata/zones.pgm output/zones
	./pipeline_test testdata/lowcontrast.pgm output/lowcontrast
	@echo ""
	@echo "All tests complete. Check output/ for visual inspection."

clean:
	rm -f *.o unit_tests pipeline_test
	rm -rf output/
