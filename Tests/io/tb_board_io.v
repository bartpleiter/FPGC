// Testbench of board_io. See tb_unit.vh for how the vectors are applied. The
// switches' synchronizer has no reset, so the testbench starts it at 0.
`timescale 1ns / 1ps
module tb_board_io;

`include "tb_unit.vh"

reg        reset;
reg        set_leds;
reg  [4:0] leds_in;
reg  [3:0] switch_pins;
wire [4:0] leds;
wire [3:0] switches;

board_io dut (
    .clk(clk),
    .reset(reset),
    .set_leds(set_leds),
    .leds_in(leds_in),
    .leds(leds),
    .switch_pins(switch_pins),
    .switches(switches)
);

initial begin
    dut.switches_sync = 4'd0;
    dut.switches      = 4'd0;
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h\n", label, reset, set_leds, leds_in,
                   switch_pins) == 5)
        @(negedge clk) $fwrite(outputs_fd, "%h %h\n", leds, switches);
    tb_close;
end

endmodule
