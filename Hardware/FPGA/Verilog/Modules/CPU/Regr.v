/*
 * Regr
 * Register that can be cleared or held
 * For passing data between CPU stages
 */
module Regr #(
    parameter N = 1 // Amount of bits in the register
) (
    input  wire         clk,
    input  wire         clear,
    input  wire         hold,
    input  wire [N-1:0] in,
    output reg  [N-1:0] out
);

always @(posedge clk)
begin
    if (clear)
    begin
        out <= {N{1'b0}};
    end
    else if (hold)
    begin
        out <= out;
    end
    else
    begin
        out <= in;
    end
end

initial
begin
    out <= {N{1'b0}};
end

endmodule
