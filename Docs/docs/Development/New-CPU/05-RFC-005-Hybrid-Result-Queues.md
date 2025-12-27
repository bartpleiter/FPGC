# RFC-005: Hybrid Pipeline with Completion Queues

**Author**: Dr. Michael Torres (Verification Specialist) & Prof. Anna Schmidt (Cache Architecture)  
**Status**: Draft  
**Created**: 2025-01-15  
**Last Updated**: 2025-01-15  

## Abstract

This RFC proposes a hybrid architecture that combines the simplicity of an in-order pipeline with completion queues for handling variable-latency operations. Instructions issue in-order but complete out-of-order through dedicated completion queues, which naturally track when results become available. This design provides the benefits of the scoreboard approach with better modularity and timing characteristics.

## 1. Motivation

The previous RFCs each have trade-offs:

| RFC | Strength | Weakness |
|-----|----------|----------|
| Classic | Simple | Full stall on multi-cycle |
| Decoupled | I-cache hiding | Modest gains, FIFO complexity |
| Scoreboard | Unified tracking | Central bottleneck, timing |
| Stall-Free | Simple hardware | Compiler complexity, NOPs |

This RFC attempts to combine strengths:
- **In-order issue** (like Classic): Simple front-end
- **Out-of-order completion** (like Scoreboard): No full stalls
- **Distributed tracking** (improvement): No central bottleneck
- **Hardware forwarding** (like Classic): Good single-cycle CPI

## 2. Proposed Architecture

### 2.1 High-Level Structure

```
┌──────────────────────────────────────────────────────────────────────┐
│                         IN-ORDER FRONT-END                            │
│  ┌──────┐    ┌──────┐    ┌──────┐    ┌─────────────────────────────┐ │
│  │  IF  │───►│  ID  │───►│ Issue │───►│  Dispatch to Queues        │ │
│  └──────┘    └──────┘    └──────┘    └─────────────────────────────┘ │
└──────────────────────────────────────────────┬───────────────────────┘
                                               │
              ┌────────────────────────────────┼────────────────────────┐
              │                                │                        │
              ▼                                ▼                        ▼
    ┌─────────────────┐              ┌─────────────────┐      ┌─────────────────┐
    │   ALU Queue     │              │   MEM Queue     │      │ Multi-Cyc Queue │
    │   (1-cycle)     │              │   (1-12 cyc)    │      │ (3-33 cyc)      │
    └────────┬────────┘              └────────┬────────┘      └────────┬────────┘
             │                                │                        │
             └────────────────────────────────┼────────────────────────┘
                                              │
                                              ▼
                                    ┌─────────────────┐
                                    │  Completion     │
                                    │  Arbiter        │
                                    └────────┬────────┘
                                              │
                                              ▼
                                    ┌─────────────────┐
                                    │  Writeback      │
                                    └─────────────────┘
```

### 2.2 Queue-Based Execution

Each functional unit has its own completion queue:

```verilog
module CompletionQueue #(
    parameter DEPTH = 4,
    parameter WIDTH = 40  // {valid, rd, result}
) (
    input  wire        clk,
    input  wire        reset,
    
    // Issue interface (in-order entry)
    input  wire        issue_valid,
    input  wire [3:0]  issue_rd,
    output wire        issue_ready,   // Can accept new entry
    
    // Result interface (may be out-of-order)
    input  wire        result_valid,
    input  wire [31:0] result_data,
    
    // Completion interface (in-order completion)
    output wire        complete_valid,
    output wire [3:0]  complete_rd,
    output wire [31:0] complete_data,
    input  wire        complete_ack,
    
    // Forwarding interface
    input  wire [3:0]  forward_rs,
    output wire        forward_hit,
    output wire [31:0] forward_data
);
```

**Key Insight**: Each queue tracks its own outstanding operations. No central scoreboard needed.

### 2.3 Queue Types

#### ALU Queue (Simple FIFO)
- Single-cycle operations
- 2-entry FIFO (for timing margin)
- Results immediately available

```verilog
module ALU_Queue (
    // ... ports ...
);

// Simple 2-entry FIFO since ALU is single-cycle
reg [35:0] entries [0:1];  // {valid, rd, result}
reg write_ptr, read_ptr;

// ALU executes immediately
always @(posedge clk) begin
    if (issue_valid && issue_ready) begin
        entries[write_ptr] <= {1'b1, issue_rd, alu_result};
        write_ptr <= ~write_ptr;
    end
end

endmodule
```

#### Memory Queue (Variable Latency)
- Handles cache hits (1 cycle) and misses (10-12 cycles)
- 4-entry queue for overlapping cache misses

```verilog
module MEM_Queue (
    // ... ports ...
);

// 4 entries to handle multiple outstanding loads
reg [67:0] entries [0:3];  // {valid, waiting, rd, addr, result}
reg [1:0] issue_ptr, complete_ptr;

// Track multiple outstanding cache operations
always @(posedge clk) begin
    // Issue: allocate entry, start memory access
    if (issue_valid && issue_ready) begin
        entries[issue_ptr] <= {1'b1, 1'b1, issue_rd, issue_addr, 32'b0};
        issue_ptr <= issue_ptr + 1;
    end
    
    // Result: mark entry complete when cache returns
    for (int i = 0; i < 4; i++) begin
        if (entries[i][66] && cache_done[i]) begin  // waiting && done
            entries[i][66] <= 1'b0;  // Clear waiting
            entries[i][31:0] <= cache_result[i];
        end
    end
end

endmodule
```

#### Multi-Cycle Queue (Long Latency)
- Division (32 cycles), multiplication (3 cycles)
- 2-entry queue (these ops are rare)

```verilog
module MultiCycle_Queue (
    // ... ports ...
);

// Only 2 entries - multi-cycle ops are rare
reg [35:0] entries [0:1];  // {valid, rd, result}
reg [1:0] state;

// State machine for division/multiplication
always @(posedge clk) begin
    case (state)
        IDLE: if (issue_valid) begin
            start_operation(issue_opcode, issue_a, issue_b);
            entries[issue_ptr] <= {1'b1, issue_rd, 32'b0};
            state <= EXECUTING;
        end
        EXECUTING: if (operation_done) begin
            entries[issue_ptr][31:0] <= operation_result;
            state <= IDLE;
        end
    endcase
end

endmodule
```

### 2.4 Forwarding from Queues

Each queue provides forwarding ports:

```verilog
// In Issue stage
wire [31:0] rs1_data;
wire [31:0] rs2_data;

// Check all queues for forwarding
wire alu_fwd_hit_rs1, mem_fwd_hit_rs1, mc_fwd_hit_rs1;
wire [31:0] alu_fwd_data_rs1, mem_fwd_data_rs1, mc_fwd_data_rs1;

ALU_Queue.forward_check(rs1, alu_fwd_hit_rs1, alu_fwd_data_rs1);
MEM_Queue.forward_check(rs1, mem_fwd_hit_rs1, mem_fwd_data_rs1);
MultiCycle_Queue.forward_check(rs1, mc_fwd_hit_rs1, mc_fwd_data_rs1);

// Priority: most recent producer wins
assign rs1_data = alu_fwd_hit_rs1 ? alu_fwd_data_rs1 :
                  mem_fwd_hit_rs1 ? mem_fwd_data_rs1 :
                  mc_fwd_hit_rs1  ? mc_fwd_data_rs1 :
                  regfile_rs1;
```

### 2.5 Issue Stage Logic

```verilog
module IssueStage (
    input  wire        clk,
    input  wire        reset,
    
    // From decode
    input  wire [31:0] instr,
    input  wire [3:0]  rs1, rs2, rd,
    
    // Queue status
    input  wire        alu_queue_ready,
    input  wire        mem_queue_ready,
    input  wire        mc_queue_ready,
    
    // Queue forwarding
    input  wire        rs1_available,  // From queue forward check
    input  wire        rs2_available,
    
    // Issue control
    output reg  [2:0]  issue_target,   // 001=ALU, 010=MEM, 100=MC
    output reg         issue_valid
);

// Determine target queue
wire is_alu_op = (opcode inside {OP_ARITH, OP_ARITHC});
wire is_mem_op = (opcode inside {OP_READ, OP_WRITE});
wire is_mc_op  = (opcode == OP_ARITHM);

// Check if can issue
wire target_queue_ready = 
    (is_alu_op && alu_queue_ready) ||
    (is_mem_op && mem_queue_ready) ||
    (is_mc_op  && mc_queue_ready);

wire operands_ready = rs1_available && rs2_available;

wire can_issue = target_queue_ready && operands_ready;

// Issue decision
always @(posedge clk) begin
    if (reset) begin
        issue_valid <= 1'b0;
    end else if (can_issue) begin
        issue_valid <= 1'b1;
        issue_target <= {is_mc_op, is_mem_op, is_alu_op};
    end else begin
        issue_valid <= 1'b0;
    end
end

endmodule
```

### 2.6 Completion Arbiter

Manages writeback from multiple queues:

```verilog
module CompletionArbiter (
    input  wire        clk,
    input  wire        reset,
    
    // From queues
    input  wire        alu_complete_valid,
    input  wire [3:0]  alu_complete_rd,
    input  wire [31:0] alu_complete_data,
    
    input  wire        mem_complete_valid,
    input  wire [3:0]  mem_complete_rd,
    input  wire [31:0] mem_complete_data,
    
    input  wire        mc_complete_valid,
    input  wire [3:0]  mc_complete_rd,
    input  wire [31:0] mc_complete_data,
    
    // To writeback
    output reg         wb_valid,
    output reg  [3:0]  wb_rd,
    output reg  [31:0] wb_data,
    
    // Ack to queues
    output wire        alu_ack,
    output wire        mem_ack,
    output wire        mc_ack
);

// Round-robin priority for fairness
reg [1:0] priority;

always @(posedge clk) begin
    if (reset) begin
        wb_valid <= 0;
        priority <= 0;
    end else begin
        // Select based on priority and availability
        case (priority)
            2'd0: begin  // ALU first
                if (alu_complete_valid) begin
                    wb_valid <= 1; wb_rd <= alu_complete_rd; wb_data <= alu_complete_data;
                    alu_ack <= 1; priority <= 2'd1;
                end else if (mem_complete_valid) begin
                    // ... MEM
                end
                // ...
            end
            // ... other priorities
        endcase
    end
end

endmodule
```

## 3. Hazard Handling

### 3.1 Structural Hazards

Handled by queue full signals:
```verilog
wire structural_hazard = !target_queue_ready;
wire stall_issue = structural_hazard;
```

### 3.2 Data Hazards (RAW)

Handled by forwarding from queues:
```verilog
// Each queue provides forwarding for its in-flight operations
wire data_hazard = !rs1_available || !rs2_available;
wire stall_issue = data_hazard;
```

### 3.3 Data Hazards (WAW)

In-order issue prevents WAW:
```verilog
// Instructions issue in order, so same rd won't be issued twice
// until first completes (queue tracks this)
```

### 3.4 Control Hazards

Branch handling:
```verilog
// On branch misprediction:
// 1. Flush issue stage
// 2. Drain all queues (wait for completions)
// 3. Resume at correct address

always @(posedge clk) begin
    if (branch_mispredict) begin
        flush_issue <= 1;
        drain_queues <= 1;
    end else if (all_queues_empty) begin
        drain_queues <= 0;
        pc <= branch_target;
    end
end
```

## 4. Critical Path Analysis

### 4.1 Issue Stage Timing

```
┌───────────┐    ┌───────────┐    ┌───────────┐    ┌───────────┐
│  Decode   │───►│  Forward  │───►│  Ready    │───►│   Issue   │
│  Output   │    │   Check   │    │   Logic   │    │  Decision │
└───────────┘    └───────────┘    └───────────┘    └───────────┘
    (reg)          2-3ns            1ns               1ns
                                            Total: ~4-5ns
```

### 4.2 Forward Check Optimization

Parallel forward checks:
```verilog
// All checks happen in parallel
assign rs1_in_alu = (rs1 == alu_entry0_rd) || (rs1 == alu_entry1_rd);
assign rs1_in_mem = (rs1 == mem_entry0_rd) || ... ;
assign rs1_in_mc  = (rs1 == mc_entry0_rd) || (rs1 == mc_entry1_rd);

// OR together for final availability
assign rs1_available = !rs1_in_alu && !rs1_in_mem && !rs1_in_mc && 
                       !rs1_waiting_alu && !rs1_waiting_mem && !rs1_waiting_mc;
```

### 4.3 Queue Timing

Each queue is independent:
- ALU Queue: 2ns (simple FIFO)
- MEM Queue: 3ns (cache interface)
- MC Queue: 2ns (start/done interface)

### 4.4 Target: 100MHz Achievable

| Path | Delay | Notes |
|------|-------|-------|
| Issue decision | 5ns | Parallel forward checks |
| ALU execution | 4ns | Single-cycle |
| Completion arbitration | 2ns | Simple priority |
| Writeback | 2ns | Register write |

**Conservative estimate**: 8ns critical path → 125MHz capable

## 5. Performance Analysis

### 5.1 Overlapped Execution

Key benefit: different instruction types execute in parallel:

```
Time:     1   2   3   4   5   6   7   8   9  10  11  12  13
ADD r1    [Issue][ALU][WB]
LOAD r2   [Issue][ Cache...hit ][WB]
MUL r3    [Issue][ Multiply... ][WB]
ADD r4        [Issue][ALU][WB]
SUB r5            [Issue][ALU][WB]

All 5 instructions complete by cycle 13 (vs 5+2+3+1+1=12 sequential)
Actually better if cache hit!
```

### 5.2 CPI Analysis

| Event | Frequency | Cycles | Classic | Hybrid |
|-------|-----------|--------|---------|--------|
| ALU→ALU | 40% | 1 | 1.0 | 1.0 |
| Load→Use (hit) | 12% | 2 | 0.12 | 0.12* |
| Load→Use (miss) | 3% | 12 | 0.36 | 0.18** |
| Multi-cycle→Use | 5% | varies | 0.40 | 0.20** |
| Branch penalty | 10% | 2 | 0.20 | 0.20 |
| Queue full stall | 2% | 1 | 0 | 0.02 |

*Can often forward from MEM queue
**Other instructions execute during wait

**Estimated CPI**: ~1.7 (vs 2.0 classic)

### 5.3 Performance Comparison

| Design | Clock | CPI | MIPS |
|--------|-------|-----|------|
| Current B32P2 | 50MHz | 1.5 | 33.3 |
| Classic 5-stage | 100MHz | 2.0 | 50.0 |
| Hybrid Queues | 100MHz | 1.7 | 58.8 |

**Improvement vs Current**: ~76% MIPS gain

## 6. Implementation Details

### 6.1 Module Structure

```
B32P3_Hybrid/
├── B32P3.v                 # Top module
├── frontend/
│   ├── FetchStage.v
│   ├── DecodeStage.v
│   └── IssueStage.v        # New: dispatch to queues
├── queues/
│   ├── ALU_Queue.v         # 2-entry, single-cycle
│   ├── MEM_Queue.v         # 4-entry, variable latency
│   └── MultiCycle_Queue.v  # 2-entry, long latency
├── execute/
│   ├── ALU.v
│   ├── MemoryUnit.v
│   └── MultiCycleUnit.v
├── backend/
│   ├── CompletionArbiter.v
│   └── WritebackStage.v
├── hazards/
│   └── ForwardingNetwork.v # Distributed in queues
└── cache/
    ├── ICache.v
    └── DCache.v
```

### 6.2 Memory Queue Detail

```verilog
module MEM_Queue #(
    parameter DEPTH = 4
) (
    input  wire        clk,
    input  wire        reset,
    
    // Issue interface
    input  wire        issue_valid,
    input  wire [3:0]  issue_rd,
    input  wire [31:0] issue_addr,
    input  wire        issue_is_load,
    output wire        issue_ready,
    
    // Cache interface
    output reg  [31:0] cache_addr,
    output reg         cache_request,
    output reg         cache_we,
    input  wire [31:0] cache_data,
    input  wire        cache_hit,
    input  wire        cache_done,
    
    // Completion interface
    output wire        complete_valid,
    output wire [3:0]  complete_rd,
    output wire [31:0] complete_data,
    input  wire        complete_ack,
    
    // Forwarding
    input  wire [3:0]  fwd_rs,
    output wire        fwd_hit,
    output wire [31:0] fwd_data
);

// Queue entries
typedef struct {
    logic        valid;
    logic        waiting;    // Waiting for cache
    logic        ready;      // Result available
    logic [3:0]  rd;
    logic [31:0] addr;
    logic [31:0] data;
} queue_entry_t;

queue_entry_t entries [DEPTH];
reg [$clog2(DEPTH)-1:0] issue_ptr, complete_ptr;

// Issue ready if queue not full
assign issue_ready = !entries[issue_ptr].valid;

// Completion ready if head has result
assign complete_valid = entries[complete_ptr].ready;
assign complete_rd = entries[complete_ptr].rd;
assign complete_data = entries[complete_ptr].data;

// Forwarding: check all entries
always_comb begin
    fwd_hit = 0;
    fwd_data = 0;
    for (int i = 0; i < DEPTH; i++) begin
        if (entries[i].valid && entries[i].rd == fwd_rs) begin
            fwd_hit = entries[i].ready;  // Only hit if result ready
            fwd_data = entries[i].data;
        end
    end
end

// State machine per entry (simplified)
always @(posedge clk) begin
    if (reset) begin
        for (int i = 0; i < DEPTH; i++) entries[i] <= '0;
        issue_ptr <= 0;
        complete_ptr <= 0;
    end else begin
        // Issue: allocate entry
        if (issue_valid && issue_ready) begin
            entries[issue_ptr].valid <= 1;
            entries[issue_ptr].waiting <= 1;
            entries[issue_ptr].ready <= 0;
            entries[issue_ptr].rd <= issue_rd;
            entries[issue_ptr].addr <= issue_addr;
            issue_ptr <= issue_ptr + 1;
            
            // Start cache access
            cache_addr <= issue_addr;
            cache_request <= 1;
        end
        
        // Cache response
        if (cache_done) begin
            // Find waiting entry and mark ready
            // (simplified - real impl needs tag matching)
            entries[waiting_entry].data <= cache_data;
            entries[waiting_entry].waiting <= 0;
            entries[waiting_entry].ready <= 1;
        end
        
        // Completion: dequeue
        if (complete_valid && complete_ack) begin
            entries[complete_ptr] <= '0;
            complete_ptr <= complete_ptr + 1;
        end
    end
end

endmodule
```

### 6.3 Branch Handling

```verilog
// Branch resolution in issue stage (early branch)
wire branch_taken = is_branch && branch_condition_met;
wire branch_mispredict = branch_taken;  // Assume not-taken prediction

// On mispredict
always @(posedge clk) begin
    if (branch_mispredict) begin
        // Flush frontend
        flush_fetch <= 1;
        flush_decode <= 1;
        
        // Don't issue new instructions
        drain_mode <= 1;
    end
    
    if (drain_mode && all_queues_empty) begin
        // Safe to redirect
        drain_mode <= 0;
        pc <= branch_target;
    end
end
```

## 7. Debug Support

### 7.1 Clean Pipeline Stop

```verilog
// Debug interface
input wire debug_halt_request;
output wire debug_halted;

always @(posedge clk) begin
    if (debug_halt_request) begin
        // Stop issuing
        issue_enable <= 0;
        
        // Wait for queues to drain
        if (all_queues_empty) begin
            debug_halted <= 1;
        end
    end
end
```

### 7.2 State Inspection

Each queue's state is well-defined:
```verilog
// Debug outputs
output wire [DEPTH-1:0] alu_queue_valid;
output wire [DEPTH-1:0] mem_queue_valid;
output wire [DEPTH-1:0] mem_queue_waiting;
output wire [DEPTH-1:0] mc_queue_valid;
```

## 8. Risk Assessment

### 8.1 Low Risk
- **Modular design**: Each queue is independent
- **Forwarding**: Similar to classic pipeline
- **Testing**: Queues can be tested individually

### 8.2 Medium Risk
- **Completion ordering**: Must maintain program order for exceptions
- **Branch handling**: Drain latency may impact branch penalty
- **Queue sizing**: May need tuning for workloads

### 8.3 High Risk
- **Complexity**: More modules than classic pipeline
- **Verification**: Multiple queues increase state space

### 8.4 Mitigation

1. **Ordering**: In-order issue + in-order completion per queue guarantees order
2. **Branch**: Keep queues shallow (2-4 entries) for fast drain
3. **Sizing**: Parameterized queues for easy adjustment
4. **Verification**: Per-queue assertions, compositional verification

## 9. Expert Feedback

### Dr. Elena Vasquez (Pipeline Architecture)
> "The queue-based approach is clever. It's like a simplified reservation station without the complexity of out-of-order issue. The forwarding from queues is the tricky part - make sure the timing works."

**Response**: Forward checks are parallel and simple (4 comparisons per queue). Timing is manageable.

### Dr. Marcus Chen (Timing Closure)
> "I like that each queue is independent. This allows local optimizations without global impact. The completion arbiter is the potential bottleneck - keep it simple."

**Response**: Round-robin arbitration is O(1). Will keep arbiter logic minimal.

### Dr. James Liu (Compiler Optimization)
> "This design benefits from instruction scheduling even without requiring it. A scheduler could order instructions to minimize queue stalls. Consider exposing queue status for software optimization."

**Response**: Good idea. Could add performance counters for queue occupancy.

### Prof. Sarah Kim (FPGA Architecture)
> "The 4-entry MEM queue might be overkill for a simple design. Start with 2 entries and measure. Each entry adds to the forward check logic."

**Response**: Will parameterize queue depths. Start small, expand if needed.

### Prof. Anna Schmidt (Cache Architecture)
> "The MEM queue overlapping cache misses is powerful. With a 4-entry queue, you can have 4 outstanding loads, which hides a lot of memory latency. This is similar to miss-under-miss handling in modern caches."

**Response**: Exactly. This is the key performance win over classic pipeline.

## 10. Conclusion

The Hybrid Pipeline with Completion Queues offers an elegant middle ground. By using queues to track outstanding operations, we avoid the complexity of a central scoreboard while enabling overlapped execution of different operation types.

Key benefits:
1. **Modular**: Each queue is independent
2. **Scalable**: Add queues for new functional units
3. **Performant**: Overlaps multi-cycle and single-cycle ops
4. **Debuggable**: Clean state management

**Recommendation**: Strongest candidate for implementation. Combines best features of other approaches with manageable complexity. Higher performance than classic, more modular than scoreboard.

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2025-01-15 | M. Torres, A. Schmidt | Initial draft |
| 0.2 | 2025-01-15 | M. Torres | Added expert feedback, detail on MEM queue |
