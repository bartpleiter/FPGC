# Stack

The `Stack` module implements a dedicated hardware stack for the B32P3 CPU, providing push and pop operations fast register backup and restoration. Note that there is no direct addressing of stack entries, and no warning is given for stack overflows or underflows.

## Module Declaration

```verilog
module Stack (
    input wire          clk,        // System clock
    input wire          reset,      // Reset signal

    input wire  [31:0]  d,          // Data to push onto stack
    output wire [31:0]  q,          // Data popped from stack
    input wire          push,       // Push operation enable
    input wire          pop,        // Pop operation enable

    input wire          clear,      // Clear/flush signal
    input wire          hold        // Hold/stall signal
);
```

## Stack Architecture

### Memory Organization

| Parameter | Value |
|-----------|-------|
| Width | 32 bits per entry |
| Depth | 128 words (512 Bytes) |
| Addressing | Internal 7-bit pointer |
| Growth | Upward (incrementing addresses) |

### Memory Structure

```verilog
reg [31:0] stack [127:0];  // Stack memory array
reg [6:0]  ptr = 7'd0;     // Stack pointer
```

The stack memory is designed to infer as Block RAM on FPGA.

## Stack Operations

### Push Operation

When `push` is asserted:

1. Store data `d` at current stack pointer location
2. Increment stack pointer

### Pop Operation

When `pop` is asserted:

1. Read data from stack pointer - 1
2. Decrement stack pointer
3. Output data on `q`

### Control Signal Handling

- **clear**: Outputs zero (creates pipeline bubble)
- **hold**: Maintains current output and pointer (for stalls)
