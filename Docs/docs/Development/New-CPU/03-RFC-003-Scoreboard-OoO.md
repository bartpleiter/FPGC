# RFC-003: Scoreboard-Based Dynamic Scheduling

**Author**: Prof. Sarah Kim (FPGA Architecture Specialist)  
**Status**: Draft  
**Created**: 2025-01-15  
**Last Updated**: 2025-01-15  

## Abstract

This RFC proposes implementing a scoreboard-based dynamic scheduling mechanism to handle variable-latency operations without complex static hazard detection. The scoreboard tracks register status and automatically stalls instructions when operands aren't ready, eliminating the need for explicit hazard logic for each instruction combination.

## 1. Motivation

The current B32P2 design uses explicit hazard detection with numerous special cases:
- Load-use hazards
- Multi-cycle ALU dependencies
- Cache miss handling
- Pop + WB forwarding conflicts

Each new instruction type or latency variation requires updating hazard logic. A scoreboard-based approach inverts this: instead of detecting hazards, we track when results are ready and stall automatically.

**Historical Context**: CDC 6600 (1964) introduced scoreboards for out-of-order execution. We use a simplified in-order variant suitable for FPGA implementation.

## 2. Proposed Architecture

### 2.1 Core Concept

The scoreboard maintains status for each register:

```
Register Scoreboard (16 entries for r0-r15):
┌─────┬─────────┬───────────┬───────────┐
│ Reg │ Status  │ Producer  │ Ready Cyc │
├─────┼─────────┼───────────┼───────────┤
│ r0  │ READY   │ -         │ -         │  (always ready)
│ r1  │ PENDING │ DIV unit  │ cycle+30  │
│ r2  │ READY   │ -         │ -         │
│ r3  │ PENDING │ MEM stage │ cycle+12  │
│ ...                                    │
└─────┴─────────┴───────────┴───────────┘
```

**Rules**:
1. When instruction issues, mark destination register as PENDING
2. When result completes, mark register as READY
3. Instruction can only issue if all source registers are READY
4. No explicit hazard detection needed!

### 2.2 Pipeline Structure

```
┌──────┐    ┌──────┐    ┌──────┐    ┌──────────┐    ┌──────┐
│  IF  │───►│  ID  │───►│ISSUE │───►│ EXECUTE  │───►│  WB  │
└──────┘    └──────┘    └──────┘    └──────────┘    └──────┘
                            │              │            │
                            ▼              │            │
                    ┌──────────────┐       │            │
                    │  Scoreboard  │◄──────┴────────────┘
                    │   (16 regs)  │
                    └──────────────┘
```

### 2.3 Scoreboard Implementation

```verilog
module Scoreboard (
    input  wire        clk,
    input  wire        reset,
    
    // Issue stage query
    input  wire [3:0]  rs1,
    input  wire [3:0]  rs2,
    output wire        rs1_ready,
    output wire        rs2_ready,
    
    // Issue stage reservation
    input  wire [3:0]  rd_issue,
    input  wire        issue_valid,
    input  wire [5:0]  latency,        // Cycles until result ready
    input  wire [2:0]  producer_unit,  // Which unit produces result
    
    // Writeback notification
    input  wire [3:0]  rd_wb,
    input  wire        wb_valid
);

// Per-register state
reg [15:0] reg_pending;     // Bit per register: 1=pending, 0=ready
reg [5:0]  reg_ready_cycle [0:15];  // Cycle when result ready
reg [2:0]  reg_producer [0:15];     // Which unit produces

// Current cycle counter
reg [31:0] cycle_count;

// Query logic - register is ready if not pending or cycle has passed
assign rs1_ready = (rs1 == 4'd0) || 
                   (!reg_pending[rs1]) || 
                   (cycle_count >= reg_ready_cycle[rs1]);
assign rs2_ready = (rs2 == 4'd0) || 
                   (!reg_pending[rs2]) || 
                   (cycle_count >= reg_ready_cycle[rs2]);

// Issue - mark destination as pending
always @(posedge clk) begin
    if (reset) begin
        reg_pending <= 16'b0;
        cycle_count <= 0;
    end else begin
        cycle_count <= cycle_count + 1;
        
        // On issue, mark destination pending
        if (issue_valid && rd_issue != 4'd0) begin
            reg_pending[rd_issue] <= 1'b1;
            reg_ready_cycle[rd_issue] <= cycle_count + latency;
            reg_producer[rd_issue] <= producer_unit;
        end
        
        // On writeback, mark destination ready
        if (wb_valid && rd_wb != 4'd0) begin
            reg_pending[rd_wb] <= 1'b0;
        end
    end
end

endmodule
```

### 2.4 Issue Stage Logic

```verilog
module IssueStage (
    input  wire        clk,
    input  wire        reset,
    
    // From decode
    input  wire [31:0] instr,
    input  wire [3:0]  rs1,
    input  wire [3:0]  rs2,
    input  wire [3:0]  rd,
    input  wire        instr_valid,
    
    // Scoreboard interface
    input  wire        rs1_ready,
    input  wire        rs2_ready,
    output reg  [3:0]  sb_rd,
    output reg         sb_reserve,
    output reg  [5:0]  sb_latency,
    
    // To execute
    output reg  [31:0] issue_instr,
    output reg         issue_valid,
    
    // Stall signal
    output wire        stall
);

// Stall if any source operand not ready
wire operands_ready = rs1_ready && rs2_ready;
assign stall = instr_valid && !operands_ready;

// Latency lookup based on instruction type
wire [5:0] instr_latency;
LatencyTable latency_lut (
    .opcode(instr[31:28]),
    .aluop(instr[27:24]),
    .latency(instr_latency)
);

always @(posedge clk) begin
    if (reset || stall) begin
        issue_valid <= 1'b0;
        sb_reserve <= 1'b0;
    end else if (instr_valid && operands_ready) begin
        // Issue instruction
        issue_instr <= instr;
        issue_valid <= 1'b1;
        
        // Reserve destination in scoreboard
        if (rd != 4'd0) begin
            sb_rd <= rd;
            sb_latency <= instr_latency;
            sb_reserve <= 1'b1;
        end else begin
            sb_reserve <= 1'b0;
        end
    end else begin
        issue_valid <= 1'b0;
        sb_reserve <= 1'b0;
    end
end

endmodule
```

### 2.5 Latency Table

All instruction latencies in one place:

```verilog
module LatencyTable (
    input  wire [3:0]  opcode,
    input  wire [3:0]  aluop,
    output reg  [5:0]  latency
);

// Instruction opcodes
localparam OP_ARITH  = 4'b0000;
localparam OP_ARITHC = 4'b0001;
localparam OP_ARITHM = 4'b0010;  // Multi-cycle
localparam OP_READ   = 4'b1110;
localparam OP_POP    = 4'b1010;
// ... etc

// Multi-cycle ALU operations
localparam ALU_DIV   = 4'b0011;
localparam ALU_DIVU  = 4'b0100;
localparam ALU_MUL   = 4'b0000;

always @(*) begin
    case (opcode)
        OP_ARITH, OP_ARITHC: latency = 6'd1;  // Single-cycle ALU
        
        OP_ARITHM: begin  // Multi-cycle ALU
            case (aluop)
                ALU_MUL:  latency = 6'd3;
                ALU_DIV:  latency = 6'd33;
                ALU_DIVU: latency = 6'd33;
                default:  latency = 6'd1;
            endcase
        end
        
        OP_READ: latency = 6'd2;  // Assume cache hit (miss handled separately)
        OP_POP:  latency = 6'd2;  // Stack access
        
        default: latency = 6'd1;
    endcase
end

endmodule
```

### 2.6 Execute Unit Design

Multiple functional units with different latencies:

```
┌─────────────────────────────────────────────────────────────┐
│                     Execute Stage                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐                │
│   │   ALU   │    │   MEM   │    │ Multi   │                │
│   │ (1 cyc) │    │ (1-2cyc)│    │ (3-33)  │                │
│   └────┬────┘    └────┬────┘    └────┬────┘                │
│        │              │              │                      │
│        └──────────────┴──────────────┘                      │
│                        │                                     │
│                   ┌────▼────┐                               │
│                   │ Result  │                               │
│                   │  Mux    │                               │
│                   └────┬────┘                               │
│                        │                                     │
└────────────────────────┼────────────────────────────────────┘
                         │
                         ▼ to Writeback
```

### 2.7 Cache Miss Handling

Cache misses are handled by updating the scoreboard:

```verilog
// In Memory stage
always @(posedge clk) begin
    if (mem_read && !dcache_hit) begin
        // Cache miss detected - update scoreboard with actual latency
        sb_update_rd <= rd_MEM;
        sb_update_latency <= CACHE_MISS_LATENCY;  // e.g., 12 cycles
        sb_update_valid <= 1'b1;
    end
end
```

## 3. Critical Path Analysis

### 3.1 Scoreboard Lookup

```
Issue Stage Critical Path:
┌───────────┐    ┌───────────┐    ┌───────────┐
│ rs1/rs2   │───►│Scoreboard │───►│ Issue     │
│ from      │    │  Lookup   │    │ Decision  │
│ Decode    │    │ (16:1 mux)│    │           │
└───────────┘    └───────────┘    └───────────┘
      1ns             3ns              1ns
                                Total: ~5ns
```

### 3.2 Per-Stage Timing

| Stage | Operations | Estimated Delay |
|-------|-----------|-----------------|
| IF | PC + I-Cache | 3ns |
| ID | Decode + RegRead | 4ns |
| Issue | Scoreboard lookup + stall logic | 5ns |
| Execute | ALU / MEM access | 4ns |
| WB | Result mux + RegWrite | 2ns |

**Critical Path**: Issue stage at ~5ns (scoreboard lookup)

### 3.3 Timing Optimization

Pipelining scoreboard lookup:
```verilog
// Register scoreboard outputs
reg rs1_ready_reg, rs2_ready_reg;

always @(posedge clk) begin
    rs1_ready_reg <= scoreboard.rs1_ready;
    rs2_ready_reg <= scoreboard.rs2_ready;
end

// Use registered values for issue decision (1 cycle latency)
assign can_issue = rs1_ready_reg && rs2_ready_reg;
```

This adds 1 cycle to effective latency but breaks critical path.

## 4. Performance Analysis

### 4.1 Scoreboard Overhead

The scoreboard adds issue-stage stalls when operands not ready:

| Scenario | Classic Stall | Scoreboard Stall |
|----------|--------------|------------------|
| ALU→ALU | 0 (forward) | 0 (ready) |
| Load→Use | 1 cycle | 1 cycle |
| Div→Use | Many (flush) | Automatic wait |
| Cache Miss→Use | Complex | Automatic wait |

**Key Insight**: Scoreboard handles all cases uniformly without special logic.

### 4.2 CPI Analysis

| Event | Frequency | Cycles | Contribution |
|-------|-----------|--------|--------------|
| Base execution | 100% | 1.0 | 1.00 |
| Scoreboard stall (ALU) | 5% | 0.0 | 0.00* |
| Scoreboard stall (load) | 15% | 1.0 | 0.15 |
| Branch penalty | 10% | 2.0 | 0.20 |
| D-Cache miss wait | 5% | 12.0 | 0.60 |
| I-Cache miss | 2% | 12.0 | 0.24 |
| Division wait | 1% | 32.0 | 0.32 |

*With proper forwarding integration

**Estimated CPI**: ~2.5 (similar to classic)

### 4.3 Performance Comparison

| Design | Clock | CPI | MIPS | Complexity |
|--------|-------|-----|------|------------|
| Current B32P2 | 50MHz | 1.5 | 33.3 | Very High |
| Classic 5-stage | 100MHz | 2.5 | 40.0 | Low |
| Scoreboard | 100MHz | 2.5 | 40.0 | Medium |

**Main Benefit**: Equivalent performance with better extensibility.

## 5. Advantages of Scoreboard Approach

### 5.1 Simplicity of Extension

Adding new instruction types:
1. Update LatencyTable with new latency
2. Done!

No hazard logic changes needed.

### 5.2 Variable Latency Operations

Cache misses, IO operations, etc. handled naturally:
1. Issue instruction with optimistic latency
2. If operation takes longer, update scoreboard
3. Dependent instructions wait automatically

### 5.3 Debug Support

Stopping the pipeline for debug:
1. Stop issuing new instructions
2. Wait for all pending operations to complete (scoreboard shows this)
3. All registers now READY
4. Safe to inspect state

### 5.4 Maintainability

Hazard detection code reduction:
```
Current B32P2:
- hazard_load_use
- hazard_pop_wb_conflict  
- hazard_multicycle_dep
- forward_a / forward_b logic
- 6+ complex conditional assignments

Scoreboard:
- rs1_ready && rs2_ready
- That's it.
```

## 6. Implementation Details

### 6.1 Module Structure

```
B32P3_Scoreboard/
├── B32P3.v                 # Top module
├── scoreboard/
│   ├── Scoreboard.v        # Main scoreboard module
│   └── LatencyTable.v      # Latency lookup
├── stages/
│   ├── FetchStage.v
│   ├── DecodeStage.v
│   ├── IssueStage.v        # New: scoreboard integration
│   ├── ExecuteStage.v
│   └── WritebackStage.v
├── execute/
│   ├── ALU.v
│   ├── MemUnit.v
│   └── MultiCycleUnit.v
└── cache/
    ├── ICache.v
    └── DCache.v
```

### 6.2 Integration with Forwarding

Scoreboard alone doesn't forward - we add forwarding for single-cycle gains:

```verilog
// Hybrid approach: scoreboard + forwarding
wire forward_from_ex = (rs1 == rd_EX) && regwrite_EX && (rs1 != 0);
wire forward_from_wb = (rs1 == rd_WB) && regwrite_WB && (rs1 != 0);

// Scoreboard only checks for multi-cycle
wire wait_for_multicycle = !scoreboard_ready && !forward_available;

// Final data selection
assign operand1 = forward_from_ex ? result_EX :
                  forward_from_wb ? result_WB :
                  regfile_rs1;
```

### 6.3 Cache Miss Update Protocol

```verilog
// When cache miss detected
always @(posedge clk) begin
    case (mem_state)
        MEM_NORMAL: begin
            if (mem_read && !dcache_hit) begin
                // Update scoreboard with miss latency
                scoreboard_update_reg <= rd_MEM;
                scoreboard_update_cycles <= MISS_LATENCY;
                scoreboard_update_valid <= 1'b1;
                mem_state <= MEM_MISS_WAIT;
            end
        end
        
        MEM_MISS_WAIT: begin
            scoreboard_update_valid <= 1'b0;
            if (dcache_done) begin
                // Result ready, scoreboard auto-updates on WB
                mem_state <= MEM_NORMAL;
            end
        end
    endcase
end
```

## 7. Risk Assessment

### 7.1 Low Risk
- **Scoreboard logic**: Simple, well-understood design pattern
- **Latency table**: Easy to maintain and test
- **Extensibility**: Primary goal achieved

### 7.2 Medium Risk
- **Timing closure**: Scoreboard lookup may need pipelining
- **Forwarding integration**: Hybrid approach adds some complexity
- **Cache miss handling**: Dynamic latency update needs care

### 7.3 High Risk
- **Performance parity**: Must match classic pipeline CPI
- **Learning curve**: Scoreboard is different paradigm

### 7.4 Mitigation

1. **Timing**: Pre-registered scoreboard lookup
2. **Forwarding**: Start with scoreboard-only, add forwarding incrementally
3. **Performance**: Detailed simulation before implementation
4. **Learning**: Extensive documentation and examples

## 8. Implementation Estimate

| Task | Time (days) | Dependencies |
|------|-------------|--------------|
| Scoreboard module | 2 | None |
| Latency table | 1 | None |
| Issue stage | 2 | Scoreboard |
| Pipeline integration | 3 | Issue stage |
| Forwarding hybrid | 2 | Pipeline |
| Cache miss handling | 2 | Pipeline |
| Testing/debug | 4 | All above |
| **Total** | **16 days** | |

## 9. Expert Feedback

### Dr. Elena Vasquez (Pipeline Architecture)
> "The scoreboard approach is elegant and scales well. However, the 5ns scoreboard lookup is concerning for 100MHz. I'd recommend the pipelined lookup even if it adds a cycle of latency."

**Response**: Agreed. Will implement registered scoreboard outputs. Extra cycle is acceptable given complexity reduction.

### Dr. Marcus Chen (Timing Closure)
> "The scoreboard as a central resource creates a potential bottleneck. Consider duplicating scoreboard state for parallel lookup of rs1 and rs2."

**Response**: Good point. Will use dual-port register file style for scoreboard (two read ports).

### Dr. James Liu (Compiler Optimization)
> "The LatencyTable is powerful for compiler optimization. If exposed, the compiler could schedule instructions to avoid stalls. Consider documenting latencies in ISA spec."

**Response**: Excellent idea. Will document latencies and consider compiler hints for scheduling.

### Prof. Anna Schmidt (Cache Architecture)
> "The dynamic latency update for cache misses is the trickiest part. Need to handle the case where scoreboard says ready but cache miss hasn't completed. Suggest conservative initial latency."

**Response**: Will use conservative cache access latency (assume miss) and update to actual on hit for early release.

### Dr. Michael Torres (Verification)
> "The scoreboard state is excellent for verification - it's a single source of truth for register availability. Add assertions that WB always clears pending bit, and that no double-pending occurs."

**Response**: Will add comprehensive assertions. Scoreboard invariants are easy to specify formally.

## 10. Conclusion

The Scoreboard-Based architecture offers a fundamentally different approach to hazard handling. Instead of explicit detection, we track register availability and let the hardware wait automatically. This dramatically simplifies adding new instruction types and variable-latency operations.

While performance is similar to the classic pipeline, the real win is in maintainability and extensibility - exactly what the project needs for features like debugging support.

**Recommendation**: Strong candidate for implementation. The scoreboard paradigm aligns well with the project's need for simpler hazard management. Consider as alternative to classic pipeline.

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2025-01-15 | S. Kim | Initial draft |
| 0.2 | 2025-01-15 | S. Kim | Added expert feedback, timing analysis |
