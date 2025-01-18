/*
* Register that can be cleared or held
* Useful for passing data between CPU stages
*/

module Regr(
    input clk,
    input clear,
    input hold,
    input wire [N-1:0] in,
    output reg [N-1:0] out
);

parameter N = 1;

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