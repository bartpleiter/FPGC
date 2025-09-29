# Branch and Jump Unit

The `BranchJumpUnit` module implements control flow operations for the B32P2 CPU, including conditional branches, unconditional jumps, and halt operations. It evaluates branch conditions and calculates target addresses for program counter updates.

!!! note
    All addresses are word-aligned, meaning that if you want to jump to the second instruction (assuming instruction codes start at address `0x00`), you need to jump to `0x01`.

## Module Declaration

```verilog
module BranchJumpUnit (
    // Branch condition inputs
    input wire  [2:0]   branchOP,       // Branch operation type
    input wire  [31:0]  data_a,         // First comparison operand
    input wire  [31:0]  data_b,         // Second comparison operand
    input wire          sig,            // Signed comparison enable
    
    // Address calculation inputs
    input wire  [31:0]  const16,        // 16-bit constant (sign-extended)
    input wire  [26:0]  const27,        // 27-bit constant for jumps
    input wire  [31:0]  pc,             // Current program counter
    
    // Control inputs
    input wire halt,                    // Halt instruction
    input wire branch,                  // Branch instruction enable
    input wire jumpc,                   // Jump with constant enable
    input wire jumpr,                   // Jump with register enable
    input wire oe,                      // Offset enable (relative addressing)

    // Outputs
    output reg [31:0] jump_addr,        // Target address for jump/branch
    output wire jump_valid              // Jump/branch should be taken
);
```

## Branch Operations

The branch unit supports 6 different conditional branch operations:

| BranchOP | Binary | Mnemonic | Condition | Signed Version |
|----------|--------|----------|-----------|----------------|
| 0 | `000` | BEQ | `A == B` | Same |
| 1 | `001` | BGT | `A > B` | BGTS |
| 2 | `010` | BGE | `A >= B` | BGES |
| 3 | `011` | Reserved | - | - |
| 4 | `100` | BNE | `A != B` | Same |
| 5 | `101` | BLT | `A < B` | BLTS |
| 6 | `110` | BLE | `A <= B` | BLES |
| 7 | `111` | Reserved | - | - |

### Branch Condition Evaluation

The branch condition is evaluated combinationally:

```verilog
always @(*) begin
    case (branchOP)
        BRANCH_OP_BEQ:  branch_passed <= (data_a == data_b);
        BRANCH_OP_BGT:  branch_passed <= (sig) ? ($signed(data_a) > $signed(data_b)) : (data_a > data_b);
        BRANCH_OP_BGE:  branch_passed <= (sig) ? ($signed(data_a) >= $signed(data_b)) : (data_a >= data_b);
        BRANCH_OP_BNE:  branch_passed <= (data_a != data_b);
        BRANCH_OP_BLT:  branch_passed <= (sig) ? ($signed(data_a) < $signed(data_b)) : (data_a < data_b);
        BRANCH_OP_BLE:  branch_passed <= (sig) ? ($signed(data_a) <= $signed(data_b)) : (data_a <= data_b);
        default:        branch_passed <= 1'b0;
    endcase
end
```

### Signed vs Unsigned Comparisons

The `sig` (signed) bit determines comparison type:

- **sig = 0**: Unsigned comparison using natural Verilog operators
- **sig = 1**: Signed comparison using `$signed()` casting

This allows the same branch opcodes to handle both signed and unsigned comparisons.

## Jump Operations

### Jump with Constant (JUMPC)

Jump instructions use a 27-bit constant for the target address:

```verilog
if (jumpc) begin
    if (oe) begin
        jump_addr <= pc + const27;      // Relative jump
    end else begin
        jump_addr <= {5'b0, const27};  // Absolute jump
    end
end
```

### Jump with Register (JUMPR)

Jump instructions can also use register values with optional offsets:

```verilog
if (jumpr) begin
    if (oe) begin
        jump_addr <= pc + (data_b + const16);    // Relative jump with register
    end else begin
        jump_addr <= data_b + const16;           // Absolute jump with register
    end
end
```

## Branch Address Calculation

Conditional branches always use relative addressing:

```verilog
if (branch) begin
    jump_addr <= pc + const16;          // PC-relative branch
end
```

As the offset is 16 bit signed, the maximum jump range is limited. Therefore, it is recommended to jump to invert the if statement to just skip the next instruction, which then can be a jump instruction with much more range.

## Halt Operation

The halt instruction creates a simple infinite loop:

```verilog
if (halt) begin
    jump_addr <= pc;                    // Jump to same address
end
```

The nice thing about this is that it does not require additional logic to implement, and it can still be interrupted. This is useful for bootloading, and in future use could be used to implement a debugger of some kind.

## Jump Valid Logic

The jump valid signal determines when a control flow change should occur:

```verilog
assign jump_valid = (jumpc | jumpr | (branch & branch_passed) | halt);
```

### Jump Valid Conditions

- **jumpc**: Unconditional jump with constant
- **jumpr**: Unconditional jump with register
- **branch & branch_passed**: Conditional branch that evaluates true
- **halt**: Halt instruction (infinite loop)

## Branch Prediction

There is no branch prediction currently implemented. You could say that the branch prediction is always "not taken", but this basically means nothing has been implemented for it. This is important for performance-critical small loops, like a rendering loop, meaning that a lot of performance can be gained by unrolling loops to reduce the number of taken jumps.

## Pipeline Integration

The branch/jump unit is used in the EXMEM1 pipeline stage. When `jump_valid` is asserted:

- **Pipeline flush**: Earlier stages (FE1, FE2, REG) are flushed
- **PC update**: Program counter updated to `jump_addr`
- **Branch penalty**: Typically 3-cycle penalty for taken branches/jumps
