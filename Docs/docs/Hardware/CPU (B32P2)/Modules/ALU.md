# ALU

The `ALU` module implements single-cycle, combinatorial arithmetic and logic operations for the B32P2 processor. See the ISA page for the complete list of supported operations.

## Module Declaration

```verilog
module ALU (
    input wire  [31:0]  a,      // First operand
    input wire  [31:0]  b,      // Second operand  
    input wire  [3:0]   opcode, // Operation selection
    output reg  [31:0]  y       // Result output
);
```

## Implementation Details

### Combinational Logic

The ALU is implemented as pure combinational logic using a case statement. Note that multiplication has been omitted compared to the FPGC6, as the hardware multipliers in the FPGA need an input and output register to prevent timing issues. This is handled by the `MultiCycleALU` module.

```verilog
always @ (*) begin
    case (opcode)
        ... // Operation cases
        default:    y = 32'd0;
    endcase
end
```

### Critical Path Considerations

Other than removing the multiplication from the ALU, not much effort is put into optimizing the timing of this module (if possible at all), as for now it is not a bottleneck.

## Control Unit Integration

The ALU opcode comes from the Control Unit via the Instruction Decoder:

```verilog
// In EXMEM1 stage
ALU alu_EXMEM1 (
    .a(alu_a_EXMEM1),           // From register or forwarding
    .b(alu_b_EXMEM1),           // From register, constant, or forwarding
    .opcode(aluOP_EXMEM1),      // From instruction decoder
    .y(alu_y_EXMEM1)            // To writeback or forwarding
);
```

## Special Considerations

### LOAD and LOADHI Operations

The `LOAD` and `LOADHI` operations are implemented as an arithmetic operation to prevent needing additional logic for loading constatnts. These operations are very simple and just set the result `y` to the constant value provided in the instruction.

### Overflow Handling

There is no overflow handling in the ALU (or anywhere else in the CPU). This means that no interrupt is generated on overflow, and that the result will always be the rightmost 32 bits of the result. As I have not found the need for overflow handling, I did not implement it. This may change in the future if I will really need it and software checking is not fast enough.
