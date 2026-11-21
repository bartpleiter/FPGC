// Testbench of activity_leds, with 2 LEDs and a tick every 4 cycles. See tb_unit.vh
// for how the vectors are applied.
`timescale 1ns / 1ps
module tb_activity_leds;

`include "tb_unit.vh"

reg        reset;
reg  [1:0] activity;
wire [1:0] leds;

activity_leds #(
    .LEDS(2),
    .TICK_BITS(2)
) dut (
    .clk(clk),
    .reset(reset),
    .activity(activity),
    .leds(leds)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h\n", label, reset, activity) == 3)
        @(negedge clk) $fwrite(outputs_fd, "%h\n", leds);
    tb_close;
end

endmodule
