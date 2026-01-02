# CPU (B32P3)

The B32P3 is a 32-bit pipelined RISC processor designed specifically for the FPGC project. It implements a classic 5-stage MIPS-style pipeline with simplified hazard handling, data forwarding, and optimized timing for running at 100MHz on a Cyclone IV/10 (and better) FPGA.

## Overview

The B32P3 implements a classic 5-stage pipeline architecture designed for simplicity and good timing characteristics. It features:

- **32-bit RISC ISA** with support for most important common instructions
- **Classic 5-stage pipeline** (IF, ID, EX, MEM, WB) for straightforward design
- **Dual L1 caches** (instruction and data) with cache controller to greatly reduce stalls
- **Simple hazard detection** with load-use stalls and data forwarding
- **Hardware stack** with push/pop instructions for single cycle stack operations (only 128 entries, but allows for quick and easy register saving and restoring)
- **Multi-cycle ALU operations** for complex arithmetic (multiplication using registered hardware multiplier or DSP blocks, division) and extensions for Fixed Point operations
- **32-bit address space** supporting up to 16GiB of addressable memory, with 27-bit jump constants for 512MiB jumpable instruction memory (in theory more by using JUMPR instructions)
- **Hardware interrupt support** with context switching by saving/restoring the program counter (PC)
- **Branch resolution in MEM stage** for improved timing at the cost of slightly higher branch penalty

## Pipeline Architecture

The B32P3 uses a classic 5-stage pipeline following the MIPS-style design:

```text
┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│    IF   │───▶│    ID   │───▶│    EX   │───▶│   MEM   │───▶│    WB   │
│  Instr  │    │ Decode  │    │ Execute │    │ Memory  │    │  Write  │
│  Fetch  │    │ & Reg   │    │   ALU   │    │ Access  │    │  Back   │
│         │    │  Read   │    │         │    │         │    │         │
└─────────┘    └─────────┘    └─────────┘    └─────────┘    └─────────┘
```

### Pipeline Stages

1. **IF (Instruction Fetch)**: Fetches instructions from L1I cache or ROM. Handles cache miss detection and initiates cache controller requests on misses.

2. **ID (Instruction Decode & Register Read)**: Decodes the instruction and reads source operands from the register file.

3. **EX (Execute)**: Performs ALU operations, calculates memory addresses, and prepares branch comparison operands. Data forwarding is applied here to resolve RAW hazards.

4. **MEM (Memory Access)**: Performs load/store operations, resolves branches/jumps, and handles L1D cache accesses. Branch resolution is performed here for improved timing.

5. **WB (Write Back)**: Writes results back to the register file. Results are selected from ALU output, memory data, stack data, or special registers.

## Memory Map

The B32P3 implements a 32-bit memory map with the following regions:

| Start Address | End Address | Size | Memory Type | Description |
|---------------|-------------|------|-------------|-------------|
| `0x0000000` | `0x6FFFFFF` | 112 MiW | SDRAM | Main system memory (cached) |
| `0x7000000` | `0x77FFFFF` | N/A | I/O | Memory-mapped peripherals |
| `0x7800000` | `0x78003FF` | 1 KiW | ROM | Boot ROM - CPU starts here |
| `0x7900000` | `0x790041F` | 1056 W | VRAM32 | 32-bit video memory |
| `0x7A00000` | `0x7A02001` | 8194 W | VRAM8 | 8-bit video memory |
| `0x7B00000` | `0x7B12BFF` | 76800 W | VRAMPX | Pixel buffer (320×240) |

### Memory Types

- **SDRAM**: Main system memory via SDRAM controller, cached in L1. Direct-mapped with 8-word cache lines.
- **ROM**: Boot memory for initial program loading. Dual-port for instruction fetch and data access.
- **VRAM32**: 32-bit video memory for tile-based graphics.
- **VRAM8**: 8-bit video memory for tile-based graphics.
- **VRAMPX**: Bitmap video memory for pixel manipulation.
- **I/O**: Memory-mapped peripherals accessed via the Memory Unit.

## Hazard Handling

The B32P3 implements straightforward hazard detection and resolution mechanisms:

### Hazard Types

1. **Load-Use Hazard**: When an instruction in ID needs data from a load currently in EX. Resolved by stalling one cycle.

2. **Pop-Use Hazard**: Similar to load-use, but for stack pop operations whose data isn't available until WB.

3. **Cache Line Hazard**: Back-to-back SDRAM accesses to different cache lines. The cache has 1-cycle read latency, so a stall is needed when consecutive memory operations target different cache lines.

4. **Control Hazards**: Branches and jumps resolved in MEM stage. Results in 2-cycle branch penalty (flush IF/ID, ID/EX, EX/MEM).

### Resolution Strategies

- **Data Forwarding**: Results forwarded from EX/MEM and MEM/WB stages to EX stage inputs
- **Pipeline Stalling**: For load-use hazards, pop-use hazards, cache misses, and multi-cycle operations
- **Pipeline Flushing**: For taken branches, jumps, interrupts, and return-from-interrupt

### Forwarding Network

The CPU supports two-level forwarding to the EX stage:

- **EX/MEM → EX**: Forward ALU results from the previous instruction (1 cycle old)
- **MEM/WB → EX**: Forward results from 2 instructions ago

## Interrupt Support

The B32P3 supports up to 8 hardware interrupt lines with the following behavior:

- Interrupts are only valid when executing from SDRAM (PC < ROM_ADDRESS)
- Interrupts only trigger during jump instructions to simplify hazard handling
- On interrupt: PC is saved, interrupts disabled, jump to INTERRUPT_JUMP_ADDR
- `INTID` instruction retrieves the interrupt ID (1-8)
- `RETI` instruction restores PC and re-enables interrupts
