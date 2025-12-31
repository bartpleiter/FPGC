# CPU Revisions

This page documents the evolution of the B32P CPU architecture through its various revisions.

## B32P3 (Current)

B32P3 is the third iteration of the B32P architecture, representing a significant redesign focused on simplicity and timing optimization.

### Design Philosophy

- **Classic MIPS-style pipeline**: 5-stage design (IF, ID, EX, MEM, WB) following well-established patterns
- **Simplified hazard handling**: Straightforward load-use detection with data forwarding
- **Timing-focused optimizations**: Critical paths broken up for reliable high-frequency operation
- **Easier verification**: Simpler design is easier to debug and validate

### Key Characteristics

- 5-stage pipeline (IF, ID, EX, MEM, WB)
- Branch resolution in MEM stage (2-cycle branch penalty)
- 2-cycle register file read latency
- EX→EX and MEM→EX forwarding
- Registered cache stall signals

---

## B32P2

B32P2 was the second iteration of the B32P architecture, with a focus on single-cycle execution of all pipeline stages.

### Design Philosophy

- **Deeper pipeline**: 6-stage design to separate concerns and handle cache misses
- **Single-cycle per stage**: Each stage designed to complete in one clock cycle
- **Complex hazard handling**: Multiple hazard types with sophisticated forwarding

### Key Characteristics

- 6-stage pipeline (FE1, FE2, REG, EXMEM1, EXMEM2, WB)
- Branch resolution in EXMEM1 stage (1-cycle branch penalty)
- Dedicated cache miss handling stages (FE2, EXMEM2)
- EXMEM2→EXMEM1 and WB→EXMEM1 forwarding
- Complex hazard detection (load-use, pop+WB conflict, multi-cycle dependency)

### Pipeline Stages

1. **FE1 (Fetch 1)**: Instruction cache fetch and ROM access
2. **FE2 (Fetch 2)**: Instruction cache miss handling
3. **REG**: Register file read and instruction decode
4. **EXMEM1**: ALU execution and data cache access
5. **EXMEM2**: Multi-cycle ALU and data cache miss handling
6. **WB**: Register file writeback

---

## B32P3 vs B32P2 Comparison

### Pipeline Architecture

| Aspect | B32P2 | B32P3 |
|--------|-------|-------|
| Pipeline depth | 6 stages | 5 stages |
| Pipeline style | Custom (FE1/FE2/REG/EXMEM1/EXMEM2/WB) | Classic MIPS (IF/ID/EX/MEM/WB) |
| Cache miss handling | Dedicated stages (FE2, EXMEM2) | Stall-based |
| Complexity | Higher | Lower |

### Hazard Handling

| Aspect | B32P2 | B32P3 |
|--------|-------|-------|
| Branch resolution | EXMEM1 stage | MEM stage |
| Branch penalty | 1 cycle | 2 cycles |
| Forwarding | EXMEM2→EXMEM1, WB→EXMEM1 | EX/MEM→EX, MEM/WB→EX |
| Load-use hazard | Complex multi-stage | Simple 1-cycle stall |
| Hazard types | 3 (load-use, pop+WB, multi-cycle) | 3 (load-use, pop-use, cache-line) |

### Register File

| Aspect | B32P2 | B32P3 |
|--------|-------|-------|
| Read latency | 1 cycle | 2 cycles |
| Address source | REG stage | IF stage |
| Data available | EXMEM1 stage | EX stage |

### Timing Optimizations

| Aspect | B32P2 | B32P3 |
|--------|-------|-------|
| Critical path | Branch evaluation in EXMEM1 | Broken up across stages |
| Cache stall | Combinational | Registered |
| Branch target | Computed in EXMEM1 | Pre-computed |
| Cache line hazard | N/A | 10-bit adder optimization |

### Why B32P3?

The redesign from B32P2 to B32P3 was motivated by several factors:

1. **Timing closure difficulties**: B32P2's critical path through the branch evaluation made timing closure at higher frequencies challenging.

2. **Complexity**: B32P2's 6-stage pipeline with multiple hazard types was complex to verify and debug.

3. **Limited benefit**: The 1-cycle branch penalty advantage of B32P2 was offset by the complexity and timing issues.

4. **Classic design patterns**: B32P3's 5-stage MIPS-style pipeline is a well-understood pattern with extensive documentation and tooling.

### Migration Notes

The ISA remains unchanged between B32P2 and B32P3. Software compiled for B32P2 runs on B32P3 without modification. Performance characteristics differ slightly due to the different pipeline depths and branch penalties.

---

## B32P (B32P1)

*Documentation for B32P1 to be added.*

The original B32P architecture was the first pipelined implementation in the FPGC project.
