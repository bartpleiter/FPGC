// PLL
// 50 MHz in; c1 is the 100 MHz system clock, and c2 the SDRAM's clock: the same, but
// later. The other outputs are unused for now, but set the same VCO and clock
// uncertainty.
module system_pll (
    input  wire clk_50,
    output wire clk_100,
    output wire clk_sdram,
    output wire locked     // the clocks are stable
);

wire [4:0] clocks;

assign clk_100   = clocks[1];
assign clk_sdram = clocks[2];

altpll #(
    .bandwidth_type("AUTO"),
    .clk0_divide_by(1),
    .clk0_duty_cycle(50),
    .clk0_multiply_by(1),
    .clk0_phase_shift("0"),
    .clk1_divide_by(1),
    .clk1_duty_cycle(50),
    .clk1_multiply_by(2),
    .clk1_phase_shift("0"),
    .clk2_divide_by(1),
    .clk2_duty_cycle(50),
    .clk2_multiply_by(2),
    .clk2_phase_shift("7813"),  // in steps of 312.5 ps
    .clk3_divide_by(2),
    .clk3_duty_cycle(50),
    .clk3_multiply_by(1),
    .clk3_phase_shift("0"),
    .clk4_divide_by(2),
    .clk4_duty_cycle(50),
    .clk4_multiply_by(5),
    .clk4_phase_shift("0"),
    .compensate_clock("CLK1"),
    .inclk0_input_frequency(20000),
    .intended_device_family("Cyclone IV E"),
    .lpm_type("altpll"),
    .operation_mode("NORMAL"),
    .pll_type("AUTO"),
    .port_clk0("PORT_USED"),
    .port_clk1("PORT_USED"),
    .port_clk2("PORT_USED"),
    .port_clk3("PORT_USED"),
    .port_clk4("PORT_USED"),
    .port_inclk0("PORT_USED"),
    .port_locked("PORT_USED"),
    .width_clock(5)
) pll (
    .inclk({1'b0, clk_50}),
    .clk(clocks),
    .locked(locked)
);

endmodule
