# Stack

The `Stack` module implements a dedicated hardware stack for the B32P2 CPU, providing push and pop operations for function calls and local variables. The stack is designed as a 32-bit wide, 1024-word deep memory structure. There currently is no way to access either the stack pointer or data that is not at the stack pointer directly. Also, there is no overflow or underflow protection, so the stack pointer can wrap around and access invalid memory locations. The stack support hold and clear signals to allow pipeline control.

## Module Declaration

```verilog
module Stack (
    input wire          clk,            // System clock
    input wire          reset,          // Reset signal

    input wire  [31:0]  d,              // Data to push onto stack
    output wire [31:0]  q,              // Data popped from stack
    input wire          push,           // Push operation enable
    input wire          pop,            // Pop operation enable

    input wire          clear,          // Clear/flush signal
    input wire          hold            // Hold/stall signal
);
```

## Stack Architecture

### Memory Organization

- **Width**: 32 bits per stack entry
- **Depth**: 1024 words (4KiB total)
- **Addressing**: Internal 10-bit stack pointer
- **Growth direction**: Upward (incrementing addresses)

## Stack Operations

### Push Operation

When `push` is asserted:

1. Store data `d` at current stack pointer location
2. Increment stack pointer
3. Debug output (simulation only)

### Pop Operation

Pop operation is more complex due to pipeline integration:

```verilog
if (pop) begin
    useRamResult <= 1'b0;
    ramResult <= stack[ptr - 1'b1];    // Read data from stack
    
    if (clear) begin
        qreg <= 32'd0;                 // Flush: return zero
    end else if (hold) begin
        qreg <= qreg;                  // Stall: maintain current value
    end else begin
        useRamResult <= 1'b1;          // Normal: use stack data
        ptr <= ptr - 1'b1;             // Decrement stack pointer
        
        `ifdef __ICARUS__              // Debug output if run in simulation
            $display("%d: pop  ptr %d := %d", $time, ptr, stack[ptr - 1'b1]);
        `endif
    end
end
```

## Memory Characteristics

The stack array is designed to infer as Block RAM:

```verilog
reg [31:0] stack [1023:0];
```
