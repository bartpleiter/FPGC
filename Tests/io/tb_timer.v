// Testbench of timer. See tb_unit.vh for how the vectors are applied.
`timescale 1ns / 1ps
module tb_timer;

`include "tb_unit.vh"

reg         reset;
reg         start;
reg  [31:0] start_count;
reg         configure;
reg         periodic_in;
reg         irq_enable_in;
reg         clear;
wire [31:0] count;
wire        periodic;
wire        irq_enable;
wire        expired;
wire        irq;

timer dut (
    .clk(clk),
    .reset(reset),
    .start(start),
    .start_count(start_count),
    .configure(configure),
    .periodic_in(periodic_in),
    .irq_enable_in(irq_enable_in),
    .clear(clear),
    .count(count),
    .periodic(periodic),
    .irq_enable(irq_enable),
    .expired(expired),
    .irq(irq)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h %h\n", label, reset, start,
                   start_count, configure, periodic_in, irq_enable_in, clear) == 8)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h %h\n", count, periodic,
                               irq_enable, expired, irq);
    tb_close;
end

endmodule
