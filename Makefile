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
B32CC_OUTPUT = $(B32CC_DIR)/output/smlrc-b32p2

# -----------------------------------------------------------------------------
# Phony Targets
# -----------------------------------------------------------------------------
.PHONY: all clean help
.PHONY: venv
.PHONY: lint format mypy ruff-lint ruff-format
.PHONY: asmpy-install asmpy-uninstall asmpy-test asmpy-clean
.PHONY: docs-serve docs-deploy
.PHONY: sim-cpu sim-cpu-uart sim-gpu sim-bootloader
.PHONY: test-cpu
.PHONY: compile-asm compile-bootloader
.PHONY: flash-asm-uart run-asm-uart
.PHONY: b32cc test-b32cc clean-b32cc

# -----------------------------------------------------------------------------
# Default Target
# -----------------------------------------------------------------------------
all: venv b32cc asmpy-install

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

asmpy-test:
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
	uv run mypy BuildTools/ASMPY/asmpy
# TODO: add other python dirs to mypy check

ruff-lint:
	@echo "Running ruff linter..."
	uv run ruff check

ruff-format:
	@echo "Running ruff formatter..."
	uv run ruff format

lint: ruff-lint mypy
	@echo "Linting complete!"

format: ruff-format
	@echo "Formatting complete!"

# =============================================================================
# B32CC (C Compiler)
# =============================================================================

b32cc: $(B32CC_OUTPUT)

$(B32CC_OUTPUT): $(B32CC_SOURCES) $(B32CC_DIR)/cgb32p2.inc
	$(CC) $(CFLAGS) -DNO_ANNOTATIONS $(B32CC_SOURCES) -o $@

# TODO: replace this test with a proper test suite
test-b32cc: $(B32CC_OUTPUT)
	cd $(B32CC_DIR) && ./output/smlrc-b32p2 tests/a.c output/out.asm

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

sim-cpu:
	./Scripts/Simulation/simulate_cpu.sh --add-ram --add-flash

sim-cpu-uart:
	./Scripts/Simulation/simulate_cpu.sh --add-ram --add-flash --add-uart

sim-gpu:
	./Scripts/Simulation/simulate_gpu.sh

sim-bootloader:
	./Scripts/ASM/compile_bootloader.sh --simulate

# =============================================================================
# Testing (Hardware)
# =============================================================================

test-cpu:
	./Scripts/Tests/run_cpu_tests.sh

# =============================================================================
# Compilation
# =============================================================================

compile-asm:
	./Scripts/ASM/compile_bare_metal_asm.sh $(file)

compile-bootloader:
	./Scripts/ASM/compile_bootloader.sh

# =============================================================================
# Hardware Programming
# =============================================================================

flash-asm-uart:
	./Scripts/Programmer/UART/flash_uart.sh

run-asm-uart: compile-asm flash-asm-uart

# =============================================================================
# Cleanup
# =============================================================================

clean:
	@echo "Cleaning all build artifacts and environments..."
	-uv pip uninstall fpgc
	-rm -rf asmpy.egg-info
	-rm -rf BuildTools/ASMPY/fpgc.egg-info
	-rm -rf Docs/site
	-rm -rf Software/BareMetalASM/Output
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
	@echo "  asmpy-test          - Run ASMPY-specific tests"
	@echo "  asmpy-clean         - Clean ASMPY build artifacts"
	@echo ""
	@echo "--- Python Code Quality & Testing ---"
	@echo "  lint                - Run ruff lint and mypy"
	@echo "  format              - Run ruff format"
	@echo "  mypy                - Run mypy type checker only"
	@echo "  ruff-lint           - Run ruff linter only"
	@echo "  ruff-format         - Run ruff formatter only"
	@echo ""
	@echo "--- B32CC (C Compiler) ---"
	@echo "  b32cc               - Build the B32P2 C compiler"
	@echo "  test-b32cc          - Test the B32P2 C compiler"
	@echo "  clean-b32cc         - Clean B32CC build artifacts"
	@echo ""
	@echo "--- Documentation ---"
	@echo "  docs-serve          - Run documentation website locally"
	@echo "  docs-deploy         - Deploy documentation website"
	@echo ""
	@echo "--- Simulation ---"
	@echo "  sim-cpu             - Run CPU simulation"
	@echo "  sim-cpu-uart        - Run CPU simulation with UART"
	@echo "  sim-gpu             - Run GPU simulation"
	@echo "  sim-bootloader      - Compile and simulate bootloader"
	@echo ""
	@echo "--- Testing (Hardware) ---"
	@echo "  test-cpu            - Run CPU tests"
	@echo ""
	@echo "--- Compilation ---"
	@echo "  compile-asm         - Compile ASM file"
	@echo "                        Usage: make compile-asm file=<filename>"
	@echo "  compile-bootloader  - Compile bootloader"
	@echo ""
	@echo "--- Hardware Programming ---"
	@echo "  flash-asm-uart      - Flash compiled ASM binary via UART"
	@echo "  run-asm-uart        - Compile and flash ASM binary via UART"
	@echo ""
	@echo "--- Cleanup ---"
	@echo "  clean               - Clean all build artifacts and environments"
	@echo ""
	@echo "==================================================================="

