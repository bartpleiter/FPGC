// Keeps the outputs of a design alive in a timing run. Quartus removes logic whose
// result isn't used, so all outputs go through this registered XOR tree to one pin.
module sink #(
    parameter W = 32
) (
    input  wire         clk,
    input  wire [W-1:0] d,
    output reg          dout
);

localparam GROUPS = (W + 31) / 32;

wire [GROUPS*32-1:0] padded = d;
reg  [GROUPS-1:0]    group;

integer i;
always @(posedge clk) begin
    for (i = 0; i < GROUPS; i = i + 1)
        group[i] <= ^padded[32*i +: 32];
    dout <= ^group;
end

endmodule
