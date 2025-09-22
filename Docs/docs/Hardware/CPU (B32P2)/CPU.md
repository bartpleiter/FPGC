# CPU (B32P2)

The B32P2 is a 32-bit pipelined RISC processor designed specifically for the FPGC project. It is the second iteration of the B32P architecture, with a focus on single-cycle execution of all pipeline stages and improved performance through efficient memory access and hazard handling.

## Overview

The B32P2 implements a 6-stage pipeline architecture optimized for fast memory access without too much complexity. It features:

- **32-bit RISC ISA** with support for most important common instructions
- **6-stage pipeline** for improved throughput
- **Dual L1 caches** (instruction and data) with cache controller to greatly reduce stalls
- **Hazard detection and forwarding** to further minimize stalls
- **Multi-cycle ALU operations** for complex arithmetic to remove the need for a (slower) memory mapped co-processor
- **32-bit address space** supporting up to 16GiB of addressable memory, although the static jump instruction is limited to 27 bits
- **Hardware interrupt support** with context switching by saving/restoring the program counter (PC)

## Pipeline Architecture

The B32P2 uses a 6-stage pipeline designed to minimize critical path delays while maintaining high instruction throughput:

```
┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│   FE1   │───▶│   FE2   │───▶│   REG   │───▶│ EXMEM1  │───▶│ EXMEM2  │───▶│   WB    │
│ I-Cache │    │I-Cache  │    │Register │    │Execute &│    │Multi-   │    │Write-   │
│ Fetch   │    │Miss     │    │Read     │    │D-Cache  │    │cycle &  │    │back     │
│         │    │Handling │    │         │    │Access   │    │D-Cache  │    │         │
│         │    │         │    │         │    │         │    │Miss     │    │         │
└─────────┘    └─────────┘    └─────────┘    └─────────┘    └─────────┘    └─────────┘
```

### Pipeline Stages

1. **FE1 (Fetch 1)**: Instruction cache fetch and ROM access
2. **FE2 (Fetch 2)**: Instruction cache miss handling and result selection  
3. **REG (Register)**: Register file read and instruction decode
4. **EXMEM1 (Execute/Memory 1)**: ALU execution and data cache access
5. **EXMEM2 (Execute/Memory 2)**: Multi-cycle ALU completion and data cache miss handling
6. **WB (Writeback)**: Register file writeback

## Memory Hierarchy

The B32P2 is mostly optimized to handle specifically different types of memory efficiently, although this does reduce the flexibility of memory types and addresses.

### Memory Types

- **(DDR3) SDRAM**: Main system memory via MIG 7 controller, cached in L1
- **ROM**: Boot memory for initial program loading
- **VRAM32**: 32-bit video memory for tile-based graphics
- **VRAM8**: 8-bit video memory for tile-based graphics
- **VRAMPX**: Bitmap video memory for pixel manipulation
- **Other I/O devices**: Memory-mapped I/O or memory at lower speeds

## Hazard Handling

The B32P2 implements hazard detection and resolution:

### Hazard Types Handled

1. **Data hazards**: RAW (Read-After-Write) dependencies
2. **Control hazards**: Branches and jumps
3. **Structural hazards**: Resource conflicts in multi-cycle operations

### Resolution Strategies

- **Forwarding/Bypassing**: Results forwarded directly between pipeline stages
- **Pipeline stalling**: When forwarding is not possible, or on multi-cycle operations/cache misses
- **Pipeline flushing**: For branches/jumps and complex multi-cycle dependencies

### Forwarding Network

The CPU supports forwarding from:
- **EXMEM2 → EXMEM1**: Mostly when instruction uses result from previous instruction
- **WB → EXMEM1**: Mostly when instruction uses result from 2 instructions ago

## Performance Characteristics

### Design Goals

- **50MHz target frequency** to keep aligned with the 100MHz MIG 7 controller clock
- **Single-cycle execution** for most instructions
- **Minimal pipeline stalls** through efficient hazard handling
- **Cache-optimized memory access** to reduce the huge SDRAM latency performance hit

### Critical Path Optimizations

- Pipeline stages designed for single-cycle execution
- Address decoding distributed across pipeline stages
- Multi-cycle operations isolated to dedicated stages
- Cache controllers operate independently of CPU pipeline
