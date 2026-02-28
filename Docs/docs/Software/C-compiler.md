# C Compiler (B32CC)

B32CC is a C compiler for the B32P3 architecture, derived from [SmallerC](https://github.com/alexfru/SmallerC) by Alexey Frunze. It compiles C source code into B32P3 assembly language, which can then be assembled with [ASMPY](Assembler.md) into executable machine code for the FPGC.

## Features

B32CC supports most of the C language common between C89 and C99. Its features can be described as follows:

- Single-pass compilation
- B32P3 assembly output
- Inline assembly support
- Optimized for FPGC's word-addressable architecture
- Self-hosting capable

## Limitations

- As the goal of this compiler setup is to keep complexity low, there is currently no support for a linker, even though the original SmallerC had one. All code will be compiled into a single assembly file
- No support for floating point types or operations
- No support for 64-bit integers (`long long`)
- Limited optimizations due to single-pass design, resulting assembly contains many (slow) read/write operations to/from memory due to stack usage. So performance critical functions should be implemented using inline assembly.

## Quick Start

### Command Line Usage

```bash
b32cc input.c output.asm

# Using make (normal workflow)
make compile-c-baremetal file=input
```

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

## Architecture & Memory Model

### Word-Addressable Memory

The B32P3 architecture uses **word-addressable memory**, where each address refers to a 32-bit word, not a byte. This has important implications:

- All data types occupy full 32-bit words in memory
- `char` is stored as a 32-bit word and therefore could contain the same value as an `int` (assuming I correctly removed truncation from B32CC). This means that using for example `short` makes no sense and can better be written as `int` to avoid confusion.
- `int` and pointers are native 32-bit words
- Pointer arithmetic is in terms of words, not bytes
- Arrays and structs are word-aligned

### Register Usage & Calling Convention

B32CC follows a specific calling convention for function calls:

#### Register Allocation

| Register | Alias | Purpose | Preserved? |
|----------|-------|---------|------------|
| r0 | zero | Always zero | N/A |
| r1 | v0 | Return value / temp | No |
| r2 | v1 | Return value / temp | No |
| r3 | - | Temp register | No |
| r4 | a0 | Argument 0 | No |
| r5 | a1 | Argument 1 | No |
| r6 | a2 | Argument 2 | No |
| r7 | a3 | Argument 3 | No |
| r8 | t0 | Temp register | No |
| r9 | t1 | Temp register | No |
| r10 | t2 | Temp register | No |
| r11 | t3 | Momentary register | No |
| r12 | t4 | Momentary register | No |
| r13 | sp | Stack pointer | Yes |
| r14 | fp | Frame pointer | Yes |
| r15 | ra | Return address | Yes |

#### Function Calling Sequence

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

## Supported and unsupported C Features

!!! note
    This list is mostly focused on uncommon feature that are supported, or common features that are not supported, as you can assume that most of the common features of C89/C99 are supported unless otherwise noted.

**Supported:**

- Fixed-point intrinsics for hardware acceleration: `__multfp(a, b)` and `__divfp(a, b)`

**Not Supported:**

- Floating point types: `float`, `double`, `long double`
- 64-bit integers: `long long`
- Bit fields in structs
- Variable-length arrays (VLAs)
- Complex numbers (`_Complex`)
- Function-like macros with complex logic (preprocessor limitations)
- Variable-length argument lists without proper declarations
- `#pragma` directives
- Complex macro expansion
- Stringification (`#`) and token pasting (`##`)
- `#error`, `#warning`

## Inline Assembly

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

## Entry Point Wrapper

B32CC generates a wrapper that:

1. Sets up the stack pointer
2. Sets up a return function (for `main()` to return to)
3. Jumps to `main()`
4. Handles the return value from `main()`
5. Halts execution

The wrapper also contains a jump to the `interrupt()` function for handling interrupts and is automatically included in the generated assembly.
The wrapper changes depending on the command line option used.

## Self-Hosting on the FPGC

B32CC can compile itself to run natively on the FPGC under BDOS. When compiled with itself (using the `__SMALLER_C__` define), several defaults change automatically:

- **`NO_ANNOTATIONS`**: Assembly output annotations are disabled at compile time to reduce output file size (and increase compilation speed).
- **`-user-bdos` mode**: Defaults to on when running on the FPGC, since the native compiler only targets userBDOS programs.
- **Command-line arguments**: Retrieved via `sys_shell_argc()` / `sys_shell_argv()` syscalls instead of traditional `main(argc, argv)`.

Surprisingly, the C compiler was able to run on the word-addressable, no-linker, no-MMU architecture of the FPGC with only minor adjustments.

The output can be further assembled using the `asm` user program and then executed directly all on the FPGC.

## Testing

B32CC can be tested in combination with the Assembler (ASMPY) and Verilog simulation using the provided test suite commands from the Makefile:

```bash
# Run all compiler tests (parallel, memory intensive)
make test-b32cc

# Run a single test
make test-b32cc-single file=04_control_flow/if_statements.c

# Debug a test with GTKWave and Verilog display output
make debug-b32cc file=04_control_flow/if_statements.c
```

!!! note
    The test suite captures a lot of things, but is not exhaustive, meaning there are likely still bugs in the compiler.
