# Branch and Jump Unit

The `BranchJumpUnit` module implements control flow operations for the B32P3 CPU, including conditional branches, unconditional jumps, and halt operations. It evaluates branch conditions and selects target addresses for program counter updates.

## Module Declaration

```verilog
module BranchJumpUnit (
    // Branch condition inputs
    input wire  [2:0]   branchOP,           // Branch operation type
    input wire  [31:0]  data_a,             // First comparison operand
    input wire  [31:0]  data_b,             // Second comparison operand
    input wire          sig,                // Signed comparison enable
    
    input wire  [31:0]  const16,            // 16-bit constant (sign-extended)
    input wire  [31:0]  pc,                 // Current program counter
    input wire  [31:0]  pre_jump_const_addr,// Pre-computed jump address
    input wire  [31:0]  pre_branch_addr,    // Pre-computed branch address
    
    // Control inputs
    input wire          halt,               // Halt instruction
    input wire          branch,             // Branch instruction enable
    input wire          jumpc,              // Jump with constant enable
    input wire          jumpr,              // Jump with register enable
    input wire          oe,                 // Offset enable (relative addressing)

    // Outputs
    output wire [31:0]  jump_addr,          // Target address for jump/branch
    output wire         jump_valid          // Jump/branch should be taken
);
```

## Branch Operations

The unit supports 6 conditional branch operations:

| BranchOP | Mnemonic | Condition | Signed Version |
|----------|----------|-----------|----------------|
| `000` | BEQ | `A == B` | Same |
| `001` | BGT | `A > B` | BGTS |
| `010` | BGE | `A >= B` | BGES |
| `011` | Reserved | - | - |
| `100` | BNE | `A != B` | Same |
| `101` | BLT | `A < B` | BLTS |
| `110` | BLE | `A <= B` | BLES |
| `111` | Reserved | - | - |

### Branch Condition Evaluation

```verilog
always @(*) begin
    case (branchOP)
        3'b000: branch_passed = (data_a == data_b);  // BEQ
        3'b001: branch_passed = sig ? ($signed(data_a) > $signed(data_b)) 
                                    : (data_a > data_b);  // BGT/BGTS
        3'b010: branch_passed = sig ? ($signed(data_a) >= $signed(data_b)) 
                                    : (data_a >= data_b);  // BGE/BGES
        3'b100: branch_passed = (data_a != data_b);  // BNE
        3'b101: branch_passed = sig ? ($signed(data_a) < $signed(data_b)) 
                                    : (data_a < data_b);  // BLT/BLTS
        3'b110: branch_passed = sig ? ($signed(data_a) <= $signed(data_b)) 
                                    : (data_a <= data_b);  // BLE/BLES
        default: branch_passed = 1'b0;
    endcase
end
```

## Jump Operations

### Jump with Constant (JUMP)

Uses a 27-bit constant for the target address:

- **Absolute** (`O=0`): `PC ← {5'b0, const27}`
- **Relative** (`O=1`): `PC ← PC + sign_extend_32(const27)`

### Jump with Register (JUMPR)

Uses a register value with optional 16-bit offset:

- **Absolute** (`O=0`): `PC ← BREG + sign_extend(const16)`
- **Relative** (`O=1`): `PC ← PC + BREG + sign_extend(const16)`

## Pre-computed Addresses

To improve timing, target addresses are pre-computed before being used:

```verilog
// Computed in MEM stage before BranchJumpUnit
wire [31:0] pre_jump_const_addr = oe ? (pc + {{5{const27[26]}}, const27}) : {5'b0, const27};
wire [31:0] pre_branch_addr = pc + const16;
```

The unit then selects the appropriate pre-computed address based on the operation type.

!!! TODO
    Verify that this is still needed with the current optimizations. If no effect on timing, consider removing.

## Output Logic

```verilog
// Address selection
assign jump_addr = jumpc   ? pre_jump_const_addr :
                   jumpr   ? (oe ? pc + data_b + const16 : data_b + const16) :
                   branch  ? pre_branch_addr :
                   halt    ? pc :
                   32'd0;

// Valid when any control flow change occurs
assign jump_valid = jumpc || jumpr || (branch && branch_passed) || halt;
```

## HALT Behavior

The HALT instruction causes `jump_valid` to be asserted with `jump_addr = PC`, creating an infinite loop at the halt location. The CPU can still respond to interrupts when halted.
