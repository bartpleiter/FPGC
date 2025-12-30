/*
 * Stack
 * 32 Bits wide, 128 words long
 * Not directly addressable, only push and pop operations supported
 *
 * TIMING OPTIMIZATION: The output q is now purely registered.
 * The BRAM read result is registered, and q uses only registered values.
 * This breaks the critical path from hold -> useRamResult -> q.
 */
module Stack (
    input wire          clk,
    input wire          reset,

    input wire  [31:0]  d,
    output wire [31:0]  q,
    input wire          push,
    input wire          pop,

    input wire          clear,
    input wire          hold
);

reg [9:0]   ptr = 10'd0;    // Stack pointer
reg [31:0]  stack [128:0];  // Stack memory

// Output register - purely registered output for timing closure
// After a pop, this gets updated with the stack value
// (* dont_retime *) prevents Quartus from retiming this register,
// which would create a combinational path from hold to the register output
(* dont_retime *) reg [31:0]  q_reg = 32'd0;

// Output is purely registered - no combinational dependency on hold
assign q = q_reg;

always @(posedge clk)
begin
    if (reset)
    begin
        ptr <= 10'd0;
        q_reg <= 32'd0;
    end
    else if (clear)
    begin
        q_reg <= 32'd0;
    end
    else if (!hold)
    begin
        if (push)
        begin
            stack[ptr] <= d;
            ptr <= ptr + 1'b1;
            $display("%d: push ptr %d := %d", $time, ptr, d);
        end

        if (pop)
        begin
            q_reg <= stack[ptr - 1'b1];
            ptr <= ptr - 1'b1;
            $display("%d: pop  ptr %d := %d", $time, ptr, stack[ptr - 1'b1]);
        end
    end
end

integer i;
initial
begin
    for (i = 0; i < 128; i = i + 1)
    begin
        stack[i] = 32'd0;
    end
end

endmodule
