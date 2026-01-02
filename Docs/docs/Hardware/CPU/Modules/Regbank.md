# Register Bank

The `Regbank` module implements the CPU's register file containing 16 general-purpose 32-bit registers. For fastest timings, the idea is to use LUT-based registers rather than block RAM.

## Module Declaration

```verilog
module Regbank (
    // Clock and reset
    input wire          clk,
    input wire          reset,

    // Read ports (addresses from IF, data available in EX)
    input wire  [3:0]   addr_a,     // Register A address
    input wire  [3:0]   addr_b,     // Register B address
    input wire          clear,      // Clear/flush signal
    input wire          hold,       // Hold/stall signal
    output wire [31:0]  data_a,     // Register A data
    output wire [31:0]  data_b,     // Register B data

    // Write port (WB stage)
    input wire  [3:0]   addr_d,     // Destination register address
    input wire  [31:0]  data_d,     // Data to write
    input wire          we          // Write enable
);
```

## Register Set

- R0 is hardwired to zero (reads return 0, writes ignored)
- R1 to R15 are general-purpose registers

## Pipeline Logic

The register bank has a clear and hold input to manage pipeline stalls and flushes.
