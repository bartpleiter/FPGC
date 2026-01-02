/*
 * Stack
 * 32 Bits wide, 128 words long
 * Not directly addressable, only push and pop operations supported
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

reg [6:0]   ptr = 7'd0;    // Stack pointer
(* ramstyle = "logic" *) reg [31:0]  stack [127:0];  // Stack memory
    
reg [31:0]  q_reg = 32'd0;
assign q = q_reg; // Registered output

always @(posedge clk)
begin
    if (reset)
    begin
        ptr <= 7'd0;
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
