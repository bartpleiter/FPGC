/*
 * Mults
 * Registered signed 32x32 multiplier
 */
module Mults (
    input wire               clk,
    input wire               reset,

    input wire signed [31:0] a,
    input wire signed [31:0] b,
    input wire               start,

    output reg signed [63:0] y,
    output reg               done
);

reg signed [31:0] areg = 32'd0;
reg signed [31:0] breg = 32'd0;
reg done_next_cycle = 1'b0;

always @ (posedge clk)
begin
    if (reset)
    begin
        y <= 64'd0;
        done <= 1'b0;
        areg <= 32'd0;
        breg <= 32'd0;
        done_next_cycle <= 1'b0;
    end
    else
    begin
        areg <= a;
        breg <= b;
        y <= areg * breg;
        done <= done_next_cycle;
        done_next_cycle <= start;
    end
end

endmodule
