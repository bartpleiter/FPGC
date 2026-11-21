// Testbench of reset_control, with a PC reset after 5 cycles. See tb_unit.vh for how
// the vectors are applied. The first output line is after the first clock edge.
`timescale 1ns / 1ps
module tb_reset_control;

`include "tb_unit.vh"

reg  locked;
reg  button_n;
reg  rts_n;
reg  dtr_n;
wire reset;

reset_control #(
    .PC_RESET_CYCLES(5)
) dut (
    .clk(clk),
    .locked(locked),
    .button_n(button_n),
    .rts_n(rts_n),
    .dtr_n(dtr_n),
    .reset(reset)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h\n", label, locked, button_n, rts_n,
                   dtr_n) == 5)
        @(negedge clk) $fwrite(outputs_fd, "%h\n", reset);
    tb_close;
end

endmodule
