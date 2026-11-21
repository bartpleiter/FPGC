// Simulation model of system_pll (Hardware/system/system_pll.v), for the testbenches
// and linting. It passes its input clock through, so a testbench drives sys_clk_50 at
// 100 MHz, and sets `locked` after LOCK_CYCLES. A testbench can force `locked` to 0 to
// play a PLL that loses its lock.
module system_pll #(
    parameter LOCK_CYCLES = 20
) (
    input  wire clk_50,
    output wire clk_100,
    output reg  locked = 1'b0
);

reg [7:0] cycles = 8'd0;

assign clk_100 = clk_50;

always @(posedge clk_100) begin
    if (cycles != LOCK_CYCLES[7:0])
        cycles <= cycles + 8'd1;
    locked <= cycles == LOCK_CYCLES[7:0];
end

endmodule
