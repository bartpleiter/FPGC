# RFC-002: Decoupled Fetch-Execute Architecture

**Author**: Dr. Marcus Chen (Timing Closure Expert)  
**Status**: Draft  
**Created**: 2025-01-15  
**Last Updated**: 2025-01-15  

## Abstract

This RFC proposes a decoupled architecture separating the instruction fetch path from the execute path using FIFO buffers. This design allows the fetch unit to continue operating during execution stalls, providing natural instruction prefetching and hiding memory latency. The execute pipeline remains simple while gaining significant performance benefits.

## 1. Motivation

The current B32P2 design tightly couples fetch and execute stages, meaning cache misses anywhere stall the entire pipeline. A decoupled architecture addresses this by:

1. **Isolating fetch latency**: I-cache misses don't stall execute
2. **Natural prefetching**: Fetch runs ahead filling instruction buffer
3. **Simpler execute pipeline**: No I-cache hazards in execute logic
4. **Better timing**: Shorter critical paths in each unit

This approach is used in many high-performance processors (Alpha 21264, AMD K6, Intel P6).

## 2. Proposed Architecture

### 2.1 High-Level Structure

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         FETCH UNIT                                       │
│  ┌──────┐    ┌──────┐    ┌──────┐    ┌─────────────────────────────┐   │
│  │  PC  │───►│I-Cache│───►│ Line │───►│    Instruction FIFO        │   │
│  │ Unit │    │Access │    │Buffer│    │    (8-16 entries)          │   │
│  └──────┘    └──────┘    └──────┘    └───────────────┬─────────────┘   │
│      ▲                                                │                  │
│      │                                                ▼                  │
│      └──────────────◄─────────────◄──── branch redirect ◄───────────────┤
└─────────────────────────────────────────────────────────────────────────┘
                                                        │
                                                        ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         EXECUTE UNIT                                     │
│  ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐             │
│  │Decode│───►│RegRead│───►│  EX  │───►│ MEM  │───►│  WB  │             │
│  │      │    │       │    │      │    │      │    │      │             │
│  └──────┘    └──────┘    └──────┘    └──────┘    └──────┘             │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Fetch Unit Design

The fetch unit operates independently with its own control:

```verilog
module FetchUnit (
    input  wire        clk,
    input  wire        reset,
    
    // I-Cache interface
    output reg  [31:0] icache_addr,
    input  wire [31:0] icache_data,
    input  wire        icache_hit,
    output reg         icache_request,
    
    // FIFO interface
    output reg  [31:0] fifo_instr,
    output reg  [31:0] fifo_pc,
    output reg         fifo_write,
    input  wire        fifo_full,
    
    // Branch redirect
    input  wire        branch_redirect,
    input  wire [31:0] branch_target,
    
    // Stall from multi-cycle ops (division, etc.)
    input  wire        execute_stall
);
```

**Key Features**:
1. **Autonomous operation**: Fetches as fast as FIFO allows
2. **Prefetch effect**: Naturally fills buffer during stalls
3. **Branch handling**: Flushes FIFO on redirect, restarts at target

### 2.3 Instruction FIFO

```verilog
module InstructionFIFO #(
    parameter DEPTH = 8
) (
    input  wire        clk,
    input  wire        reset,
    
    // Write port (from fetch)
    input  wire [63:0] write_data,   // {PC, instruction}
    input  wire        write_enable,
    output wire        full,
    
    // Read port (to execute)
    output wire [63:0] read_data,
    input  wire        read_enable,
    output wire        empty,
    
    // Flush (on branch)
    input  wire        flush
);
```

**Sizing Rationale**:
- 8 entries covers ~12-cycle cache miss (fetch continues, execute stalls)
- 16 entries provides better buffering for burst misses
- Each entry: 64 bits (32-bit instruction + 32-bit PC)

### 2.4 Execute Pipeline

A clean 4-stage execute pipeline:

```
┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐
│ Decode │───►│   EX   │───►│  MEM   │───►│   WB   │
└────────┘    └────────┘    └────────┘    └────────┘
     │             │             │             │
     ▼             ▼             ▼             ▼
  FIFO Read    ALU/Branch    D-Cache       RegFile
  + Decode    + Forward     Access        Writeback
```

**Simplifications vs Current B32P2**:
- No I-cache stall logic in pipeline
- No FE1/FE2 stage complexity
- Clean hazard detection (standard patterns)

### 2.5 Hazard Handling

#### Data Hazards (Execute Pipeline)
Standard forwarding:
```verilog
// EX→EX forwarding
wire forward_ex_a = (rs1_EX == rd_MEM) && regwrite_MEM && (rs1_EX != 0);
wire forward_ex_b = (rs2_EX == rd_MEM) && regwrite_MEM && (rs2_EX != 0);

// MEM→EX forwarding  
wire forward_mem_a = (rs1_EX == rd_WB) && regwrite_WB && (rs1_EX != 0);
wire forward_mem_b = (rs2_EX == rd_WB) && regwrite_WB && (rs2_EX != 0);
```

#### Load-Use Hazard
```verilog
// Stall decode for 1 cycle when load result needed
wire load_use_hazard = memread_EX && 
    ((rd_EX == rs1_decode) || (rd_EX == rs2_decode)) && (rd_EX != 0);
wire decode_stall = load_use_hazard;
```

#### Control Hazards (Branch)
```verilog
// On branch resolution in EX:
// 1. Signal fetch unit to redirect
// 2. Flush FIFO (all speculative instructions)
// 3. Flush decode stage
assign branch_redirect = branch_taken_EX;
assign branch_target = branch_addr_EX;
assign fifo_flush = branch_taken_EX;
assign decode_flush = branch_taken_EX;
```

#### Multi-Cycle Operations
```verilog
// Multi-cycle ops stall execute pipeline (not fetch!)
wire execute_stall = dcache_miss || division_busy || io_busy;

// Fetch continues unless FIFO full
wire fetch_stall = fifo_full;
```

### 2.6 Memory System Integration

**I-Cache**:
- Connected only to fetch unit
- Miss handling in fetch unit (simpler state machine)
- Non-blocking: fetch stalls locally, FIFO provides buffer

**D-Cache**:
- Connected in MEM stage of execute pipeline
- Miss stalls execute pipeline (blocks WB)
- Fetch continues during D-cache miss (fills FIFO)

```
D-Cache Miss Timeline (12 cycles):

Fetch:   [I1][I2][I3][I4][I5][I6][I7][I8][FULL]...wait...[resume]
FIFO:    [  ][I1][I2][I3][I4][I5][I6][I7][I8  ][I8 ][I8 ][I8 ][drain]
Decode:  [  ][  ][I1][I2][I3][I4][I5][I6][STALL]...wait...[I7]
EX:      [  ][  ][  ][I1][I2][I3][I4][I5][STALL]...wait...[I6]
MEM:     [  ][  ][  ][  ][LD][S ][S ][S ][S    ][S  ][S  ][done]
WB:      [  ][  ][  ][  ][  ][S ][S ][S ][S    ][S  ][S  ][S  ]
```

## 3. Critical Path Analysis

### 3.1 Fetch Unit Paths

| Path | Operations | Estimated Delay |
|------|-----------|-----------------|
| PC→Cache | PC mux + BRAM address | 2ns |
| Cache→FIFO | BRAM read + FIFO write logic | 4ns |
| Branch redirect | Mux + PC load | 2ns |

**Fetch critical path**: ~4ns (can run at 200MHz+)

### 3.2 Execute Unit Paths

| Path | Operations | Estimated Delay |
|------|-----------|-----------------|
| FIFO→Decode | FIFO read + instruction decode | 4ns |
| Decode→EX | RegFile read + forward mux | 4ns |
| EX (ALU) | Forward select + ALU | 5ns |
| EX→MEM | Result mux + address calc | 3ns |
| MEM | D-Cache BRAM access | 3ns |
| WB | Result mux + RegFile setup | 2ns |

**Execute critical path**: ~5ns (EX stage)

### 3.3 Target Frequency

Both units can achieve 100MHz (10ns) with margin:
- Fetch: 4ns + routing margin = ~6ns = 166MHz capable
- Execute: 5ns + routing margin = ~7ns = 142MHz capable

**Conservative target**: 100MHz unified clock

## 4. Performance Analysis

### 4.1 FIFO Benefits

**Without Decoupling** (current B32P2 behavior):
- I-cache miss: 12 cycles stall
- D-cache miss: 12 cycles stall (no parallel fetch)
- Total cache penalty: sum of both

**With Decoupling**:
- I-cache miss: ~0 cycles (if FIFO not empty)
- D-cache miss: 12 cycles (but fetch continues)
- Net effect: I-cache misses mostly hidden

### 4.2 CPI Analysis

| Event | Frequency | Cycles | Classic | Decoupled |
|-------|-----------|--------|---------|-----------|
| Base execution | 100% | 1.0 | 1.00 | 1.00 |
| Load-use stall | 15% | 1.0 | 0.15 | 0.15 |
| Branch penalty | 10% | 2.0 | 0.20 | 0.20 |
| D-Cache miss | 5% | 12.0 | 0.60 | 0.60 |
| I-Cache miss | 2% | 12.0 | 0.24 | ~0.05* |
| Division | 1% | 32.0 | 0.32 | 0.32 |

*I-cache miss mostly hidden by FIFO buffer

**Estimated CPI**: ~2.3 (vs 2.5 for classic)

### 4.3 Performance Comparison

| Design | Clock | CPI | MIPS |
|--------|-------|-----|------|
| Current B32P2 | 50MHz | 1.5 | 33.3 |
| Classic 5-stage | 100MHz | 2.5 | 40.0 |
| Decoupled | 100MHz | 2.3 | 43.5 |

**Improvement vs Current**: ~30% MIPS gain

## 5. Implementation Details

### 5.1 Module Structure

```
B32P3_Decoupled/
├── B32P3.v                 # Top module
├── fetch/
│   ├── FetchUnit.v         # Fetch control
│   ├── PCUnit.v            # PC management
│   └── InstrFIFO.v         # Instruction buffer
├── execute/
│   ├── DecodeStage.v       # Instruction decode
│   ├── ExecuteStage.v      # ALU and branch
│   ├── MemoryStage.v       # D-Cache access
│   └── WritebackStage.v    # Result writeback
├── hazards/
│   ├── HazardUnit.v        # Execute hazards only
│   └── ForwardingUnit.v    # Data forwarding
├── cache/
│   ├── ICache.v            # I-Cache (fetch unit only)
│   └── DCache.v            # D-Cache (execute only)
└── multicycle/
    ├── Divider.v
    └── Multiplier.v
```

### 5.2 FIFO Implementation

```verilog
module InstrFIFO #(
    parameter DEPTH = 8,
    parameter WIDTH = 64
) (
    input  wire              clk,
    input  wire              reset,
    input  wire              flush,
    
    // Write port
    input  wire [WIDTH-1:0]  write_data,
    input  wire              write_enable,
    output wire              full,
    
    // Read port
    output wire [WIDTH-1:0]  read_data,
    input  wire              read_enable,
    output wire              empty,
    
    // Status
    output wire [3:0]        count
);

reg [WIDTH-1:0] buffer [0:DEPTH-1];
reg [2:0] write_ptr = 0;
reg [2:0] read_ptr = 0;
reg [3:0] fill_count = 0;

assign full = (fill_count == DEPTH);
assign empty = (fill_count == 0);
assign count = fill_count;
assign read_data = buffer[read_ptr];

always @(posedge clk) begin
    if (reset || flush) begin
        write_ptr <= 0;
        read_ptr <= 0;
        fill_count <= 0;
    end else begin
        // Simultaneous read and write
        if (write_enable && !full) begin
            buffer[write_ptr] <= write_data;
            write_ptr <= write_ptr + 1;
        end
        
        if (read_enable && !empty) begin
            read_ptr <= read_ptr + 1;
        end
        
        // Update count
        case ({write_enable && !full, read_enable && !empty})
            2'b10: fill_count <= fill_count + 1;
            2'b01: fill_count <= fill_count - 1;
            default: fill_count <= fill_count;
        endcase
    end
end

endmodule
```

### 5.3 Branch Handling

```verilog
// In top module
always @(posedge clk) begin
    if (branch_taken_EX) begin
        // 1. Redirect fetch
        fetch_redirect <= 1'b1;
        fetch_target <= branch_addr_EX;
        
        // 2. Flush FIFO
        fifo_flush <= 1'b1;
        
        // 3. Flush decode stage
        decode_valid <= 1'b0;
    end else begin
        fetch_redirect <= 1'b0;
        fifo_flush <= 1'b0;
    end
end
```

## 6. Advanced Optimizations

### 6.1 Speculative Fetch (Optional)

Simple branch prediction in fetch unit:
```verilog
// Predict backward branches taken (loops)
wire predict_taken = (branch_offset[15]) ? 1'b1 : 1'b0;  // Negative offset = backward

// Update fetch PC speculatively
wire [31:0] next_fetch_pc = predict_taken ? branch_target : pc + 1;
```

### 6.2 Critical Word First (Optional)

On D-cache miss, return needed word first:
```verilog
// Request critical word first from SDRAM controller
wire [2:0] critical_offset = mem_addr[2:0];
// ... SDRAM returns this word first
```

### 6.3 Non-Blocking Loads (Future)

Track outstanding loads with MSHR:
```verilog
// Miss Status Holding Register (1-2 entries)
reg [31:0] mshr_addr;
reg [3:0]  mshr_rd;
reg        mshr_valid;
// ... allows continued execution until dependency
```

## 7. Risk Assessment

### 7.1 Low Risk
- **FIFO design**: Simple, well-understood structure
- **Execute pipeline**: Standard hazard patterns
- **Cache integration**: Cleaner than current design

### 7.2 Medium Risk
- **Clock domain**: Both units at same clock (simplifies)
- **FIFO sizing**: May need tuning for optimal depth
- **Branch latency**: Same as classic (can add prediction later)

### 7.3 High Risk
- **Complexity budget**: More modules than classic design
- **Debug visibility**: FIFO adds state to track

### 7.4 Mitigation

1. **Complexity**: Modular design, independent testing
2. **FIFO sizing**: Parameterized, easy to adjust
3. **Debug**: Add FIFO counters/status to debug interface

## 8. Implementation Estimate

| Task | Time (days) | Dependencies |
|------|-------------|--------------|
| Fetch unit + FIFO | 3 | None |
| Execute pipeline | 3 | None |
| Integration | 2 | Fetch + Execute |
| Cache adaptation | 2 | Integration |
| Multi-cycle unit | 1 | Execute |
| Testing/debug | 4 | All above |
| **Total** | **15 days** | |

## 9. Expert Feedback

### Dr. Elena Vasquez (Pipeline Architecture)
> "The decoupled design is elegant but adds complexity for modest gain (~8% over classic). For a learning project, the classic pipeline might be better to understand first, then add decoupling as an enhancement."

**Response**: Valid point. Could implement classic first, then add FIFO as enhancement. Document supports incremental approach.

### Prof. Sarah Kim (FPGA Architecture)
> "The FIFO needs careful implementation for timing. Use BRAM-based FIFO for depth >4 entries. Cyclone IV has dedicated FIFO IP that's already optimized."

**Response**: Good suggestion. Will use BRAM-based FIFO and evaluate Intel FIFO IP.

### Dr. James Liu (Compiler Optimization)
> "The I-cache miss hiding is valuable but depends on sequential code patterns. For code with many branches, FIFO empties quickly. Consider tracking FIFO effectiveness metrics."

**Response**: Will add performance counters for FIFO hit rate and average fill level.

### Prof. Anna Schmidt (Cache Architecture)
> "This design pairs well with an I-cache line buffer (fetch whole cache line, dispatch sequentially). Reduces I-cache bandwidth requirements."

**Response**: Excellent idea. Line buffer can be part of FetchUnit, feeding FIFO word-by-word.

### Dr. Michael Torres (Verification)
> "The FIFO adds interesting verification challenges: empty/full boundary conditions, flush during active writes, etc. Recommend formal verification of FIFO properties."

**Response**: Will add assertions for FIFO invariants and use cover properties for corner cases.

## 10. Conclusion

The Decoupled Fetch-Execute architecture provides meaningful performance improvement over the classic design by hiding I-cache miss latency. The additional complexity is well-contained in the fetch unit, leaving the execute pipeline simple. This design offers a good balance of performance, complexity, and timing characteristics.

**Recommendation**: Good candidate for implementation after mastering classic pipeline. Offers clear performance benefits with manageable complexity increase.

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2025-01-15 | M. Chen | Initial draft |
| 0.2 | 2025-01-15 | M. Chen | Added expert feedback, FIFO details |
