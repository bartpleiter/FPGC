# BDOS V2 Executive Summary

**Prepared by: OS Architecture Consultancy Team**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Updated based on stakeholder feedback - 64 MiB memory constraint, position-independent code, software interrupt instruction, custom raw Ethernet protocol*

---

## Team Composition

This report is prepared by a consultancy team of five experts:

1. **Dr. Sarah Chen** - Operating Systems Architect (Lead)
2. **Marcus Rodriguez** - Embedded Systems Specialist  
3. **Dr. Emily Watson** - Compiler & Toolchain Expert
4. **James O'Brien** - File Systems & Storage Specialist
5. **Aisha Patel** - Human-Computer Interaction & Input Systems Expert

---

## Project Overview

BDOS V2 is a text-based operating system designed for the FPGC (FPGA Computer) hobby project. It represents an evolution of the original BDOS, built from the ground up with improved architecture, maintainability, and proper separation of concerns.

### Goals

1. **Educational Value**: Implement common OS concepts found in real-world systems
2. **Text-Based Interface**: No GUI or mouse support required
3. **Single User**: Security is not a concern
4. **Multi-Program Support**: Load and switch between multiple programs ("alt-tab")
5. **BRFS Integration**: Full utilization of the custom filesystem
6. **Network Capabilities**: Custom protocol for program deployment and remote control
7. **Maintainable Codebase**: Clear abstractions and separation of concerns

---

## Executive Assessment of Old BDOS

### What Worked Well

1. **Simple Architecture**: Monolithic design kept complexity manageable
2. **Working Features**: Shell, filesystem, networking, USB keyboard all functional
3. **Interrupt Handling**: Basic user program interrupt forwarding implemented
4. **System Calls**: Basic syscall mechanism via memory-mapped communication

### Critical Limitations Identified

1. **Tight Coupling**: All components directly include each other with no clear boundaries
2. **Single Program Only**: No multi-tasking or program switching capability
3. **Hacky User Interrupt Handling**: Assembly-based interrupt forwarding is fragile
4. **No stdin/stdout Abstraction**: Direct hardware access instead of streams
5. **Fixed Memory Layout**: Hard-coded addresses throughout the codebase
6. **No Scheduler**: Single program runs until completion
7. **No Input Abstraction**: USB keyboard directly writes to HID FIFO

---

## Hardware Constraints Summary

### CPU (B32P3)
- 32-bit RISC, 5-stage pipeline
- 16 registers (r0 hardwired to 0)
- Word-addressable only (4 bytes per address)
- No MMU or paging support
- Single interrupt handler entry point
- 27-bit jump constants (512 MiB addressable)

### Memory
- SDRAM: 16 MiW (64 MiB) available on physical device
- I/O: 0x7000000 - 0x77FFFFF
- ROM: 1 KiW at 0x7800000 (boot location)
- VRAM: Various ranges starting at 0x7900000

**Note**: While the FPGA memory map supports up to 448 MiB, the physical device has 64 MiB.

### Toolchain
- **B32CC Compiler**: Single-pass, no linker, no struct returns, limited optimizations
- **ASMPY Assembler**: Supports pseudo-instructions, sections, labels
- **No Dynamic Linking**: All code must be compiled into single binary

---

## BDOS V2 Architecture Recommendation

### Kernel Model: Minimal Monolithic Kernel

Given the hardware constraints (no MMU, no linker), we recommend a **minimal monolithic kernel** with:

- Clear module boundaries through header files and naming conventions
- Defined interfaces between subsystems
- Hardware Abstraction Layer (HAL) for all hardware access
- System call interface for user programs

This is similar to early MS-DOS or CP/M, adapted for modern C programming practices.

### Memory Layout (Proposed)

**Total Available: 64 MiB (16 MiW)**

```
0x0000000 +---------------------------+
          |      BDOS Kernel          |
          |      (512 KiW / 2 MiB)    |
0x0080000 +---------------------------+
          |      Kernel Heap          |
          |      (512 KiW / 2 MiB)    |
0x0100000 +---------------------------+
          |                           |
          |      Program Area         |
          |      Position-Independent |
          |      14 × 512 KiW slots   |
          |      (7 MiW / 28 MiB)     |
          |      (programs can span   |
          |       multiple slots)     |
          |                           |
0x0800000 +---------------------------+
          |      BRFS Cache           |
          |      (8 MiW / 32 MiB)     |
0x1000000 +---------------------------+
```

Programs are compiled as **position-independent code** and can be loaded into any available slot(s). Large programs (like Doom) can span multiple consecutive 512 KiW slots.

### Key Components

| Component | Responsibility |
|-----------|----------------|
| **Kernel Core** | Boot, initialization, main loop, interrupt handling |
| **Process Manager** | Program slots, switching, state management |
| **Memory Manager** | Heap allocation, program loading |
| **File System** | BRFS integration, VFS abstraction |
| **Input Subsystem** | Keyboard, network HID, input event queue |
| **Output Subsystem** | Terminal, UART debug output |
| **Network Subsystem** | ENC28J60 driver, custom raw Ethernet protocol |
| **System Call Handler** | Software interrupt-based kernel services |
| **Shell** | Command interpreter (kernel component) |

---

## Key Design Decisions

### Decision 1: Shell Location

**Decision**: Shell runs in kernel space.

**Rationale**: A kernel shell simplifies development and is common in embedded/hobby OS designs (like CP/M, early DOS). This avoids the complexity of bootstrapping a user-space shell.

### Decision 2: Position-Independent Code

**Decision**: All user programs are compiled as position-independent code (PIC).

**Rationale**: This allows programs to be loaded at any available slot without compile-time slot assignment. The assembler converts absolute JUMP instructions to relative JUMPO instructions during assembly. This enables:
- Dynamic slot allocation at runtime
- Programs spanning multiple consecutive slots (for large programs like Doom)
- No need to compile programs multiple times for different slots

### Decision 3: Program Slots

**Decision**: 14 × 512 KiW slots with support for spanning multiple slots.

**Rationale**: 512 KiW granularity provides flexibility for both small utilities and large programs. Programs can request multiple consecutive slots during loading.

### Decision 4: Background Tasks

**Decision**: Timer-based cooperative tasks.

**Rationale**: Timer-based interrupt tasks (like the current USB keyboard polling) can run small kernel tasks without full preemption. Programs cooperatively yield for background network services.

### Decision 5: System Call Mechanism

**Decision**: Software interrupt instruction (new CPU instruction).

**Rationale**: A dedicated software interrupt instruction (`INT` or `SWI`) provides clean separation between user and kernel code. The CPU saves PC to a register and jumps to a fixed interrupt handler address. This is simpler than memory-mapped syscall frames.

---

## Implementation Phases

### Phase 1: Foundation (Weeks 1-2)
- Set up new codebase structure
- Implement HAL for all hardware
- Basic kernel with terminal output
- Interrupt handling framework

### Phase 2: Core Services (Weeks 3-4)
- Process manager with program slots
- Memory manager with heap
- System call interface
- BRFS integration

### Phase 3: Input/Output (Weeks 5-6)
- Input subsystem (keyboard, network HID)
- stdin/stdout abstraction
- Shell implementation

### Phase 4: Advanced Features (Weeks 7-8)
- Program switching (alt-tab)
- Network subsystem
- Background task support

### Phase 5: Polish (Weeks 9-10)
- Piping support
- Testing and debugging
- Documentation

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| Compiler limitations | High | Design around known issues, test early |
| Memory constraints | Medium | Careful memory map, measure usage |
| Interrupt complexity | High | Extensive testing, simple design first |
| Program switching bugs | Medium | Clear state machine, logging |

---

## Report Index

1. **Executive Summary** (this document)
2. **System Architecture** - Detailed component design
3. **Memory Management** - Memory map and allocation
4. **Process Management** - Program slots and switching
5. **System Calls & IPC** - Interface specification
6. **File System Integration** - BRFS usage patterns
7. **Input Subsystem** - Keyboard and input handling
8. **Network Subsystem** - ENC28J60 and custom protocol
9. **Shell & User Programs** - Command interface and user API
10. **Implementation Guide** - Code structure and examples

---

## Conclusion

BDOS V2 represents a significant architectural improvement over the original BDOS while remaining achievable within the hardware and toolchain constraints. The recommended minimal monolithic kernel with fixed program slots provides a good balance between functionality and complexity.

The key success factors are:
1. Clear abstraction layers from the start
2. Well-defined system call interface
3. Proper separation of kernel and user code
4. Incremental implementation with testing at each phase

The subsequent reports provide detailed specifications for each subsystem.
