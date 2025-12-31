# Stack

The `Stack` module implements a dedicated hardware stack for the B32P3 CPU, providing push and pop operations for function calls and local variable management.

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
| Depth | 1024 words (4KiB) |
| Addressing | Internal 10-bit pointer |
| Growth | Upward (incrementing addresses) |

### Memory Structure

```verilog
reg [31:0] stack [1023:0];  // Stack memory array
reg [9:0]  ptr = 10'd0;     // Stack pointer
```

The stack memory is designed to infer as Block RAM on FPGA.

## Stack Operations

### Push Operation

When `push` is asserted:

1. Store data `d` at current stack pointer location
2. Increment stack pointer

```verilog
if (push && !clear && !hold) begin
    stack[ptr] <= d;
    ptr <= ptr + 1'b1;
end
```

### Pop Operation

When `pop` is asserted:

1. Read data from stack pointer - 1
2. Decrement stack pointer
3. Output data on `q`

```verilog
if (pop && !clear && !hold) begin
    q <= stack[ptr - 1'b1];
    ptr <= ptr - 1'b1;
end
```

### Control Signal Handling

- **clear**: Outputs zero (creates pipeline bubble)
- **hold**: Maintains current output and pointer (for stalls)

## Pipeline Integration

The stack operates in the MEM stage of the pipeline:

```verilog
// Stack control signals
wire stack_push = ex_mem_valid && ex_mem_push && !backend_pipeline_stall;
wire stack_pop = ex_mem_valid && ex_mem_pop && !backend_pipeline_stall;

Stack stack (
    .clk    (clk),
    .reset  (reset),
    .d      (ex_mem_breg_data),     // Data from BREG for PUSH
    .q      (stack_q),              // Data for POP result
    .push   (stack_push),
    .pop    (stack_pop),
    .clear  (1'b0),
    .hold   (backend_pipeline_stall)
);
```

### Pop Result Availability

Pop results are available in the WB stage (one cycle after the pop operation):

- Pop operation occurs in MEM stage
- Stack output `q` is captured in MEM/WB register
- Result written to register file in WB stage

This creates a pop-use hazard similar to load-use hazards.

## Stack Operations in ISA

### PUSH Instruction

```text
PUSH Breg   ; stack[SP] = Breg; SP++
```

- Encoding: `1011 | xxxx...xxxx | BREG | xxxx`
- Pushes the value in BREG onto the stack

### POP Instruction

```text
POP Dreg    ; SP--; Dreg = stack[SP]
```

- Encoding: `1010 | xxxx...xxxx | DREG`
- Pops the top value from stack into DREG

## Usage Considerations

### No Overflow/Underflow Protection

The stack does not implement overflow or underflow detection:

- Stack pointer wraps around at boundaries
- Software must ensure stack stays within bounds
- Typical usage: function calls and local variables

### No Direct Stack Access

The current design does not support:

- Reading the stack pointer value
- Accessing stack elements other than top
- Setting the stack pointer to arbitrary values

For these operations, use general-purpose registers and memory.

### Performance

- **Push**: 1 cycle (occurs in MEM stage)
- **Pop**: 1 cycle + 1 cycle forwarding delay (result in WB)
- **Pop-use hazard**: 1 cycle stall if next instruction needs pop result

## Example Usage

```asm
; Function prologue
PUSH r14            ; Save return address
PUSH r13            ; Save callee-saved register

; Function body
...

; Function epilogue
POP r13             ; Restore callee-saved register
POP r14             ; Restore return address
JUMPR r14, 0        ; Return
```
