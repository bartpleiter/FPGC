# Multi-Cycle ALU

The `MultiCycleALU` module implements complex arithmetic operations that require multiple clock cycles, such as multiplication and division. It provides a state machine interface for operation timing and integrates with the pipeline stall logic.

## Module Declaration

```verilog
module MultiCycleALU (
    input wire          clk,        // System clock
    input wire          reset,      // Reset signal

    input wire          start,      // Start operation signal
    output reg          done,       // Operation complete signal

    input wire [31:0]   a,          // First operand
    input wire [31:0]   b,          // Second operand
    input wire [3:0]    opcode,     // Operation selection
    output reg [31:0]   y           // Result output
);
```

## Supported Operations

| Opcode | Operation | Description | Typical Cycles |
|--------|-----------|-------------|----------------|
| `0000` | MULTS | Signed multiplication | ~4 |
| `0001` | MULTU | Unsigned multiplication | ~4 |
| `0010` | MULTFP | Fixed-point multiplication | ~4 |
| `0011` | DIVS | Signed division | ~32 |
| `0100` | DIVU | Unsigned division | ~32 |
| `0101` | DIVFP | Fixed-point division | ~32 |
| `0110` | MODS | Signed modulo | ~32 |
| `0111` | MODU | Unsigned modulo | ~32 |

## Communication Interface

The module uses a simple handshake protocol:

1. **Start**: Caller asserts `start` for one cycle with valid `a`, `b`, and `opcode`
2. **Processing**: Module performs operation over multiple cycles
3. **Done**: Module asserts `done` for one cycle when result is ready on `y`

## Implementation Details

### Multiplication

Multiplication is optimized for FPGA DSP slices:

- Input and output registers for optimal timing
- Signed/unsigned variants handled separately

### Division

Division uses an iterative algorithm:

- Non-restoring or SRT division
- ~32 cycles for 32-bit operands
- Handles signed/unsigned and modulo operations

### Fixed-Point Operations

Fixed-point variants (MULTFP, DIVFP) assume a Q16.16 format:

- 16 bits integer, 16 bits fraction
- Multiplication: `(a * b) >> 16`
- Division: `(a << 16) / b`
