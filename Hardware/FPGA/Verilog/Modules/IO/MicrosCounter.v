/*
 * MicrosCounter
 * 32 bit microsecond counter starting from reset
 * Assumes 100MHz input clock
 */
module MicrosCounter (
    input wire          clk,
    input wire          reset,
    output reg  [31:0]  micros = 32'd0
);

reg [15:0] delay_counter = 16'd0; // counter for timing 1 us

always @(posedge clk)
begin
    if (reset)
    begin
        micros        <= 32'd0;
        delay_counter <= 16'd0;
    end
    else
    begin
        if (delay_counter == 16'd99)
        begin
            delay_counter <= 16'd0;
            micros <= micros + 1'b1;
        end
        else
        begin
            delay_counter <= delay_counter + 1'b1;
        end
    end
end

endmodule
