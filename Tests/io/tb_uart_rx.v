// Testbench of uart_rx, at 1 Mbaud (100 cycles per bit). See tb_unit.vh for how the
// vectors are applied. The vectors are the line, one sample per cycle.
`timescale 1ns / 1ps
module tb_uart_rx;

`include "tb_unit.vh"

reg        reset;
reg        rx;
wire       valid;
wire [7:0] data;
wire       framing_error;

uart_rx #(
    .CLOCKS_PER_BIT(100)
) dut (
    .clk(clk),
    .reset(reset),
    .rx(rx),
    .valid(valid),
    .data(data),
    .framing_error(framing_error)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h\n", label, reset, rx) == 3)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h\n", valid, data, framing_error);
    tb_close;
end

endmodule
