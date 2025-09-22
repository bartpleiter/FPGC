# Pipeline Register (Regr)

The `Regr` module implements a parameterizable pipeline register with clear and hold capabilities, used throughout the B32P2 CPU to pass data between pipeline stages while supporting hazard control.

## Module Declaration

```verilog
module Regr #(
    parameter N = 1             // Width of the register in bits
) (
    input  wire         clk,    // System clock
    input  wire         clear,  // Clear/flush signal
    input  wire         hold,   // Hold/stall signal
    input  wire [N-1:0] in,     // Input data
    output reg  [N-1:0] out     // Output data (registered)
);
```

## Parameters

| Parameter | Description | Default | Usage |
|-----------|-------------|---------|-------|
| `N` | Register width in bits | 1 | Set to required data width |

Parameter `N` can be set to any value to allow forwarding many signals at the same time without needing to instantiate multiple registers:
```verilog
Regr #(.N(65)) regr_combined_stageX_to_stage_Y (
    .clk(clk),
    .clear(clear_signal),
    .hold(hold_signal),
    .in({data_32_a, data_32_b, signal_bit}),
    .out({data_32_a_out, data_32_b_out, signal_bit_out})
);
```

## Functionality

The pipeline register implements three operational modes based on control signals:

### Normal Operation (clear=0, hold=0)

```verilog
out <= in;      // Pass input to output on clock edge
```

- **Purpose**: Standard pipeline register behavior
- **Usage**: Data flows normally through pipeline stages

### Clear Operation (clear=1)

```verilog
out <= {N{1'b0}};   // Output all zeros
```

- **Purpose**: Create pipeline bubble (flush)
- **Usage**: Control hazards, exceptions, branch mispredictions
- **Effect**: Inserts NOP equivalent into pipeline

### Hold Operation (hold=1, clear=0)

```verilog
out <= out;     // Maintain current output value
```

- **Purpose**: Stall pipeline stage
- **Usage**: Data hazards, cache misses, multi-cycle operations
- **Effect**: Freezes pipeline stage state
