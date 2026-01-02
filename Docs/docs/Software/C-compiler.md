# C Compiler (B32CC)

B32CC is a C compiler for the B32P3 architecture, derived from [SmallerC](https://github.com/alexfru/SmallerC) by Alexey Frunze. It compiles C source code into B32P3 assembly language, which can then be assembled with [ASMPY](Assembler.md) into executable machine code for the FPGC.

## Features

B32CC supports most of the C language common between C89 and C99. Its features can be described as follows:

- **Single-pass compilation** - Fast compilation without intermediate files
- **B32P3 assembly output** - Generates readable assembly for the target architecture
- **Inline assembly** - Direct assembly code embedding with `asm()` 
- **Optimized for word-addressable architecture** - All data is 32-bit word aligned since B32CC does not support byte-addressable memory
- **Self-hosting capable** - Can compile itself and run on the target architecture (using BDOS)

## Limitations

- As the goal of this compiler setup is to keep complexity low, there is currently no support for a linker, even though the original SmallerC had one. All code will be compiled into a single assembly file
- No support for floating point types or operations
- No support for 64-bit integers (`long long`)
- Limited optimizations due to single-pass design, resulting assembly contains many (slow) read/write operations to/from memory due to stack usage

## Quick Start

### Command Line Usage

```bash
# Compile C source to assembly
b32cc input.c output.asm

# Using make (example for bare-metal programs)
make compile-c-baremetal file=hello_world
```

### Example Program

The program should define a `main()` function as the entry point and an `interrupt()` function for handling interrupts:

```c
int main() {
    int sum = 0;
    for (int i = 1; i <= 10; i++) {
        sum += i;
    }
    return sum;  // Returns 55
}

void interrupt() {
    // Interrupt handler (required)
}
```

### Building the Compiler

```bash
# Build B32CC from source
make b32cc

# The compiler binary will be at:
# BuildTools/B32CC/output/b32cc
```

## Compilation Process

The typical workflow for compiling C programs involves multiple steps:

1. **C Compilation**: `b32cc` compiles `.c` files to `.asm` assembly
2. **Assembly**: ASMPY assembles `.asm` to `.list` (for simulation) from which a `.bin` binary is generated
3. **Deployment**: Binary is loaded to FPGC via UART (or a different method)

See the `Makefile` for detailed commands and options.

## Architecture & Memory Model

### Word-Addressable Memory

The B32P3 architecture uses **word-addressable memory**, where each address refers to a 32-bit word, not a byte. This has important implications:

- All data types occupy full 32-bit words in memory
- `char` is stored as a 32-bit word and therefore could contain the same value as an `int` (I think, as I did not test if there is truncation)
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
2. Additional arguments pushed to stack (right-to-left)
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
| Argument N       |  (if more than 4 arguments)
| ...              |
| Argument 5       |
+------------------+
| Return address   |  <- saved by prologue (offset +1 from fp)
+------------------+
| Saved FP         |  <- current fp points here (offset 0)
+------------------+
| Local var 1      |  (offset -1 from fp)
| Local var 2      |  (offset -2 from fp)
| ...              |
| Local var N      |
+------------------+  <- sp
Lower addresses
```

## Supported C Features

### Data Types & Declarations

**Supported:**

- Integer types: `char`, `unsigned char`, `short`, `unsigned short`, `int`, `unsigned int`
- Pointers and pointer arithmetic
- Arrays (single and multi-dimensional)
- Structs and unions
- Typedef declarations
- Global and local variables
- Static variables
- Enum declarations
- Type qualifiers: `const`, `volatile`, `signed`, `unsigned`

**Not Supported:**

- Floating point types: `float`, `double`, `long double`
- 64-bit integers: `long long`
- Bit fields in structs
- Variable-length arrays (VLAs)
- Complex numbers (`_Complex`)

### Operators

**Supported:**

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Logical: `&&`, `||`, `!`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
- Increment/decrement: `++`, `--` (both prefix and postfix)
- Pointer: `*`, `&`, `->`
- Ternary conditional: `? :`
- Comma operator: `,`
- Member access: `.`, `->`
- Array subscript: `[]`
- Function call: `()`
- Cast: `(type)`
- `sizeof`

### Control Flow

**Supported:**

- `if`, `else if`, `else`
- `while` loops
- `do-while` loops  
- `for` loops
- `switch` / `case` / `default`
- `break` and `continue`
- `goto` and labels
- `return`

### Functions

**Supported:**
- Function declarations and definitions
- Function calls with arguments
- Return values
- Recursion
- Forward declarations
- Function pointers
- Inline assembly via `asm()`

**Not Supported:**

- Function-like macros with complex logic (preprocessor limitations)
- Variable-length argument lists without proper declarations

### Preprocessor

**Supported:**

- `#include` (system and local headers)
- `#define` (simple macros and constants)
- `#ifdef`, `#ifndef`, `#else`, `#endif`
- `#undef`
- File inclusion paths

**Not Supported:**

- `#pragma` directives
- Complex macro expansion
- Stringification (`#`) and token pasting (`##`)
- `#error`, `#warning`

### Other Features

**Supported:**

- String literals
- Comments (`/* */` and `//`)

## Inline Assembly

B32CC supports inline assembly using the `asm()` syntax:

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
    There is no safety for the inline assembly, so you should almost always push and pop any registers you use.

**Notes for inline assembly:**

- Use `asm("instruction1" "instruction2" ...)` syntax, newlines are automatically added between string literals
- Use `;` for comments within assembly code
- Instructions are inserted directly into the assembly output, you have to take care of the generated function prologue/epilogue around it from the C function it is in

## Special Requirements

### Interrupt Handler

Every C program compiled with B32CC **must** define an `interrupt()` function:

```c
void interrupt() {
    // Handle interrupts here
    // Or leave empty if interrupts are not used
}
```

This function is called when hardware interrupts occur. It can be empty if your program doesn't use interrupts.

### Entry Point Wrapper

B32CC generates a wrapper that:

1. Sets up the stack pointer
2. Sets up a return function (for `main()` to return to)
3. Jumps to `main()`
4. Handles the return value from `main()`
5. Halts execution

The wrapper also contains a jump to the `interrupt()` function for handling interrupts and is automatically included in the generated assembly.

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

### Test Categories

The test suite is organized into categories:

- `01_return` - Return values
- `02_variables` - Variable declarations and assignments
- `03_functions` - Function calls and arguments
- `04_control_flow` - If/else, loops, switches
- `05_arithmetic` - Arithmetic operations
- `06_comparison` - Comparison operators
- `07_logical_bitwise` - Logical and bitwise operations
- `08_pointers` - Pointer operations
- `09_arrays` - Array indexing and operations
- `10_globals` - Global variables
- `11_structs` - Structure definitions and usage
- `12_recursion` - Recursive functions
- `13_unary` - Unary operators
- `14_compound_assignment` - Compound assignment operators
- `15_strings` - String literals and manipulation
- `16_casts` - Type casting
- `17_ternary` - Ternary conditional operator
- `18_literals` - Various literal types
- `19_forward_declaration` - Forward function declarations
- `20_precedence` - Operator precedence
- `21_edge_cases` - Edge cases and corner cases
- `22_asm` - Inline assembly
- `23_found_bugs` - Regression tests for fixed bugs
