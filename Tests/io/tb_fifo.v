// Testbench of fifo, with the UART's size: 32 bytes. See tb_unit.vh for how the
// vectors are applied. read_data is only written in the cycle after a pop that took
// a byte, and as 0 otherwise, because the RAM's output is unknown before the first.
`timescale 1ns / 1ps
module tb_fifo;

`include "tb_unit.vh"

reg        reset;
reg        push;
reg  [7:0] write_data;
reg        pop;
wire [7:0] read_data;
wire       empty;
wire       full;

fifo #(
    .WIDTH(8),
    .DEPTH_BITS(5)
) dut (
    .clk(clk),
    .reset(reset),
    .push(push),
    .write_data(write_data),
    .pop(pop),
    .read_data(read_data),
    .empty(empty),
    .full(full)
);

reg popped = 1'b0;

always @(posedge clk)
    popped <= !reset && pop && !empty;

initial begin
    tb_open;
    while ($fscanf(vectors_fd, "%h %h %h %h %h\n",
                   label, reset, push, write_data, pop) == 5)
        @(negedge clk) $fwrite(outputs_fd, "%h %h %h\n", popped ? read_data : 8'h00,
                               empty, full);
    tb_close;
end

endmodule
