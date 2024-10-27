/*
* Phase generator for color carrier
*/
module PhaseGen(
    input               clk, //2*57.272727MHz
    input [4:0]         select, // phase select
    output wire         colorPhase, // phase of selected color
    output wire         burstPhase // phase of color burst
);

reg [4:0] counter;

wire [4:0] phaseCounter;
assign phaseCounter = counter - select + 5'd0;

assign burstPhase = counter[4];
assign colorPhase = phaseCounter[4];

always @(posedge clk) 
begin
   counter <= counter + 1'b1;
end

initial
begin
    counter <= 5'd0;
end

endmodule
