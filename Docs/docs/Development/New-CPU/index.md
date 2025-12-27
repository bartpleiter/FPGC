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

## Related Documentation

- [Architecture Development Guide](../Architecture-Development-Guide.md) - Current CPU documentation
- [ISA Documentation](../../Hardware/CPU%20(B32P2)/ISA.md) - Instruction set reference
