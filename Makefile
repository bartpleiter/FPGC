# =============================================================================
# FPGC Project Makefile
# =============================================================================

SHELL := /bin/bash

# -----------------------------------------------------------------------------
# B32CC (C Compiler) Variables
# -----------------------------------------------------------------------------
CC = gcc
CFLAGS = -Wall -Wextra -O2
B32CC_DIR = BuildTools/B32CC
B32CC_SOURCES = $(B32CC_DIR)/smlrc.c
B32CC_OUTPUT = $(B32CC_DIR)/output/b32cc

# -----------------------------------------------------------------------------
# QBE (Backend Compiler) Variables
# -----------------------------------------------------------------------------
QBE_DIR = BuildTools/QBE
QBE_OUTPUT = $(QBE_DIR)/output/qbe

# -----------------------------------------------------------------------------
# cproc (C Frontend) Variables
# -----------------------------------------------------------------------------
CPROC_DIR = BuildTools/cproc
CPROC_OUTPUT = $(CPROC_DIR)/output/cproc-qbe

# -----------------------------------------------------------------------------
# Phony Targets
# -----------------------------------------------------------------------------
.PHONY: all clean help
.PHONY: venv
.PHONY: lint format format-check mypy ruff-lint ruff-format ruff-format-check
.PHONY: asmpy-install asmpy-uninstall test-asmpy asmpy-clean
.PHONY: test-asm-link test-cpp test-term2 test-shell-host test-host
.PHONY: docs-serve docs-deploy
.PHONY: sim-cpu sim-sdram sim-bootloader
.PHONY: test-cpu test-cpu-single debug-cpu quartus-timing
.PHONY: test-c test-c-single
.PHONY: compile-asm compile-bootloader compile-c-baremetal compile-bdos
.PHONY: compile-userbdos compile-userbdos-all compile-doom
.PHONY: compile-userbdos-b32cc compile-userbdos-all-b32cc
.PHONY: run-uart uart-monitor run-asm-uart run-c-baremetal-uart run-bdos
.PHONY: run-userbdos run-doom
.PHONY: flash-c-baremetal-spi flash-bdos
.PHONY: b32cc test-b32cc test-b32cc-single debug-b32cc clean-b32cc
.PHONY: qbe clean-qbe
.PHONY: cproc clean-cproc
.PHONY: selfhost-qbe selfhost-cproc selfhost-all
.PHONY: check
.PHONY: fnp-upload-text fnp-upload-userbdos fnp-upload-userbdos-b32cc
.PHONY: fnp-keyboard fnp-detect-iface fnp-sync-files fnp-run
.PHONY: fnp-debug-userbdos
.PHONY: convert-w3d-textures

# -----------------------------------------------------------------------------
# Default Target
# -----------------------------------------------------------------------------
all: venv b32cc qbe cproc asmpy-install check

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

test-asm-link:
	@echo "Running asm-link byte-for-byte regression tests..."
	uv run pytest Scripts/Tests/asm_link_tests.py -v

test-cpp:
	@echo "Running cpp byte-for-byte regression tests vs gcc cpp..."
	uv run pytest Scripts/Tests/cpp_tests.py -v

test-term2:
	@echo "Running libterm v2 host unit tests..."
	uv run pytest Scripts/Tests/term2_tests.py -v

test-shell-host:
	@echo "Running BDOS shell host unit tests..."
	uv run pytest Scripts/Tests/shell_host_tests.py -v

test-host: test-term2 test-shell-host
	@echo "All host-side unit tests passed."

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

check: format-check lint test-asmpy test-cpu test-c
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
	@mkdir -p Tests/B32CC/tmp
	@if [ -z "$(file)" ]; then \
		echo "Usage: make test-b32cc-single file=<test_file>"; \
		echo "Example: make test-b32cc-single file=04_control_flow/if_statements.c"; \
		echo "Available tests:"; \
		find Tests/B32CC -name "*.c" -type f | grep -v "old_tests" | grep -v "tmp" | sed 's|Tests/B32CC/||' | sort; \
		exit 1; \
	fi
	./Scripts/Tests/run_b32cc_tests.sh $(file)

debug-b32cc: $(B32CC_OUTPUT)
	@mkdir -p Tests/B32CC/tmp
	@if [ -z "$(file)" ]; then \
		echo "Usage: make debug-b32cc file=<test_file>"; \
		echo "Example: make debug-b32cc file=04_control_flow/if_statements.c"; \
		echo "Available tests:"; \
		find Tests/B32CC -name "*.c" -type f | grep -v "old_tests" | grep -v "tmp" | sed 's|Tests/B32CC/||' | sort; \
		exit 1; \
	fi
	./Scripts/Tests/debug_b32cc_test.sh $(file)

clean-b32cc:
	rm -f $(B32CC_OUTPUT)

# =============================================================================
# C Test Suite (cproc + QBE)
# =============================================================================

test-c: $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@mkdir -p Tests/tmp
	./Scripts/Tests/run_c_tests.sh

test-c-single: $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@mkdir -p Tests/C/tmp
	@if [ -z "$(file)" ]; then \
		echo "Usage: make test-c-single file=<test_file>"; \
		echo "Example: make test-c-single file=01_return/return_constant.c"; \
		echo "Available tests:"; \
		find Tests/C -name "*.c" -type f | grep -v "tmp" | sed 's|Tests/C/||' | sort; \
		exit 1; \
	fi
	./Scripts/Tests/run_c_tests.sh $(file)

# =============================================================================
# QBE (Backend Compiler for B32P3)
# =============================================================================

qbe: $(QBE_OUTPUT)

$(QBE_OUTPUT):
	$(MAKE) -C $(QBE_DIR)

clean-qbe:
	$(MAKE) -C $(QBE_DIR) clean

# =============================================================================
# cproc (C Frontend for B32P3)
# =============================================================================

cproc: $(CPROC_OUTPUT)

$(CPROC_OUTPUT):
	$(MAKE) -C $(CPROC_DIR)

clean-cproc:
	$(MAKE) -C $(CPROC_DIR) clean

# =============================================================================
# Self-Hosting: QBE & cproc as BDOS UserBDOS Binaries
# =============================================================================

# Minimal libc/userlib subset for compiler tools (no graphics, no fixed-point)
SELFHOST_LIBC = \
	Software/ASM/crt0/crt0_userbdos.asm \
	Software/C/libc/string/string.c \
	Software/C/libc/stdlib/stdlib.c \
	Software/C/libc/stdlib/malloc.c \
	Software/C/libc/ctype/ctype.c \
	Software/C/libc/stdio/stdio.c \
	Software/C/userlib/src/syscall_asm.asm \
	Software/C/userlib/src/syscall.c \
	Software/C/userlib/src/io_stubs.c

SELFHOST_FLAGS = --libc -I Software/C/userlib/include -h -i

# Cross-compilation: C source → B32P3 assembly via cproc + QBE
# Uses temp files to avoid clobbering output on failure
XCOMPILE = bash -o pipefail -c 'cpp -nostdinc -P -I Software/C/libc/include -I Software/C/userlib/include -I $(QBE_DIR) -I $(QBE_DIR)/b32p3 -DQBE_BITS32 -D__B32P3__ $< | \
	$(CPROC_OUTPUT) -t b32p3 | $(QBE_OUTPUT) > $@.tmp' && mv $@.tmp $@
XCOMPILE_CPROC = bash -o pipefail -c 'cpp -nostdinc -P -I Software/C/libc/include -I Software/C/userlib/include -I $(CPROC_DIR) -D__B32P3__ $< | \
	$(CPROC_OUTPUT) -t b32p3 | $(QBE_OUTPUT) > $@.tmp' && mv $@.tmp $@

QBE_ASM_DIR = BuildTools/QBE/output/asm

# QBE cross-compiled .asm files (generated from .c sources)
QBE_C_SOURCES = abi.c alias.c cfg.c copy.c emit.c fold.c live.c load.c main.c \
	mem.c parse.c rega.c simpl.c spill.c ssa.c util.c
QBE_B32P3_SOURCES = b32p3/abi.c b32p3/emit.c b32p3/isel.c b32p3/targ.c

QBE_ASM_FILES = \
	$(patsubst %.c,$(QBE_ASM_DIR)/%.asm,$(QBE_C_SOURCES)) \
	$(patsubst b32p3/%.c,$(QBE_ASM_DIR)/b32p3_%.asm,$(QBE_B32P3_SOURCES))

# Pattern rules: rebuild .asm when .c changes
$(QBE_ASM_DIR)/%.asm: $(QBE_DIR)/%.c $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@mkdir -p $(QBE_ASM_DIR)
	@echo "  XCOMPILE $< → $@"
	@$(XCOMPILE)

$(QBE_ASM_DIR)/b32p3_%.asm: $(QBE_DIR)/b32p3/%.c $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@mkdir -p $(QBE_ASM_DIR)
	@echo "  XCOMPILE $< → $@"
	@$(XCOMPILE)

CPROC_ASM_DIR = BuildTools/cproc/output/asm

CPROC_C_SOURCES = attr.c decl.c eval.c expr.c init.c main.c map.c pp.c \
	qbe.c scan.c scope.c stmt.c targ.c token.c tree.c type.c utf.c util.c

CPROC_ASM_FILES = $(patsubst %.c,$(CPROC_ASM_DIR)/%.asm,$(CPROC_C_SOURCES))

$(CPROC_ASM_DIR)/%.asm: $(CPROC_DIR)/%.c $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@mkdir -p $(CPROC_ASM_DIR)
	@echo "  XCOMPILE $< → $@"
	@$(XCOMPILE_CPROC)

selfhost-qbe: $(QBE_ASM_FILES)
	@mkdir -p BuildTools/QBE/output
	./Scripts/BCC/compile_modern_c.sh \
		$(SELFHOST_LIBC) \
		Software/C/libc/stdlib/getopt.c \
		$(QBE_ASM_FILES) \
		$(SELFHOST_FLAGS) \
		-o BuildTools/QBE/output/qbe.bin
	@mkdir -p Files/BRFS-init/bin
	@cp BuildTools/QBE/output/qbe.bin Files/BRFS-init/bin/qbe
	@echo "Binary copied to Files/BRFS-init/bin/qbe"

selfhost-cproc: $(CPROC_ASM_FILES)
	@mkdir -p BuildTools/cproc/output
	./Scripts/BCC/compile_modern_c.sh \
		$(SELFHOST_LIBC) \
		$(CPROC_ASM_FILES) \
		$(SELFHOST_FLAGS) \
		-o BuildTools/cproc/output/cproc.bin
	@mkdir -p Files/BRFS-init/bin
	@cp BuildTools/cproc/output/cproc.bin Files/BRFS-init/bin/cproc
	@echo "Binary copied to Files/BRFS-init/bin/cproc"

selfhost-all: selfhost-qbe selfhost-cproc

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

compile-c-baremetal: $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@if [ -z "$(file)" ]; then \
		echo "Usage: make compile-c-baremetal file=<c_filename_in_bareMetal_dir_without_extension>"; \
		echo "Example: make compile-c-baremetal file=hello_world"; \
		echo "Available programs:"; \
		find Software/C/bareMetal -name "*.c" -type f | grep -v "tmp" | sed 's|Software/C/bareMetal/||' | sed 's|.c||' | sort; \
		exit 1; \
	fi
	@mkdir -p Software/ASM/Output
	./Scripts/BCC/compile_modern_c.sh Software/ASM/crt0/crt0_baremetal.asm Software/C/bareMetal/$(file).c -h -o Software/ASM/Output/code.bin

BDOS_V3_SOURCES = \
	Software/ASM/crt0/crt0_bdos.asm \
	Software/C/libc/sys/_exit.asm \
	Software/C/libc/string/string.c \
	Software/C/libc/stdlib/stdlib.c \
	Software/C/libc/stdlib/malloc.c \
	Software/C/libc/ctype/ctype.c \
	Software/C/libc/stdio/stdio.c \
	Software/C/libc/sys/syscalls.c \
	Software/C/libfpgc/sys/sys_asm.asm \
	Software/C/libfpgc/sys/sys.c \
	Software/C/libfpgc/io/spi.c \
	Software/C/libfpgc/io/uart.c \
	Software/C/libfpgc/io/timer.c \
	Software/C/libfpgc/io/spi_flash.c \
	Software/C/libfpgc/io/ch376.c \
	Software/C/libfpgc/io/enc28j60.c \
	Software/C/libfpgc/gfx/gpu_hal.c \
	Software/C/libfpgc/gfx/gpu_fb.c \
	Software/C/libfpgc/gfx/gpu_data_ascii.c \
	Software/C/libfpgc/term/term2.c \
	Software/C/libfpgc/mem/debug.c \
	Software/C/libfpgc/fs/brfs.c \
	Software/C/libfpgc/fs/brfs_storage_spi_flash.c \
	Software/C/libfpgc/fs/brfs_cache.c \
	Software/C/bdos/slot_asm.asm \
	Software/C/bdos/main.c \
	Software/C/bdos/init.c \
	Software/C/bdos/heap.c \
	Software/C/bdos/syscall.c \
	Software/C/bdos/vfs.c \
	Software/C/bdos/proc.c \
	Software/C/bdos/slot.c \
	Software/C/bdos/hid.c \
	Software/C/bdos/fs.c \
	Software/C/bdos/eth.c \
	Software/C/bdos/shell.c \
	Software/C/bdos/shell_cmds.c \
	Software/C/bdos/shell_path.c \
	Software/C/bdos/shell_util.c \
	Software/C/bdos/shell_format.c \
	Software/C/bdos/shell_vars.c \
	Software/C/bdos/shell_lex.c \
	Software/C/bdos/shell_parse.c \
	Software/C/bdos/shell_exec.c \
	Software/C/bdos/shell_script.c

compile-bdos: $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@mkdir -p Software/ASM/Output
	./Scripts/BCC/compile_modern_c.sh \
		$(BDOS_V3_SOURCES) \
		--libc \
		-I Software/C/libfpgc/include \
		-I Software/C/bdos/include \
		-h -s \
		-o Software/ASM/Output/code.bin

# User library sources linked into every userBDOS program
USERLIB_SOURCES = \
	Software/ASM/crt0/crt0_userbdos.asm \
	Software/C/libc/string/string.c \
	Software/C/libc/stdlib/stdlib.c \
	Software/C/libc/stdlib/malloc.c \
	Software/C/libc/ctype/ctype.c \
	Software/C/libc/stdio/stdio.c \
	Software/C/userlib/src/syscall_asm.asm \
	Software/C/userlib/src/syscall.c \
	Software/C/userlib/src/io_stubs.c \
	Software/C/userlib/src/time.c \
	Software/C/userlib/src/fixedmath.c \
	Software/C/userlib/src/fixed64_asm.asm \
	Software/C/userlib/src/fixed64.c \
	Software/C/userlib/src/plot.c \
	Software/C/userlib/src/fnp.c

USERLIB_FLAGS = --libc -I Software/C/userlib/include -h -i

# --- Doom build (special multi-file target) ---

DOOM_DIR = Software/C/userBDOS/doom

# Core doom source files (order matters for linker)
DOOM_SOURCES = \
	$(DOOM_DIR)/doom_asm.asm \
	$(DOOM_DIR)/doom_libc_bridge.c \
	Software/C/libc/stdio/stdio.c \
	$(DOOM_DIR)/doomgeneric_fpgc.c \
	$(DOOM_DIR)/dummy.c \
	$(DOOM_DIR)/am_map.c \
	$(DOOM_DIR)/doomdef.c \
	$(DOOM_DIR)/doomstat.c \
	$(DOOM_DIR)/dstrings.c \
	$(DOOM_DIR)/d_event.c \
	$(DOOM_DIR)/d_items.c \
	$(DOOM_DIR)/d_iwad.c \
	$(DOOM_DIR)/d_loop.c \
	$(DOOM_DIR)/d_main.c \
	$(DOOM_DIR)/d_mode.c \
	$(DOOM_DIR)/d_net.c \
	$(DOOM_DIR)/f_finale.c \
	$(DOOM_DIR)/f_wipe.c \
	$(DOOM_DIR)/g_game.c \
	$(DOOM_DIR)/hu_lib.c \
	$(DOOM_DIR)/hu_stuff.c \
	$(DOOM_DIR)/info.c \
	$(DOOM_DIR)/i_cdmus.c \
	$(DOOM_DIR)/i_endoom.c \
	$(DOOM_DIR)/i_joystick.c \
	$(DOOM_DIR)/i_scale.c \
	$(DOOM_DIR)/i_sound.c \
	$(DOOM_DIR)/i_system.c \
	$(DOOM_DIR)/i_timer.c \
	$(DOOM_DIR)/memio.c \
	$(DOOM_DIR)/m_argv.c \
	$(DOOM_DIR)/m_bbox.c \
	$(DOOM_DIR)/m_cheat.c \
	$(DOOM_DIR)/m_config.c \
	$(DOOM_DIR)/m_controls.c \
	$(DOOM_DIR)/m_fixed.c \
	$(DOOM_DIR)/m_menu.c \
	$(DOOM_DIR)/m_misc.c \
	$(DOOM_DIR)/m_random.c \
	$(DOOM_DIR)/p_ceilng.c \
	$(DOOM_DIR)/p_doors.c \
	$(DOOM_DIR)/p_enemy.c \
	$(DOOM_DIR)/p_floor.c \
	$(DOOM_DIR)/p_inter.c \
	$(DOOM_DIR)/p_lights.c \
	$(DOOM_DIR)/p_map.c \
	$(DOOM_DIR)/p_maputl.c \
	$(DOOM_DIR)/p_mobj.c \
	$(DOOM_DIR)/p_plats.c \
	$(DOOM_DIR)/p_pspr.c \
	$(DOOM_DIR)/p_saveg.c \
	$(DOOM_DIR)/p_setup.c \
	$(DOOM_DIR)/p_sight.c \
	$(DOOM_DIR)/p_spec.c \
	$(DOOM_DIR)/p_switch.c \
	$(DOOM_DIR)/p_telept.c \
	$(DOOM_DIR)/p_tick.c \
	$(DOOM_DIR)/p_user.c \
	$(DOOM_DIR)/r_bsp.c \
	$(DOOM_DIR)/r_data.c \
	$(DOOM_DIR)/r_draw.c \
	$(DOOM_DIR)/r_main.c \
	$(DOOM_DIR)/r_plane.c \
	$(DOOM_DIR)/r_segs.c \
	$(DOOM_DIR)/r_sky.c \
	$(DOOM_DIR)/r_things.c \
	$(DOOM_DIR)/sha1.c \
	$(DOOM_DIR)/sounds.c \
	$(DOOM_DIR)/statdump.c \
	$(DOOM_DIR)/st_lib.c \
	$(DOOM_DIR)/st_stuff.c \
	$(DOOM_DIR)/s_sound.c \
	$(DOOM_DIR)/tables.c \
	$(DOOM_DIR)/v_video.c \
	$(DOOM_DIR)/wi_stuff.c \
	$(DOOM_DIR)/w_checksum.c \
	$(DOOM_DIR)/w_file.c \
	$(DOOM_DIR)/w_main.c \
	$(DOOM_DIR)/w_wad.c \
	$(DOOM_DIR)/z_zone.c \
	$(DOOM_DIR)/w_file_stdc.c \
	$(DOOM_DIR)/i_input.c \
	$(DOOM_DIR)/i_video.c \
	$(DOOM_DIR)/doomgeneric.c \
	$(DOOM_DIR)/gusconf.c \
	$(DOOM_DIR)/mus2mid.c

DOOM_FLAGS = --libc -I Software/C/userlib/include -I $(DOOM_DIR) -h -i

# NOTE: Doom uses a subset of USERLIB_SOURCES (omits io_stubs, fixedmath, fixed64,
# plot, fnp, and the standard stdio.c since it provides its own via DOOM_SOURCES).
compile-doom: $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@mkdir -p Software/ASM/Output
	./Scripts/BCC/compile_modern_c.sh \
		Software/ASM/crt0/crt0_userbdos.asm \
		Software/C/libc/string/string.c \
		Software/C/libc/stdlib/stdlib.c \
		Software/C/libc/stdlib/malloc.c \
		Software/C/libc/ctype/ctype.c \
		Software/C/userlib/src/syscall_asm.asm \
		Software/C/userlib/src/syscall.c \
		Software/C/userlib/src/time.c \
		$(DOOM_SOURCES) \
		$(DOOM_FLAGS) \
		-o Software/ASM/Output/code.bin
	@mkdir -p Files/BRFS-init/bin
	@cp Software/ASM/Output/code.bin Files/BRFS-init/bin/doom
	@echo "Binary copied to Files/BRFS-init/bin/doom"

compile-userbdos: $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@if [ -z "$(file)" ]; then \
		echo "Usage: make compile-userbdos file=<c_filename_in_userBDOS_dir_without_extension>"; \
		echo "Example: make compile-userbdos file=snake"; \
		echo "Available programs:"; \
		find Software/C/userBDOS -name "*.c" -type f | sed 's|Software/C/userBDOS/||' | sed 's|.c||' | sort; \
		exit 1; \
	fi
	@mkdir -p Software/ASM/Output
	./Scripts/BCC/compile_modern_c.sh \
		$(USERLIB_SOURCES) \
		Software/C/userBDOS/$(file).c \
		$(wildcard Software/C/userBDOS/$(file)_asm.asm) \
		$(USERLIB_FLAGS) \
		-o Software/ASM/Output/code.bin
	@mkdir -p Files/BRFS-init/bin
	@cp Software/ASM/Output/code.bin Files/BRFS-init/bin/$(file)
	@echo "Binary copied to Files/BRFS-init/bin/$(file)"

compile-userbdos-all: $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@rm -rf Files/BRFS-init/bin
	@mkdir -p Files/BRFS-init/bin
	@echo "Compiling all userBDOS programs (modern C)..."
	@FAILED=0; TOTAL=0; \
	for src in Software/C/userBDOS/*.c; do \
		name=$$(basename "$$src" .c); \
		TOTAL=$$((TOTAL + 1)); \
		echo ""; \
		echo "=== [$$TOTAL] Compiling $$name ==="; \
		EXTRA_ASM=""; \
		if [ -f "Software/C/userBDOS/$${name}_asm.asm" ]; then \
			EXTRA_ASM="Software/C/userBDOS/$${name}_asm.asm"; \
		fi; \
		if ./Scripts/BCC/compile_modern_c.sh $(USERLIB_SOURCES) "$$src" $$EXTRA_ASM $(USERLIB_FLAGS) -o Software/ASM/Output/code.bin > /dev/null 2>&1; then \
			cp Software/ASM/Output/code.bin Files/BRFS-init/bin/$$name; \
			echo "  -> Files/BRFS-init/bin/$$name"; \
		else \
			echo "  FAILED: $$name"; \
			FAILED=$$((FAILED + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "============================================================"; \
	echo "Compiled $$((TOTAL - FAILED))/$$TOTAL programs to Files/BRFS-init/bin/"; \
	if [ $$FAILED -gt 0 ]; then \
		echo "WARNING: $$FAILED program(s) failed to compile"; \
		exit 1; \
	fi; \
	echo "============================================================"

# --- B32CC (legacy — userBDOS only, until self-hosting) ---

compile-userbdos-b32cc: $(B32CC_OUTPUT)
	@if [ -z "$(file)" ]; then \
		echo "Usage: make compile-userbdos-b32cc file=<c_filename_in_userBDOS_dir_without_extension>"; \
		echo "Example: make compile-userbdos-b32cc file=snake"; \
		echo "Available programs:"; \
		find Software/C/userBDOS -name "*.c" -type f | sed 's|Software/C/userBDOS/||' | sed 's|.c||' | sort; \
		exit 1; \
	fi
	./Scripts/BCC/compile_user_bdos.sh $(file)
	@mkdir -p Files/BRFS-init/bin
	@cp Software/ASM/Output/code.bin Files/BRFS-init/bin/$(file)
	@echo "Binary copied to Files/BRFS-init/bin/$(file)"

compile-userbdos-all-b32cc: $(B32CC_OUTPUT)
	@rm -rf Files/BRFS-init/bin
	@mkdir -p Files/BRFS-init/bin
	@echo "Compiling all userBDOS programs (B32CC)..."
	@FAILED=0; TOTAL=0; \
	for src in Software/C/userBDOS/*.c; do \
		name=$$(basename "$$src" .c); \
		TOTAL=$$((TOTAL + 1)); \
		echo ""; \
		echo "=== [$$TOTAL] Compiling $$name ==="; \
		if ./Scripts/BCC/compile_user_bdos.sh "$$name" > /dev/null 2>&1; then \
			cp Software/ASM/Output/code.bin Files/BRFS-init/bin/$$name; \
			echo "  -> Files/BRFS-init/bin/$$name"; \
		else \
			echo "  FAILED: $$name"; \
			FAILED=$$((FAILED + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "============================================================"; \
	echo "Compiled $$((TOTAL - FAILED))/$$TOTAL programs to Files/BRFS-init/bin/"; \
	if [ $$FAILED -gt 0 ]; then \
		echo "WARNING: $$FAILED program(s) failed to compile"; \
		exit 1; \
	fi; \
	echo "============================================================"

# =============================================================================
# Hardware Programming
# =============================================================================

run-uart:
	./Scripts/Programmer/UART/run_uart.sh

uart-monitor:
	.venv/bin/python3 Scripts/Programmer/UART/uart_monitor.py -p $(uart_port)

run-asm-uart: compile-asm run-uart

run-c-baremetal-uart: compile-c-baremetal run-uart

run-bdos: compile-bdos run-uart

flash-c-baremetal-spi: compile-c-baremetal $(QBE_OUTPUT) $(CPROC_OUTPUT)
	@if [ -z "$(file)" ]; then \
		echo "Usage: make flash-c-baremetal-spi file=<c_filename_in_bareMetal_dir_without_extension>"; \
		echo "Example: make flash-c-baremetal-spi file=libtests/test_term"; \
		echo "Available programs:"; \
		find Software/C/bareMetal -name "*.c" -type f | grep -v "tmp" | grep -v "flash_writer" | sed 's|Software/C/bareMetal/||' | sed 's|.c||' | sort; \
		exit 1; \
	fi
	./Scripts/Programmer/flash_spi.sh

flash-bdos: compile-bdos
	./Scripts/Programmer/flash_bdos.sh

# =============================================================================
# Asset Conversion
# =============================================================================

# W3D texture order
W3D_TEXTURES = Files/textures/eagle.png \
               Files/textures/redbrick.png \
               Files/textures/purplestone.png \
               Files/textures/greystone.png \
               Files/textures/bluestone.png \
               Files/textures/mossy.png \
               Files/textures/wood.png \
               Files/textures/colorstone.png

W3D_TEX_OUT  = Files/BRFS-init/data/w3d/textures.dat

convert-w3d-textures:
	@mkdir -p $(dir $(W3D_TEX_OUT))
	@.venv/bin/python3 Scripts/Graphics/convert_textures.py -o $(W3D_TEX_OUT) $(W3D_TEXTURES)

# =============================================================================
# FNP (Network Programming)
# =============================================================================

# Target FPGC device number (1-5). MAC addresses are 02:B4:B4:00:00:0X.
# Usage: make fnp-sync-files dev=3
dev ?= 1
FNP_MAC = 02:B4:B4:00:00:0$(dev)

fnp-detect-iface:
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py detect-iface

fnp-upload-text:
	@if [ -z "$(file)" ] || [ -z "$(dest)" ]; then \
		echo "Usage: make fnp-upload-text file=<local_file> dest=<fpgc_path> [dev=1-5]"; \
		echo "Example: make fnp-upload-text file=readme.txt dest=/user/readme.txt dev=2"; \
		exit 1; \
	fi
	FNP_TARGET_MAC="$(FNP_MAC)" ./Scripts/Programmer/Network/fnp_upload_text.sh $(file) $(dest)

fnp-upload-userbdos: compile-userbdos
	@if [ -z "$(file)" ]; then \
		echo "Usage: make fnp-upload-userbdos file=<name> [dev=1-5]"; \
		echo "Example: make fnp-upload-userbdos file=hello dev=3"; \
		echo "Available programs:"; \
		find Software/C/userBDOS -name "*.c" -type f 2>/dev/null | grep -v "tmp" | sed 's|Software/C/userBDOS/||' | sed 's|.c||' | sort; \
		exit 1; \
	fi
	@echo "Uploading to FPGC: /bin/$(file)"
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) upload Software/ASM/Output/code.bin /bin/$(file)
	@echo "Done! Program uploaded to /bin/$(file)"

fnp-upload-userbdos-b32cc: $(B32CC_OUTPUT)
	@if [ -z "$(file)" ]; then \
		echo "Usage: make fnp-upload-userbdos-b32cc file=<name> [flags=<B32CC flags>] [dev=1-5]"; \
		echo "Example: make fnp-upload-userbdos-b32cc file=hello dev=3"; \
		echo "Available programs:"; \
		find Software/C/userBDOS -name "*.c" -type f 2>/dev/null | grep -v "tmp" | sed 's|Software/C/userBDOS/||' | sed 's|.c||' | sort; \
		exit 1; \
	fi
	FNP_TARGET_MAC="$(FNP_MAC)" ./Scripts/Programmer/Network/fnp_upload_userbdos.sh $(file) $(flags)

fnp-keyboard:
	FNP_TARGET_MAC="$(FNP_MAC)" ./Scripts/Programmer/Network/fnp_keyboard.sh

fnp-sync-files:
	@echo "Syncing to device $(dev) (MAC $(FNP_MAC))"
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) sync-files Files/BRFS-init

# Launch a command on a specific device: make fnp-run dev=2 cmd="mbrot_client"
fnp-run:
	@if [ -z "$(cmd)" ]; then \
		echo "Usage: make fnp-run cmd=<command> [dev=1-5]"; \
		exit 1; \
	fi
	@echo "Sending '$(cmd)' + Enter to device $(dev) ($(FNP_MAC))"
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) key "$(cmd)"
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) keycode 0x0A

# Compile, upload, and run a userBDOS program in one step
run-userbdos: compile-userbdos
	@echo "Uploading $(file) to /bin/$(file) on device $(dev) ($(FNP_MAC))"
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) upload Software/ASM/Output/code.bin /bin/$(file)
	@echo "Launching $(file)..."
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) key "$(file)"
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) keycode 0x0A

# Compile, upload, and run Doom in one step
run-doom: compile-doom
	@echo "Uploading doom binary to /bin/doom on device $(dev) ($(FNP_MAC))"
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) upload Software/ASM/Output/code.bin /bin/doom
	@echo "Launching doom..."
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) key "doom"
	@.venv/bin/python3 Scripts/Programmer/Network/fnp_tool.py --mac $(FNP_MAC) keycode 0x0A

# UART port and debug capture duration (seconds)
uart_port ?= /dev/ttyUSB0
duration ?= 3

# Compile, upload via FNP, run, and capture UART debug output:
#   make fnp-debug-userbdos file=hello [dev=1] [duration=3] [uart_port=/dev/ttyUSB0]
fnp-debug-userbdos: compile-userbdos
	@.venv/bin/python3 Scripts/Programmer/fnp_debug_capture.py \
		--mac $(FNP_MAC) --cmd $(file) \
		--bin Software/ASM/Output/code.bin --dest /bin/$(file) \
		--duration $(duration) --port $(uart_port)

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
	-$(MAKE) -C $(QBE_DIR) clean
	-$(MAKE) -C $(CPROC_DIR) clean
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
	@echo "--- Setup ---"
	@echo "  all                 - Build all tools (venv, compilers, assembler, run checks)"
	@echo "  venv                - Create or sync virtual environment"
	@echo ""
	@echo "--- ASMPY Assembler ---"
	@echo "  asmpy-install       - Install ASMPY assembler tool"
	@echo "  asmpy-uninstall     - Uninstall ASMPY assembler tool"
	@echo "  test-asmpy          - Run ASMPY-specific tests"
	@echo "  test-asm-link       - Run asm-link byte-for-byte regression tests vs ASMPY"
	@echo "  test-cpp            - Run cpp byte-for-byte regression tests vs gcc cpp"
	@echo "  test-term2          - Run libterm v2 host unit tests"
	@echo "  test-shell-host     - Run BDOS shell (lex/parse/expand) host unit tests"
	@echo "  test-host           - Run all host-side C unit tests"
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
	@echo "--- QBE (Backend Compiler) ---"
	@echo "  qbe                 - Build the QBE backend compiler for B32P3"
	@echo "  clean-qbe           - Clean QBE build artifacts"
	@echo ""
	@echo "--- cproc (C Frontend) ---"
	@echo "  cproc               - Build the cproc C frontend for B32P3"
	@echo "  clean-cproc         - Clean cproc build artifacts"
	@echo ""
	@echo "--- C Test Suite ---"
	@echo "  test-c              - Run all C compiler tests (parallel)"
	@echo "  test-c-single       - Run a single C test"
	@echo "                        Usage: make test-c-single file=<test_file>"
	@echo ""
	@echo "--- Documentation ---"
	@echo "  docs-serve          - Run documentation website locally"
	@echo "  docs-deploy         - Deploy documentation website"
	@echo ""
	@echo "--- Simulation ---"
	@echo "  sim-cpu             - Run CPU simulation"
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
	@echo "  compile-bdos        - Compile BDOS kernel"
	@echo "  compile-userbdos    - Compile a single userBDOS program"
	@echo "                        Usage: make compile-userbdos file=<filename>"
	@echo "  compile-userbdos-all - Compile ALL userBDOS programs"
	@echo "  compile-doom        - Compile Doom"
	@echo ""
	@echo "  B32CC (legacy — userBDOS only, until self-hosting):"
	@echo "  compile-userbdos-b32cc    - Compile userBDOS program with B32CC"
	@echo "  compile-userbdos-all-b32cc - Compile ALL userBDOS with B32CC"
	@echo ""
	@echo "--- Hardware Programming ---"
	@echo "  run-uart              - Run compiled binary via UART"
	@echo "  uart-monitor          - Launch UART monitor"
	@echo "                          Usage: make uart-monitor [uart_port=/dev/ttyUSB0]"
	@echo "  run-asm-uart          - Compile and run ASM binary via UART"
	@echo "  run-c-baremetal-uart  - Compile and run C binary via UART"
	@echo "  run-bdos              - Compile and run BDOS via UART"
	@echo "  flash-c-baremetal-spi - Flash C binary to SPI flash"
	@echo "                          Usage: make flash-c-baremetal-spi file=<filename>"
	@echo "  flash-bdos            - Flash BDOS to SPI flash"
	@echo ""
	@echo "--- Asset Conversion ---"
	@echo "  convert-w3d-textures  - Convert W3D textures to binary"
	@echo ""
	@echo "--- FNP (Network Programming) ---"
	@echo "  All FNP commands accept dev=1-5 to select target device (default: 1)"
	@echo "  MAC addresses: 02:B4:B4:00:00:01 through :05"
	@echo ""
	@echo "  fnp-detect-iface      - Print auto-detected Ethernet interface"
	@echo "  fnp-upload-text       - Upload a text file to the FPGC"
	@echo "                          Usage: make fnp-upload-text file=<local> dest=<fpgc_path> [dev=N]"
	@echo "  fnp-upload-userbdos   - Compile and upload userBDOS program to /bin"
	@echo "                          Usage: make fnp-upload-userbdos file=<name> [dev=N]"
	@echo "  fnp-upload-userbdos-b32cc - Compile (B32CC) and upload userBDOS program to /bin"
	@echo "  fnp-keyboard          - Interactive keyboard streaming to FPGC"
	@echo "                          Usage: make fnp-keyboard [dev=N]"
	@echo "  fnp-sync-files        - Sync Files/BRFS-init/ to FPGC root filesystem"
	@echo "                          Usage: make fnp-sync-files [dev=N]"
	@echo "  fnp-run               - Run a shell command on an FPGC device"
	@echo "                          Usage: make fnp-run cmd=<command> [dev=N]"
	@echo "  run-userbdos          - Compile, upload, and run a userBDOS program on FPGC"
	@echo "                          Usage: make run-userbdos file=<name> [dev=N]"
	@echo "  run-doom              - Compile, upload, and run Doom on FPGC"
	@echo "                          Usage: make run-doom [dev=N]"
	@echo "  fnp-debug-userbdos    - Compile, upload, run and capture UART debug output"
	@echo "                          Usage: make fnp-debug-userbdos file=<name> [dev=N] [duration=3] [uart_port=/dev/ttyUSB0]"
	@echo ""
	@echo "--- Cleanup ---"
	@echo "  clean               - Clean all build artifacts and environments"
	@echo ""
	@echo "==================================================================="

