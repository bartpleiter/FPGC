# Instruction Decoder

The `InstructionDecoder` module extracts and decodes various fields from 32-bit B32P3 instructions. It performs pure combinational field extraction without complex logic.

## Module Declaration

```verilog
module InstructionDecoder (
    input   wire [31:0]  instr,      // 32-bit instruction input
    
    // Opcode outputs
    output  wire [3:0]   instrOP,    // Instruction opcode [31:28]
    output  wire [3:0]   aluOP,      // ALU operation code [27:24]
    output  wire [2:0]   branchOP,   // Branch operation code [3:1]

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
    output  wire         he,         // High-enable bit
    output  wire         oe,         // Offset-enable bit
    output  wire         sig         // Signed comparison bit
);
```

## Field Extraction

### Instruction Opcode

```verilog
assign instrOP = instr[31:28];
```

The 4-bit instruction opcode determines the instruction type (ARITH, BRANCH, JUMP, etc.).

### ALU/Branch Opcodes

```verilog
assign aluOP    = instr[27:24];     // For ARITH/ARITHM instructions
assign branchOP = instr[3:1];       // For BRANCH instructions
```

### Register Addresses

Register field positions vary by instruction type:

```verilog
// For ARITHC/ARITHMC (constant instructions): A is in bits [7:4]
// For other instructions: A is in bits [11:8]
assign areg = (instrOP == 4'b0001 || instrOP == 4'b0011) ? instr[7:4] : instr[11:8];

// B register is always in bits [7:4] (or unused)
assign breg = (instrOP == 4'b0001 || instrOP == 4'b0011) ? 4'd0 : instr[7:4];

// Destination register is always in bits [3:0]
assign dreg = instr[3:0];
```

### Constants

```verilog
// 27-bit constant for JUMP instruction
assign const27 = instr[27:1];

// 16-bit constants with sign extension
assign const16u = instr[27:12];
assign const16  = {{16{instr[27]}}, instr[27:12]};  // Sign-extended

// ALU constants (position depends on instruction format)
assign constAlu  = {{16{instr[23]}}, instr[23:8]};  // Signed
assign constAluu = {16'b0, instr[23:8]};            // Unsigned
```

### Control Bits

```verilog
assign oe  = instr[0];   // Offset enable for jumps
assign sig = instr[0];   // Signed comparison for branches
assign he  = instr[0];   // High-enable (unused in current design)
```

## Instruction Format Reference

```text
         |31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
----------------------------------------------------------------------------------------------------------
ARITH      0  0  0  0||--aluOP-| x  x  x  x  x  x  x  x  x  x  x  x |--A REG---||--B REG---||--D REG---|
ARITHC     0  0  0  1||--aluOP-||----------------16 BIT CONSTANT---------------||--A REG---||--D REG---|
BRANCH     0  1  1  0||----------------16 BIT CONSTANT---------------||--A REG---||--B REG---||-brOP-||S|
JUMP       1  0  0  1||--------------------------------27 BIT CONSTANT--------------------------------||O|
READ       1  1  1  0||----------------16 BIT CONSTANT---------------||--A REG---| x  x  x  x |--D REG---|
```

## Pipeline Integration

The instruction decoder is instantiated in the ID stage:

```verilog
InstructionDecoder instr_decoder (
    .instr      (if_id_instr),      // From IF/ID pipeline register
    .instrOP    (id_instr_op),
    .aluOP      (id_alu_op),
    .branchOP   (id_branch_op),
    // ... other outputs
);
```

Decoded fields are then captured in the ID/EX pipeline register for use in subsequent stages.

## Early Register Address Extraction

For timing optimization, register addresses are also extracted directly in the IF stage:

```verilog
// Extract register addresses from IF instruction (same logic as decoder)
wire [3:0] if_instr_op = if_instr[31:28];
wire [3:0] if_areg = (if_instr_op == 4'b0001 || if_instr_op == 4'b0011) ? 
                     if_instr[7:4] : if_instr[11:8];
wire [3:0] if_breg = (if_instr_op == 4'b0001 || if_instr_op == 4'b0011) ? 
                     4'd0 : if_instr[7:4];
```

These addresses go directly to the register file, which has 2-cycle read latency, ensuring data is available in the EX stage.
