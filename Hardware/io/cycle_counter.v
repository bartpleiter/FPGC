// Cycle counter
module cycle_counter (
    input  wire        clk,
    input  wire        reset,

    input  wire        take_snapshot,  // copies the count into snapshot
    output reg  [63:0] snapshot
);

reg [63:0] count;

always @(posedge clk) begin
    count <= reset ? 64'd0 : count + 64'd1;
    if (reset)
        snapshot <= 64'd0;
    else if (take_snapshot)
        snapshot <= count;
end

endmodule
