# FPGC Project Context

This document provides comprehensive context for understanding the FPGC (FPGA Computer) project architecture.

## Project Overview

FPGC is a complete custom computer system implemented on FPGA hardware, featuring:

- Custom 32-bit RISC CPU (B32P3)
- Custom C compiler (B32CC)
- Custom assembler (ASMPY)
- GPU with pixel rendering capabilities
- SDRAM controller with L1 instruction and data caches
- Custom File System (BRFS).
- TODO: a custom Operating System (BDOS)

The goal of this project is to create a fully functional computer system from the ground up, including hardware design, CPU architecture, compiler toolchain, and software libraries, with the end goal of running Doom on a custom Operating System (BDOS).

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
- 32 bits, word-addressable only
- 32 bit address space for 16GiB of addressable memory
  - 27 bits jump constant for 512MiB of easily jumpable instruction memory

Memory Map:
- SDRAM:  0x0000000 - 0x6FFFFFF (112MiW)
- I/O:    0x7000000 - 0x77FFFFF
- ROM:    0x7800000 - 0x78003FF (1KiW) - CPU starts here
- VRAM32: 0x7900000 - 0x790041F
- VRAM8:  0x7A00000 - 0x7A02001
- VRAMPX: 0x7B00000 - 0x7B12BFF

### ISA (Instruction Set Architecture)

All instructions are 32 bits, word-addressable. See ISA.md for full instruction set details.
All assembly related information in Assembler.md.

### Registers

There are 16 registers, but r0 is hardwired to 0.


## C Compiler (B32CC)

B32CC is a single-pass C compiler based on Smaller C, targeting the B32P3 ISA.
Since this is a very simple single pass C compiler, many things are not supported (like macro functions, returning a struct from a function, etc). The most important limitation in C programming is that there is NO linker at all, so some clever tricks are used to include library code.

### Location

- Source: `BuildTools/B32CC/smlrc.c` (main compiler)
- Backend: `BuildTools/B32CC/cgB32P3.inc` (B32P3 code generation)

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

-There is support for different sections, but all they do is that .code sections are compiled first and all the other sections (like .data) are moved below the .code section in memory.

**Directives:**

- Labels: `Label_name:`
- Comments: `;`

## Testing Infrastructure

### Make Commands

These are very important for testing and debugging both the CPU and the C compiler.

**When you change Verilog code:**

Run `make test-cpu` to run all CPU tests, followed by `make test-b32cc` to run all C compiler tests (which also test the CPU indirectly). If a test fails, you can run it individually using `make test-cpu-single file=<path>` or `make test-b32cc-single file=<path>`. Finally, to actually debug with verilog display output, run `make debug-cpu file=<path>` or `make debug-b32cc file=<path>`.

**When you change C compiler code:**

Run `make test-b32cc` to run all C compiler tests. If a test fails, you can run it individually using `make test-b32cc-single file=<path>`. To debug with verilog display output, run `make debug-b32cc file=<path>`.

### How the testing framework works

- First, the C code is compiled to B32P3 assembly using B32CC.
- Then, ASMPY assembles the assembly into .list files, which in turn are used to initialize the memories in the Verilog testbench.
- The Verilog testbench simulates the CPU running the program, capturing UART output and register/memory traces.
- Finally, the test framework checks the UART output (in case of B32CC tests, otherwise just the last r15 write) against expected values.

Note that the tests use `cpu_tests_tb.v` as simulation testbench, while the `make debug` commands use `cpu_tb.v` as simulation testbench.

---

## C Programming Guide

### B32CC Compiler Capabilities and Limitations

B32CC is a single-pass C compiler based on Smaller C. Understanding its capabilities and limitations is essential for effective C programming on FPGC.

**Supported C Features:**

- Basic data types: `int` (32-bit), `char` (8-bit), pointers, arrays
- Structs and unions (with limitations)
- Functions with up to ~8 parameters efficiently
- Single-dimensional arrays and pointer arithmetic
- Basic operators: arithmetic, logical, bitwise, comparison
- Control flow: `if`, `else`, `while`, `for`, `do-while`, `switch`, `break`, `continue`, `return`
- Global and local variables
- Function pointers
- Inline assembly via `asm()` statements
- Preprocessor: `#define`, `#include`, `#ifdef`, `#ifndef`, `#if`, `#else`, `#endif`
- Type casting
- Forward declarations

**Critical Limitations Discovered:**

1. **No Complex Macro Expressions**
   - Ternary operators in `#define` macros do NOT work
   - Example: `#define MAX(a,b) ((a) > (b) ? (a) : (b))` - **FAILS**

2. **No Struct Return Values**
   - Functions cannot return structs by value
   - Standard functions like `div()` that return `div_t` cannot be implemented

3. **Limited Static Initializers**
   - Complex static variable initialization is not supported
   - Static arrays with non-constant initializers fail

4. **Variadic Functions Limitations**
   - `va_list`, `va_start`, `va_arg`, `va_end` have limited support
   - Complex format strings in `printf`/`sprintf` may not work correctly

5. **No Floating-Point Support**
   - CPU has no FPU, compiler has no `float` or `double` support
   - All floating-point math must use fixed-point arithmetic

6. **Limited Type Support**
   - No `long long` (64-bit integers)
   - `long` is same as `int` (32-bit)
   - No `unsigned long long`

**Additional Limitations:**

- No function overloading (C doesn't support this anyway)
- Limited optimization (single-pass compiler)
- No inline functions (except via macro hacks)
- Limited string literal handling
- No wide character support (`wchar_t`)
- No standard library by default (must implement yourself)

### C Library Structure

The FPGC project uses a unique library structure due to the absence of a linker. Understanding this structure is crucial for using and extending libraries.

**Directory Layout:**

```
Software/C/libs/
├── common/          # Libraries shared between kernel and user programs
│   └── common.h     # Orchestrator header (includes all common libraries)
├── kernel/          # Kernel-specific libraries
│   └── kernel.h     # Kernel orchestrator
└── user/            # User program libraries
    └── user.h       # User orchestrator
```

**Orchestrator Pattern (No Linker Workaround):**

Since B32CC doesn't support linking, all library code must be included directly in the compilation unit. The orchestrator headers provide flag-based inclusion.

**Best Practices:**

1. **Define flags before including**: Always `#define` library flags before `#include "common.h"`
2. **Don't include .c files directly**: Use the orchestrator header instead
3. **Minimize includes**: Only include what you need to reduce compilation time
4. **Be aware of dependencies**: stdio automatically includes string, stdlib includes string
5. **Use include guards**: All .h files use `#ifndef HEADER_H` guards

### C Standard Library Implementation

The project includes custom implementations of essential C standard library functions, tailored to work without an OS and without floating-point support.

### Testing C Code

**Test File Format:**

```c
// Test file: Tests/C/XX_category/test_name.c
#define COMMON_STDLIB  // Include needed libraries
#include "libs/common/common.h"

int main() {
    // Test code
    int result = some_function();
    
    // Return expected value for test verification
    return result; // expected=42
}

void interrupt() {
   // Required empty interrupt handler
}
```

**Expected Value Comment:**

The test runner checks the return value against the expected value in the comment:

```c
return 5; // expected=5
```
