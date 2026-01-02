# ALU

The `ALU` module implements single-cycle, combinatorial arithmetic and logic operations for the B32P3 processor. See the ISA page for the complete list of supported operations.

## Module Declaration

```verilog
module ALU (
    input wire  [31:0]  a,      // First operand
    input wire  [31:0]  b,      // Second operand  
    input wire  [3:0]   opcode, // Operation selection
    output reg  [31:0]  y       // Result output
);
```

## Operations

The ALU supports the following single-cycle operations:

| Opcode | Operation | Description |
|--------|-----------|-------------|
| `0000` | OR        | `y = a \| b` |
| `0001` | AND       | `y = a & b` |
| `0010` | XOR       | `y = a ^ b` |
| `0011` | ADD       | `y = a + b` |
| `0100` | SUB       | `y = a - b` |
| `0101` | SHIFTL    | `y = a << b` |
| `0110` | SHIFTR    | `y = a >> b` (logical) |
| `0111` | NOT       | `y = ~a` |
| `1010` | SLT       | `y = (signed(a) < signed(b)) ? 1 : 0` |
| `1011` | SLTU      | `y = (a < b) ? 1 : 0` |
| `1100` | LOAD      | `y = b` |
| `1101` | LOADHI    | `y = {b[15:0], a[15:0]}` |
| `1110` | SHIFTRS   | `y = a >>> b` (arithmetic) |

## Implementation Details

### Combinational Logic

The ALU is implemented as pure combinational logic using a case statement:

```verilog
always @(*) begin
    case (opcode)
        4'b0000: y = a | b;           // OR
        4'b0001: y = a & b;           // AND
        4'b0010: y = a ^ b;           // XOR
        4'b0011: y = a + b;           // ADD
        4'b0100: y = a - b;           // SUB
        // ... other operations
        default: y = 32'd0;
    endcase
end
```

### Multiplication Note

Multiplication operations are handled by the `MultiCycleALU` module rather than this ALU. Hardware multipliers in FPGAs require input and output registers for optimal timing, making them unsuitable for a purely combinational single-cycle ALU.

## Pipeline Integration

The ALU operates in the EX (Execute) stage of the pipeline:

```verilog
ALU alu (
    .a      (ex_alu_a),        // From forwarding mux
    .b      (ex_alu_b),        // From forwarding mux or constant
    .opcode (id_ex_alu_op),    // From instruction decoder
    .y      (ex_alu_result)    // To EX/MEM register
);
```

## Special Operations

### LOAD and LOADHI

These operations handle constant loading:

- **LOAD**: Simply passes operand B (the constant) through
- **LOADHI**: Combines high 16 bits from B with low 16 bits from A, enabling 32-bit constant construction:

```asm
LOAD  r1, 0x1234      ; r1 = 0x00001234
LOADHI r1, 0x5678     ; r1 = 0x56781234
```

### Overflow Handling

The ALU does not generate overflow exceptions. Results are always truncated to 32 bits. Software must check for overflow conditions if needed.
