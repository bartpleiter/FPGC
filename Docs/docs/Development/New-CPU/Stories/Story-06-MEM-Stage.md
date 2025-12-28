# Story 06: Memory Stage (MEM) - Basic

**Sprint**: 1  
**Priority**: P0 (Blocker)  
**Estimate**: 3 hours  
**Status**: Not Started

## Description

As a developer, I want to implement the MEM stage so that load/store operations can access memory (VRAM, ROM data, Memory Unit).

## Acceptance Criteria

1. [ ] Memory address calculation and output
2. [ ] Memory write data output
3. [ ] Memory read data capture
4. [ ] VRAM32, VRAM8, VRAMPX access
5. [ ] ROM data read (second port)
6. [ ] Memory Unit access for peripheral I/O
7. [ ] Basic load/store test passes

## Technical Details

### Memory Map

Make sure to follow the memory map defined in Memory-Map.md (the same as the existing CPU).

### MEM Stage Flow

```
EX/MEM Register
    │
    ├─► Address Decoder ─► Memory Select Signals
    │
    ├─► Memory Address ─► VRAM/ROM/MU/Cache
    │
    ├─► Write Data ─► VRAM/MU/Cache
    │
    └─► Read Data Mux ◄─ VRAM/ROM/MU/Cache
    │
    ▼
MEM/WB Register
```

### Address Decoding

```verilog
// =============================================================================
// MEMORY (MEM) STAGE
// =============================================================================

// Memory operation signals from EX/MEM
wire        mem_read  = ex_mem_mem_read && ex_mem_valid;
wire        mem_write = ex_mem_mem_write && ex_mem_valid;
wire [31:0] mem_addr  = ex_mem_alu_result;  // Address from ALU
wire [31:0] mem_wdata = ex_mem_rt_data;     // Write data from rt

// Address decode (determine which memory to access)
wire mem_sel_rom    = ...;
wire mem_sel_vram32 = ...;
wire mem_sel_vram8  = ...;
wire mem_sel_vrampx = ...;
wire mem_sel_mu     = ...;
wire mem_sel_sdram  = ...;
```

### VRAM32 Interface

```verilog
// VRAM32: 1024 x 32-bit words
// Address offset: 0x200, so subtract base
wire [9:0] vram32_offset = mem_addr[11:2] - 10'h080;  // Word aligned

assign vram32_addr = vram32_offset;
assign vram32_d    = mem_wdata;
assign vram32_we   = mem_write && mem_sel_vram32;

wire [31:0] vram32_rdata = vram32_q;
```

### VRAM8 Interface

```verilog
// VRAM8: 8192 x 8-bit bytes
// Address offset: 0x600
wire [12:0] vram8_offset = mem_addr[12:0] - 13'h0600;

assign vram8_addr = vram8_offset;
assign vram8_d    = mem_wdata[7:0];
assign vram8_we   = mem_write && mem_sel_vram8;

// Byte read - zero extend to 32 bits
wire [31:0] vram8_rdata = {24'b0, vram8_q};
```

### VRAMPX Interface

```verilog
// VRAMPX: Pixel VRAM (larger)
// Address offset: 0x2600
wire [16:0] vrampx_offset = mem_addr[16:0] - 17'h02600;

assign vrampx_addr = vrampx_offset;
assign vrampx_d    = mem_wdata[7:0];
assign vrampx_we   = mem_write && mem_sel_vrampx;

wire [31:0] vrampx_rdata = {24'b0, vrampx_q};
```

### ROM Data Port

```verilog
// ROM read (using port B, port A is for IF stage)
assign rom_addr_b = mem_addr[8:0];  // 9-bit address

wire [31:0] rom_rdata = rom_q_b;
```

### Memory Unit Interface

```verilog
// Memory Unit for peripheral I/O
assign mu_addr  = mem_addr;
assign mu_d     = mem_wdata;
assign mu_we    = mem_write && mem_sel_mu;
assign mu_start = (mem_read || mem_write) && mem_sel_mu;

wire [31:0] mu_rdata = mu_q;
wire        mu_stall = mem_sel_mu && !mu_done && (mem_read || mem_write);
```

### L1D Cache Interface (SDRAM)

```verilog
// L1D Cache for SDRAM access
assign l1d_addr  = mem_addr[26:0];
assign l1d_d     = mem_wdata;
assign l1d_we    = mem_write && mem_sel_sdram;
assign l1d_start = (mem_read || mem_write) && mem_sel_sdram;

wire [31:0] l1d_rdata = l1d_q;
wire        l1d_stall = mem_sel_sdram && !l1d_hit && !l1d_cache_reset;
```

### Read Data Multiplexer

```verilog
// Select read data based on address
reg [31:0] mem_rdata;
always @(*) begin
    case (1'b1)
        mem_sel_rom:    mem_rdata = rom_rdata;
        mem_sel_vram32: mem_rdata = vram32_rdata;
        mem_sel_vram8:  mem_rdata = vram8_rdata;
        mem_sel_vrampx: mem_rdata = vrampx_rdata;
        mem_sel_mu:     mem_rdata = mu_rdata;
        mem_sel_sdram:  mem_rdata = l1d_rdata;
        default:        mem_rdata = 32'hDEAD_BEEF;  // Invalid address
    endcase
end
```

### MEM Stage Stall

```verilog
// MEM stage stalls entire pipeline when:
// 1. L1D cache miss
// 2. Memory Unit operation in progress
assign mem_stall = (l1d_stall || mu_stall) && (mem_read || mem_write);
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset && ex_mem_valid) begin
        if (mem_read) begin
            $display("%0t MEM: READ addr=%h data=%h sel_rom=%b sel_vram32=%b sel_sdram=%b",
                     $time, mem_addr, mem_rdata, mem_sel_rom, mem_sel_vram32, mem_sel_sdram);
        end
        if (mem_write) begin
            $display("%0t MEM: WRITE addr=%h data=%h sel_vram32=%b sel_sdram=%b",
                     $time, mem_addr, mem_wdata, mem_sel_vram32, mem_sel_sdram);
        end
        if (mem_stall) begin
            $display("%0t MEM: STALL l1d=%b mu=%b", $time, l1d_stall, mu_stall);
        end
    end
end
```

## Tasks

1. [ ] Implement address decoder for all memory regions
2. [ ] Implement VRAM32 interface
3. [ ] Implement VRAM8 interface
4. [ ] Implement VRAMPX interface
5. [ ] Implement ROM data read (port B)
6. [ ] Implement Memory Unit interface
7. [ ] Implement L1D cache interface
8. [ ] Implement read data multiplexer
9. [ ] Implement MEM stall signal
10. [ ] Wire to MEM/WB pipeline register
11. [ ] Add debug `$display` statements

## Definition of Done

- All memory regions accessible
- Read and write operations work
- Stall on cache miss
- Stall on Memory Unit busy
- Load/store tests pass

## Test Plan

### Test 1: Load from VRAM32
```assembly
; Store then load from VRAM32
load 0x200 r1       ; VRAM32 base address  
load 42 r2
write r1 r2 0       ; Write 42 to VRAM32[0]
read r1 r15 0       ; Read back ; expected=42
halt
```

## Dependencies

- Story 1-5 (Pipeline stages up to EX)

## Notes

- You might want to reconsider the vram address offset logic to match the memory map and keep it simple.
- For the tests, make sure to use the project Makefile to run the test. Inspect the Makefile to understand how the test framework works.
- There are two testbenches: cpu_tb.v for `make sim-cpu` and cpu_tests_tb.v that is used with the `make test-cpu` commands.
- ROM read uses port B (port A reserved for IF stage)
- VRAM access is single-cycle (BRAM)
- L1D cache hit is single-cycle, miss stalls
- Memory Unit has variable latency

## Review Checklist

- [ ] Address decoding matches memory map
- [ ] All VRAM regions accessible
- [ ] ROM read works (data port)
- [ ] Memory Unit interface correct
- [ ] L1D cache interface correct
- [ ] Stall signals generated correctly
- [ ] No write to ROM allowed
