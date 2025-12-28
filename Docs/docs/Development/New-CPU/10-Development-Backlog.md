# B32P3 Classic 5-Stage Pipeline - Development Backlog

**Project**: FPGC CPU Redesign (Phase 1)  
**Target**: Classic 5-Stage Pipeline as defined in RFC-001  
**Team**: Development Team (Implementation)  
**Reviewers**: Expert Team (Dr. Vasquez, Dr. Chen, Prof. Kim, Dr. Liu, Dr. Torres)

## Project Overview

This document tracks the development of the new B32P3 CPU with a classic 5-stage pipeline architecture. The goal is to replace the complex B32P2 design with a simpler, more maintainable architecture while maintaining ISA compatibility.

## Acceptance Criteria (Phase 1 Complete)

- [ ] All existing CPU tests pass (`make test-cpu`)
- [ ] Simplified hazard detection (3 types max)
- [ ] Clean forwarding logic
- [ ] Debug output via `$display` statements for debugging
- [ ] Documentation updated

## Architecture Summary

```
┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐
│  IF  │───►│  ID  │───►│  EX  │───►│ MEM  │───►│  WB  │
└──────┘    └──────┘    └──────┘    └──────┘    └──────┘
   │           │           │           │           │
   ▼           ▼           ▼           ▼           ▼
 I-Cache    Decode +     ALU +      D-Cache    Register
  + ROM     RegRead     Branch      + I/O      Writeback
```

## Sprint Backlog

### Story 1: Project Setup and Module Skeleton
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

Create the new B32P3 module structure while preserving the external interface to work with existing testbench and cache controller.

### Story 2: Pipeline Registers and Basic Flow
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

Implement pipeline registers (IF/ID, ID/EX, EX/MEM, MEM/WB) with basic data flow.

### Story 3: Instruction Fetch Stage (IF)
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

Implement IF stage with ROM and I-cache access, PC management.

### Story 4: Instruction Decode Stage (ID)
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

Implement ID stage with instruction decoding and register read.

### Story 5: Execute Stage (EX) - ALU Operations
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

Implement EX stage with ALU, branch resolution, and jump logic.

### Story 6: Memory Stage (MEM) - Basic
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

Implement MEM stage with memory access (VRAM, ROM read, I/O).

### Story 7: Writeback Stage (WB)
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

Implement WB stage with result selection and register writeback.

### Story 8: Data Forwarding Unit
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

Implement EX→EX and MEM→EX forwarding paths.

### Story 9: Hazard Detection Unit
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

Implement load-use hazard detection and stall logic.

### Story 10: Control Hazards (Branch/Jump)
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

Implement pipeline flush on branch/jump taken.

### Story 11: Cache Integration (L1i and L1d)
**Priority**: P0 (Blocker)  
**Estimate**: 4 hours  
**Status**: Not Started

Integrate with existing cache controller for cache misses.

### Story 12: Multi-Cycle Operations (Division/Multiplication)
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

Integrate multi-cycle ALU for division and multiplication.

### Story 13: Stack Operations (Push/Pop)
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

Implement hardware stack with push/pop operations.

### Story 14: Interrupt Support
**Priority**: P1 (High)  
**Estimate**: 2 hours  
**Status**: Not Started

Integrate interrupt controller and handling.

### Story 15: Final Integration and Testing
**Priority**: P0 (Blocker)  
**Estimate**: 4 hours  
**Status**: Not Started

Full integration, run all tests, fix remaining issues.

## Story Dependencies

```
Story 1 (Setup)
    │
    ├──► Story 2 (Pipeline Registers)
    │        │
    │        ├──► Story 3 (IF) ──────────────────┐
    │        │        │                          │
    │        │        └──► Story 11 (Cache)      │
    │        │                                   │
    │        ├──► Story 4 (ID) ──────────────────┤
    │        │                                   │
    │        ├──► Story 5 (EX) ──────────────────┼──► Story 8 (Forwarding)
    │        │        │                          │         │
    │        │        └──► Story 10 (Control)    │         │
    │        │                                   │         │
    │        ├──► Story 6 (MEM) ─────────────────┤         │
    │        │        │                          │         │
    │        │        └──► Story 12 (Multi-cyc)  │         │
    │        │                                   │         │
    │        └──► Story 7 (WB) ──────────────────┘         │
    │                                                      │
    └──► Story 13 (Stack)                                  │
                                                           │
Story 9 (Hazard Detection) ◄───────────────────────────────┘
    │
    └──► Story 14 (Interrupts)
            │
            └──► Story 15 (Final Integration)
```

## Existing Infrastructure to Reuse

### Modules to Keep (with possible interface changes)
- `InstructionDecoder.v` - Already extracts all needed fields
- `ALU.v` - Single-cycle ALU operations
- `MultiCycleALU.v` - Division, multiplication
- `ControlUnit.v` - Control signal generation
- `Stack.v` - Hardware stack
- `BranchJumpUnit.v` - Branch/jump resolution (may need simplification)
- `InterruptController.v` - Interrupt handling
- `AddressDecoder.v` - Memory map decoding
- `Regbank.v` - Register file
- `Regr.v` - Pipeline register primitive

### External Modules (Interface Must Match)
- `CacheControllerSDRAM.v` - L1 cache controller (external, 100MHz)
- `MemoryUnit.v` - Memory-mapped I/O
- VRAM modules - GPU memory
- ROM module - Boot ROM

### Test Infrastructure
- `make test-cpu` - Run all CPU tests
- `make test-cpu-single file=<path>` - Run single test
- `make debug-cpu file=<path>` - Debug with GTKWave
- `make sim-cpu` - General simulation

## Key Design Decisions

1. **5-Stage vs 6-Stage**: We use 5 stages (IF, ID, EX, MEM, WB) instead of the current 6 stages
2. **Unified MEM Stage**: Cache hit/miss handling in single MEM stage with stall
3. **No EXMEM Split**: Single EX stage for ALU, separate MEM for memory
4. **Simple Forwarding**: Only EX→EX and MEM→EX, no complex chains
5. **Full Pipeline Stall**: On cache miss or multi-cycle op, entire pipeline stalls

## Risk Register

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Cache interface mismatch | High | Medium | Early Story 11 prototyping |
| Hazard logic bugs | High | Medium | Comprehensive test suite |
| Performance regression | Medium | Low | CPI monitoring in tests |
| Timing issues | Medium | Low | Keep paths short, register outputs |

## Progress Tracking

| Story | Status | Started | Completed | Notes |
|-------|--------|---------|-----------|-------|
| 1 | Not Started | - | - | |
| 2 | Not Started | - | - | |
| ... | | | | |

---

## Changelog

| Date | Author | Changes |
|------|--------|---------|
| 2025-01-15 | Dev Team | Initial backlog created |
