// Simulation model of system_pll (Hardware/system/system_pll.v), for the testbenches
// and linting. It passes its input clock through, so a testbench drives sys_clk_50 at
// 100 MHz, and gives it 7.9 ns later as the SDRAM's clock (sdram decision 6). It sets
// `locked` after LOCK_CYCLES. A testbench can force `locked` to 0 to play a PLL that
// loses its lock. Its delay is in the testbench's time unit, ns.
module system_pll #(
    parameter LOCK_CYCLES = 20
) (
    input  wire clk_50,
    output wire clk_100,
    output reg  clk_sdram = 1'b0,
    output reg  locked = 1'b0
);

reg [7:0] cycles = 8'd0;

assign clk_100 = clk_50;

// verilator lint_off ASSIGNDLY
always @(clk_50)
    clk_sdram <= #7.9 clk_50;
// verilator lint_on ASSIGNDLY

always @(posedge clk_100) begin
    if (cycles != LOCK_CYCLES[7:0])
        cycles <= cycles + 8'd1;
    locked <= cycles == LOCK_CYCLES[7:0];
end

endmodule
