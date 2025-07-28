# Register Bank

The `Regbank` module implements the CPU's register file containing 16 general-purpose 32-bit registers. It provides dual-port read access for the pipeline while supporting write-back operations with sophisticated hazard handling.

## Module Declaration

```verilog
module Regbank (
    // Clock and reset
    input wire clk,
    input wire reset,

    // REG stage ports (read)
    input wire  [3:0]   addr_a,     // Register A address
    input wire  [3:0]   addr_b,     // Register B address
    input wire          clear,      // Clear/flush signal
    input wire          hold,       // Hold/stall signal
    output wire [31:0]  data_a,     // Register A data
    output wire [31:0]  data_b,     // Register B data

    // WB stage ports (write)
    input wire  [3:0]   addr_d,     // Destination register address
    input wire  [31:0]  data_d,     // Data to write
    input wire          we          // Write enable
);
```

## Architecture Overview

The register bank implements a sophisticated dual-port memory structure designed for efficient pipeline operation:

```
    REG Stage           Register Bank           WB Stage
  ┌─────────────┐     ┌─────────────────┐    ┌─────────────┐
  │   addr_a    │────▶│   Port A Read   │    │             │
  │   addr_b    │────▶│   Port B Read   │    │   addr_d    │────┐
  │   clear     │────▶│                 │    │   data_d    │──┐ │
  │   hold      │────▶│  16 x 32-bit    │    │   we        │─┐│ │
  └─────────────┘     │   Registers     │    └─────────────┘ ││ │
                      │                 │                    ││ │
  ┌─────────────┐     │   Write Port    │◀───────────────────┘│ │
  │   data_a    │◀────│                 │◀────────────────────┘ │
  │   data_b    │◀────│                 │◀──────────────────────┘
  └─────────────┘     └─────────────────┘
```

## Register Organization

### Register Set

- **16 registers**: R0 through R15
- **32-bit width**: Each register holds a 32-bit word
- **R0 special behavior**: Always reads as zero, writes are ignored

### Memory Implementation

The register bank is designed to be inferred as dual port single clock Block RAM by synthesis tools:

```verilog
reg [31:0] regs [0:15];  // 16 x 32-bit register array
```

## Read Operation Logic

The register bank implements read logic to handle pipeline hazards and special cases:

### Read Scenarios

1. **Normal Read**: Use Block RAM result
2. **Register R0**: Always return zero
3. **Clear/Flush**: Return zero to create pipeline bubble
4. **Hold/Stall**: Maintain previous read result
5. **Write Forwarding**: Return new value when reading register being written
