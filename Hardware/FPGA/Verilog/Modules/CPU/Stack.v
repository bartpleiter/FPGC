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

reg [9:0]   ptr = 10'd0;    // Stack pointer
reg [31:0]  stack [128:0]; // Stack memory

// RamResult is used to store the result of a pop operation
// useRamResult is used to determine if the result of a pop operation should be used
// qreg is used last value of q in case of a hold, or 0 if clear
reg [31:0]  ramResult = 32'd0;
reg         useRamResult = 1'b0;
reg [31:0]  qreg = 32'd0;

assign q = (useRamResult) ? ramResult : qreg;

always @(posedge clk)
begin
    if (reset)
    begin
        ptr <= 10'd0;
        useRamResult <= 1'b0;
        ramResult <= 32'd0;
        qreg <= 32'd0;
    end
    else 
    begin
        qreg <= q;
        
        if (push)
        begin
            stack[ptr] <= d;
            ptr <= ptr + 1'b1;
            $display("%d: push ptr %d := %d", $time, ptr, d);
        end

        if (pop)
        begin
            useRamResult <= 1'b0;
            ramResult <= stack[ptr - 1'b1];
            if (clear)
            begin
                qreg <= 32'd0;
            end
            else if (hold)
            begin
                qreg <= qreg;
            end
            else
            begin
                useRamResult <= 1'b1;
                ptr <= ptr - 1'b1;
                $display("%d: pop  ptr %d := %d", $time, ptr, stack[ptr - 1'b1]);
            end
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
