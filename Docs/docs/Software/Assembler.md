# Assembler (ASMPY)

ASMPY is an assembler for the B32P3 ISA written in Python and allows the user to write and assemble code for the FPGC. While the main focus of the assembler is to assemble the output of the C compiler toolchain (cproc + QBE), and therefore might not be great for large hand-written assembly projects, it can still be used for small assembly programs like bootloaders, or to create functional tests for the CPU/FPGC.

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
- `-i, --independent` - Generate relocatable code with a relocation table

## Relocatable Code

When a program is loaded at an address that is not known at assembly time (e.g. user programs loaded by BDOS into a program slot), the binary must be patched at load time. The FPGC does not support memory paging, dynamic linking, or an MMU. The `-i` flag makes the assembler produce a **relocatable binary** assembled at base address 0, with a relocation table appended after the program data.

### How It Works

The assembler resolves all labels to absolute addresses starting from 0. Three types of references contain absolute addresses that need fixing at load time:

| Reference Type | Example | Relocation |
|----------------|---------|------------|
| Data word (`.int label`) | Initialized pointer in `.data` | Add load-base to the full 32-bit word |
| `addr2reg` → `load`+`loadhi` pair | Loading a label address into a register | Reconstruct 32-bit address from both instruction fields, add load-base, re-encode |
| Header `jump Main` | Entry point jump in header | Add load-base to the 27-bit byte address field |

Jumps within the program body are converted to relative `jumpo` instructions and need no relocation. Branch instructions already use relative offsets.

### Binary Layout

```
Word 0:   jump Main           ; entry point (relocated by loader)
Word 1:   nop                 ; interrupt vector (nop for user programs)
Word 2:   .dw program_size    ; word count of code+data (including header)
Word P:   .dw reloc_count     ; number of relocation entries (P = program_size)
Word P+1: reloc_entry[0]      ; first relocation entry
...
Word P+N: reloc_entry[N-1]    ; last relocation entry
```

Each relocation entry is a 32-bit word: `[31:8] byte_offset, [7:0] type` (Type 0 = data word, Type 1 = load/loadhi pair, Type 2 = jump).

The BDOS loader reads `program_size` from header word 2. If the file is larger than `program_size`, it applies the relocation table by adding the slot base address to each referenced location.

### Interrupt Vector

With `-h`, the header normally contains `jump Int` at offset 1. With `-i`, this becomes `nop` because user programs don't handle interrupts (BDOS handles them). This removes the need for an `Int:` label in the program.

## Assembly Language Syntax

### Basic Structure

Assembly files consist of several types of lines:

- **Instructions** - CPU operations and data definitions
- **Labels** - Address markers for jumps and references
- **Directives** - Section organization (`.code`, `.data`, etc.), used by the C compiler output
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
| `read` | C16 | R | R | Read word from addr in Arg2 with 16-bit signed byte offset from Arg1, write to Arg3 |
| `readb` | C16 | R | R | Read byte (sign-extended) from addr in Arg2 with byte offset from Arg1, write to Arg3 |
| `readbu` | C16 | R | R | Read byte (zero-extended) from addr in Arg2 with byte offset from Arg1, write to Arg3 |
| `readh` | C16 | R | R | Read halfword (sign-extended) from addr in Arg2 with byte offset from Arg1, write to Arg3 |
| `readhu` | C16 | R | R | Read halfword (zero-extended) from addr in Arg2 with byte offset from Arg1, write to Arg3 |
| `write` | C16 | R | R | Write word to addr in Arg2 with 16-bit signed byte offset from Arg1, data from Arg3 |
| `writeb` | C16 | R | R | Write byte to addr in Arg2 with byte offset from Arg1, low 8 bits of Arg3 |
| `writeh` | C16 | R | R | Write halfword to addr in Arg2 with byte offset from Arg1, low 16 bits of Arg3 |
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
    The modern C compiler toolchain (cproc + QBE) uses ELF-style directives (`.eint`, `.ebyte`, `.eshort`, `.eascii`, `.efill`) which are packed into 32-bit words by the assembler. The legacy `.dw` and `.dsw` directives are also supported for hand-written assembly.

| Directive | Description | Example |
|-----------|-------------|---------|
| `.dw` | 32-bit word data | `.dw 0x12345678 42 -1` |
| `.dbb` | Bytes merged into 32-bit words | `.dbb 0x12 0x34 0x56 0x78` |
| `.ddb` | 16-bit values merged into 32-bit words | `.ddb 0x1234 0x5678` |
| `.dsb` | String packed into 32-bit words (4 chars per word, big-endian) | `.dsb \"Hello World\"` |
| `.dbw` | Bytes as separate 32-bit words | `.dbw 0x12 0x34` |
| `.ddw` | 16-bit values as separate 32-bit words | `.ddw 0x1234 0x5678` |
| `.dsw` | String as separate 32-bit words (1 char per word) | `.dsw \"Hi\"` |

### Section Directives

!!! note
    These directives are produced by the C compiler toolchain, and are reordered in the following order to make sure the executable code is not interleaved with data:

    1. `.code`
    2. `.data`
    3. `.rdata`
    4. `.bss`

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
    For the assembler, the general-purpose registers are identical. For the C compiler, there are further conventions. See the [C compiler documentation](C-compiler.md) for details.


## Preprocessor Features

**Include Files**:

Include other assembly files using `#include "path"`. These files are basically pasted into the file. Useful for manually writing big assembly programs.

**Define Statements**:

Define text replacements using `#define`. They basically act as a search-and-replace before the actual assembly process.
