// Test data for timing runs. W flip-flops, driven by a single input pin, change every
// cycle in a way Quartus can't predict or simplify.
module source #(
    parameter W = 32
) (
    input  wire         clk,
    input  wire         din,
    output reg  [W-1:0] q
);

always @(posedge clk)
    q <= {q[W-2:0], din} ^ {1'b0, q[W-1:1]};

endmodule
