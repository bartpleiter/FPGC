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
# Basic usage
asmpy input.asm output.bin

# With debug logging
asmpy input.asm output.bin --log-level debug

# With detailed logging (includes timestamps and line numbers)
asmpy input.asm output.bin --log-level debug --log-details
```

### Command Line Arguments

- `file` - Input assembly source file (required)
- `output` - Output binary file path (required)
- `-l, --log-level` - Set logging level: debug, info, warning, error, critical (default: info)
- `-d, --log-details` - Enable detailed logging with timestamps and line numbers
- `-h, --header` - Add bare-metal header (`jump Main`, `jump Int`, `.dw line_count`)
- `-o, --offset` - Set address offset for absolute label placement (ignored with `--independent`)
- `-i, --independent` - Enable position-independent output (PIC): converts label `jump` to relative `jumpo`, rewrites `addr2reg` to `savpc` + `add/sub` chain, and with `--header` replaces `jump Int` by `nop`

## Position-Independent Code (PIC)

When a program is loaded at an address that is not known at assembly time (e.g. user programs loaded by BDOS into a program slot), it must not contain absolute addresses. The `-i` flag makes the assembler produce position-independent output by transforming three constructs:

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

**3. Interrupt vector** — With `-h`, the header normally contains `jump Int` at offset 1. With `-i`, this becomes `nop` because user programs don't handle interrupts (BDOS handles them).

### When to Use PIC

Use `-h -i` together when assembling user programs for BDOS:

```bash
asmpy program.asm program.list -h -i
```

Bare-metal programs and the BDOS kernel itself do **not** use `-i` because they run at a fixed address.

## Assembly Language Syntax

### Basic Structure

Assembly files consist of several types of lines:

- **Instructions** - CPU operations and data definitions
- **Labels** - Address markers for jumps and references
- **Directives** - Section organization (`.code`, `.data`, etc.), only relevant for the C compiler (B32CC) output
- **Comments** - Documentation using `;` delimiter
- **Preprocessor directives** - File inclusion and definitions using `#`

### Example Program

```asm
; Simple B32P3 assembly program
Main:
    load 0xDE r1        ; Load constant into register 1
    load 42 r2          ; Load value 42 into register 2
    add r1 r2 r3        ; Add r1 + r2, store in r3
    halt                ; Halt

; Interrupt handler is required
Int:
    reti                ; Return from interrupt immediately
```

## Instruction Set Reference

### Quick Reference Table

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
- `C16` = 16-bit constant (signed unless noted)
- `C27` = 27-bit constant
- `C32` = 32-bit constant
- `L` = Label
- `-` = No argument

### Control Operations

| Instruction | Arguments | Description | Example |
|-------------|-----------|-------------|---------|
| `halt` | none | Halt CPU execution | `halt` |
| `nop` | none | No operation, does nothing | `nop` |
| `savpc` | reg | Save current program counter to register | `savpc r1` |
| `reti` | none | Return from interrupt | `reti` |
| `readintid` | reg | Read interrupt ID to register | `readintid r2` |
| `ccache` | none | Clear l1i/l1d cache | `ccache` |

### Memory Operations

| Instruction | Arguments | Description | Example |
|-------------|-----------|-------------|---------|
| `read` | const16, reg, reg | Read from memory address (base + offset). Args: signed 16-bit offset, base register, destination register | `read 4 r13 r1` |
| `write` | const16, reg, reg | Write to memory address (base + offset). Args: signed 16-bit offset, base register, source register | `write -8 r13 r2` |
| `push` | reg | Push register value to stack | `push r1` |
| `pop` | reg | Pop value from stack to register | `pop r2` |

### Arithmetic Operations

#### Single-Cycle Operations

| Instruction | Arguments | Description | Example |
|-------------|-----------|-------------|---------|
| `add` | reg, reg/const16, reg | Add two values. Args: source register, source register or signed 16-bit constant, destination register | `add r1 r2 r3` or `add r1 42 r3` |
| `sub` | reg, reg/const16, reg | Subtract reg2/const from reg1. Args: source register, source register or signed 16-bit constant, destination register | `sub r1 r2 r3` or `sub r1 10 r3` |
| `and` | reg, reg/const16, reg | Bitwise AND. Args: source register, source register or signed 16-bit constant, destination register | `and r1 r2 r3` or `and r1 0xFF r3` |
| `or` | reg, reg/const16, reg | Bitwise OR. Args: source register, source register or signed 16-bit constant, destination register | `or r1 r2 r3` or `or r1 0x100 r3` |
| `xor` | reg, reg/const16, reg | Bitwise XOR. Args: source register, source register or signed 16-bit constant, destination register | `xor r1 r2 r3` or `xor r1 0xFFFF r3` |
| `not` | reg, reg | Bitwise NOT. Args: source register, destination register | `not r1 r2` |
| `shiftl` | reg, reg/const16, reg | Logical left shift. Args: source register, shift amount (register or unsigned 5-bit constant), destination register | `shiftl r1 r2 r3` or `shiftl r1 4 r3` |
| `shiftr` | reg, reg/const16, reg | Logical right shift. Args: source register, shift amount (register or unsigned 5-bit constant), destination register | `shiftr r1 r2 r3` or `shiftr r1 8 r3` |
| `shiftrs` | reg, reg/const16, reg | Arithmetic right shift. Args: source register, shift amount (register or unsigned 5-bit constant), destination register | `shiftrs r1 r2 r3` or `shiftrs r1 2 r3` |
| `slt` | reg, reg/const16, reg | Set if less than (signed). Args: source register, source register or signed 16-bit constant, destination register | `slt r1 r2 r3` or `slt r1 100 r3` |
| `sltu` | reg, reg/const16, reg | Set if less than (unsigned). Args: source register, source register or signed 16-bit constant, destination register | `sltu r1 r2 r3` or `sltu r1 50 r3` |

#### Multi-Cycle Operations

| Instruction | Arguments | Description | Example |
|-------------|-----------|-------------|---------|
| `mults` | reg, reg/const16, reg | Multiply (signed). Args: source register, source register or signed 16-bit constant, destination register | `mults r1 r2 r3` or `mults r1 -5 r3` |
| `multu` | reg, reg/const16, reg | Multiply (unsigned). Args: source register, source register or signed 16-bit constant, destination register | `multu r1 r2 r3` or `multu r1 10 r3` |
| `multfp` | reg, reg/const16, reg | Multiply fixed-point. Args: source register, source register or signed 16-bit constant, destination register | `multfp r1 r2 r3` or `multfp r1 0x8000 r3` |
| `divs` | reg, reg/const16, reg | Divide (signed). Args: dividend register, divisor register or signed 16-bit constant, destination register | `divs r1 r2 r3` or `divs r1 4 r3` |
| `divu` | reg, reg/const16, reg | Divide (unsigned). Args: dividend register, divisor register or signed 16-bit constant, destination register | `divu r1 r2 r3` or `divu r1 8 r3` |
| `divfp` | reg, reg/const16, reg | Divide fixed-point. Args: dividend register, divisor register or signed 16-bit constant, destination register | `divfp r1 r2 r3` or `divfp r1 0x4000 r3` |
| `mods` | reg, reg/const16, reg | Modulo (signed). Args: dividend register, divisor register or signed 16-bit constant, destination register | `mods r1 r2 r3` or `mods r1 7 r3` |
| `modu` | reg, reg/const16, reg | Modulo (unsigned). Args: dividend register, divisor register or signed 16-bit constant, destination register | `modu r1 r2 r3` or `modu r1 16 r3` |

### Load Operations

| Instruction | Arguments | Description | Example |
|-------------|-----------|-------------|---------|
| `load` | const16, reg | Load 16-bit constant to lower bits of register. Args: unsigned 16-bit constant, destination register | `load 0x1234 r1` |
| `loadhi` | const16, reg | Load 16-bit constant to upper bits of register. Args: unsigned 16-bit constant, destination register | `loadhi 0x5678 r1` |
| `load32` | const32, reg | Load 32-bit constant (expands to load + loadhi). Args: signed 32-bit constant, destination register | `load32 0x12345678 r1` |
| `addr2reg` | label, reg | Load label address to register (expands to load + loadhi). Args: label name, destination register | `addr2reg MyLabel r2` |

### Branch Operations

Branch instructions support both numeric offsets and labels. When a label is used, the assembler automatically calculates the relative offset (which is assumed to fit in 16 bits signed).

| Instruction | Arguments | Description | Example |
|-------------|-----------|-------------|---------|
| `beq` | reg, reg, const16/label | Branch if equal. Args: register 1, register 2, signed 16-bit offset or label | `beq r1 r2 -4` or `beq r1 r2 MyLabel` |
| `bne` | reg, reg, const16/label | Branch if not equal. Args: register 1, register 2, signed 16-bit offset or label | `bne r1 r0 8` or `bne r1 r0 Loop` |
| `bgt` | reg, reg, const16/label | Branch if greater (unsigned). Args: register 1, register 2, signed 16-bit offset or label | `bgt r1 r2 12` |
| `bge` | reg, reg, const16/label | Branch if greater or equal (unsigned). Args: register 1, register 2, signed 16-bit offset or label | `bge r1 r2 -8` |
| `blt` | reg, reg, const16/label | Branch if less (unsigned). Args: register 1, register 2, signed 16-bit offset or label | `blt r1 r2 16` |
| `ble` | reg, reg, const16/label | Branch if less or equal (unsigned). Args: register 1, register 2, signed 16-bit offset or label | `ble r1 r2 -12` |
| `bgts` | reg, reg, const16/label | Branch if greater (signed). Args: register 1, register 2, signed 16-bit offset or label | `bgts r1 r2 4` |
| `bges` | reg, reg, const16/label | Branch if greater or equal (signed). Args: register 1, register 2, signed 16-bit offset or label | `bges r1 r0 -16` |
| `blts` | reg, reg, const16/label | Branch if less (signed). Args: register 1, register 2, signed 16-bit offset or label | `blts r1 r0 20` |
| `bles` | reg, reg, const16/label | Branch if less or equal (signed). Args: register 1, register 2, signed 16-bit offset or label | `bles r1 r2 -4` |

### Jump Operations

| Instruction | Arguments | Description | Example |
|-------------|-----------|-------------|---------|
| `jump` | const27/label | Jump to absolute address. Args: 27-bit address constant or label name | `jump Main` or `jump 0x1000` |
| `jumpo` | const27 | Jump with relative offset. Args: signed 27-bit offset | `jumpo 16` or `jumpo -8` |
| `jumpr` | const16, reg | Jump absolute to register + offset. Args: signed 16-bit offset, register | `jumpr 4 r14` |
| `jumpro` | const16, reg | Jump relative to register + offset. Args: signed 16-bit offset, register | `jumpro -12 r15` |

## Data Definitions

### Data Directives

!!! note
    Currently only `.dw` and `.dsw` are supported.

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

## Labels and Addressing

Labels mark specific locations in code and can be referenced by jump or addr2reg instructions:

```asm
Main:                   ; Label definition
    load 42 r1
    addr2reg Data r2    ; Load address of Data into r2
    jump Loop           ; Label reference

Loop:
    add r1 1 r1
    jump Loop           ; Jump back to Loop

Data:
    .dw 123 456
```

## Preprocessor Features

### Include Files

Include other assembly files using `#include`:

```asm
#include "library.asm"
#include "constants.asm"
```

### Define Statements

Define text replacements using `#define`:

```asm
#define STACK_SIZE 1024
#define MAX_ITERATIONS 100

load STACK_SIZE r13     ; Replaced with: load 1024 r13
```

## Advanced Features

### Instruction Expansion

Some instructions automatically expand into multiple machine instructions:

- `load32 0x12345678 r1` → `load 0x5678 r1` + `loadhi 0x1234 r1`
- `addr2reg Label r2` → `load Label[15:0] r2` + `loadhi Label[31:16] r2`

### Section Reordering

ASMPY automatically reorders sections in the output as this is required for executing B32CC output.

1. `.code` - Executable code
2. `.data` - Initialized data  
3. `.rdata` - Read-only data
4. `.bss` - Uninitialized data

## Error Handling

ASMPY provides detailed error messages with source file locations:

```text
ERROR: Invalid register: r16
  File: program.asm, Line: 25
  Code: add r15 r16 r1
```
