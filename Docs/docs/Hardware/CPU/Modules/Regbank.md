# Register Bank

The `Regbank` module implements the CPU's register file containing 16 general-purpose 32-bit registers. It provides read access for the pipeline with 2-cycle latency and single-cycle write capability.

## Module Declaration

```verilog
module Regbank (
    // Clock and reset
    input wire          clk,
    input wire          reset,

    // Read ports (addresses from IF, data available in EX)
    input wire  [3:0]   addr_a,     // Register A address
    input wire  [3:0]   addr_b,     // Register B address
    input wire          clear,      // Clear/flush signal
    input wire          hold,       // Hold/stall signal
    output wire [31:0]  data_a,     // Register A data
    output wire [31:0]  data_b,     // Register B data

    // Write port (WB stage)
    input wire  [3:0]   addr_d,     // Destination register address
    input wire  [31:0]  data_d,     // Data to write
    input wire          we          // Write enable
);
```

## Register Organization

### Register Set

| Register | Name | Description |
|----------|------|-------------|
| R0 | Zero | Hardwired to zero (writes ignored) |
| R1-R14 | General | General-purpose registers |
| R15 | - | Often used for return values |

### Memory Structure

```verilog
reg [31:0] regs [0:15];  // 16 x 32-bit register array
```

The register array is designed to infer as distributed RAM or Block RAM depending on FPGA resources.

## 2-Cycle Read Latency

The register file implements 2-cycle read latency for timing optimization:

```text
Cycle 1 (IF): Address captured from instruction
Cycle 2 (ID): Data read from register array, registered
Cycle 3 (EX): Data available at outputs
```

This design choice:

- Breaks the critical path from instruction decode to register read
- Allows the register file to be clocked at higher frequencies
- Requires forwarding logic to handle RAW hazards

### Pipeline Integration

```verilog
// Addresses extracted in IF stage
wire [3:0] if_areg = ...;  // From instruction bits
wire [3:0] if_breg = ...;

// Data available in EX stage
Regbank regbank (
    .addr_a (if_areg),      // Address in IF
    .addr_b (if_breg),      // Address in IF
    .data_a (ex_areg_data), // Data in EX
    .data_b (ex_breg_data), // Data in EX
    ...
);
```

## Read Operation Logic

The register bank handles several special cases:

### Normal Read

```verilog
// First stage: capture address and read from array
always @(posedge clk) begin
    addr_a_reg <= addr_a;
    data_a_raw <= regs[addr_a];
end

// Second stage: output with special case handling
always @(posedge clk) begin
    if (addr_a_reg == 4'd0)
        data_a <= 32'd0;        // R0 always zero
    else if (clear)
        data_a <= 32'd0;        // Flush: output zero
    else if (hold)
        data_a <= data_a;       // Stall: hold value
    else
        data_a <= data_a_raw;   // Normal: use read data
end
```

### Register R0 Behavior

Register R0 is hardwired to zero:

- Reads always return `32'd0`
- Writes are silently ignored

### Write Forwarding

If reading a register being written in the same cycle:

```verilog
if (we && addr_d == addr_a_reg && addr_d != 4'd0)
    data_a <= data_d;  // Forward write data
```

## Write Operation

Writes occur on the rising clock edge when `we` is asserted:

```verilog
always @(posedge clk) begin
    if (we && addr_d != 4'd0) begin
        regs[addr_d] <= data_d;
    end
end
```

### Write Port Connections

The write port connects to the WB stage:

```verilog
.addr_d (wb_dreg),      // Destination register from WB
.data_d (wb_data),      // Result data from WB
.we     (wb_dreg_we)    // Write enable from WB
```

## Control Signals

### Clear (Flush)

When `clear` is asserted, outputs return zero to create a pipeline bubble. This is used during:

- Branch mispredictions
- Interrupts
- Return from interrupt

### Hold (Stall)

When `hold` is asserted, outputs maintain their previous value. This is used during:

- Load-use hazards
- Cache misses
- Multi-cycle operations

## Timing Considerations

The 2-cycle read latency means:

1. Forwarding is essential for back-to-back dependent instructions
2. Load-use hazards require one stall cycle
3. Branch comparisons use forwarded data in MEM stage
