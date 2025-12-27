# Expert Consultation Summary and Recommendations

**Document**: Final Report from Expert Consultation Team  
**Date**: 2025-01-15  
**Client**: FPGC Project  

## Executive Summary

The expert team has analyzed the B32P2 CPU design challenges and produced five RFC proposals for addressing the hazard complexity and timing issues. This document summarizes findings and provides final recommendations.

## Team Composition

| Expert | Specialization | Primary Contribution |
|--------|---------------|---------------------|
| Dr. Elena Vasquez | Pipeline Architecture | RFC-001: Classic 5-Stage |
| Dr. Marcus Chen | Timing Closure | RFC-002: Decoupled Architecture |
| Prof. Sarah Kim | FPGA Architecture | RFC-003: Scoreboard |
| Dr. James Liu | Compiler Optimization | RFC-004: Stall-Free Pipeline |
| Dr. Michael Torres | Verification | RFC-005: Hybrid Queues |
| Prof. Anna Schmidt | Cache Architecture | RFC-005: Hybrid Queues |

## Problem Summary

The current B32P2 design suffers from:

1. **Hazard Complexity Explosion**: 63+ possible pipeline states requiring explicit handling
2. **Timing Path Limitations**: Forwarding logic creates 15-20ns critical paths (50MHz limit)
3. **Extension Difficulty**: Adding features like debugging multiplies state space
4. **Maintenance Burden**: Hazard logic is fragile and error-prone

## Proposal Comparison Matrix

| Criteria | RFC-001 | RFC-002 | RFC-003 | RFC-004 | RFC-005 |
|----------|---------|---------|---------|---------|---------|
| **Architecture** | Classic 5-Stage | Decoupled Fetch | Scoreboard | Stall-Free | Hybrid Queues |
| **Clock Target** | 100MHz | 100MHz | 100MHz | 125MHz | 100MHz |
| **Estimated CPI** | 2.5 | 2.3 | 2.5 | 2.64 | 1.7 |
| **Estimated MIPS** | 40.0 | 43.5 | 40.0 | 47.3 | 58.8 |
| **vs Current** | +20% | +30% | +20% | +42% | +76% |
| **Hardware Complexity** | Low | Medium | Medium | Very Low | Medium-High |
| **Compiler Changes** | None | None | None | Major | None |
| **Hazard Logic** | Simple | Simple | Centralized | None | Distributed |
| **Timing Risk** | Low | Low | Medium | Very Low | Medium |
| **Implementation Time** | 12 days | 15 days | 16 days | 15 days* | 18 days |
| **Debug Support** | Easy | Easy | Easy | Trivial | Easy |
| **Extensibility** | Good | Good | Excellent | Poor** | Excellent |

*Plus significant compiler work
**Requires compiler changes for new instructions

## Detailed Recommendation

### Primary Recommendation: RFC-001 (Classic 5-Stage) → RFC-005 (Hybrid Queues)

We recommend a **phased approach**:

#### Phase 1: Classic 5-Stage Pipeline (RFC-001)

**Rationale**:
- Lowest implementation risk
- Well-documented design pattern
- Achieves primary goals (100MHz, simpler hazards)
- Educational value: understand classic pipeline deeply
- Foundation for future enhancements

**Expected Outcome**:
- 100MHz operation
- ~40 MIPS (20% improvement)
- Clean codebase for future work
- ~12 days implementation

#### Phase 2: Hybrid Queues Enhancement (RFC-005)

**Rationale**:
- Builds on Phase 1 foundation
- Adds significant performance (additional 47%)
- Modular addition to classic pipeline
- Can be done incrementally

**Expected Outcome**:
- 100MHz operation (same clock)
- ~59 MIPS (76% total improvement over current)
- Better cache miss handling
- ~6 additional days (queues overlay on classic base)

### Alternative Path: Scoreboard (RFC-003)

If maintainability is the absolute priority over performance:

**Scoreboard Benefits**:
- Single source of truth for hazard state
- Easiest to extend with new instructions
- Cleanest conceptual model

**Scoreboard Trade-offs**:
- Same performance as classic (no queue benefits)
- Central bottleneck potential
- Slightly more complex timing

### Not Recommended: RFC-004 (Stall-Free)

While elegant in hardware simplicity, the compiler changes required violate the project's learning goals for the current phase. Consider for future educational exploration.

## Implementation Roadmap

### Month 1: Classic Pipeline (Phase 1)

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 1 | Design review, module skeleton | Module interfaces defined |
| 2 | Pipeline core, hazard unit | Basic instruction flow |
| 3 | Cache integration, multi-cycle | Full functionality |
| 4 | Testing, timing closure | 100MHz verified |

### Month 2: Hybrid Enhancements (Phase 2)

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 1 | ALU Queue, MEM Queue | Queue infrastructure |
| 2 | Completion arbiter | Out-of-order completion |
| 3 | Forwarding integration | Full forwarding from queues |
| 4 | Testing, optimization | Performance verified |

### Parallel Track: Documentation and Testing

- Update Architecture-Development-Guide.md
- Create comprehensive test suite for new CPU
- Document hazard handling for future reference

## Risk Mitigation

### Timing Closure Risk

| Risk | Mitigation |
|------|-----------|
| ALU path too long | Register forwarding mux outputs |
| Cache timing | Use dual-port BRAM, optimize tag compare |
| Clock crossing | Keep all CPU logic in single clock domain |

### Verification Risk

| Risk | Mitigation |
|------|-----------|
| Pipeline correctness | Reuse existing test suite |
| Hazard detection | Add formal assertions |
| Cache interaction | Dedicated cache tests |

### Schedule Risk

| Risk | Mitigation |
|------|-----------|
| Unexpected complexity | Start with classic (lowest risk) |
| Timing iterations | Build in 1 week buffer |
| Tool issues | Use proven Quartus synthesis patterns |

## Success Metrics

### Phase 1 Success Criteria

- [ ] All existing tests pass
- [ ] 100MHz timing closure on Cyclone IV
- [ ] Hazard logic < 100 lines
- [ ] Debug halt/resume works
- [ ] Code compiles with existing B32CC

### Phase 2 Success Criteria

- [ ] All existing tests pass
- [ ] MIPS improvement > 50% over current B32P2
- [ ] Cache miss penalty reduced
- [ ] Multiple outstanding loads supported

## Appendices

### A. Referenced Documents

1. [Problem Statement](00-Problem-Statement.md)
2. [RFC-001: Classic 5-Stage](01-RFC-001-Classic-5-Stage.md)
3. [RFC-002: Decoupled Architecture](02-RFC-002-Decoupled-Architecture.md)
4. [RFC-003: Scoreboard](03-RFC-003-Scoreboard-OoO.md)
5. [RFC-004: Stall-Free Pipeline](04-RFC-004-Stall-Free-Pipeline.md)
6. [RFC-005: Hybrid Queues](05-RFC-005-Hybrid-Result-Queues.md)

### B. Glossary

| Term | Definition |
|------|-----------|
| CPI | Cycles Per Instruction - average cycles to execute one instruction |
| MIPS | Million Instructions Per Second - raw throughput metric |
| Forwarding | Bypassing pipeline registers to provide data early |
| Hazard | Condition requiring special handling to maintain correctness |
| Scoreboard | Central table tracking register/resource availability |
| Completion Queue | FIFO buffer tracking outstanding operations |

### C. Expert Consensus Notes

**Unanimous Agreement**:
- Current B32P2 hazard complexity is unsustainable
- 100MHz is achievable on Cyclone IV with proper design
- Classic 5-stage is the safest starting point
- Performance should improve by at least 20%

**Points of Discussion**:
- Scoreboard vs forwarding complexity (resolved: forwarding preferred for performance)
- Compiler-assisted scheduling (resolved: defer to future phase)
- Queue depth sizing (resolved: start small, parameterize)

---

## Final Statement

The FPGC project's goal of learning every layer of computer design is well-served by implementing a classic pipeline first. This foundational knowledge will make subsequent enhancements (queues, decoupling, etc.) more meaningful and successful.

The current B32P2 design, while ambitious, tried to optimize too many dimensions simultaneously. A return to fundamentals, followed by incremental enhancement, will yield better results.

**Approved by Expert Team**: 2025-01-15

---

## Change Log

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-01-15 | Initial release |
