/*
* Stack
* mainly used for backing up registers in assembly
* 32 bits wide, can store an entire register per entry
* 128 words long
* TODO optional: send interrupt when pop on empty stack
*/

module Stack (
    input clk,
    input reset,
    input [31:0] d,
    output [31:0] q,
    input push,
    input pop,
    input clear, hold
);

reg [9:0]   ptr = 10'd0;            // stack pointer
reg [31:0]  stack [1023:0];  // stack

reg [31:0] ramResult = 32'd0;
reg useRamResult = 1'b0;
reg [31:0] qreg = 32'd0;

assign q = (useRamResult) ? ramResult : qreg;

always @(posedge clk)
begin
    if (reset)
    begin
        ptr <= 10'd0;
        useRamResult <= 32'd0;
        qreg <= 32'd0;
    end
    else 
    begin
        if (push)
        begin
            stack[ptr] <= d;
            ptr <= ptr + 1'b1;
            $display("%d: push @%d := %d", $time, ptr, d);
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
                //q <= stack[ptr - 1'b1]; // simulation does not like this when ptr = 0
                $display("%d: pop @%d := %d", $time, ptr, stack[ptr - 1'b1]);
            end
        end
    end
end

integer i;
initial
begin
    for (i = 0; i < 1024; i = i + 1)
    begin
        stack[i] = 32'd0;
    end
end

endmodule