# New CPU Architecture Documentation

This folder contains comprehensive documentation for redesigning the B32P2 CPU to achieve 100MHz operation with reduced hazard complexity.

## Document Overview

| Document | Description |
|----------|-------------|
| [Problem Statement](00-Problem-Statement.md) | Analysis of current B32P2 issues and requirements |
| [RFC-001: Classic 5-Stage](01-RFC-001-Classic-5-Stage.md) | Traditional MIPS-style pipeline proposal |
| [RFC-002: Decoupled Architecture](02-RFC-002-Decoupled-Architecture.md) | Fetch-execute decoupling with FIFO |
| [RFC-003: Scoreboard](03-RFC-003-Scoreboard-OoO.md) | Scoreboard-based dynamic scheduling |
| [RFC-004: Stall-Free Pipeline](04-RFC-004-Stall-Free-Pipeline.md) | Compiler-assisted delay slots |
| [RFC-005: Hybrid Queues](05-RFC-005-Hybrid-Result-Queues.md) | Completion queue architecture |
| [Summary and Recommendations](06-Summary-and-Recommendations.md) | Final expert recommendations |

## Quick Reference

### Current Issues

1. **Hazard Complexity**: 63+ pipeline states requiring explicit handling
2. **Timing Limitation**: 50MHz due to forwarding path delays
3. **Maintenance Burden**: Fragile hazard detection logic
4. **Extension Difficulty**: Adding features multiplies complexity

### Recommended Solution

**Phase 1**: Classic 5-Stage Pipeline (RFC-001)
- 12 days implementation
- 100MHz target
- ~40 MIPS

**Phase 2**: Hybrid Queue Enhancement (RFC-005)
- +6 days implementation  
- ~59 MIPS total

### Performance Comparison

| Design | Clock | CPI | MIPS | vs Current |
|--------|-------|-----|------|------------|
| Current B32P2 | 50MHz | 1.5 | 33.3 | baseline |
| Phase 1 (Classic) | 100MHz | 2.5 | 40.0 | +20% |
| Phase 2 (Hybrid) | 100MHz | 1.7 | 58.8 | +76% |

## Expert Team

- **Dr. Elena Vasquez** - Pipeline Architecture
- **Dr. Marcus Chen** - Timing Closure
- **Prof. Sarah Kim** - FPGA Architecture
- **Dr. James Liu** - Compiler Optimization
- **Dr. Michael Torres** - Verification
- **Prof. Anna Schmidt** - Cache Architecture

## Getting Started

1. Read the [Problem Statement](00-Problem-Statement.md) for context
2. Review [RFC-001](01-RFC-001-Classic-5-Stage.md) for the recommended starting point
3. Check [Summary and Recommendations](06-Summary-and-Recommendations.md) for implementation roadmap

## Implementation

| Document | Description |
|----------|-------------|
| [Development Backlog](10-Development-Backlog.md) | Sprint backlog and story tracking |

### Stories

| Story | Title | Status |
|-------|-------|--------|
| [Story 01](Stories/Story-01-Project-Setup.md) | Project Setup and Module Skeleton | Not Started |
| [Story 02](Stories/Story-02-Pipeline-Registers.md) | Pipeline Registers and Basic Flow | Not Started |
| [Story 03](Stories/Story-03-IF-Stage.md) | Instruction Fetch Stage (IF) | Not Started |
| [Story 04](Stories/Story-04-ID-Stage.md) | Instruction Decode Stage (ID) | Not Started |
| [Story 05](Stories/Story-05-EX-Stage.md) | Execute Stage (EX) - ALU Operations | Not Started |
| [Story 06](Stories/Story-06-MEM-Stage.md) | Memory Stage (MEM) - Basic | Not Started |
| [Story 07](Stories/Story-07-WB-Stage.md) | Writeback Stage (WB) | Not Started |
| [Story 08](Stories/Story-08-Forwarding-Unit.md) | Data Forwarding Unit | Not Started |
| [Story 09](Stories/Story-09-Hazard-Detection.md) | Hazard Detection Unit | Not Started |
| [Story 10](Stories/Story-10-Control-Hazards.md) | Control Hazards (Branch/Jump) | Not Started |
| [Story 11](Stories/Story-11-Cache-Integration.md) | Cache Integration (L1i and L1d) | Not Started |
| [Story 12](Stories/Story-12-MultiCycle-ALU.md) | Multi-Cycle Operations (Division/Multiplication) | Not Started |
| [Story 13](Stories/Story-13-Stack-Operations.md) | Stack Operations (Push/Pop) | Not Started |
| [Story 14](Stories/Story-14-Interrupt-Support.md) | Interrupt Support | Not Started |
| [Story 15](Stories/Story-15-Final-Integration.md) | Final Integration and Testing | Not Started |

## Related Documentation

- [Architecture Development Guide](../Architecture-Development-Guide.md) - Current CPU documentation
- [ISA Documentation](../../Hardware/CPU%20(B32P2)/ISA.md) - Instruction set reference
