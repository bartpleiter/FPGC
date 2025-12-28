# Story 14: Interrupt Support

**Sprint**: 3  
**Priority**: P1 (High)  
**Estimate**: 2 hours  
**Status**: Not Started

## Description

As a developer, I want to integrate interrupt handling so that external events can preempt normal execution.

## Acceptance Criteria

1. [ ] Interrupt input interface (8 interrupt lines)
2. [ ] Interrupt detection
3. [ ] Pipeline flush on interrupt
4. [ ] Jump to interrupt handler
5. [ ] Return address saving
6. [ ] Interrupt enable/disable control

## Technical Details

### Interrupt Interface

```verilog
// 8 interrupt input lines
input wire [7:0] int_in;

// Interrupt priority:
// int_in[0] = highest priority
// int_in[7] = lowest priority
```

### Interrupt Controller (May Need to Create or Modify)

```verilog
// =============================================================================
// INTERRUPT HANDLING
// =============================================================================

// Interrupt enable flag (controlled by software)
reg int_enabled;

// Interrupt acknowledge (to clear interrupt source)
reg int_ack;

// Active interrupt (one-hot or encoded)
wire [7:0] int_pending = int_in & {8{int_enabled}};
wire int_request = |int_pending;

// Interrupt priority encoder
wire [2:0] int_number;
always @(*) begin
    casez (int_pending)
        8'b???????1: int_number = 3'd0;
        8'b??????10: int_number = 3'd1;
        8'b?????100: int_number = 3'd2;
        8'b????1000: int_number = 3'd3;
        8'b???10000: int_number = 3'd4;
        8'b??100000: int_number = 3'd5;
        8'b?1000000: int_number = 3'd6;
        8'b10000000: int_number = 3'd7;
        default:     int_number = 3'd0;
    endcase
end

// Interrupt vector table base
parameter INT_VECTOR_BASE = 32'h0000_0080;  // Or wherever defined

// Handler address for current interrupt
wire [31:0] int_handler_addr = INT_VECTOR_BASE + {26'b0, int_number, 3'b0};
```

### Interrupt Timing

```
Interrupts are checked at WB stage (instruction commit point)
This ensures:
1. Interrupted instruction completes
2. Precise exception state
3. Clean pipeline for handler

Alternative: Check at IF stage for faster response
```

### Interrupt Handling Logic

```verilog
// Interrupt state machine
localparam INT_IDLE    = 2'b00;
localparam INT_PENDING = 2'b01;
localparam INT_TAKEN   = 2'b10;

reg [1:0] int_state;

always @(posedge clk) begin
    if (reset) begin
        int_state <= INT_IDLE;
        int_enabled <= 1'b0;  // Interrupts disabled on reset
    end else begin
        case (int_state)
            INT_IDLE: begin
                if (int_request && int_enabled && !pipeline_stall) begin
                    int_state <= INT_PENDING;
                end
            end
            
            INT_PENDING: begin
                // Wait for current instruction to complete
                // Then take interrupt
                int_state <= INT_TAKEN;
            end
            
            INT_TAKEN: begin
                // Handler starts executing
                int_state <= INT_IDLE;
            end
        endcase
    end
end

// Interrupt signal for pipeline control
wire int_take = (int_state == INT_TAKEN);
```

### Pipeline Flush for Interrupt

```verilog
// On interrupt taken:
// 1. Save return address (PC of next instruction)
// 2. Flush pipeline
// 3. Jump to handler

wire int_flush = int_take;

// Return address = PC of next instruction after completed one
// This is the instruction in IF stage when interrupt is taken
wire [31:0] int_return_addr = pc;

// PC redirect to handler
wire int_redirect = int_take;
wire [31:0] int_target = int_handler_addr;
```

### Return Address Handling

```verilog
// Return address saved to:
// Option 1: Special register (EPC - Exception PC)
// Option 2: Stack (push automatically)
// Option 3: Link register (r14 or similar)

// Using special EPC register:
reg [31:0] epc;  // Exception Program Counter

always @(posedge clk) begin
    if (int_take) begin
        epc <= int_return_addr;
    end
end

// RETI instruction reads EPC and jumps back
// Or: EPC is memory-mapped, software reads it
```

### Interrupt Enable/Disable

```verilog
// Special instructions or memory-mapped control
// 
// Option 1: EI/DI instructions
// EI: int_enabled <= 1
// DI: int_enabled <= 0
//
// Option 2: Control register write
// Write to INTCTRL address enables/disables

// For Phase 1: Use simple flag controlled by instruction
wire set_int_enable = (id_opcode == OP_EI);
wire set_int_disable = (id_opcode == OP_DI);

always @(posedge clk) begin
    if (reset) begin
        int_enabled <= 1'b0;
    end else if (set_int_enable) begin
        int_enabled <= 1'b1;
    end else if (set_int_disable || int_take) begin
        int_enabled <= 1'b0;  // Disable on taking interrupt
    end
end
```

### Return from Interrupt

```verilog
// RETI instruction
wire id_is_reti = (id_opcode == OP_RETI);

// In EX stage:
wire ex_reti = id_ex_is_reti && id_ex_valid;

// Return: jump to EPC and re-enable interrupts
always @(posedge clk) begin
    if (ex_reti) begin
        int_enabled <= 1'b1;
    end
end

wire reti_redirect = ex_reti;
wire [31:0] reti_target = epc;
```

### Debug Output

```verilog
always @(posedge clk) begin
    if (!reset) begin
        if (int_request) begin
            $display("%0t INT: Request pending=%b enabled=%b",
                     $time, int_pending, int_enabled);
        end
        if (int_take) begin
            $display("%0t INT: TAKEN int#=%d handler=%h return=%h",
                     $time, int_number, int_handler_addr, int_return_addr);
        end
        if (ex_reti) begin
            $display("%0t INT: RETI returning to %h", $time, epc);
        end
    end
end
```

## Tasks

1. [ ] Review existing InterruptController module (if any)
2. [ ] Implement interrupt request detection
3. [ ] Implement priority encoder
4. [ ] Implement interrupt state machine
5. [ ] Add EPC register for return address
6. [ ] Implement pipeline flush on interrupt
7. [ ] Implement PC redirect to handler
8. [ ] Implement EI/DI instructions
9. [ ] Implement RETI instruction
10. [ ] Add debug output
11. [ ] Test interrupt handling

## Definition of Done

- Interrupts detected when enabled
- Pipeline flushes on interrupt
- Handler executed at correct address
- Return address saved
- RETI returns to correct location
- Interrupts can be enabled/disabled

## Test Plan

### Test 1: Basic Interrupt
```assembly
; Set up handler at vector address
; Enable interrupts
; Wait for interrupt
; Verify handler runs
; Verify return works
```

(Interrupt testing may require testbench modification)

## Dependencies

- Story 1-13 (Complete pipeline)

## Notes

- Interrupt testing requires testbench to generate int_in signals
- May defer detailed interrupt testing to integration phase
- Focus on interface compatibility first

## Review Checklist

- [ ] Interrupt input interface matches testbench
- [ ] Priority encoding correct
- [ ] EPC saved correctly
- [ ] Handler address calculation correct
- [ ] Pipeline flush complete
- [ ] Interrupts disabled in handler
- [ ] RETI works correctly
