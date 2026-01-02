# =============================================================================
# FPGC Project Makefile
# =============================================================================

# -----------------------------------------------------------------------------
# B32CC (C Compiler) Variables
# -----------------------------------------------------------------------------
CC = gcc
CFLAGS = -Wall -Wextra -O2
B32CC_DIR = BuildTools/B32CC
B32CC_SOURCES = $(B32CC_DIR)/smlrc.c
B32CC_OUTPUT = $(B32CC_DIR)/output/b32cc

# -----------------------------------------------------------------------------
# Phony Targets
# -----------------------------------------------------------------------------
.PHONY: all clean help
.PHONY: venv
.PHONY: lint format format-check mypy ruff-lint ruff-format ruff-format-check
.PHONY: asmpy-install asmpy-uninstall test-asmpy asmpy-clean
.PHONY: docs-serve docs-deploy
.PHONY: sim-cpu sim-gpu sim-sdram sim-bootloader
.PHONY: test-cpu test-cpu-single debug-cpu quartus-timing
.PHONY: compile-asm compile-bootloader compile-c-baremetal
.PHONY: run-uart run-asm-uart run-c-baremetal-uart
.PHONY: flash-c-baremetal-spi
.PHONY: b32cc test-b32cc test-b32cc-single debug-b32cc clean-b32cc
.PHONY: check

# -----------------------------------------------------------------------------
# Default Target
# -----------------------------------------------------------------------------
all: venv b32cc asmpy-install check

# =============================================================================
# Python Development Environment (General)
# =============================================================================

venv:
	@echo "Creating/syncing Python virtual environment..."
	uv sync
	@echo "To activate the virtual environment:"
	@echo "  source .venv/bin/activate"
	@echo "Note: make/bash scripts that require the venv will automatically activate it."


# =============================================================================
# ASMPY Assembler
# =============================================================================

asmpy-install:
	@echo "Installing ASMPY assembler..."
	uv pip install -e .
	@echo "ASMPY installed! You can now use:"
	@echo "  uv run asmpy <file> <output>  - Run asmpy with any changes applied"
	@echo "  asmpy <file> <output>         - Run asmpy directly (if venv activated)"

asmpy-uninstall:
	@echo "Uninstalling ASMPY assembler..."
	uv pip uninstall fpgc

test-asmpy:
	@echo "Running ASMPY-specific tests..."
	uv run pytest BuildTools/ASMPY/tests/ --cov=asmpy

asmpy-clean:
	@echo "Cleaning ASMPY build artifacts..."
	-rm -rf asmpy.egg-info
	-rm -rf BuildTools/ASMPY/fpgc.egg-info
	-find BuildTools/ASMPY -type d -name __pycache__ -exec rm -r {} \+ 2>/dev/null; true

# =============================================================================
# Python Code Quality & Testing (All Python Code)
# =============================================================================

mypy:
	@echo "Running mypy type checker..."
	uv run mypy -p asmpy

ruff-lint:
	@echo "Running ruff linter..."
	uv run ruff check

ruff-format:
	@echo "Running ruff formatter..."
	uv run ruff format

ruff-format-check:
	@echo "Checking ruff formatting..."
	uv run ruff format --check

lint: ruff-lint mypy
	@echo "Linting complete!"

format: ruff-format
	@echo "Formatting complete!"

format-check: ruff-format-check
	@echo "Format check complete!"

# =============================================================================
# Full Check (Format, Lint, All Tests)
# =============================================================================

check: format-check lint test-asmpy test-cpu test-b32cc
	@echo "All checks passed!"

# =============================================================================
# B32CC (C Compiler)
# =============================================================================

b32cc: $(B32CC_OUTPUT)

$(B32CC_OUTPUT): $(B32CC_SOURCES) $(B32CC_DIR)/cgb32p3.inc
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DNO_ANNOTATIONS $(B32CC_SOURCES) -o $@

test-b32cc: $(B32CC_OUTPUT)
	@mkdir -p Tests/tmp
	./Scripts/Tests/run_b32cc_tests.sh

test-b32cc-single: $(B32CC_OUTPUT)
	@mkdir -p Tests/C/tmp
	@if [ -z "$(file)" ]; then \
		echo "Usage: make test-b32cc-single file=<test_file>"; \
		echo "Example: make test-b32cc-single file=04_control_flow/if_statements.c"; \
		echo "Available tests:"; \
		find Tests/C -name "*.c" -type f | grep -v "old_tests" | grep -v "tmp" | sed 's|Tests/C/||' | sort; \
		exit 1; \
	fi
	./Scripts/Tests/run_b32cc_tests.sh $(file)

debug-b32cc: $(B32CC_OUTPUT)
	@mkdir -p Tests/C/tmp
	@if [ -z "$(file)" ]; then \
		echo "Usage: make debug-b32cc file=<test_file>"; \
		echo "Example: make debug-b32cc file=04_control_flow/if_statements.c"; \
		echo "Available tests:"; \
		find Tests/C -name "*.c" -type f | grep -v "old_tests" | grep -v "tmp" | sed 's|Tests/C/||' | sort; \
		exit 1; \
	fi
	./Scripts/Tests/debug_b32cc_test.sh $(file)

clean-b32cc:
	rm -f $(B32CC_OUTPUT)

# =============================================================================
# Documentation
# =============================================================================

docs-serve:
	./Scripts/Docs/run.sh

docs-deploy:
	./Scripts/Docs/deploy.sh

# =============================================================================
# Simulation
# =============================================================================

SIMULATION_OUTPUT_DIR = Hardware/FPGA/Verilog/Simulation/Output

sim-cpu:
	@mkdir -p $(SIMULATION_OUTPUT_DIR)
	./Scripts/Simulation/simulate_cpu.sh --add-ram --add-flash

sim-gpu:
	@mkdir -p $(SIMULATION_OUTPUT_DIR)
	./Scripts/Simulation/simulate_gpu.sh

sim-sdram:
	@mkdir -p $(SIMULATION_OUTPUT_DIR)
	./Scripts/Simulation/simulate_sdram.sh

sim-bootloader:
	@mkdir -p $(SIMULATION_OUTPUT_DIR)
	./Scripts/ASM/compile_bootloader.sh --simulate

# =============================================================================
# Testing (Hardware)
# =============================================================================

test-cpu:
	@mkdir -p $(SIMULATION_OUTPUT_DIR)
	@mkdir -p Tests/tmp
	./Scripts/Tests/run_cpu_tests.sh

test-cpu-single:
	@mkdir -p $(SIMULATION_OUTPUT_DIR)
	@if [ -z "$(file)" ]; then \
		echo "Usage: make test-cpu-single file=<test_file>"; \
		echo "Example: make test-cpu-single file=01_load/load.asm"; \
		echo "Available tests:"; \
		find Tests/CPU -name "*.asm" -type f | grep -v "tmp" | sed 's|Tests/CPU/||' | sort; \
		exit 1; \
	fi
	./Scripts/Tests/run_cpu_tests.sh $(file)

debug-cpu:
	@mkdir -p $(SIMULATION_OUTPUT_DIR)
	@if [ -z "$(file)" ]; then \
		echo "Usage: make debug-cpu file=<test_file>"; \
		echo "Example: make debug-cpu file=01_load/load.asm"; \
		echo "Available tests:"; \
		find Tests/CPU -name "*.asm" -type f | grep -v "tmp" | sed 's|Tests/CPU/||' | sort; \
		exit 1; \
	fi
	./Scripts/Tests/debug_cpu_test.sh $(file)

quartus-timing:
	./Scripts/Tests/quartus_timing.sh

# =============================================================================
# Compilation
# =============================================================================

compile-asm:
	@if [ -z "$(file)" ]; then \
		echo "Usage: make compile-asm file=<asm_filename_in_programs_dir_without_extension>"; \
		echo "Example: make compile-asm file=pixel_loop"; \
		echo "Available programs:"; \
		find Software/ASM/Programs -name "*.asm" -type f | grep -v "tmp" | sed 's|Software/ASM/Programs/||' | sed 's|.asm||' | sort; \
		exit 1; \
	fi
	./Scripts/ASM/compile_bare_metal_asm.sh $(file)

compile-bootloader:
	./Scripts/ASM/compile_bootloader.sh

compile-c-baremetal: $(B32CC_OUTPUT)
	@if [ -z "$(file)" ]; then \
		echo "Usage: make compile-c-baremetal file=<c_filename_in_bareMetal_dir_without_extension>"; \
		echo "Example: make compile-c-baremetal file=hello_world"; \
		echo "Available programs:"; \
		find Software/C/bareMetal -name "*.c" -type f | grep -v "tmp" | sed 's|Software/C/bareMetal/||' | sed 's|.c||' | sort; \
		exit 1; \
	fi
	./Scripts/BCC/compile_bare_metal_c.sh $(file)

# =============================================================================
# Hardware Programming
# =============================================================================

run-uart:
	./Scripts/Programmer/UART/run_uart.sh

run-asm-uart: compile-asm run-uart

run-c-baremetal-uart: compile-c-baremetal run-uart

flash-c-baremetal-spi: $(B32CC_OUTPUT)
	@if [ -z "$(file)" ]; then \
		echo "Usage: make flash-c-baremetal-spi file=<c_filename_in_bareMetal_dir_without_extension>"; \
		echo "Example: make flash-c-baremetal-spi file=libtests/test_term"; \
		echo "Available programs:"; \
		find Software/C/bareMetal -name "*.c" -type f | grep -v "tmp" | grep -v "flash_writer" | sed 's|Software/C/bareMetal/||' | sed 's|.c||' | sort; \
		exit 1; \
	fi
	./Scripts/Programmer/flash_spi.sh $(file)

# =============================================================================
# Cleanup
# =============================================================================

clean:
	@echo "Cleaning all build artifacts and environments..."
	-uv pip uninstall fpgc
	-rm -rf asmpy.egg-info
	-rm -rf BuildTools/ASMPY/fpgc.egg-info
	-rm -rf Docs/site
	-rm -rf Software/ASM/Output
	-rm -rf build
	-rm -rf .venv
	-rm -rf .mypy_cache
	-rm -rf .pytest_cache
	-rm -rf .ruff_cache
	-rm -rf .coverage
	-rm -f $(B32CC_OUTPUT)
	-find . -type d -name __pycache__ -exec rm -r {} \+ 2>/dev/null; true
	@echo "Cleanup complete!"

# =============================================================================
# Help
# =============================================================================

help:
	@echo "==================================================================="
	@echo "FPGC Project Makefile"
	@echo "==================================================================="
	@echo ""
	@echo "--- Python Development Environment ---"
	@echo "  venv                - Create or sync virtual environment"
	@echo ""
	@echo "--- ASMPY Assembler ---"
	@echo "  asmpy-install       - Install ASMPY assembler tool"
	@echo "  asmpy-uninstall     - Uninstall ASMPY assembler tool"
	@echo "  test-asmpy          - Run ASMPY-specific tests"
	@echo "  asmpy-clean         - Clean ASMPY build artifacts"
	@echo ""
	@echo "--- Python Code Quality & Testing ---"
	@echo "  check               - Run format-check, lint, and all tests (CI-safe)"
	@echo "  lint                - Run ruff lint and mypy"
	@echo "  format              - Run ruff format (modifies files)"
	@echo "  format-check        - Check ruff formatting without modifying"
	@echo "  mypy                - Run mypy type checker only"
	@echo "  ruff-lint           - Run ruff linter only"
	@echo "  ruff-format         - Run ruff formatter only (modifies files)"
	@echo "  ruff-format-check   - Check ruff formatting only"
	@echo ""
	@echo "--- B32CC (C Compiler) ---"
	@echo "  b32cc               - Build the B32P3 C compiler"
	@echo "  test-b32cc          - Run all B32P3 C compiler tests (parallel)"
	@echo "  test-b32cc-single   - Run a single test"
	@echo "                        Usage: make test-b32cc-single file=<test_file>"
	@echo "  debug-b32cc         - Debug a single test with GTKWave"
	@echo "                        Usage: make debug-b32cc file=<test_file>"
	@echo "  clean-b32cc         - Clean B32CC build artifacts"
	@echo ""
	@echo "--- Documentation ---"
	@echo "  docs-serve          - Run documentation website locally"
	@echo "  docs-deploy         - Deploy documentation website"
	@echo ""
	@echo "--- Simulation ---"
	@echo "  sim-cpu             - Run CPU simulation"
	@echo "  sim-gpu             - Run GPU simulation"
	@echo "  sim-sdram           - Run SDRAM controller simulation"
	@echo "  sim-bootloader      - Compile and simulate bootloader"
	@echo ""
	@echo "--- Testing (Hardware) ---"
	@echo "  test-cpu            - Run all CPU tests (parallel)"
	@echo "  test-cpu-single     - Run a single CPU test"
	@echo "                        Usage: make test-cpu-single file=<test_file>"
	@echo "  debug-cpu           - Debug a single CPU test with GTKWave"
	@echo "                        Usage: make debug-cpu file=<test_file>"
	@echo "  quartus-timing      - Run Quartus timing analysis"
	@echo ""
	@echo "--- Compilation ---"
	@echo "  compile-asm         - Compile ASM file"
	@echo "                        Usage: make compile-asm file=<filename>"
	@echo "  compile-c-baremetal - Compile bare-metal C file"
	@echo "                        Usage: make compile-c-baremetal file=<filename>"
	@echo "  compile-bootloader  - Compile bootloader"
	@echo ""
	@echo "--- Hardware Programming ---"
	@echo "  run-uart              - Run compiled ASM binary via UART"
	@echo "  run-asm-uart          - Compile and run ASM binary via UART"
	@echo "  run-c-baremetal-uart  - Compile and run C binary via UART"
	@echo "  flash-c-baremetal-spi - Flash C binary to SPI flash (persistent)"
	@echo "                          Usage: make flash-c-baremetal-spi file=<filename>"
	@echo ""
	@echo "--- Cleanup ---"
	@echo "  clean               - Clean all build artifacts and environments"
	@echo ""
	@echo "==================================================================="

