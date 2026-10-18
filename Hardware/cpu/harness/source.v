// Test data for timing runs: W flip-flops that change every cycle in a way Quartus
// can't predict or simplify, driven by a single input pin.
//
// Each bit becomes the XOR of its two neighbours,
// so no bit is a plain copy of another one. Plain copies would be merged, and a
// plain shift register could be turned into a RAM.
//
// Use one source per design and take slices of it: two sources of the same width
// would be identical, and Quartus would merge them into one.
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
