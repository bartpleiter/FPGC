# Multi-Cycle ALU

The `MultiCycleALU` module implements complex arithmetic operations that require multiple clock cycles to complete, such as multiplication and division. It provides a state machine interface to control operation timing and integrates with the pipeline stall logic. In previous versions of the FPGC, these operations were included in the Memory Unit as a kind of co-processor, while the `MULT*` operations were implemented in the ALU. This required wrappers in software for these operations and reduced timing closure possibilities as the hardware multipliers in the FPGA need an input and output register to work optimally. See ISA page for the complete list of supported operations.

## Module Declaration

```verilog
module MultiCycleALU (
    input wire clk,               // System clock
    input wire reset,             // Reset signal

    input wire start,             // Start operation signal
    output reg done = 1'b0,       // Operation complete signal

    input wire [31:0] a,          // First operand
    input wire [31:0] b,          // Second operand
    input wire [3:0] opcode,      // Operation selection
    output reg [31:0] y = 32'd0   // Result output
);
```

## Communication Interface

This module uses the same simple bus like interface as other parts in the FPGC, by using a start and done signal. The `start` signal is asserted for one clock cycle to initiate the operation, and the `done` signal indicates for one clock cycle when the result is ready. The operands `a` and `b` together with `opcode` are provided as inputs (equivalent to `data`), and the result is output on `y` (equivalent to `q`).

## Implementation Details

Currently the module attempts to optimize multiplication for the DSP slices of the FPGA, and while this does seem to work for now, it is not really clean nor efficient and can be improved in the future. As the module works with a big state machine, it should be relatively easy to add more implementations based on the opcode.
