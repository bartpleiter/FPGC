# CPU

The B32P3 is a 32-bit RISC CPU designed from scratch for the FPGC. It's the third iteration of the B32P design, optimized to run at 100 MHz on a Cyclone IV FPGA. The architecture follows a classic 5-stage MIPS-style pipeline.

## Architecture Overview

The CPU has 16 general-purpose 32-bit registers (r0 is hardwired to zero), a 256-entry hardware stack, and a byte-addressable address space. It runs at a single clock frequency of 100 MHz with no clock gating or dynamic frequency scaling.

The pipeline has five stages:

1. **IF (Instruction Fetch)**: Reads the next instruction from ROM or L1I cache
2. **ID (Instruction Decode)**: Decodes instruction fields, reads register file
3. **EX (Execute)**: ALU operations, branch condition evaluation
4. **MEM (Memory Access)**: Load/store through L1D cache, VRAM, or I/O
5. **WB (Write Back)**: Writes results back to the register file

In ideal conditions (cache hits, no hazards, no branches), the CPU executes one instruction per clock cycle. In practice, stalls from cache misses, multi-cycle ALU operations (like division), and pipeline hazards reduce the throughput.

## Instruction Set

The ISA has 16 instructions, all 32 bits wide. There are no variable-length instructions or instruction modes. The opcode is always in the top 4 bits.

### Instruction Encoding

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

The instruction set is split into four categories:

**Control flow:** HALT, JUMP, JUMPR, BRANCH, SAVPC, RETI

**Memory access:** READ (load word/halfword/byte), WRITE (store word/halfword/byte), PUSH, POP

**Arithmetic/Logic (single-cycle):** ARITH and ARITHC use the combinational ALU. ARITHC takes a 16-bit immediate instead of a second register.

**Arithmetic/Logic (multi-cycle):** ARITHM and ARITHMC use the multi-cycle ALU for multiplication, division, and modulo. Division takes about 32 cycles.

**Miscellaneous:** INTID (get interrupt ID), CCACHE (flush + invalidate the L1 instruction cache; required after self-modifying code or loading new code into SDRAM), CCACHED (flush + invalidate the L1 data cache; required for DMA producers/consumers — see [Memory Map](../Memory-Map.md) for the DMA register block)

### READ/WRITE Sub-Opcodes

The READ and WRITE instructions support sub-word access through a 4-bit sub-opcode field. When the sub-opcode is `0000`, the instruction performs a standard 32-bit word operation (backward compatible). Other encodings select byte or halfword access:

**READ sub-opcode (bits [7:4]):**

| Sub-opcode | Mnemonic | Description |
|------------|----------|-------------|
| `0000` | `read` | Load 32-bit word |
| `0001` | `readb` | Load byte, sign-extend to 32 bits |
| `0101` | `readbu` | Load byte, zero-extend to 32 bits |
| `0010` | `readh` | Load halfword, sign-extend to 32 bits |
| `0110` | `readhu` | Load halfword, zero-extend to 32 bits |

**WRITE sub-opcode (bits [3:0]):**

| Sub-opcode | Mnemonic | Description |
|------------|----------|-------------|
| `0000` | `write` | Store 32-bit word |
| `0001` | `writeb` | Store byte (low 8 bits of register) |
| `0010` | `writeh` | Store halfword (low 16 bits of register) |

Word loads/stores require 4-byte alignment, halfword loads/stores require 2-byte alignment, and byte loads/stores have no alignment requirement. Unaligned accesses silently ignore the low bits (the hardware does not raise an exception).

### ALU Operations (Single-Cycle)

| Opcode | Operation | Description |
|--------|-----------|-------------|
| `0000` | OR | Bitwise OR |
| `0001` | AND | Bitwise AND |
| `0010` | XOR | Bitwise XOR |
| `0011` | ADD | Addition |
| `0100` | SUB | Subtraction |
| `0101` | SHIFTL | Logical shift left |
| `0110` | SHIFTR | Logical shift right |
| `0111` | NOT | Bitwise NOT (of A) |
| `1010` | SLT | Set if A < B (signed) |
| `1011` | SLTU | Set if A < B (unsigned) |
| `1100` | LOAD | Load B (or constant) |
| `1101` | LOADHI | Load upper 16 bits: `{const16, A[15:0]}` |
| `1110` | SHIFTRS | Arithmetic shift right |

### ALU Operations (Multi-Cycle)

| Opcode | Operation | Description | Cycles |
|--------|-----------|-------------|--------|
| `0000` | MULTS | Signed multiply | ~4 |
| `0001` | MULTU | Unsigned multiply | ~4 |
| `0010` | MULTFP | Fixed-point multiply (Q16.16) | ~4 |
| `0011` | DIVS | Signed divide | ~32 |
| `0100` | DIVU | Unsigned divide | ~32 |
| `0101` | DIVFP | Fixed-point divide | ~32 |
| `0110` | MODS | Signed modulo | ~32 |
| `0111` | MODU | Unsigned modulo | ~32 |
| `1000` | FMUL | FP64 fixed-point multiply (Q32.32) | ~5 |
| `1001` | FADD | FP64 fixed-point add (Q32.32) | 1 |
| `1010` | FSUB | FP64 fixed-point subtract (Q32.32) | 1 |
| `1011` | FLD | Load FP64 register from two CPU registers | 1 |
| `1100` | FSTHI | Store FP64 register high word to CPU register | 1 |
| `1101` | FSTLO | Store FP64 register low word to CPU register | 1 |
| `1110` | MULSHI | Signed multiply, return upper 32 bits | ~4 |
| `1111` | MULTUHI | Unsigned multiply, return upper 32 bits | ~4 |

The FP64 operations (`1000`–`1101`) use the FP64 coprocessor described below. MULSHI and MULTUHI return the upper 32 bits of a 64-bit multiply result, which is useful for implementing fixed-point arithmetic without the coprocessor.

### Branch Conditions

| Opcode | Condition | Signed variant (S=1) |
|--------|-----------|---------------------|
| `000` | BEQ (A == B) | Same |
| `001` | BGT (A > B) | BGTS |
| `010` | BGE (A >= B) | BGES |
| `100` | BNE (A != B) | Same |
| `101` | BLT (A < B) | BLTS |
| `110` | BLE (A <= B) | BLES |

### Registers

16 registers, r0 hardwired to zero:

| Register | Notes |
|----------|-------|
| r0 | Always zero. Writes are ignored. |
| r1 through r14 | General purpose |
| r15 | General purpose. Conventionally used for return values. |

## Hardware Stack

The CPU has a 256-entry hardware stack with dedicated PUSH and POP instructions. The stack is used primarily for saving/restoring registers during function calls and interrupt handlers. The stack pointer wraps around at 256, so pushing beyond that will overwrite old entries silently.

The stack pointer is readable and writable as a CPU-internal I/O register at `0x1F000004`, which is useful for context switching or debugging.

## Memory Map

All memory and I/O is mapped into a flat byte-addressed space. The CPU starts execution at the ROM address (`0x1E000000`).

| Address Range | Region | Size | Description |
|---|---|---|---|
| `0x0000000` - `0x03FFFFFF` | SDRAM | 64 MiB | Main working memory, accessed through L1I/L1D caches |
| `0x1C000000` - `0x1C00006F` | I/O | 28 registers | UART, SPI, Timers, GPIO, etc. |
| `0x1E000000` - `0x1E000FFF` | ROM | 4 KiB (1 KiW) | Boot ROM (also the initial PC value) |
| `0x1E400000` - `0x1E40107C` | VRAM32 | 32-bit entries | Tile patterns and palettes |
| `0x1E800000` - `0x1E808004` | VRAM8 | 8-bit entries | Tile maps, scroll registers |
| `0x1EC00000` - `0x1EC4AFFC` | VRAMpixel | 8-bit entries | 320x240 pixel framebuffer (external SRAM) |
| `0x1F000000` - `0x1F000004` | CPU Internal I/O | 2 registers | PC Backup (`0x00`), Stack Pointer (`0x04`) |

SDRAM is the main working memory. It's accessed through L1 instruction (L1I) and data (L1D) caches, so most reads complete in a single cycle on cache hits. Only SDRAM and ROM can be used as instruction memory.

The VRAM regions are on-chip dual-port block RAM (VRAM32 and VRAM8) or external SRAM (VRAMpixel) and are accessed in a single cycle without caching. They are used by the GPU for rendering.

I/O devices are accessed through the Memory Unit, which is a separate module that handles SPI, UART, timers, and other peripherals. I/O accesses stall the pipeline until complete.

## Interrupts

The CPU supports 8 interrupt lines, priority-encoded (lower index = higher priority). Interrupts are edge-triggered with CDC synchronization.

When an interrupt fires:
1. The current PC is saved to `PC_backup` (readable/writable at `0x1F000000`)
2. Interrupts are disabled (no nesting)
3. PC jumps to address `0x0000004` (the interrupt handler, i.e., the second instruction)

The handler uses INTID to determine which interrupt fired, handles it, then executes RETI to restore the PC and re-enable interrupts.

### Interrupt Assignments

| Bit | INT ID | Source | Description |
|-----|--------|--------|-------------|
| 0 | 1 | UART RX | UART byte received |
| 1 | 2 | Timer 1 (OST1) | OS timer 1 |
| 2 | 3 | Timer 2 (OST2) | OS timer 2 (USB keyboard polling) |
| 3 | 4 | Timer 3 (OST3) | OS timer 3 (delay) |
| 4 | 5 | Frame Drawn | GPU vblank signal |
| 5 | 6 | ENC28J60 RX | Ethernet packet received (inverted `~INT` pin) |
| 6 | 7 | DMA Done | DMA engine transfer complete (or error) |
| 7 | 8 | *(unused)* | — |

An important constraint: interrupts only fire when a jump or branch is being taken in the MEM stage. This greatly simplifies pipeline hazard handling during interrupt delivery, at the cost of slightly delayed interrupt response. In practice, most code has enough jumps (function calls, loops) that the latency is negligible.

## FP64 Coprocessor

The FP64 coprocessor extends the B32P3 with 64-bit fixed-point arithmetic using a signed Q32.32 format. It has its own register file of 8 × 64-bit registers (`f0`–`f7`), separate from the CPU's general-purpose registers.

### Q32.32 Fixed-Point Format

Each 64-bit FP register stores a value in signed Q32.32 format:

```text
 63        32 31         0
+------------+------------+
|   hi (s32) |   lo (u32) |
+------------+------------+
```

- **hi** (bits 63–32): Signed 32-bit integer part
- **lo** (bits 31–0): Unsigned 32-bit fractional part (representing a value in the range [0, 1))

The represented value is: `value = hi + lo / 2^32`

For example, to represent 3.75: `hi = 3`, `lo = 0xC0000000` (0.75 × 2^32).

### FP64 Register File

| Register | Width | Description |
|----------|-------|-------------|
| f0–f7 | 64 bits | General-purpose FP64 registers |

There is no hardwired zero register. All 8 registers are freely usable. The register index is encoded in 3 bits within the ARITHM instruction fields (A, B, and D register fields, using only the lower 3 bits).

### FP64 Instructions

All FP64 instructions use the **ARITHM** encoding (opcode `0010`). The `alu_op` field (bits 27–24) selects the operation. Register fields A, B, and D are reused: for FP64 operations, only the lower 3 bits index into the FP64 register file.

| Instruction | alu_op | Operands | Description |
|-------------|--------|----------|-------------|
| FMUL | `1000` | fd, fa, fb | `fd = fa × fb` (Q32.32 multiply, ~5 cycles) |
| FADD | `1001` | fd, fa, fb | `fd = fa + fb` (Q32.32 add, 1 cycle) |
| FSUB | `1010` | fd, fa, fb | `fd = fa - fb` (Q32.32 subtract, 1 cycle) |
| FLD | `1011` | fd, areg, breg | `fd = {areg, breg}` (load hi/lo from CPU registers) |
| FSTHI | `1100` | dreg, fa | `dreg = fa[63:32]` (store hi word to CPU register) |
| FSTLO | `1101` | dreg, fa | `dreg = fa[31:0]` (store lo word to CPU register) |

FLD loads an FP64 register from two CPU registers: `areg` provides the high (integer) word and `breg` provides the low (fractional) word. FSTHI and FSTLO extract the high or low 32-bit half of an FP64 register into a CPU register, which is the only way to move data from the FP64 register file back to the CPU.

FADD and FSUB complete in a single cycle. FMUL takes approximately 5 cycles because it uses DSP multiplier blocks and accumulates partial products. During multi-cycle FP64 operations, the pipeline stalls the same way as for regular multi-cycle ALU operations.
