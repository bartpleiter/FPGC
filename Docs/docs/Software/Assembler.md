# Assembler (ASMPY)

ASMPY is an assembler for the B32P3 ISA written in Python and allows the user to write and assemble code for the FPGC. While the main focus of the assembler is to assemble the output of the C compiler (B32CC), and therefore might not be great for large hand-written assembly projects, it can still be used for small assembly programs like bootloaders, or to create functional tests for the CPU/FPGC.

!!! note
    ASMPY is **not** the same as the assembler that runs on the FPGC itself, as Python cannot run on the FPGC. ASMPY's focus is to have a developer-friendly assembler on the development machine that can be easily extended, modified, debugged and improved. Eventually, when FPGC becomes a more self-contained system, a native assembler running on the FPGC will be created, just like in FPGC6.

## Features

ASMPY converts B32P3 assembly language source code into binary machine instructions. ASMPY features:

- **Proper Python modular code** with modern dev-setup and tests
- **Logging** with configurable verbosity levels  
- **Error handling** with source location information

## Quick Start

### Command Line Usage

```bash
asmpy input.asm output.bin

# Using make (normal workflow)
make assemble file=input
```

### Command Line Arguments

- `file` - Input assembly source file (required)
- `output` - Output binary file path (required)
- `-l, --log-level` - Set logging level: debug, info, warning, error, critical (default: info)
- `-d, --log-details` - Enable detailed logging with timestamps and line numbers
- `-h, --header` - Add header instructions (`jump Main`, `jump Int`, `.dw line_count`)
- `-o, --offset` - Set address offset for absolute label placement (ignored with `--independent`)
- `-i, --independent` - Enable position-independent code (PIC)

## Position-Independent Code (PIC)

When a program is loaded at an address that is not known at assembly time (e.g. user programs loaded by BDOS into a program slot), it must not contain absolute addresses. This is, because the FPGC does not support memory paging, dynamic linking or similar. The `-i` flag makes the assembler produce position-independent code by transforming three constructs:

**1. Jumps to labels** — `jump Label` becomes `jumpo offset`, which adds a signed offset to the program counter instead of loading an absolute address.

**2. Label address loading** — `addr2reg Label r1` (which normally loads an absolute address into a register) is rewritten as a `savpc` + `add`/`sub` sequence:

```asm
; Before (-i):
addr2reg Label r1      ; r1 = absolute address of Label

; After (-i):
savpc r1               ; r1 = current PC
add r1 <offset> r1     ; r1 = PC + offset to Label
```

This computes the label's address relative to the current program counter, so it works regardless of where the program is loaded in memory.

**3. Interrupt vector** — With `-h`, the header normally contains `jump Int` at offset 1. With `-i`, this becomes `nop` because user programs don't handle interrupts (BDOS handles them). This removes the need for an `Int:` label in the program.

## Assembly Language Syntax

### Basic Structure

Assembly files consist of several types of lines:

- **Instructions** - CPU operations and data definitions
- **Labels** - Address markers for jumps and references
- **Directives** - Section organization (`.code`, `.data`, etc.), only relevant for the C compiler (B32CC) output
- **Comments** - Using `;` delimiter
- **Preprocessor directives** - File inclusion and definitions using `#`

### Example Program

```asm
; Simple B32P3 assembly program
Main:
    load 0xB4 r1        ; Load constant into register 1
    load 37 r2          ; Load value 37 into register 2
    add r1 r2 r3        ; Add r1 + r2, store in r3
    halt                ; Halt

; Interrupt handler is required when using -h
Int:
    reti                ; Return from interrupt immediately
```

## Instruction Set Reference

### Instruction Table

The following table provides a complete overview of all B32P3 instructions supported by ASMPY:

| Instruction | Arg1 | Arg2 | Arg3 | Description |
|-------------|------|------|------|-------------|
| `halt` | - | - | - | Halt CPU by jumping to current address |
| `nop` | - | - | - | No operation (converted to OR r0 r0 r0) |
| `savpc` | R | - | - | Save program counter to Arg1 |
| `reti` | - | - | - | Return from interrupt |
| `readintid` | R | - | - | Store interrupt ID from CPU to Arg1 |
| `ccache` | - | - | - | Clear L1 instruction/data cache |
| `read` | C16 | R | R | Read from addr in Arg2 with 16-bit signed offset from Arg1, write to Arg3 |
| `write` | C16 | R | R | Write to addr in Arg2 with 16-bit signed offset from Arg1, data from Arg3 |
| `push` | R | - | - | Push Arg1 to stack |
| `pop` | R | - | - | Pop from stack to Arg1 |
| `add` | R | R/C16 | R | Compute Arg1 + Arg2, write result to Arg3 |
| `sub` | R | R/C16 | R | Compute Arg1 - Arg2, write result to Arg3 |
| `and` | R | R/C16 | R | Compute Arg1 AND Arg2, write result to Arg3 |
| `or` | R | R/C16 | R | Compute Arg1 OR Arg2, write result to Arg3 |
| `xor` | R | R/C16 | R | Compute Arg1 XOR Arg2, write result to Arg3 |
| `not` | R | R | - | Compute NOT (~) Arg1, write result to Arg2 |
| `shiftl` | R | R/C16 | R | Compute Arg1 << Arg2, write result to Arg3 |
| `shiftr` | R | R/C16 | R | Compute Arg1 >> Arg2 (logical), write result to Arg3 |
| `shiftrs` | R | R/C16 | R | Compute Arg1 >> Arg2 (arithmetic signed), write result to Arg3 |
| `slt` | R | R/C16 | R | If Arg1 < Arg2 (signed), write 1 to Arg3, else 0 |
| `sltu` | R | R/C16 | R | If Arg1 < Arg2 (unsigned), write 1 to Arg3, else 0 |
| `mults` | R | R/C16 | R | Compute Arg1 * Arg2 (signed), write result to Arg3 |
| `multu` | R | R/C16 | R | Compute Arg1 * Arg2 (unsigned), write result to Arg3 |
| `multfp` | R | R/C16 | R | Compute Arg1 * Arg2 (signed FixedPoint16.16), write result to Arg3 |
| `divs` | R | R/C16 | R | Compute Arg1 / Arg2 (signed), write result to Arg3 |
| `divu` | R | R/C16 | R | Compute Arg1 / Arg2 (unsigned), write result to Arg3 |
| `divfp` | R | R/C16 | R | Compute Arg1 / Arg2 (signed fixed-point), write result to Arg3 |
| `mods` | R | R/C16 | R | Compute Arg1 % Arg2 (signed), write result to Arg3 |
| `modu` | R | R/C16 | R | Compute Arg1 % Arg2 (unsigned), write result to Arg3 |
| `fmul` | FR | FR | FR | FP64 multiply: Arg3 = Arg1 × Arg2 (Q32.32) |
| `fadd` | FR | FR | FR | FP64 add: Arg3 = Arg1 + Arg2 (Q32.32) |
| `fsub` | FR | FR | FR | FP64 subtract: Arg3 = Arg1 - Arg2 (Q32.32) |
| `fld` | FR | R | R | Load FP64 register Arg1 from hi=Arg2, lo=Arg3 |
| `fsthi` | FR | R | - | Store high 32 bits of FP64 register Arg1 to Arg2 |
| `fstlo` | FR | R | - | Store low 32 bits of FP64 register Arg1 to Arg2 |
| `mulshi` | R | R/C16 | R | Signed multiply, return upper 32 bits to Arg3 |
| `multuhi` | R | R/C16 | R | Unsigned multiply, return upper 32 bits to Arg3 |
| `load` | C16 | R | - | Load unsigned 16-bit constant from Arg1 into lower bits of Arg2 |
| `loadhi` | C16 | R | - | Load unsigned 16-bit constant from Arg1 into upper bits of Arg2 |
| `load32` | C32 | R | - | Load signed 32-bit constant from Arg1 into Arg2 (expands to load+loadhi) |
| `addr2reg` | L | R | - | Load address from label Arg1 to Arg2 (expands to load+loadhi) |
| `beq` | R | R | C16/L | If Arg1 == Arg2, jump to 16-bit signed offset or label in Arg3 |
| `bne` | R | R | C16/L | If Arg1 != Arg2, jump to 16-bit signed offset or label in Arg3 |
| `bgt` | R | R | C16/L | If Arg1 > Arg2 (unsigned), jump to 16-bit signed offset or label in Arg3 |
| `bge` | R | R | C16/L | If Arg1 >= Arg2 (unsigned), jump to 16-bit signed offset or label in Arg3 |
| `blt` | R | R | C16/L | If Arg1 < Arg2 (unsigned), jump to 16-bit signed offset or label in Arg3 |
| `ble` | R | R | C16/L | If Arg1 <= Arg2 (unsigned), jump to 16-bit signed offset or label in Arg3 |
| `bgts` | R | R | C16/L | If Arg1 > Arg2 (signed), jump to 16-bit signed offset or label in Arg3 |
| `bges` | R | R | C16/L | If Arg1 >= Arg2 (signed), jump to 16-bit signed offset or label in Arg3 |
| `blts` | R | R | C16/L | If Arg1 < Arg2 (signed), jump to 16-bit signed offset or label in Arg3 |
| `bles` | R | R | C16/L | If Arg1 <= Arg2 (signed), jump to 16-bit signed offset or label in Arg3 |
| `jump` | C27/L | - | - | Jump to label or 27-bit constant address in Arg1 |
| `jumpo` | C27 | - | - | Jump to signed 27-bit constant offset in Arg1 |
| `jumpr` | C16 | R | - | Jump to Arg2 with 16-bit signed offset in Arg1 |
| `jumpro` | C16 | R | - | Jump to offset in Arg2 with 16-bit signed offset in Arg1 |

**Legend:**

- `R` = Register (r0-r15)
- `FR` = FP64 register (f0-f7)
- `C16` = 16-bit constant (signed unless noted)
- `C27` = 27-bit constant
- `C32` = 32-bit constant
- `L` = Label
- `-` = No argument

!!! note
    Branch instructions support both numeric offsets and labels. When a label is used, the assembler automatically calculates the relative offset (which is assumed to fit in 16 bits signed).

## Data Definitions

### Data Directives

Multiple data entries can be defined in a single directive by separating them with spaces.

!!! note
    Currently only `.dw` and `.dsw` are supported as these are the only ones produced by the C compiler (B32CC). The other directives could be useful for hand-written assembly, but in this revision of the project I am more focused on writing C code with inline assembly for performance-critical sections, which generally do not need these other directives.

| Directive | Description | Example |
|-----------|-------------|---------|
| `.dw` | 32-bit word data | `.dw 0x12345678 42 -1` |
| `.dbb` | Bytes merged into 32-bit words | `.dbb 0x12 0x34 0x56 0x78` |
| `.ddb` | 16-bit values merged into 32-bit words | `.ddb 0x1234 0x5678` |
| `.dsb` | String merged into 32-bit words | `.dsb "Hello World"` |
| `.dbw` | Bytes as separate 32-bit words | `.dbw 0x12 0x34` |
| `.ddw` | 16-bit values as separate 32-bit words | `.ddw 0x1234 0x5678` |
| `.dsw` | String as separate 32-bit words | `.dsw "Hi"` |

### Section Directives

!!! note
    These directives are produced by the C compiler (B32CC), and are reordered in the following order to make sure the executable code is not interleaved with data:

    1. `.code`
    2. `.data`
    3. `.rdata`
    4. `.bss`
    
    Note that data, rdata and bss sections are conceptually the same for the FPGC, and are there mostly because they were already produced by B32CC, as it is not a fully from scratch designed compiler for this project.

| Directive | Purpose |
|-----------|---------|
| `.code` | Executable code section |
| `.data` | Initialized data section |
| `.rdata` | Read-only data section |
| `.bss` | Uninitialized data section |

## Number Formats

ASMPY supports multiple number formats:

- **Decimal**: `42`, `-100`
- **Hexadecimal**: `0x1234`, `0xABCD`
- **Binary**: `0b10101010`, `0B11110000`

## Registers

The B32P3 has 16 general-purpose registers:

- `r0` - Always contains zero (cannot be modified)
- `r1` to `r15` - General-purpose registers

!!! note
    For the assembler, the general-purpose registers are identical. For the C compiler, there are further conventions. See the [B32CC documentation](OS.md) for details.


## Preprocessor Features

**Include Files**:

Include other assembly files using `#include "path"`. These files are basically pasted into the file. Useful for manually writing big assembly programs, but not used for the output of the C compiler (B32CC).

**Define Statements**:

Define text replacements using `#define`. They basically act as a search-and-replace before the actual assembly process.
