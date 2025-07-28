# Instruction Decoder

The `InstructionDecoder` module is responsible for extracting and decoding various fields from 32-bit B32P2 instructions. It performs field extraction without any complex logic, serving as a pure combinational decoder.

## Module Declaration

```verilog
module InstructionDecoder (
    input   wire [31:0]  instr,      // 32-bit instruction input
    
    // Opcode outputs
    output  wire [3:0]   instrOP,    // Instruction opcode
    output  wire [3:0]   aluOP,      // ALU operation code
    output  wire [2:0]   branchOP,   // Branch operation code

    // Constant value outputs
    output  wire [31:0]  constAlu,   // 16-bit signed constant for ALU
    output  wire [31:0]  constAluu,  // 16-bit unsigned constant for ALU
    output  wire [31:0]  const16,    // 16-bit signed constant 
    output  wire [15:0]  const16u,   // 16-bit unsigned constant
    output  wire [26:0]  const27,    // 27-bit constant for jumps

    // Register address outputs
    output  wire [3:0]   areg,       // A register address
    output  wire [3:0]   breg,       // B register address  
    output  wire [3:0]   dreg,       // Destination register address

    // Control bit outputs
    output  wire         he,         // High-enable bit (loadhi)
    output  wire         oe,         // Offset-enable bit (jump[r])
    output  wire         sig         // Signed comparison bit (branch)
);
```

## Functionality

The instruction decoder performs pure combinational logic to extract different fields from the 32-bit instruction word based on the B32P2 ISA encoding. Sign extension is applied where necessary. The easier the ISA is designed, the easier it is to decode the instruction. For now this module is quite simple, and the comments above on the input and output signals should be mostly enough to understand how it works.
