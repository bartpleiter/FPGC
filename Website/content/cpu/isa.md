---
title: Instruction set
weight: 1
---

This page defines the instruction set of B32P4, the CPU that runs in the FPGC. The instruction encoding is almost identical to B32P3, as B32P4 focuses mostly on performance and efficiency improvements instead of functional changes.

## Overview

- **32-bit RISC**, with 16 registers of 32 bits. `r0` always reads as 0.
- **Fixed-length 32-bit instructions**, with the opcode always in the leftmost 4 bits.
- **Byte-addressed, little-endian memory.** Reads and writes can be 8, 16 or 32 bits wide.
- **A hardware stack** of 256 words for `push` and `pop`, separate from memory. Primarily for quick register backups.
- **Hardware interrupts**, taken only at jumps and branches to simplify the CPU pipeline.
- **No security features.** Since the goal is to be a personal hobby CPU where I am the only user, adding security features only adds unnecessary complexity.
- **No floating point.** To keep complexity down, decimal arithmetic is done using Q16.16 fixed-point format, just like the Sony PS1 did.
- **Error register.** (new feature in B32P4) Illegal instructions and accesses set a flag in the [error register](#error-register).

### Notation

| Notation | Meaning |
| --- | --- |
| `A`, `B` | the source registers |
| `D` | the destination register |
| `PC` | the address of the instruction itself (program counter) |
| `c16`, `c27` | the 16-bit and 27-bit constant fields |
| `sext(x)`, `zext(x)` | `x` sign-extended or zero-extended to 32 bits |
| `x[h:l]` | bits `h` down to `l` of `x` |

All arithmetic wraps around at 32 bits unless stated otherwise.

## Registers

| Register | Use |
| --- | --- |
| `r0` | Always 0. Writes to it are ignored. |
| `r1`–`r15` | General purpose. Technically no difference between these registers. |

## Instruction encoding

Every instruction is one 32-bit word. Bits marked **x** are unused in hardware.

```text
         |31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
----------------------------------------------------------------------------------------------------------
 HALT      1  1  1  1| 1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
 READ      1  1  1  0||----------------16 BIT CONSTANT---------------||--A REG---||-RD SUBOP-||--D REG---|
 WRITE     1  1  0  1||----------------16 BIT CONSTANT---------------||--A REG---||--B REG---||-WR SUBOP|
 INTID     1  1  0  0| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x |--D REG---|
 PUSH      1  0  1  1| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x |--B REG---| x  x  x  x
 POP       1  0  1  0| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x |--D REG---|
 JUMP      1  0  0  1||--------------------------------27 BIT CONSTANT--------------------------------||O|
 JUMPR     1  0  0  0||----------------16 BIT CONSTANT---------------| x  x  x  x |--B REG---| x  x  x |O|
 CCACHE    0  1  1  1| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x
 CCACHED   0  1  1  1| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  1
 BRANCH    0  1  1  0||----------------16 BIT CONSTANT---------------||--A REG---||--B REG---||-OPCODE||S|
 SAVPC     0  1  0  1| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x |--D REG---|
 RETI      0  1  0  0| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x
 ARITHMC   0  0  1  1||--OPCODE--||----------------16 BIT CONSTANT---------------||--A REG---||--D REG---|
 ARITHM    0  0  1  0||--OPCODE--| x  x  x  x  x  x  x  x  x  x  x  x |--A REG---||--B REG---||--D REG---|
 ARITHC    0  0  0  1||--OPCODE--||----------------16 BIT CONSTANT---------------||--A REG---||--D REG---|
 ARITH     0  0  0  0||--OPCODE--| x  x  x  x  x  x  x  x  x  x  x  x |--A REG---||--B REG---||--D REG---|
```

### ALU instructions

ARITH and ARITHC use the single-cycle ALU. The operation is `D = A op B`, where the second operand B is:

- **ARITH:** register B
- **ARITHC:** `sext(c16)`, except `load` and `loadhi`, which use `zext(c16)`

| op | Mnemonic | Result |
| --- | --- | --- |
| `0000` | `or` | `A \| B` |
| `0001` | `and` | `A & B` |
| `0010` | `xor` | `A ^ B` |
| `0011` | `add` | `A + B` |
| `0100` | `sub` | `A - B` |
| `0101` | `shiftl` | `A << B[4:0]` |
| `0110` | `shiftr` | `A >> B[4:0]`, logical (shifts in zeros) |
| `0111` | `not` | `~A` (B isn't used) |
| `1000` | — | reserved: illegal |
| `1001` | — | reserved: illegal |
| `1010` | `slt` | `1` if `A < B` as signed numbers, else `0` |
| `1011` | `sltu` | `1` if `A < B` as unsigned numbers, else `0` |
| `1100` | `load` | `B` |
| `1101` | `loadhi` | `{B[15:0], A[15:0]}` |
| `1110` | `shiftrs` | `A >> B[4:0]`, arithmetic (shifts in the sign bit) |
| `1111` | — | reserved: illegal |


- Shifts use only the low 5 bits of B, so a shift by 32 is a shift by 0.
- Constants for `sltu` are sign-extended too: `sltu r1 -1 r2` compares with `0xFFFFFFFF`.
- Loading a 32-bit constant takes two instructions:
  1. `load c16 D` sets `D = zext(c16)`.
  2. `loadhi c16 D`, assembled with A = D, sets the upper half and keeps the lower half.

  The assembler's `load32` and `addr2reg` expand into these two instructions.

### Multiply and divide instructions

ARITHM and ARITHMC have the same form as ARITH and ARITHC: B is register B, or `sext(c16)`. These take multiple cycles to complete (stalls the CPU).

| op | Mnemonic | Result |
| --- | --- | --- |
| `0000` | `mults` | low 32 bits of `A × B` (signed) |
| `0001` | `multu` | low 32 bits of `A × B` (unsigned) |
| `0010` | `multfp` | bits [47:16] of the signed 64-bit product: a Q16.16 fixed-point multiply |
| `0011` | `divs` | `A / B`, signed, rounded toward zero |
| `0100` | `divu` | `A / B`, unsigned |
| `0101` | `divfp` | `(A × 2^16) / B`, signed, computed in 64 bits and rounded toward zero; the result is the low 32 bits. This is a Q16.16 fixed-point divide. |
| `0110` | `mods` | the remainder of `divs` |
| `0111` | `modu` | the remainder of `divu` |
| `1000`–`1101` | — | reserved: illegal |
| `1110` | `mulshi` | bits [63:32] of `A × B` (signed) |
| `1111` | `multuhi` | bits [63:32] of `A × B` (unsigned) |

Defined edge cases:

| Case | Result |
| --- | --- |
| Division by zero | `divs`, `divu`, `divfp`: `0xFFFFFFFF`. `mods`, `modu`: A. |
| `divs` of `0x80000000` by −1 | `0x80000000`, and `mods` gives 0 |
| `divfp` result doesn't fit in 32 bits | the low 32 bits of the exact 64-bit quotient |

### Memory instructions

READ and WRITE access memory at the byte address `A + sext(c16)`.

| Instruction | Size field | Mnemonic | Effect |
| --- | --- | --- | --- |
| READ | `0000` | `read c16 A D` | `D` = the 32-bit word |
| READ | `0001` | `readb c16 A D` | `D` = the byte, sign-extended |
| READ | `0101` | `readbu c16 A D` | `D` = the byte, zero-extended |
| READ | `0010` | `readh c16 A D` | `D` = the 16-bit halfword, sign-extended |
| READ | `0110` | `readhu c16 A D` | `D` = the halfword, zero-extended |
| WRITE | `0000` | `write c16 A B` | writes the word B |
| WRITE | `0001` | `writeb c16 A B` | writes `B[7:0]` |
| WRITE | `0010` | `writeh c16 A B` | writes `B[15:0]` |

- Little-endian: the byte at address `n` is bits [7:0] of the word at `n & ~3`.
- Word accesses must be at a multiple of 4, halfword accesses at a multiple of 2.

### Hardware stack

The stack is 256 words of on-chip memory, separate from the address space. It's meant for saving registers in functions and interrupt handlers.

| Instruction | Mnemonic | Effect |
| --- | --- | --- |
| PUSH | `push B` | `stack[SP] = B`, then `SP = SP + 1` |
| POP | `pop D` | `SP = SP - 1`, then `D = stack[SP]` |

- The SP can be read and written through a CPU-internal I/O register, which gives software (such as a context switch) access to SP.
- The SP wraps around on overflow and underflow.


### Control flow instructions

| Instruction | Mnemonic | New PC |
| --- | --- | --- |
| JUMP, O = 0 | `jump c27` or `jump label` | `zext(c27)` |
| JUMP, O = 1 | `jumpo c27` | `PC + sext(c27)` |
| JUMPR, O = 0 | `jumpr c16 B` | `B + sext(c16)` |
| JUMPR, O = 1 | `jumpro c16 B` | `PC + B + sext(c16)` |
| BRANCH | `beq A B c16` or `beq A B label`, ... | `PC + sext(c16)` if the condition holds, else `PC + 4` |
| HALT | `halt` | `PC`: it jumps to itself |

- All targets are byte addresses and must be multiples of 4.
- The range of absolute `jump` is 0 to 128 MiB which covers SDRAM. Higher jumps should use `jumpo` or `jumpr`.
- A branch reaches ±32 KiB from itself, as the argument is an offset from the current PC. Can be combined with a jump instruction for larger jumps.
- HALT keeps the CPU on one instruction until an interrupt is taken. Implemented by jumping to the current address.

### Branch conditions

| cond [3:1] | S = 0 (unsigned) | S = 1 (signed) |
| --- | --- | --- |
| `000` | `beq`: A = B | `beq` (S is ignored) |
| `001` | `bgt`: A > B | `bgts` |
| `010` | `bge`: A ≥ B | `bges` |
| `011` | reserved: illegal | reserved: illegal |
| `100` | `bne`: A ≠ B | `bne` (S is ignored) |
| `101` | `blt`: A < B | `blts` |
| `110` | `ble`: A ≤ B | `bles` |
| `111` | reserved: illegal | reserved: illegal |

### SAVPC

`savpc D` sets `D = PC`, the address of the `savpc` itself. Together with an add, this can be used to create a return address for function jumps.

## Interrupts

The CPU has several interrupt inputs, with IDs 1 starting from 1 upwards. ID 1 has the highest priority.

Pending interrupts:

- A rising edge on an input makes that interrupt pending.
- A pending interrupt stays pending until it's taken, one at a time, in priority order.

When an interrupt is taken, all of these must hold:

- interrupts are enabled
- an interrupt is pending
- the CPU is about to execute a jump, `jumpr`, taken branch or `halt`
- that instruction is in SDRAM, so ROM code is never interrupted

This simplifies the implementation: the interrupt is taken instead of that control-flow instruction.

1. `pc_backup` is set to the address of the control-flow instruction.
2. Interrupts are disabled.
3. Execution continues at address `0x4`, the interrupt vector.

The handler reads the ID with INTID, handles the interrupt and ends with RETI.

| Instruction | Mnemonic | Effect |
| --- | --- | --- |
| RETI | `reti` | `PC = pc_backup`, and interrupts are enabled |
| INTID | `readintid D` | `D` = the ID of the last interrupt taken, or 0 if there has been none since reset |

After RETI, the interrupted control-flow instruction executes as if nothing happened.

Other rules:

- No nesting: an interrupt that arrives during the handler stays pending until after RETI.
- Interrupts are enabled after reset. There are no instructions to enable or disable them.
- Reading `pc_backup`: the handler can read and write it via a CPU-internal register, for example to switch tasks.

## Cache instructions

SDRAM is accessed through an instruction cache (L1I) and a data cache (L1D). There are two instructions to invalidate the caches, which is needed for loading code into SDRAM, or for DMA operations.

| Instruction | Mnemonic | Effect |
| --- | --- | --- |
| CCACHE, data-only = 0 | `ccache` | write all changed L1D data back to SDRAM, then invalidate L1I and L1D |
| CCACHE, data-only = 1 | `ccached` | write all changed L1D data back to SDRAM, then invalidate L1D only |

## Memory map

The CPU's view of the 32-bit address space. Other addresses ignore writes and return 0 for reads.

| Address range | Region | Notes |
| --- | --- | --- |
| `0x00000000`–`0x03FFFFFF` | SDRAM, 64 MiB | code and data, through the L1 caches |
| `0x1C000000`–`0x1DFFFFFF` | I/O | handled by the Memory Unit, the devices and their addresses are in the I/O documentation |
| `0x1E000000`–`0x1E000FFF` | ROM, 4 KiB | code and read-only data. The CPU starts here. Writes are illegal. |
| `0x1E400000`–`0x1EFFFFFF` | video memory | the GPU's memories. The exact layout is defined with the GPU. |
| `0x1F000000`–`0x1F00000F` | CPU-internal I/O | the registers below |

Only SDRAM and ROM can hold executable code.

### CPU-internal I/O registers

| Address | Name | Access | Contents |
| --- | --- | --- | --- |
| `0x1F000000` | `PC_BACKUP` | read/write | the return address of the current interrupt |
| `0x1F000004` | `SP` | read/write | the hardware stack pointer in bits [7:0]. Reads give 0 in bits [31:8]; writes use only bits [7:0]. |
| `0x1F000008` | `ERR_STATUS` | read/write | the error flags (below). Any write clears all flags and `ERR_PC`. |
| `0x1F00000C` | `ERR_PC` | read only | the address of the instruction that caused the first error since the last clear |

### Error register

`ERR_STATUS` records illegal events, to help debugging:

- The flags are sticky: each flag stays set until software clears it by writing `ERR_STATUS`.
- The first event is recorded in detail: its kind (bits [10:8]) and its instruction address (`ERR_PC`). Later events only set their flag.

| Bit | Kind number | Flag | Set by |
| --- | --- | --- | --- |
| 0 | 1 | illegal instruction | a reserved ALU, multiply/divide, branch-condition or memory-size encoding |
| 1 | 2 | unaligned access | a word access not at a multiple of 4, or a halfword access not at a multiple of 2 |
| 2 | 3 | bad address | an access to an unmapped address, a write to ROM, or an access to CPU-internal I/O other than a word access to a register above |
| 3 | 4 | bad jump target | a taken jump or branch to an address that isn't a multiple of 4, or that's outside SDRAM and ROM |
| 4 | 5 | stack overflow | a push when SP is 255 |
| 5 | 6 | stack underflow | a pop when SP is 0 |
| [10:8] | — | first kind | the kind number of the first event since the last clear, or 0 if there was none |

All other bits read as 0.

- If one instruction causes two events, both flags are set, and the lower kind number counts as the first.
- Instructions that never execute don't set flags.

Currently no interrupt is attached to this, I might add that later.
