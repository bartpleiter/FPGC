# C Compiler

The FPGC has two C compilation toolchains. The primary toolchain is based on [cproc](https://github.com/michaelforney/cproc) by Michael Forney (C frontend) and [QBE](https://c9x.me/compile/) by Quentin Carbonneaux (backend), which together form a proper multi-file C11 compiler with standard library support. The legacy toolchain is [B32CC](#b32cc-legacy), derived from [SmallerC](https://github.com/alexfru/SmallerC) by Alexey Frunze, which is a single-pass C89/C99 compiler primarily kept for its self-hosting capability on the FPGC and for compiling userBDOS programs until the new toolchain can be self-hosted.

Both toolchains compile C source code into B32P3 assembly, which is then assembled and linked by [ASMPY](Assembler.md) into executable machine code.

## Modern C Toolchain (cproc + QBE)

The modern toolchain consists of three stages:

1. **cproc** — C11 frontend, parses C source and emits QBE intermediate representation
2. **QBE** — Backend optimizer and code generator, transforms QBE IR into B32P3 assembly
3. **ASMPY** — Assembler and linker, resolves cross-file symbols and produces the final binary

Both cproc and QBE have been adapted from their upstream versions to target the B32P3 architecture.

### Features

- C11 language support (structs, enums, typedefs, switch, designated initializers, etc.)
- Multi-file compilation with proper linking (separate `.c` and `.asm` files)
- Standard include paths and preprocessor via `cpp`
- Freestanding C standard library (`libc`) tailored for the FPGC
- Hardware abstraction library (`libfpgc`) for all FPGC peripherals

### Limitations

- No inline assembly (use separate `.asm` files and the assembler's `.global` directive instead)
- No `volatile` qualifier for memory-mapped I/O (all MMIO must go through `hwio_write()`/`hwio_read()` assembly helpers)
- No floating point types (`float`, `double`) — use the FP64 coprocessor intrinsics via B32CC instead
- No 64-bit integers (`long long`)

### Quick Start

```bash
# Compile a bare-metal C program
make compile-c-baremetal file=demo/mandelbrot

# Compile and run BDOS
make run-bdos

# Compile a userBDOS program
make compile-userbdos file=snake
```

The build script `Scripts/BCC/compile_modern_c.sh` handles the full pipeline. It accepts mixed `.c` and `.asm` source files, supports `-I` include paths, and passes through flags like `--libc`, `-h` (add header), `-i` (position-independent), and `-s` (syscall vector) to the assembler.

### Standard Library (libc)

The FPGC ships with a minimal freestanding C standard library at `Software/C/libc/`, inspired by [picolibc](https://github.com/picolibc/picolibc). It provides:

| Header | Functions |
|--------|-----------|
| `<string.h>` | `memcpy`, `memmove`, `memset`, `memcmp`, `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strcat`, `strncat`, `strstr`, `strchr`, `strrchr`, `strtok` |
| `<stdlib.h>` | `atoi`, `strtol`, `strtoul`, `abs`, `qsort`, `bsearch`, `rand`, `srand`, `malloc`, `free`, `realloc` |
| `<stdio.h>` | `printf`, `sprintf`, `snprintf`, `vsnprintf`, `puts`, `putchar`, `getchar`, `fopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fprintf`, `fseek`, `ftell`, `feof` |
| `<ctype.h>` | `isalpha`, `isdigit`, `isalnum`, `isspace`, `toupper`, `tolower`, etc. |
| `<stddef.h>`, `<stdint.h>`, `<stdbool.h>`, `<limits.h>`, `<errno.h>`, `<stdarg.h>`, `<assert.h>` | Standard types and macros |

The `malloc` implementation uses a first-fit free list allocator with `_sbrk()` for heap expansion. Low-level I/O (`_write`, `_read`) goes through UART by default, with BDOS syscall redirection when running under the kernel.

### Hardware Abstraction Library (libfpgc)

The hardware abstraction library at `Software/C/libfpgc/` provides drivers for all FPGC peripherals:

| Module | Provides |
|--------|----------|
| `sys/sys.c` | Interrupt ID reading, boot mode, microsecond timer |
| `io/spi.c` | SPI bus controller (6 buses: 2× Flash, 2× USB, 1× Ethernet, 1× SD) |
| `io/uart.c` | UART TX/RX with interrupt-driven receive ring buffer |
| `io/timer.c` | 3 hardware timers with callbacks, periodic mode, `delay()` |
| `io/spi_flash.c` | SPI flash read/write/erase operations |
| `io/ch376.c` | CH376 USB host controller driver |
| `io/enc28j60.c` | ENC28J60 Ethernet controller driver |
| `gfx/gpu_hal.c` | GPU VRAM access (tile map, palette, sprite, pixel planes) |
| `gfx/gpu_fb.c` | Framebuffer graphics (lines, rectangles, circles, blit) |
| `gfx/gpu_data_ascii.c` | 32 color palettes + 256 ASCII tile patterns |
| `term/term.c` | 40×25 terminal emulator on the window tile plane |
| `mem/debug.c` | Hex dump utility |
| `fs/brfs.c` | BRFS filesystem driver |

All memory-mapped I/O is handled through `hwio_write()`/`hwio_read()` assembly helpers in `sys/hwio.asm`, which work around the lack of `volatile` support in cproc.

### Program Structure

The program should define a `main()` function as the entry point and an `interrupt()` function (unless `-user-bdos` is used) for handling interrupts:

```c
int main() {
    // Main entry point
    return 37;  // Return value is sent over UART
}

void interrupt() {
    // Interrupt handler, can be empty if not used
}
```

### Register Usage & Calling Convention

Both toolchains share the same calling convention:

| Register | Alias | Purpose | Preserved? |
|----------|-------|---------|------------|
| r0 | zero | Always zero | N/A |
| r1 | v0 | Return value / temp | No |
| r2-r3 | - | Temp registers | No |
| r4 | a0 | Argument 0 | No |
| r5 | a1 | Argument 1 | No |
| r6 | a2 | Argument 2 | No |
| r7 | a3 | Argument 3 | No |
| r8-r11 | s0-s3 | Callee-saved (cproc) / Temp (B32CC) | cproc: Yes, B32CC: No |
| r12 | t4 | Temp register | No |
| r13 | sp | Stack pointer | Yes |
| r14 | fp | Frame pointer | Yes |
| r15 | ra | Return address | Yes |

!!! note
    The modern toolchain (cproc/QBE) treats r8-r11 as callee-saved, while B32CC treats them as caller-saved temporaries. This means object code from one toolchain is not ABI-compatible with the other.

## B32CC (Legacy)

B32CC is the original C compiler for the FPGC, derived from [SmallerC](https://github.com/alexfru/SmallerC) by Alexey Frunze. It is a single-pass compiler that supports most of C89/C99 and compiles everything into a single assembly file (no linker). B32CC is primarily kept for its self-hosting capability (it can compile itself to run natively on the FPGC under BDOS) and for compiling userBDOS programs until the modern toolchain can be self-hosted.

B32CC source code is located at `Software/C/b32cc/`, and its associated libraries are at `Software/C/b32cc/libs/`.

### B32CC Limitations

- No linker — all code compiles into a single assembly file
- No floating point types
- No 64-bit integers (`long long`)
- Limited optimizations due to single-pass design

### B32CC Quick Start

```bash
# Compile a userBDOS program with B32CC
make compile-userbdos-b32cc file=snake
```

### Function Calling Sequence

**Before call (caller):**

1. First 4 arguments passed in `r4-r7` (a0-a3)
2. Additional arguments pushed to (hardware) stack (right-to-left)
3. Call instruction saves return address in `r15` (ra)

**Prologue (callee):**

1. Save argument registers (r4-r7) to stack if needed
2. Adjust stack pointer to allocate local variables
3. Save frame pointer (`r14`) to stack
4. Set frame pointer to current stack position  
5. Save return address (`r15`) if function makes calls (non-leaf)

**Epilogue (callee):**

1. Restore return address if saved (non-leaf functions)
2. Restore frame pointer from stack
3. Restore stack pointer
4. Jump to return address (`jumpr 0 r15`)

**After call (caller):**

1. Return value in `r1` (v0), optionally `r2` (v1)
2. Caller cleans up stack arguments if any

#### Stack Frame Layout

```
Higher addresses
+------------------+
| Return address   |
+------------------+
| Saved FP         |
+------------------+
| Local var 1      |
| Local var 2      |
| ...              |
| Local var N      |
+------------------+  <- sp
Lower addresses
```

### Supported and Unsupported C Features

!!! note
    This list is mostly focused on uncommon features that are supported, or common features that are not supported, as you can assume that most of the common features of C89/C99 are supported unless otherwise noted.

**Supported:**

- Fixed-point 16.16 intrinsics for hardware acceleration: `__multfp(a, b)` and `__divfp(a, b)`
- Fixed-point 64-bit (Q32.32) intrinsics using the FP64 coprocessor (see [FP64 Intrinsics](#fp64-intrinsics) below)

**Not Supported:**

- Floating point types: `float`, `double`, `long double`
- 64-bit integers: `long long` (use the FP64 coprocessor intrinsics for 64-bit fixed-point arithmetic instead)
- Bit fields in structs
- Variable-length arrays (VLAs)
- Complex numbers (`_Complex`)
- Function-like macros with complex logic (preprocessor limitations)
- Variable-length argument lists without proper declarations
- `#pragma` directives
- Complex macro expansion
- Stringification (`#`) and token pasting (`##`)
- `#error`, `#warning`

### FP64 Intrinsics

B32CC provides built-in intrinsics for the FP64 coprocessor, enabling 64-bit fixed-point arithmetic (Q32.32 format) from C code. The FP64 coprocessor has 8 dedicated 64-bit registers (`f0`–`f7`), separate from the CPU's general-purpose registers.

#### Q32.32 Format

Each FP64 register holds a signed fixed-point value split into two 32-bit halves:

- **hi** (signed integer part): the whole number portion
- **lo** (unsigned fractional part): represents a fraction in the range [0, 1)

The value is: `value = hi + lo / 2^32`. For example, 3.75 is stored as `hi = 3`, `lo = 0xC0000000`.

#### Available Intrinsics

| Intrinsic | Description |
|-----------|-------------|
| `__fld(fd, hi, lo)` | Load FP64 register `fd` with `{hi, lo}` |
| `__fadd(fd, fa, fb)` | `fd = fa + fb` (Q32.32 add) |
| `__fsub(fd, fa, fb)` | `fd = fa - fb` (Q32.32 subtract) |
| `__fmul(fd, fa, fb)` | `fd = fa × fb` (Q32.32 multiply) |
| `__fsthi(fs)` | Returns the high (integer) word of FP64 register `fs` |
| `__fstlo(fs)` | Returns the low (fractional) word of FP64 register `fs` |

All register arguments (`fd`, `fa`, `fb`, `fs`) are integer constants in the range 0–7, corresponding to registers `f0`–`f7`. The `hi` and `lo` arguments to `__fld` are regular C expressions (variables or constants).

#### FP64 Register Allocation

The FP64 registers are **not** managed by the compiler, the programmer is responsible for choosing which registers to use and avoiding conflicts. Since the FP64 register file is completely separate from the CPU registers, there are no interactions with the compiler's register allocator.

### Inline Assembly

B32CC supports inline assembly using the `asm()` syntax, without the need for `\n` for newlines, and allows `;` for comments. For example:

```c
int main() {
    int result = 5;
    asm(
        "load32 7 r1      ; Load 7 into r1"
        "write -1 r14 r1  ; Write r1 to local variable"
    );
    return result;  // Returns 7
}

void interrupt() {}
```

!!! Warning
    There is no safety for the inline assembly, so you should push and pop registers if they might be used.

### Entry Point Wrapper

B32CC generates a wrapper that:

1. Sets up the stack pointer
2. Sets up a return function (for `main()` to return to)
3. Jumps to `main()`
4. Handles the return value from `main()`
5. Halts execution

The wrapper also contains a jump to the `interrupt()` function for handling interrupts and is automatically included in the generated assembly.
The wrapper changes depending on the command line option used.

### Self-Hosting on the FPGC

B32CC can compile itself to run natively on the FPGC under BDOS. When compiled with itself (using the `__SMALLER_C__` define), several defaults change automatically:

- **`NO_ANNOTATIONS`**: Assembly output annotations are disabled at compile time to reduce output file size (and increase compilation speed).
- **`-user-bdos` mode**: Defaults to on when running on the FPGC, since the native compiler only targets userBDOS programs.
- **Command-line arguments**: Retrieved via `sys_shell_argc()` / `sys_shell_argv()` syscalls instead of traditional `main(argc, argv)`.

Surprisingly, the C compiler was able to run on the no-linker, no-MMU architecture of the FPGC with only minor adjustments.

The output can be further assembled using the `asm` user program and then executed directly all on the FPGC.

### Testing

Both toolchains can be tested in combination with the Assembler (ASMPY) and Verilog simulation:

```bash
# Modern C (cproc + QBE) tests
make test-c
make test-c-single file=01_return/return_constant.c

# B32CC tests
make test-b32cc
make test-b32cc-single file=04_control_flow/if_statements.c

# Debug a B32CC test with GTKWave
make debug-b32cc file=04_control_flow/if_statements.c
```

!!! note
    The test suites capture a lot of things, but are not exhaustive, meaning there are likely still bugs in both compilers.
