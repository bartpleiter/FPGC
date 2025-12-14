# Custom C Library Implementation Plan

## Goal
Implement minimal C standard libraries for the FPGC project as described further in this document. These libraries will be used in the OS (kernel) development and for user programs. The ultimate goal of the project is to port a simplified version of Doom to run on the to be implemented OS, and these libraries should provide a step into that direction.

**Constraints:**
- No floating-point unit (FPU)
- No float type support in C compiler
- Using fixed point arithmetic for math functions
- No linker (static linking only)
- There is no Operating System implemented yet, so certain library function cannot be fully implmemented yet. These functions should be implemented as far as possible, with stubs or TODO comments for the rest, making sure they can be easily further implemented later when the OS is ready.

**Further instructions:**
- Some functions cannot be implemented yet as they depend on hardware or OS features that are not yet available. Do not spend too much time when you are stuck on a library, and define follow-up tasks instead. For example, the file I/O functions depend on a filesystem that is not implemented yet, so you can create stubs for these functions with TODO comments for later implementation.
- Create tests for each function where necessary to verify correctness in Tests/C/24_libc_tests. Make sure to not create too many small tests and only test the important functions. Tests can be run individually or entirely using commands from the Makefile. Make sure to read the Makefile if you want to compile, or test anything.
- You must read the Architecture-Development-Guide.md document to understand the project first before starting the implementation.

---

## Directory Structure

The libary files should live in the Software/C/libs directory. Within this folder there are three subfolders:
- common: libaries that are shared between the OS kernel and user programs
- kernel: libraries specific to the OS kernel
- user: libraries specific to user programs

Each of these three folders has a .h file with the same name as the folder as a library orchestrator as a workaround for the lack of a linker. Make sure to understand how this works, and think of a proper way to add the new libraries into this setup. It is really important that the libaries are structured properly as this project is unique in the way that is cannot use a linker!

---

## Function Checklist by Module

For each of these functions, think of a proper file structure and organization that fits within the existing library structure, and implement them accordingly.

### String/Memory Functions (`string.h`)
- [x] `memcpy` - Copy memory blocks
- [x] `memset` - Fill memory with a byte
- [x] `memmove` - Copy overlapping memory
- [x] `memcmp` - Compare memory blocks
- [x] `strlen` - String length
- [x] `strcpy` - Copy string
- [x] `strncpy` - Copy n characters
- [x] `strcmp` - Compare strings
- [x] `strncmp` - Compare n characters
- [x] `strcat` - Concatenate strings
- [x] `strncat` - Concatenate n characters (added)
- [x] `strchr` - Find character in string
- [x] `strrchr` - Find last occurrence of character
- [x] `strstr` - Find substring

### Memory Allocation (`stdlib.h`)
- [x] `malloc` - Allocate memory (simple free-list allocator)
- [x] `free` - Free memory
- [x] `calloc` - Allocate and zero memory
- [x] `realloc` - Resize allocation

### Standard Library (`stdlib.h`)
- [x] `atoi` - String to integer
- [x] `atol` - String to long
- [x] `abs` - Absolute value
- [x] `labs` - Absolute value of long (added)
- [x] `rand` - Random number generator (LCG)
- [x] `srand` - Seed random number generator
- [x] `qsort` - Quick sort (useful for BSP trees)
- [x] `bsearch` - Binary search (added)
- [x] `exit` - Exit program (halts CPU in bare-metal)
- [x] `int_min` / `int_max` / `int_clamp` - Utility functions (functions instead of macros due to B32CC limitations)

### Standard I/O (`stdio.h`)
- [x] `printf` - Formatted output (supports %d, %i, %u, %x, %X, %o, %c, %s, %p, %%)
- [x] `sprintf` - Format to string
- [x] `snprintf` - Format to string with size limit
- [x] `fprintf` - Format to stream
- [x] `putchar` - Output character (via UART)
- [x] `puts` - Output string
- [ ] `getchar` - Input character (stub - needs UART RX implementation)

### File I/O (`stdio.h` - stubs for future filesystem)
- [x] `fopen` - Open file (stub)
- [x] `fclose` - Close file (stub)
- [x] `fread` - Read from file (stub)
- [x] `fwrite` - Write to file (works for stdout/stderr)
- [x] `fseek` - Seek in file (stub)
- [x] `ftell` - Get file position (stub)
- [x] `feof` - Test end of file
- [x] `ferror` - Test error indicator (added)
- [x] `clearerr` - Clear indicators (added)
- [x] `fputc` / `fputs` / `fgetc` / `fgets` - Character I/O (added)
- [x] `fflush` - Flush buffer (no-op, no buffering)

### Character Classification (`ctype.h`)
- [x] `isdigit` - Check if digit
- [x] `isalpha` - Check if alphabetic
- [x] `isalnum` - Check if alphanumeric
- [x] `isspace` - Check if whitespace
- [x] `isupper` - Check if uppercase (added)
- [x] `islower` - Check if lowercase (added)
- [x] `isxdigit` - Check if hex digit (added)
- [x] `isprint` - Check if printable (added)
- [x] `iscntrl` - Check if control character (added)
- [x] `ispunct` - Check if punctuation (added)
- [x] `isgraph` - Check if graphical (added)
- [x] `toupper` - Convert to uppercase
- [x] `tolower` - Convert to lowercase

### Fixed-Point Math (`fixedmath.h`)
- [x] Define `fixed_t` type (int)
- [x] Define `FRACBITS` (16) and `FRACUNIT` (65536)
- [x] `int2fixed` - Convert integer to fixed-point
- [x] `fixed2int` - Convert fixed-point to integer
- [x] `fixed_frac` - Get fractional part (added)
- [x] `fixed_mul` - Multiply two fixed-point numbers (handles overflow)
- [x] `fixed_div` - Divide two fixed-point numbers
- [x] `fixed_sqrt` - Square root (Newton-Raphson)
- [x] `fixed_sin` - Sine (91-entry lookup table)
- [x] `fixed_cos` - Cosine (lookup table)
- [x] `fixed_tan` - Tangent (computed from sin/cos)
- [x] `fixed_atan2` - Arc tangent for angles (lookup table)

### Fixed-Point Utility Functions (`fixedmath.h`)
- [x] `fixed_abs` - Absolute value
- [x] `fixed_sign` - Sign of value (added)
- [x] `fixed_min` - Minimum of two values
- [x] `fixed_max` - Maximum of two values
- [x] `fixed_clamp` - Clamp value between min and max
- [x] `fixed_lerp` - Linear interpolation (added)
- [x] `fixed_dist_approx` - Fast distance approximation (added)
- [x] `fixed_dot2d` - 2D dot product (added)

### Standard Definitions (`stddef.h`)
- [x] `NULL` - Null pointer constant
- [x] `size_t` - Size type
- [x] `ptrdiff_t` - Pointer difference type

---

## Implementation Notes

### B32CC Compiler Limitations
During implementation, several B32CC compiler limitations were discovered:

1. **No complex macros**: The compiler doesn't support ternary operators or complex expressions in `#define` macros. All utility macros (like `min`, `max`, `clamp`) were implemented as functions instead.

2. **No struct return values**: Functions cannot return structs by value. The standard `div_t` and `ldiv_t` types and their associated functions (`div`, `ldiv`) were omitted for this reason.

3. **Limited static initializers**: Complex static variable initialization is not supported. The `FILE` structures in `stdio.c` are initialized at runtime via `_init_stdio()`.

4. **Variadic function limitations**: While `printf`/`sprintf` are implemented, variadic argument handling (`va_arg`) may not work correctly in all cases. Tests use simple format strings to avoid this issue.

### Memory Allocation
The heap allocator is a simple free-list implementation:
- Heap starts at address `0x00100000` (after program code)
- Each block has an 8-byte header (size + next pointer)
- Blocks are 4-byte aligned (word-aligned for the architecture)
- First-fit allocation strategy
- Adjacent free blocks are coalesced on free

### Fixed-Point Math
- Uses 16.16 format (16 integer bits, 16 fractional bits)
- Sine lookup table covers 0-90 degrees (91 entries), with symmetry for other quadrants
- Arctangent lookup table with 256 entries for angle calculation
- Square root uses Newton-Raphson iteration

### File I/O
File functions are mostly stubs awaiting filesystem implementation:
- `stdin`, `stdout`, `stderr` are defined as global FILE pointers
- `stdout` and `stderr` output via UART (address `0x7000000`)
- `fopen`, `fclose`, `fread`, `fseek`, `ftell` return stub values
- Real filesystem support to be added when available

### Testing
Tests are located in `Tests/C/24_libc_tests/`:
- `string_basic.c` - Tests string/memory functions
- `stdlib_basic.c` - Tests malloc, atoi, rand, qsort, etc.
- `ctype_basic.c` - Tests character classification
- `fixedmath_basic.c` - Tests fixed-point math operations
- `sprintf_basic.c` - Tests sprintf with simple format strings

Run tests with: `make test-b32cc`

---

## Future Work

- [ ] Implement `getchar` when UART RX is available
- [ ] Implement real file I/O when filesystem is ready
- [ ] Add `errno` support for error handling
- [ ] Consider adding `setjmp`/`longjmp` for non-local jumps
- [ ] Add more comprehensive `printf` format support if variadic issues are resolved
