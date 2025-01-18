// 32 bit millisecond counter starting from reset

module MillisCounter(
    input clk,
    input reset,
    output reg [31:0] millis = 32'd0
);

reg [15:0] delayCounter = 16'd0; // counter for timing 1 ms

always @(posedge clk)
begin
    if (reset)
    begin
        millis          <= 32'd0;
        delayCounter    <= 16'd0;
    end
    else
    begin
        if (delayCounter == 16'd49999)
        begin
            delayCounter <= 16'd0;
            millis <= millis + 1'b1;
        end
        else
        begin
            delayCounter <= delayCounter + 1'b1;
        end
    end
end

endmodule
