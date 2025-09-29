# Control Unit

The `ControlUnit` module generates control signals for the CPU pipeline based on the instruction opcodes. It determines which functional units should be active and how data should flow through the pipeline for each instruction type. See ISA page for the instruction opcodes and their meanings.

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

## Control Logic Implementation

The control unit uses a combinational always block with a case statement to generate control signals:

```verilog
always @(*) begin
    // Set all signals to default values
    ...
    // Set signals based on instruction opcode
    case (instrOP)
        // Individual instruction handling
        ...
    endcase
end
```

### Special Cases

#### Unsigned Constant Selection

For `ARITHC` and `ARITHMC` instructions, the control unit checks the ALU opcode to determine if unsigned constants should be used:

```verilog
if (aluOP[3:1] == 3'b110) begin
    alu_use_constu <= 1'b1;
end
```

This corresponds to `LOAD` and `LOADHI` operations which use unsigned immediates.

### Extensibility

- Easy to add new instructions by extending the case statement
- Control signals can be added without affecting existing logic
- Opcode space allows for future instruction additions
