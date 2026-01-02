# Control Unit

The `ControlUnit` module generates control signals for the CPU pipeline based on instruction opcodes. It determines which functional units should be active and how data should flow through the pipeline for each instruction type.

## Module Declaration

```verilog
module ControlUnit(
    input wire [3:0]    instrOP,        // Main instruction opcode
    input wire [3:0]    aluOP,          // ALU operation code

    // ALU control signals
    output reg          alu_use_const,  // Use constant instead of register B
    output reg          alu_use_constu, // Use unsigned constant
    
    // Stack control signals
    output reg          push,           // Push operation
    output reg          pop,            // Pop operation
    
    // Register control signals
    output reg          dreg_we,        // Destination register write enable
    
    // Memory control signals
    output reg          mem_write,      // Memory write enable
    output reg          mem_read,       // Memory read enable
    
    // Multi-cycle operation control
    output reg          arithm,         // Multi-cycle arithmetic operation
    
    // Control flow signals
    output reg          jumpc,          // Jump with constant
    output reg          jumpr,          // Jump with register
    output reg          branch,         // Branch operation
    output reg          halt,           // Halt processor
    output reg          reti,           // Return from interrupt
    
    // Special operation signals
    output reg          getIntID,       // Get interrupt ID
    output reg          getPC,          // Get program counter
    output reg          clearCache      // Clear cache
);
```

## Instruction Decoding

The control unit uses a combinational always block with a case statement:

```verilog
always @(*) begin
    // Default all outputs to inactive
    alu_use_const  = 1'b0;
    alu_use_constu = 1'b0;
    push           = 1'b0;
    pop            = 1'b0;
    dreg_we        = 1'b0;
    mem_write      = 1'b0;
    mem_read       = 1'b0;
    arithm         = 1'b0;
    jumpc          = 1'b0;
    jumpr          = 1'b0;
    branch         = 1'b0;
    halt           = 1'b0;
    reti           = 1'b0;
    getIntID       = 1'b0;
    getPC          = 1'b0;
    clearCache     = 1'b0;
    
    case (instrOP)
        // Instruction-specific control signal generation
        ...
    endcase
end
```

## Control Signals by Instruction

| Instruction | Key Control Signals |
|-------------|---------------------|
| HALT | `halt=1` |
| READ | `mem_read=1`, `dreg_we=1` |
| WRITE | `mem_write=1` |
| INTID | `getIntID=1`, `dreg_we=1` |
| PUSH | `push=1` |
| POP | `pop=1`, `dreg_we=1` |
| JUMP | `jumpc=1` |
| JUMPR | `jumpr=1` |
| CCACHE | `clearCache=1` |
| BRANCH | `branch=1` |
| SAVPC | `getPC=1`, `dreg_we=1` |
| RETI | `reti=1` |
| ARITH | `dreg_we=1` |
| ARITHC | `alu_use_const=1`, `dreg_we=1` |
| ARITHM | `arithm=1`, `dreg_we=1` |
| ARITHMC | `arithm=1`, `alu_use_const=1`, `dreg_we=1` |

## Special Cases

### Unsigned Constant Selection

For ARITHC and ARITHMC instructions, the control unit checks the ALU opcode to determine if unsigned constants should be used:

```verilog
// LOAD and LOADHI use unsigned constants
if (aluOP[3:1] == 3'b110) begin
    alu_use_constu = 1'b1;
end
```

This corresponds to `LOAD` (opcode `1100`) and `LOADHI` (opcode `1101`) operations which use unsigned 16-bit immediates.

### Register Write Enable

The `dreg_we` signal enables writing to the destination register. It's set for:

- ALU operations (ARITH, ARITHC, ARITHM, ARITHMC)
- Memory reads (READ)
- Stack pops (POP)
- Special registers (INTID, SAVPC)

### Multi-cycle Operations

The `arithm` signal indicates that the instruction requires the MultiCycleALU (multiplication, division, modulo). The pipeline stalls until the operation completes.
