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

```text
        clk     ─┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──
                 └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  
        start   ─────┐                              
                     └─────────────────────────────────────
        a, b    ═════╳═════════════════════════════════════
                     valid
        done                                    ┌────┐
                ────────────────────────────────┘    └─────
        y       ════════════════════════════════╳══════════
                                                valid
```

## Pipeline Integration

### State Machine in CPU

The CPU uses a state machine to control multi-cycle operations:

```verilog
localparam MALU_IDLE    = 2'b00;
localparam MALU_STARTED = 2'b01;
localparam MALU_DONE    = 2'b10;

reg [1:0] malu_state = MALU_IDLE;
reg       malu_request_finished = 1'b0;
reg [31:0] malu_result_reg = 32'd0;
```

### Stall Generation

The pipeline stalls while a multi-cycle operation is in progress:

```verilog
assign multicycle_stall = id_ex_valid && id_ex_arithm && !malu_request_finished;
```

### Operation Flow

1. **IDLE**: Waiting for ARITHM/ARITHMC instruction
2. **STARTED**: `start` pulse sent to MultiCycleALU
3. **Wait**: Pipeline stalled, waiting for `done`
4. **DONE**: Result captured, stall released

```verilog
always @(posedge clk) begin
    case (malu_state)
        MALU_IDLE: begin
            if (id_ex_valid && id_ex_arithm && !backend_stall) begin
                malu_state <= MALU_STARTED;
                malu_start_reg <= 1'b1;
                malu_a_reg <= ex_alu_a;
                malu_b_reg <= ex_breg_forwarded;
                malu_opcode_reg <= id_ex_alu_op;
            end
        end
        
        MALU_STARTED: begin
            malu_start_reg <= 1'b0;
            if (malu_done) begin
                malu_result_reg <= malu_result;
                malu_request_finished <= 1'b1;
                malu_state <= MALU_DONE;
            end
        end
        
        MALU_DONE: begin
            // Result captured, ready for next instruction
            malu_request_finished <= 1'b0;
            malu_state <= MALU_IDLE;
        end
    endcase
end
```

## Implementation Details

### Multiplication

Multiplication is optimized for FPGA DSP slices:

- Uses hardware multiplier primitives when available
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

## Performance Considerations

- **Multiplication**: Fastest operation (~4 cycles)
- **Division/Modulo**: Slowest operations (~32 cycles)
- **Pipeline impact**: Other instructions stall during operation
- **Optimization**: Avoid division in tight loops when possible
