# Story 11: Cache Integration (L1i and L1d)

**Sprint**: 3  
**Priority**: P0 (Blocker)  
**Estimate**: 4 hours  
**Status**: Not Started

## Description

As a developer, I want to integrate the CPU with the existing L1 instruction and data caches so that SDRAM access works correctly.

## Acceptance Criteria

1. [ ] L1I cache interface for IF stage
2. [ ] L1D cache interface for MEM stage  
3. [ ] Cache miss stall handling
4. [ ] Cache reset handling
5. [ ] SDRAM load/store tests pass

## Technical Details

### Cache Architecture Overview

```
                        ┌─────────────────┐
                        │ CacheController │ (100MHz)
                        │    SDRAM.v      │
                        └────────┬────────┘
                                 │
              ┌──────────────────┼──────────────────┐
              │                  │                  │
        ┌─────▼─────┐      ┌─────▼─────┐           │
        │  L1I      │      │  L1D      │           │
        │  Cache    │      │  Cache    │           │
        └─────┬─────┘      └─────┬─────┘           │
              │                  │                  │
        ┌─────▼─────┐      ┌─────▼─────┐      ┌────▼────┐
        │    IF     │      │   MEM     │      │  SDRAM  │
        │   Stage   │      │   Stage   │      │   Chip  │
        └───────────┘      └───────────┘      └─────────┘
              │                  │
              └──────────────────┘
                   B32P3 (50MHz)
```

### L1I Cache Interface (IF Stage)

```verilog
// =============================================================================
// L1 INSTRUCTION CACHE INTERFACE
// =============================================================================

// L1I is accessed when:
// 1. PC >= 512 (outside ROM region)
// 2. Cache is not being reset

wire if_use_l1i = (pc >= 32'd512) && !l1i_cache_reset;

// Address output (27 bits for SDRAM addressing)
assign l1i_addr = pc[26:0];

// Cache hit detection
// l1i_hit is provided by cache controller
// l1i_q provides the instruction word

wire if_l1i_miss = if_use_l1i && !l1i_hit;
wire if_l1i_stall = if_l1i_miss;

// Instruction selection
wire [31:0] if_instr_from_cache = l1i_q;
wire [31:0] if_instr = if_use_l1i ? if_instr_from_cache : rom_q_a;

// Stall IF stage on cache miss
wire if_stall_cache = if_l1i_stall;
```

### L1D Cache Interface (MEM Stage)

```verilog
// =============================================================================
// L1 DATA CACHE INTERFACE  
// =============================================================================

// L1D is accessed when:
// 1. Memory address >= 0x1000_0000 (SDRAM region)
// 2. Memory read or write operation

wire mem_use_l1d = (mem_addr >= 32'h1000_0000) && 
                   (ex_mem_mem_read || ex_mem_mem_write) &&
                   ex_mem_valid;

// Address output
assign l1d_addr = mem_addr[26:0];

// Write data and enable
assign l1d_d = ex_mem_rt_data;
assign l1d_we = mem_use_l1d && ex_mem_mem_write;

// Start signal - pulse for one cycle to initiate access
reg l1d_start_r;
always @(posedge clk) begin
    if (reset) begin
        l1d_start_r <= 1'b0;
    end else begin
        // Start on first cycle of access, don't re-trigger while stalled
        l1d_start_r <= mem_use_l1d && !mem_l1d_stall;
    end
end
assign l1d_start = mem_use_l1d && !l1d_start_r && !l1d_hit;

// Cache hit detection
wire mem_l1d_miss = mem_use_l1d && !l1d_hit && !l1d_cache_reset;
wire mem_l1d_stall = mem_l1d_miss;

// Read data from cache
wire [31:0] mem_data_from_cache = l1d_q;
```

### Cache Miss Stall Handling

```verilog
// Combined stall signal from caches
wire cache_stall = if_l1i_stall || mem_l1d_stall;

// Pipeline stall on cache miss
// When either cache misses:
// 1. Stall all stages (freeze pipeline state)
// 2. Wait for cache to fill
// 3. Resume when hit signal asserts

// Connect to hazard unit
assign stall_if  = cache_stall || other_stalls;
assign stall_id  = cache_stall || other_stalls;
// EX/MEM/WB also freeze during cache stall
```

### Cache Reset Handling

```verilog
// Cache reset occurs during:
// 1. System reset
// 2. Cache line invalidation

// During cache reset, don't stall - cache will return garbage
// Code should not execute from SDRAM during reset anyway
wire l1i_in_reset = l1i_cache_reset;
wire l1d_in_reset = l1d_cache_reset;

// Modify stall logic to not stall during reset
assign if_l1i_stall = if_use_l1i && !l1i_hit && !l1i_in_reset;
assign mem_l1d_stall = mem_use_l1d && !l1d_hit && !l1d_in_reset;
```

### Cache Timing

```verilog
// Cache access timing (50MHz CPU clock):
//
// Hit case:
// Cycle 0: Address output
// Cycle 0: Data available (combinational hit)
//
// Miss case:
// Cycle 0: Address output, hit=0
// Cycle 1-N: Stall while cache fills (100MHz cache controller)
// Cycle N+1: hit=1, data available

// The cache controller operates at 100MHz (2x CPU clock)
// Cache fill typically takes 8-16 cycles at 100MHz = 4-8 cycles at 50MHz
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset) begin
        // L1I debug
        if (if_use_l1i) begin
            $display("%0t L1I: addr=%h hit=%b data=%h",
                     $time, l1i_addr, l1i_hit, l1i_q);
            if (if_l1i_stall) begin
                $display("%0t L1I: MISS - Stalling pipeline", $time);
            end
        end
        
        // L1D debug
        if (mem_use_l1d) begin
            $display("%0t L1D: addr=%h we=%b hit=%b rdata=%h wdata=%h",
                     $time, l1d_addr, l1d_we, l1d_hit, l1d_q, l1d_d);
            if (mem_l1d_stall) begin
                $display("%0t L1D: MISS - Stalling pipeline", $time);
            end
        end
    end
end
```

## Tasks

1. [ ] Implement L1I address generation
2. [ ] Implement L1I hit/miss detection
3. [ ] Implement L1I stall logic
4. [ ] Implement L1D address generation
5. [ ] Implement L1D write interface
6. [ ] Implement L1D start pulse
7. [ ] Implement L1D hit/miss detection
8. [ ] Implement L1D stall logic
9. [ ] Handle cache reset signals
10. [ ] Connect cache stalls to hazard unit
11. [ ] Add debug output
12. [ ] Test with SDRAM access

## Definition of Done

- L1I cache access works for instruction fetch
- L1D cache access works for load/store
- Pipeline stalls on cache miss
- Pipeline resumes when cache fills
- SDRAM tests pass

## Test Plan

### Test 1: Simple SDRAM Store/Load
```assembly
; Tests/CPU/10_sdram_cache/basic_store_load.asm
load 0x10000000 r1   ; SDRAM base address
load 42 r2
write r1 r2 0        ; Store to SDRAM
read r1 r15 0        ; Load from SDRAM ; expected=42
halt
```

### Test 2: Multiple SDRAM Access
```assembly
load 0x10000000 r1
load 100 r2
load 200 r3
write r1 r2 0        ; Store 100 at offset 0
write r1 r3 4        ; Store 200 at offset 4
read r1 r4 0         ; Load from offset 0
read r1 r5 4         ; Load from offset 4
add r4 r5 r15        ; expected=300
halt
```

## Dependencies

- Story 1-10 (Complete pipeline with hazard handling)
- External: CacheControllerSDRAM.v (existing)

## Notes

- Cache controller is external and operates at 100MHz
- CPU cannot modify cache internals, only use hit/q interface
- First SDRAM access will always miss (cold cache)
- Sequential accesses benefit from cache line fill

## Review Checklist

- [ ] L1I address matches cache controller expectations
- [ ] L1D address, data, we signals correct
- [ ] Start signal pulse correctly generated
- [ ] Hit signal correctly interpreted
- [ ] Stall applied to all pipeline stages
- [ ] No deadlock on cache miss
- [ ] Reset handling correct
