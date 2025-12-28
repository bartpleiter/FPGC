# Story 01: Project Setup and Module Skeleton

**Sprint**: 1  
**Priority**: P0 (Blocker)  
**Estimate**: 2 hours  
**Status**: Not Started

## Description

As a developer, I want to create the new B32P3 module structure so that I have a foundation for implementing the 5-stage pipeline while maintaining compatibility with the existing testbench.

## Acceptance Criteria

1. [ ] New `B32P3.v` file created in `Hardware/FPGA/Verilog/CPU/`
2. [ ] Module interface matches existing B32P2 interface exactly (same port names/widths)
3. [ ] Empty submodule placeholders for each pipeline stage
4. [ ] Module compiles without errors (`iverilog -t null`)
5. [ ] Testbench can instantiate B32P3 instead of B32P2

## Technical Details

### Port Interface (Must Match B32P2)

```verilog
module B32P3(
    // Clock and reset
    input wire clk,
    input wire reset,
    
    // ROM interface
    output wire [8:0]   rom_addr_a,
    input  wire [31:0]  rom_q_a,
    output wire [8:0]   rom_addr_b,
    input  wire [31:0]  rom_q_b,
    
    // VRAM32 interface
    output wire [9:0]   vram32_addr,
    output wire [31:0]  vram32_d,
    input  wire [31:0]  vram32_q,
    output wire         vram32_we,
    
    // VRAM8 interface
    output wire [12:0]  vram8_addr,
    output wire [7:0]   vram8_d,
    input  wire [7:0]   vram8_q,
    output wire         vram8_we,
    
    // VRAMPX interface
    output wire [16:0]  vrampx_addr,
    output wire [7:0]   vrampx_d,
    input  wire [7:0]   vrampx_q,
    output wire         vrampx_we,
    
    // L1 Instruction Cache interface
    output wire [26:0]  l1i_addr,
    input  wire [31:0]  l1i_q,
    input  wire         l1i_hit,
    input  wire         l1i_cache_reset,
    
    // L1 Data Cache interface
    output wire [26:0]  l1d_addr,
    output wire [31:0]  l1d_d,
    input  wire [31:0]  l1d_q,
    output wire         l1d_we,
    output wire         l1d_start,
    input  wire         l1d_hit,
    input  wire         l1d_cache_reset,
    
    // Memory Unit interface
    input  wire [31:0]  mu_q,
    input  wire         mu_done,
    output wire [31:0]  mu_addr,
    output wire [31:0]  mu_d,
    output wire         mu_we,
    output wire         mu_start,
    
    // Interrupts
    input  wire [7:0]   int_in
);
```

### File Structure to Create

```
Hardware/FPGA/Verilog/CPU/
├── B32P3.v              # Main CPU module (NEW)
├── PipelineRegisters.v  # IF/ID, ID/EX, EX/MEM, MEM/WB (NEW)
├── ForwardingUnit.v     # Data forwarding logic (NEW)
├── HazardUnit.v         # Hazard detection (NEW)
├── ... (existing modules to reuse)
```

### Initial B32P3.v Structure

```verilog
module B32P3(
    // ... full port list ...
);

// =============================================================================
// PIPELINE REGISTERS (to be implemented in Story 2)
// =============================================================================

// IF/ID
reg [31:0] if_id_pc;
reg [31:0] if_id_instr;
// ... more fields ...

// ID/EX
// ... fields ...

// EX/MEM
// ... fields ...

// MEM/WB
// ... fields ...

// =============================================================================
// STAGE WIRES (internal connections)
// =============================================================================

// IF stage outputs
wire [31:0] if_pc_next;
wire [31:0] if_instr;
// ... more wires ...

// =============================================================================
// SUBMODULE INSTANTIATIONS
// =============================================================================

// Reuse existing modules
InstructionDecoder instrDecoder(...);
Regbank regbank(...);
ALU alu(...);
// ... etc ...

// =============================================================================
// PIPELINE STAGES (Sequential Logic)
// =============================================================================

// IF Stage
always @(posedge clk) begin
    // To be implemented
end

// ID Stage (combinational decode)
// ...

// EX Stage
// ...

// MEM Stage
// ...

// WB Stage
// ...

endmodule
```

## Tasks

1. [ ] Create `B32P3.v` with full port interface
2. [ ] Add wire declarations for all internal signals
3. [ ] Add empty pipeline register declarations
4. [ ] Add placeholder instantiations for existing modules
5. [ ] Verify compilation with `iverilog`

## Definition of Done

- B32P3.v compiles without errors
- Can be instantiated in cpu_tb.v
- All ports connected (even if to dummy values)
- Ready for Story 2 (Pipeline Registers)

## Dependencies

- None (First story)

## Notes

- Keep B32P2.v intact for reference
- Use identical port names to avoid testbench changes
- Add header comment with module description

## Review Checklist

- [ ] Port interface matches B32P2 exactly
- [ ] No syntax errors
- [ ] Clear comments and organization
- [ ] Debug `$display` statements included for key signals
