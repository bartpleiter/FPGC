# C Compiler

The FPGC's C compilation toolchain is based on [cproc](https://github.com/michaelforney/cproc) by Michael Forney (C frontend) and [QBE](https://c9x.me/compile/) by Quentin Carbonneaux (backend), which together form a multi-file C11 compiler with standard library support. The toolchain is self-hosting: it can compile itself and run natively on the FPGC under BDOS.

The toolchain compiles C source code into B32P3 assembly, which is then assembled and linked by [ASMPY](Assembler.md) into executable machine code.

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
- No `volatile` qualifier for memory-mapped I/O (use `__builtin_store()`/`__builtin_load()` compiler builtins for inline MMIO)
- No floating point types (`float`, `double`)
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

The build script `Scripts/BCC/compile_modern_c.sh` handles the full pipeline. It accepts mixed `.c` and `.asm` source files, supports `-I` include paths, and passes through flags like `--libc`, `-h` (add header), `-i` (relocatable), and `-s` (syscall vector) to the assembler.

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

All memory-mapped I/O uses compiler builtins that emit inline `write`/`read` instructions: `__builtin_store(addr, value)`, `__builtin_storeb(addr, value)`, `__builtin_load(addr)`, `__builtin_loadb(addr)`. These bypass the lack of `volatile` support in cproc with zero function call overhead.

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

| Register | Alias | Purpose | Preserved? |
|----------|-------|---------|------------|
| r0 | zero | Always zero | N/A |
| r1 | v0 | Return value / temp | No |
| r2-r3 | - | Temp registers | No |
| r4 | a0 | Argument 0 | No |
| r5 | a1 | Argument 1 | No |
| r6 | a2 | Argument 2 | No |
| r7 | a3 | Argument 3 | No |
| r8-r11 | s0-s3 | Callee-saved | Yes |
| r12 | t4 | Temp register / scratch | No |
| r13 | sp | Stack pointer | Yes |
| r14 | fp | Frame pointer | Yes |
| r15 | ra | Return address | Yes |


## Self-Hosting on the FPGC

The toolchain can compile itself and run natively on the FPGC under BDOS. The on-device flow uses the same `cpp` → `cproc` → `qbe` → `asm-link` pipeline as the host build, with libc and userlib pre-compiled into a `/lib/asm-cache/` cache so that user-program compiles only have to compile the user source and link.

Use `make stage-cc-toolchain` on the host to lay out the cached `.asm` files and the `cc` / `libc-build` shell wrappers under `Files/BRFS-init/`, then push to the device with `make fnp-sync-files dev=N`. On-device:

```sh
libc-build              # one-time: build /lib/asm-cache/
cc /user/hello.c hello  # compile a single C file to /bin/hello
hello                   # run it
```

## Testing

The toolchain can be tested in combination with the Assembler (ASMPY) and Verilog simulation:

```bash
make test-c
make test-c-single file=01_return/return_constant.c
```

!!! note
    The test suite captures a lot of things, but is not exhaustive, meaning there are likely still bugs.
