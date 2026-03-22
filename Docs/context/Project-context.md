# FPGC Project Context

This document provides a complete context for understanding the FPGC (FPGA Computer) project architecture. This file is mainly intended for AI tooling like Github Copilot.

## Project Overview

FPGC is a complete custom computer system implemented on FPGA hardware, featuring:

- Custom 32-bit RISC CPU (B32P3)
- Modern C toolchain: cproc (C11 frontend) → QBE (optimizing backend) → ASMPY (assembler)
- Legacy C compiler (B32CC) — still used for B32CC test suite and self-hosting on BDOS
- GPU with pixel rendering capabilities
- SDRAM controller with L1 instruction and data caches
- Custom File System (BRFS)
- Operating System (BDOS) — fully functional with shell, filesystem, multitasking, networking

The goal of this project is to create a fully functional computer system from the ground up, including hardware design, CPU architecture, compiler toolchain, and software libraries, with the end goal of running Doom on BDOS.

## CPU Architecture (B32P3)

### Pipeline Stages

B32P3 is a classic 5-stage pipelined CPU:
- 32-bit pipelined CPU with classic 5-stage pipeline
- Third iteration of B32P with focus on simplified hazard handling

Features:
- 5 stage pipeline (Classic MIPS-style)
  - IF:  Instruction Fetch
  - ID:  Instruction Decode & Register Read
  - EX:  Execute (ALU operations, branch resolution)
  - MEM: Memory Access (Load/Store)
  - WB:  Write Back
- Simple hazard detection (load-use only)
- Data forwarding (EX→EX, MEM→EX)
- 32 bits, byte-addressable (char=1 byte, short=2 bytes, int=4 bytes, pointer=4 bytes)
- 32 bit address space for 4 GiB of addressable memory
  - 27 bits jump constant for 512 MiB of easily jumpable instruction memory
- Hardware byte/halfword/word load/store: `read`/`write` (word), `readb`/`writeb` (byte), `readh`/`writeh` (halfword)
- No branch prediction (taken branches flush the 5-stage pipeline)
- No MMU or virtual memory
- Hardware fixed-point: `multfp`/`divfp` (16.16) on GPRs; FP64 coprocessor (Q32.32) with 8 dedicated 64-bit registers

Memory Map (byte addresses):
- SDRAM:  0x0000000 - 0x3FFFFFF (64 MiB)
- I/O:    0x1C000000 - 0x1C0000FF
- ROM:    0x1E000000 - 0x1E000FFF (4 KiB) - CPU starts here
- VRAM32: 0x1E400000 - 0x1E40107F (pattern + palette tables)
- VRAM8:  0x1E800000 - 0x1E808007 (tile + color tables)
- VRAMPX: 0x1EC00000 - 0x1EC4AFFF (320×240 pixel framebuffer)
- PC/HW:  0x1F000000 - 0x1F000007 (PC backup, HW stack pointer)

### ISA (Instruction Set Architecture)

All instructions are 32 bits. See ISA.md for full instruction set details.
All assembly related information in Assembler.md.

### Registers

There are 16 registers, but r0 is hardwired to 0.
- r0: hardwired zero
- r1–r12: general purpose (r1=return value, r4–r7=function args)
- r13: stack pointer (SP)
- r14: frame pointer (FP)
- r15: return address (RA)


## C Toolchain

### Modern Toolchain (Primary)

The primary C toolchain uses three components in a pipeline:

```
source.c → cpp (preprocessor) → cproc (C11 frontend) → QBE IR → QBE (backend) → B32P3 assembly → ASMPY (assembler) → binary
```

- **cproc**: C11-compliant frontend that emits QBE intermediate representation. Supports full C11 including structs, variadics, separate compilation. Located at `BuildTools/cproc/`.
- **QBE**: Optimizing compiler backend with graph-coloring register allocation, SSA, constant folding, instruction selection. B32P3 backend in `BuildTools/QBE/b32p3/`. Produces ASMPY-compatible assembly.
- **ASMPY**: Python assembler that produces flat binaries. Supports ELF-style data directives, label offsets, and an assembly-level linker for multi-file compilation. Located at `BuildTools/ASMPY/asmpy/`.

Build: `make qbe cproc` from project root.

Pipeline with linker (multi-file):
```
file1.c → cproc → QBE → file1.asm ─┐
file2.c → cproc → QBE → file2.asm ─┼→ linker → combined.asm → ASMPY → .list → .bin
crt0.asm ───────────────────────────┘
```

Compile script: `Scripts/BCC/compile_modern_c.sh` handles the full pipeline including preprocessing, compilation, linking, and assembly. Supports mixed `.c` and `.asm` inputs, `-h` (header), `-i` (PIC), `-s` (syscall vector), `--libc`, and `-I` include paths.

### B32CC (Legacy)

B32CC is a single-pass C compiler based on Smaller C, targeting the B32P3 ISA. It was the original compiler for the project and is still used for:
- B32CC test suite (`Tests/B32CC/`) — validates B32CC itself
- Self-hosting on BDOS (compiling user programs on the FPGC itself)
- userBDOS compilation via B32CC (until the new toolchain handles this)

Key limitations: no linker (single compilation unit via orchestrator pattern), no C11 features, limited struct handling, no optimization.

### Location

- Modern toolchain: `BuildTools/cproc/`, `BuildTools/QBE/`, `BuildTools/ASMPY/`
- B32CC: `BuildTools/B32CC/smlrc.c` (compiler), `BuildTools/B32CC/cgB32P3.inc` (backend)

## Assembler (ASMPY)

ASMPY is written in Python and assembles B32P3 assembly into binary.

### Assembler Location

- Source: `BuildTools/ASMPY/asmpy/`
- Tests: `BuildTools/ASMPY/tests/`

### Key Features

**Pseudo-instructions** (expand to multiple real instructions):

- `load32 constant reg` → `load low16 reg` + `loadhi high16 reg`
- `addr2reg label reg` → `load label_low reg` + `loadhi label_high reg`

**Sections:**

- There is support for different sections, but all they do is that .code sections are compiled first and all the other sections (like .data) are moved below the .code section in memory.

**ELF-style directives** (for QBE output):

- `.int`, `.byte`, `.short`, `.ascii`, `.fill` — data directives with byte-level packing
- `.balign`, `.globl`/`.global`, `.text` — alignment and symbol visibility
- Label offset syntax: `SYMBOL+OFFSET` in instructions and data

**Assembly-level linker** (`asmpy/linker.py`):

- Combines multiple `.asm` files into one assembly
- Renames local labels (`.L*`) to avoid cross-file conflicts
- Validates global symbol uniqueness

**Directives:**

- Labels: `Label_name:`
- Comments: `;` or `/* */`

## Software Structure

### Directory Layout

```
Software/C/
├── libc/                    # Standard C library (picolibc-derived subset)
│   ├── include/             # Standard headers: string.h, stdlib.h, stdio.h, etc.
│   ├── string/string.c     # String functions
│   ├── stdlib/stdlib.c      # Standard library + malloc.c
│   ├── stdio/stdio.c       # printf/sprintf (tinystdio-based)
│   ├── ctype/ctype.c       # Character classification
│   └── sys/                 # System stubs: syscalls.c, hwio.asm, _exit.asm
│
├── libfpgc/                 # FPGC hardware abstraction library
│   ├── include/             # fpgc.h (mem map + I/O), gpu_hal.h, term.h, uart.h, etc.
│   └── src/                 # Drivers: spi.c, uart.c, timer.c, ch376.c, enc28j60.c,
│                            #   gpu_hal.c, gpu_fb.c, term.c, brfs.c, sys.c, sys_asm.asm
│
├── bdos/                    # BDOS kernel (modern C, separately compiled)
│   ├── include/             # Kernel headers: bdos.h, bdos_syscall.h, bdos_hid.h, etc.
│   └── src files            # main.c, init.c, syscall.c, shell.c, hid.c, eth.c, etc.
│
├── userlib/                 # User program library (syscalls, FNP, plot, fixed-point)
│   ├── include/             # syscall.h, fnp.h, plot.h, fixed64.h, fixedmath.h, time.h
│   └── src/                 # syscall.c, syscall_asm.asm, fnp.c, plot.c, etc.
│
├── userBDOS/                # User programs (compiled with modern toolchain)
│
├── bareMetal/               # Bare-metal test programs
│
└── b32cc/                   # Archived B32CC-era code
    ├── BDOS/                # Old B32CC BDOS (archived)
    ├── libs/                # Old orchestrator libraries (archived)
    └── userBDOS/            # Old B32CC user programs (being ported)
```

### Startup Files (crt0)

Located at `Software/ASM/crt0/`:

| File | Program Type | Stack Init | Return Behavior |
|------|-------------|------------|-----------------|
| `crt0_baremetal.asm` | Bare metal (tests, flash_writer) | SP=0x1DFFFFC, FP=0 | UART TX r1, halt |
| `crt0_bdos.asm` | BDOS kernel | SP=0x3DFFFC, FP=0 | UART TX r1, halt |
| `crt0_userbdos.asm` | User programs under BDOS | FP=0, push/pop r15 | Return to BDOS |

## Testing Infrastructure

### Make Commands

**When you change Verilog code:**

Run `make test-cpu` to run all CPU tests, followed by `make test-b32cc` and `make test-modern-c` to run all C compiler tests. If a test fails, you can run it individually using `make test-cpu-single file=<path>` or `make test-b32cc-single file=<path>`. To debug with Verilog display output, run `make debug-cpu file=<path>` or `make debug-b32cc file=<path>`.

**When you change the modern C toolchain (cproc, QBE, ASMPY):**

Run `make test-modern-c` to run all modern C tests. If a test fails, run individually with `make test-modern-c-single file=<path>`.

**When you change B32CC compiler code:**

Run `make test-b32cc` to run all B32CC compiler tests.

**When you change BDOS or a user program:**

Just compile the code (`make compile-bdos` or `make compile-userbdos file=<name>`). No need to run test suites.

### How the testing framework works

- The C code is compiled to B32P3 assembly (via cproc+QBE or B32CC).
- ASMPY assembles the assembly into .list files, which initialize the memories in the Verilog testbench.
- The Verilog testbench simulates the CPU running the program, capturing UART output.
- The test framework checks the UART output against expected values.

Note that the tests use `cpu_tests_tb.v` as simulation testbench, while the `make debug` commands use `cpu_tb.v` as simulation testbench.

---

## C Programming Guide

### Modern Toolchain (Recommended)

The modern toolchain supports full C11:

- All standard data types including proper `char` (1 byte), `short` (2 bytes), `int` (4 bytes)
- Full struct/union support including return by value
- Standard `#include <header.h>` with picolibc-derived standard library
- Separate compilation with linker — each `.c` file compiled independently
- No inline assembly — assembly code goes in separate `.asm` files
- Variadics, complex initializers, function pointers — all work correctly

**UserBDOS program template:**
```c
#include <syscall.h>

int main(void)
{
    sys_print_str("Hello from BDOS!\n");
    return 0;
}
```

Compile: `make compile-userbdos file=hello`

**Multi-file program:**
```c
// main.c
#include <string.h>
#include <stdlib.h>
#include <syscall.h>
#include <fnp.h>

int main(void) { ... }
```

### B32CC Compiler (Legacy)

B32CC is a single-pass C compiler with significant limitations. See `Docs/plans/modern-c-compiler-support.md` for details on why the modern toolchain was created.

Key limitations: no linker (orchestrator pattern required), no C11, limited struct handling, no optimization, limited variadics.

### Testing C Code

**Modern C test file format:**

```c
// Test file: Tests/C/XX_category/test_name.c
#include <string.h>  // if needed

int main(void)
{
    int result = 42;
    return result; // expected=42
}

void interrupt(void) {}
```

**B32CC test file format (legacy):**

```c
// Test file: Tests/B32CC/XX_category/test_name.c
#define COMMON_STDLIB
#include "libs/common/common.h"

int main() {
    return 5; // expected=5
}

void interrupt() {}
```
