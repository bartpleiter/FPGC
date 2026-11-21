// Testbench of uart, at 1 Mbaud (100 cycles per bit). See tb_unit.vh for how the
// vectors are applied. The vectors have the strobes and the rx line side by side, and
// the outputs have the tx line. rx_byte is only written while `received` is 1, as
// it's x before the first byte.
`timescale 1ns / 1ps
module tb_uart;

`include "tb_unit.vh"

reg        reset;
reg        transmit;
reg  [7:0] tx_byte;
reg        receive;
reg        clear;
reg        configure;
reg        irq_enable_in;
reg        rx;
wire       tx_full;
wire       received;
wire [7:0] rx_byte;
wire       rx_waiting;
wire       overflow;
wire       framing_error;
wire       irq_enable;
wire       irq;
wire       tx;

uart #(
    .CLOCKS_PER_BIT(100)
) dut (
    .clk(clk),
    .reset(reset),
    .transmit(transmit),
    .tx_byte(tx_byte),
    .tx_full(tx_full),
    .receive(receive),
    .received(received),
    .rx_byte(rx_byte),
    .rx_waiting(rx_waiting),
    .clear(clear),
    .overflow(overflow),
    .framing_error(framing_error),
    .configure(configure),
    .irq_enable_in(irq_enable_in),
    .irq_enable(irq_enable),
    .irq(irq),
    .rx(rx),
    .tx(tx)
);

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h %h %h %h %h\n", label, reset, transmit,
                   tx_byte, receive, clear, configure, irq_enable_in, rx) == 9)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h %h %h %h %h %h %h\n", tx_full,
                               received, received ? rx_byte : 8'd0, rx_waiting,
                               overflow, framing_error, irq_enable, irq, tx);
    tb_close;
end

endmodule
