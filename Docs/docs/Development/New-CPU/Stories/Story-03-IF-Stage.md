# Story 03: Instruction Fetch Stage (IF)

**Sprint**: 1  
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

## Description

As a developer, I want to implement the IF stage so that instructions can be fetched from ROM and instruction cache.

## Acceptance Criteria

1. [ ] PC register management (increment, branch, jump)
2. [ ] ROM access for boot region (same address space used as existing CPU)
3. [ ] L1 instruction cache access for SDRAM region
4. [ ] Pipeline stall on cache miss
5. [ ] Branch target address loading
6. [ ] Basic load_immediate test passes

## Technical Details

### Memory Map for Instruction Fetch

Needs to use exactly the same memory map as the existing CPU:

- ROM at 0x7800000 (should boot from here)
- SDRAM at 0x00
- See Memory-Map.md for more details

### IF Stage Signals

```verilog
// =============================================================================
// INSTRUCTION FETCH (IF) STAGE
// =============================================================================

// Program Counter
reg [31:0] pc;              // Current PC
wire [31:0] pc_next;        // Next PC (computed)
wire [31:0] pc_plus_1;      // PC + 1 (sequential)
wire [31:0] pc_branch;      // Branch/jump target

// Instruction fetch
wire [31:0] if_instr;       // Fetched instruction
wire        if_instr_valid; // Instruction is valid (not stalled)

// Memory interface
wire        if_use_rom;     // Fetch from ROM
wire        if_use_cache;   // Fetch from cache
wire        if_cache_stall; // Cache miss - stall

// PC source selection
wire [1:0]  pc_src;         // 0=pc+1, 1=branch, 2=jump, 3=exception
```

### PC Logic

```verilog
// PC + 1 calculation
assign pc_plus_1 = pc + 32'd1;

// PC source mux
assign pc_next = (pc_src == 2'd0) ? pc_plus_1 :
                 (pc_src == 2'd1) ? pc_branch :
                 (pc_src == 2'd2) ? pc_jump :
                 /* exception */    pc_exception;

// PC update
always @(posedge clk) begin
    if (reset) begin
        pc <= 32'h0000_0000;  // Start at ROM address 0
    end else if (!stall_if) begin
        pc <= pc_next;
    end
    // On stall: hold current PC
end
```

### ROM Access

```verilog
// ROM region detection
assign if_use_rom = (pc >= 32'h7800000);

// ROM address output (use port A for IF stage)
assign rom_addr_a = pc[9:0];  // 10-bit address for 1024 words

// ROM provides data combinatorially
wire [31:0] rom_instr = rom_q_a;
```

### L1 Instruction Cache Access

```verilog
// Cache region detection
assign if_use_cache = !if_use_rom;

// L1I cache address output
assign l1i_addr = pc[26:0];  // 27-bit address

// Cache hit/miss handling
assign if_cache_stall = if_use_cache && !l1i_hit && !l1i_cache_reset;

// Instruction selection
assign if_instr = if_use_rom ? rom_instr : l1i_q;
assign if_instr_valid = !if_cache_stall && !reset;
```

### Stall Handling

```verilog
// IF stage stalls when:
// 1. L1I cache miss (waiting for cache fill)
// 2. Pipeline stall from later stage (hazard)
// 3. Multi-cycle operation in progress

assign stall_if = if_cache_stall || hazard_stall || multicycle_stall;
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset) begin
        $display("%0t IF: pc=%h rom=%b cache=%b stall=%b instr=%h", 
                 $time, pc, if_use_rom, if_use_cache, stall_if, if_instr);
        if (pc_src != 2'd0) begin
            $display("%0t IF: PC REDIRECT src=%d target=%h", 
                     $time, pc_src, pc_next);
        end
    end
end
```

## Tasks

1. [ ] Implement PC register with reset
2. [ ] Implement pc_next calculation with mux
3. [ ] Implement ROM address generation
4. [ ] Implement L1I cache address generation
5. [ ] Implement cache miss stall logic
6. [ ] Implement instruction selection mux
7. [ ] Connect to IF/ID pipeline register
8. [ ] Add debug `$display` statements
9. [ ] Test with simple ROM-only program

## Definition of Done

- PC increments correctly
- ROM fetch works
- Cache interface signals generated correctly
- Stall on cache miss
- Branch/jump targets can be loaded
- `load_immediate.asm` test passes

## Test Plan

### Test 1: Basic ROM Fetch
```assembly
; Tests/CPU/01_load_store/load_immediate.asm
load 37 r15 ; expected=37
halt
```

For this, make sure to use the test from Tests/CPU/01_load_store/load_immediate.asm, and use the project Makefile to run the test. Inspect the Makefile to understand how the test framework works.

Expected behavior:
1. PC starts at 0
2. Fetch instruction from ROM address 0
3. Pass to ID stage
4. Increment PC to 1

## Dependencies

- Story 1: Project Setup
- Story 2: Pipeline Registers

## Notes

- There are two testbenches: cpu_tb.v for `make sim-cpu` and cpu_tests_tb.v that is used with the `make test-cpu` commands.
- ROM has 2 read ports - use port A for IF, port B for MEM stage data reads
- Cache reset signal must be handled (don't stall during reset)
- Branch target comes from EX stage (feedback path)
- ROM starts at address 0x7800000 in full address space and is where the PC starts on reset. RAM starts at 0x00.

## Review Checklist

- [ ] PC initializes to 0 on reset
- [ ] ROM address correctly truncated to 9 bits
- [ ] Cache address uses correct bits
- [ ] Stall signal correct for cache miss
- [ ] No fetch during stall
