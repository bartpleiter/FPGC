# RFC-001: Classic 5-Stage In-Order Pipeline

**Author**: Dr. Elena Vasquez (Pipeline Architecture Specialist)  
**Status**: Draft  
**Created**: 2025-01-15  
**Last Updated**: 2025-01-15  

## Abstract

This RFC proposes redesigning B32P2 as a classic 5-stage RISC pipeline (IF, ID, EX, MEM, WB) with clean stage separation, simplified hazard detection, and deterministic stalling. Multi-cycle operations are handled by stalling the entire pipeline, trading some CPI efficiency for dramatically simpler logic and higher clock speeds.

## 1. Motivation

The current B32P2 design attempts to maximize throughput by allowing variable-latency operations to complete asynchronously in EXMEM2 while forwarding results opportunistically. This creates exponential hazard complexity.

The classic MIPS-style 5-stage pipeline, refined over 40 years of academic and industrial use, provides:
- Well-understood hazard patterns (exactly 3 types)
- Simple forwarding logic (EX→EX, MEM→EX)
- Deterministic behavior
- Proven timing closure at high frequencies

## 2. Proposed Architecture

### 2.1 Pipeline Stages

```
┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐
│  IF  │───►│  ID  │───►│  EX  │───►│ MEM  │───►│  WB  │
└──────┘    └──────┘    └──────┘    └──────┘    └──────┘
   │           │           │           │           │
   ▼           ▼           ▼           ▼           ▼
  PC +      Decode +     ALU +     Memory      Register
 I-Cache    RegRead    Branch     Access      Writeback
```

| Stage | Operations | Outputs |
|-------|-----------|---------|
| IF | PC management, I-Cache access | Instruction word |
| ID | Decode, register read, hazard detect | Operands, control signals |
| EX | ALU, branch resolution, address calc | ALU result, branch target |
| MEM | D-Cache access, memory operations | Memory data |
| WB | Result mux, register write | Final result |

### 2.2 Multi-Cycle Operation Handling

**Key Design Decision**: All multi-cycle operations stall the entire pipeline.

```
┌─────────────────────────────────────────────────────────────┐
│                    Multi-Cycle Unit                          │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │ Divider  │  │Multiplier│  │  Cache   │  │    IO    │    │
│  │ (32 cyc) │  │ (3 cyc)  │  │Controller│  │  Access  │    │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘    │
│                         │                                    │
│                    ┌────▼────┐                              │
│                    │  Arbiter │                              │
│                    └────┬────┘                              │
│                         │                                    │
│                    ┌────▼────┐                              │
│                    │ Stall   │──────► stall_all             │
│                    │ Signal  │                              │
│                    └─────────┘                              │
└─────────────────────────────────────────────────────────────┘
```

When a multi-cycle operation begins:
1. Pipeline stalls completely (all stages hold)
2. Operation executes in dedicated unit
3. Result appears in MEM stage when done
4. Pipeline resumes

### 2.3 Hazard Detection (Simplified)

Only 3 hazard types exist:

#### Data Hazards
```verilog
// EX hazard: instruction in EX needs result from MEM stage
wire ex_hazard_a = (rs1_ID == rd_EX) && regwrite_EX && (rs1_ID != 0);
wire ex_hazard_b = (rs2_ID == rd_EX) && regwrite_EX && (rs2_ID != 0);

// MEM hazard: instruction in EX needs result from WB stage  
wire mem_hazard_a = (rs1_ID == rd_MEM) && regwrite_MEM && (rs1_ID != 0);
wire mem_hazard_b = (rs2_ID == rd_MEM) && regwrite_MEM && (rs2_ID != 0);

// Load-use hazard: must stall for 1 cycle
wire load_use_hazard = memread_EX && (ex_hazard_a || ex_hazard_b);
```

#### Control Hazards
```verilog
// Branch/jump resolved in EX - flush IF and ID
wire branch_taken;  // From branch unit in EX
wire flush_IF = branch_taken;
wire flush_ID = branch_taken;
```

#### Structural Hazards
```verilog
// Multi-cycle unit busy - stall everything
wire multicycle_busy;  // From multi-cycle unit
wire stall_all = multicycle_busy;
```

### 2.4 Forwarding Unit

Simple 2-source forwarding:

```verilog
module ForwardingUnit (
    input [3:0] rs1_EX,
    input [3:0] rs2_EX,
    input [3:0] rd_MEM,
    input [3:0] rd_WB,
    input       regwrite_MEM,
    input       regwrite_WB,
    
    output [1:0] forward_a,  // 00=reg, 01=MEM, 10=WB
    output [1:0] forward_b
);

assign forward_a = (regwrite_MEM && rd_MEM != 0 && rd_MEM == rs1_EX) ? 2'b01 :
                   (regwrite_WB  && rd_WB  != 0 && rd_WB  == rs1_EX) ? 2'b10 :
                   2'b00;

assign forward_b = (regwrite_MEM && rd_MEM != 0 && rd_MEM == rs2_EX) ? 2'b01 :
                   (regwrite_WB  && rd_WB  != 0 && rd_WB  == rs2_EX) ? 2'b10 :
                   2'b00;
                   
endmodule
```

### 2.5 Cache Integration

**Instruction Cache**:
- Accessed in IF stage
- Miss causes full pipeline stall
- Result available in 1 cycle on hit

**Data Cache**:
- Accessed in MEM stage
- Hit: result forwarded to WB normally
- Miss: stall pipeline, wait for cache fill

```
Cache Miss Timeline (example: 12-cycle SDRAM burst):

Cycle:  1   2   3   4   5   6   7   8   9  10  11  12  13  14
IF:    [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [I1]---
ID:    [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [S]  -  [D1]
EX:    [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [S]  -   - 
MEM:   [LD][S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [S][data] -
WB:     -  [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [S] [WB]

[S] = Stalled, [LD] = Load instruction waiting
```

## 3. Critical Path Analysis

### 3.1 Per-Stage Timing

| Stage | Operations | Estimated Delay |
|-------|-----------|-----------------|
| IF | PC mux + Cache BRAM read | 3ns |
| ID | Decode + RegFile read + Hazard detect | 4ns |
| EX | Mux + ALU + Branch compare | 5ns |
| MEM | Address mux + Cache BRAM read | 3ns |
| WB | Result mux + RegFile write setup | 2ns |

**Critical Path**: EX stage at ~5ns

### 3.2 Timing Optimization Techniques

1. **Register ALU inputs**: Forward mux registered at end of ID
2. **Early branch detection**: Partial decode in IF for branch prediction
3. **Cache tag comparison in parallel**: Tag check during read

**Target**: 8ns cycle time (125MHz), conservative 10ns (100MHz)

## 4. Performance Analysis

### 4.1 CPI Estimation

| Event | Frequency | Cycles | Contribution |
|-------|-----------|--------|--------------|
| Base execution | 100% | 1.0 | 1.00 |
| Load-use stall | 15% | 1.0 | 0.15 |
| Branch taken | 10% | 2.0 | 0.20 |
| D-Cache miss | 5% | 12.0 | 0.60 |
| I-Cache miss | 2% | 12.0 | 0.24 |
| Division | 1% | 32.0 | 0.32 |

**Estimated CPI**: ~2.5

### 4.2 Performance Comparison

| Design | Clock | CPI | MIPS |
|--------|-------|-----|------|
| Current B32P2 | 50MHz | 1.5 | 33.3 |
| Classic 5-stage | 100MHz | 2.5 | 40.0 |

**Improvement**: ~20% MIPS gain

### 4.3 Performance Trade-offs

**Loses**:
- No overlapping of multi-cycle operations
- Higher branch penalty (2 vs possibly 1)
- No instruction/data cache miss overlap

**Gains**:
- 2x clock frequency
- Simpler hazard logic
- Predictable performance

## 5. Implementation Details

### 5.1 Module Structure

```
B32P3_Classic/
├── B32P3.v                 # Top module
├── stages/
│   ├── IF_Stage.v          # Instruction fetch
│   ├── ID_Stage.v          # Decode and register read
│   ├── EX_Stage.v          # Execute
│   ├── MEM_Stage.v         # Memory access
│   └── WB_Stage.v          # Writeback
├── hazards/
│   ├── HazardUnit.v        # All hazard detection
│   └── ForwardingUnit.v    # Data forwarding
├── cache/
│   ├── ICache.v            # Instruction cache (simplified)
│   └── DCache.v            # Data cache (simplified)
└── multicycle/
    ├── MultiCycleUnit.v    # Arbiter for multi-cycle ops
    ├── Divider.v           # Integer division
    └── Multiplier.v        # Integer multiplication
```

### 5.2 Pipeline Registers

```verilog
// IF/ID Register
reg [31:0] IF_ID_pc;
reg [31:0] IF_ID_instr;
reg        IF_ID_valid;

// ID/EX Register
reg [31:0] ID_EX_pc;
reg [31:0] ID_EX_rs1_data;
reg [31:0] ID_EX_rs2_data;
reg [31:0] ID_EX_imm;
reg [3:0]  ID_EX_rs1;
reg [3:0]  ID_EX_rs2;
reg [3:0]  ID_EX_rd;
reg [3:0]  ID_EX_aluop;
// ... control signals

// EX/MEM Register
reg [31:0] EX_MEM_alu_result;
reg [31:0] EX_MEM_rs2_data;
reg [3:0]  EX_MEM_rd;
// ... control signals

// MEM/WB Register
reg [31:0] MEM_WB_result;
reg [3:0]  MEM_WB_rd;
// ... control signals
```

### 5.3 Stall Logic

```verilog
// Master stall signal
wire stall = load_use_hazard || icache_miss || dcache_miss || multicycle_busy;

// Stage control
wire IF_stall = stall;
wire ID_stall = stall;
wire EX_stall = stall && !load_use_hazard;  // On load-use, EX continues
wire MEM_stall = stall && !load_use_hazard;
wire WB_stall = stall && !load_use_hazard;

// Flush on branch
wire IF_flush = branch_taken && !stall;
wire ID_flush = branch_taken && !stall;
```

## 6. Risk Assessment

### 6.1 Low Risk
- **Architecture proven**: Classic MIPS pipeline is textbook material
- **Tools support**: Well-documented synthesis patterns
- **Testing**: Existing test suite directly applicable

### 6.2 Medium Risk
- **Performance uncertainty**: CPI estimates may vary with real workloads
- **Cache timing**: Integration with existing cache controller needs care

### 6.3 Mitigation Strategies

1. **Performance**: Add I-cache prefetching to reduce miss rate
2. **Cache**: Adapt existing CacheController with cleaner interface

## 7. Implementation Estimate

| Task | Time (days) | Dependencies |
|------|-------------|--------------|
| Pipeline core | 3 | None |
| Hazard/Forward units | 2 | Pipeline core |
| Cache integration | 2 | Pipeline core |
| Multi-cycle unit | 2 | Pipeline core |
| Testing/debug | 3 | All above |
| **Total** | **12 days** | |

## 8. Expert Feedback

### Dr. Marcus Chen (Timing Closure Expert)
> "The classic 5-stage is the safest choice for timing closure. My only concern is the EX stage timing with the forward mux + ALU chain. Consider registering forward mux outputs to break this path, accepting one extra cycle of load-use penalty."

**Response**: Good point. Will add option for registered forwarding in detailed design.

### Prof. Sarah Kim (FPGA Architecture)
> "For Cyclone IV, ensure the ALU doesn't use inferring multiplication - use explicit DSP blocks. Also, the 3ns IF stage estimate assumes single-cycle BRAM, which needs 10MHz+ headroom."

**Response**: Will use explicit DSP for multiply. BRAM timing is 2-3ns on Cyclone IV at our target size.

### Dr. James Liu (Compiler Optimization)
> "The 2-cycle branch penalty will hurt tight loops. Consider adding a simple 1-bit branch predictor (branch history table) to reduce this to 1 cycle for taken branches in loops."

**Response**: Branch prediction adds complexity. Will evaluate in v2 if performance is insufficient.

### Prof. Anna Schmidt (Cache Architecture)
> "Stalling the entire pipeline on cache miss is inefficient. Consider non-blocking loads with a small miss-status holding register (MSHR). But this adds complexity you're trying to avoid."

**Response**: Agreed this impacts performance. For v1, keeping simple stall model. Can revisit for v2.

### Dr. Michael Torres (Verification)
> "The simplified hazard model is excellent for verification. Three hazard types = exhaustive testing is feasible. I recommend adding formal assertions for pipeline invariants."

**Response**: Will include SystemVerilog assertions for simulation. Great suggestion.

## 9. Conclusion

The Classic 5-Stage Pipeline offers the safest path to 100MHz with dramatically reduced complexity. While CPI increases, the 2x clock speed more than compensates for typical workloads. This design serves as an excellent foundation that can be optimized incrementally.

**Recommendation**: Strong candidate for implementation. Low risk, moderate performance gain, excellent maintainability.

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2025-01-15 | E. Vasquez | Initial draft |
| 0.2 | 2025-01-15 | E. Vasquez | Added expert feedback |
